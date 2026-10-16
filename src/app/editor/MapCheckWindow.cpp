// MapCheckWindow.cpp
// Kartenprüfung.

#include "EditorShared.hpp"

namespace nge {

// ============================================================================
// Kartenprüfung (Werkzeuge > Kartenprüfung), wie Unreals "Map Check": sammelt, was an der offenen
// Karte im Spiel nicht funktionieren würde. Datenquellen: SHBD (state.walkGrid), World/NPC.txt
// (ShineNPC + LinkTable), MobRegen/<Karte>.txt, MapInfo.shn, SHMD-Objekte samt NIF-Ladefehlern
// des Objekt-Renderers, Terrain-Layer der INI und QuestData.shn. Der Prüfkern liegt in
// core/MapCheck (Test: test_map_check).
// ============================================================================
std::string MapCheckFolderForMapName(EditorState& state, const std::string& mapName) {
    EnsureMapInfoLoaded(state);
    if (!state.mapInfoLoaded) return mapName;
    const auto& f = state.mapInfoShn;
    const int cName = FindShnColumnByName(f, "MapName");
    const int cFolder = FindShnColumnByName(f, "MapFolderName");
    if (cName < 0 || cFolder < 0) return mapName;
    for (const auto& row : f.rows) {
        if (static_cast<std::size_t>(std::max(cName, cFolder)) >= row.values.size()) continue;
        if (LowerAscii(core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cName)])) == LowerAscii(mapName)) {
            const std::string folder = TrimAscii(core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cFolder)]));
            return folder.empty() ? mapName : folder;
        }
    }
    return mapName;
}
// SHBD einer anderen Karte (Gate-Ziel); nullptr = Karte bzw. SHBD nicht gefunden.
const core::WalkGrid* MapCheckGridForMap(EditorState& state, const std::string& mapName) {
    const std::string folder = MapCheckFolderForMapName(state, mapName);
    if (LowerAscii(folder) == LowerAscii(state.legacySaveStem) || LowerAscii(mapName) == LowerAscii(state.legacySaveStem))
        return state.walkGrid.Width() > 0 ? &state.walkGrid : nullptr;
    const std::string key = LowerAscii(folder);
    if (const auto it = state.mapCheckGridCache.find(key); it != state.mapCheckGridCache.end()) return it->second.get();
    std::shared_ptr<core::WalkGrid> grid;
    EnsureDiscoveredMaps(state);
    int idx = FindDiscoveredMapByName(state, folder);
    if (idx < 0) idx = FindDiscoveredMapByName(state, mapName);
    if (idx >= 0 && !state.discoveredMaps[static_cast<std::size_t>(idx)].standaloneNif) {
        const std::filesystem::path ini(state.discoveredMaps[static_cast<std::size_t>(idx)].iniPath);
        if (auto shbd = core::legacy::FindSiblingFileByStem(ini.parent_path(), ini.stem().string(), ".shbd")) {
            const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Client, *shbd);
            if (auto loaded = core::ImportLegacyShbdAutoSize(path))
                grid = std::make_shared<core::WalkGrid>(std::move(*loaded));
        }
    }
    state.mapCheckGridCache[key] = grid;
    return grid.get();
}
void RunMapCheck(EditorState& state) {
    using core::MapCheckCode;
    using core::MapCheckIssue;
    using core::MapCheckPoint;
    using core::MapCheckSeverity;
    std::vector<MapCheckIssue> issues;
    const std::string stem = state.legacySaveStem;
    const bool haveGrid = state.walkGrid.Width() > 0 && state.walkGrid.Height() > 0 &&
                          core::level::WalkableFraction(state.walkGrid) > 0.0;
    if (!haveGrid) {
        MapCheckIssue i;
        i.severity = MapCheckSeverity::Warning;
        i.code = MapCheckCode::NoWalkGrid;
        i.subject = stem;
        issues.push_back(i);
    }

    // NPCs und Gates der Karte (World/NPC.txt).
    EnsureNpcTextLoaded(state);
    EnsureQuestNpcCatalog(state);
    std::unordered_set<std::string> knownMobs;
    for (const auto& e : state.questNpcCatalog) knownMobs.insert(LowerAscii(e.inx));
    if (auto* npcs = state.npcTextFile.FindTable("ShineNPC")) {
        std::unordered_set<std::string> linkArguments;
        if (auto* links = state.npcTextFile.FindTable("LinkTable")) {
            const LinkTableColumns lc = FindLinkTableColumns(*links);
            for (const auto& link : links->records)
                if (!LinkValue(link, lc.argument).empty()) linkArguments.insert(LinkValue(link, lc.argument));
        }
        std::vector<MapCheckPoint> npcPoints, gatePoints;
        for (const std::size_t idx : NpcRecordsForCurrentMap(state)) {
            const auto& rec = npcs->records[idx];
            if (rec.values.size() < 5) continue;
            const float x = static_cast<float>(std::atof(rec.values[2].c_str()));
            const float z = static_cast<float>(std::atof(rec.values[3].c_str()));
            const std::string role = rec.values.size() > 6 ? rec.values[6] : std::string();
            const std::string label = rec.values[0] + (role.empty() ? std::string() : " (" + role + ")");
            if (x == 0.0f && z == 0.0f) {
                MapCheckIssue i;
                i.severity = MapCheckSeverity::Info;
                i.code = MapCheckCode::NpcWithoutPosition;
                i.subject = label;
                i.ref = static_cast<int>(idx);
                issues.push_back(i);
                continue;
            }
            const bool gate = role == "Gate" || role == "IDGate";
            (gate ? gatePoints : npcPoints).push_back({label, x, z, static_cast<int>(idx)});
            if (role == "Gate" && rec.values.size() > 7 && !rec.values[7].empty() && !linkArguments.contains(rec.values[7])) {
                MapCheckIssue i;
                i.severity = MapCheckSeverity::Warning; // kommt in NA2016 bei 7 Gates vor
                i.code = MapCheckCode::GateLinkMissing;
                i.subject = label;
                i.detail = rec.values[7];
                i.hasPosition = true;
                i.x = x;
                i.z = z;
                i.ref = static_cast<int>(idx);
                issues.push_back(i);
            }
            if (!knownMobs.empty() && !knownMobs.contains(LowerAscii(rec.values[0]))) {
                MapCheckIssue i;
                i.severity = MapCheckSeverity::Error;
                i.code = MapCheckCode::SpawnMobUnknown;
                i.subject = label;
                i.detail = rec.values[0];
                i.hasPosition = true;
                i.x = x;
                i.z = z;
                i.ref = static_cast<int>(idx);
                issues.push_back(i);
            }
        }
        if (haveGrid) {
            core::CheckPointsWalkable(state.walkGrid, npcPoints, MapCheckCode::NpcOnBlockedCell, {}, issues);
            core::CheckPointsWalkable(state.walkGrid, gatePoints, MapCheckCode::GateOnBlockedCell, {}, issues);
        }
    }

    // Ziele der ausgehenden Gates: auf der Zielkarte begehbar?
    for (const auto& marker : CollectPortalMarkers(state)) {
        if (marker.kind != kPortalKindGateLink) continue;
        const std::string target = PortalTargetMapName(marker);
        const core::WalkGrid* grid = MapCheckGridForMap(state, target);
        MapCheckIssue i;
        i.subject = marker.label;
        i.detail = target + " " + std::to_string(static_cast<int>(marker.targetX)) + "/" +
                   std::to_string(static_cast<int>(marker.targetY));
        i.hasPosition = true;
        i.x = marker.x;
        i.z = marker.y;
        i.ref = static_cast<int>(marker.idx);
        if (!grid) {
            i.severity = MapCheckSeverity::Info;
            i.code = MapCheckCode::GateTargetMapUnknown;
            issues.push_back(i);
            continue;
        }
        if (marker.targetX >= static_cast<float>(grid->Cols()) * core::WalkGrid::kCellSize ||
            marker.targetY >= static_cast<float>(grid->Rows()) * core::WalkGrid::kCellSize ||
            marker.targetX < 0.0f || marker.targetY < 0.0f) {
            i.severity = MapCheckSeverity::Error;
            i.code = MapCheckCode::GateTargetOutside;
            issues.push_back(i);
            continue;
        }
        std::vector<MapCheckIssue> targetIssues;
        const MapCheckPoint p{marker.label, marker.targetX, marker.targetY, static_cast<int>(marker.idx)};
        // Ankunftspunkte müssen begehbar sein: strengere Staffel als bei NPCs.
        core::CheckPointsWalkable(*grid, std::span<const MapCheckPoint>(&p, 1), MapCheckCode::GateTargetBlocked,
                                  {1.0f * core::WalkGrid::kCellSize, 4.0f * core::WalkGrid::kCellSize}, targetIssues);
        for (auto& t : targetIssues) {
            i.severity = t.severity;
            i.code = t.code;
            i.value = t.value;
            issues.push_back(i);
        }
    }

    // Ankunftspunkte auf dieser Karte: LinkTable-Einträge (Gates anderer Karten) mit dieser Zielkarte.
    const std::string mapName = [&] {
        // MapInfo-Name dieser Karte (Ordner "Rou" -> MapName "Rou"; MapName kann vom Ordner abweichen).
        return std::string(stem);
    }();
    if (haveGrid) {
        if (auto* links = state.npcTextFile.FindTable("LinkTable")) {
            const LinkTableColumns lc = FindLinkTableColumns(*links);
            std::vector<MapCheckPoint> arrivals;
            for (std::size_t l = 0; l < links->records.size(); ++l) {
                const auto& link = links->records[l];
                const std::string client = TrimAscii(LinkValue(link, lc.mapClient));
                const std::string target = client.empty() ? TrimAscii(LinkValue(link, lc.mapServer)) : client;
                if (target.empty() || LowerAscii(MapCheckFolderForMapName(state, target)) != LowerAscii(mapName)) continue;
                arrivals.push_back({LinkValue(link, lc.argument), static_cast<float>(std::atof(LinkValue(link, lc.x).c_str())),
                                    static_cast<float>(std::atof(LinkValue(link, lc.y).c_str())), static_cast<int>(l)});
            }
            core::CheckPointsWalkable(state.walkGrid, arrivals, MapCheckCode::GateArrivalBlocked,
                                      {1.0f * core::WalkGrid::kCellSize, 4.0f * core::WalkGrid::kCellSize}, issues);
        }
        // TownPortal- und Rückruf-Ziele auf dieser Karte.
        EnsureTownPortalLoaded(state);
        EnsureRecallCoordLoaded(state);
        std::vector<MapCheckPoint> teleports;
        for (const auto& marker : CollectPortalMarkers(state))
            if (marker.kind == kPortalKindTown || marker.kind == kPortalKindRecall)
                teleports.push_back({marker.label, marker.x, marker.y, static_cast<int>(marker.idx)});
        core::CheckPointsWalkable(state.walkGrid, teleports, MapCheckCode::TeleportTargetBlocked,
                                  {1.0f * core::WalkGrid::kCellSize, 4.0f * core::WalkGrid::kCellSize}, issues);
    }

    // Spawn-Zonen (MobRegen/<Karte>.txt).
    EnsureMobRegenLoaded(state);
    if (state.mobRegenTextLoaded && state.mobRegenLoadedForMap == stem) {
        std::vector<core::MapCheckZone> zones;
        std::unordered_map<std::string, int> zoneByGroup;
        if (auto* zoneTable = state.mobRegenTextFile.FindTable("MobRegenGroup")) {
            for (std::size_t z = 0; z < zoneTable->records.size(); ++z) {
                const auto& rec = zoneTable->records[z];
                if (rec.values.size() < 7) continue;
                const float w = static_cast<float>(std::atoi(rec.values[4].c_str()));
                const float h = static_cast<float>(std::atoi(rec.values[5].c_str()));
                const float range = static_cast<float>(std::atoi(rec.values[6].c_str()));
                // Gleiche Form wie die 2D-Anzeige: Breite/Höhe, sonst Range als Radius.
                const float radius = (w > 0.0f || h > 0.0f) ? std::max(w, h) * 0.5f : std::abs(range);
                zones.push_back({rec.values[0], static_cast<float>(std::atoi(rec.values[2].c_str())),
                                 static_cast<float>(std::atoi(rec.values[3].c_str())), radius, static_cast<int>(z)});
                zoneByGroup[rec.values[0]] = static_cast<int>(zones.size() - 1);
            }
        }
        // Nur Zonen prüfen, in denen ein aktiver MobRegen-Eintrag Monster erzeugt; auskommentierte
        // Event-Zonen (z.B. Rou EventSnow10) sind im Original oft unbenutzt.
        std::unordered_set<std::string> usedGroups;
        if (auto* regen = state.mobRegenTextFile.FindTable("MobRegen"))
            for (const auto& rec : regen->records)
                if (!rec.values.empty()) usedGroups.insert(rec.values[0]);
        std::vector<core::MapCheckZone> usedZones;
        for (const auto& zone : zones) {
            if (usedGroups.contains(zone.label)) { usedZones.push_back(zone); continue; }
            MapCheckIssue i;
            i.severity = MapCheckSeverity::Info;
            i.code = MapCheckCode::SpawnZoneUnused;
            i.subject = zone.label;
            i.hasPosition = true;
            i.x = zone.x;
            i.z = zone.z;
            i.ref = zone.ref;
            issues.push_back(i);
        }
        if (haveGrid) core::CheckSpawnZones(state.walkGrid, usedZones, {}, issues);
        if (auto* regen = state.mobRegenTextFile.FindTable("MobRegen"); regen && !knownMobs.empty()) {
            for (const auto& rec : regen->records) {
                if (rec.values.size() < 2 || rec.values[1].empty() || knownMobs.contains(LowerAscii(rec.values[1]))) continue;
                MapCheckIssue i;
                i.severity = MapCheckSeverity::Error;
                i.code = MapCheckCode::SpawnMobUnknown;
                i.subject = rec.values[0];
                i.detail = rec.values[1];
                if (const auto it = zoneByGroup.find(rec.values[0]); it != zoneByGroup.end()) {
                    const auto& zone = zones[static_cast<std::size_t>(it->second)];
                    i.hasPosition = true;
                    i.x = zone.x;
                    i.z = zone.z;
                    i.ref = zone.ref;
                }
                issues.push_back(i);
            }
        }
    }

    // Wiederbelebungspunkt (MapInfo.shn RegenX/RegenY).
    EnsureMapInfoLoaded(state);
    if (float rx = 0, ry = 0; haveGrid && FindMapRegen(state, rx, ry)) {
        const MapCheckPoint p{stem, rx, ry, -1};
        core::CheckPointsWalkable(state.walkGrid, std::span<const MapCheckPoint>(&p, 1), MapCheckCode::RespawnPointBlocked,
                                  {1.0f * core::WalkGrid::kCellSize, 4.0f * core::WalkGrid::kCellSize}, issues);
    }

    // Objekte außerhalb der Karte und NIF-/Texturfehler des Objekt-Renderers.
    const float sizeX = static_cast<float>(std::max<std::uint32_t>(state.heightmap.Width(), 1) - 1) * state.heightmap.BlockWidth();
    const float sizeZ = static_cast<float>(std::max<std::uint32_t>(state.heightmap.Height(), 1) - 1) * state.heightmap.BlockHeight();
    std::unordered_map<std::string, std::pair<int, int>> firstObjectOfModel; // Modell -> (erstes Objekt, Anzahl)
    {
        std::vector<MapCheckPoint> objects;
        for (std::size_t o = 0; o < state.placementSet.Count(); ++o) {
            const auto& obj = state.placementSet.At(o);
            objects.push_back({obj.modelPath, obj.posX, obj.posZ, static_cast<int>(o)});
            auto& entry = firstObjectOfModel.try_emplace(obj.modelPath, static_cast<int>(o), 0).first->second;
            ++entry.second;
        }
        if (sizeX > 0.0f && sizeZ > 0.0f)
            core::CheckPointsInsideMap(objects, sizeX, sizeZ, 2.0f * state.heightmap.BlockWidth(), MapCheckCode::ObjectOutsideMap,
                                       MapCheckSeverity::Warning, issues);
    }
    const auto& diag = state.nifMeshRenderer.Diagnostics();
    const auto modelIssue = [&](MapCheckCode code, MapCheckSeverity severity, const std::string& model, std::string detail) {
        // Der Renderer sammelt Ladefehler je Karte; nur Modelle melden, die noch platziert sind.
        if (!firstObjectOfModel.contains(model)) return;
        MapCheckIssue i;
        i.severity = severity;
        i.code = code;
        i.subject = model;
        i.detail = std::move(detail);
        if (const auto it = firstObjectOfModel.find(model); it != firstObjectOfModel.end()) {
            const auto& obj = state.placementSet.At(static_cast<std::size_t>(it->second.first));
            i.hasPosition = true;
            i.x = obj.posX;
            i.z = obj.posZ;
            i.ref = it->second.first;
            i.value = it->second.second;
        }
        issues.push_back(std::move(i));
    };
    for (const auto& model : diag.missingModels) modelIssue(MapCheckCode::ModelMissing, MapCheckSeverity::Error, model, {});
    for (const auto& [model, error] : diag.failedModels) modelIssue(MapCheckCode::ModelLoadFailed, MapCheckSeverity::Error, model, error);
    for (const auto& [model, textures] : diag.missingTextures) {
        std::string list;
        for (const auto& t : textures) list += (list.empty() ? "" : ", ") + t;
        modelIssue(MapCheckCode::TextureMissing, MapCheckSeverity::Warning, model, list);
    }

    // Terrain-Texturen der INI.
    if (state.legacyIniPath[0] != '\0') {
        const std::filesystem::path iniDir = std::filesystem::path(state.legacyIniPath).parent_path();
        for (std::size_t l = 0; l < state.textureStack.LayerCount(); ++l) {
            const auto& layer = state.textureStack.Layer(l);
            if (layer.diffuseFileName.empty() || core::legacy::ResolveLegacyAssetPath(iniDir, layer.diffuseFileName)) continue;
            MapCheckIssue i;
            i.severity = MapCheckSeverity::Error;
            i.code = MapCheckCode::TerrainTextureMissing;
            i.subject = layer.name;
            i.detail = layer.diffuseFileName;
            i.ref = static_cast<int>(l);
            issues.push_back(i);
        }
    }

    // Minimap: der Client lädt resmenu\minimap\<MapFolderName>.dds, wenn MinimapView gesetzt ist.
    if (const auto viewRow = FindMapViewRow(state, EditorState::ShnSource::Client);
        viewRow && viewRow->minimapView && GameMinimapPath(state, viewRow->folder).empty()) {
        MapCheckIssue i;
        i.severity = MapCheckSeverity::Warning;
        i.code = MapCheckCode::MinimapMissing;
        i.subject = viewRow->folder;
        i.detail = core::legacy::MinimapRelativePath(viewRow->folder);
        issues.push_back(i);
    }

    // Spieldaten: aktivierte Quests mit Start-NPC, der nicht existiert oder auf keiner Karte steht.
    if (state.mapCheckIncludeQuests) {
        EnsureQuestDataLoaded(state);
        for (std::size_t q = 0; q < state.questDataFile.records.size(); ++q) {
            const auto& quest = state.questDataFile.records[q];
            if (quest.enableQuest == 0 || quest.needNpc == 0 || quest.startingNpc == 0) continue;
            const auto* npc = FindQuestNpc(state, quest.startingNpc);
            if (npc && !npc->placements.empty()) continue;
            MapCheckIssue i;
            i.severity = npc ? MapCheckSeverity::Warning : MapCheckSeverity::Error;
            i.code = npc ? MapCheckCode::QuestStartNpcNotPlaced : MapCheckCode::QuestStartNpcUnknown;
            const std::string title = QuestTextOf(state, quest.title);
            i.subject = "#" + std::to_string(quest.id) + (title.empty() ? std::string() : " " + title);
            i.detail = npc ? npc->name + " (" + npc->inx + ")" : std::to_string(quest.startingNpc);
            i.ref = static_cast<int>(q);
            issues.push_back(i);
        }
    }

    core::SortMapCheckIssues(issues);
    std::size_t counts[3] = {0, 0, 0};
    for (const auto& i : issues) ++counts[static_cast<int>(i.severity)];
    state.mapCheckIssues = std::move(issues);
    state.mapCheckForMap = stem;
    char buf[160];
    std::snprintf(buf, sizeof(buf), L("%zu Fehler, %zu Warnungen, %zu Hinweise", "%zu errors, %zu warnings, %zu notes"),
                  counts[0], counts[1], counts[2]);
    state.mapCheckSummary = buf;
}
std::string MapCheckMessage(const core::MapCheckIssue& i) {
    using core::MapCheckCode;
    char buf[512];
    const double cells = i.value / core::WalkGrid::kCellSize;
    switch (i.code) {
        case MapCheckCode::NoWalkGrid:
            return L("Kein Block&Walk-Gitter (SHBD) geladen - Positionsprüfungen entfallen.",
                     "No Block&Walk grid (SHBD) loaded - position checks skipped.");
        case MapCheckCode::NpcOnBlockedCell:
        case MapCheckCode::GateOnBlockedCell:
        case MapCheckCode::RespawnPointBlocked:
        case MapCheckCode::GateTargetBlocked:
        case MapCheckCode::GateArrivalBlocked:
        case MapCheckCode::TeleportTargetBlocked: {
            const char* what = i.code == MapCheckCode::NpcOnBlockedCell    ? L("NPC steht auf blockierter Zelle", "NPC stands on a blocked cell")
                               : i.code == MapCheckCode::GateOnBlockedCell ? L("Gate steht auf blockierter Zelle", "Gate stands on a blocked cell")
                               : i.code == MapCheckCode::RespawnPointBlocked
                                   ? L("Wiederbelebungspunkt (MapInfo RegenX/RegenY) auf blockierter Zelle",
                                       "Respawn point (MapInfo RegenX/RegenY) on a blocked cell")
                               : i.code == MapCheckCode::GateArrivalBlocked
                                   ? L("Gate einer anderen Karte kommt hier auf blockierter Zelle an", "A gate from another map arrives here on a blocked cell")
                               : i.code == MapCheckCode::TeleportTargetBlocked
                                   ? L("Teleport-/Rückruf-Ziel auf blockierter Zelle", "Teleport/recall target on a blocked cell")
                                   : L("Gate-Ziel liegt auf blockierter Zelle", "Gate target is on a blocked cell");
            if (i.value < 0.0)
                std::snprintf(buf, sizeof(buf), L("%s - keine freie Zelle in der Nähe", "%s - no free cell nearby"), what);
            else
                std::snprintf(buf, sizeof(buf), L("%s - nächste freie Zelle %.0f Einheiten (%.0f Zellen)",
                                                  "%s - nearest free cell %.0f units (%.0f cells)"), what, i.value, cells);
            std::string out = buf;
            if (!i.detail.empty()) out += " · " + i.detail;
            return out;
        }
        case MapCheckCode::NpcWithoutPosition:
            return L("NPC ohne Koordinate (0/0) - im Spiel nicht aufgestellt", "NPC without coordinates (0/0) - not placed in the game");
        case MapCheckCode::GateTargetOutside:
            return std::string(L("Gate-Ziel liegt außerhalb der Zielkarte: ", "Gate target is outside the target map: ")) + i.detail;
        case MapCheckCode::GateTargetMapUnknown:
            return std::string(L("Zielkarte nicht im Client-Ordner gefunden, Ziel ungeprüft: ",
                                 "Target map not found in the client folder, target unchecked: ")) + i.detail;
        case MapCheckCode::GateLinkMissing:
            return std::string(L("Gate verweist auf fehlenden LinkTable-Eintrag: ", "Gate refers to a missing LinkTable entry: ")) + i.detail;
        case MapCheckCode::SpawnZoneCenterBlocked:
            std::snprintf(buf, sizeof(buf), L("Mittelpunkt der Spawn-Zone blockiert (%.0f %% der Zone blockiert)",
                                              "Spawn zone centre is blocked (%.0f %% of the zone blocked)"), i.value * 100.0);
            return buf;
        case MapCheckCode::SpawnZoneMostlyBlocked:
            std::snprintf(buf, sizeof(buf), L("Spawn-Zone liegt in Wänden: %.0f %% der Fläche blockiert",
                                              "Spawn zone is inside walls: %.0f %% of the area blocked"), i.value * 100.0);
            return buf;
        case MapCheckCode::SpawnZoneOutside:
            return L("Spawn-Zone liegt außerhalb der Karte", "Spawn zone is outside the map");
        case MapCheckCode::SpawnZoneUnused:
            return L("Spawn-Zone ohne aktiven MobRegen-Eintrag (erzeugt keine Monster)", "Spawn zone without an active MobRegen entry (spawns no monsters)");
        case MapCheckCode::SpawnMobUnknown:
            return std::string(L("Mob nicht in MobInfo.shn: ", "Mob not in MobInfo.shn: ")) + i.detail;
        case MapCheckCode::ObjectOutsideMap:
            std::snprintf(buf, sizeof(buf), L("Objekt außerhalb der Karte (%.0f / %.0f)", "Object outside the map (%.0f / %.0f)"), i.x, i.z);
            return buf;
        case MapCheckCode::ModelMissing:
            std::snprintf(buf, sizeof(buf), L("NIF nicht gefunden (%.0f Objekte)", "NIF not found (%.0f objects)"), i.value);
            return buf;
        case MapCheckCode::ModelLoadFailed:
            return std::string(L("NIF nicht lesbar: ", "NIF not readable: ")) + i.detail;
        case MapCheckCode::TextureMissing:
            return std::string(L("Textur fehlt: ", "Texture missing: ")) + i.detail;
        case MapCheckCode::TerrainTextureMissing:
            return std::string(L("Terrain-Textur nicht gefunden: ", "Terrain texture not found: ")) + i.detail;
        case MapCheckCode::QuestStartNpcUnknown:
            return std::string(L("Start-NPC nicht in MobInfo.shn: ID ", "Starting NPC not in MobInfo.shn: ID ")) + i.detail;
        case MapCheckCode::QuestStartNpcNotPlaced:
            return std::string(L("Start-NPC steht auf keiner Karte - Quest kann nicht angenommen werden: ",
                                 "Starting NPC is not placed on any map - the quest cannot be accepted: ")) + i.detail;
        case MapCheckCode::MinimapMissing:
            return std::string(L("Minimap eingeschaltet, aber Bild fehlt: resmenu/", "Minimap enabled but image missing: resmenu/")) + i.detail +
                   L(" (Minimap-Panel: 'Minimap erzeugen')", " (minimap panel: 'Generate minimap')");
    }
    return {};
}
const char* MapCheckCategoryLabel(core::MapCheckCode code) {
    using core::MapCheckCode;
    switch (code) {
        case MapCheckCode::NoWalkGrid: return "SHBD";
        case MapCheckCode::NpcOnBlockedCell:
        case MapCheckCode::NpcWithoutPosition: return "NPC";
        case MapCheckCode::GateOnBlockedCell:
        case MapCheckCode::GateTargetBlocked:
        case MapCheckCode::GateTargetOutside:
        case MapCheckCode::GateTargetMapUnknown:
        case MapCheckCode::GateLinkMissing:
        case MapCheckCode::GateArrivalBlocked: return "Gate";
        case MapCheckCode::TeleportTargetBlocked: return "Teleport";
        case MapCheckCode::SpawnZoneUnused:
        case MapCheckCode::SpawnZoneCenterBlocked:
        case MapCheckCode::SpawnZoneMostlyBlocked:
        case MapCheckCode::SpawnZoneOutside:
        case MapCheckCode::SpawnMobUnknown: return "Spawn";
        case MapCheckCode::RespawnPointBlocked: return "MapInfo";
        case MapCheckCode::ObjectOutsideMap:
        case MapCheckCode::ModelMissing:
        case MapCheckCode::ModelLoadFailed:
        case MapCheckCode::TextureMissing: return L("Objekt", "Object");
        case MapCheckCode::TerrainTextureMissing: return "Terrain";
        case MapCheckCode::QuestStartNpcUnknown:
        case MapCheckCode::QuestStartNpcNotPlaced: return "Quest";
        case MapCheckCode::MinimapMissing: return "Minimap";
    }
    return "";
}
void JumpToMapCheckIssue(EditorState& state, const core::MapCheckIssue& i) {
    using core::MapCheckCode;
    if (i.code == MapCheckCode::QuestStartNpcUnknown || i.code == MapCheckCode::QuestStartNpcNotPlaced) {
        state.screen = AppScreen::ShnEditor;
        state.shnSubTab = 4;
        state.selectedQuestIdx = i.ref;
        return;
    }
    if (i.code == MapCheckCode::TerrainTextureMissing) {
        state.selectedLayer = i.ref;
        return;
    }
    if (!i.hasPosition) return;
    state.screen = AppScreen::MapEditorWorkspace;
    const bool objectIssue = i.code == MapCheckCode::ObjectOutsideMap || i.code == MapCheckCode::ModelMissing ||
                             i.code == MapCheckCode::ModelLoadFailed || i.code == MapCheckCode::TextureMissing;
    if (objectIssue && i.ref >= 0 && static_cast<std::size_t>(i.ref) < state.placementSet.Count()) {
        state.selectedObjects = {i.ref};
        state.camera.SetTarget(i.x, state.placementSet.At(static_cast<std::size_t>(i.ref)).posY, i.z);
    } else {
        if (i.code == MapCheckCode::SpawnZoneCenterBlocked || i.code == MapCheckCode::SpawnZoneMostlyBlocked ||
            i.code == MapCheckCode::SpawnZoneOutside || i.code == MapCheckCode::SpawnMobUnknown ||
            i.code == MapCheckCode::SpawnZoneUnused)
            state.selectedMobZoneIdx = i.ref;
        state.camera.SetTarget(i.x, state.heightmap.SampleWorld(i.x, i.z) + 25.0f, i.z);
    }
    state.camera.Zoom(420.0f - state.camera.Distance());
}
void DrawMapCheckWindow(EditorState& state) {
    if (state.mapCheckRunRequest) {
        state.mapCheckRunRequest = false;
        RunMapCheck(state);
    }
    if (!state.mapCheckOpen) return;
    ImGui::SetNextWindowSize(ImVec2(980.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(L("Kartenprüfung##mapCheck", "Map check##mapCheck"), &state.mapCheckOpen)) {
        ImGui::End();
        return;
    }
    if (UI::Button(L("Prüfen##mapCheckRun", "Check##mapCheckRun"))) RunMapCheck(state);
    ImGui::SameLine();
    UI::Checkbox(L("Fehler", "Errors"), &state.mapCheckShowErrors);
    ImGui::SameLine();
    UI::Checkbox(L("Warnungen", "Warnings"), &state.mapCheckShowWarnings);
    ImGui::SameLine();
    UI::Checkbox(L("Hinweise", "Notes"), &state.mapCheckShowInfo);
    ImGui::SameLine();
    UI::Checkbox(L("Quests prüfen", "Check quests"), &state.mapCheckIncludeQuests);
    if (!state.mapCheckForMap.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s: %s", state.mapCheckForMap.c_str(), state.mapCheckSummary.c_str());
        if (state.mapCheckForMap != state.legacySaveStem) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s", L("(andere Karte - neu prüfen)", "(other map - check again)"));
        }
    } else {
        ImGui::TextDisabled("%s", L("Noch nicht geprüft.", "Not checked yet."));
    }
    ImGui::TextDisabled("%s", L("Doppelklick springt zur Stelle (Quests öffnen den Quest-Editor). Hinweise sind Fälle, die auch in den Originalkarten vorkommen.",
                                "Double-click jumps to the spot (quests open the quest editor). Notes are cases that also occur in the original maps."));
    std::vector<const core::MapCheckIssue*> rows;
    for (const auto& i : state.mapCheckIssues) {
        if (i.severity == core::MapCheckSeverity::Error && !state.mapCheckShowErrors) continue;
        if (i.severity == core::MapCheckSeverity::Warning && !state.mapCheckShowWarnings) continue;
        if (i.severity == core::MapCheckSeverity::Info && !state.mapCheckShowInfo) continue;
        rows.push_back(&i);
    }
    if (ImGui::BeginTable("##mapCheckRows", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV |
                                                  ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(L("Schwere", "Severity"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(L("Bereich", "Area"), ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn(L("Betrifft", "Subject"), ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn(L("Meldung", "Message"), ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size()));
        while (clipper.Step()) {
            for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
                const auto& i = *rows[static_cast<std::size_t>(r)];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const ImVec4 col = i.severity == core::MapCheckSeverity::Error     ? ImVec4(1.0f, 0.42f, 0.38f, 1.0f)
                                   : i.severity == core::MapCheckSeverity::Warning ? ImVec4(1.0f, 0.78f, 0.32f, 1.0f)
                                                                                   : ImVec4(0.55f, 0.75f, 0.95f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                const char* sev = i.severity == core::MapCheckSeverity::Error     ? L("Fehler", "Error")
                                  : i.severity == core::MapCheckSeverity::Warning ? L("Warnung", "Warning")
                                                                                  : L("Hinweis", "Note");
                if (ImGui::Selectable((std::string(sev) + "##mc" + std::to_string(r)).c_str(), false,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick) &&
                    ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    JumpToMapCheckIssue(state, i);
                ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(MapCheckCategoryLabel(i.code));
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(i.subject.c_str());
                ImGui::TableSetColumnIndex(3);
                const std::string msg = MapCheckMessage(i);
                ImGui::TextUnformatted(msg.c_str());
                if (ImGui::IsItemHovered() && msg.size() > 60) ImGui::SetTooltip("%s", msg.c_str());
            }
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace nge
