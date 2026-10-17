// NewMapWizard.cpp
// Assistent "Neue Karte" bis ins Spiel.

#include "EditorShared.hpp"

namespace nge {

// ============================================================================
// Assistent "Neue Karte" bis ins Spiel: Karte aus Vorlage (core/legacy/NewMapFromTemplate), dazu
// MapInfo.shn + MapViewInfo.shn (Client und Server), World/Field.txt (FieldList) und optional ein
// Gate von einer vorhandenen Karte samt Rückweg (World/NPC.txt ShineNPC + LinkTable). Alles wird
// sofort in den Projektordner geschrieben; die Quellordner bleiben unberührt.
// ============================================================================
namespace {
// Fügt die Kartenzeile in eine Client- und Server-SHN ein (Vorlage: Zeile der Vorlagenkarte).
void AddMapShnRows(EditorState& state, const char* fileName, long long templateId, const std::string& templateName,
                   long long newId, const std::string& newName,
                   const std::vector<std::pair<const char*, std::string>>& cells, std::vector<std::string>& report) {
    for (const auto src : {EditorState::ShnSource::Client, EditorState::ShnSource::Server}) {
        // Bytegleiche Client/Server-Dateien spiegelt SaveShnDocument auf die Gegenseite: steht die neue
        // ID dort schon, ist diese Seite bereits geschrieben - nicht ein zweites Mal klonen.
        if (const int existing = FindOrLoadShnDoc(state, fileName, src); existing >= 0) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(existing)].file;
            const int cId = ShnColumnIndexByName(f, "ID");
            bool present = false;
            for (const auto& row : f.rows) {
                long long v = 0;
                if (cId >= 0 && ShnValueAsInt(row.values[static_cast<std::size_t>(cId)], v) && v == newId) present = true;
            }
            if (present) {
                report.push_back(std::string("+ ") + ShnSourceName(src) + " " + fileName + ": " + newName + " (ID " + std::to_string(newId) +
                                 L(") - gekoppelt mit der anderen Seite gespeichert", ") - saved together with the other side"));
                continue;
            }
        }
        const auto rows = CloneRowsById(state, fileName, src, "MapName", templateId, templateName, newId, newName, report);
        const int di = FindOrLoadShnDoc(state, fileName, src);
        if (rows.empty() || di < 0) continue;
        auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        for (const auto r : rows)
            for (const auto& [col, value] : cells) SetShnCellText(f, r, col, value);
        const bool saved = SaveShnDocument(state, di);
        report.push_back(std::string(saved ? "+ " : "- FEHLER ") + ShnSourceName(src) + " " + fileName + ": " + newName + " (ID " +
                         std::to_string(newId) + ")" + (saved ? std::string() : " - " + state.shnStatus));
    }
}

// Höchste ID unter `below` (NA2016: reguläre Karten 0..159, Sonderkarten EventF/EventF01 ab 1000).
long long MaxShnId(EditorState& state, const char* fileName, long long below = 1000) {
    long long maxId = -1;
    for (const auto src : {EditorState::ShnSource::Client, EditorState::ShnSource::Server}) {
        const int di = FindOrLoadShnDoc(state, fileName, src);
        if (di < 0) continue;
        const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        const int c = ShnColumnIndexByName(f, "ID");
        if (c < 0) continue;
        for (const auto& row : f.rows) {
            long long v = 0;
            if (ShnValueAsInt(row.values[static_cast<std::size_t>(c)], v) && v < below) maxId = std::max(maxId, v);
        }
    }
    return maxId;
}

int ShineColumn(const core::legacy::ShineTable& t, const char* name) {
    for (std::size_t i = 0; i < t.columns.size(); ++i)
        if (LowerAscii(t.columns[i].name) == LowerAscii(name)) return static_cast<int>(i);
    return -1;
}

void SetShineValue(core::legacy::ShineRecord& rec, const core::legacy::ShineTable& t, const char* col, const std::string& v) {
    const int c = ShineColumn(t, col);
    if (c < 0) return;
    if (rec.values.size() <= static_cast<std::size_t>(c)) rec.values.resize(static_cast<std::size_t>(c) + 1);
    rec.values[static_cast<std::size_t>(c)] = v;
}

// Shine-Textdateien ersetzen '#' beim Lesen durch Leerzeichen (#exchange); Anzeigenamen daher mit '#'.
std::string ShineTextName(std::string s) {
    for (char& c : s) if (c == ' ') c = '#';
    return s;
}
}
 // namespace

bool CreateNewMapFromWizard(EditorState& state) {
    auto& rep = state.newMapReport;
    rep.clear();
    EnsureDiscoveredMaps(state);
    const std::string name = state.newMapName;
    if (state.project.projectFolder[0] == '\0') {
        rep.push_back(L("- Kein Projektordner konfiguriert.", "- No project folder configured."));
        return false;
    }
    if (!core::legacy::IsValidNewMapName(name)) {
        rep.push_back(L("- Ungültiger Name: Buchstabe am Anfang, dann Buchstaben/Ziffern/_, höchstens 15 Zeichen.",
                        "- Invalid name: letter first, then letters/digits/_, at most 15 characters."));
        return false;
    }
    if (FindDiscoveredMapByName(state, name) >= 0) {
        rep.push_back(L("- Eine Karte mit diesem Namen gibt es schon.", "- A map with this name already exists."));
        return false;
    }
    if (state.newMapTemplate < 0 || static_cast<std::size_t>(state.newMapTemplate) >= state.discoveredMaps.size()) {
        rep.push_back(L("- Keine Vorlagenkarte gewählt.", "- No template map chosen."));
        return false;
    }
    const auto& tmplEntry = state.discoveredMaps[static_cast<std::size_t>(state.newMapTemplate)];
    const std::filesystem::path tmplIni(tmplEntry.iniPath);
    auto tmpl = core::legacy::OpenLegacyMap(LegacyMapWorkingIniPath(state, tmplIni));
    if (!tmpl) {
        rep.push_back(std::string("- ") + L("Vorlage nicht lesbar: ", "Template not readable: ") + tmpl.error());
        return false;
    }
    const std::uint32_t points = state.newMapSize == 0 ? 129u : state.newMapSize == 2 ? 513u : 257u;
    auto created = core::legacy::CreateMapFromTemplate(*tmpl, {name, points, state.newMapBaseHeight});
    if (!created) {
        rep.push_back("- " + created.error());
        return false;
    }
    const float span = static_cast<float>(points - 1) * created->heightmap.BlockWidth();
    const int centerX = static_cast<int>(span * 0.5f), centerY = static_cast<int>(span * 0.5f);

    // Karte übernehmen (Texturen relativ zur Vorlage auflösen) und ins Projekt schreiben.
    ApplyProjectToState(state, std::move(*created), tmplIni.parent_path());
    LoadMapRenderData(state, tmplIni.parent_path(), tmplIni.stem().string());
    state.legacySourceStem = name;
    state.legacySourceFileNames.clear();
    state.legacyShmdSourceCount = 0;
    state.walkGridAtLoad = core::WalkGrid{}; // neue Karte: auch die Server-Begehbarkeit schreiben
    state.legacyMapIniPath[0] = '\0';
    std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", name.c_str());
    const auto outputDir = std::filesystem::path(state.project.projectFolder) / "Client" / "resmap" / "field" / name;
    std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", outputDir.string().c_str());
    state.mapDirty = true;
    if (!SaveLegacyMapProject(state)) {
        rep.push_back("- " + state.statusMessage);
        return false;
    }
    std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s", (outputDir / (name + ".ini")).string().c_str());
    rep.push_back("+ Client resmap/field/" + name + ": INI, HTD, SHBD, SHMD, " +
                  std::to_string(state.textureStack.LayerCount()) + L(" Masken", " masks") +
                  (state.hasLegacyZoneMetadata ? ", AID" : "") + L(" (Vorlage ", " (template ") + tmplEntry.name + ")");
    rep.push_back("+ Server 9Data/Shine/BlockInfo/" + name + ".shbd");

    // Vorlage in MapInfo: Zeile, deren MapFolderName dem Vorlagenordner entspricht.
    long long tmplId = -1;
    std::string tmplName;
    if (const int di = FindOrLoadShnDoc(state, "MapInfo.shn", EditorState::ShnSource::Client); di >= 0) {
        const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        const int cId = ShnColumnIndexByName(f, "ID"), cName = ShnColumnIndexByName(f, "MapName"),
                  cFolder = ShnColumnIndexByName(f, "MapFolderName");
        for (const auto& row : f.rows) {
            if (cId < 0 || cName < 0 || cFolder < 0) break;
            if (!SameMapName(core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cFolder)]), tmplEntry.name)) continue;
            ShnValueAsInt(row.values[static_cast<std::size_t>(cId)], tmplId);
            tmplName = TrimAscii(core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cName)]));
            break;
        }
    }
    const long long newId = std::max(MaxShnId(state, "MapInfo.shn"), MaxShnId(state, "MapViewInfo.shn")) + 1;

    // Eigenes Ladebild als loading/<Name>.<ext> im Projekt; MapViewInfo.Loading zeigt dann auf <Name>.
    // Je Variante des Vorlagen-Ladebilds (PNG/TGA) entsteht eine Datei mit deren Größe und Format: aus
    // dem gewählten Quellbild (newMapLoadingSource) umgerechnet, sonst eine unveränderte Kopie der
    // Vorlage. Ersetzen lässt es sich danach im Ladebildschirm-Editor, ohne andere Karten zu berühren.
    std::string ownLoading;
    if (state.newMapWriteMapInfo && tmplId >= 0) {
        std::string tmplLoading;
        if (const int di = FindOrLoadShnDoc(state, "MapViewInfo.shn", EditorState::ShnSource::Client); di >= 0) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
            for (std::size_t r = 0; r < f.rows.size(); ++r)
                if (std::atoll(TrimAscii(ShnCellText(f, r, "ID")).c_str()) == tmplId) tmplLoading = TrimAscii(ShnCellText(f, r, "Loading"));
        }
        const auto variants = tmplLoading.empty() ? std::vector<std::string>{} : LoadingImageFiles(state, tmplLoading);
        auto readBytes = [](const std::filesystem::path& p) {
            std::ifstream in(p, std::ios::binary);
            return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        };
        // Bytes je Variante: aus dem Quellbild, wenn es sich für alle Varianten umrechnen lässt.
        std::vector<std::vector<std::uint8_t>> converted;
        std::vector<std::string> formats;
        const std::string sourcePath = TrimAscii(state.newMapLoadingSource);
        if (!sourcePath.empty() && !variants.empty()) {
            auto source = core::LoadImageFile(sourcePath);
            const auto sourceFormat = core::InspectImageFile(sourcePath);
            std::string error = source ? std::string() : source.error();
            for (const auto& rel : variants) {
                if (!error.empty()) break;
                const auto original = core::InspectImageFile(ResmenuEffectivePath(state, rel));
                if (!original) { error = rel + ": " + original.error(); break; }
                // Hat die Quelle schon exakt Format und Größe der Vorlage, wird sie unverändert übernommen
                // (keine erneute verlustbehaftete Kompression) - wie im Ersetzen-Dialog.
                const auto& o = *original;
                const bool identical = sourceFormat && sourceFormat->container == o.container && sourceFormat->width == o.width &&
                                       sourceFormat->height == o.height &&
                                       (o.container != core::ImageContainer::Dds || (sourceFormat->dds == o.dds && sourceFormat->mipmaps == o.mipmaps)) &&
                                       (o.container != core::ImageContainer::Tga || sourceFormat->tgaBits == o.tgaBits) &&
                                       (o.container != core::ImageContainer::Png || sourceFormat->pngAlpha == o.pngAlpha);
                if (identical) {
                    converted.push_back(readBytes(sourcePath));
                } else if (auto built = core::BuildReplacement(*source, o, {core::FitMode::Fill, true})) {
                    converted.push_back(std::move(built->bytes));
                } else {
                    error = rel + ": " + built.error();
                    break;
                }
                formats.push_back(core::DescribeImageFormat(o));
            }
            if (!error.empty() || converted.size() != variants.size()) {
                converted.clear();
                rep.push_back(std::string(L("- Ladebild: Quellbild nicht verwendbar (", "- Loading image: source image not usable (")) + error +
                              L("), Vorlage wird kopiert", "), copying the template"));
            }
        }
        int written = 0;
        for (std::size_t i = 0; i < variants.size(); ++i) {
            const auto& rel = variants[i];
            const std::vector<std::uint8_t> bytes = converted.empty() ? readBytes(ResmenuEffectivePath(state, rel)) : converted[i];
            const std::string target = "loading/" + name + std::filesystem::path(rel).extension().string();
            const auto projectPath = InterfaceProjectOverridePath(state, target);
            std::error_code ec;
            std::filesystem::create_directories(projectPath.parent_path(), ec);
            std::ofstream out(projectPath, std::ios::binary);
            out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!bytes.empty() && out) ++written;
        }
        if (written > 0 && written == static_cast<int>(variants.size())) {
            ownLoading = name;
            ++state.resmenuRevision;
            if (converted.empty()) {
                rep.push_back("+ Client resmenu/loading/" + name + L(": eigenes Ladebild (Kopie von ", ": own loading image (copy of ") +
                              tmplLoading + ", " + std::to_string(written) + L(" Datei(en))", " file(s))"));
            } else {
                std::string list;
                for (const auto& f : formats) list += (list.empty() ? "" : "; ") + f;
                rep.push_back("+ Client resmenu/loading/" + name + L(": eigenes Ladebild aus ", ": own loading image from ") +
                              std::filesystem::path(sourcePath).filename().string() + " (" + list + ")");
            }
        } else {
            rep.push_back(std::string(L("- Ladebild: keine Datei der Vorlage gefunden, Karte nutzt ", "- Loading image: no template file found, map uses ")) +
                          (tmplLoading.empty() ? std::string("-") : tmplLoading));
        }
    }
    const std::string display = state.newMapDisplayName[0] ? state.newMapDisplayName : name;
    if (state.newMapWriteMapInfo) {
        if (tmplId < 0) {
            rep.push_back(L("- MapInfo: Vorlage nicht gefunden, keine Einträge.", "- MapInfo: template not found, no entries."));
        } else {
            AddMapShnRows(state, "MapInfo.shn", tmplId, tmplName, newId, name,
                          {{"Name", display}, {"MapFolderName", name}, {"RegenX", std::to_string(centerX)},
                           {"RegenY", std::to_string(centerY)}, {"IsWMLink", "0"}, {"KingdomMap", "0"}, {"InSide", "0"}}, rep);
            // MinimapView 0, bis die Minimap erzeugt ist (GenerateMinimap setzt 1 und den Ausschnitt).
            std::vector<std::pair<const char*, std::string>> viewCells = {{"MapFolderName", name}, {"MinimapView", "0"}, {"WorldMapView", "0"}};
            if (!ownLoading.empty()) viewCells.emplace_back("Loading", ownLoading);
            AddMapShnRows(state, "MapViewInfo.shn", tmplId, tmplName, newId, name, viewCells, rep);
        }
    }

    // World/Field.txt: FieldList-Zeile der Vorlage klonen. Serial ist eindeutig, CheckSum == Serial
    // (belegt an allen 153 NA2016-Einträgen); xsize/ysize = Blöcke.
    if (state.newMapWriteField && !state.shineTextRoot.empty()) {
        const auto source = std::filesystem::path(state.shineTextRoot) / "World" / "Field.txt";
        auto field = core::legacy::LoadShineTextFile(PreferProjectOverride(state.project, core::ProjectOutputSide::Server, source));
        auto* table = field ? field->FindTable("FieldList") : nullptr;
        const int cMap = table ? ShineColumn(*table, "MapIDClient") : -1, cSerial = table ? ShineColumn(*table, "Serial") : -1;
        const core::legacy::ShineRecord* tmplRec = nullptr;
        long long maxSerial = -1;
        if (table && cMap >= 0 && cSerial >= 0) {
            for (const auto& rec : table->records) {
                if (rec.values.size() <= static_cast<std::size_t>(std::max(cMap, cSerial))) continue;
                maxSerial = std::max(maxSerial, std::atoll(rec.values[static_cast<std::size_t>(cSerial)].c_str()));
                if (!tmplRec && SameMapName(rec.values[static_cast<std::size_t>(cMap)], tmplName.empty() ? tmplEntry.name : tmplName))
                    tmplRec = &rec;
            }
        }
        if (!tmplRec) {
            rep.push_back(L("- Field.txt: Vorlage nicht gefunden, kein Eintrag.", "- Field.txt: template not found, no entry."));
        } else {
            core::legacy::ShineRecord rec = *tmplRec;
            rec.sourceLine = 0;
            rec.styleLine = tmplRec->sourceLine; // Form der Vorlagenzeile (Leerzeichen hinter Zahlen, Auffüllung)
            const std::string serial = std::to_string(maxSerial + 1);
            const std::string blocks = std::to_string(points - 1);
            SetShineValue(rec, *table, "MapIDClient", name);
            SetShineValue(rec, *table, "Serial", serial);
            SetShineValue(rec, *table, "CheckSum", serial);
            SetShineValue(rec, *table, "MapName", ShineTextName(display));
            SetShineValue(rec, *table, "xsize", blocks);
            SetShineValue(rec, *table, "ysize", blocks);
            SetShineValue(rec, *table, "RegenCity", name);
            SetShineValue(rec, *table, "RegenXA", std::to_string(centerX));
            SetShineValue(rec, *table, "RegenYA", std::to_string(centerY));
            table->records.push_back(std::move(rec));
            auto saved = SaveProjectShineOverride(*field, ProjectServerShinePath(state.project, std::filesystem::path("World") / "Field.txt"));
            rep.push_back(saved ? "+ Server World/Field.txt: FieldList " + name + " (Serial " + serial + ")"
                                : "- FEHLER Field.txt: " + saved.error());
        }
    }

    // Optional: Gate von einer vorhandenen Karte und zurück.
    if (state.newMapGate) {
        EnsureNpcTextLoaded(state);
        auto* npcs = state.npcTextFile.FindTable("ShineNPC");
        auto* links = state.npcTextFile.FindTable("LinkTable");
        const std::string from = TrimAscii(state.newMapGateFrom);
        if (!npcs || !links || from.empty()) {
            rep.push_back(L("- Gate: NPC.txt oder Ausgangskarte fehlt.", "- Gate: NPC.txt or source map missing."));
        } else {
            const LinkTableColumns lc = FindLinkTableColumns(*links);
            const auto addLink = [&](const std::string& arg, const std::string& map, int x, int y) {
                core::legacy::ShineRecord rec;
                rec.sourceLine = 0;
                rec.values.assign(std::max<std::size_t>(links->columns.size(), static_cast<std::size_t>(lc.party) + 1), "");
                rec.values[static_cast<std::size_t>(lc.argument)] = arg;
                rec.values[static_cast<std::size_t>(lc.mapServer)] = map;
                rec.values[static_cast<std::size_t>(lc.mapClient)] = map;
                rec.values[static_cast<std::size_t>(lc.x)] = std::to_string(x);
                rec.values[static_cast<std::size_t>(lc.y)] = std::to_string(y);
                rec.values[static_cast<std::size_t>(lc.direct)] = "0";
                rec.values[static_cast<std::size_t>(lc.party)] = "1";
                links->records.push_back(std::move(rec));
            };
            const auto addGate = [&](const std::string& map, int x, int y, const std::string& arg) {
                core::legacy::ShineRecord rec;
                rec.sourceLine = 0;
                rec.values = {"MapLinkGate", map, std::to_string(x), std::to_string(y), "0", "0", "Gate", arg};
                npcs->records.push_back(std::move(rec));
            };
            const std::string toNew = "Gate" + name + "1", back = "Gate" + name + "2";
            // Ankunft 150 Einheiten neben dem jeweiligen Gate, damit man nicht sofort zurückspringt.
            addGate(from, state.newMapGateX, state.newMapGateY, toNew);
            addLink(toNew, name, centerX, centerY - 150);
            addGate(name, centerX, centerY + 150, back);
            addLink(back, from, state.newMapGateX, state.newMapGateY - 150);
            auto saved = SaveProjectShineOverride(state.npcTextFile, ProjectServerShinePath(state.project, std::filesystem::path("World") / "NPC.txt"));
            rep.push_back(saved ? "+ Server World/NPC.txt: Gate " + from + " (" + std::to_string(state.newMapGateX) + "/" +
                                      std::to_string(state.newMapGateY) + ") -> " + name + ", Rückweg " + name + " -> " + from
                                : "- FEHLER NPC.txt: " + saved.error());
        }
    }

    // Minimap: wird erzeugt, sobald die Karte im 3D-Bild geladen ist (ProcessPendingMinimap); setzt
    // dann MinimapView 1 und schreibt resmenu/minimap/<Karte>.dds ins Projekt.
    state.minimapPendingGenerate = true;
    rep.push_back(L("Minimap: wird nach dem Laden erzeugt (resmenu/minimap/", "Minimap: generated after loading (resmenu/minimap/") + name + ".dds).");
    // Neue Karte in der Kartenliste (Projekt) sichtbar machen.
    MergeProjectOnlyMaps(state.project, state.discoveredMaps);
    state.mapInfoLoaded = false; // MapInfo mit dem neuen Eintrag neu lesen
    state.npcRenderSetForMap.clear();
    state.statusMessage = std::string(L("Neue Karte '", "New map '")) + name + L("' angelegt und ins Projekt geschrieben.",
                                                                                  "' created and written to the project.");
    state.screen = AppScreen::MapEditorWorkspace;
    return true;
}

} // namespace nge
