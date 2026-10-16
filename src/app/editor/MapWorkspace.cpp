// MapWorkspace.cpp
// Map-Editor-Arbeitsbereich (Docking-Layout).

#include "EditorShared.hpp"

namespace nge {

void DrawMapEditorWorkspace(EditorState& state) {
    DrawTopNav(state, L("Karte","Map"));
    DrawWorkspaceTabBar(state);
    ImGui::Separator();

    const ImGuiID dockspaceId = ImGui::GetID("##MapEditorDockspace");
    if (state.pendingMapWorkspacePreset >= 0) {
        state.mapWorkspacePreset = std::clamp(state.pendingMapWorkspacePreset,0,3);
        state.pendingMapWorkspacePreset = -1;
        state.resetMapDockLayout = true;
        SaveWorkspaceSettings(state);
    }
    if (!state.mapEditorDockspaceBuilt || state.resetMapDockLayout) {
        const bool haveSavedLayout = ImGui::DockBuilderGetNode(dockspaceId) != nullptr;
        const bool buildDefault = state.resetMapDockLayout || !haveSavedLayout;
        state.mapEditorDockspaceBuilt = true;

        if (buildDefault) {
            // Presets verändern ausschließlich das Dock-Rezept. Danach darf der Nutzer
            // weiterhin frei ziehen; ImGui speichert diese individuellen Anpassungen in layout.ini.
            // Premium default layout: the 3D viewport is intentionally the visual hero.
            float leftRatio=0.16f, inspectorRatio=0.23f, bottomRatio=0.29f;
            float navigatorRatio=0.33f, assetRatio=0.43f;
            switch (state.mapWorkspacePreset) {
                case 1: // 3D-Fokus
                    leftRatio=0.14f; inspectorRatio=0.20f; bottomRatio=0.20f;
                    navigatorRatio=0.30f; assetRatio=0.38f;
                    break;
                case 2: // Terrain / 2D
                    leftRatio=0.16f; inspectorRatio=0.22f; bottomRatio=0.52f;
                    navigatorRatio=0.30f; assetRatio=0.28f;
                    break;
                case 3: // Daten / Szene
                    leftRatio=0.25f; inspectorRatio=0.31f; bottomRatio=0.30f;
                    navigatorRatio=0.28f; assetRatio=0.48f;
                    break;
                default:
                    break;
            }

            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetContentRegionAvail());

            ImGuiID center = dockspaceId;
            ImGuiID leftId = 0, inspectorId = 0, bottomId = 0;
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, leftRatio, &leftId, &center);
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, inspectorRatio, &inspectorId, &center);
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, bottomRatio, &bottomId, &center);

            ImGuiID navigatorId = 0, sceneId = leftId;
            ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Up, navigatorRatio, &navigatorId, &sceneId);

            ImGuiID assetId = 0, view2dId = bottomId;
            ImGui::DockBuilderSplitNode(bottomId, ImGuiDir_Left, assetRatio, &assetId, &view2dId);
            const ImGuiID view3dId = center;

            ImGui::DockBuilderDockWindow("Navigator##mapNavigator", navigatorId);
            ImGui::DockBuilderDockWindow("Layer##layerManager", sceneId);
            ImGui::DockBuilderDockWindow("Sichtbarkeit##visibilityPanel", sceneId);
            ImGui::DockBuilderDockWindow("Szene##sceneOutliner", sceneId);
            ImGui::DockBuilderDockWindow("Eigenschaften##fileToolsCol", inspectorId);
            ImGui::DockBuilderDockWindow("Asset Browser##assetBrowser", assetId);
            ImGui::DockBuilderDockWindow("Minimap##minimapPanel", assetId);
            ImGui::DockBuilderDockWindow("2D-Ansicht##view2d", view2dId);
            ImGui::DockBuilderDockWindow("Ausgabe##outputLog", view2dId);
            ImGui::DockBuilderDockWindow("3D-Ansicht##view3d", view3dId);
            ImGui::DockBuilderFinish(dockspaceId);
            state.statusMessage=std::string(L("Workspace-Preset angewendet: ","Workspace preset applied: "))+MapWorkspacePresetName(state.mapWorkspacePreset);
        }
        state.resetMapDockLayout = false;
    }

    const float statusH = 28.0f;
    const float dockH = std::max(120.0f, ImGui::GetContentRegionAvail().y - statusH);
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, dockH), ImGuiDockNodeFlags_None);

    auto modeName = [&]() -> const char* {
        switch (state.editMode) {
            case EditMode::Heightmap: return "Terrain / Heightmap";
            case EditMode::TexturePaint: return L("Textur-Layer","Texture layer");
            case EditMode::BlockWalk: return "Block & Walk";
            case EditMode::ObjectPlacement: return L("Objekte","Objects");
            case EditMode::Npcs: return "NPCs";
            case EditMode::Mobs: return L("Mobs / Spawn-Zonen","Mobs / spawn zones");
            case EditMode::Portals: return L("Portale","Portals");
            case EditMode::NpcAi: return "NPC AI";
            case EditMode::MobAi: return "Mob AI";
        }
        return L("Werkzeug","Tool");
    };

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("Navigator##mapNavigator");
    DrawPanelHeader("navigatorHeader", L("KARTE","MAP"), DrawIconGlobe, "nav.world");
    ImGui::Text("%s", state.legacySaveStem[0] ? state.legacySaveStem : L("(keine Karte)","(no map)"));
    if (state.legacySaveDir[0]) ImGui::TextDisabled("%s", state.legacySaveDir);
    ImGui::Dummy(ImVec2(0,4));
    ImGui::TextDisabled("%s",L("Größe","Size"));
    if (MapSupportsTerrainEditing(state))
        ImGui::Text("%u × %u", state.heightmap.Width(), state.heightmap.Height());
    else
        ImGui::TextDisabled("%s", L("NIF-Szene · kein HTD/HTDG","NIF scene · no HTD/HTDG"));
    ImGui::TextDisabled("%s",L("Textur-Layer","Texture layers"));
    ImGui::Text("%zu", state.textureStack.LayerCount());
    ImGui::TextDisabled("%s",L("Objekte","Objects"));
    ImGui::Text("%zu", state.placementSet.Count() + state.shmdCategoryRenderSet.Count());
    ImGui::Separator();
    DrawPanelHeader("activeToolHeader", L("AKTIVES WERKZEUG","ACTIVE TOOL"),
                    DrawIconGear, "panel.tools", modeName());
    ImGui::Dummy(ImVec2(0,6));
    if (UI::Button(L("Karte wechseln","Change map"), ImVec2(-1,0))) state.screen = AppScreen::MapEditorLauncher;
    if (UI::Button(L("Spieldaten öffnen","Open game data"), ImVec2(-1,0))) state.screen = AppScreen::ShnEditor;
    if (UI::Button("Animationen / KFM", ImVec2(-1,0))) state.screen = AppScreen::KfmBrowser;
    ImGui::Separator();
    ImGui::TextDisabled("%s",L("Workspace-Preset","Workspace preset"));
    const char* workspacePresets[]={L("Standard","Standard"),L("3D-Fokus","3D focus"),L("Terrain / 2D","Terrain / 2D"),L("Daten / Szene","Data / Scene")};
    int presetChoice=state.mapWorkspacePreset;
    ImGui::SetNextItemWidth(-1.0f);
    if (UI::Combo("##mapWorkspacePreset", &presetChoice, workspacePresets, 4) &&
        presetChoice != state.mapWorkspacePreset) {
        RequestMapWorkspacePreset(state,presetChoice);
    }
    if (UI::Button(L("Preset erneut anwenden","Reapply preset"), ImVec2(-1,0)))
        RequestMapWorkspacePreset(state,state.mapWorkspacePreset);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("Szene##sceneOutliner");
    DrawSceneOutlinerPanel(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("Layer##layerManager");
    DrawLayerManagerPanel(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("Sichtbarkeit##visibilityPanel");
    DrawPanelHeader("visibilityHeader", L("SICHTBARKEIT","VISIBILITY"),
                    DrawIconEye, "state.visibility");
    DrawVisibilityPanel(state);
    ImGui::SeparatorText(L("Ansicht","View"));
    UI::Checkbox(T("workspace.wireframe"), &state.wireframe);
    if (UI::Button(T("workspace.centercamera"), ImVec2(-1,0))) {
        if (MapSupportsTerrainEditing(state)) {
            const float spanX = static_cast<float>(state.heightmap.Width() - 1) * state.heightmap.BlockWidth();
            const float spanZ = static_cast<float>(state.heightmap.Height() - 1) * state.heightmap.BlockHeight();
            const auto [lo, hi] = state.heightmap.MinMax();
            state.camera.SetTarget(spanX * 0.5f, (lo + hi) * 0.5f, spanZ * 0.5f);
        } else if (!FocusLoadedNifSceneFromBounds(state)) {
            state.statusMessage = L("Keine renderbare NIF-Geometrie zum Zentrieren gefunden.",
                                    "No renderable NIF geometry was found to center.");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("Eigenschaften##fileToolsCol");
    DrawPanelHeader("propertiesHeader", L("EIGENSCHAFTEN","PROPERTIES"),
                    DrawIconGear, "panel.properties", modeName());
    DrawToolsContent(state);

    if (UI::CollapsingHeader(L("Datei & Fiesta Import/Export","File & Fiesta Import/Export"))) {
        if (UI::Button(T("workspace.saveas"), ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("##saveAsPopup");
        if (ImGui::BeginPopup("##saveAsPopup")) {
            UI::InputText(L("Ordner","Folder"), state.legacySaveDir, sizeof(state.legacySaveDir));
            UI::InputText(L("Name","Name"), state.legacySaveStem, sizeof(state.legacySaveStem));
            if (UI::Button(T("workspace.save"))) {
                SaveLegacyMapProject(state);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        DrawAdvancedFileOps(state);
    }
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("3D-Ansicht##view3d");
    DrawPanelHeader("view3dHeader", L("3D ANSICHT","3D VIEW"),
                    DrawIconCube, "view.3d", modeName());
    DrawViewportLayout(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("Asset Browser##assetBrowser");
    DrawWorkspaceAssetBrowser(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("Minimap##minimapPanel");
    DrawMinimapPreviewContent(state);
    ImGui::End();
    ImGui::PopStyleColor();

    // Bestehende layout.ini kennen das Ausgabeprotokoll noch nicht: beim ersten Auftauchen
    // als Tab neben die 2D-Ansicht docken statt frei schwebend zu öffnen.
    if (ImGuiWindow* view2dWindow = ImGui::FindWindowByName("2D-Ansicht##view2d");
        view2dWindow && view2dWindow->DockId != 0)
        ImGui::SetNextWindowDockID(view2dWindow->DockId, ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("Ausgabe##outputLog");
    DrawOutputLogPanel(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::Begin("2D-Ansicht##view2d");
    DrawPanelHeader("view2dHeader", L("2D DRAUFSICHT","2D TOP VIEW"),
                    DrawIconGrid, "view.2d", L("Nord oben","North up"));
    DrawEditor2DContent(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::PanelDeep);
    ImGui::PushStyleColor(ImGuiCol_Border, UiTheme::Border);
    ImGui::BeginChild("##mapStatusBar", ImVec2(0.0f, 27.0f), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextColored(state.mapDirty ? UiTheme::Warning : UiTheme::TextSecondary,
                       "%s", state.mapDirty ? "*" : "*");
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextDisabled(L("Map: %s  ·  Werkzeug: %s  ·  Auswahl: %zu  ·  %.0f FPS",
                          "Map: %s  ·  Tool: %s  ·  Selection: %zu  ·  %.0f FPS"),
                        state.legacySaveStem[0] ? state.legacySaveStem : "-",
                        modeName(), state.selectedObjects.size(), ImGui::GetIO().Framerate);
    if (const std::size_t hiddenCount = CountEditorHiddenObjects(state); hiddenCount > 0) {
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::Warning, L("  ·  %zu ausgeblendet (Strg+H)", "  ·  %zu hidden (Ctrl+H)"), hiddenCount);
    }
    if (state.playtestActive) {
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::AccentBlue, "%s", L("  ·  SPIELTEST AKTIV", "  ·  PLAYTEST ACTIVE"));
    }
    if (state.selectedObject != kNoObjectSelection) {
        if (const auto* selected = EditableObject(state, state.selectedObject)) {
            ImGui::SameLine();
            ImGui::TextDisabled("  ·  XYZ %.1f / %.1f / %.1f", selected->posX, selected->posY, selected->posZ);
        }
    }
    if (!state.statusMessage.empty() && ImGui::GetContentRegionAvail().x > 260.0f) {
        ImGui::SameLine();
        ImGui::TextColored(UiTheme::TextSecondary, "  ·  %s", state.statusMessage.c_str());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

} // namespace nge
