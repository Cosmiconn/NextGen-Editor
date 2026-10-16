// WorkspacePanels.cpp
// Mehrere Viewports, Workspace-Tabs, Szene-Outliner und Layer-Manager.

#include "EditorShared.hpp"

namespace nge {

void SwapViewportState(EditorState& state, EditorState::ViewportSlot& v) {
    ForEachViewportField(state, v, [](auto& a, auto& b) { std::swap(a, b); });
}
void SetActiveViewport(EditorState& state, int index) {
    index = std::clamp(index, 0, static_cast<int>(state.viewportSlots.size()) - 1);
    if (index == state.activeViewport) return;
    auto& oldSlot = state.viewportSlots[static_cast<std::size_t>(state.activeViewport)];
    ForEachViewportField(state, oldSlot, [](auto& live, auto& slot) { slot = live; });
    auto& newSlot = state.viewportSlots[static_cast<std::size_t>(index)];
    ForEachViewportField(state, newSlot, [](auto& live, auto& slot) { live = slot; });
    state.activeViewport = index;
    state.cameraLooking = false;
    state.marqueeActive = false;
    state.objectGizmoMatrixValid = false;
}
// Neue Viewports starten wie Unreals 4er-Layout: Perspektive, Oben, Süden, Osten - jeweils
// orthografisch auf dasselbe Ziel ausgerichtet wie die aktuelle Kamera.
void InitializeViewportSlots(EditorState& state) {
    constexpr int kPresets[4] = {0, 1, 2, 5};
    for (int k = 0; k < 4; ++k) {
        auto& v = state.viewportSlots[static_cast<std::size_t>(k)];
        if (k == state.activeViewport || v.initialized) continue;
        ForEachViewportField(state, v, [](auto& live, auto& slot) { slot = live; });
        v.cameraLooking = false;
        v.marqueeActive = false;
        v.initialized = true;
        SwapViewportState(state, v);
        ApplyViewportPreset(state, kPresets[k]);
        SwapViewportState(state, v);
    }
    state.viewportSlots[static_cast<std::size_t>(state.activeViewport)].initialized = true;
}
const char* ViewportLayoutName(int layout) {
    switch (layout) {
        case 2: return L("2 Viewports nebeneinander", "2 viewports side by side");
        case 4: return L("4 Viewports (2x2)", "4 viewports (2x2)");
        default: return L("Einzelner Viewport", "Single viewport");
    }
}
void SetViewportLayout(EditorState& state, int layout) {
    layout = layout == 2 || layout == 4 ? layout : 1;
    if (state.activeViewport >= layout) SetActiveViewport(state, 0);
    state.viewportLayout = layout;
    if (layout > 1) InitializeViewportSlots(state);
    state.viewportSettingsDirty = true;
}
void DrawViewportLayout(EditorState& state) {
    const int count = state.viewportLayout == 2 || state.viewportLayout == 4 ? state.viewportLayout : 1;
    if (count > 1) InitializeViewportSlots(state);
    if (state.activeViewport >= count) SetActiveViewport(state, 0);

    // Klick in einen anderen Viewport aktiviert ihn, BEVOR gezeichnet wird - so wirkt derselbe Klick
    // bereits als Auswahl/Kamera-Eingabe im neuen aktiven Viewport.
    if (count > 1 && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) &&
        (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
        const ImVec2 m = ImGui::GetMousePos();
        for (int k = 0; k < count; ++k) {
            const ImVec2 p = state.viewportRectPos[static_cast<std::size_t>(k)];
            const ImVec2 sz = state.viewportRectSize[static_cast<std::size_t>(k)];
            if (m.x >= p.x && m.y >= p.y && m.x < p.x + sz.x && m.y < p.y + sz.y) {
                if (k != state.activeViewport) SetActiveViewport(state, k);
                break;
            }
        }
    }

    if (count == 1) {
        state.drawingActiveViewport = true;
        state.renderer.SelectRenderTarget(0);
        DrawPreview3DContent(state);
        state.viewportRectPos[0] = state.viewport3dPos;
        state.viewportRectSize[0] = state.viewport3dSize;
        return;
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    constexpr float kGap = 3.0f;
    const int cols = 2, rows = count == 4 ? 2 : 1;
    const float cellW = std::max(40.0f, (avail.x - kGap * static_cast<float>(cols - 1)) / static_cast<float>(cols));
    const float cellH = std::max(40.0f, (avail.y - kGap * static_cast<float>(rows - 1)) / static_cast<float>(rows));
    for (int k = 0; k < count; ++k) {
        const int col = k % cols, row = k / cols;
        const ImVec2 pos(origin.x + static_cast<float>(col) * (cellW + kGap), origin.y + static_cast<float>(row) * (cellH + kGap));
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(k);
        ImGui::BeginChild("##viewportCell", ImVec2(cellW, cellH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const bool live = k == state.activeViewport;
        auto& slot = state.viewportSlots[static_cast<std::size_t>(k)];
        if (!live) SwapViewportState(state, slot);
        state.drawingActiveViewport = live;
        state.renderer.SelectRenderTarget(k);
        DrawPreview3DContent(state);
        state.viewportRectPos[static_cast<std::size_t>(k)] = state.viewport3dPos;
        state.viewportRectSize[static_cast<std::size_t>(k)] = state.viewport3dSize;
        if (!live) SwapViewportState(state, slot);
        ImGui::EndChild();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(pos, ImVec2(pos.x + cellW, pos.y + cellH),
                    live ? IM_COL32(255, 196, 64, 235) : IM_COL32(40, 60, 80, 200), 0.0f, 0, live ? 2.0f : 1.0f);
        ImGui::PopID();
    }
    state.drawingActiveViewport = true;
    state.renderer.SelectRenderTarget(0);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + avail.y));
}
// Tab-Leiste des Arbeitsbereichs (Hightmap/Texturing/Block-Walk/Objects/NPCs/NPC AI/Mobs/
// Mob AI + Zurück, siehe Mockup) - steuert denselben EditMode, der früher über Radio-Buttons
// im Werkzeuge-Panel gewählt wurde.
void DrawWorkspaceTabBar(EditorState& state) {
    auto setMode = [&](EditMode mode) {
        if (state.editMode != mode) {
            state.editMode = mode;
            state.portalPickMode = false;
            state.layerPreviewDirty = true;
            state.walkPreviewDirty = true;
        }
        if (mode == EditMode::TexturePaint) ImGui::SetWindowFocus("Layer##layerManager");
        if (mode == EditMode::ObjectPlacement || mode == EditMode::Npcs ||
            mode == EditMode::Mobs || mode == EditMode::Portals)
            ImGui::SetWindowFocus("Szene##sceneOutliner");
    };
    auto undoAvailable = [&]() { return CanUndoMapEdit(state); };
    auto redoAvailable = [&]() { return CanRedoMapEdit(state); };
    auto doUndo = [&]() { UndoMapEdit(state); };
    auto doRedo = [&]() { RedoMapEdit(state); };

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::PanelDeep);
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::Border);
    ImGui::BeginChild("##workspaceCommandBar", ImVec2(0.0f, 70.0f), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 5.0f));
    ImGui::BeginGroup();

    const bool canSave = state.hasLegacyIniMeta || state.legacySaveDir[0] != '\0';
    if (DrawIconButton("cmd.save", L("Speichern","Save"), DrawIconSave, false, ImVec2(78,58), canSave, "file.save")) {
        SaveLegacyMapProject(state);
    }
    ImGui::SameLine();
    if (DrawIconButton("cmd.undo", L("Rückgängig","Undo"), DrawIconUndo, false, ImVec2(68,58), undoAvailable(), "history.undo")) doUndo();
    ImGui::SameLine();
    if (DrawIconButton("cmd.redo", L("Wiederholen","Redo"), DrawIconRedo, false, ImVec2(68,58), redoAvailable(), "history.redo")) doRedo();

    // Transform-Gruppe: dieselben echten ImGuizmo-Modi wie im Objekt-Inspector, jetzt
    // direkt in der primären Toolbar erreichbar. Kein neuer Fake-Modus; die Buttons
    // schalten den bestehenden ObjectPlacement-Auswahlmodus und objectGizmoOperation.
    ImGui::SameLine(); ImGui::Dummy(ImVec2(8.0f, 1.0f)); ImGui::SameLine();
    auto activateTransform = [&](int operation) {
        setMode(EditMode::ObjectPlacement);
        state.objectPlaceMode = 0; // Auswahlmodus statt neues Objekt platzieren
        state.objectGizmoOperation = operation;
        state.objectGizmoMatrixValid = false;
    };
    if (DrawIconButton("cmd.select", L("Auswählen","Select"), DrawIconCube,
                       state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 0 &&
                       state.objectGizmoOperation < 0, ImVec2(72,58), true, "transform.select")) {
        activateTransform(-1);
    }
    ImGui::SameLine();
    if (DrawIconButton("cmd.move", L("Verschieben","Move"), DrawIconMove,
                       state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 0 &&
                       state.objectGizmoOperation == 0, ImVec2(72,58), true, "transform.move")) activateTransform(0);
    ImGui::SameLine();
    if (DrawIconButton("cmd.rotate", L("Rotieren","Rotate"), DrawIconRotate,
                       state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 0 &&
                       state.objectGizmoOperation == 1, ImVec2(72,58), true, "transform.rotate")) activateTransform(1);
    ImGui::SameLine();
    if (DrawIconButton("cmd.scale", L("Skalieren","Scale"), DrawIconScale,
                       state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 0 &&
                       state.objectGizmoOperation == 2, ImVec2(72,58), true, "transform.scale")) activateTransform(2);
    ImGui::SameLine();
    const bool transformSnapAvailable =
        state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 0 &&
        state.objectGizmoOperation >= 0;
    if (DrawIconButton("cmd.snap", "Snap", DrawIconSnap,
                       transformSnapAvailable && state.objectGizmoSnap,
                       ImVec2(64,58), transformSnapAvailable)) {
        state.objectGizmoSnap = !state.objectGizmoSnap;
    }

    ImGui::SameLine(); ImGui::Dummy(ImVec2(8.0f, 1.0f)); ImGui::SameLine();
    if (DrawIconButton("cmd.view2d", "2D", DrawIconGrid, false,
                       ImVec2(56,58), true, "view.2d")) {
        ImGui::SetWindowFocus("2D-Ansicht##view2d");
    }
    ImGui::SameLine();
    if (DrawIconButton("cmd.view3d", "3D", DrawIconCube, false,
                       ImVec2(56,58), true, "view.3d")) {
        ImGui::SetWindowFocus("3D-Ansicht##view3d");
    }
    ImGui::SameLine();
    const bool terrainEditingAvailable = MapSupportsTerrainEditing(state);
    if (DrawIconButton("cmd.layers", L("Layer","Layers"), DrawIconLayers, false,
                       ImVec2(64,58), terrainEditingAvailable, "world.layers")) {
        ImGui::SetWindowFocus("Layer##layerManager");
    }

    ImGui::SameLine(); ImGui::Dummy(ImVec2(8.0f, 1.0f)); ImGui::SameLine();

    struct Tool { const char* id; const char* label; EditMode mode; IconDrawFn icon; const char* semanticIcon; };
    const Tool tools[] = {
        {"terrain",L("Terrain","Terrain"),EditMode::Heightmap,DrawIconTerrain,"world.terrain"},
        {"texture",L("Textur","Texture"),EditMode::TexturePaint,DrawIconBrush,"tool.brush"},
        {"walk","Block & Walk",EditMode::BlockWalk,DrawIconGrid,"gameplay.block_walk"},
        {"objects",L("Objekte","Objects"),EditMode::ObjectPlacement,DrawIconCube,"world.objects"},
        {"npcs","NPCs",EditMode::Npcs,DrawIconPerson,"nav.npcs"},
        {"mobs","Mobs",EditMode::Mobs,DrawIconSpawn,"nav.spawns"},
        // Das Paket besitzt noch kein dediziertes Portal-Hauptsymbol; bis zur Ergänzung
        // bleibt bewusst der funktionale Legacy-Fallback statt einer falschen Zuordnung.
        {"portals",L("Portale","Portals"),EditMode::Portals,DrawIconPortal,nullptr},
    };
    for (const auto& tool : tools) {
        const bool terrainOnly =
            tool.mode == EditMode::Heightmap || tool.mode == EditMode::TexturePaint ||
            tool.mode == EditMode::BlockWalk;
        const bool enabled = !terrainOnly || terrainEditingAvailable;
        if (DrawIconButton(tool.id, tool.label, tool.icon, state.editMode == tool.mode,
                           ImVec2(74.0f,58.0f), enabled, tool.semanticIcon))
            setMode(tool.mode);
        ImGui::SameLine();
    }

    // Workspace-Wechsel (Spieldaten/Animationen/Projekt/Map-Auswahl) bleibt bewusst in
    // der globalen Topbar. Die Map-Toolbar enthält nur Aktionen für die aktuelle Karte.

    ImGui::EndGroup();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}
void DrawSceneOutlinerPanel(EditorState& state) {
    const char* context = L("Objekte","Objects");
    if (state.editMode == EditMode::Npcs) context = "NPCs";
    else if (state.editMode == EditMode::Mobs) context = L("Mob-Zonen","Mob zones");
    else if (state.editMode == EditMode::Portals) context = L("Portale","Portals");
    DrawPanelHeader("sceneOutlinerHeader", L("SZENE","SCENE"), DrawIconGrid,
                    "panel.outliner", context);

    DrawSearchInput("objectOutlinerFilter", L("Szene filtern...","Filter scene..."),
                    state.objectOutlinerFilter, sizeof(state.objectOutlinerFilter));
    const std::string needle = LowerAscii(state.objectOutlinerFilter);

    if (state.editMode == EditMode::Npcs) {
        EnsureNpcTextLoaded(state);
        if (!state.npcTextLoaded || state.legacySaveStem[0] == '\0') {
            ImGui::TextDisabled("%s",L("Keine NPC-Daten für die aktuelle Karte verfügbar.","No NPC data is available for the current map."));
            return;
        }
        auto* table = state.npcTextFile.FindTable("ShineNPC");
        if (!table) { ImGui::TextDisabled("%s",L("ShineNPC-Tabelle fehlt.","ShineNPC table is missing.")); return; }
        const auto indices = NpcRecordsForCurrentMap(state);
        ImGui::TextDisabled("%zu NPCs", indices.size());
        ImGui::SameLine();
        if (DrawTinyIconButton("npcRouteOverlayVisible", DrawIconRoute, state.showRoamRoutes,
                               state.showRoamRoutes ? L("Routen-Overlay ausblenden","Hide route overlay") : L("Routen-Overlay einblenden","Show route overlay"),
                               ImVec2(22,22),"gameplay.path")) {
            state.showRoamRoutes = !state.showRoamRoutes;
            state.roamOverlayKey.clear();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.showRoamRoutes);
        if (UI::Checkbox(L("Alle Routen##npcRouteOverlayAll","All routes##npcRouteOverlayAll"), &state.showAllRoamRoutes))
            state.roamOverlayKey.clear();
        ImGui::EndDisabled();

        if(SceneQuickFilterButton("npcAll",L("Alle","All"),state.sceneNpcQuickFilter==0)) state.sceneNpcQuickFilter=0;
        ImGui::SameLine();
        if(SceneQuickFilterButton("npcQuest","Quest",state.sceneNpcQuickFilter==1)) state.sceneNpcQuickFilter=1;
        ImGui::SameLine();
        if(SceneQuickFilterButton("npcTrade",L("Handel","Trade"),state.sceneNpcQuickFilter==2)) state.sceneNpcQuickFilter=2;
        ImGui::SameLine();
        if(SceneQuickFilterButton("npcService","Service",state.sceneNpcQuickFilter==3)) state.sceneNpcQuickFilter=3;
        ImGui::SameLine();
        if(SceneQuickFilterButton("npcGate","Gates",state.sceneNpcQuickFilter==4)) state.sceneNpcQuickFilter=4;

        struct NpcSceneEntry {
            std::size_t idx = 0;
            std::string role;
            std::string arg;
            std::string label;
            std::string group;
            int groupRank = 0;
            SceneSemanticIcon semantic;
        };
        std::vector<NpcSceneEntry> npcEntries;
        npcEntries.reserve(indices.size());
        for (std::size_t idx : indices) {
            auto& rec = table->records[idx];
            if (rec.values.size() < 8) continue;
            const std::string role = rec.values[6];
            const std::string arg = rec.values[7];
            const bool roleMatches = state.sceneNpcQuickFilter==0 ||
                                     (state.sceneNpcQuickFilter==1 && role=="QuestNpc") ||
                                     (state.sceneNpcQuickFilter==2 && (role=="Merchant" || role=="StoreManager")) ||
                                     (state.sceneNpcQuickFilter==3 && (role=="NPCMenu" || role=="Guard")) ||
                                     (state.sceneNpcQuickFilter==4 && role=="Gate");
            if(!roleMatches) continue;
            const std::string label = rec.values[0] + "  ·  " + role +
                                      ((!arg.empty() && arg != "-") ? (" / " + arg) : std::string());
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            npcEntries.push_back({idx,role,arg,label,NpcSceneGroupLabel(role,arg),
                                  NpcSceneGroupRank(role,arg),ResolveNpcSceneIcon(role,arg)});
        }
        std::stable_sort(npcEntries.begin(),npcEntries.end(),[](const NpcSceneEntry& a,const NpcSceneEntry& b) {
            if (a.groupRank != b.groupRank) return a.groupRank < b.groupRank;
            if (a.group != b.group) return LowerAscii(a.group) < LowerAscii(b.group);
            return LowerAscii(a.label) < LowerAscii(b.label);
        });

        ImGui::BeginChild("##sceneNpcList", ImVec2(0,0), true);
        for (std::size_t first = 0; first < npcEntries.size();) {
            std::size_t last = first + 1;
            while (last < npcEntries.size() && npcEntries[last].group == npcEntries[first].group) ++last;
            const std::string groupTitle = npcEntries[first].group + "  (" + std::to_string(last-first) + ")";
            ImGui::PushID(npcEntries[first].group.c_str());
            const bool open = UI::CollapsingHeader(groupTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (open) {
                for (std::size_t pos = first; pos < last; ++pos) {
                    const auto& entry = npcEntries[pos];
                    auto& rec = table->records[entry.idx];
                    ImGui::PushID(static_cast<int>(entry.idx));
                    DrawInlineIcon("role", entry.semantic.icon, entry.semantic.color, entry.semantic.tooltip.c_str());
                    ImGui::SameLine(0,4);
                    const bool selected = state.selectedNpcRecordIdx == static_cast<int>(entry.idx);
                    if (UI::Selectable((entry.label + "##npcScene").c_str(), selected)) {
                        state.selectedNpcRecordIdx = static_cast<int>(entry.idx);
                        if (!state.showAllRoamRoutes) state.roamOverlayKey.clear();
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        FocusCurrentSceneSelection(state);
                    if (ImGui::BeginPopupContextItem("##npcSceneContext")) {
                        state.selectedNpcRecordIdx = static_cast<int>(entry.idx);
                        DrawContextMenuHeader("npcContextHeader","NPC",DrawIconPerson,"nav.npcs",
                                              entry.semantic.tooltip.c_str());
                        const std::string npcFocusShortcut=ShortcutLabel(state.shortcutFocus);
                        if (UI::MenuItem(L("Im 3D-Viewport fokussieren","Focus in 3D viewport"),
                                         npcFocusShortcut.c_str())) FocusCurrentSceneSelection(state);
                        ImGui::Separator();
                        if (UI::MenuItem(L("Dialog bearbeiten","Edit dialog"))) OpenNpcDialogEditor(state,rec.values[0]);
                        if (UI::MenuItem(L("Lua / AI bearbeiten","Edit Lua / AI"))) OpenAiScriptEditor(state,rec.values[0]);
                        if (UI::MenuItem(L("Route bearbeiten","Edit route"))) OpenPatrolRouteEditor(state,rec.values[0]);
                        if (entry.role == "Merchant" && UI::MenuItem(L("Shop / Inventar bearbeiten","Edit shop / inventory"))) {
                            EnsureShopTextLoaded(state,rec.values[0]);
                            state.shopEditorOpen=true;
                        }
                        if (entry.role == "Gate") {
                            for (const auto& marker : CollectPortalMarkers(state)) {
                                if (marker.kind != kPortalKindGateLink || marker.idx != entry.idx) continue;
                                if (UI::MenuItem(L("Gate-Ziel öffnen","Open gate target"))) NavigateToPortalTarget(state,marker);
                                break;
                            }
                        }
                        ImGui::EndPopup();
                    }
                    if (selected) {
                        RefreshRoamOverlayRoutes(state);
                        const bool hasSelectedRoute = std::any_of(
                            state.roamOverlayRoutes.begin(), state.roamOverlayRoutes.end(),
                            [&](const EditorState::RoamOverlayRoute& route) { return route.name == rec.values[0]; });
                        if (hasSelectedRoute) {
                            ImGui::SameLine();
                            DrawInlineIcon("route", DrawIconRoute, IM_COL32(90,220,255,245),
                                           L("MobRoam-Route vorhanden","MobRoam route available"),
                                           ImVec2(20,20),"gameplay.path");
                        }

                        ImGui::Indent(24.0f);
                        if (DrawTinyIconButton("npcInlineDialog",DrawIconDialog,false,L("Dialog bearbeiten","Edit dialog")))
                            OpenNpcDialogEditor(state,rec.values[0]);
                        ImGui::SameLine(0,3);
                        if (DrawTinyIconButton("npcInlineAi",DrawIconCode,false,L("Lua / AI bearbeiten","Edit Lua / AI"),
                                                   ImVec2(22,22),"module.ai"))
                            OpenAiScriptEditor(state,rec.values[0]);
                        ImGui::SameLine(0,3);
                        if (DrawTinyIconButton("npcInlineRoute",DrawIconRoute,false,L("Route bearbeiten","Edit route"),
                                                      ImVec2(22,22),"gameplay.path"))
                            OpenPatrolRouteEditor(state,rec.values[0]);
                        if (entry.role == "Merchant") {
                            ImGui::SameLine(0,3);
                            if (DrawTinyIconButton("npcInlineShop",DrawIconShop,false,L("Shop / Inventar bearbeiten","Edit shop / inventory"))) {
                                EnsureShopTextLoaded(state,rec.values[0]);
                                state.shopEditorOpen=true;
                            }
                        }
                        if (entry.role == "Gate") {
                            for (const auto& marker : CollectPortalMarkers(state)) {
                                if (marker.kind != kPortalKindGateLink || marker.idx != entry.idx) continue;
                                ImGui::SameLine(0,3);
                                if (DrawTinyIconButton("npcInlineGate",DrawIconPortal,false,L("Gate-Ziel öffnen","Open gate target")))
                                    NavigateToPortalTarget(state,marker);
                                break;
                            }
                        }
                        ImGui::Unindent(24.0f);
                    }
                    ImGui::PopID();
                }
            }
            ImGui::PopID();
            first = last;
        }
        if (npcEntries.empty()) ImGui::TextDisabled("%s",L("Keine passenden NPCs.","No matching NPCs."));
        ImGui::EndChild();
        return;
    }

    if (state.editMode == EditMode::Mobs) {
        EnsureMobRegenLoaded(state);
        if (!state.mobRegenTextLoaded || state.legacySaveStem[0] == '\0') {
            ImGui::TextDisabled("%s",L("Keine MobRegen-Daten für die aktuelle Karte verfügbar.","No MobRegen data is available for the current map."));
            return;
        }
        auto* zones = state.mobRegenTextFile.FindTable("MobRegenGroup");
        auto* spawns = state.mobRegenTextFile.FindTable("MobRegen");
        if (!zones) { ImGui::TextDisabled("%s",L("MobRegenGroup-Tabelle fehlt.","MobRegenGroup table is missing.")); return; }
        std::unordered_map<std::string,int> groupCounts;
        std::unordered_map<std::string,int> totalMobCounts;
        if (spawns) {
            for (const auto& rec : spawns->records) {
                if (rec.values.empty()) continue;
                ++groupCounts[rec.values[0]];
                if (rec.values.size() >= 3)
                    totalMobCounts[rec.values[0]] += std::max(0,std::atoi(rec.values[2].c_str()));
            }
        }
        ImGui::TextDisabled(L("%zu Spawn-Zonen","%zu spawn zones"), zones->records.size());
        ImGui::SameLine();
        if (DrawTinyIconButton("mobRouteOverlayVisible", DrawIconRoute, state.showRoamRoutes,
                               state.showRoamRoutes ? L("Routen-Overlay ausblenden","Hide route overlay") : L("Routen-Overlay einblenden","Show route overlay"),
                               ImVec2(22,22),"gameplay.path")) {
            state.showRoamRoutes = !state.showRoamRoutes;
            state.roamOverlayKey.clear();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.showRoamRoutes);
        if (UI::Checkbox(L("Alle Routen##mobRouteOverlayAll","All routes##mobRouteOverlayAll"), &state.showAllRoamRoutes))
            state.roamOverlayKey.clear();
        ImGui::EndDisabled();

        if(SceneQuickFilterButton("mobAll",L("Alle","All"),state.sceneMobQuickFilter==0)) state.sceneMobQuickFilter=0;
        ImGui::SameLine();
        if(SceneQuickFilterButton("mobEmpty",L("Leer","Empty"),state.sceneMobQuickFilter==1)) state.sceneMobQuickFilter=1;
        ImGui::SameLine();
        if(SceneQuickFilterButton("mobSingle",L("1 Art","1 species"),state.sceneMobQuickFilter==2)) state.sceneMobQuickFilter=2;
        ImGui::SameLine();
        if(SceneQuickFilterButton("mobMixed",L("Gemischt","Mixed"),state.sceneMobQuickFilter==3)) state.sceneMobQuickFilter=3;

        struct MobSceneEntry {
            std::size_t idx = 0;
            int speciesCount = 0;
            int totalMobs = 0;
            int groupRank = 0;
            std::string group;
            std::string label;
            SceneSemanticIcon semantic;
        };
        std::vector<MobSceneEntry> mobEntries;
        mobEntries.reserve(zones->records.size());
        for (std::size_t i = 0; i < zones->records.size(); ++i) {
            auto& rec = zones->records[i];
            if (rec.values.empty()) continue;
            const int groups = groupCounts[rec.values[0]];
            const int totalMobs = totalMobCounts[rec.values[0]];
            const bool groupMatches = state.sceneMobQuickFilter==0 ||
                                      (state.sceneMobQuickFilter==1 && groups==0) ||
                                      (state.sceneMobQuickFilter==2 && groups==1) ||
                                      (state.sceneMobQuickFilter==3 && groups>1);
            if(!groupMatches) continue;
            const std::string label = rec.values[0] + "  ·  " + std::to_string(groups) +
                                      (groups == 1 ? L(" Art"," species") : L(" Arten"," species")) + " · " +
                                      std::to_string(totalMobs) + (totalMobs == 1 ? " Mob" : " Mobs");
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            mobEntries.push_back({i,groups,totalMobs,MobSceneGroupRank(groups),MobSceneGroupLabel(groups),
                                  label,ResolveMobZoneSceneIcon(groups)});
        }
        std::stable_sort(mobEntries.begin(),mobEntries.end(),[](const MobSceneEntry& a,const MobSceneEntry& b) {
            if (a.groupRank != b.groupRank) return a.groupRank < b.groupRank;
            return LowerAscii(a.label) < LowerAscii(b.label);
        });

        ImGui::BeginChild("##sceneMobZoneList", ImVec2(0,0), true);
        for (std::size_t first = 0; first < mobEntries.size();) {
            std::size_t last = first + 1;
            while (last < mobEntries.size() && mobEntries[last].group == mobEntries[first].group) ++last;
            const std::string groupTitle = mobEntries[first].group + "  (" + std::to_string(last-first) + ")";
            ImGui::PushID(mobEntries[first].group.c_str());
            const bool open = UI::CollapsingHeader(groupTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (open) {
                for (std::size_t pos = first; pos < last; ++pos) {
                    const auto& entry = mobEntries[pos];
                    auto& rec = zones->records[entry.idx];
                    ImGui::PushID(static_cast<int>(entry.idx));
                    DrawInlineIcon("spawn", entry.semantic.icon, entry.semantic.color, entry.semantic.tooltip.c_str());
                    ImGui::SameLine(0,4);
                    const bool selected = state.selectedMobZoneIdx == static_cast<int>(entry.idx);
                    if (UI::Selectable((entry.label + "##mobScene").c_str(), selected)) {
                        state.selectedMobZoneIdx = static_cast<int>(entry.idx);
                        if (!state.showAllRoamRoutes) state.roamOverlayKey.clear();
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        FocusCurrentSceneSelection(state);
                    if (ImGui::BeginPopupContextItem("##mobSceneContext")) {
                        state.selectedMobZoneIdx = static_cast<int>(entry.idx);
                        const std::string mobContextMeta =
                            entry.semantic.tooltip + " · " +
                            std::to_string(entry.speciesCount) + " " + L("Arten","species") + " · " +
                            std::to_string(entry.totalMobs) + " Mobs";
                        DrawContextMenuHeader("mobContextHeader",L("MOB-ZONE","MOB ZONE"),
                                              DrawIconPerson,"nav.spawns",mobContextMeta.c_str());
                        const std::string mobFocusShortcut=ShortcutLabel(state.shortcutFocus);
                        if (UI::MenuItem(L("Im 3D-Viewport fokussieren","Focus in 3D viewport"),
                                         mobFocusShortcut.c_str())) FocusCurrentSceneSelection(state);
                        ImGui::Separator();
                        if (spawns && ImGui::BeginMenu(L("Monster in dieser Zone","Monsters in this zone"))) {
                            bool any=false;
                            for (const auto& spawn : spawns->records) {
                                if (spawn.values.size() < 2 || spawn.values[0] != rec.values[0]) continue;
                                any=true;
                                const std::string mobName=spawn.values[1];
                                if (ImGui::BeginMenu(mobName.c_str())) {
                                    if (UI::MenuItem(L("Lua / AI bearbeiten","Edit Lua / AI"))) OpenAiScriptEditor(state,mobName);
                                    if (UI::MenuItem(L("MobRoam-Route bearbeiten","Edit MobRoam route"))) OpenPatrolRouteEditor(state,mobName);
                                    ImGui::EndMenu();
                                }
                            }
                            if (!any) ImGui::TextDisabled("%s",L("Keine Monster","No monsters"));
                            ImGui::EndMenu();
                        }
                        ImGui::EndPopup();
                    }
                    if (selected) {
                        RefreshRoamOverlayRoutes(state);
                        bool hasSelectedRoute = false;
                        if (spawns) {
                            std::unordered_set<std::string> zoneNames;
                            for (const auto& spawn : spawns->records) {
                                if (spawn.values.size() >= 2 && spawn.values[0] == rec.values[0] && !spawn.values[1].empty())
                                    zoneNames.insert(spawn.values[1]);
                            }
                            hasSelectedRoute = std::any_of(
                                state.roamOverlayRoutes.begin(), state.roamOverlayRoutes.end(),
                                [&](const EditorState::RoamOverlayRoute& route) { return zoneNames.count(route.name) != 0; });
                        }
                        if (hasSelectedRoute) {
                            ImGui::SameLine();
                            DrawInlineIcon("route", DrawIconRoute, IM_COL32(90,220,255,245),
                                           L("Mindestens eine MobRoam-Route vorhanden","At least one MobRoam route is available"),
                                           ImVec2(20,20),"gameplay.path");
                        }

                        if (spawns) {
                            ImGui::Indent(24.0f);
                            for (std::size_t si=0; si<spawns->records.size(); ++si) {
                                const auto& spawn=spawns->records[si];
                                if (spawn.values.size()<2 || spawn.values[0]!=rec.values[0]) continue;
                                ImGui::PushID(static_cast<int>(si));
                                DrawInlineIcon("mobEntry",DrawIconPerson,IM_COL32(180,195,215,235),L("MobRegen-Eintrag","MobRegen entry"),
                                               ImVec2(18,18),"nav.spawns");
                                ImGui::SameLine(0,3);
                                const int amount=spawn.values.size()>=3?std::max(0,std::atoi(spawn.values[2].c_str())):0;
                                ImGui::Text("%s  x%d",spawn.values[1].c_str(),amount);
                                ImGui::SameLine();
                                if (DrawTinyIconButton("mobEntryAi",DrawIconCode,false,L("Lua / AI bearbeiten","Edit Lua / AI"),
                                                       ImVec2(19,19),"module.ai"))
                                    OpenAiScriptEditor(state,spawn.values[1]);
                                ImGui::SameLine(0,2);
                                if (DrawTinyIconButton("mobEntryRoute",DrawIconRoute,false,L("MobRoam-Route bearbeiten","Edit MobRoam route"),
                                                          ImVec2(19,19),"gameplay.path"))
                                    OpenPatrolRouteEditor(state,spawn.values[1]);
                                ImGui::PopID();
                            }
                            ImGui::Unindent(24.0f);
                        }
                    }
                    ImGui::PopID();
                }
            }
            ImGui::PopID();
            first = last;
        }
        if (mobEntries.empty()) ImGui::TextDisabled("%s",L("Keine passenden Spawn-Zonen.","No matching spawn zones."));
        ImGui::EndChild();
        return;
    }

    if (state.editMode == EditMode::Portals) {
        EnsurePortalDataLoaded(state);
        const auto markers = CollectPortalMarkers(state);
        const std::size_t outboundCount = static_cast<std::size_t>(std::count_if(
            markers.begin(),markers.end(),[](const PortalMarker& m){ return m.kind==kPortalKindGateLink; }));
        ImGui::TextDisabled(L("%zu Marker · %zu ausgehend","%zu markers · %zu outbound"), markers.size(), outboundCount);

        if(SceneQuickFilterButton("portalAll",L("Alle","All"),state.scenePortalQuickFilter==0)) state.scenePortalQuickFilter=0;
        ImGui::SameLine();
        if(SceneQuickFilterButton("portalGates","Gates",state.scenePortalQuickFilter==1)) state.scenePortalQuickFilter=1;
        ImGui::SameLine();
        if(SceneQuickFilterButton("portalTown","Town",state.scenePortalQuickFilter==2)) state.scenePortalQuickFilter=2;
        ImGui::SameLine();
        if(SceneQuickFilterButton("portalRecall","Recall",state.scenePortalQuickFilter==3)) state.scenePortalQuickFilter=3;

        struct PortalSceneEntry {
            std::size_t markerIndex = 0;
            int groupRank = 0;
            std::string group;
            std::string label;
        };
        std::vector<PortalSceneEntry> portalEntries;
        portalEntries.reserve(markers.size());
        for (std::size_t i = 0; i < markers.size(); ++i) {
            const auto& m = markers[i];
            const bool town = m.kind == kPortalKindTown;
            const bool recall = m.kind == kPortalKindRecall;
            const bool typeMatches = state.scenePortalQuickFilter==0 ||
                                     (state.scenePortalQuickFilter==1 && m.kind==kPortalKindGateLink) ||
                                     (state.scenePortalQuickFilter==2 && town) ||
                                     (state.scenePortalQuickFilter==3 && recall);
            if(!typeMatches) continue;
            std::string label = town ? ("TownPortal · " + m.label)
                              : recall ? ("Recall · " + m.label)
                                       : ("Gate · " + m.label);
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            const int groupRank = m.kind == kPortalKindGateLink ? 0 : town ? 1 : 2;
            const char* group = m.kind == kPortalKindGateLink ? L("Ausgehende Gates","Outbound gates")
                              : town ? "TownPortal"
                                     : "RecallCoord";
            portalEntries.push_back({i,groupRank,group,label});
        }
        std::stable_sort(portalEntries.begin(),portalEntries.end(),[](const PortalSceneEntry& a,const PortalSceneEntry& b) {
            if (a.groupRank != b.groupRank) return a.groupRank < b.groupRank;
            return LowerAscii(a.label) < LowerAscii(b.label);
        });

        ImGui::BeginChild("##scenePortalList", ImVec2(0,0), true);
        for (std::size_t first = 0; first < portalEntries.size();) {
            std::size_t last = first + 1;
            while (last < portalEntries.size() && portalEntries[last].group == portalEntries[first].group) ++last;
            const std::string groupTitle = portalEntries[first].group + "  (" + std::to_string(last-first) + ")";
            ImGui::PushID(portalEntries[first].group.c_str());
            const bool open = UI::CollapsingHeader(groupTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (open) {
                for (std::size_t pos = first; pos < last; ++pos) {
                    const auto& entry = portalEntries[pos];
                    const auto& m = markers[entry.markerIndex];
                    const bool town = m.kind == kPortalKindTown;
                    const bool recall = m.kind == kPortalKindRecall;
                    const bool selected = m.kind == state.selectedPortalKind && static_cast<int>(m.idx) == state.selectedPortalIdx;
                    ImGui::PushID(static_cast<int>(entry.markerIndex));
                    DrawInlineIcon("portal", DrawIconPortal,
                                   town ? IM_COL32(95,195,255,245)
                                        : recall ? IM_COL32(190,125,255,245)
                                                 : IM_COL32(100,225,160,245),
                                   town ? "TownPortal" : recall ? L("RecallCoord / Schriftrolle","RecallCoord / scroll") : L("Ausgehender Gate-Link","Outbound gate link"));
                    ImGui::SameLine(0,4);
                    if (UI::Selectable((entry.label + "##portalScene").c_str(), selected)) {
                        state.selectedPortalKind = m.kind;
                        state.selectedPortalIdx = static_cast<int>(m.idx);
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        FocusCurrentSceneSelection(state);
                    if (ImGui::BeginPopupContextItem("##portalSceneContext")) {
                        state.selectedPortalKind = m.kind;
                        state.selectedPortalIdx = static_cast<int>(m.idx);
                        DrawContextMenuHeader("portalContextHeader","PORTAL",DrawIconPortal,nullptr);
                        const std::string portalFocusShortcut=ShortcutLabel(state.shortcutFocus);
                        if (UI::MenuItem(L("Im 3D-Viewport fokussieren","Focus in 3D viewport"),
                                         portalFocusShortcut.c_str())) FocusCurrentSceneSelection(state);
                        ImGui::Separator();
                        if (m.kind == kPortalKindGateLink) {
                            if (UI::MenuItem(L("Zielkarte öffnen","Open target map"))) NavigateToPortalTarget(state,m);
                        } else if (UI::MenuItem(L("Position per 2D-Klick setzen","Set position by 2D click"))) {
                            state.portalPickMode=true;
                        }
                        ImGui::EndPopup();
                    }
                    if (selected) {
                        ImGui::Indent(24.0f);
                        if (m.kind == kPortalKindGateLink) {
                            const std::string target=PortalTargetMapName(m);
                            ImGui::TextDisabled("-> %s  (%.0f, %.0f)",target.empty()?L("(kein Ziel)","(no target)"):target.c_str(),m.targetX,m.targetY);
                            ImGui::SameLine();
                            if (DrawTinyIconButton("portalInlineOpen",DrawIconPortal,false,L("Zielkarte öffnen","Open target map"),ImVec2(19,19)))
                                NavigateToPortalTarget(state,m);
                        } else {
                            ImGui::TextDisabled("Position: %.0f / %.0f",m.x,m.y);
                            ImGui::SameLine();
                            if (DrawTinyIconButton("portalInlinePick",DrawIconMove,state.portalPickMode,L("Position per 2D-Klick setzen","Set position by 2D click"),
                                                      ImVec2(19,19),"transform.move"))
                                state.portalPickMode=!state.portalPickMode;
                        }
                        ImGui::Unindent(24.0f);
                    }
                    ImGui::PopID();
                }
            }
            ImGui::PopID();
            first = last;
        }
        if (portalEntries.empty()) ImGui::TextDisabled("%s",L("Keine passenden Portal-Marker.","No matching portal markers."));
        ImGui::EndChild();
        return;
    }

    const std::size_t shmdSceneCount = state.shmdCategoryRenderSet.Count();
    const std::size_t total = state.placementSet.Count() + shmdSceneCount;
    ImGui::TextDisabled(L("%zu Objekte","%zu objects"), total);

    if (!ImGui::GetIO().WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        SelectAllNormalObjects(state);
    }

    if (UI::SmallButton(L("Alle normalen","All normal"))) SelectAllNormalObjects(state);
    ImGui::SameLine();
    if (UI::SmallButton(L("Auswahl aufheben","Clear selection"))) ClearObjectSelection(state);
    ImGui::SameLine();
    ImGui::TextDisabled(L("%zu gewählt","%zu selected"), state.selectedObjects.size());

    if (!state.selectedObjects.empty()) {
        const std::string copyTip = L("Kopieren (Strg+C)","Copy (Ctrl+C)");
        const std::string duplicateTip =
            std::string(L("Duplizieren (","Duplicate (")) +
            ShortcutLabel(state.shortcutDuplicate) + ")";
        const std::string focusTip =
            std::string(L("Auswahl fokussieren (","Focus selection (")) +
            ShortcutLabel(state.shortcutFocus) + ")";
        const std::string deleteTip =
            std::string(L("Auswahl löschen (","Delete selection (")) +
            ShortcutLabel(state.shortcutDelete) + ")";
        if (DrawTinyIconButton("outlinerCopy",DrawIconDuplicate,false,copyTip.c_str(),ImVec2(22,22),"edit.copy"))
            CopySelectedObjects(state);
        ImGui::SameLine();
        if (DrawTinyIconButton("outlinerDuplicate",DrawIconDuplicate,false,duplicateTip.c_str(),ImVec2(22,22),"edit.duplicate"))
            DuplicateSelectedObjects(state);
        ImGui::SameLine();
        if (DrawTinyIconButton("outlinerFocus",DrawIconCube,false,focusTip.c_str()))
            FocusSelectedObjects(state);
        ImGui::SameLine();
        if (DrawTinyIconButton("outlinerDelete",DrawIconDelete,false,deleteTip.c_str(),ImVec2(22,22),"edit.delete"))
            DeleteSelectedObjects(state);
        ImGui::SameLine();
        const std::string shortcutSummary =
            std::string(L("Strg+C/V · Fokus ","Ctrl+C/V · Focus ")) +
            ShortcutLabel(state.shortcutFocus) + L(" · Boden "," · Ground ") +
            ShortcutLabel(state.shortcutGround);
        ImGui::TextDisabled("%s",shortcutSummary.c_str());
    }

    SyncObjectEditorMetadata(state);

    struct OutlinerEntry {
        int id = kNoObjectSelection;
        std::string label;
        std::string modelPath;
        std::string group;
    };
    std::vector<OutlinerEntry> entries;
    entries.reserve(total);

    auto leafName=[](std::string path) {
        std::replace(path.begin(),path.end(),'\\','/');
        const auto slash=path.find_last_of('/');
        return slash==std::string::npos?path:path.substr(slash+1);
    };
    auto appendIfMatch=[&](int id,const std::string& prefix,const std::string& modelPath) {
        const std::string custom=ObjectEditorLabel(state,id);
        const std::string group=ObjectEditorGroup(state,id);
        const std::string display=custom.empty()?leafName(modelPath):custom;
        const std::string hay=LowerAscii(display+" "+modelPath+" "+group+" "+prefix);
        if(!needle.empty()&&hay.find(needle)==std::string::npos) return;
        entries.push_back({id,prefix+display,modelPath,group});
    };

    for(std::size_t i=0;i<shmdSceneCount;++i) {
        const int id=ShmdSelectionId(i);
        const std::string category=ShmdSelectionCategoryName(state,id);
        appendIfMatch(id,"["+(category.empty()?std::string("SHMD"):category)+"] ",
                      state.shmdCategoryRenderSet.At(i).modelPath);
    }
    for(std::size_t i=0;i<state.placementSet.Count();++i)
        appendIfMatch(static_cast<int>(i),"",state.placementSet.At(i).modelPath);

    std::stable_sort(entries.begin(),entries.end(),[](const OutlinerEntry& a,const OutlinerEntry& b) {
        const std::string ga=LowerAscii(a.group), gb=LowerAscii(b.group);
        if(ga!=gb) {
            if(a.group.empty()) return false;
            if(b.group.empty()) return true;
            return ga<gb;
        }
        return LowerAscii(a.label)<LowerAscii(b.label);
    });

    std::vector<int> visibleIds;
    visibleIds.reserve(entries.size());
    for(const auto& entry:entries) visibleIds.push_back(entry.id);

    struct GroupRange { std::string name; std::size_t first=0,last=0; };
    std::vector<GroupRange> groups;
    for(std::size_t i=0;i<entries.size();) {
        const std::string group=entries[i].group;
        std::size_t end=i+1;
        while(end<entries.size()&&entries[end].group==group) ++end;
        groups.push_back({group,i,end});
        i=end;
    }

    ImGui::BeginChild("##sceneObjectList",ImVec2(0,0),true);
    for(const auto& group:groups) {
        const std::string title=group.name.empty()
            ? L("Ohne Gruppe","No group")
            : group.name;
        ImGui::PushID(title.c_str());
        const bool open=UI::CollapsingHeader(
            (title+"  ("+std::to_string(group.last-group.first)+")").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen);

        if(open) {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(group.last-group.first));
            while(clipper.Step()) {
                for(int local=clipper.DisplayStart;local<clipper.DisplayEnd;++local) {
                    const std::size_t rowIndex=group.first+static_cast<std::size_t>(local);
                    auto& entry=entries[rowIndex];
                    const int id=entry.id;
                    const bool selected=std::find(state.selectedObjects.begin(),state.selectedObjects.end(),id)
                                        !=state.selectedObjects.end();
                    ImGui::PushID(id);

                    const bool hidden=IsObjectEditorHidden(state,id);
                    const bool locked=IsObjectEditorLocked(state,id);
                    if(DrawTinyIconButton("eye",DrawIconEye,!hidden,hidden?L("Einblenden","Show"):L("Ausblenden","Hide"),ImVec2(22,22),"state.visibility")) {
                        if(id>=0) {
                            state.objectEditorHidden[static_cast<std::size_t>(id)]=hidden?0:1;
                            state.objectVisKey.clear();
                        } else {
                            const std::string key=ShmdEditorObjectKey(state,id);
                            if(!key.empty()) {
                                if(hidden) state.shmdEditorHiddenKeys.erase(key);
                                else state.shmdEditorHiddenKeys.insert(key);
                            }
                        }
                    }
                    ImGui::SameLine(0,2);
                    if(DrawTinyIconButton("lock",DrawIconLock,locked,locked?L("Entsperren","Unlock"):L("Sperren","Lock"),ImVec2(22,22),locked?"state.unlock":"state.lock")) {
                        if(id>=0) state.objectEditorLocked[static_cast<std::size_t>(id)]=locked?0:1;
                        else {
                            const std::string key=ShmdEditorObjectKey(state,id);
                            if(!key.empty()) {
                                if(locked) state.shmdEditorLockedKeys.erase(key);
                                else state.shmdEditorLockedKeys.insert(key);
                            }
                        }
                        if(!locked) state.objectDragActive=false;
                    }
                    ImGui::SameLine(0,5);

                    if(id<0) {
                        const auto renderIndex=ShmdRenderIndex(id);
                        const int kind=renderIndex&&*renderIndex<state.shmdCategoryRenderKind.size()
                            ?state.shmdCategoryRenderKind[*renderIndex]:-1;
                        const char* badge=kind==0?"SKY":kind==1?"WATER":kind==2?"GROUND":"SHMD";
                        ImGui::TextColored(ImVec4(0.30f,0.78f,0.95f,1.0f),"%s",badge);
                        ImGui::SameLine(0,5);
                    }

                    ImGui::BeginDisabled(locked);
                    const bool clicked=UI::Selectable(entry.label.c_str(),selected,
                                                       ImGuiSelectableFlags_AllowDoubleClick);
                    ImGui::EndDisabled();
                    if(clicked) {
                        state.editMode=EditMode::ObjectPlacement;
                        state.objectPlaceMode=0;
                        SelectObjectFromList(state,id,static_cast<int>(rowIndex),visibleIds,
                                             ImGui::GetIO().KeyCtrl,ImGui::GetIO().KeyShift);
                        if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) FocusSelectedObjects(state);
                    }
                    if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        ImGui::SetTooltip("%s%s%s",
                                          entry.modelPath.c_str(),
                                          entry.group.empty()?"":L("\nGruppe: ","\nGroup: "),
                                          entry.group.empty()?"":entry.group.c_str());

                    if(ImGui::BeginPopupContextItem("##objectContext")) {
                        if(!selected)
                            SelectObjectFromList(state,id,static_cast<int>(rowIndex),visibleIds,false,false);
                        if(ImGui::IsWindowAppearing()) {
                            const std::string currentLabel=ObjectEditorLabel(state,id);
                            const std::string currentGroup=ObjectEditorGroup(state,id);
                            std::snprintf(state.objectMetaLabelBuffer,sizeof(state.objectMetaLabelBuffer),
                                          "%s",currentLabel.c_str());
                            std::snprintf(state.objectMetaGroupBuffer,sizeof(state.objectMetaGroupBuffer),
                                          "%s",currentGroup.c_str());
                        }

                        DrawContextMenuHeader("objectContextHeader",
                                              entry.label.empty()?L("OBJEKT","OBJECT"):entry.label.c_str(),
                                              DrawIconCube,"nav.objects",entry.modelPath.c_str());
                        ImGui::SeparatorText(L("Editor-Organisation","Editor organization"));
                        UI::InputTextWithHint("Label##objectMeta",L("frei / leer = Modellname","free / empty = model name"),
                                              state.objectMetaLabelBuffer,sizeof(state.objectMetaLabelBuffer));
                        if(UI::Button(L("Label übernehmen##objectMeta","Apply label##objectMeta"),ImVec2(-1,0))) {
                            SetObjectEditorLabel(state,id,state.objectMetaLabelBuffer);
                            state.statusMessage=L("Editor-Label aktualisiert.","Editor label updated.");
                        }
                        UI::InputTextWithHint(L("Gruppe / Ordner##objectMeta","Group / folder##objectMeta"),L("z.B. Häuser, Deko, Spawn","e.g. Houses, Decor, Spawn"),
                                              state.objectMetaGroupBuffer,sizeof(state.objectMetaGroupBuffer));
                        if(UI::Button(state.selectedObjects.size()>1
                                         ?L("Gruppe auf Auswahl anwenden##objectMeta","Apply group to selection##objectMeta")
                                         :L("Gruppe übernehmen##objectMeta","Apply group##objectMeta"),ImVec2(-1,0))) {
                            const std::string newGroup=state.objectMetaGroupBuffer;
                            if(state.selectedObjects.empty()) SetObjectEditorGroup(state,id,newGroup);
                            else for(const int selectedId:state.selectedObjects)
                                SetObjectEditorGroup(state,selectedId,newGroup);
                            state.statusMessage=newGroup.empty()
                                ?L("Editor-Gruppe entfernt.","Editor group removed.")
                                :std::string(L("Editor-Gruppe gesetzt: ","Editor group set: "))+newGroup;
                        }

                        ImGui::Separator();
                        const std::string focusShortcut=ShortcutLabel(state.shortcutFocus);
                        const std::string groundShortcut=ShortcutLabel(state.shortcutGround);
                        const std::string duplicateShortcut=ShortcutLabel(state.shortcutDuplicate);
                        if(UI::MenuItem(L("Fokussieren","Focus"),focusShortcut.c_str())) FocusSelectedObjects(state);
                        if(UI::MenuItem(L("Auf Terrain setzen","Place on terrain"),groundShortcut.c_str(),false,
                                           id<0||!IsObjectEditorLocked(state,id)))
                            GroundSelectedObjects(state);
                        if(UI::MenuItem(L("Kopieren","Copy"),L("Strg+C","Ctrl+C"))) CopySelectedObjects(state);
                        if(UI::MenuItem(L("Duplizieren","Duplicate"),duplicateShortcut.c_str())) DuplicateSelectedObjects(state);
                        {
                            const bool menuHidden=IsObjectEditorHidden(state,id);
                            const bool menuLocked=IsObjectEditorLocked(state,id);
                            if(UI::MenuItem(menuHidden?L("Einblenden","Show"):L("Ausblenden","Hide"))) {
                                if(id>=0) {
                                    state.objectEditorHidden[static_cast<std::size_t>(id)]=menuHidden?0:1;
                                    state.objectVisKey.clear();
                                } else {
                                    const std::string key=ShmdEditorObjectKey(state,id);
                                    if(menuHidden) state.shmdEditorHiddenKeys.erase(key);
                                    else if(!key.empty()) state.shmdEditorHiddenKeys.insert(key);
                                }
                            }
                            if(UI::MenuItem(menuLocked?L("Entsperren","Unlock"):L("Sperren","Lock"))) {
                                if(id>=0) state.objectEditorLocked[static_cast<std::size_t>(id)]=menuLocked?0:1;
                                else {
                                    const std::string key=ShmdEditorObjectKey(state,id);
                                    if(menuLocked) state.shmdEditorLockedKeys.erase(key);
                                    else if(!key.empty()) state.shmdEditorLockedKeys.insert(key);
                                }
                            }
                        }
                        ImGui::Separator();
                        const std::string deleteShortcut=ShortcutLabel(state.shortcutDelete);
                        if(UI::MenuItem(L("Löschen","Delete"),deleteShortcut.c_str(),false,
                                           id<0||!IsObjectEditorLocked(state,id)))
                            DeleteSelectedObjects(state);
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
            }
        }
        ImGui::PopID();
    }
    if(entries.empty()) ImGui::TextDisabled("%s",L("Keine passenden Objekte.","No matching objects."));
    ImGui::EndChild();
}
void DrawLayerManagerPanel(EditorState& state) {
    const std::string layerCount = std::to_string(state.textureStack.LayerCount()) + " / " +
                                   std::to_string(app::HeightmapRenderer::kMaxTextureLayers);
    DrawPanelHeader("layerPanelHeader", "LAYER", DrawIconLayers, "world.layers", layerCount.c_str());

    if (state.textureAssetRoot.empty()) {
        if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
            if (const auto texRoot = FindNamedSubfolder(*resmapRoot, {"fieldtexture"})) state.textureAssetRoot = *texRoot;
        }
    }

    ImGui::BeginChild("##layerManagerList", ImVec2(0, 220), true);
    for (std::size_t i = 0; i < state.textureStack.LayerCount(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const bool selected = state.selectedLayer == static_cast<int>(i);
        auto& layer = state.textureStack.Layer(i);
        if (!layer.diffuseFileName.empty() && !state.textureAssetRoot.empty()) {
            const auto thumb = GetOrLoadAssetThumbnail(state, state.textureAssetRoot / layer.diffuseFileName, false);
            if (thumb.tex) ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(thumb.tex)), ImVec2(42,42));
            else ImGui::Dummy(ImVec2(42,42));
        } else {
            ImGui::Dummy(ImVec2(42,42));
        }
        ImGui::SameLine();
        const std::string label = layer.name.empty() ? ("Layer " + std::to_string(i + 1)) : layer.name;
        if (state.layerRenameIndex == static_cast<int>(i)) {
            ImGui::SetNextItemWidth(-1.0f);
            if (state.layerRenameFocusPending) {
                ImGui::SetKeyboardFocusHere();
                state.layerRenameFocusPending=false;
            }
            const bool enter=ImGui::InputText("##layerRename",state.layerRenameBuffer,
                                              sizeof(state.layerRenameBuffer),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
            if (enter || ImGui::IsItemDeactivatedAfterEdit()) {
                if (state.layerRenameBuffer[0] != '\0' && layer.name != state.layerRenameBuffer) {
                    const auto before = CaptureTextureLayerMetadata(layer);
                    const int selectedBefore = state.selectedLayer;
                    layer.name=state.layerRenameBuffer;
                    RecordTextureLayerMetadata(state, i, before, selectedBefore);
                    state.mapDirty=true;
                }
                state.layerRenameIndex=-1;
            }
        } else if (UI::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0,42))) {
            state.selectedLayer = static_cast<int>(i);
            state.editMode = EditMode::TexturePaint;
            state.layerPreviewDirty = true;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                state.layerRenameIndex=static_cast<int>(i);
                state.layerRenameFocusPending=true;
                std::snprintf(state.layerRenameBuffer,sizeof(state.layerRenameBuffer),"%s",label.c_str());
            }
        }

        if (ImGui::BeginDragDropSource()) {
            const int sourceIndex=static_cast<int>(i);
            ImGui::SetDragDropPayload("NEXTGEN_LAYER_INDEX",&sourceIndex,sizeof(sourceIndex));
            ImGui::Text("%s",L("Layer verschieben","Move layer"));
            ImGui::TextDisabled("%s",label.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_DDS_ASSET")) {
                const char* rel=static_cast<const char*>(payload->Data);
                const auto before = CaptureTextureLayerMetadata(layer);
                const int selectedBefore = state.selectedLayer;
                layer.diffuseFileName=rel;
                RecordTextureLayerMetadata(state, i, before, selectedBefore);
                state.renderer.LoadTerrainTextures(state.textureStack,
                    !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
                state.layerPreviewDirty=true;
                state.mapDirty=true;
                state.statusMessage=L("Layer-Textur ersetzt: ","Layer texture replaced: ")+std::string(rel);
            }
            if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_LAYER_INDEX")) {
                const int from=*static_cast<const int*>(payload->Data);
                const int to=static_cast<int>(i);
                if(from>=0 && from<static_cast<int>(state.textureStack.LayerCount()) && from!=to) {
                    const int selectedBefore = state.selectedLayer;
                    state.textureStack.MoveLayer(static_cast<std::size_t>(from),static_cast<std::size_t>(to));
                    if (state.selectedLayer==from) state.selectedLayer=to;
                    else if (from<state.selectedLayer && state.selectedLayer<=to) --state.selectedLayer;
                    else if (to<=state.selectedLayer && state.selectedLayer<from) ++state.selectedLayer;
                    RecordTextureLayerMove(state, static_cast<std::size_t>(from),
                                           static_cast<std::size_t>(to), selectedBefore);
                    state.layerHidden.assign(state.textureStack.LayerCount(),0);
                    state.renderer.LoadTerrainTextures(state.textureStack,
                        !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
                    state.layerPreviewDirty=true;
                    state.mapDirty=true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("##layerContext")) {
            state.selectedLayer=static_cast<int>(i);
            state.editMode=EditMode::TexturePaint;
            state.layerPreviewDirty=true;
            DrawContextMenuHeader("layerContextHeader","LAYER",DrawIconLayers,"nav.layers",label.c_str());
            if (UI::MenuItem(L("Umbenennen","Rename"))) {
                state.layerRenameIndex=static_cast<int>(i);
                state.layerRenameFocusPending=true;
                std::snprintf(state.layerRenameBuffer,sizeof(state.layerRenameBuffer),"%s",label.c_str());
            }
            {
                const bool locked = state.lockedTextureLayers.contains(layer.name);
                if (UI::MenuItem(locked ? L("Entsperren","Unlock") : L("Sperren","Lock"))) {
                    if (locked) state.lockedTextureLayers.erase(layer.name); else state.lockedTextureLayers.insert(layer.name);
                }
            }
            if (UI::MenuItem(L("Duplizieren","Duplicate"))) {
                const int selectedBefore = state.selectedLayer;
                const auto copy=layer;
                const std::size_t ni=state.textureStack.AddLayer(copy.name+" Kopie",copy.diffuseFileName,copy.uvScaleDiffuse);
                state.textureStack.Layer(ni)=copy;
                state.textureStack.Layer(ni).name=copy.name+" Kopie";
                state.selectedLayer=static_cast<int>(ni);
                RecordTextureLayerAdded(state, ni, selectedBefore);
                state.layerHidden.resize(state.textureStack.LayerCount(),0);
                state.renderer.LoadTerrainTextures(state.textureStack,
                    !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
                state.layerPreviewDirty=true;
                state.mapDirty=true;
            }
            ImGui::Separator();
            if (UI::MenuItem(L("Entfernen","Remove"),nullptr,false,state.textureStack.LayerCount()>1)) {
                const int selectedBefore = state.selectedLayer;
                auto removedLayer = state.textureStack.TakeLayer(i);
                state.selectedLayer=state.textureStack.LayerCount()==0?-1:
                    std::min(state.selectedLayer,static_cast<int>(state.textureStack.LayerCount())-1);
                if (removedLayer) RecordTextureLayerRemoved(state, i, std::move(*removedLayer), selectedBefore);
                state.layerHidden.resize(state.textureStack.LayerCount(),0);
                state.renderer.LoadTerrainTextures(state.textureStack,
                    !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
                state.layerPreviewDirty=true;
                state.mapDirty=true;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::InvisibleButton("##ddsNewLayerDrop",ImVec2(-1,34));
    const ImVec2 dropMin=ImGui::GetItemRectMin(), dropMax=ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRect(dropMin,dropMax,IM_COL32(45,110,155,180),4.0f,0,1.2f);
    const char* dropText=L("DDS hier ablegen = neuer Layer","Drop DDS here = new layer");
    const ImVec2 ts=ImGui::CalcTextSize(dropText);
    ImGui::GetWindowDrawList()->AddText(ImVec2((dropMin.x+dropMax.x-ts.x)*0.5f,(dropMin.y+dropMax.y-ts.y)*0.5f),
                                        IM_COL32(150,200,230,230),dropText);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_DDS_ASSET")) {
            const char* rel=static_cast<const char*>(payload->Data);
            std::filesystem::path rp(rel);
            const int selectedBefore = state.selectedLayer;
            const std::size_t ni=state.textureStack.AddLayer(rp.stem().string(),rel,1.0f);
            state.selectedLayer=static_cast<int>(ni);
            RecordTextureLayerAdded(state, ni, selectedBefore);
            state.layerHidden.resize(state.textureStack.LayerCount(),0);
            state.renderer.LoadTerrainTextures(state.textureStack,
                !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
            state.layerPreviewDirty=true;
            state.mapDirty=true;
            state.statusMessage=L("Neuer Layer aus DDS: ","New layer from DDS: ")+std::string(rel);
        }
        ImGui::EndDragDropTarget();
    }

    if (state.selectedLayer >= 0 && static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount()) {
        const auto& layer = state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer));
        ImGui::TextDisabled("%s",L("Ausgewählt","Selected"));
        ImGui::TextWrapped("%s", layer.name.c_str());
        ImGui::TextDisabled("Diffuse: %s", layer.diffuseFileName.c_str());
        ImGui::TextDisabled("UV Scale: %.3f", layer.uvScaleDiffuse);
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s",L("Neuer Layer","New layer"));
    UI::InputText("Name##layerDock", state.newLayerName, sizeof(state.newLayerName));
    UI::InputText("Diffuse##layerDock", state.newLayerDiffuse, sizeof(state.newLayerDiffuse));
    ImGui::TextDisabled("%s",L("Textur kann auch unten im Asset Browser gewählt werden.","A texture can also be selected in the Asset Browser below."));
    UI::InputFloat("UV-Scale##layerDock", &state.newLayerUvScale);

    ImGui::BeginDisabled(state.textureStack.LayerCount() >= static_cast<std::size_t>(app::HeightmapRenderer::kMaxTextureLayers));
    if (UI::Button(L("Layer hinzufügen##layerDock","Add layer##layerDock"), ImVec2(-1,0))) {
        const int selectedBefore = state.selectedLayer;
        const bool createdStack = state.textureStack.Width() == 0;
        core::TextureLayerStack emptyBefore;
        if (createdStack) {
            emptyBefore = std::move(state.textureStack);
            state.textureStack = core::TextureLayerStack(1024u, 1024u);
        }
        const auto newIndex = state.textureStack.AddLayer(state.newLayerName, state.newLayerDiffuse, state.newLayerUvScale);
        state.selectedLayer = static_cast<int>(newIndex);
        if (createdStack) RecordTextureStackReplacement(state, std::move(emptyBefore), selectedBefore);
        else RecordTextureLayerAdded(state, newIndex, selectedBefore);
        state.editMode = EditMode::TexturePaint;
        state.layerPreviewDirty = true;
        state.mapDirty = true;
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(state.selectedLayer < 0);
    if (UI::Button(L("Layer duplizieren##layerDock","Duplicate layer##layerDock"), ImVec2(-1,0))) {
        const int selectedBefore = state.selectedLayer;
        const auto copy=state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer));
        const std::size_t ni=state.textureStack.AddLayer(copy.name+" Kopie",copy.diffuseFileName,copy.uvScaleDiffuse);
        state.textureStack.Layer(ni)=copy;
        state.textureStack.Layer(ni).name=copy.name+" Kopie";
        state.selectedLayer=static_cast<int>(ni);
        RecordTextureLayerAdded(state, ni, selectedBefore);
        state.layerHidden.resize(state.textureStack.LayerCount(),0);
        state.renderer.LoadTerrainTextures(state.textureStack,
            !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
        state.layerPreviewDirty=true;
        state.mapDirty=true;
    }
    if (UI::Button(L("Ausgewählten Layer entfernen##layerDock","Remove selected layer##layerDock"), ImVec2(-1,0))) {
        const int selectedBefore = state.selectedLayer;
        const auto removeIndex = static_cast<std::size_t>(state.selectedLayer);
        auto removedLayer = state.textureStack.TakeLayer(removeIndex);
        if (state.textureStack.LayerCount() == 0) state.selectedLayer = -1;
        else state.selectedLayer = std::min(state.selectedLayer, static_cast<int>(state.textureStack.LayerCount()) - 1);
        if (removedLayer) RecordTextureLayerRemoved(state, removeIndex, std::move(*removedLayer), selectedBefore);
        state.layerPreviewDirty = true;
        state.mapDirty = true;
    }
    ImGui::EndDisabled();
}

} // namespace nge
