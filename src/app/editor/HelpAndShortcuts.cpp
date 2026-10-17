// HelpAndShortcuts.cpp
// Handbuch, Hilfeleiste, Kamera-Lesezeichen, Spieltest, globale Tastenkürzel.

#include "EditorShared.hpp"

namespace nge {

// ============================================================================
// Handbuch (F1): Kapitel/Suche links, Text rechts, dazu die "Spalten-Referenz (live)" ueber alle geladenen
// SHN-Tabellen. Inhalte: core::manual (zweisprachig). CHANGELOG [0.44.33].
// ============================================================================
void DrawManualBody(const char* body) {
    std::istringstream in(body);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) { ImGui::Spacing(); continue; }
        if (line.rfind("# ", 0) == 0) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "%s", line.c_str() + 2);
            ImGui::Separator();
        } else if (line.rfind("- ", 0) == 0) {
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", line.c_str() + 2);
        } else {
            ImGui::TextWrapped("%s", line.c_str());
        }
    }
}
void DrawColumnReference(EditorState& state) {
    const bool de = app::CurrentLanguage() == app::Language::German;
    ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "%s", L("Spalten-Referenz (live)", "Column reference (live)"));
    ImGui::TextWrapped("%s", L("Zeigt alle Spalten der in den SHN-Editor geladenen Tabellen mit Typ und - wo bekannt - Beschreibung. 'vermutet' = aus dem Namen abgeleitet, nicht belegt; '-' = noch nicht beschrieben.",
                               "Shows all columns of the tables loaded into the SHN editor with type and - where known - a description. 'assumed' = derived from the name, not proven; '-' = not described yet."));
    if (state.shnFiles.empty()) { ImGui::TextDisabled("%s", L("Noch keine SHN geladen (SHN-Editor öffnen).", "No SHN loaded yet (open the SHN editor).")); return; }
    std::vector<std::string> names;
    for (const auto& d : state.shnFiles) names.push_back(std::string(ShnSourceName(d.source)) + ": " + d.file.FileName());
    state.manualColumnDoc = std::clamp(state.manualColumnDoc, 0, static_cast<int>(names.size()) - 1);
    ImGui::SetNextItemWidth(360.0f);
    if (ImGui::BeginCombo("##coldoc", names[static_cast<std::size_t>(state.manualColumnDoc)].c_str())) {
        for (std::size_t i = 0; i < names.size(); ++i)
            if (ImGui::Selectable((names[i] + "##cd" + std::to_string(i)).c_str(), static_cast<int>(i) == state.manualColumnDoc)) state.manualColumnDoc = static_cast<int>(i);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    DrawSearchInput("manualColumnFilter",L("Spalte suchen","Search column"),
                    state.manualColumnFilter,sizeof(state.manualColumnFilter),175.0f);
    const auto& doc = state.shnFiles[static_cast<std::size_t>(state.manualColumnDoc)];
    std::string table = doc.file.FileName();
    if (table.size() > 4 && LowerAscii(table.substr(table.size() - 4)) == ".shn") table.resize(table.size() - 4);
    const std::string needle = LowerAscii(state.manualColumnFilter);
    ImGui::TextDisabled("%s: %zu %s, %zu %s", table.c_str(), doc.file.columns.size(), L("Spalten", "columns"), doc.file.rows.size(), L("Zeilen", "rows"));
    if (ImGui::BeginTable("##coltbl", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 34.0f);
        ImGui::TableSetupColumn(L("Spalte", "Column"), ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableSetupColumn(L("Typ", "Type"), ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn(L("Beschreibung", "Description"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < doc.file.columns.size(); ++i) {
            const auto& col = doc.file.columns[i];
            if (!needle.empty() && LowerAscii(col.name).find(needle) == std::string::npos) continue;
            const auto* info = core::manual::FindColumn(table, col.name);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", i);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(col.name.c_str());
            ImGui::TableSetColumnIndex(2); {
                static const char* const kKindNames[] = {"UInt8", "UInt16", "UInt32", "Int8", "Int16", "Int32", "Float", "Text", "2xUInt32", "Raw"};
                ImGui::TextDisabled("%s", kKindNames[std::min<std::size_t>(static_cast<std::size_t>(col.kind), 9)]);
            }
            ImGui::TableSetColumnIndex(3);
            if (info) {
                ImGui::TextWrapped("%s%s", de ? info->de : info->en, info->certain ? "" : (de ? "  (vermutet)" : "  (assumed)"));
            } else {
                ImGui::TextDisabled("-");
            }
        }
        ImGui::EndTable();
    }
}
void DrawManualWindow(EditorState& state) {
    if (!state.manualOpen) return;
    const bool de = app::CurrentLanguage() == app::Language::German;
    ImGui::SetNextWindowSize(ImVec2(1050.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(120.0f, 60.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(L("Handbuch", "Manual"), &state.manualOpen)) { ImGui::End(); return; }
    DrawSearchInput("manualSearch",
                    L("Suchen (z.B. Kamera, Skill, Shop, Walk ...)",
                      "Search (e.g. camera, skill, shop, walk ...)"),
                    state.manualQuery,sizeof(state.manualQuery),315.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", L("F1 = Handbuch öffnen/schließen", "F1 = open/close the manual"));
    ImGui::Separator();
    ImGui::BeginChild("##manualnav", ImVec2(300.0f, 0.0f), true);
    const bool searching = state.manualQuery[0] != '\0';
    if (searching) {
        const auto hits = core::manual::Search(state.manualQuery, de);
        ImGui::TextDisabled("%zu %s", hits.size(), L("Treffer", "hits"));
        for (const auto* sec : hits) {
            if (ImGui::Selectable(((de ? sec->titleDe : sec->titleEn) + std::string("##s_") + sec->id).c_str(), state.manualSection == sec->id)) state.manualSection = sec->id;
        }
    } else {
        for (const auto& ch : core::manual::Chapters()) {
            if (!ImGui::TreeNodeEx((de ? ch.titleDe : ch.titleEn), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) continue;
            for (const auto& sec : core::manual::Sections()) {
                if (std::string(sec.chapterId) != ch.id) continue;
                if (ImGui::Selectable(((de ? sec.titleDe : sec.titleEn) + std::string("##s_") + sec.id).c_str(), state.manualSection == sec.id)) state.manualSection = sec.id;
            }
            if (std::string(ch.id) == "reference") {
                if (ImGui::Selectable((std::string(L("Spalten-Referenz (live)", "Column reference (live)")) + "##s_reference.columns").c_str(), state.manualSection == "reference.columns")) state.manualSection = "reference.columns";
            }
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##manualbody", ImVec2(0.0f, 0.0f), true);
    if (state.manualSection == "reference.columns") {
        DrawColumnReference(state);
    } else {
        const core::manual::Section* current = nullptr;
        for (const auto& sec : core::manual::Sections()) if (state.manualSection == sec.id) current = &sec;
        if (current == nullptr) {
            ImGui::TextDisabled("%s", L("Abschnitt links wählen.", "Choose a section on the left."));
        } else {
            ImGui::TextColored(ImVec4(0.40f, 0.72f, 0.96f, 1.0f), "%s", de ? current->titleDe : current->titleEn);
            ImGui::Separator();
            DrawManualBody(de ? current->bodyDe : current->bodyEn);
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
// Kleine, immer sichtbare Leiste unten rechts: Handbuch + Sprache (auf allen Bildschirmen erreichbar).
void DrawGlobalHelpBar(EditorState& state, const ImVec2& displaySize) {
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 12.0f, displaySize.y - 10.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 25, 32, 220));
    ImGui::Begin("##HelpBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    if (ImGui::Button(L("? Handbuch (F1)", "? Manual (F1)"))) state.manualOpen = !state.manualOpen;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Öffnet das Handbuch: Tasten, Funktionen, Spalten - alles erklärt.", "Opens the manual: keys, functions, columns - everything explained."));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(64.0f);
    int langIdx = app::CurrentLanguage() == app::Language::German ? 0 : 1;
    const char* langItems[] = {"DE", "EN"};
    if (ImGui::Combo("##lang2", &langIdx, langItems, 2)) app::SetLanguage(langIdx == 0 ? app::Language::German : app::Language::English);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Sprache der Oberfläche, Tooltips und des Handbuchs", "Language of the interface, tooltips and manual"));
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
// ---- Kamera-Lesezeichen / Ansichts-Presets ----------------------------------------------------
// Drehen verlässt eine Achsenansicht und schaltet zurück auf die Perspektive.
void LeaveAxisView(EditorState& state) {
    state.viewportViewPreset = 0;
    state.camera.SetOrthographic(false);
}
void SetCameraBookmark(EditorState& state, int slot) {
    if (slot < 0 || slot >= static_cast<int>(core::level::CameraBookmarkSet::kSlots)) return;
    EnsureCameraBookmarksLoaded(state);
    if (state.cameraBookmarksForMap.empty()) {
        state.statusMessage = L("Kamera-Lesezeichen brauchen eine geöffnete Karte.",
                                "Camera bookmarks need an open map.");
        return;
    }
    auto& b = state.cameraBookmarks.slots[static_cast<std::size_t>(slot)];
    b.valid = true;
    b.targetX = state.camera.TargetX(); b.targetY = state.camera.TargetY(); b.targetZ = state.camera.TargetZ();
    b.yaw = state.camera.Yaw(); b.pitch = state.camera.Pitch(); b.distance = state.camera.Distance();
    SaveCameraBookmarks(state);
    state.statusMessage = std::string(L("Kamera-Lesezeichen ", "Camera bookmark ")) + std::to_string(slot) +
                          L(" gesetzt.", " set.");
}
bool JumpToCameraBookmark(EditorState& state, int slot) {
    if (slot < 0 || slot >= static_cast<int>(core::level::CameraBookmarkSet::kSlots)) return false;
    EnsureCameraBookmarksLoaded(state);
    const auto& b = state.cameraBookmarks.slots[static_cast<std::size_t>(slot)];
    if (!b.valid) return false;
    state.camera.SetTarget(b.targetX, b.targetY, b.targetZ);
    state.camera.SetDistance(b.distance);
    state.camera.SetOrientation(b.yaw, b.pitch);
    LeaveAxisView(state);
    state.statusMessage = std::string(L("Kamera-Lesezeichen ", "Camera bookmark ")) + std::to_string(slot);
    return true;
}
const char* ViewportPresetName(int preset) {
    switch (preset) {
        case 1: return L("Oben", "Top"); // Norden oben
        case 2: return L("Von Süden", "From south");
        case 3: return L("Von Norden", "From north");
        case 4: return L("Von Westen", "From west");
        case 5: return L("Von Osten", "From east");
        default: return L("Perspektive", "Perspective");
    }
}
// Achsen-Ansichten (orthografisch über OrbitCamera::ProjectionMatrix, die alle Renderer,
// Picking, Gizmo und Overlays gemeinsam nutzen).
// Yaw 0 = Kamera südlich des Ziels mit Blick nach Norden (+Z in Fiesta-Weltkoordinaten).
void ApplyViewportPreset(EditorState& state, int preset) {
    constexpr float kPi = 3.14159265f;
    state.viewportViewPreset = std::clamp(preset, 0, 5);
    // Wie in Unreal: Achsenansichten sind orthografisch, die Perspektive bleibt perspektivisch.
    state.camera.SetOrthographic(state.viewportViewPreset != 0);
    switch (state.viewportViewPreset) {
        case 1: state.camera.SetOrientation(0.0f, 1.5f); break;
        case 2: state.camera.SetOrientation(0.0f, 0.06f); break;
        case 3: state.camera.SetOrientation(kPi, 0.06f); break;
        case 4: state.camera.SetOrientation(-kPi * 0.5f, 0.06f); break;
        case 5: state.camera.SetOrientation(kPi * 0.5f, 0.06f); break;
        default: state.camera.SetOrientation(0.7f, 0.6f); break;
    }
}
   // nur Darstellung des Spielermarkers

float PlaytestGroundHeight(const EditorState& state, float x, float z, float fallback) {
    return SurfaceHeightAt(state, x, z, fallback);
}
void StartPlaytest(EditorState& state) {
    if (state.playtestActive) return;
    if (!state.hasLegacyIniMeta && state.legacySaveStem[0] == '\0') {
        state.statusMessage = L("Spieltest: zuerst eine Karte öffnen.", "Playtest: open a map first.");
        return;
    }
    state.playtestSavedCamera = state.camera;
    state.marqueeActive = false;
    state.cameraLooking = false;

    float x = state.camera.TargetX(), z = state.camera.TargetZ();
    state.playtestSpawnSource = L("Kameraziel", "camera target");
    EnsurePortalDataLoaded(state);
    float regenX = 0.0f, regenY = 0.0f;
    if (FindMapRegen(state, regenX, regenY)) {
        x = regenX; z = regenY;
        state.playtestSpawnSource = "MapInfo.shn RegenX/RegenY";
    }

    state.playtestWalkGridUsable = core::level::WalkableFraction(state.walkGrid, 11) > 0.0005;
    state.playtestConfig.collideWithWalkGrid = state.playtestWalkGridUsable;
    if (state.playtestWalkGridUsable) {
        if (const auto spot = core::level::FindNearestWalkable(state.walkGrid, x, z, 480)) {
            x = (*spot)[0]; z = (*spot)[1];
        }
    }
    state.playtestPawn = {};
    state.playtestPawn.x = x;
    state.playtestPawn.z = z;
    state.playtestPawn.y = PlaytestGroundHeight(state, x, z, state.camera.TargetY());
    state.playtestPawn.facingYaw = -state.camera.Yaw();
    state.playtestActive = true;

    state.camera.SetTarget(x, state.playtestPawn.y + kPlaytestEyeHeight, z);
    state.camera.SetDistance(420.0f);
    state.camera.SetOrientation(state.camera.Yaw(), 0.38f);

    std::string msg = std::string(L("Spieltest gestartet (Spawn: ", "Playtest started (spawn: ")) +
                      state.playtestSpawnSource + "). " +
                      L("WASD laufen · Shift gehen · RMB drehen · Esc beendet.",
                        "WASD run · Shift walk · RMB turn · Esc ends.");
    if (!state.playtestWalkGridUsable)
        msg += L(" Warnung: kein begehbares SHBD-Gitter geladen - Kollision deaktiviert.",
                 " Warning: no walkable SHBD grid loaded - collision disabled.");
    state.statusMessage = msg;
}
void StopPlaytest(EditorState& state) {
    if (!state.playtestActive) return;
    state.playtestActive = false;
    state.camera = state.playtestSavedCamera;
    char buf[160];
    std::snprintf(buf, sizeof(buf), L("Spieltest beendet · %.0f Einheiten gelaufen.",
                                      "Playtest ended · %.0f units travelled."),
                  state.playtestPawn.distanceTravelled);
    state.statusMessage = buf;
}
void TogglePlaytest(EditorState& state) {
    if (state.playtestActive) StopPlaytest(state);
    else StartPlaytest(state);
}
// Ziffern 0..9 springen zu Kamera-Lesezeichen, sofern die Ziffer nicht an ein anderes Kürzel
// gebunden ist. Unter Unreal-Navigation gelten die Gizmo-Ziffern 1/2/3 nicht (dort W/E/R).
bool GizmoShortcutPressed(const EditorState& state, const EditorState::ShortcutBinding& binding) {
    const bool plainDigit = binding.key >= ImGuiKey_0 && binding.key <= ImGuiKey_9 &&
                            !binding.ctrl && !binding.shift && !binding.alt;
    if (state.unrealNavigation && plainDigit) return false;
    return ShortcutPressed(binding);
}
bool DigitReservedByShortcut(EditorState& state, ImGuiKey key) {
    for (const auto& [name, binding] : ShortcutSettings(state)) {
        if (binding->key != key || binding->ctrl || binding->shift || binding->alt) continue;
        const bool gizmo = binding == &state.shortcutGizmoMove || binding == &state.shortcutGizmoRotate ||
                           binding == &state.shortcutGizmoScale;
        if (gizmo && state.unrealNavigation) continue;
        return true;
    }
    return false;
}
void SetGizmoOperation(EditorState& state, int op) {
    state.objectGizmoOperation = op;
    state.objectGizmoMatrixValid = false;
}
// Globale Tastenkuerzel: F1 = Handbuch, Strg+Z / Strg+Y (Strg+Shift+Z) = Rueckgaengig/Wiederholen im aktiven Karten-Werkzeug.
void HandleGlobalShortcuts(EditorState& state) {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) state.manualOpen = !state.manualOpen;
    if (ShortcutPressed(state.shortcutPalette)) {
        state.commandPaletteOpen = true;
        state.commandPaletteSelection = 0;
    }

    if (state.playtestActive && state.screen != AppScreen::MapEditorWorkspace) StopPlaytest(state);
    // Daten-Editoren (SHN, Quest, Skill, Drops ...): ein gemeinsamer Verlauf für Strg+Z/Strg+Y.
    if (!io.WantTextInput && state.screen == AppScreen::ShnEditor &&
        !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
        if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) DataUndo(state);
        else if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y, false) ||
                                (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))) DataRedo(state);
    }
    if (io.WantTextInput || state.screen != AppScreen::MapEditorWorkspace) return;

    // Spieltest: Esc oder Alt+P beendet; währenddessen keine Editor-Kürzel.
    if (state.playtestActive) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
            (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_P, false)))
            StopPlaytest(state);
        return;
    }
    if (io.KeyAlt && !io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
        StartPlaytest(state);
        return;
    }

    const bool noMods = !io.KeyCtrl && !io.KeyShift && !io.KeyAlt;
    const bool flying = state.cameraLooking; // rechte Maustaste: WASD gehört der Kamera
    if (!flying && noMods && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
        state.gameView = !state.gameView;
        state.statusMessage = state.gameView
            ? L("Spielansicht: Editor-Helfer ausgeblendet (G)", "Game view: editor helpers hidden (G)")
            : L("Spielansicht aus", "Game view off");
    }
    for (int digit = 0; digit <= 9; ++digit) {
        const auto key = static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_0) + digit);
        if (!ImGui::IsKeyPressed(key, false)) continue;
        if (io.KeyCtrl && !io.KeyShift && !io.KeyAlt) SetCameraBookmark(state, digit);
        else if (noMods && !flying && !DigitReservedByShortcut(state, key) && !JumpToCameraBookmark(state, digit))
            state.statusMessage = std::string(L("Kamera-Lesezeichen ", "Camera bookmark ")) + std::to_string(digit) +
                                  L(" ist leer (Strg+", " is empty (Ctrl+") + std::to_string(digit) +
                                  L(" setzt es).", " sets it).");
    }

    if (ShortcutPressed(state.shortcutSave) &&
        state.legacySaveDir[0] != '\0' && state.legacySaveStem[0] != '\0') {
        SaveLegacyMapProject(state);
        if (state.editMode == EditMode::Portals) {
            if (state.townPortalDirty) SaveTownPortalFiles(state);
            if (state.recallCoordDirty) SaveRecallCoordFile(state);
        }
    }

    if (ShortcutPressed(state.shortcutFocus))
        FocusCurrentSceneSelection(state);

    if (state.editMode == EditMode::ObjectPlacement) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) CopySelectedObjects(state);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) PasteObjectClipboard(state);
        if (ShortcutPressed(state.shortcutDuplicate)) DuplicateSelectedObjects(state);
        if (ShortcutPressed(state.shortcutGround)) GroundSelectedObjects(state);
        if (ShortcutPressed(state.shortcutDelete)) DeleteSelectedObjects(state);

        if (GizmoShortcutPressed(state, state.shortcutGizmoMove)) SetGizmoOperation(state, 0);
        if (GizmoShortcutPressed(state, state.shortcutGizmoRotate)) SetGizmoOperation(state, 1);
        if (GizmoShortcutPressed(state, state.shortcutGizmoScale)) SetGizmoOperation(state, 2);
        // Unreal: W/E/R wählen das Gizmo, Leertaste schaltet weiter (nicht während des Fliegens).
        if (state.unrealNavigation && !flying && noMods) {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) SetGizmoOperation(state, 0);
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) SetGizmoOperation(state, 1);
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) SetGizmoOperation(state, 2);
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
                SetGizmoOperation(state, (std::max(0, state.objectGizmoOperation) + 1) % 3);
        }
        // Editor-Sichtbarkeit wie in Unreal: H / Shift+H / Strg+H.
        if (ImGui::IsKeyPressed(ImGuiKey_H, false)) {
            if (io.KeyCtrl && !io.KeyShift && !io.KeyAlt) ShowAllObjects(state);
            else if (io.KeyShift && !io.KeyCtrl && !io.KeyAlt) HideUnselectedObjects(state);
            else if (noMods) HideSelectedObjects(state);
        }
        if (noMods && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !state.selectedObjects.empty() &&
            !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
            ClearObjectSelection(state);
    }

    const bool undo = io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false);
    const bool redo = io.KeyCtrl &&
        (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)));
    if (!undo && !redo) return;

    if (undo) UndoMapEdit(state);
    else RedoMapEdit(state);
}

} // namespace nge
