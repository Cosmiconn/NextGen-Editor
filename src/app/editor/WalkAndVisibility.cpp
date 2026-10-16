// WalkAndVisibility.cpp
// Objekt-Kategorien, Sichtbarkeit, Fußabdrücke, Block&Walk neu berechnen, Minimap-Vorschau.

#include "EditorShared.hpp"

namespace nge {

const char* ObjectCategoryDisplayName(int category) {
    switch (category) {
        case kCatTrees: return L("Bäume & Büsche","Trees & bushes");
        case kCatGrass: return L("Gras & Blumen","Grass & flowers");
        case kCatRocks: return L("Felsen & Steine","Rocks & stones");
        case kCatBuildings: return L("Gebäude","Buildings");
        case kCatFences: return L("Zäune, Mauern & Brücken","Fences, walls & bridges");
        case kCatProps: return L("Dekoration & Möbel","Decor & furniture");
        case kCatWater: return L("Wasser & Schiffe","Water & ships");
        case kCatCreatures: return L("Tiere & Kreaturen","Animals & creatures");
        case kCatEffects: return L("Effekte & Licht","Effects & light");
        default: return L("Sonstiges","Other");
    }
}
int ClassifyObjectModel(const std::string& modelPath) {
    std::string name = LowerAscii(modelPath);
    if (const auto slash = name.find_last_of("\\/"); slash != std::string::npos) name = name.substr(slash + 1);
    if (const auto dot = name.rfind('.'); dot != std::string::npos) name.resize(dot);
    auto has = [&](std::initializer_list<const char*> words) {
        for (const char* w : words) if (name.find(w) != std::string::npos) return true;
        return false;
    };
    if (has({"lightbug", "effect", "fire", "smoke", "glow", "particle", "light", "torch", "flame", "sparkle", "spark", "cloud", "sky", "fog"})) return kCatEffects;
    if (has({"flowerpot", "pot", "box", "barrel", "chair", "table", "lamp", "sign", "banner", "bench", "cart", "chest", "crate", "statue", "fountain", "bed", "book", "bottle", "ladder", "flag", "sack", "bucket", "basket", "shelf", "stove", "anvil", "ticker", "machine", "post_lamp", "rope", "candle", "drum", "tube", "wool", "totem", "umbrella", "wagon", "satue", "nest", "notice", "swing", "garden", "safety", "portal"})) return kCatProps;
    if (has({"waterfall", "water", "ship", "boat", "sea", "dock", "anchor", "wave", "sail", "harbor", "fishing", "raft"})) return kCatWater;
    if (has({"chicken", "cat", "dog", "bird", "pig", "horse", "duck", "frog", "mouse", "rabbit", "crab", "seagull", "butterfly", "bee", "fish", "cow", "sheep", "goat", "boar", "bug", "spider", "snake", "wolf", "bat", "butter", "dove", "elephant", "beaver", "turtle", "fox", "cheeta", "deer", "pigeon", "lizard", "monkey"})) return kCatCreatures;
    if (has({"grass", "flower", "flws", "weed", "reed", "lawn", "clover", "mushroom", "fern", "crop", "wheat", "jabpul"})) return kCatGrass;
    if (has({"tree", "palm", "bush", "plant", "leaf", "leaves", "vine", "bamboo", "cactus", "sapling", "wood", "log", "stump", "branch", "shrub", "hedge", "bark"})) return kCatTrees;
    if (has({"rock", "stone", "boulder", "cliff", "cave", "mountain", "pebble", "crystal", "spike", "coral", "reef", "ore", "ston", "dolmen"})) return kCatRocks;
    if (has({"fence", "wall", "gate", "bridge", "stair", "door", "pillar", "arch", "rail", "tunnel", "fort", "barrier", "post"})) return kCatFences;
    if (has({"house", "hall", "shop", "tower", "build", "cabin", "tent", "church", "castle", "smith", "inn", "temple", "hut", "mill", "barn", "stable", "guild", "monolith", "altar", "lodge", "villa", "hotel", "office", "store", "roof", "market", "santuary", "sanctuary", "pavilion"})) return kCatBuildings;
    return kCatOther;
}
// Berechnet (bei Aenderung) Kategorie und "ausgeblendet" je Objekt neu.
void RefreshObjectVisibility(EditorState& state) {
    SyncObjectEditorMetadata(state);
    std::string key = std::to_string(state.placementSet.Count());
    for (int c = 0; c < kCatCount; ++c) key += state.categoryVisible[c] ? '1' : '0';
    for (char h : state.objectEditorHidden) key += h ? 'h' : '-';
    if (key == state.objectVisKey) return;
    state.objectVisKey = key;
    const std::size_t n = state.placementSet.Count();
    if (state.objectCategory.size() != n) {
        state.objectCategory.assign(n, kCatOther);
        for (std::size_t i = 0; i < n; ++i) {
            const std::string& path = state.placementSet.At(i).modelPath;
            auto it = state.modelCategoryCache.find(path);
            if (it == state.modelCategoryCache.end()) it = state.modelCategoryCache.emplace(path, ClassifyObjectModel(path)).first;
            state.objectCategory[i] = it->second;
        }
    }
    state.objectHidden.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const bool categoryHidden = !state.categoryVisible[state.objectCategory[i]];
        const bool editorHidden = i < state.objectEditorHidden.size() && state.objectEditorHidden[i] != 0;
        state.objectHidden[i] = (categoryHidden || editorHidden) ? 1 : 0;
    }
}
bool IsObjectHidden(const EditorState& state, std::size_t i) {
    return i < state.objectHidden.size() && state.objectHidden[i] != 0;
}
// Bereich "Sichtbarkeit" im Werkzeug-Panel des Map-Editors.
void DrawVisibilityPanel(EditorState& state) {
    RefreshObjectVisibility(state);
    const bool terrainAvailable = MapSupportsTerrainEditing(state);
    if (!terrainAvailable) state.showTerrain = false;
    ImGui::BeginDisabled(!terrainAvailable);
    UI::Checkbox("Terrain", &state.showTerrain);
    ImGui::EndDisabled();
    UI::Checkbox(L("Objekt-Modelle (3D)","Object models (3D)"), &state.showObjectMeshes);
    UI::Checkbox(L("Objekt-Platzhalter (3D)","Object markers (3D)"), &state.showObjectMarkers);

    if (!state.shmdCategoryRenderKind.empty()) {
        int shmdCounts[3] = {};
        for (const int kind : state.shmdCategoryRenderKind) {
            if (kind >= 0 && kind < 3) ++shmdCounts[kind];
        }
        ImGui::SeparatorText(L("SHMD-Szenenmodelle","SHMD scene models"));
        UI::Checkbox("Sky (SHMD)", &state.showShmdSky);
        ImGui::SameLine(); ImGui::TextDisabled(L("%d Modell(e)","%d model(s)"), shmdCounts[0]);
        UI::Checkbox("Water (SHMD)", &state.showShmdWater);
        ImGui::SameLine(); ImGui::TextDisabled(L("%d Modell(e)","%d model(s)"), shmdCounts[1]);
        UI::Checkbox("GroundObject (SHMD)", &state.showShmdGroundObject);
        ImGui::SameLine(); ImGui::TextDisabled(L("%d Modell(e)","%d model(s)"), shmdCounts[2]);
    }

    UI::Checkbox(L("NPC-Modelle (3D)","NPC models (3D)"), &state.showNpcModels);
    UI::Checkbox(L("NPC-Namen und Blickpfeile (3D, NPC-Modus)","NPC names and facing arrows (3D, NPC mode)"), &state.showNpcLabels);
    UI::Checkbox(L("Patrouillen-/Roam-Routen (2D/3D, Auswahl)","Patrol / roam routes (2D/3D, selection)"), &state.showRoamRoutes);
    if (state.npcModelsMissing > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), L("%d NPC(s) ohne Modell (Platzhalter fehlen):","%d NPC(s) without model (placeholder missing):"), state.npcModelsMissing);
        for (const auto& n : state.npcModelsMissingNames) ImGui::BulletText("%s", n.c_str());
    }
    UI::Checkbox(L("Objekte im 2D-View","Objects in 2D view"), &state.showObjects2D);

    if (state.textureStack.LayerCount() > 0) {
        ImGui::SeparatorText(L("Terrain-Layer","Terrain layers"));
        state.layerHidden.resize(state.textureStack.LayerCount(), 0);
        for (std::size_t i = 0; i < state.textureStack.LayerCount(); ++i) {
            bool visible = state.layerHidden[i] == 0;
            const std::string label = state.textureStack.Layer(i).name + "##vislayer" + std::to_string(i);
            if (UI::Checkbox(label.c_str(), &visible)) state.layerHidden[i] = visible ? 0 : 1;
        }
        if (UI::SmallButton(L("Alle Layer an","Show all layers"))) std::fill(state.layerHidden.begin(), state.layerHidden.end(), 0);
    }

    if (state.placementSet.Count() > 0) {
        ImGui::SeparatorText(L("Objekt-Kategorien","Object categories"));
        int counts[kCatCount] = {};
        for (const int c : state.objectCategory) if (c >= 0 && c < kCatCount) ++counts[c];
        for (int c = 0; c < kCatCount; ++c) {
            if (counts[c] == 0) continue;
            ImGui::PushID(c);
            UI::Checkbox((std::string(ObjectCategoryDisplayName(c)) + " (" + std::to_string(counts[c]) + ")").c_str(), &state.categoryVisible[c]);
            ImGui::SameLine();
            if (UI::SmallButton(L("nur","only"))) { for (int k = 0; k < kCatCount; ++k) state.categoryVisible[k] = (k == c); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",L("Nur diese Kategorie zeigen","Show only this category"));
            ImGui::PopID();
        }
        if (UI::SmallButton(L("Alle an","All on"))) for (bool& v : state.categoryVisible) v = true;
        ImGui::SameLine();
        if (UI::SmallButton(L("Alle aus","All off"))) for (bool& v : state.categoryVisible) v = false;
    }
}
// Berechnet (gecacht) die Grundfläche eines Objekt-Modells als Bounding-Box in lokalen X/Z-
// Koordinaten, direkt aus den echten NIF-Vertex-Positionen (core::NifMeshPart::positions) -
// siehe CHANGELOG [0.44.24], Nutzerwunsch "die Grundfläche die den Boden berührt". NIF-Daten
// sind bereits Y-up (X/Z = horizontale Ebene, siehe Achsen-Remap-Doku), passend zur 2D-
// Draufsicht. Auflösung nutzt denselben Kartenordner (state.legacySaveDir) wie
// NifMeshRenderer::LoadModelsForSet für die 3D-Anzeige. Liefert valid=false, wenn Datei nicht
// geladen werden kann (z.B. Model noch nicht im lokal bekannten Assets) - Aufrufer fällt dann
// auf den einfachen Punkt-Marker zurück, kein Fehler.
const EditorState::ObjectFootprint& GetOrComputeFootprint(EditorState& state, const std::string& modelPath) {
    auto it = state.footprintCache.find(modelPath);
    if (it != state.footprintCache.end()) return it->second;

    EditorState::ObjectFootprint fp;
    const auto mapDir = CurrentObjectAssetMapDir(state);
    if (!mapDir.empty()) {
        auto resolved = core::legacy::ResolveLegacyAssetPath(mapDir, modelPath);
        if (resolved) {
            auto model = core::LoadNifMesh(*resolved);
            if (model) {
                bool first = true;
                for (auto& part : model->parts) {
                    for (auto& p : part.positions) {
                        if (first) {
                            fp.minX = fp.maxX = p.x; fp.minY = fp.maxY = p.y; fp.minZ = fp.maxZ = p.z;
                            first = false;
                        } else {
                            fp.minX = std::min(fp.minX, p.x); fp.maxX = std::max(fp.maxX, p.x);
                            fp.minY = std::min(fp.minY, p.y); fp.maxY = std::max(fp.maxY, p.y);
                            fp.minZ = std::min(fp.minZ, p.z); fp.maxZ = std::max(fp.maxZ, p.z);
                        }
                    }
                }
                fp.valid = !first;
                fp.contactSegments = core::ComputeGroundContactSegments(*model);
                fp.hull = core::ComputeFootprintHull(*model);
            }
        }
    }
    return state.footprintCache.emplace(modelPath, fp).first->second;
}
// Zellen einer konvexen Grundfläche im Walk-Gitter zählen (gesamt, davon in `reference` gesperrt).
std::pair<std::size_t, std::size_t> CountFootprintCells(core::WalkGrid& scratch, const core::WalkGrid& reference,
                                                               const std::vector<std::pair<float, float>>& polygon) {
    core::WalkUndoPatch patch; std::vector<std::uint64_t> seen;
    core::ApplyWalkConvexPolygon(scratch, polygon, true, patch, seen);
    const std::uint32_t cols = scratch.Cols(), rows = scratch.Rows();
    float minX = 1e30f, minZ = 1e30f, maxX = -1e30f, maxZ = -1e30f;
    for (const auto& [px, pz] : polygon) { minX = std::min(minX, px); maxX = std::max(maxX, px); minZ = std::min(minZ, pz); maxZ = std::max(maxZ, pz); }
    std::size_t cells = 0, blocked = 0;
    if (cols > 0 && rows > 0) {
        const auto cx0 = static_cast<std::uint32_t>(std::clamp(minX / core::WalkGrid::kCellSize, 0.0f, cols - 1.0f));
        const auto cx1 = static_cast<std::uint32_t>(std::clamp(maxX / core::WalkGrid::kCellSize, 0.0f, cols - 1.0f));
        const auto cz0 = static_cast<std::uint32_t>(std::clamp(minZ / core::WalkGrid::kCellSize, 0.0f, rows - 1.0f));
        const auto cz1 = static_cast<std::uint32_t>(std::clamp(maxZ / core::WalkGrid::kCellSize, 0.0f, rows - 1.0f));
        for (std::uint32_t cz = cz0; cz <= cz1; ++cz)
            for (std::uint32_t cx = cx0; cx <= cx1; ++cx)
                if (scratch.CellBlocked(cx, cz)) { ++cells; if (reference.CellBlocked(cx, cz)) ++blocked; }
    }
    core::RevertWalkPatch(scratch, patch);
    return {cells, blocked};
}
// Je Modell: Zellen aller Grundflächen und Anteil, der im beim Laden vorhandenen SHBD gesperrt
// war. Belegt an Roumen: Gebäude 92-99 %, Gras 23 %, Schiff auf dem Meer 16 %.
const std::unordered_map<std::string, std::pair<std::size_t, std::size_t>>& ModelBlockStats(EditorState& state) {
    const std::string key = std::string(state.legacySaveStem) + "|" + std::to_string(state.placementSet.Count()) + "|" +
                            std::to_string(state.walkGridAtLoad.Width()) + "x" + std::to_string(state.walkGridAtLoad.Height());
    if (key == state.modelBlockStatsKey) return state.modelBlockStats;
    state.modelBlockStatsKey = key;
    state.modelBlockStats.clear();
    if (state.walkGridAtLoad.Width() == 0) return state.modelBlockStats;
    core::WalkGrid scratch(state.walkGridAtLoad.Width(), state.walkGridAtLoad.Height(), 0);
    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
        const auto& obj = state.placementSet.At(i);
        if (obj.sourceIndex < 0) continue; // neu platzierte Objekte haben keinen Vergleich im Original
        const auto polygon = ObjectFootprintWorldPolygon(state, obj);
        if (polygon.size() < 3) continue;
        const auto [cells, blocked] = CountFootprintCells(scratch, state.walkGridAtLoad, polygon);
        auto& agg = state.modelBlockStats[LowerAscii(obj.modelPath)];
        agg.first += cells; agg.second += blocked;
    }
    return state.modelBlockStats;
}
// true, wenn das Modell laut Originaldaten nicht blockiert (< 50 % der Grundflächenzellen gesperrt).
bool ModelWalkableInSource(EditorState& state, const std::string& modelPath) {
    const auto& stats = ModelBlockStats(state);
    const auto it = stats.find(LowerAscii(modelPath));
    return it != stats.end() && it->second.first > 0 && it->second.second * 2 < it->second.first;
}
std::vector<std::vector<std::pair<float, float>>> CollectVisibleObjectFootprints(EditorState& state) {
    std::vector<std::vector<std::pair<float, float>>> polygons;
    RefreshObjectVisibility(state);
    polygons.reserve(state.placementSet.Count() + state.shmdCategoryRenderSet.Count());
    const bool filter = state.walkFootprintOnlyBlocking && state.walkFootprintPreviewBlocked;

    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
        if (IsObjectHidden(state, i)) continue;
        if (filter && ModelWalkableInSource(state, state.placementSet.At(i).modelPath)) continue;
        auto polygon = ObjectFootprintWorldPolygon(state, state.placementSet.At(i));
        if (polygon.size() >= 3) polygons.push_back(std::move(polygon));
    }

    RefreshShmdCategoryVisibility(state);
    for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
        if (i < state.shmdCategoryHidden.size() && state.shmdCategoryHidden[i]) continue;
        auto polygon = ObjectFootprintWorldPolygon(state, state.shmdCategoryRenderSet.At(i));
        if (polygon.size() >= 3) polygons.push_back(std::move(polygon));
    }
    return polygons;
}
void StampObjectFootprints(EditorState& state, bool blocked) {
    const auto polygons = CollectVisibleObjectFootprints(state);
    core::WalkUndoPatch patch;
    std::vector<std::uint64_t> seen;
    for (const auto& polygon : polygons)
        core::ApplyWalkConvexPolygon(state.walkGrid, polygon, blocked, patch, seen);

    if (!patch.entries.empty()) {
        // All visible footprints are deliberately aggregated into one patch: Apply is one
        // semantic user action and therefore exactly one undo step.
        PushWalkHistory(state, std::move(patch));
        state.walkPreviewDirty = true;
        state.mapDirty = true;
        state.statusMessage = std::to_string(polygons.size()) +
            (blocked ? L(" Objekt-Grundflächen gesperrt. Ein Undo-Schritt.",
                         " object footprints blocked. One undo step.")
                     : L(" Objekt-Grundflächen freigegeben. Ein Undo-Schritt.",
                         " object footprints made walkable. One undo step."));
    } else {
        state.statusMessage = L("Keine Walk-/Block-Zellen durch die sichtbaren Grundflächen geändert.",
                                "No Walk/Block cells changed by the visible footprints.");
    }
}
// ============================================================================
// Block&Walk aus Terrain und Objekten neu berechnen (core/WalkGenerate). Standard: nur dort, wo sich
// Terrain oder Objekte seit dem Laden geändert haben; Handarbeit im Original (Meer, Wege) bleibt.
// Ergebnis erst als Vorschau (Vergleichsfarben im 2D-View), Übernehmen = ein Undo-Schritt.
// ============================================================================
std::string ObjectChangeKey(const core::PlacedObject& o) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "|%.2f|%.2f|%.2f|%.4f|%.4f|%.4f|%.4f|%.4f", o.posX, o.posY, o.posZ, o.rotX, o.rotY,
                  o.rotZ, o.rotW, o.scale);
    return LowerAscii(o.modelPath) + buf;
}
void StampPolygonsBlocked(core::WalkGrid& grid, const std::vector<std::vector<std::pair<float, float>>>& polygons) {
    core::WalkUndoPatch patch;
    std::vector<std::uint64_t> seen;
    for (const auto& polygon : polygons) core::ApplyWalkConvexPolygon(grid, polygon, true, patch, seen);
}
void ComputeWalkProposal(EditorState& state) {
    state.walkProposal.reset();
    if (!MapSupportsTerrainEditing(state) || state.walkGrid.Cols() == 0 || state.walkGrid.Rows() == 0) {
        state.walkGenStatus = L("Keine Karte mit Terrain und Block&Walk-Gitter geladen.", "No map with terrain and Block&Walk grid loaded.");
        return;
    }
    const auto blocking = [&](const core::PlacedObject& o) {
        return !(state.walkFootprintOnlyBlocking && ModelWalkableInSource(state, o.modelPath));
    };
    using Polygons = std::vector<std::vector<std::pair<float, float>>>;
    const auto emptyGrid = [&] { return core::WalkGrid(state.walkGrid.Width(), state.walkGrid.Height(), 0); };
    core::WalkGrid result;
    if (state.walkGenScope == 0) {
        core::WalkRegion region = core::TerrainChangeRegion(state.walkGrid, state.heightmapAtLoad, state.heightmap);
        const std::size_t terrainCells = core::RegionCellCount(region);
        core::WalkGrid add, remove;
        std::size_t changedObjects = 0;
        if (state.walkGenObjects) {
            // Objekte über Modell+Transformation abgleichen (unabhängig von der SHMD-Reihenfolge).
            std::unordered_map<std::string, int> loadKeys, currentKeys;
            for (std::size_t i = 0; i < state.placementAtLoad.Count(); ++i) ++loadKeys[ObjectChangeKey(state.placementAtLoad.At(i))];
            for (std::size_t i = 0; i < state.placementSet.Count(); ++i) ++currentKeys[ObjectChangeKey(state.placementSet.At(i))];
            Polygons addPolys, removePolys, coverPolys;
            for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
                const auto& o = state.placementSet.At(i);
                if (!blocking(o)) continue;
                auto polygon = ObjectFootprintWorldPolygon(state, o);
                if (polygon.size() < 3) continue;
                auto it = loadKeys.find(ObjectChangeKey(o));
                if (it != loadKeys.end() && it->second > 0) --it->second;
                else addPolys.push_back(polygon);
                coverPolys.push_back(std::move(polygon));
            }
            for (std::size_t i = 0; i < state.placementAtLoad.Count(); ++i) {
                const auto& o = state.placementAtLoad.At(i);
                auto it = currentKeys.find(ObjectChangeKey(o));
                if (it != currentKeys.end() && it->second > 0) { --it->second; continue; }
                if (!blocking(o)) continue;
                auto polygon = ObjectFootprintWorldPolygon(state, o);
                if (polygon.size() >= 3) removePolys.push_back(std::move(polygon));
            }
            changedObjects = addPolys.size() + removePolys.size();
            add = emptyGrid();
            remove = emptyGrid();
            StampPolygonsBlocked(add, addPolys);
            StampPolygonsBlocked(remove, removePolys);
            if (!removePolys.empty()) {
                // Freigeben nur, wo kein (anderes) aktuelles Objekt die Zelle noch abdeckt.
                core::WalkGrid cover = emptyGrid();
                StampPolygonsBlocked(cover, coverPolys);
                for (std::uint32_t z = 0; z < remove.Height(); ++z)
                    for (std::uint32_t w = 0; w < remove.Width(); ++w)
                        remove.Set(w, z, static_cast<std::int16_t>(static_cast<std::uint16_t>(remove.At(w, z)) &
                                                                   ~static_cast<std::uint16_t>(cover.At(w, z))));
            }
            for (const Polygons* list : {&addPolys, &removePolys}) {
                for (const auto& polygon : *list) {
                    float x0 = polygon[0].first, x1 = x0, z0 = polygon[0].second, z1 = z0;
                    for (const auto& [px, pz] : polygon) {
                        x0 = std::min(x0, px); x1 = std::max(x1, px);
                        z0 = std::min(z0, pz); z1 = std::max(z1, pz);
                    }
                    core::AddRectToRegion(region, state.walkGrid, x0, z0, x1, z1);
                }
            }
        }
        state.walkProposalRegionCells = core::RegionCellCount(region);
        result = core::UpdateWalkGridForChanges(state.walkGrid, state.heightmapAtLoad, state.heightmap, state.walkGenSettings,
                                                region, add, remove);
        char buf[200];
        std::snprintf(buf, sizeof(buf), L("Geänderter Bereich: %zu Zellen (Terrain %zu), %zu geänderte Objekt-Grundflächen.",
                                          "Changed area: %zu cells (terrain %zu), %zu changed object footprints."),
                      state.walkProposalRegionCells, terrainCells, changedObjects);
        state.walkGenStatus = buf;
    } else {
        core::WalkGrid extra;
        if (state.walkGenObjects) {
            Polygons polys;
            for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
                const auto& o = state.placementSet.At(i);
                if (!blocking(o)) continue;
                auto polygon = ObjectFootprintWorldPolygon(state, o);
                if (polygon.size() >= 3) polys.push_back(std::move(polygon));
            }
            extra = emptyGrid();
            StampPolygonsBlocked(extra, polys);
        }
        result = core::GenerateWalkGrid(state.walkGrid, state.heightmap, state.walkGenSettings, nullptr, extra);
        state.walkProposalRegionCells = static_cast<std::size_t>(state.walkGrid.Cols()) * state.walkGrid.Rows();
        state.walkGenStatus = L("Ganze Karte aus Terrain-Regeln und Objekt-Grundflächen neu berechnet.",
                                "Whole map recomputed from terrain rules and object footprints.");
    }
    state.walkProposalDiff = core::DiffWalkGrids(state.walkGrid, result);
    state.walkProposalVsOriginal = core::DiffWalkGrids(state.walkGridAtLoad, result);
    state.walkProposal = std::move(result);
    state.walkDiffDirty = true;
}
void ApplyWalkProposal(EditorState& state) {
    if (!state.walkProposal) return;
    const auto& proposal = *state.walkProposal;
    if (proposal.Width() != state.walkGrid.Width() || proposal.Height() != state.walkGrid.Height()) return;
    core::WalkUndoPatch patch;
    for (std::uint32_t z = 0; z < proposal.Height(); ++z)
        for (std::uint32_t w = 0; w < proposal.Width(); ++w)
            if (proposal.At(w, z) != state.walkGrid.At(w, z)) patch.entries.push_back({w, z, state.walkGrid.At(w, z)});
    const std::size_t words = patch.entries.size();
    if (!patch.entries.empty()) {
        state.walkGrid = proposal;
        PushWalkHistory(state, std::move(patch));
        state.mapDirty = true;
        state.walkPreviewDirty = true;
        state.modelBlockStatsKey.clear();
    }
    state.walkProposal.reset();
    state.statusMessage = words ? std::string(L("Block&Walk neu berechnet übernommen (", "Recomputed Block&Walk applied (")) +
                                      std::to_string(state.walkProposalDiff.newlyBlocked) + L(" neu blockiert, ", " newly blocked, ") +
                                      std::to_string(state.walkProposalDiff.newlyFree) + L(" neu begehbar). Ein Undo-Schritt.", " newly walkable). One undo step.")
                                : std::string(L("Neu berechnet: keine Änderung.", "Recomputed: no change."));
}
// Vergleichstextur (R8): 0 bleibt begehbar, 85 bleibt blockiert, 170 neu blockiert, 255 neu begehbar.
void UpdateWalkDiffTexture(EditorState& state) {
    if (!state.walkProposal) return;
    const auto& a = state.walkGrid;
    const auto& b = *state.walkProposal;
    const std::uint32_t cols = a.Cols(), rows = a.Rows();
    if (cols == 0 || rows == 0 || b.Cols() != cols || b.Rows() != rows) return;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(cols) * rows);
    for (std::uint32_t z = 0; z < rows; ++z)
        for (std::uint32_t x = 0; x < cols; ++x) {
            const bool was = a.CellBlocked(x, z), now = b.CellBlocked(x, z);
            pixels[static_cast<std::size_t>(z) * cols + x] = was ? (now ? 85 : 255) : (now ? 170 : 0);
        }
    if (state.walkDiffTex == 0) glGenTextures(1, &state.walkDiffTex);
    glBindTexture(GL_TEXTURE_2D, state.walkDiffTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(cols), static_cast<int>(rows), 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    state.walkDiffDirty = false;
}
void DrawWalkGeneratePanel(EditorState& state) {
    ImGui::SeparatorText(L("Aus Terrain und Objekten neu berechnen", "Recompute from terrain and objects"));
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat(L("Max. Neigung (°)##walkGen", "Max. slope (°)##walkGen"), &state.walkGenSettings.maxSlopeDegrees, 5.0f, 80.0f, "%.0f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", L("Steiler = blockiert. Gemessen: auf Roumen sind 98 % der Zellen steiler als 30° im Original blockiert.",
                                  "Steeper = blocked. Measured: on Roumen 98 % of the cells steeper than 30° are blocked in the original."));
    UI::Checkbox(L("Max. Höhe##walkGenMaxH", "Max. height##walkGenMaxH"), &state.walkGenSettings.useMaxHeight);
    if (state.walkGenSettings.useMaxHeight) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("##walkGenMaxHeight", &state.walkGenSettings.maxHeight, 0.0f, 0.0f, "%.0f");
    }
    UI::Checkbox(L("Min. Höhe (z.B. Wasser)##walkGenMinH", "Min. height (e.g. water)##walkGenMinH"), &state.walkGenSettings.useMinHeight);
    if (state.walkGenSettings.useMinHeight) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("##walkGenMinHeight", &state.walkGenSettings.minHeight, 0.0f, 0.0f, "%.0f");
    }
    UI::Checkbox(L("Objekt-Grundflächen einbeziehen##walkGen", "Include object footprints##walkGen"), &state.walkGenObjects);
    ImGui::RadioButton(L("Nur Geändertes##walkGenScope", "Changes only##walkGenScope"), &state.walkGenScope, 0);
    ImGui::SameLine();
    ImGui::RadioButton(L("Ganze Karte##walkGenScope", "Whole map##walkGenScope"), &state.walkGenScope, 1);
    ImGui::TextDisabled("%s", state.walkGenScope == 0
        ? L("Nur wo sich Terrain oder Objekte seit dem Laden geändert haben; gibt nur frei, was wegen Hang oder Objekt gesperrt war.",
            "Only where terrain or objects changed since loading; frees only what was blocked by slope or an object.")
        : L("Ersetzt das ganze Gitter durch Terrain-Regeln + Grundflächen - für neue Karten. Handarbeit (Meer, Wege) geht verloren.",
            "Replaces the whole grid with terrain rules + footprints - for new maps. Hand-made blocks (sea, paths) are lost."));
    if (UI::Button(L("Vorschau berechnen##walkGen", "Compute preview##walkGen"))) ComputeWalkProposal(state);
    if (!state.walkGenStatus.empty()) ImGui::TextWrapped("%s", state.walkGenStatus.c_str());
    if (!state.walkProposal) return;
    const auto& d = state.walkProposalDiff;
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), L("%zu neu blockiert", "%zu newly blocked"), d.newlyBlocked);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.45f, 1.0f), L("%zu neu begehbar", "%zu newly walkable"), d.newlyFree);
    ImGui::TextDisabled(L("Übereinstimmung mit dem Original danach: %.2f %%", "Agreement with the original afterwards: %.2f %%"),
                        state.walkProposalVsOriginal.Agreement() * 100.0);
    ImGui::TextDisabled("%s", L("2D-View: gelb = neu blockiert, grün = neu begehbar, rot = bleibt blockiert.",
                                "2D view: yellow = newly blocked, green = newly walkable, red = stays blocked."));
    if (UI::Button(L("Übernehmen##walkGen", "Apply##walkGen"))) ApplyWalkProposal(state);
    ImGui::SameLine();
    if (UI::Button(L("Verwerfen##walkGen", "Discard##walkGen"))) {
        state.walkProposal.reset();
        state.walkGenStatus.clear();
    }
}
std::vector<core::NifGroundContactSegment> ObjectGroundContactWorldSegments(
    EditorState& state, const core::PlacedObject& obj) {
    const auto& fp = GetOrComputeFootprint(state, obj.modelPath);
    if (!fp.valid || fp.contactSegments.empty()) return {};

    const float angle = 2.0f * std::atan2(obj.rotY, obj.rotW);
    const float c = std::cos(angle), s = std::sin(angle);
    auto transform = [&](float lx, float lz) {
        const float sx = lx * obj.scale;
        const float sz = lz * obj.scale;
        return std::pair<float,float>{
            obj.posX + (sx * c + sz * s),
            obj.posZ + (-sx * s + sz * c)
        };
    };

    std::vector<core::NifGroundContactSegment> world;
    world.reserve(fp.contactSegments.size());
    for (const auto& edge : fp.contactSegments) {
        const auto a = transform(edge.x0, edge.z0);
        const auto b = transform(edge.x1, edge.z1);
        world.push_back({a.first, a.second, b.first, b.second});
    }
    return world;
}
float DistanceToObjectGroundContactXZ(EditorState& state, const core::PlacedObject& obj,
                                      float worldX, float worldZ) {
    const auto& fp = GetOrComputeFootprint(state, obj.modelPath);
    if (!fp.valid || fp.contactSegments.empty())
        return std::numeric_limits<float>::infinity();

    const float angle = 2.0f * std::atan2(obj.rotY, obj.rotW);
    const float c = std::cos(angle), s = std::sin(angle);
    auto transform = [&](float lx, float lz) {
        const float sx = lx * obj.scale;
        const float sz = lz * obj.scale;
        return std::pair<float,float>{
            obj.posX + (sx * c + sz * s),
            obj.posZ + (-sx * s + sz * c)
        };
    };
    auto segmentDistance = [&](const std::pair<float,float>& a,
                               const std::pair<float,float>& b) {
        const float vx = b.first - a.first;
        const float vz = b.second - a.second;
        const float wx = worldX - a.first;
        const float wz = worldZ - a.second;
        const float len2 = vx * vx + vz * vz;
        const float t = len2 > 1.0e-8f
            ? std::clamp((wx * vx + wz * vz) / len2, 0.0f, 1.0f)
            : 0.0f;
        const float dx = worldX - (a.first + vx * t);
        const float dz = worldZ - (a.second + vz * t);
        return std::sqrt(dx * dx + dz * dz);
    };

    float best = std::numeric_limits<float>::infinity();
    for (const auto& edge : fp.contactSegments)
        best = std::min(best, segmentDistance(transform(edge.x0, edge.z0),
                                              transform(edge.x1, edge.z1)));
    return best;
}
bool DrawObjectFootprint2D(EditorState& state, const core::PlacedObject& obj, ImDrawList* drawList,
                           const ImVec2& cursorScreenPos, const ImVec2& imageSize,
                           float spanX, float spanZ, ImU32 color, float thickness) {
    if (spanX <= 0.0f || spanZ <= 0.0f) return false;
    const auto edges = ObjectGroundContactWorldSegments(state, obj);
    if (edges.empty()) return false;

    for (const auto& edge : edges) {
        const ImVec2 a(
            cursorScreenPos.x + (edge.x0 / spanX) * imageSize.x,
            cursorScreenPos.y + (1.0f - edge.z0 / spanZ) * imageSize.y);
        const ImVec2 b(
            cursorScreenPos.x + (edge.x1 / spanX) * imageSize.x,
            cursorScreenPos.y + (1.0f - edge.z1 / spanZ) * imageSize.y);
        drawList->AddLine(a, b, color, thickness);
    }
    return true;
}
std::vector<std::pair<float, float>> ObjectFootprintWorldPolygon(EditorState& state, const core::PlacedObject& obj) {
    const auto& fp = GetOrComputeFootprint(state, obj.modelPath);
    if (!fp.valid) return {};
    const float angle = 2.0f * std::atan2(obj.rotY, obj.rotW);
    const float c = std::cos(angle), s = std::sin(angle);
    std::vector<std::pair<float, float>> local = fp.hull;
    if (local.size() < 3) local = {{fp.minX, fp.minZ}, {fp.maxX, fp.minZ}, {fp.maxX, fp.maxZ}, {fp.minX, fp.maxZ}};
    std::vector<std::pair<float, float>> world;
    world.reserve(local.size());
    for (const auto& [lx, lz] : local) {
        const float sx = lx * obj.scale, sz = lz * obj.scale;
        world.emplace_back(obj.posX + (sx * c + sz * s), obj.posZ + (-sx * s + sz * c));
    }
    return world;
}
bool FocusLoadedNifSceneFromBounds(EditorState& state) {
    bool any = false;
    float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f, minZ = 0.0f, maxZ = 0.0f;
    auto includeObject = [&](const core::PlacedObject& obj) {
        const auto& fp = GetOrComputeFootprint(state, obj.modelPath);
        if (!fp.valid) return;
        const float angle = 2.0f * std::atan2(obj.rotY, obj.rotW);
        const float c = std::cos(angle), sn = std::sin(angle);
        for (float lx : {fp.minX, fp.maxX}) for (float ly : {fp.minY, fp.maxY}) for (float lz : {fp.minZ, fp.maxZ}) {
            const float sx = lx * obj.scale, sy = ly * obj.scale, sz = lz * obj.scale;
            const float x = obj.posX + sx * c + sz * sn;
            const float y = obj.posY + sy;
            const float z = obj.posZ - sx * sn + sz * c;
            if (!any) {
                minX=maxX=x; minY=maxY=y; minZ=maxZ=z; any=true;
            } else {
                minX=std::min(minX,x); maxX=std::max(maxX,x);
                minY=std::min(minY,y); maxY=std::max(maxY,y);
                minZ=std::min(minZ,z); maxZ=std::max(maxZ,z);
            }
        }
    };
    for (std::size_t i=0;i<state.placementSet.Count();++i) includeObject(state.placementSet.At(i));
    for (std::size_t i=0;i<state.shmdCategoryRenderSet.Count();++i) includeObject(state.shmdCategoryRenderSet.At(i));
    if (!any) return false;

    const float spanX=maxX-minX, spanY=maxY-minY, spanZ=maxZ-minZ;
    const float extent=std::max({spanX,spanY,spanZ,100.0f});
    state.camera.SetTarget((minX+maxX)*0.5f,(minY+maxY)*0.5f,(minZ+maxZ)*0.5f);
    state.camera.Zoom(std::clamp(extent*1.35f,200.0f,50000.0f)-state.camera.Distance());
    return true;
}
void DrawWalkFootprintPreview(EditorState& state, ImDrawList* drawList,
                                     const ImVec2& mapOrigin, const ImVec2& mapSize,
                                     float spanX, float spanZ) {
    if (!state.walkFootprintPreviewActive || spanX <= 0.0f || spanZ <= 0.0f) return;

    const auto polygons = CollectVisibleObjectFootprints(state);
    const ImU32 fill = state.walkFootprintPreviewBlocked
        ? IM_COL32(245,85,85,58)
        : IM_COL32(80,220,145,58);
    const ImU32 edge = state.walkFootprintPreviewBlocked
        ? IM_COL32(255,104,104,245)
        : IM_COL32(92,235,158,245);

    for (const auto& polygon : polygons) {
        std::vector<ImVec2> screen;
        screen.reserve(polygon.size());
        for (const auto& [wx,wz] : polygon) {
            const float u = wx / spanX;
            const float v = 1.0f - wz / spanZ;
            screen.emplace_back(mapOrigin.x + u * mapSize.x, mapOrigin.y + v * mapSize.y);
        }
        if (screen.size() < 3) continue;
        drawList->AddConvexPolyFilled(screen.data(), static_cast<int>(screen.size()), fill);
        drawList->AddPolyline(screen.data(), static_cast<int>(screen.size()), edge,
                              ImDrawFlags_Closed, 2.2f);
    }
}
void DrawMinimapPreviewContent(EditorState& state) {
    DrawPanelHeader("minimapHeader", "MINIMAP", DrawIconGrid, "panel.minimap",
                    L("Editor-Vorschau","Editor preview"));

    UI::Checkbox(L("2D-Ausschnitt","2D viewport"), &state.minimapShowViewport);
    ImGui::SameLine();
    UI::Checkbox(L("Objekte","Objects"), &state.minimapShowObjects);

    const float spanX = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth();
    const float spanZ = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight();
    if (spanX <= 0.0f || spanZ <= 0.0f) {
        ImGui::TextDisabled("%s", L("Keine Kartengeometrie geladen.","No map geometry loaded."));
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float maxW = std::max(80.0f, avail.x);
    const float maxH = std::max(80.0f, avail.y - ImGui::GetTextLineHeightWithSpacing() - 4.0f);
    const float aspect = spanX / spanZ;
    ImVec2 imageSize(maxW, maxW / std::max(aspect, 0.001f));
    if (imageSize.y > maxH) {
        imageSize.y = maxH;
        imageSize.x = maxH * aspect;
    }
    imageSize.x = std::max(80.0f, imageSize.x);
    imageSize.y = std::max(80.0f, imageSize.y);

    if (imageSize.x < maxW) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (maxW - imageSize.x) * 0.5f);
    const ImVec2 imageMin = ImGui::GetCursorScreenPos();
    const int renderW = std::clamp(static_cast<int>(std::lround(imageSize.x)), 64, 1024);
    const int renderH = std::clamp(static_cast<int>(std::lround(imageSize.y)), 64, 1024);
    const std::uint32_t tex = state.renderer.RenderTopDownOverview(renderW, renderH);
    if (tex == 0) {
        ImGui::Dummy(imageSize);
        ImGui::TextDisabled("%s",L("Minimap-Vorschau nicht verfügbar.","Minimap preview unavailable."));
        return;
    }

    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), imageSize,
                 ImVec2(0,0), ImVec2(1,1));
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(imageMin, imageMax, IM_COL32(31,82,116,230), 3.0f, 0, 1.0f);

    if (state.minimapShowObjects) {
        RefreshObjectVisibility(state);
        for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
            if (IsObjectHidden(state, i)) continue;
            const auto& obj = state.placementSet.At(i);
            const float u = std::clamp(obj.posX / spanX, 0.0f, 1.0f);
            const float v = 1.0f - std::clamp(obj.posZ / spanZ, 0.0f, 1.0f);
            const bool selected = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), static_cast<int>(i)) != state.selectedObjects.end();
            dl->AddCircleFilled(ImVec2(imageMin.x + u * imageSize.x, imageMin.y + v * imageSize.y),
                                selected ? 2.8f : 1.3f,
                                selected ? IM_COL32(255,255,255,245) : IM_COL32(65,190,230,150));
        }
    }

    const float zoom = std::clamp(state.view2dZoom, 1.0f, 40.0f);
    if (state.minimapShowViewport) {
        const float half = 0.5f / zoom;
        const float u0 = std::clamp(state.view2dCenterU - half, 0.0f, 1.0f);
        const float v0 = std::clamp(state.view2dCenterV - half, 0.0f, 1.0f);
        const float u1 = std::clamp(state.view2dCenterU + half, 0.0f, 1.0f);
        const float v1 = std::clamp(state.view2dCenterV + half, 0.0f, 1.0f);
        dl->AddRect(ImVec2(imageMin.x + u0 * imageSize.x, imageMin.y + v0 * imageSize.y),
                    ImVec2(imageMin.x + u1 * imageSize.x, imageMin.y + v1 * imageSize.y),
                    IM_COL32(32,221,242,245), 1.5f, 0, 2.0f);
    }

    if (hovered) {
        ImGui::SetTooltip("%s",L("Klick: 2D-Ansicht hier zentrieren","Click: center the 2D view here"));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const float u = std::clamp((mouse.x - imageMin.x) / imageSize.x, 0.0f, 1.0f);
            const float v = std::clamp((mouse.y - imageMin.y) / imageSize.y, 0.0f, 1.0f);
            const float half = 0.5f / zoom;
            state.view2dCenterU = std::clamp(u, half, 1.0f - half);
            state.view2dCenterV = std::clamp(v, half, 1.0f - half);
            state.statusMessage = L("2D-Ansicht über Minimap zentriert.","2D view centered from minimap.");
        }
    }

    ImGui::TextDisabled("%s",L("Editor-Preview · Fiesta-Export bis Formatverifikation gesperrt",
                               "Editor preview · Fiesta export locked until format verification"));
}

} // namespace nge
