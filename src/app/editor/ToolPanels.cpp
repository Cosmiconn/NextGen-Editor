// ToolPanels.cpp
// Einstellungen, Befehlspalette und Werkzeug-Panel.

#include "EditorShared.hpp"

namespace nge {

void DrawSettingsWindow(EditorState& state) {
    if (!state.settingsOpen) return;

    ImGui::SetNextWindowSize(ImVec2(760.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(L("Einstellungen##nextgenSettings","Settings##nextgenSettings"), &state.settingsOpen)) {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.30f,0.78f,1.0f,1.0f), "SHORTCUTS");
    ImGui::SameLine();
    ImGui::TextDisabled(L("werden unter %s gespeichert","saved under %s"),
                        (NextGenUserSettingsDir() / "shortcuts.txt").string().c_str());
    ImGui::Separator();

    struct ShortcutRow {
        const char* label;
        EditorState::ShortcutBinding* binding;
    };
    const std::array<ShortcutRow,9> rows = {{
        {L("Befehlspalette","Command palette"), &state.shortcutPalette},
        {L("Karte speichern","Save map"), &state.shortcutSave},
        {L("Gizmo: Move","Gizmo: Move"), &state.shortcutGizmoMove},
        {L("Gizmo: Rotate","Gizmo: Rotate"), &state.shortcutGizmoRotate},
        {L("Gizmo: Scale","Gizmo: Scale"), &state.shortcutGizmoScale},
        {L("Auswahl fokussieren","Focus selection"), &state.shortcutFocus},
        {L("Auf Terrain setzen","Drop to terrain"), &state.shortcutGround},
        {L("Objekte duplizieren","Duplicate objects"), &state.shortcutDuplicate},
        {L("Objekte löschen","Delete objects"), &state.shortcutDelete},
    }};

    bool changed = false;
    if (ImGui::BeginTable("##shortcutSettings", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(L("Aktion","Action"), ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn(L("Strg","Ctrl"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Shift", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Alt", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn(L("Taste","Key"), ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn(L("Status","Status"), ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (std::size_t i=0;i<rows.size();++i) {
            auto& row = rows[i];
            auto& binding = *row.binding;
            bool conflict = false;
            for (std::size_t j=0;j<rows.size();++j) {
                if (i != j && SameShortcut(binding,*rows[j].binding)) { conflict=true; break; }
            }

            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(row.label);
            ImGui::TableNextColumn(); changed |= ImGui::Checkbox("##ctrl",&binding.ctrl);
            ImGui::TableNextColumn(); changed |= ImGui::Checkbox("##shift",&binding.shift);
            ImGui::TableNextColumn(); changed |= ImGui::Checkbox("##alt",&binding.alt);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##key", ShortcutKeyName(binding.key))) {
                for (const auto& option : ShortcutKeyOptions()) {
                    const bool selected = binding.key == option.key;
                    if (ImGui::Selectable(ShortcutKeyName(option.key),selected)) {
                        binding.key = option.key;
                        changed = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            if (conflict)
                ImGui::TextColored(ImVec4(1.0f,0.45f,0.35f,1.0f),"%s",L("Konflikt","Conflict"));
            else
                ImGui::TextColored(ImVec4(0.45f,0.85f,0.60f,1.0f),"OK");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (changed) SaveShortcutSettings(state);

    ImGui::Separator();
    if (UI::Button(L("Shortcuts zurücksetzen","Reset shortcuts"))) {
        ResetShortcutSettings(state);
        SaveShortcutSettings(state);
        state.statusMessage = L("Shortcuts auf Standard zurückgesetzt.","Shortcuts reset to defaults.");
    }
    ImGui::SameLine();
    if (UI::Button(L("Map-Workspace zurücksetzen","Reset map workspace"))) {
        RequestMapWorkspacePreset(state,0);
    }

    ImGui::SeparatorText("Workspace");
    ImGui::TextWrapped("%s",L("Dock-Größen und Positionen werden automatisch zwischen Sitzungen gespeichert.","Dock sizes and positions are saved automatically between sessions."));
    ImGui::TextDisabled("%s", (NextGenUserSettingsDir() / "layout.ini").string().c_str());
    ImGui::TextWrapped("%s",L("Zusätzlich kann ein Ausgangs-Preset gewählt werden. Nach dem Anwenden bleibt das Layout frei dockbar.","You can also choose a starting preset. After applying it, the layout remains freely dockable."));
    const char* presetNames[]={L("Standard","Standard"),L("3D-Fokus","3D focus"),L("Terrain / 2D","Terrain / 2D"),L("Daten / Szene","Data / scene")};
    int settingsPreset=state.mapWorkspacePreset;
    ImGui::SetNextItemWidth(220.0f);
    if (UI::Combo(L("Map-Workspace-Preset","Map workspace preset"), &settingsPreset, presetNames, 4) &&
        settingsPreset != state.mapWorkspacePreset) {
        RequestMapWorkspacePreset(state,settingsPreset);
    }
    ImGui::SameLine();
    if (UI::Button(L("Anwenden","Apply")))
        RequestMapWorkspacePreset(state,settingsPreset);
    ImGui::TextDisabled(L("Auswahl gespeichert in %s","Selection saved in %s"),
                        (NextGenUserSettingsDir() / "workspace.txt").string().c_str());
    ImGui::TextWrapped("%s",L("Gizmo-Shortcuts verwenden standardmäßig 1 / 2 / 3, damit sie nicht mit der WASD-Kamera kollidieren.","Gizmo shortcuts default to 1 / 2 / 3 so they do not conflict with the WASD camera."));

    ImGui::SeparatorText(L("Viewport-Navigation","Viewport navigation"));
    if (UI::Checkbox(L("Unreal-Navigation","Unreal navigation"), &state.unrealNavigation)) SaveViewportSettings(state);
    ImGui::TextWrapped("%s", state.unrealNavigation
        ? L("Aktiv: RMB halten + WASD/Q/E fliegen (Mausrad = Tempo 1..8), LMB-Ziehen vor/zurück + drehen, Alt+LMB Orbit, "
            "Alt+RMB Dolly, MMB schieben. W/E/R bzw. Leertaste wählen das Gizmo, 0..9 springen zu Kamera-Lesezeichen "
            "(Strg+0..9 setzt sie), G Spielansicht, H/Shift+H/Strg+H Ausblenden/Isolieren/Alle zeigen, "
            "Strg+Alt+LMB Rahmenauswahl, Alt+Ziehen am Gizmo dupliziert, Alt+P Spieltest.",
            "Active: hold RMB + WASD/Q/E to fly (mouse wheel = speed 1..8), LMB drag forward/back + turn, Alt+LMB orbit, "
            "Alt+RMB dolly, MMB pan. W/E/R or Space select the gizmo, 0..9 jump to camera bookmarks "
            "(Ctrl+0..9 sets them), G game view, H/Shift+H/Ctrl+H hide/isolate/show all, "
            "Ctrl+Alt+LMB marquee, Alt+drag on the gizmo duplicates, Alt+P playtest.")
        : L("Klassisch: WASD bewegt die Kamera, sobald der 3D-View gehovert ist; LMB-Ziehen kreist um das Ziel.",
            "Classic: WASD moves the camera whenever the 3D view is hovered; LMB drag orbits the target."));
    ImGui::TextDisabled("%s", (NextGenUserSettingsDir() / "viewport.txt").string().c_str());

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.30f,0.78f,1.0f,1.0f), "%s", L("AUTOMATISCHES SPEICHERN", "AUTOSAVE"));
    ImGui::Separator();
    bool autosaveChanged = UI::Checkbox(L("Offene Karte automatisch sichern", "Autosave the open map"), &state.autosaveEnabled);
    ImGui::SetNextItemWidth(220.0f);
    autosaveChanged |= ImGui::SliderInt(L("Intervall (Minuten)##autosave", "Interval (minutes)##autosave"), &state.autosaveMinutes, 1, 60);
    if (autosaveChanged) SaveWorkspaceSettings(state);
    ImGui::TextWrapped("%s", L("Gesichert wird nur bei Änderungen, nach <Projekt>/Autosave (nicht in die Projektausgabe). "
                               "Nach einem Absturz bietet der Editor beim nächsten Öffnen der Karte die Wiederherstellung an; "
                               "Speichern ins Projekt löscht die Sicherung.",
                               "Saved only when something changed, to <Project>/Autosave (not into the project output). "
                               "After a crash the editor offers to restore it the next time the map is opened; "
                               "saving to the project deletes the autosave."));
    if (!state.autosaveStatus.empty()) ImGui::TextDisabled("%s", state.autosaveStatus.c_str());
    if (UI::Button(L("Jetzt sichern##autosaveNow", "Autosave now##autosaveNow")) && !StartMapAutosave(state))
        state.autosaveStatus = L("Keine Karte mit Projektordner offen.", "No map with a project folder open.");

    ImGui::End();
}
void DrawCommandPalette(EditorState& state) {
    struct Command {
        std::string label;
        std::string hint;
        std::function<void()> action;
    };
    std::vector<Command> commands;
    commands.reserve(32);

    auto add = [&](std::string label, std::string hint, std::function<void()> action) {
        commands.push_back({std::move(label), std::move(hint), std::move(action)});
    };

    add(L("Projekt: Übersicht","Project: Overview"), "", [&] { state.screen = AppScreen::ProjectHub; });
    add(L("Karte: Kartenübersicht öffnen","Map: Open map browser"), "", [&] {
        state.mapLauncherView = EditorState::MapLauncherView::Browse;
        state.screen = AppScreen::MapEditorLauncher;
    });
    add(L("Karte: Neue Karte","Map: New map"), "", [&] {
        state.mapLauncherView = EditorState::MapLauncherView::NewMap;
        state.screen = AppScreen::MapEditorLauncher;
    });
    add(L("Workspace: Standard","Workspace: Standard"), "", [&] { RequestMapWorkspacePreset(state,0); state.screen = AppScreen::MapEditorWorkspace; });
    add(L("Workspace: 3D-Fokus","Workspace: 3D focus"), "", [&] { RequestMapWorkspacePreset(state,1); state.screen = AppScreen::MapEditorWorkspace; });
    add(L("Workspace: Terrain / 2D","Workspace: Terrain / 2D"), "", [&] { RequestMapWorkspacePreset(state,2); state.screen = AppScreen::MapEditorWorkspace; });
    add(L("Workspace: Daten / Szene","Workspace: Data / scene"), "", [&] { RequestMapWorkspacePreset(state,3); state.screen = AppScreen::MapEditorWorkspace; });
    add(L("Spieldaten: Single SHN","Game data: Single SHN"), "", [&] { state.shnSubTab = 0; state.screen = AppScreen::ShnEditor; });
    add(L("Spieldaten: Multi SHN","Game data: Multi SHN"), "", [&] { state.shnSubTab = 1; state.screen = AppScreen::ShnEditor; });
    add(L("Spieldaten: Quest Editor","Game data: Quest editor"), "", [&] { state.shnSubTab = 4; state.screen = AppScreen::ShnEditor; });
    add(L("Spieldaten: Portal Editor","Game data: Portal editor"), "", [&] { state.shnSubTab = 5; state.screen = AppScreen::ShnEditor; });
    add(L("Spieldaten: Custom NPC / Mob","Game data: Custom NPC / Mob"), "", [&] { state.shnSubTab = 6; state.screen = AppScreen::ShnEditor; });
    add(L("Spieldaten: Skill Editor","Game data: Skill editor"), "", [&] { state.shnSubTab = 7; state.screen = AppScreen::ShnEditor; });
    add(L("Spieldaten: AI Workspace","Game data: AI workspace"), "", [&] { state.shnSubTab = 8; state.screen = AppScreen::ShnEditor; });
    add(L("Spieldaten: Interface Browser","Game data: Interface browser"), "", [&] { state.shnSubTab = 9; state.screen = AppScreen::ShnEditor; });
    add(L("Spieldaten: Drop Table Browser","Game data: Drop table browser"), "", [&] { state.shnSubTab = 10; state.screen = AppScreen::ShnEditor; });
    if (state.aiScriptDirty && !state.aiScriptEditorPath.empty())
        add(L("AI: Aktuelles Skript speichern","AI: Save current script"), ShortcutLabel(state.shortcutSave), [&] { SaveAiScript(state); });
    if (state.dropTableDirty)
        add(L("Drops: ItemDropTable speichern","Drops: Save ItemDropTable"), ShortcutLabel(state.shortcutSave), [&] { SaveDropTable(state); });
    add(L("Animationen: KFM","Animations: KFM"), "", [&] { state.screen = AppScreen::KfmBrowser; });
    add(L("Hilfe: Handbuch","Help: Manual"), "F1", [&] { state.manualOpen = true; });
    add(L("Einstellungen: Shortcuts & Workspace","Settings: Shortcuts & workspace"), "", [&] { state.settingsOpen = true; });

    if (state.hasLegacyIniMeta || state.legacySaveStem[0] != '\0') {
        if (MapSupportsTerrainEditing(state)) {
            add(L("Karte: Terrain-Werkzeug","Map: Terrain tool"), "", [&] {
                state.editMode = EditMode::Heightmap; state.screen = AppScreen::MapEditorWorkspace;
            });
            add(L("Karte: Textur-Werkzeug","Map: Texture tool"), "", [&] {
                state.editMode = EditMode::TexturePaint; state.screen = AppScreen::MapEditorWorkspace;
            });
            add(L("Karte: Block & Walk","Map: Block & Walk"), "", [&] {
                state.editMode = EditMode::BlockWalk; state.screen = AppScreen::MapEditorWorkspace;
            });
        }
        add(L("Karte: Objekte","Map: Objects"), "", [&] {
            state.editMode = EditMode::ObjectPlacement; state.objectPlaceMode = 0;
            state.screen = AppScreen::MapEditorWorkspace;
        });
        add(L("Karte: NPCs","Map: NPCs"), "", [&] { state.editMode = EditMode::Npcs; state.screen = AppScreen::MapEditorWorkspace; });
        add(L("Karte: Mobs","Map: Mobs"), "", [&] { state.editMode = EditMode::Mobs; state.screen = AppScreen::MapEditorWorkspace; });
        add(L("Karte: Portale","Map: Portals"), "", [&] { state.editMode = EditMode::Portals; state.screen = AppScreen::MapEditorWorkspace; });

        if (state.legacySaveDir[0] != '\0' && state.legacySaveStem[0] != '\0') {
            add(L("Karte: Speichern","Map: Save"), ShortcutLabel(state.shortcutSave), [&] {
                SaveLegacyMapProject(state);
            });
        }
    }

    if (!state.selectedObjects.empty()) {
        add(L("Szene: Auswahl fokussieren","Scene: Focus selection"), ShortcutLabel(state.shortcutFocus), [&] { FocusCurrentSceneSelection(state); });
        add(L("Objekte: Auf Terrain setzen","Objects: Drop to terrain"), ShortcutLabel(state.shortcutGround), [&] { GroundSelectedObjects(state); });
        add(L("Objekte: Duplizieren","Objects: Duplicate"), ShortcutLabel(state.shortcutDuplicate), [&] { DuplicateSelectedObjects(state); });
        add(L("Objekte: Kopieren","Objects: Copy"), L("Strg+C","Ctrl+C"), [&] { CopySelectedObjects(state); });
        add(L("Objekte: Löschen","Objects: Delete"), ShortcutLabel(state.shortcutDelete), [&] { DeleteSelectedObjects(state); });
    }

    if (DirtyShnDocumentCount(state) > 0) {
        add(L("Spieldaten: Alle geänderten SHN speichern","Game data: Save all changed SHN"), "", [&] {
            const auto [saved,failed] = SaveAllDirtyShnDocuments(state);
            state.statusMessage = "SHN gespeichert: " + std::to_string(saved) +
                (failed ? ", Fehler: " + std::to_string(failed) : std::string{});
        });
    }
    if (state.questDirty && state.questDataLoaded && !state.shnServerRoot.empty()) {
        add(L("Quest: QuestData speichern","Quest: Save QuestData"), ShortcutLabel(state.shortcutSave), [&] {
            SaveQuestDataProject(state);
        });
    }

    if (state.commandPaletteOpen) {
        ImGui::OpenPopup("##commandPalette");
        state.commandPaletteOpen = false;
        state.commandPaletteSelection = 0;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(640.0f, 430.0f), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("##commandPalette", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings)) return;

    DrawInlineIcon("commandPaletteHeader", nullptr, IM_COL32(100,205,255,245), nullptr,
                   ImVec2(20.0f,20.0f), "system.command_palette");
    ImGui::SameLine(0.0f,6.0f);
    ImGui::TextColored(ImVec4(0.30f,0.78f,1.0f,1.0f), "%s", L("BEFEHLSPALETTE","COMMAND PALETTE"));
    ImGui::SameLine();
    const std::string paletteCloseHint =
        ShortcutLabel(state.shortcutPalette) + L(" · Esc schließen"," · Esc to close");
    ImGui::TextDisabled("%s", paletteCloseHint.c_str());
    ImGui::Separator();

    const bool focusCommandSearch = ImGui::IsWindowAppearing();
    if (focusCommandSearch) state.commandPaletteQuery[0] = '\0';
    if (DrawSearchInput("commandPaletteQuery",
                        L("Befehl oder Werkzeug suchen...","Search command or tool..."),
                        state.commandPaletteQuery,sizeof(state.commandPaletteQuery),
                        -1.0f,focusCommandSearch))
        state.commandPaletteSelection = 0;

    std::string needle = state.commandPaletteQuery;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    std::vector<int> matches;
    for (int i = 0; i < static_cast<int>(commands.size()); ++i) {
        std::string hay = commands[static_cast<std::size_t>(i)].label + " " +
                          commands[static_cast<std::size_t>(i)].hint;
        std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (needle.empty() || hay.find(needle) != std::string::npos) matches.push_back(i);
    }

    if (!matches.empty()) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
            state.commandPaletteSelection = (state.commandPaletteSelection + 1) % static_cast<int>(matches.size());
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
            state.commandPaletteSelection = (state.commandPaletteSelection + static_cast<int>(matches.size()) - 1) %
                                            static_cast<int>(matches.size());
        state.commandPaletteSelection = std::clamp(state.commandPaletteSelection, 0,
                                                   static_cast<int>(matches.size()) - 1);
    } else {
        state.commandPaletteSelection = 0;
    }

    bool executeSelected = !matches.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    ImGui::BeginChild("##commandList", ImVec2(0,0), true);
    for (int visibleIndex = 0; visibleIndex < static_cast<int>(matches.size()); ++visibleIndex) {
        auto& command = commands[static_cast<std::size_t>(matches[visibleIndex])];
        const bool selected = visibleIndex == state.commandPaletteSelection;
        std::string label = command.label;
        if (!command.hint.empty()) label += "    [" + command.hint + "]";
        if (ImGui::Selectable(label.c_str(), selected) || (selected && executeSelected)) {
            auto action = command.action;
            ImGui::CloseCurrentPopup();
            action();
            break;
        }
        if (selected) ImGui::SetItemDefaultFocus();
    }
    if (matches.empty()) ImGui::TextDisabled("%s", L("Keine passenden Befehle.","No matching commands."));
    ImGui::EndChild();
    ImGui::EndPopup();
}
void DrawToolsContent(EditorState& state) {
    if (state.editMode == EditMode::Heightmap) {
        ImGui::TextDisabled("%s",L("PINSELMODUS","BRUSH MODE"));
        {
            struct ModeButton { const char* id; const char* label; core::BrushMode mode; };
            const ModeButton modes[] = {
                {"terrainRaise",L("Anheben","Raise"),core::BrushMode::Raise},
                {"terrainLower",L("Absenken","Lower"),core::BrushMode::Lower},
                {"terrainSmooth",L("Glätten","Smooth"),core::BrushMode::Smooth},
                {"terrainFlatten",L("Einebnen","Flatten"),core::BrushMode::Flatten},
                {"terrainNoise",L("Rauschen","Noise"),core::BrushMode::Noise},
                {"terrainTerrace",L("Terrassen","Terrace"),core::BrushMode::Terrace},
                {"terrainErode",L("Erosion","Erode"),core::BrushMode::Erode},
                {"terrainSharpen",L("Schärfen","Sharpen"),core::BrushMode::Sharpen},
                {"terrainRamp",L("Rampe","Ramp"),core::BrushMode::Ramp},
            };
            for (std::size_t i = 0; i < std::size(modes); ++i) {
                if (i % 3 != 0) ImGui::SameLine();
                if (SceneQuickFilterButton(modes[i].id, modes[i].label, state.brushMode == modes[i].mode)) {
                    state.brushMode = modes[i].mode;
                    state.rampStart.reset();
                }
            }
        }

        ImGui::SeparatorText(L("Pinsel","Brush"));
        UI::SliderFloat("Radius", &state.brush.radius, 10.0f, 2000.0f);
        ImGui::TextDisabled("%s",L("Radius-Presets","Radius presets"));
        for (float preset : {50.0f,100.0f,250.0f,500.0f}) {
            ImGui::SameLine();
            const std::string label=std::to_string(static_cast<int>(preset))+"##terrainRadius";
            if (UI::SmallButton(label.c_str())) state.brush.radius=preset;
        }
        UI::SliderFloat(L("Stärke","Strength"), &state.brush.strength, 0.1f, 100.0f);
        ImGui::TextDisabled("%s",L("Stärke-Presets","Strength presets"));
        for (float preset : {1.0f,5.0f,10.0f,25.0f}) {
            ImGui::SameLine();
            const std::string label=std::to_string(static_cast<int>(preset))+"##terrainStrength";
            if (UI::SmallButton(label.c_str())) state.brush.strength=preset;
        }
        {
            const char* falloffs[] = {L("Weich","Smooth"), L("Linear","Linear"), L("Kugel","Spherical"),
                                      L("Spitz","Tip"), L("Konstant","Constant")};
            int falloff = static_cast<int>(state.brush.falloff);
            if (UI::Combo(L("Falloff##terrainFalloff","Falloff##terrainFalloff"), &falloff, falloffs, 5))
                state.brush.falloff = static_cast<core::BrushFalloff>(falloff);
        }
        switch (state.brushMode) {
        case core::BrushMode::Flatten: {
            UI::Checkbox(L("Zielhöhe beim Strichbeginn aufnehmen","Pick target height at stroke start"), &state.flattenPickOnStroke);
            ImGui::BeginDisabled(state.flattenPickOnStroke);
            UI::InputFloat(L("Zielhöhe","Target height"), &state.brush.flattenTarget);
            ImGui::EndDisabled();
            const char* sides[] = {L("Anheben und absenken","Raise and lower"), L("Nur anheben","Raise only"), L("Nur absenken","Lower only")};
            int side = static_cast<int>(state.brush.flattenSide);
            if (UI::Combo(L("Richtung##flattenSide","Direction##flattenSide"), &side, sides, 3))
                state.brush.flattenSide = static_cast<core::FlattenSide>(side);
            break;
        }
        case core::BrushMode::Noise: {
            UI::SliderFloat(L("Wellenlänge","Wavelength"), &state.brush.noiseScale, 50.0f, 4000.0f);
            int seed = static_cast<int>(state.brush.noiseSeed);
            if (UI::InputInt(L("Seed##noiseSeed","Seed##noiseSeed"), &seed)) state.brush.noiseSeed = static_cast<std::uint32_t>(std::max(seed, 0));
            ImGui::TextDisabled("%s", L("Stärke = Amplitude je Anwendung.", "Strength = amplitude per application."));
            break;
        }
        case core::BrushMode::Terrace:
            UI::SliderFloat(L("Stufenhöhe","Step height"), &state.brush.terraceStep, 10.0f, 1000.0f);
            break;
        case core::BrushMode::Erode:
            UI::SliderFloat(L("Erlaubte Steigung je Block","Allowed slope per block"), &state.brush.erodeTalus, 0.0f, 200.0f);
            ImGui::TextDisabled("%s", L("Material rutscht von steileren Stellen bergab; das Volumen bleibt gleich.",
                                       "Material slides down from steeper spots; volume is preserved."));
            break;
        case core::BrushMode::Ramp:
            ImGui::TextWrapped("%s", L("Erster Klick setzt den Start, zweiter Klick das Ende (jeweils mit der Terrainhöhe dort). "
                                       "Breite = Radius, weicher Rand = halber Radius.",
                                       "First click sets the start, second click the end (each at the terrain height there). "
                                       "Width = radius, soft edge = half radius."));
            if (state.rampStart) {
                ImGui::TextColored(UiTheme::Warning, L("Start gesetzt: %.0f / %.0f, Höhe %.1f","Start set: %.0f / %.0f, height %.1f"),
                                   (*state.rampStart)[0], (*state.rampStart)[2], (*state.rampStart)[1]);
                if (UI::SmallButton(L("Start verwerfen","Discard start"))) state.rampStart.reset();
            }
            break;
        default: break;
        }
        DrawBrushAlphaControls(state, "terrainAlpha");
        DrawSplineControls(state);

        ImGui::Separator();
        ImGui::BeginDisabled(!CanUndoMapEdit(state));
        if (UI::Button(L("Rückgängig (Strg+Z)","Undo (Ctrl+Z)"))) UndoMapEdit(state);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!CanRedoMapEdit(state));
        if (UI::Button(L("Wiederholen (Strg+Y)","Redo (Ctrl+Y)"))) RedoMapEdit(state);
        ImGui::EndDisabled();

        ImGui::Separator();
        const auto [lo, hi] = state.heightmap.MinMax();
        ImGui::Text(L("Gitter: %u x %u","Grid: %u x %u"), state.heightmap.Width(), state.heightmap.Height());
        ImGui::Text(L("Höhen-Range: [%.2f, %.2f]","Height range: [%.2f, %.2f]"), lo, hi);
    } else if (state.editMode == EditMode::TexturePaint) {
        ImGui::TextDisabled("%s",L("Layer-Auswahl und Layer-Verwaltung befinden sich im separaten Layer-Dock.","Layer selection and management are available in the separate Layer dock."));
        ImGui::Separator();
        ImGui::Text("%s",L("Textur-Rasterauflösung","Texture grid resolution"));
        UI::InputInt(L("Breite##texResolution","Width##texResolution"), &state.textureResolutionWidth);
        UI::InputInt(L("Höhe##texResolution","Height##texResolution"), &state.textureResolutionHeight);
        state.textureResolutionWidth = std::clamp(state.textureResolutionWidth, 64, 4096);
        state.textureResolutionHeight = std::clamp(state.textureResolutionHeight, 64, 4096);
        ImGui::SameLine();
        if (UI::Button(L("Auflösung anwenden##texResolution","Apply resolution##texResolution"))) {
            if (state.textureStack.Width() != static_cast<std::uint32_t>(state.textureResolutionWidth) ||
                state.textureStack.Height() != static_cast<std::uint32_t>(state.textureResolutionHeight)) {
                const int selectedBefore = state.selectedLayer;
                core::TextureLayerStack before = std::move(state.textureStack);
                core::TextureLayerStack resized(
                    static_cast<std::uint32_t>(state.textureResolutionWidth),
                    static_cast<std::uint32_t>(state.textureResolutionHeight));
                for (std::size_t i = 0; i < before.LayerCount(); ++i) {
                    const auto& src = before.Layer(i);
                    const auto index = resized.AddLayer(src.name, src.diffuseFileName, src.uvScaleDiffuse);
                    auto& dst = resized.Layer(index);
                    dst.regionStartX = src.regionStartX;
                    dst.regionStartY = src.regionStartY;
                    dst.regionWidth = src.regionWidth;
                    dst.regionHeight = src.regionHeight;
                }
                state.textureStack = std::move(resized);
                RecordTextureStackReplacement(state, std::move(before), selectedBefore);
                state.layerPreviewDirty = true;
                state.renderer.UpdateBlendTextures(state.textureStack);
                state.mapDirty = true;
            }
        }
        ImGui::TextDisabled("%s",L("Hinweis: Anwenden setzt die Layer-Gewichte auf dem neuen Raster zurück.","Note: applying the resolution resets layer weights on the new grid."));

        ImGui::SeparatorText(L("Pinsel","Brush"));
        if (SceneQuickFilterButton("textureIncrease",L("Auftragen","Paint"),state.paintMode==core::PaintMode::Increase))
            state.paintMode=core::PaintMode::Increase;
        ImGui::SameLine();
        if (SceneQuickFilterButton("textureDecrease",L("Abtragen","Erase"),state.paintMode==core::PaintMode::Decrease))
            state.paintMode=core::PaintMode::Decrease;
        UI::SliderFloat("Radius##tex", &state.paintSettings.radius, 10.0f, 2000.0f);
        ImGui::TextDisabled("Radius-Presets");
        for (float preset : {50.0f,100.0f,250.0f,500.0f}) {
            ImGui::SameLine();
            const std::string label=std::to_string(static_cast<int>(preset))+"##textureRadius";
            if (UI::SmallButton(label.c_str())) state.paintSettings.radius=preset;
        }
        UI::SliderFloat(L("Stärke##tex","Strength##tex"), &state.paintSettings.strength, 0.01f, 1.0f);
        ImGui::TextDisabled("%s",L("Stärke-Presets","Strength presets"));
        for (float preset : {0.10f,0.25f,0.50f,1.00f}) {
            ImGui::SameLine();
            char label[32]; std::snprintf(label,sizeof(label),"%.2f##textureStrength",preset);
            if (UI::SmallButton(label)) state.paintSettings.strength=preset;
        }
        if (state.selectedLayer >= 0 && static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount()) {
            const std::string layerName = state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer)).name;
            bool locked = state.lockedTextureLayers.contains(layerName);
            if (UI::Checkbox(L("Gewählten Layer sperren##texLock", "Lock selected layer##texLock"), &locked)) {
                if (locked) state.lockedTextureLayers.insert(layerName); else state.lockedTextureLayers.erase(layerName);
            }
        }
        DrawBrushAlphaControls(state, "texAlpha");
        DrawTextureRuleControls(state);

        ImGui::Separator();
        ImGui::BeginDisabled(!CanUndoMapEdit(state));
        if (UI::Button(L("Rückgängig","Undo"))) UndoMapEdit(state);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!CanRedoMapEdit(state));
        if (UI::Button(L("Wiederholen","Redo"))) RedoMapEdit(state);
        ImGui::EndDisabled();

        if (state.selectedLayer >= 0) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", L("Jeder Layer hat seine eigene Maske und liegt über den Layern, die in der Liste vor ihm stehen. "
                                       "Malen ändert nur den gewählten Layer.",
                                       "Each layer has its own mask and lies over the layers listed before it. "
                                       "Painting changes only the selected layer."));
        }
    } else if (state.editMode == EditMode::BlockWalk) {
        ImGui::TextWrapped("%s",L("Block & Walk: jede Zelle (6.25 Einheiten) ist blockiert (rot) oder begehbar. Klick/Ziehen im 2D-View setzt Zellen im Kreis um den Mauszeiger.",
                                "Block & Walk: each cell (6.25 units) is blocked (red) or walkable. Click/drag in the 2D view paints cells in a circle around the cursor."));
        ImGui::TextColored(ImVec4(1.0f,0.38f,0.38f,1.0f),"%s",L("* blockiert","* blocked"));
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.32f,0.90f,0.58f,1.0f),"%s",L("* begehbar","* walkable"));
        if (SceneQuickFilterButton("walkBlock",L("Sperren","Block"),state.walkBlockMode)) state.walkBlockMode=true;
        ImGui::SameLine();
        if (SceneQuickFilterButton("walkOpen",L("Freigeben","Walkable"),!state.walkBlockMode)) state.walkBlockMode=false;
        UI::SliderFloat("Radius##walk", &state.walkSettings.radius, 1.0f, 1000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
        ImGui::TextDisabled("Radius-Presets");
        for (float preset : {6.25f,25.0f,50.0f,100.0f,250.0f}) {
            ImGui::SameLine();
            char label[32]; std::snprintf(label,sizeof(label),"%.0f##walkRadius",preset);
            if (UI::SmallButton(label)) state.walkSettings.radius=preset;
        }
        ImGui::SeparatorText(L("Aus sichtbaren Objekten","From visible objects"));
        const float footprintButtonW=std::max(120.0f,(ImGui::GetContentRegionAvail().x-8.0f)*0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button,IM_COL32(112,39,48,220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,IM_COL32(170,52,64,235));
        if (UI::Button(L("Vorschau: sperren","Preview: block"),ImVec2(footprintButtonW,34.0f))) {
            state.walkFootprintPreviewActive = true;
            state.walkFootprintPreviewBlocked = true;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,IM_COL32(20,101,72,220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,IM_COL32(26,145,98,235));
        if (UI::Button(L("Vorschau: freigeben","Preview: walkable"),ImVec2(footprintButtonW,34.0f))) {
            state.walkFootprintPreviewActive = true;
            state.walkFootprintPreviewBlocked = false;
        }
        ImGui::PopStyleColor(2);

        UI::Checkbox(L("Nur Modelle sperren, die im Original blockieren","Only block models that block in the original"),
                     &state.walkFootprintOnlyBlocking);
        if (state.walkFootprintOnlyBlocking) {
            const auto& stats = ModelBlockStats(state);
            std::vector<std::pair<std::string, int>> excluded;
            for (const auto& [model, agg] : stats)
                if (agg.first > 0 && agg.second * 2 < agg.first)
                    excluded.emplace_back(model, static_cast<int>(100.0 * agg.second / agg.first));
            std::sort(excluded.begin(), excluded.end());
            ImGui::TextDisabled(L("%zu Modelle ausgenommen (im Original überwiegend begehbar)",
                                  "%zu models excluded (mostly walkable in the original)"), excluded.size());
            if (!excluded.empty() && ImGui::IsItemHovered()) {
                std::string tip;
                for (std::size_t k = 0; k < excluded.size() && k < 30; ++k)
                    tip += std::filesystem::path(excluded[k].first).filename().string() + "  " + std::to_string(excluded[k].second) + " %\n";
                if (excluded.size() > 30) tip += "...";
                ImGui::SetTooltip("%s", tip.c_str());
            }
        }
        if (state.walkFootprintPreviewActive) {
            const auto previewPolygons = CollectVisibleObjectFootprints(state);
            const ImVec4 previewColor = state.walkFootprintPreviewBlocked
                ? ImVec4(1.0f,0.38f,0.38f,1.0f)
                : ImVec4(0.32f,0.90f,0.58f,1.0f);
            ImGui::TextColored(previewColor, L("Vorschau aktiv: %zu Grundflächen -> %s",
                                               "Preview active: %zu footprints -> %s"),
                               previewPolygons.size(),
                               state.walkFootprintPreviewBlocked ? L("blockiert","blocked")
                                                                 : L("begehbar","walkable"));
            ImGui::TextDisabled("%s",L("Die farbigen Polygone im 2D-View zeigen exakt die Geometrie, die angewendet wird.",
                                       "The colored polygons in the 2D view show exactly the geometry that will be applied."));
            const float actionW=std::max(100.0f,(ImGui::GetContentRegionAvail().x-8.0f)*0.5f);
            ImGui::BeginDisabled(previewPolygons.empty());
            if (DrawCompactIconTextButton("applyFootprints", L("Anwenden","Apply"), DrawIconSave,
                                          "panel.validation", true,
                                          L("Vorschau als einen Undo-Schritt anwenden",
                                            "Apply preview as one undo step"))) {
                StampObjectFootprints(state, state.walkFootprintPreviewBlocked);
                state.walkFootprintPreviewActive = false;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (DrawCompactIconTextButton("cancelFootprints", L("Abbrechen","Cancel"), DrawIconDelete,
                                          "edit.delete", true,
                                          L("Vorschau verwerfen, Gitter unverändert lassen",
                                            "Discard preview and leave grid unchanged"))) {
                state.walkFootprintPreviewActive = false;
                state.statusMessage = L("Grundflächen-Vorschau verworfen.",
                                        "Footprint preview discarded.");
            }
        } else {
            ImGui::TextDisabled("%s",L("Erst Vorschau wählen, im 2D-View prüfen und dann anwenden.",
                                       "Choose a preview first, inspect it in the 2D view, then apply."));
        }

        ImGui::TextDisabled("%s",L("Wirkt nur auf aktuell sichtbare Kategorien - ideal für Gebäude, Deko oder einzelne Gruppen.",
                                   "Affects only currently visible categories - ideal for buildings, decoration or selected groups."));
        ImGui::TextDisabled(L("Zellen im Gitter: %u x %u (%.0f x %.0f Einheiten)","Grid cells: %u x %u (%.0f x %.0f units)"), state.walkGrid.Cols(), state.walkGrid.Rows(),
                            state.walkGrid.Cols() * WalkGridCellSize(), state.walkGrid.Rows() * WalkGridCellSize());

        DrawWalkGeneratePanel(state);

        ImGui::SeparatorText(L("Historie","History"));
        if (DrawCompactIconTextButton("walkUndo", L("Rückgängig","Undo"), DrawIconUndo,
                                      "history.undo", CanUndoMapEdit(state),
                                      L("Letzte Kartenaktion rückgängig machen",
                                        "Undo last map action"))) {
            UndoMapEdit(state);
        }
        ImGui::SameLine();
        if (DrawCompactIconTextButton("walkRedo", L("Wiederholen","Redo"), DrawIconRedo,
                                      "history.redo", CanRedoMapEdit(state),
                                      L("Letzte rückgängig gemachte Kartenaktion wiederholen",
                                        "Redo last undone map action"))) {
            RedoMapEdit(state);
        }
    } else if (state.editMode == EditMode::ObjectPlacement) {
        if (state.standaloneNifMap) {
            state.objectPlaceMode = 0;
            ImGui::TextColored(UiTheme::Warning, "%s", L(
                "NIF-only: Szenengeometrie ist read-only. Auswahl/Fokus bleiben verfügbar.",
                "NIF-only: scene geometry is read-only. Selection/focus remain available."));
        }
        ImGui::Text("%s",L("Klick im Editor (2D) unten:","Click in the 2D editor below:"));
        ImGui::BeginDisabled(state.standaloneNifMap);
        UI::RadioButton(L("Platzieren","Place"), &state.objectPlaceMode, 1);
        ImGui::EndDisabled();
        ImGui::SameLine();
        UI::RadioButton(L("Auswählen","Select"), &state.objectPlaceMode, 0);
        ImGui::SameLine();
        ImGui::BeginDisabled(state.standaloneNifMap || !MapSupportsTerrainEditing(state));
        UI::RadioButton(L("Malen (Foliage)","Paint (foliage)"), &state.objectPlaceMode, 2);
        ImGui::EndDisabled();

        if (state.objectPlaceMode) {
            UI::InputText(L("Modellpfad","Model path"), state.newObjectModelPath, sizeof(state.newObjectModelPath));
            ImGui::SameLine();
            if (UI::Button(L("Durchsuchen...##nif","Browse...##nif"))) {
                if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
                    state.availableNifFiles = ListFilesByExtension(*resmapRoot, {".nif"});
                    state.nifAssetRoot = *resmapRoot;
                    state.resmapRootForThumbnails = *resmapRoot;
                    state.nifListScanned = true;
                    state.assetPickerFilter.clear();
                    if (state.availableNifFiles.empty()) {
                        state.statusMessage = L("Keine .nif-Dateien unter ","No .nif files found under ") + resmapRoot->string() + L(" gefunden.",".");
                    } else {
                        ImGui::OpenPopup("##nifPicker");
                    }
                } else {
                    state.statusMessage = L("Kein Client-Ordner/'resmap' aktiv - unter 'Neu' ein Projekt mit Client Ordner anlegen.","No client folder/'resmap' is active - create a project with a client folder under 'New'.");
                }
            }
            {
                std::string picked;
                if (DrawAssetPickerPopup("##nifPicker", state.availableNifFiles, state.assetPickerFilter, picked,
                                          state, state.nifAssetRoot, true, state.resmapRootForThumbnails)) {
                    const std::string legacyModelPath = ToLegacyResmapModelPath(picked);
                    std::snprintf(state.newObjectModelPath, sizeof(state.newObjectModelPath), "%s", legacyModelPath.c_str());
                }
            }
            if (state.objectPlaceMode == 2) {
                DrawScatterControls(state);
            } else {
                UI::SliderFloat(L("Rotation um Hochachse (°)##new","Rotation around up axis (°)##new"), &state.newObjectRotDeg, -180.0f, 180.0f);
                UI::SliderFloat(L("Skalierung##new","Scale##new"), &state.newObjectScale, 0.1f, 5.0f);
            }
        }

        DrawPrefabControls(state);
        ImGui::Separator();
        ImGui::TextDisabled("%s",L("Auswahl erfolgt im separaten Objekt-Outliner oder direkt in 2D/3D.","Select objects in the separate Object Outliner or directly in 2D/3D."));

        const bool multipleSelection = state.selectedObjects.size() > 1;
        if (multipleSelection) {
            ImGui::Text(L("%zu Objekte ausgewählt","%zu objects selected"), state.selectedObjects.size());
            ImGui::TextWrapped("%s",L("Koordinaten, Rotation, Skalierung und Drag&Drop wirken gemeinsam. Relative Abstände und Höhen bleiben beim Verschieben erhalten.",
                                    "Coordinates, rotation, scale and drag & drop affect the selection together. Relative spacing and heights are preserved when moving."));
        }

        SyncSelectedObjectModelPath(state);
        if (auto* obj = EditableObject(state, state.selectedObject)) {
            ImGui::SeparatorText(L("Transform-Werkzeug","Transform tool"));
            if (DrawIconButton("gizmoMoveProps","Move",DrawIconMove,state.objectGizmoOperation==0,ImVec2(72,52),true,"transform.move")) {
                state.objectGizmoOperation=0; state.objectGizmoMatrixValid=false;
            }
            ImGui::SameLine();
            if (DrawIconButton("gizmoRotateProps","Rotate",DrawIconRotate,state.objectGizmoOperation==1,ImVec2(72,52),true,"transform.rotate")) {
                state.objectGizmoOperation=1; state.objectGizmoMatrixValid=false;
            }
            ImGui::SameLine();
            if (DrawIconButton("gizmoScaleProps","Scale",DrawIconScale,state.objectGizmoOperation==2,ImVec2(72,52),true,"transform.scale")) {
                state.objectGizmoOperation=2; state.objectGizmoMatrixValid=false;
            }
            ImGui::SameLine();
            if (DrawTinyIconButton("gizmoSnapProps",DrawIconSnap,state.objectGizmoSnap,
                                   state.objectGizmoSnap ? L("Snap deaktivieren","Disable snap") : L("Snap aktivieren","Enable snap"),
                                   ImVec2(28,28)))
                state.objectGizmoSnap=!state.objectGizmoSnap;
            if (UI::Checkbox("Local##gizmoProps",&state.objectGizmoLocal)) state.objectGizmoMatrixValid=false;
            if (state.objectGizmoSnap) {
                if (state.objectGizmoOperation==0) {
                    UI::InputFloat("Grid-Snap",&state.objectMoveSnap,10.0f,50.0f,"%.1f");
                    state.objectMoveSnap=std::max(0.01f,state.objectMoveSnap);
                } else if (state.objectGizmoOperation==1) {
                    UI::InputFloat(L("Winkel-Snap (°)","Angle snap (°)"),&state.objectRotateSnap,1.0f,5.0f,"%.1f");
                    state.objectRotateSnap=std::max(0.1f,state.objectRotateSnap);
                } else {
                    UI::InputFloat("Scale-Snap",&state.objectScaleSnap,0.01f,0.10f,"%.2f");
                    state.objectScaleSnap=std::max(0.001f,state.objectScaleSnap);
                }
            }
            const std::string focusButtonLabel =
                std::string(L("Auswahl fokussieren (","Focus selection (")) +
                ShortcutLabel(state.shortcutFocus) + ")";
            const std::string groundButtonLabel =
                std::string(L("Auf Terrain (","Place on terrain (")) +
                ShortcutLabel(state.shortcutGround) + ")";
            if (UI::Button(focusButtonLabel.c_str())) FocusSelectedObjects(state);
            ImGui::SameLine();
            if (UI::Button(groundButtonLabel.c_str())) GroundSelectedObjects(state);
            ImGui::Separator();
            const bool shmdScene = IsShmdSelection(state.selectedObject);
            if (shmdScene) {
                const std::string categoryName = ShmdSelectionCategoryName(state, state.selectedObject);
                ImGui::Text(L("SHMD-Kategorie: %s","SHMD category: %s"), categoryName.empty() ? L("Szenenmodell","Scene model") : categoryName.c_str());
            }
            if (shmdScene || (multipleSelection && std::any_of(state.selectedObjects.begin(), state.selectedObjects.end(),
                                                               [](int id) { return IsShmdSelection(id); }))) {
                ImGui::TextWrapped("%s",L("Transformänderungen wandeln betroffene Sky/Water/GroundObject-Einträge automatisch in normale Placements um, da die SHMD-Kategorielisten keine Transformfelder besitzen.",
                                        "Transform changes automatically convert affected Sky/Water/GroundObject entries into normal placements because the SHMD category lists do not contain transform fields."));
            }

            ImGui::BeginDisabled(multipleSelection);
            if (multipleSelection) ImGui::TextDisabled("%s",L("Modellwechsel nur bei Einzelauswahl","Model changes require a single selection"));
            if (UI::InputText(L("Modellpfad##selected","Model path##selected"), state.selectedObjectModelPath, sizeof(state.selectedObjectModelPath))) {
                state.selectedObjectModelPathDirty = true;
            }
            if (state.selectedObjectModelPathDirty && ImGui::IsItemDeactivatedAfterEdit()) {
                ApplySelectedObjectModelPath(state, state.selectedObjectModelPath);
                state.selectedObjectModelPathDirty = false;
                obj = EditableObject(state, state.selectedObject);
            }
            ImGui::SameLine();
            if (UI::Button(L("Durchsuchen...##selectedNif","Browse...##selectedNif"))) {
                if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
                    state.availableNifFiles = ListFilesByExtension(*resmapRoot, {".nif"});
                    state.nifAssetRoot = *resmapRoot;
                    state.resmapRootForThumbnails = *resmapRoot;
                    state.assetPickerFilter.clear();
                    if (!state.availableNifFiles.empty()) ImGui::OpenPopup("##selectedNifPicker");
                }
            }
            {
                std::string picked;
                if (DrawAssetPickerPopup("##selectedNifPicker", state.availableNifFiles, state.assetPickerFilter, picked,
                                         state, state.nifAssetRoot, true, state.resmapRootForThumbnails)) {
                    const std::string legacyModelPath = ToLegacyResmapModelPath(picked);
                    ApplySelectedObjectModelPath(state, legacyModelPath);
                    obj = EditableObject(state, state.selectedObject);
                }
            }
            ImGui::EndDisabled();

            if (obj) {
                const float oldX = obj->posX, oldY = obj->posY, oldZ = obj->posZ;
                float position[3] = {oldX, oldY, oldZ};
                if (ImGui::InputFloat3(multipleSelection ? L("Position (aktives Objekt)","Position (active object)") : "Position", position, "%.1f")) {
                    BeginObjectEditTransaction(state);
                    MoveSelectedObjectsBy(state, position[0] - oldX, position[1] - oldY, position[2] - oldZ);
                    obj = EditableObject(state, state.selectedObject);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) CommitObjectEditTransaction(state);

                if (obj) {
                    const EditQuat oldQ=NormalizeEditQuat({obj->rotX,obj->rotY,obj->rotZ,obj->rotW});
                    app::Mat4 matrix=ObjectEditMatrix({obj->posX,obj->posY,obj->posZ},oldQ,std::max(0.001f,obj->scale));
                    float tr[3]{}, rot[3]{}, sc[3]{};
                    ImGuizmo::DecomposeMatrixToComponents(matrix.m,tr,rot,sc);
                    float editedRot[3]={rot[0],rot[1],rot[2]};
                    if (ImGui::InputFloat3("Rotation XYZ (°)",editedRot,"%.1f")) {
                        BeginObjectEditTransaction(state);
                        float uniformScale[3]={std::max(0.001f,obj->scale),std::max(0.001f,obj->scale),std::max(0.001f,obj->scale)};
                        app::Mat4 changed=app::Mat4::Identity();
                        ImGuizmo::RecomposeMatrixFromComponents(tr,editedRot,uniformScale,changed.m);
                        const EditQuat newQ=MatrixRotationQuat(changed);
                        const EditQuat delta=MulEditQuat(newQ,ConjugateEditQuat(oldQ));
                        const auto pivot=ComputeObjectSelectionPivot(state);
                        if(pivot.valid) RotateSelectedObjectsAroundPivot(state,pivot.position,delta);
                        state.objectGizmoMatrixValid=false;
                        obj=EditableObject(state,state.selectedObject);
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit()) CommitObjectEditTransaction(state);
                }

                if (obj) {
                    const float oldScale = obj->scale;
                    float scale = oldScale;
                    if (UI::SliderFloat(L("Skalierung","Scale"), &scale, 0.01f, 100.0f, "%.3f",
                                        ImGuiSliderFlags_Logarithmic)) {
                        BeginObjectEditTransaction(state);
                        const float factor = oldScale > 1.0e-6f ? scale / oldScale : 1.0f;
                        ScaleSelectedObjectsBy(state, factor);
                        state.objectGizmoMatrixValid=false;
                        obj = EditableObject(state, state.selectedObject);
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit()) CommitObjectEditTransaction(state);
                }
            }

            if (UI::Button(L("Ausgewählte Objekte löschen","Delete selected objects"))) DeleteSelectedObjects(state);
        }

        ImGui::Separator();
        if (UI::Button(L("Alle normalen Objekte entfernen","Remove all normal objects"))) ImGui::OpenPopup("##deleteAllNormalObjects");
        if (ImGui::BeginPopupModal("##deleteAllNormalObjects", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(L("Wirklich alle %zu normalen Placement-Objekte entfernen?","Really remove all %zu normal placement objects?"), state.placementSet.Count());
            ImGui::TextDisabled("%s",L("Sky, Water und GroundObject bleiben erhalten.","Sky, Water and GroundObject are preserved."));
            if (UI::Button(L("Alle normalen entfernen","Remove all normal objects"))) {
                DeleteAllNormalObjects(state);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UI::Button(L("Abbrechen","Cancel"))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    } else if (state.editMode == EditMode::Npcs) {
        if (state.shineTextRoot.empty()) {
            ImGui::TextWrapped("%s",L("Server-Textdaten-Ordner (Server/9Data/Shine, enthält World/NPC.txt) noch nicht gewählt.","Server text-data folder (Server/9Data/Shine, containing World/NPC.txt) has not been selected yet."));
            std::vector<char> rootBuf(state.shineTextRoot.begin(), state.shineTextRoot.end());
            rootBuf.resize(std::max<std::size_t>(rootBuf.size() + 1, 512));
            ImGui::SetNextItemWidth(-1.0f);
            if (UI::InputText("##shinetextrootnpc", rootBuf.data(), rootBuf.size())) {
                state.shineTextRoot.assign(rootBuf.data()); state.npcTextLoaded = false;
            }
#ifdef _WIN32
            if (UI::Button(L("Ordner wählen...##shinetextnpc","Choose folder...##shinetextnpc"))) {
                if (auto p = BrowseForFolderWindows(L("Server/9Data/Shine Ordner wählen","Choose Server/9Data/Shine folder"))) { state.shineTextRoot = *p; state.npcTextLoaded = false; }
            }
#endif
        } else {
            EnsureNpcTextLoaded(state);
            if (!state.npcTextLoaded) {
                ImGui::TextWrapped(L("World/NPC.txt konnte unter '%s' nicht geladen werden.","World/NPC.txt could not be loaded from '%s'."), state.shineTextRoot.c_str());
            } else if (state.legacySaveStem[0] == '\0') {
                ImGui::TextDisabled("%s",L("Keine Karte offen.","No map open."));
            } else {
                auto indices = NpcRecordsForCurrentMap(state);
                auto* table = state.npcTextFile.FindTable("ShineNPC");
                ImGui::TextDisabled(L("%zu NPCs auf '%s' · Auswahl links im Szene-Outliner","%zu NPCs on '%s' · select in the Scene Outliner on the left"),
                                    indices.size(), state.legacySaveStem);
                if (state.selectedNpcRecordIdx >= 0 && static_cast<std::size_t>(state.selectedNpcRecordIdx) < table->records.size()) {
                    auto& rec = table->records[static_cast<std::size_t>(state.selectedNpcRecordIdx)];
                    if (rec.values.size() >= 8) {
                        ImGui::Separator();
                        ImGui::TextColored(UiTheme::AccentCyan,"%s",rec.values[0].c_str());
                        ImGui::SeparatorText("Transform");
                        int x = std::atoi(rec.values[2].c_str());
                        int y = std::atoi(rec.values[3].c_str());
                        bool changed = false;
                        changed |= UI::InputInt("Coord-X", &x);
                        changed |= UI::InputInt("Coord-Y", &y);
                        if (changed) {
                            rec.values[2] = std::to_string(x); rec.values[3] = std::to_string(y);
                            RefreshNpcTransforms(state); // Modelle nur neu positionieren (nicht neu laden)
                        }
                        int direct = std::atoi(rec.values[4].c_str());
                        if (UI::InputInt(L("Richtung","Direction"), &direct)) { rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        // Schnell drehen (Blickrichtung im 3D-View pruefen) + Zuordnung Richtung -> Blickwinkel einstellbar
                        if (UI::SmallButton("-15")) { direct -= 15; rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        ImGui::SameLine();
                        if (UI::SmallButton("+15")) { direct += 15; rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        ImGui::SameLine();
                        if (UI::SmallButton("+90")) { direct += 90; rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        ImGui::SameLine();
                        if (UI::SmallButton("180")) { direct += 180; rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        if (UI::Button(L("Kamera zu diesem NPC","Camera to this NPC"))) {
                            const float nx = static_cast<float>(x), nz = static_cast<float>(y);
                            state.camera.SetTarget(nx, state.heightmap.SampleWorld(nx, nz) + 25.0f, nz);
                            state.camera.Zoom(-state.camera.Distance() + 220.0f);
                        }
                        ImGui::SeparatorText(L("Darstellung & Blickrichtung","Appearance & facing direction"));
                        {
                            bool orientChanged = false;
                            int signIdx = state.npcDirSign < 0 ? 1 : 0;
                            const char* signItems[] = {L("Richtung wie angegeben (+)","Direction as specified (+)"), L("Richtung gespiegelt (-)","Mirrored direction (-)")};
                            ImGui::SetNextItemWidth(220.0f);
                            if (UI::Combo(L("Drehsinn","Rotation sense"), &signIdx, signItems, 2)) { state.npcDirSign = signIdx == 1 ? -1 : 1; orientChanged = true; }
                            ImGui::SetNextItemWidth(220.0f);
                            // Freier Versatz statt fester 90-Grad-Stufen - die Block&Walk-Analyse fand das beste
                            // Ergebnis nicht exakt auf einem 90-Grad-Vielfachen.
                            if (UI::SliderInt(L("Versatz bei Richtung 0 (Grad)","Offset at direction 0 (degrees)"), &state.npcDirOffsetDeg, -180, 180, L("%d Grad","%d deg"))) orientChanged = true;
                            ImGui::SameLine();
                            if (UI::SmallButton("0##offreset")) { state.npcDirOffsetDeg = 0; orientChanged = true; }
                            if (orientChanged) RefreshNpcTransforms(state);
                            ImGui::TextDisabled("%s",L("Der Pfeil zeigt die Blickrichtung des Modells. Stimmt er nicht mit dem Spiel ueberein,\nDrehsinn/Versatz hier einstellen (gilt fuer alle NPCs) oder unten schaetzen lassen.","The arrow shows the model facing direction. If it does not match the game,\nadjust rotation sense/offset here (applies to all NPCs) or estimate it below."));
                            if (UI::Button(L("Versatz aus dieser Karte schaetzen (Block&Walk)","Estimate offset from this map (Block & Walk)"))) EstimateNpcOrientation(state);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L(
                                "Testet fuer alle NPCs dieser Karte, welcher Versatz sie im Schnitt am ehesten von der naechsten Wand weg blicken laesst\n"
                                "(Annahme: NPCs stehen normalerweise mit dem Ruecken zur Wand). Grobe Schaetzung, kein Ersatz fuer einen Blick ins echte Spiel.",
                                "For all NPCs on this map, tests which offset most often makes them face away from the nearest wall on average\n"
                                "(assumption: NPCs usually stand with their back to a wall). A rough estimate, not a substitute for checking the real game."));
                            if (!state.npcOrientEstimateStatus.empty()) ImGui::TextWrapped("%s", state.npcOrientEstimateStatus.c_str());
                        }
                        ImGui::SeparatorText(L("Rolle","Role"));
                        {
                            // Rolle und Rollenargument (NPC.txt: Spalten Role/RoleArg0) - bearbeitbar.
                            // Bekannte Kombinationen aus NA2016: QuestNpc/Quest|GBDice, Guard/Quest,
                            // Merchant/Item|Weapon|WeaponTitle|Skill|SoulStone|Guild, NPCMenu/Guild|
                            // ExchangeCoin|RandomOption, StoreManager/-. Merchant mit Item/Weapon/
                            // WeaponTitle/Skill/Guild nutzt NPCItemList/<NPC>.txt (siehe Händler-Inventar).
                            ImGui::SetNextItemWidth(160.0f);
                            if (ImGui::BeginCombo(L("Rolle","Role"), rec.values[6].empty() ? L("(keine)","(none)") : rec.values[6].c_str())) {
                                for (const char* role : kNpcRoles) {
                                    if (UI::Selectable(role, rec.values[6] == role)) rec.values[6] = role;
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(160.0f);
                            {
                                static const char* const kArgs[] = {"Quest", "GBDice", "Item", "Weapon", "WeaponTitle", "Skill", "SoulStone", "Guild", "ExchangeCoin", "RandomOption", "-"};
                                if (ImGui::BeginCombo(L("Argument","Argument"), rec.values[7].empty() ? L("(keins)","(none)") : rec.values[7].c_str())) {
                                    for (const char* arg : kArgs) {
                                        if (UI::Selectable(arg, rec.values[7] == arg)) rec.values[7] = arg;
                                    }
                                    ImGui::EndCombo();
                                }
                            }
                        }
                        ImGui::SeparatorText("Dialog · AI · Route");
                        if (DrawIconButton("npcDialogAction","Dialog",DrawIconDialog,false,ImVec2(86,56)))
                            OpenNpcDialogEditor(state,rec.values[0]);
                        ImGui::SameLine();
                        if (DrawIconButton("npcLuaAction","Lua / AI",DrawIconCode,false,ImVec2(86,56),true,"module.ai"))
                            OpenAiScriptEditor(state,rec.values[0]);
                        ImGui::SameLine();
                        if (DrawIconButton("npcRouteAction","Route",DrawIconRoute,false,ImVec2(86,56),true,"gameplay.path"))
                            OpenPatrolRouteEditor(state,rec.values[0]);

                        if (rec.values[6] == "Merchant") {
                            ImGui::SameLine();
                            if (DrawIconButton("npcShopAction","Shop",DrawIconShop,false,ImVec2(86,56))) {
                                EnsureShopTextLoaded(state,rec.values[0]);
                                state.shopEditorOpen=true;
                            }
                        }
                    }
                }
                DrawDialogEditorPopup(state);
                ImGui::Separator();
                if (UI::Button(L("World/NPC.txt speichern","Save World/NPC.txt"))) {
                    auto saved = SaveProjectShineOverride(
                        state.npcTextFile,
                        ProjectServerShinePath(state.project, std::filesystem::path("World") / "NPC.txt"));
                    state.statusMessage = saved
                        ? std::string(L("NPC.txt als Projekt-Override gespeichert.","NPC.txt saved as project override."))
                        : L("Fehler: ","Error: ") + saved.error();
                }
            }
        }
        DrawShopEditorPopup(state);
    } else if (state.editMode == EditMode::Mobs) {
        if (state.shineTextRoot.empty()) {
            ImGui::TextWrapped("%s",L("Server-Textdaten-Ordner (Server/9Data/Shine, enthält MobRegen/) noch nicht gewählt.","Server text-data folder (Server/9Data/Shine, containing MobRegen/) has not been selected yet."));
            std::vector<char> rootBuf(state.shineTextRoot.begin(), state.shineTextRoot.end());
            rootBuf.resize(std::max<std::size_t>(rootBuf.size() + 1, 512));
            ImGui::SetNextItemWidth(-1.0f);
            if (UI::InputText("##shinetextrootmob", rootBuf.data(), rootBuf.size())) {
                state.shineTextRoot.assign(rootBuf.data()); state.mobRegenTextLoaded = false;
            }
#ifdef _WIN32
            if (UI::Button(L("Ordner wählen...##shinetextmob","Choose folder...##shinetextmob"))) {
                if (auto p = BrowseForFolderWindows(L("Server/9Data/Shine Ordner wählen","Choose Server/9Data/Shine folder"))) { state.shineTextRoot = *p; state.mobRegenTextLoaded = false; }
            }
#endif
        } else if (state.legacySaveStem[0] == '\0') {
            ImGui::TextDisabled("%s",L("Keine Karte offen.","No map open."));
        } else {
            EnsureMobRegenLoaded(state);
            if (!state.mobRegenTextLoaded) {
                ImGui::TextWrapped(L("MobRegen/%s.txt konnte nicht geladen werden.","MobRegen/%s.txt could not be loaded."), state.legacySaveStem);
            } else {
                auto* zoneTable = state.mobRegenTextFile.FindTable("MobRegenGroup");
                auto* spawnTable = state.mobRegenTextFile.FindTable("MobRegen");
                if (!zoneTable) {
                    ImGui::TextDisabled("%s",L("Keine 'MobRegenGroup'-Tabelle gefunden.","No 'MobRegenGroup' table found."));
                } else {
                    ImGui::TextDisabled(L("%zu Spawn-Zonen auf '%s' · Auswahl links im Szene-Outliner","%zu spawn zones on '%s' · select in the Scene Outliner on the left"),
                                        zoneTable->records.size(), state.legacySaveStem);
                    ImGui::SeparatorText(L("Zonenverwaltung","Zone management"));
                    UI::Checkbox(L("Löschen freigeben","Enable deletion"), &state.mobDeleteArmed);
                    if (UI::Button(L("+ Zone (Kopie der gewählten)","+ Zone (copy selected)")) && state.selectedMobZoneIdx >= 0 &&
                        static_cast<std::size_t>(state.selectedMobZoneIdx) < zoneTable->records.size()) {
                        core::legacy::ShineRecord nz = zoneTable->records[static_cast<std::size_t>(state.selectedMobZoneIdx)];
                        nz.sourceLine = 0;
                        // Freien Namen "<Karte><nn>" suchen - GroupIndex verknüpft Zone und Monster.
                        std::string name;
                        for (int n = 1; n < 1000 && name.empty(); ++n) {
                            char buf[64];
                            std::snprintf(buf, sizeof(buf), "%s%02d", state.legacySaveStem, n);
                            bool used = false;
                            for (auto& z : zoneTable->records) if (!z.values.empty() && z.values[0] == buf) used = true;
                            if (!used) name = buf;
                        }
                        if (!nz.values.empty()) nz.values[0] = name;
                        zoneTable->records.push_back(std::move(nz));
                        state.selectedMobZoneIdx = static_cast<int>(zoneTable->records.size()) - 1;
                        state.statusMessage = L("Neue Zone '","New zone '") + name + L("' angelegt (noch ohne Monster).","' created (no monsters yet).");
                    }
                    if (state.mobDeleteArmed && state.selectedMobZoneIdx >= 0 &&
                        static_cast<std::size_t>(state.selectedMobZoneIdx) < zoneTable->records.size()) {
                        ImGui::SameLine();
                        if (UI::Button(L("Zone samt Monstern löschen","Delete zone and monsters"))) {
                            const std::string gone = zoneTable->records[static_cast<std::size_t>(state.selectedMobZoneIdx)].values.empty()
                                ? std::string() : zoneTable->records[static_cast<std::size_t>(state.selectedMobZoneIdx)].values[0];
                            zoneTable->records.erase(zoneTable->records.begin() + state.selectedMobZoneIdx);
                            if (spawnTable) {
                                spawnTable->records.erase(std::remove_if(spawnTable->records.begin(), spawnTable->records.end(),
                                    [&](const core::legacy::ShineRecord& r) { return !r.values.empty() && r.values[0] == gone; }), spawnTable->records.end());
                            }
                            state.selectedMobZoneIdx = -1;
                            state.mobDeleteArmed = false;
                        }
                    }
                    if (state.selectedMobZoneIdx >= 0 && static_cast<std::size_t>(state.selectedMobZoneIdx) < zoneTable->records.size()) {
                        auto& zone = zoneTable->records[static_cast<std::size_t>(state.selectedMobZoneIdx)];
                        if (zone.values.size() >= 7) {
                            ImGui::SeparatorText("Zone");
                            ImGui::TextColored(UiTheme::AccentCyan,"Zone: %s",zone.values[0].c_str());
                            ImGui::TextDisabled("%s",L("Position, Ausdehnung, Spawn-Radius und weitere Zonenwerte.","Position, extent, spawn radius and other zone values."));
                            DrawShineRecordFields(zone.values, zoneTable->columns, 1);
                            if (spawnTable) {
                                ImGui::SeparatorText(L("Monstergruppe","Monster group"));
                                ImGui::TextDisabled("%s",L("Monster, Anzahl, Spawnwerte sowie AI/Lua und Route je Eintrag.","Monster, amount, spawn values plus AI/Lua and route for each entry."));
                                int removeSpawn = -1;
                                for (std::size_t si = 0; si < spawnTable->records.size(); ++si) {
                                    auto& sr = spawnTable->records[si];
                                    if (sr.values.empty() || sr.values[0] != zone.values[0] || sr.values.size() < 3) continue;
                                    ImGui::PushID(static_cast<int>(si));
                                    const std::string header = sr.values[1] + " x" + sr.values[2] + "##spawn";
                                    if (UI::CollapsingHeader(header.c_str())) {
                                        DrawShineRecordFields(sr.values, spawnTable->columns, 1);
                                        if (DrawTinyIconButton("mobLua",DrawIconCode,false,L("KI / Lua bearbeiten","Edit AI / Lua"),
                                                               ImVec2(22,22),"module.ai"))
                                            OpenAiScriptEditor(state,sr.values[1]);
                                        ImGui::SameLine(0,3);
                                        if (DrawTinyIconButton("mobRoute",DrawIconRoute,false,L("MobRoam-Route bearbeiten","Edit MobRoam route"),
                                                                 ImVec2(22,22),"gameplay.path"))
                                            OpenPatrolRouteEditor(state,sr.values[1]);
                                        ImGui::SameLine();
                                        if (state.mobDeleteArmed && UI::SmallButton(L("Monster entfernen","Remove monster"))) removeSpawn = static_cast<int>(si);
                                    }
                                    ImGui::PopID();
                                }
                                if (removeSpawn >= 0) spawnTable->records.erase(spawnTable->records.begin() + removeSpawn);
                                // Neues Monster in dieser Zone: Name (MobIndex aus MobInfoServer.shn) + Anzahl.
                                ImGui::SetNextItemWidth(160.0f);
                                UI::InputTextWithHint("##newmob", L("Monstername (MobIndex)","Monster name (MobIndex)"), state.newMobSpawnName, sizeof(state.newMobSpawnName));
                                ImGui::SameLine();
                                if (UI::Button(L("+ Monster hinzufügen","+ Add monster")) && state.newMobSpawnName[0] != '\0') {
                                    core::legacy::ShineRecord nr;
                                    nr.sourceLine = 0;
                                    nr.values.assign(std::max<std::size_t>(spawnTable->columns.size(), 3), "0");
                                    nr.values[0] = zone.values[0];
                                    nr.values[1] = state.newMobSpawnName;
                                    nr.values[2] = "1";
                                    spawnTable->records.push_back(std::move(nr));
                                    state.statusMessage = std::string(L("Monster '","Monster '")) + state.newMobSpawnName + L("' zur Zone hinzugefügt (Werte danach anpassen).","' added to the zone (adjust values afterwards).");
                                }
                            }
                        }
                    }
                }
                ImGui::Separator();
                if (UI::Button(L("MobRegen speichern","Save MobRegen"))) {
                    auto saved = SaveProjectShineOverride(
                        state.mobRegenTextFile,
                        ProjectServerShinePath(
                            state.project,
                            std::filesystem::path("MobRegen") / (CurrentMapServerName(state) + ".txt")));
                    state.statusMessage = saved
                        ? std::string(L("MobRegen als Projekt-Override gespeichert.","MobRegen saved as project override."))
                        : L("Fehler: ","Error: ") + saved.error();
                }
            }
        }
    } else if (state.editMode == EditMode::Portals) {
        DrawPortalsToolsPanel(state);
    }
    DrawAiScriptEditorPopup(state); // aus Npcs- UND Mobs-Tab erreichbar, siehe CHANGELOG [0.44.22]
    DrawPatrolRouteEditorPopup(state); // dito, siehe CHANGELOG [0.44.23]

}

} // namespace nge
