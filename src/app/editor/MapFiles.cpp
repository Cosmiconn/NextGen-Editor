// MapFiles.cpp
// Karte öffnen, Autosave, Vorschautexturen, Dateidialoge, Karten suchen, Datei-Werkzeuge.

#include "EditorShared.hpp"

namespace nge {

// Verteilt ein frisch geöffnetes LegacyMapProject auf die einzelnen Editor-Zustandsfelder -
// setzt außerdem alle Undo-Stacks/Auswahl/Dirty-Flags zurück (neue Karte, alte Historie ungültig).
void ApplyProjectToState(EditorState& state, core::legacy::LegacyMapProject&& project, const std::filesystem::path& mapDir) {
    state.sceneEnvironmentFromShmd = project.hasObjects && !project.objects.originalText.empty();
    state.environmentAtLoad = project.objects.environment;
    const bool hadHeightmap = state.mapHasHeightmap;
    const bool hasHeightmap = project.hasHeightmap &&
        project.heightmap.Width() > 0 && project.heightmap.Height() > 0;
    state.preservedMapFiles = std::move(project.preservedFiles);
    state.heightmap = std::move(project.heightmap);
    state.heightmapAtLoad = state.heightmap;
    state.walkProposal.reset();
    state.walkGenStatus.clear();
    state.mapHasHeightmap = hasHeightmap;
    state.standaloneNifMap = false;
    state.htdHeader = project.htdHeader;
    state.htdTrailingBytes = project.htdTrailingBytes;
    ClearMapHistory(state);
    state.meshDirty = true;

    // Terrain maps are framed from the real height grid. Terrain-less SHMD/NIF maps are
    // framed later from actual NIF mesh bounds; never invent a 50x50 pseudo-heightmap.
    if (hasHeightmap) {
        // NIF-only maps force terrain visibility off. When returning to a real terrain map,
        // restore it once; terrain-to-terrain switches still preserve a user's manual hide.
        if (!hadHeightmap) state.showTerrain = true;
        const float spanX = static_cast<float>(state.heightmap.Width() - 1) * state.heightmap.BlockWidth();
        const float spanZ = static_cast<float>(state.heightmap.Height() - 1) * state.heightmap.BlockHeight();
        const auto [lo, hi] = state.heightmap.MinMax();
        state.camera.SetTarget(spanX * 0.5f, (lo + hi) * 0.5f, spanZ * 0.5f);
        state.camera.Zoom(std::max(spanX, spanZ) * 0.9f - state.camera.Distance());
        state.focusLoadedNifScenePending = false;
    } else {
        state.focusLoadedNifScenePending = true;
        state.showTerrain = false;
        if (state.editMode == EditMode::Heightmap || state.editMode == EditMode::TexturePaint ||
            state.editMode == EditMode::BlockWalk)
            state.editMode = EditMode::ObjectPlacement;
    }
    state.view2dZoom = 1.0f;
    state.view2dCenterU = state.view2dCenterV = 0.5f;

    state.textureStack = std::move(project.textureStack);
    state.selectedLayer = state.textureStack.LayerCount() > 0 ? 0 : -1;
    state.layerPreviewDirty = true;
    state.renderer.LoadTerrainTextures(state.textureStack, mapDir);

    state.walkGrid = std::move(project.walkGrid);
    state.walkGridAtLoad = state.walkGrid;
    state.modelBlockStatsKey.clear();
    state.shbdHeader = project.shbdHeader;
    state.walkPreviewDirty = true;

    state.legacyShmdSourceCount = static_cast<std::int32_t>(project.objects.Count());
    state.legacySourceStem = project.sourceStem;
    state.legacySourceFileNames = project.sourceFileNames;
    state.placementSet = std::move(project.objects);
    state.placementAtLoad = state.placementSet;
    state.selectedObject = kNoObjectSelection;
    state.selectedObjects.clear();
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
    state.objectEditorHidden.assign(state.placementSet.Count(), 0);
    state.objectEditorLocked.assign(state.placementSet.Count(), 0);
    state.objectEditorLabels.assign(state.placementSet.Count(), {});
    state.objectEditorGroups.assign(state.placementSet.Count(), {});
    state.shmdEditorHiddenKeys.clear();
    state.shmdEditorLockedKeys.clear();
    state.shmdEditorLabels.clear();
    state.shmdEditorGroups.clear();
    // Versucht, für alle Objekte echte .nif-Meshes zu laden (aktuell nur untexturierte Meshes
    // erfolgreich, siehe docs/MAP_FORMAT.md) - für den Rest bleibt der Platzhalter-Marker.
    state.nifMeshRenderer.LoadModelsForSet(state.placementSet, mapDir);
    RebuildShmdCategoryRenderSet(state, mapDir);

    state.legacySpatialIndex = std::move(project.spatialIndex);
    state.hasLegacySpatialIndex = project.hasSpatialIndex;
    state.legacyZoneMetadata = std::move(project.zone);
    state.selectedZone = 0;
    state.hasLegacyZoneMetadata = project.hasZone;
    std::snprintf(state.zoneNameBuf, sizeof(state.zoneNameBuf), "%s", state.legacyZoneMetadata.name.c_str());

    state.legacyIniMeta = std::move(project.ini);
    state.hasLegacyIniMeta = true;
    state.mapDirty = false;
}
std::filesystem::path LegacyMapWorkingIniPath(
    const EditorState& state,
    const std::filesystem::path& iniPath,
    bool preferProjectOutput) {

    if (!preferProjectOutput || state.project.projectFolder[0] == '\0' ||
        IsProjectSidePath(state.project, core::ProjectOutputSide::Client, iniPath)) {
        return iniPath;
    }
    return PreferProjectOverride(state.project, core::ProjectOutputSide::Client, iniPath);
}
// Kartenlokale Render-Daten des Clients: Vertex-Color-Bitmap (#VerTexColorTexture der .ini) und
// <Karte>.conf. Fehlende Dateien sind kein Fehler (viele Karten haben keine .conf), werden aber
// im Hinweis vermerkt. Nur lesend - der Editor schreibt diese Dateien nicht.
void LoadMapRenderData(EditorState& state, const std::filesystem::path& mapDir, const std::string& stem) {
    state.mapRenderConfig = {};
    state.mapRenderConfigPath.clear();
    state.mapVertexColorPath.clear();
    state.mapRenderDataNote.clear();
    state.renderer.SetVertexColorMap(nullptr);

    if (state.hasLegacyIniMeta && !state.legacyIniMeta.vertexColorTexture.empty()) {
        auto resolved = core::legacy::ResolveLegacyAssetPath(mapDir, state.legacyIniMeta.vertexColorTexture);
        if (!resolved) {
            const auto name = core::legacy::LegacyPathToNative(state.legacyIniMeta.vertexColorTexture).filename();
            if (auto sibling = core::legacy::ResolveCaseInsensitivePath(mapDir, name)) resolved = sibling;
        }
        if (resolved) {
            if (auto image = core::legacy::ReadBmpRgb(*resolved)) {
                state.renderer.SetVertexColorMap(&*image);
                state.mapVertexColorPath = resolved->string();
            } else {
                state.mapRenderDataNote += "Vertex-Color-Bitmap nicht lesbar: " + image.error() + "\n";
            }
        } else {
            state.mapRenderDataNote += "Vertex-Color-Bitmap nicht gefunden: " + state.legacyIniMeta.vertexColorTexture + "\n";
        }
    }
    if (!stem.empty()) {
        if (auto confPath = core::legacy::ResolveCaseInsensitivePath(mapDir, stem + ".conf")) {
            if (auto conf = core::legacy::LoadMapRenderConfig(*confPath)) {
                state.mapRenderConfig = std::move(*conf);
                state.mapRenderConfigPath = confPath->string();
                for (const auto& w : state.mapRenderConfig.warnings) state.mapRenderDataNote += stem + ".conf: " + w + "\n";
            }
        }
    }
    state.meshDirty = true; // Vertexfarben liegen im Terrain-Vertexpuffer
}
bool OpenLegacyMapIntoState(EditorState& state, const std::filesystem::path& iniPath,
                            bool preferProjectOutput) {
    const auto workingIniPath = LegacyMapWorkingIniPath(state, iniPath, preferProjectOutput);
    core::legacy::LegacyMapOpenReport report;
    auto result = core::legacy::OpenLegacyMap(workingIniPath, &report);
    if (!result) {
        state.statusMessage = "Karte öffnen fehlgeschlagen: " + result.error();
        return false;
    }

    // Keep the selected source path as stable identity. The actual working copy may live
    // below <Project>/Client/resmap/... and must win for reads once it exists.
    std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s", iniPath.string().c_str());
    ApplyProjectToState(state, std::move(*result), workingIniPath.parent_path());
    LoadMapRenderData(state, workingIniPath.parent_path(), iniPath.stem().string());
    if (preferProjectOutput && state.project.projectFolder[0] != '\0') {
        std::filesystem::path outputDir;
        if (IsProjectSidePath(state.project, core::ProjectOutputSide::Client, iniPath)) {
            outputDir = iniPath.parent_path();
        } else if (auto target = ProjectPathForSource(
                       state.project, core::ProjectOutputSide::Client, iniPath)) {
            outputDir = target->parent_path();
        }
        std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", outputDir.string().c_str());
    } else {
        state.legacySaveDir[0] = '\0'; // source trees are read-only; no project means no save target
    }
    std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", iniPath.stem().string().c_str());

    TouchRecentMap(state, iniPath.string());
    CheckAutosaveRecovery(state);
    state.statusMessage = "Karte geöffnet" +
                          std::string(workingIniPath != iniPath ? " (Projekt-Override)" : "") +
                          " (" + std::to_string(report.issues.size()) + " Hinweis(e)) - " +
                          std::to_string(state.placementSet.Count() + state.shmdCategoryRenderSet.Count()) +
                          " Objekte, " + std::to_string(state.textureStack.LayerCount()) + " Textur-Layer.";
    for (const auto& issue : report.issues) state.statusMessage += "\n- " + issue;
    return true;
}
bool OpenStandaloneNifMapIntoState(EditorState& state, const std::filesystem::path& nifPath) {
    const auto workingPath = IsProjectSidePath(state.project, core::ProjectOutputSide::Client, nifPath)
        ? nifPath
        : PreferProjectOverride(state.project, core::ProjectOutputSide::Client, nifPath);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(workingPath, ec)) {
        state.statusMessage = "NIF-Karte nicht gefunden: " + nifPath.string();
        return false;
    }
    auto parsed = core::LoadNifMesh(workingPath);
    if (!parsed) {
        state.statusMessage = "NIF-Karte konnte nicht gelesen werden: " + parsed.error();
        return false;
    }

    core::legacy::LegacyMapProject project;
    core::ObjectCategoryList ground;
    ground.name = "GroundObject";
    ground.modelPaths.push_back(workingPath.filename().string());
    project.objects.categories.push_back(std::move(ground));
    project.hasObjects = true;

    // Keep the immutable source identity while rendering from the project working copy.
    std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s", nifPath.string().c_str());
    ApplyProjectToState(state, std::move(project), workingPath.parent_path());
    state.standaloneNifMap = true;
    state.hasLegacyIniMeta = false;
    LoadMapRenderData(state, workingPath.parent_path(), nifPath.stem().string());
    state.legacySaveDir[0] = '\0';
    std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", nifPath.stem().string().c_str());
    state.editMode = EditMode::ObjectPlacement;
    state.objectPlaceMode = 0;
    state.showTerrain = false;
    state.mapDirty = false;
    TouchRecentMap(state, nifPath.string());
    state.statusMessage = "NIF-Karte geöffnet (ohne HTD/HTDG)" +
                          std::string(workingPath != nifPath ? " (Projekt-Override): " : ": ") +
                          nifPath.string() +
                          "\nTerrain-/Textur-/Walk-Werkzeuge bleiben deaktiviert; die NIF-Szene wird direkt gerendert.";
    return true;
}
bool OpenMapPathIntoState(EditorState& state, const std::filesystem::path& path,
                          bool preferProjectOutput) {
    // Ein laufender Spieltest/Rahmen gehört zur bisherigen Karte; die Editor-Kamera wird von
    // der neuen Karte ohnehin neu ausgerichtet.
    state.playtestActive = false;
    state.marqueeActive = false;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (ext == ".nif") return OpenStandaloneNifMapIntoState(state, path);
    return OpenLegacyMapIntoState(state, path, preferProjectOutput);
}
// ============================================================================
// Automatisches Speichern: Die offene Karte wird bei Änderungen alle N Minuten mit SaveLegacyMap
// nach <Projekt>/Autosave/Client/resmap/field/<Karte>/ geschrieben (Hintergrund-Thread auf einer
// Kopie). Beim nächsten Öffnen der Karte bietet der Editor die Wiederherstellung an; normales
// Speichern ins Projekt oder "Verwerfen" löscht die Sicherung.
// ============================================================================
bool MapAutosaveSupported(const EditorState& state) {
    return state.project.projectFolder[0] != '\0' && state.legacySaveStem[0] != '\0' && !state.standaloneNifMap &&
           state.hasLegacyIniMeta;
}
void CheckAutosaveRecovery(EditorState& state) {
    state.autosaveRecoveryPrompt = false;
    state.autosaveLastTime = ImGui::GetTime();
    state.autosaveSavedRevision = state.mapEditRevision;
    state.autosaveWasDirty = false;
    if (!MapAutosaveSupported(state)) return;
    const auto dir = core::AutosaveMapDir(state.project.projectFolder, state.legacySaveStem);
    if (auto meta = core::ReadAutosaveMeta(dir)) {
        state.autosaveRecoveryMeta = *meta;
        state.autosaveRecoveryPrompt = true;
    }
}
// Startet eine Sicherung, falls keine läuft. Kopiert den Zustand sofort; geschrieben wird im Hintergrund.
bool StartMapAutosave(EditorState& state) {
    if (!MapAutosaveSupported(state)) return false;
    if (state.autosaveTask.valid()) return false;
    auto project = std::make_shared<core::legacy::LegacyMapProject>(BuildProjectFromState(state));
    const auto dir = core::AutosaveMapDir(state.project.projectFolder, state.legacySaveStem);
    core::AutosaveMeta meta;
    meta.sourceIni = state.legacyMapIniPath;
    meta.mapStem = state.legacySaveStem;
    meta.savedAtUnix = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count();
    state.autosaveTask = std::async(std::launch::async, [project, dir, meta]() -> std::string {
        // Alte Sicherung erst nach erfolgreichem Schreiben ersetzen: in einen Nachbarordner schreiben.
        const auto tmp = dir.parent_path() / (dir.filename().string() + ".tmp");
        std::error_code ec;
        std::filesystem::remove_all(tmp, ec);
        if (auto r = core::legacy::SaveLegacyMap(*project, tmp, meta.mapStem); !r) return r.error();
        if (auto r = core::WriteAutosaveMeta(tmp, meta); !r) return r.error();
        std::filesystem::remove_all(dir, ec);
        std::filesystem::rename(tmp, dir, ec);
        if (ec) return "Autosave-Ordner nicht ersetzbar: " + ec.message();
        return {};
    });
    state.autosaveLastTime = ImGui::GetTime();
    state.autosaveSavedRevision = state.mapEditRevision;
    return true;
}
void FinishMapAutosave(EditorState& state, bool wait) {
    if (!state.autosaveTask.valid()) return;
    if (!wait && state.autosaveTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    const std::string error = state.autosaveTask.get();
    char when[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(when, sizeof(when), "%H:%M:%S", std::localtime(&now));
    state.autosaveStatus = error.empty() ? std::string(L("Automatisch gesichert um ", "Autosaved at ")) + when
                                         : std::string(L("Automatisches Speichern fehlgeschlagen: ", "Autosave failed: ")) + error;
    if (!error.empty()) state.statusMessage = state.autosaveStatus;
}
void TickAutosave(EditorState& state) {
    FinishMapAutosave(state, false);
    if (!MapAutosaveSupported(state)) return;
    const double now = ImGui::GetTime();
    // Erste Änderung nach dem Öffnen/Sichern startet die Wartezeit.
    if (state.mapDirty && !state.autosaveWasDirty) state.autosaveLastTime = now;
    state.autosaveWasDirty = state.mapDirty;
    const bool changedSinceAutosave = state.mapDirty && state.mapEditRevision != state.autosaveSavedRevision;
    if (!core::AutosaveDue(state.autosaveEnabled, changedSinceAutosave, now, state.autosaveLastTime,
                           static_cast<double>(state.autosaveMinutes) * 60.0))
        return;
    // Nicht mitten in einem Pinselstrich oder Ziehvorgang sichern.
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) || state.objectDragActive) return;
    StartMapAutosave(state);
}
// Übernimmt die gesicherten Kartenmodule in die frisch geöffnete Karte. Texturen, Render-Daten und
// Pfade kommen aus der normal geöffneten Karte; aus der Sicherung stammen Heightmap, Layer-Masken,
// Block&Walk, Objekte und die dazu passende IDM-Zuordnung.
bool RestoreMapAutosave(EditorState& state) {
    FinishMapAutosave(state, true);
    const auto dir = core::AutosaveMapDir(state.project.projectFolder, state.legacySaveStem);
    const auto ini = core::legacy::FindSiblingFileByStem(dir, state.legacySaveStem, ".ini");
    if (!ini) {
        state.statusMessage = L("Sicherung unvollständig (keine INI).", "Autosave incomplete (no INI).");
        return false;
    }
    auto loaded = core::legacy::OpenLegacyMap(*ini);
    if (!loaded) {
        state.statusMessage = std::string(L("Sicherung nicht lesbar: ", "Autosave not readable: ")) + loaded.error();
        return false;
    }
    auto& saved = *loaded;
    std::size_t restoredParts = 0;
    if (saved.hasHeightmap && saved.heightmap.Width() == state.heightmap.Width() && saved.heightmap.Height() == state.heightmap.Height()) {
        state.heightmap = std::move(saved.heightmap);
        state.meshDirty = true;
        ++restoredParts;
    }
    if (saved.textureStack.LayerCount() == state.textureStack.LayerCount()) {
        for (std::size_t l = 0; l < state.textureStack.LayerCount(); ++l) {
            auto& dst = state.textureStack.Layer(l);
            const auto& src = saved.textureStack.Layer(l);
            if (dst.name != src.name || dst.blend.Width() != src.blend.Width() || dst.blend.Height() != src.blend.Height()) continue;
            dst.blend = src.blend;
        }
        state.layerPreviewDirty = true;
        ++restoredParts;
    }
    if (saved.hasWalkGrid && saved.walkGrid.Width() == state.walkGrid.Width() && saved.walkGrid.Height() == state.walkGrid.Height()) {
        state.walkGrid = std::move(saved.walkGrid);
        state.walkPreviewDirty = true;
        state.modelBlockStatsKey.clear();
        ++restoredParts;
    }
    if (saved.hasObjects) {
        state.placementSet = std::move(saved.objects);
        // Die Sicherung ist eine eigene SHMD/IDM-Generation: ihre Reihenfolge ist die neue Quelle.
        state.legacyShmdSourceCount = static_cast<std::int32_t>(state.placementSet.Count());
        state.legacySpatialIndex = std::move(saved.spatialIndex);
        state.hasLegacySpatialIndex = saved.hasSpatialIndex;
        state.selectedObjects.clear();
        state.selectedObject = kNoObjectSelection;
        state.objectEditorHidden.assign(state.placementSet.Count(), 0);
        state.objectEditorLocked.assign(state.placementSet.Count(), 0);
        state.objectEditorLabels.assign(state.placementSet.Count(), {});
        state.objectEditorGroups.assign(state.placementSet.Count(), {});
        state.nifMeshRenderer.LoadModelsForSet(state.placementSet, CurrentObjectAssetMapDir(state));
        ++restoredParts;
    }
    ClearMapHistory(state);
    state.mapDirty = true;
    state.autosaveRecoveryPrompt = false;
    state.statusMessage = std::string(L("Sicherung wiederhergestellt (", "Autosave restored (")) + std::to_string(restoredParts) +
                          L(" Module) - mit Speichern ins Projekt übernehmen.", " modules) - save to write it to the project.");
    return true;
}
void DrawAutosaveRecoveryPopup(EditorState& state) {
    if (state.autosaveRecoveryPrompt && !ImGui::IsPopupOpen(L("Sicherung gefunden###autosaveRecovery", "Autosave found###autosaveRecovery")))
        ImGui::OpenPopup(L("Sicherung gefunden###autosaveRecovery", "Autosave found###autosaveRecovery"));
    if (!ImGui::BeginPopupModal(L("Sicherung gefunden###autosaveRecovery", "Autosave found###autosaveRecovery"), nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize))
        return;
    char when[64] = "?";
    const std::time_t t = static_cast<std::time_t>(state.autosaveRecoveryMeta.savedAtUnix);
    if (const std::tm* tm = std::localtime(&t)) std::strftime(when, sizeof(when), "%d.%m.%Y %H:%M:%S", tm);
    ImGui::Text(L("Für die Karte %s gibt es eine automatische Sicherung vom %s,", "There is an autosave of map %s from %s"),
                state.legacySaveStem, when);
    ImGui::TextUnformatted(L("die nicht ins Projekt gespeichert wurde (z.B. nach einem Absturz).",
                             "that was not saved to the project (e.g. after a crash)."));
    ImGui::TextDisabled("%s", core::AutosaveMapDir(state.project.projectFolder, state.legacySaveStem).string().c_str());
    ImGui::Spacing();
    if (UI::Button(L("Wiederherstellen##autosave", "Restore##autosave"))) {
        RestoreMapAutosave(state);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (UI::Button(L("Verwerfen##autosave", "Discard##autosave"))) {
        core::RemoveAutosave(state.project.projectFolder, state.legacySaveStem);
        state.autosaveRecoveryPrompt = false;
        state.statusMessage = L("Sicherung verworfen.", "Autosave discarded.");
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (UI::Button(L("Später##autosave", "Later##autosave"))) {
        state.autosaveRecoveryPrompt = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
// Leitet die Block&Walk-Gitterauflösung aus der Heightmap ab: Breite = QuadsBreite/2, Höhe =
// QuadsBreite*8 - KEIN Quadrat. Korrigiert anhand von vier echten Referenzkarten: die vorherige
// Annahme ("quadratisch, max(QuadsBreite,QuadsHöhe)*2") bestätigte nur die GESAMT-Byte-Anzahl,
// nicht die tatsächliche Breite/Höhe-Aufteilung - dadurch wurde z.B. bei RouVal01 (echte Daten:
// 256x4096) fälschlich als 1024x1024 interpretiert, was beim Anzeigen sichtbar als "4x
// dieselbe Silhouette nebeneinander" auffiel (vom Nutzer per Screenshot entdeckt). Die
// richtige Breite ergibt bei allen vier Karten ein einzelnes, nicht wiederholtes Bild; das
// zweite Header-Feld der echten .shbd-Dateien enthält die Höhe sogar direkt (siehe
// docs/MAP_FORMAT.md). Achtung: nutzt nur QuadsBreite (X-Achse), QuadsHöhe fließt nicht ein -
// bei nicht-quadratischen Karten (z.B. Adl) durch die echten Header-Werte bestätigt, aber die
// genaue X/Z-Achsenzuordnung bleibt ungeklärt (siehe Dokumentation).
void SyncWalkGridSize(EditorState& state) {
    const std::uint32_t quadsW = state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 0;
    const std::uint32_t gridWidth = quadsW / 2;
    const std::uint32_t gridHeight = quadsW * 8;
    state.walkGrid.Resize(gridWidth, gridHeight, -1);
    ClearMapHistory(state);
    state.walkPreviewDirty = true;
}
// Erzeugt/aktualisiert die Graustufen-Vorschautextur aus den aktuellen Höhendaten.
void UpdatePreviewTexture(EditorState& state) {
    const auto& hm = state.heightmap;
    const auto [lo, hi] = hm.MinMax();
    const float range = (hi - lo) > 0.0001f ? (hi - lo) : 1.0f;

    std::vector<unsigned char> pixels(static_cast<std::size_t>(hm.Width()) * hm.Height());
    const auto data = hm.Data();
    for (std::size_t i = 0; i < data.size(); ++i) {
        const float t = std::clamp((data[i] - lo) / range, 0.0f, 1.0f);
        pixels[i] = static_cast<unsigned char>(t * 255.0f);
    }

    if (state.previewTex == 0) {
        glGenTextures(1, &state.previewTex);
    }
    glBindTexture(GL_TEXTURE_2D, state.previewTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Ohne Swizzle liest OpenGL bei GL_R8-Einkanaltexturen G/B als 0 -> erscheint rot statt grau.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(hm.Width()), static_cast<int>(hm.Height()),
                 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
// Erzeugt/aktualisiert die Graustufen-Vorschautextur für den aktuell ausgewählten Textur-Layer.
void UpdateLayerPreviewTexture(EditorState& state) {
    if (state.selectedLayer < 0 || static_cast<std::size_t>(state.selectedLayer) >= state.textureStack.LayerCount()) {
        return;
    }
    const auto& blend = state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer)).blend;
    if (blend.Width() == 0 || blend.Height() == 0) return;

    std::vector<unsigned char> pixels(static_cast<std::size_t>(blend.Width()) * blend.Height());
    const auto data = blend.Data();
    for (std::size_t i = 0; i < data.size(); ++i) {
        pixels[i] = static_cast<unsigned char>(std::clamp(data[i], 0.0f, 1.0f) * 255.0f);
    }

    if (state.layerPreviewTex == 0) {
        glGenTextures(1, &state.layerPreviewTex);
    }
    glBindTexture(GL_TEXTURE_2D, state.layerPreviewTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Ohne Swizzle liest OpenGL bei GL_R8-Einkanaltexturen G/B als 0 -> erscheint rot statt grau.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(blend.Width()), static_cast<int>(blend.Height()),
                 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
// Vorschau des Block&Walk-Gitters auf ZELL-Ebene (CHANGELOG [0.44.29]): eine Textur-Zeile je Gitter-
// Zeile, ein Pixel je Zelle (6.25 Welteinheiten), 255 = blockiert, 0 = begehbar. Fruehere Fassung
// stellte die 16-Bit-Woerter als Pixel dar (16fach zu grob) und streckte sie auf die Kartenform.
void UpdateWalkPreviewTexture(EditorState& state) {
    const auto& grid = state.walkGrid;
    if (grid.Width() == 0 || grid.Height() == 0) return;
    const std::uint32_t cols = grid.Cols(), rows = grid.Rows();

    std::vector<unsigned char> pixels(static_cast<std::size_t>(cols) * rows);
    for (std::uint32_t z = 0; z < rows; ++z) {
        unsigned char* dst = &pixels[static_cast<std::size_t>(z) * cols];
        for (std::uint32_t wx = 0; wx < grid.Width(); ++wx) {
            const auto word = static_cast<std::uint16_t>(grid.At(wx, z));
            for (std::uint32_t b = 0; b < 16; ++b) dst[wx * 16 + b] = ((word >> b) & 1u) ? 255 : 0;
        }
    }

    if (state.walkPreviewTex == 0) {
        glGenTextures(1, &state.walkPreviewTex);
    }
    glBindTexture(GL_TEXTURE_2D, state.walkPreviewTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Ohne Swizzle liest OpenGL bei GL_R8-Einkanaltexturen G/B als 0 -> erscheint rot statt grau.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(cols), static_cast<int>(rows), 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    state.walkPreviewCols = cols;
    state.walkPreviewRows = rows;
}
// Aktualisiert nur ein Zell-Rechteck der Vorschau (waehrend des Malens - die Gesamttextur kann bei
// grossen Karten ~58 MB haben, ein Vollupload pro Mausbewegung waere zu langsam).
void UpdateWalkPreviewRect(EditorState& state, std::uint32_t x0, std::uint32_t z0, std::uint32_t x1, std::uint32_t z1) {
    const auto& grid = state.walkGrid;
    if (state.walkPreviewTex == 0 || state.walkPreviewCols != grid.Cols() || state.walkPreviewRows != grid.Rows()) { state.walkPreviewDirty = true; return; }
    x1 = std::min(x1, grid.Cols() - 1);
    z1 = std::min(z1, grid.Rows() - 1);
    if (x0 > x1 || z0 > z1) return;
    const std::uint32_t w = x1 - x0 + 1, h = z1 - z0 + 1;
    std::vector<unsigned char> block(static_cast<std::size_t>(w) * h);
    for (std::uint32_t z = 0; z < h; ++z)
        for (std::uint32_t x = 0; x < w; ++x) block[static_cast<std::size_t>(z) * w + x] = grid.CellBlocked(x0 + x, z0 + z) ? 255 : 0;
    glBindTexture(GL_TEXTURE_2D, state.walkPreviewTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<int>(x0), static_cast<int>(z0), static_cast<int>(w), static_cast<int>(h), GL_RED, GL_UNSIGNED_BYTE, block.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
#ifdef _WIN32
// Nativer "Ordner wählen"-Dialog. Ältere, aber sehr stabile Win32-API (seit Windows 95
// verfügbar) - bewusst statt der moderneren IFileOpenDialog-COM-API gewählt, da die
// Ausdrucksweise hier deutlich weniger fehleranfällig ist (kein manuelles COM-Interface-
// Lifetime-Management nötig).
// Ordnerdialog über die moderne IFileOpenDialog-COM-API (seit Windows Vista) statt des
// alten SHBrowseForFolder - letzteres zeigte den Ordnerinhalt auf manchen Systemen nicht
// zuverlässig an und ließ sich schlecht navigieren (siehe Rückmeldung nach dem ersten
// echten Testlauf). IFileOpenDialog verhält sich wie ein normales Explorer-Fenster
// (Inhalt sichtbar, Adressleiste, Favoriten, Größe änderbar).
std::optional<std::string> BrowseForFolderWindows(const char* title) {
    // COM ggf. selbst initialisieren, aber nur wieder freigeben, wenn WIR es initialisiert
    // haben (S_OK) - war es schon initialisiert (S_FALSE) oder mit anderem Threading-Modell
    // aktiv (RPC_E_CHANGED_MODE, z.B. durch eine andere Bibliothek), NICHT per CoUninitialize
    // eingreifen, das würde fremden Zustand kaputt machen.
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool weOwnCom = SUCCEEDED(comInit);

    std::optional<std::string> result;
    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr) && dialog != nullptr) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

        wchar_t wtitle[256]{};
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, static_cast<int>(std::size(wtitle)));
        dialog->SetTitle(wtitle);

        const HWND owner = g_appWindowForDialogs != nullptr ? glfwGetWin32Window(g_appWindowForDialogs) : nullptr;
        hr = dialog->Show(owner);
        if (SUCCEEDED(hr)) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
                PWSTR pathW = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pathW)) && pathW != nullptr) {
                    char pathA[1024]{};
                    WideCharToMultiByte(CP_UTF8, 0, pathW, -1, pathA, static_cast<int>(std::size(pathA)), nullptr, nullptr);
                    result = std::string(pathA);
                    CoTaskMemFree(pathW);
                }
                item->Release();
            }
        }
        // hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) bei Abbruch durch den Nutzer - kein Fehler,
        // result bleibt einfach std::nullopt.
        dialog->Release();
    }

    if (weOwnCom) {
        CoUninitialize();
    }
    return result;
}
#endif

#ifdef _WIN32
std::optional<std::string> BrowseForShnFileWindows(const char* title, bool kfm) {
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool weOwnCom = SUCCEEDED(comInit);
    std::optional<std::string> result;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog))) && dialog) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
        COMDLG_FILTERSPEC filters[] = {{kfm ? L"Fiesta KFM" : L"Fiesta SHN", kfm ? L"*.kfm" : L"*.shn"}, {L"Alle Dateien", L"*.*"}};
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        wchar_t wtitle[256]{};
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, static_cast<int>(std::size(wtitle)));
        dialog->SetTitle(wtitle);
        const HWND owner = g_appWindowForDialogs ? glfwGetWin32Window(g_appWindowForDialogs) : nullptr;
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR pathW = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pathW)) && pathW) {
                    char pathA[4096]{};
                    WideCharToMultiByte(CP_UTF8, 0, pathW, -1, pathA, static_cast<int>(std::size(pathA)), nullptr, nullptr);
                    result = std::string(pathA);
                    CoTaskMemFree(pathW);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (weOwnCom) CoUninitialize();
    return result;
}
#endif

#ifdef _WIN32
std::optional<std::string> BrowseForInterfaceAssetWindows(const char* title, const std::string& extension) {
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool weOwnCom = SUCCEEDED(comInit);
    std::optional<std::string> result;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog))) && dialog) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);

        std::wstring extW;
        if (!extension.empty()) {
            wchar_t converted[32]{};
            MultiByteToWideChar(CP_UTF8, 0, extension.c_str(), -1, converted,
                                static_cast<int>(std::size(converted)));
            extW = converted;
        }
        const std::wstring pattern = extW.empty() ? L"*.*" : (L"*" + extW);
        COMDLG_FILTERSPEC filters[] = {
            {L"Passender Dateityp", pattern.c_str()},
            {L"Alle Dateien", L"*.*"}
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);

        wchar_t wtitle[256]{};
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, static_cast<int>(std::size(wtitle)));
        dialog->SetTitle(wtitle);
        const HWND owner = g_appWindowForDialogs ? glfwGetWin32Window(g_appWindowForDialogs) : nullptr;
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR pathW = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pathW)) && pathW) {
                    char pathA[4096]{};
                    WideCharToMultiByte(CP_UTF8, 0, pathW, -1, pathA,
                                        static_cast<int>(std::size(pathA)), nullptr, nullptr);
                    result = std::string(pathA);
                    CoTaskMemFree(pathW);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (weOwnCom) CoUninitialize();
    return result;
}
#endif

// Sammelt ALLE Ordner namens "resmap" (Groß-/Kleinschreibung egal) unter root, bis zu einer
// begrenzten Tiefe - bewusst NICHT nur den ersten Treffer (siehe Rückmeldung: es gibt
// offenbar mehr als einen so benannten Ordner, z.B. einen leeren/falschen unter einem
// anderen Unterordner - der erste Treffer war nicht der richtige). Welcher Kandidat
// tatsächlich Kartendaten enthält, entscheidet ResolveMapSearchRootAndScan weiter unten
// (scannt jeden Kandidaten und nimmt den mit den meisten gefundenen Karten). Steigt bewusst
// NICHT in "ressystem" oder "fieldTexture"/"fieldtexture" ab - das sind andere,
// potenziell große Ressourcen-Ordner ohne Kartendaten.
void FindAllResmapCandidates(const std::filesystem::path& root, int depthRemaining,
                              std::vector<std::filesystem::path>& outCandidates) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return;
    }
    std::vector<std::filesystem::path> subdirs;
    for (const auto& entry : std::filesystem::directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec || !entry.is_directory(ec)) continue;
        std::string name = entry.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == "resmap") {
            outCandidates.push_back(entry.path());
            continue; // in ein gefundenes resmap selbst nicht nochmal hinabsteigen
        }
        if (name != "ressystem" && name != "fieldtexture") subdirs.push_back(entry.path());
    }
    if (depthRemaining <= 0) return;
    for (const auto& sub : subdirs) {
        FindAllResmapCandidates(sub, depthRemaining - 1, outCandidates);
    }
}
// Sucht rekursiv (begrenzte Tiefe) unter root nach Karten-.ini-Dateien - das sind die
// Karten-Einstiegspunkte für OpenLegacyMap. WICHTIG: akzeptiert nur .ini-Dateien, deren
// Dateiname (ohne Endung) mit dem Namen ihres eigenen Ordners übereinstimmt
// (<MAPORDNER>/<MAPORDNER>.ini - die übliche Konvention). Das schließt gezielt andere,
// nicht zu einer Karte gehörende .ini-Dateien aus, die sonst z.B. direkt unter einem
// Kategorie-Ordner (UNTERORDNER) liegen könnten und fälschlich als "Karte" auftauchten.
// Steigt bewusst NICHT in "fieldTexture"/"fieldtexture" ab (dort liegen nur geteilte
// Texturen, keine Karten-inis, aber potenziell sehr viele Dateien).
std::vector<DiscoveredMap> ScanForMaps(
    const std::filesystem::path& root,
    bool allowDirectNifMapFolders) {
    std::vector<DiscoveredMap> found;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return found;
    }

    constexpr int kMaxDepth = 6;
    std::vector<DiscoveredMap> nifOnlyCandidates;
    std::unordered_set<std::string> legacyMapDirs;
    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; it != end && !ec; it.increment(ec)) {
        const auto& entry = *it;
        if (entry.is_directory(ec)) {
            const std::string dirName = entry.path().filename().string();
            const bool isTextureDir = dirName.size() == 12 &&
                (dirName == "fieldTexture" || dirName == "fieldtexture" || dirName == "FieldTexture");
            if (isTextureDir || it.depth() >= kMaxDepth) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!entry.is_regular_file(ec)) continue;

        const auto& path = entry.path();
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".ini" && ext != ".nif") continue;

        std::string stem = path.stem().string();
        std::string folderName = path.parent_path().filename().string();
        std::string stemLower = stem, folderLower = folderName;
        std::transform(stemLower.begin(), stemLower.end(), stemLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(folderLower.begin(), folderLower.end(), folderLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (stemLower != folderLower) continue;

        std::string parentKey = path.parent_path().lexically_normal().string();
        std::transform(parentKey.begin(), parentKey.end(), parentKey.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".ini") {
            legacyMapDirs.insert(parentKey);
            found.push_back({folderName, path.string(), false, allowDirectNifMapFolders});
            continue;
        }

        // Source trees are conservative: only the established "field" hierarchy counts as
        // a standalone NIF map. The project tree additionally permits the documented new-map
        // layout <Project>/Client/resmap/<Map>/<Map>.nif.
        bool belowField = false;
        for (const auto& component : path.parent_path()) {
            std::string c = component.string();
            std::transform(c.begin(), c.end(), c.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (c == "field") { belowField = true; break; }
        }
        const bool directChildMap =
            allowDirectNifMapFolders &&
            path.parent_path().parent_path().lexically_normal() == root.lexically_normal();
        if (belowField || directChildMap)
            nifOnlyCandidates.push_back({folderName, path.string(), true, allowDirectNifMapFolders});
    }

    for (auto& candidate : nifOnlyCandidates) {
        std::string parentKey = std::filesystem::path(candidate.iniPath).parent_path().lexically_normal().string();
        std::transform(parentKey.begin(), parentKey.end(), parentKey.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!legacyMapDirs.contains(parentKey)) found.push_back(std::move(candidate));
    }

    std::sort(found.begin(), found.end(), [](const DiscoveredMap& a, const DiscoveredMap& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.standaloneNif < b.standaloneNif;
    });
    return found;
}
// "Client Ordner" ist laut Rückmeldung der Client-WURZELordner (enthält "resmap" - direkt
// oder ein paar Ebenen tiefer -, nicht direkt die Karten). Sammelt ALLE Ordner namens
// "resmap" und scannt JEDEN davon - verwendet den mit den MEISTEN gefundenen Karten (nicht
// einfach den ersten Treffer, siehe Rückmeldung: es gab einen leeren/falschen "resmap" an
// anderer Stelle, der vorher fälschlich zuerst gefunden wurde). Liefert `root = nullopt`,
// wenn gar kein "resmap"-Ordner existiert - fällt NIE auf eine Suche im gesamten
// Client-Ordner zurück.
ResmapResolution ResolveMapSearchRootAndScan(const std::filesystem::path& clientFolder) {
    ResmapResolution result;

    std::string selfName = clientFolder.filename().string();
    std::transform(selfName.begin(), selfName.end(), selfName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::vector<std::filesystem::path> candidates;
    if (selfName == "resmap") {
        candidates.push_back(clientFolder); // Nutzer hat resmap selbst als Client Ordner gewählt
    } else {
        FindAllResmapCandidates(clientFolder, 3, candidates);
    }
    result.candidateCount = static_cast<int>(candidates.size());
    if (candidates.empty()) {
        return result;
    }

    std::size_t bestIdx = 0;
    std::vector<DiscoveredMap> bestMaps = ScanForMaps(candidates[0]);
    for (std::size_t i = 1; i < candidates.size(); ++i) {
        auto maps = ScanForMaps(candidates[i]);
        if (maps.size() > bestMaps.size()) {
            bestIdx = i;
            bestMaps = std::move(maps);
        }
    }
    result.root = candidates[bestIdx];
    result.maps = std::move(bestMaps);
    return result;
}
std::size_t MergeProjectOnlyMaps(
    const ProjectConfig& cfg,
    std::vector<DiscoveredMap>& maps) {

    if (cfg.projectFolder[0] == '\0') return 0;
    auto projectResmap = ProjectPathForRelative(
        cfg, core::ProjectOutputSide::Client, std::filesystem::path("resmap"));
    if (!projectResmap) return 0;

    auto projectMaps = ScanForMaps(*projectResmap, true);
    const auto mapNameKey = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    };
    std::unordered_set<std::string> knownNames;
    for (const auto& map : maps) knownNames.insert(mapNameKey(map.name));

    std::size_t added = 0;
    for (auto& map : projectMaps) {
        if (!knownNames.insert(mapNameKey(map.name)).second) {
            // Existing source maps keep their immutable source identity. Their opener already
            // prefers the matching project override path.
            continue;
        }
        map.projectOnly = true;
        maps.push_back(std::move(map));
        ++added;
    }
    std::sort(maps.begin(), maps.end(), [](const DiscoveredMap& a, const DiscoveredMap& b) {
        if (a.name != b.name) return a.name < b.name;
        if (a.projectOnly != b.projectOnly) return a.projectOnly < b.projectOnly;
        return a.standaloneNif < b.standaloneNif;
    });
    return added;
}
// Enthält die komplette, bereits bestehende "Karte öffnen/speichern"- und
// Einzelmodul-Import/Export-Logik (früher Inhalt des "Datei"-Menüs) - jetzt als
// eigenständiger Inhalt statt Menü-Dropdown, damit er sich in die neue "Datei"-Spalte
// des Arbeitsbereichs (siehe DrawMapEditorWorkspace) einbetten lässt. Funktional
// unverändert gegenüber der bisherigen Menüleiste.
void DrawAdvancedFileOps(EditorState& state) {
    ImGui::TextColored(ImVec4(0.90f, 0.94f, 1.0f, 1.0f), "Karte öffnen/speichern (komplett, alle Module)");

#ifdef _WIN32
        if (UI::Button("Asset-Ordner wählen...")) {
            if (auto picked = BrowseForFolderWindows("Client-Ordner mit den Karten w\u00e4hlen (enth\u00e4lt z.B. 'field\\')")) {
                std::snprintf(state.resmapRootPath, sizeof(state.resmapRootPath), "%s", picked->c_str());
                state.discoveredMaps = ScanForMaps(state.resmapRootPath);
                MergeProjectOnlyMaps(state.project, state.discoveredMaps);
                state.selectedMapIndex = -1;
                state.statusMessage = std::to_string(state.discoveredMaps.size()) + " Karte(n) gefunden in: " + std::string(state.resmapRootPath);
            }
        }
        ImGui::SameLine();
#endif
        ImGui::TextDisabled("%s", state.resmapRootPath[0] != '\0' ? state.resmapRootPath : "(kein Ordner gew\u00e4hlt)");

        if (!state.discoveredMaps.empty()) {
            ImGui::Text("Gefundene Karten (%zu):", state.discoveredMaps.size());
            ImGui::BeginChild("DiscoveredMaps", ImVec2(0, 100), true);
            for (int i = 0; i < static_cast<int>(state.discoveredMaps.size()); ++i) {
                const bool selected = state.selectedMapIndex == i;
                const auto& discovered = state.discoveredMaps[static_cast<std::size_t>(i)];
                const std::string label = discovered.name +
                    (discovered.projectOnly ? " [Project]" : "") +
                    (discovered.standaloneNif ? " [NIF-only]##advancedMap" : "##advancedMap");
                if (UI::Selectable(label.c_str(), selected)) {
                    state.selectedMapIndex = i;
                    std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s",
                                  discovered.iniPath.c_str());
                }
            }
            ImGui::EndChild();
        }

        UI::InputText("Karte-.ini##project", state.legacyMapIniPath, sizeof(state.legacyMapIniPath));
        if (UI::Button("Karte öffnen")) {
            OpenMapPathIntoState(state, std::filesystem::path(state.legacyMapIniPath), true);
        }
        UI::InputText("Ausgabeverzeichnis##project", state.legacySaveDir, sizeof(state.legacySaveDir));
#ifdef _WIN32
        ImGui::SameLine();
        if (UI::Button("Wählen...##saveDir")) {
            if (auto picked = BrowseForFolderWindows("Ausgabeverzeichnis w\u00e4hlen")) {
                std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", picked->c_str());
            }
        }
#endif
        UI::InputText("Kartenname##project", state.legacySaveStem, sizeof(state.legacySaveStem));
        ImGui::BeginDisabled(!state.hasLegacyIniMeta);
        if (UI::Button("Karte speichern")) {
            SaveLegacyMapProject(state);
        }
        ImGui::EndDisabled();
        if (!state.hasLegacyIniMeta) {
            ImGui::TextDisabled("(Erst eine Karte \u00f6ffnen, bevor gespeichert werden kann.)");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Einzelne Module (fortgeschritten)");
        if (UI::MenuItem("Neu (257x257)")) {
            state.heightmap = core::Heightmap(257, 257, 50.0f, 50.0f);
            state.mapHasHeightmap = true;
            ClearMapHistory(state);
            state.meshDirty = true;
            SyncWalkGridSize(state);
            state.selectedLayer = -1;
            state.statusMessage = "Neue leere Heightmap erstellt.";
        }
        ImGui::Separator();

        ImGui::TextDisabled("%s", L("Höhendaten austauschen (16-Bit-PNG / R16, z.B. World Machine, Gaea)",
                                    "Exchange height data (16-bit PNG / R16, e.g. World Machine, Gaea)"));
        UI::InputText(L("Datei (.png/.r16/.raw)##hx","File (.png/.r16/.raw)##hx"), state.heightExchangePath, sizeof(state.heightExchangePath));
        if (UI::Button(L("Exportieren##hx","Export##hx")) && state.heightExchangePath[0] != '\0') {
            auto r = core::ExportHeightmapImage(state.heightmap, state.heightExchangePath);
            const auto [lo, hi] = state.heightmap.MinMax();
            char buf[160];
            std::snprintf(buf, sizeof(buf), L("Exportiert, Höhenbereich %.1f .. %.1f","Exported, height range %.1f .. %.1f"), lo, hi);
            state.heightExchangeStatus = r ? std::string(buf) : r.error();
        }
        ImGui::SameLine();
        if (UI::Button(L("Bereich aus Datei##hx","Range from file##hx")) && state.heightExchangePath[0] != '\0') {
            if (auto img = core::ImportHeightImage(state.heightExchangePath); img && img->range) {
                state.heightExchangeMin = img->range->first;
                state.heightExchangeMax = img->range->second;
            } else {
                state.heightExchangeStatus = img ? L("Datei enthält keinen Höhenbereich - bitte angeben.","File holds no height range - please enter it.")
                                                 : img.error();
            }
        }
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputFloat(L("Min##hx","Min##hx"), &state.heightExchangeMin, 0.0f, 0.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputFloat(L("Max##hx","Max##hx"), &state.heightExchangeMax, 0.0f, 0.0f, "%.1f");
        if (UI::Button(L("Importieren (ein Undo-Schritt)##hx","Import (one undo step)##hx")) && state.heightExchangePath[0] != '\0') {
            auto img = core::ImportHeightImage(state.heightExchangePath);
            if (!img) state.heightExchangeStatus = img.error();
            else {
                auto patch = core::ApplyHeightImage(state.heightmap, *img, state.heightExchangeMin, state.heightExchangeMax);
                const std::size_t changed = patch.entries.size();
                if (changed) {
                    PushHeightmapHistory(state, std::move(patch));
                    state.meshDirty = true;
                    state.mapDirty = true;
                }
                state.heightExchangeStatus = std::to_string(img->width) + "x" + std::to_string(img->height) +
                    L(" importiert, "," imported, ") + std::to_string(changed) + L(" Höhenpunkte geändert"," height points changed") +
                    (img->width != state.heightmap.Width() || img->height != state.heightmap.Height()
                         ? L(" (auf Kartengröße umgerechnet)"," (resampled to map size)") : "");
            }
        }
        if (!state.heightExchangeStatus.empty()) ImGui::TextWrapped("%s", state.heightExchangeStatus.c_str());
        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Heightmap-Import/Export (.HTD / .HTDG)");
        UI::InputText("Pfad##legacy", state.legacyPath, sizeof(state.legacyPath));
        UI::InputInt("Breite (aus .ini)", &state.legacyWidth);
        UI::InputInt("Höhe (aus .ini)", &state.legacyHeight);
        UI::InputFloat("Blockbreite", &state.legacyBlockWidth);
        UI::InputFloat("Blockhöhe", &state.legacyBlockHeight);
        if (UI::Button("Importieren##htd")) {
            if (state.legacyWidth > 0 && state.legacyHeight > 0) {
                const auto inputPath = PreferProjectOverride(
                    state.project, core::ProjectOutputSide::Client,
                    std::filesystem::path(state.legacyPath));
                auto result = core::ImportLegacyHtd(
                    inputPath,
                    static_cast<std::uint32_t>(state.legacyWidth),
                    static_cast<std::uint32_t>(state.legacyHeight),
                    state.legacyBlockWidth, state.legacyBlockHeight, &state.htdHeader, &state.htdTrailingBytes);
                if (result) {
                    state.heightmap = std::move(*result);
                    state.mapHasHeightmap = true;
                    ClearMapHistory(state);
                    state.meshDirty = true;
                    SyncWalkGridSize(state);
                    state.statusMessage = "Legacy-Datei importiert: " + std::string(state.legacyPath);
                } else {
                    state.statusMessage = "Import fehlgeschlagen: " + result.error();
                }
            }
        }
        ImGui::SameLine();
        if (UI::Button("Exportieren##htd")) {
            auto target = PrepareProjectClientRequestedOutput(
                state.project, std::filesystem::path(state.legacyPath));
            if (!target) state.statusMessage = "Export blockiert: " + target.error();
            else {
                auto result = core::ExportLegacyHtd(
                    state.heightmap, *target, state.htdHeader, state.htdTrailingBytes);
                state.statusMessage = result ? "Legacy-HTD als Projekt-Override exportiert nach: " + target->string()
                                             : "Export fehlgeschlagen: " + result.error();
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Texturing-Set (ini + Blend-BMPs)");
        UI::InputText("ini-Pfad##legacyTex", state.legacyIniPath, sizeof(state.legacyIniPath));
        if (UI::Button("Legacy-Set importieren")) {
            core::legacy::TextureSetImportReport report;
            const auto inputPath = PreferProjectOverride(
                state.project, core::ProjectOutputSide::Client,
                std::filesystem::path(state.legacyIniPath));
            auto result = core::legacy::ImportLegacyTextureSet(inputPath, &report);
            auto metaResult = core::legacy::ParseLegacyMapIni(inputPath);
            if (result && metaResult) {
                state.textureStack = std::move(*result);
                ClearMapHistory(state);
                state.selectedLayer = state.textureStack.LayerCount() > 0 ? 0 : -1;
                state.layerPreviewDirty = true;
                state.legacyIniMeta = std::move(*metaResult);
                state.hasLegacyIniMeta = true;
                state.statusMessage = "Legacy-Set importiert (" + std::to_string(state.textureStack.LayerCount()) +
                                       " Layer, " + std::to_string(report.missingBlendFiles.size()) + " Blend-BMP(s) fehlend/nicht ladbar).";
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + (result ? metaResult.error() : result.error());
            }
        }
        UI::InputText("Export-Verzeichnis##legacyTex", state.legacyExportDir, sizeof(state.legacyExportDir));
        ImGui::BeginDisabled(!state.hasLegacyIniMeta);
        if (UI::Button("Legacy-Set exportieren")) {
            auto target = PrepareProjectClientRequestedOutput(
                state.project, std::filesystem::path(state.legacyExportDir));
            if (!target) state.statusMessage = "Export blockiert: " + target.error();
            else {
                auto result = core::legacy::ExportLegacyTextureSet(
                    state.textureStack, state.legacyIniMeta, *target, "Rou.ini");
                state.statusMessage = result ? "Legacy-Set als Projekt-Override exportiert nach: " + target->string()
                                             : "Export fehlgeschlagen: " + result.error();
            }
        }
        ImGui::EndDisabled();
        if (!state.hasLegacyIniMeta) {
            ImGui::TextDisabled("(Erst \u00fcber 'Legacy-Set importieren' Metadaten laden.)");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Import/Export (.shbd)");
        UI::InputText("Pfad##walkLegacy", state.walkLegacyPath, sizeof(state.walkLegacyPath));
        UI::InputInt("Breite##walkLegacy", &state.walkLegacyWidth);
        UI::InputInt("Höhe##walkLegacy", &state.walkLegacyHeight);
        if (UI::Button("Importieren##shbd")) {
            if (state.walkLegacyWidth > 0 && state.walkLegacyHeight > 0) {
                const auto inputPath = PreferProjectOverride(
                    state.project, core::ProjectOutputSide::Client,
                    std::filesystem::path(state.walkLegacyPath));
                auto result = core::ImportLegacyShbd(
                    inputPath,
                    static_cast<std::uint32_t>(state.walkLegacyWidth),
                    static_cast<std::uint32_t>(state.walkLegacyHeight), &state.shbdHeader);
                if (result) {
                    state.walkGrid = std::move(*result);
                    state.walkGridAtLoad = state.walkGrid;
                    state.modelBlockStatsKey.clear();
                    ClearMapHistory(state);
                    state.walkPreviewDirty = true;
                    state.statusMessage = "Legacy-shbd importiert: " + std::string(state.walkLegacyPath);
                } else {
                    state.statusMessage = "Import fehlgeschlagen: " + result.error();
                }
            }
        }
        ImGui::SameLine();
        if (UI::Button("Exportieren##shbd")) {
            auto target = PrepareProjectClientRequestedOutput(
                state.project, std::filesystem::path(state.walkLegacyPath));
            if (!target) state.statusMessage = "Export blockiert: " + target.error();
            else {
                auto result = core::ExportLegacyShbd(state.walkGrid, *target, state.shbdHeader);
                state.statusMessage = result ? "Legacy-shbd als Projekt-Override exportiert nach: " + target->string()
                                             : "Export fehlgeschlagen: " + result.error();
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Objekt-Placement (.shmd)");
        UI::InputText("Pfad##shmd", state.legacyShmdPath, sizeof(state.legacyShmdPath));
        if (UI::Button("Importieren##shmd")) {
            const auto inputPath = PreferProjectOverride(
                state.project, core::ProjectOutputSide::Client,
                std::filesystem::path(state.legacyShmdPath));
            auto result = core::legacy::ParseLegacyShmd(inputPath);
            if (result) {
                state.legacyShmdSourceCount = static_cast<std::int32_t>(result->Count());
                state.placementSet = std::move(*result);
                state.selectedObject = kNoObjectSelection;
                state.selectedObjects.clear();
                state.selectedObjectModelPathFor = kNoObjectSelection;
                state.selectedObjectModelPathDirty = false;
                state.objectListRangeAnchor = -1;
                state.objectEditorHidden.assign(state.placementSet.Count(), 0);
                state.objectEditorLocked.assign(state.placementSet.Count(), 0);
                state.objectEditorLabels.assign(state.placementSet.Count(), {});
                state.objectEditorGroups.assign(state.placementSet.Count(), {});
                state.shmdEditorHiddenKeys.clear();
                state.shmdEditorLockedKeys.clear();
                state.shmdEditorLabels.clear();
                state.shmdEditorGroups.clear();
                const std::filesystem::path shmdMapDir = inputPath.parent_path();
                state.nifMeshRenderer.LoadModelsForSet(state.placementSet, shmdMapDir);
                RebuildShmdCategoryRenderSet(state, shmdMapDir);
                state.statusMessage = "Legacy-shmd importiert (" + std::to_string(state.placementSet.Count()) +
                                      " Placement-Objekte, " + std::to_string(state.shmdCategoryRenderSet.Count()) +
                                      " SHMD-Szenenmodelle): " + std::string(state.legacyShmdPath);
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + result.error();
            }
        }
        ImGui::SameLine();
        if (UI::Button("Exportieren##shmd")) {
            auto target = PrepareProjectClientRequestedOutput(
                state.project, std::filesystem::path(state.legacyShmdPath));
            if (!target) state.statusMessage = "Export blockiert: " + target.error();
            else {
                auto result = core::legacy::SerializeLegacyShmd(state.placementSet, *target);
                state.statusMessage = result ? "Legacy-shmd als Projekt-Override exportiert nach: " + target->string()
                                             : "Export fehlgeschlagen: " + result.error();
            }
        }

        ImGui::TextDisabled("Legacy räumlicher Index (.idm) - reiner Pass-Through, Semantik ungeklärt");
        UI::InputText("Pfad##idm", state.legacyIdmPath, sizeof(state.legacyIdmPath));
        if (UI::Button("Importieren##idm")) {
            const auto inputPath = PreferProjectOverride(
                state.project, core::ProjectOutputSide::Client,
                std::filesystem::path(state.legacyIdmPath));
            auto result = core::legacy::ParseLegacyIdm(inputPath);
            if (result) {
                state.legacySpatialIndex = std::move(*result);
                state.hasLegacySpatialIndex = true;
                state.statusMessage = "Legacy-idm importiert (" + std::to_string(state.legacySpatialIndex.groups.size()) + " Gruppen, roh gehalten): " + std::string(state.legacyIdmPath);
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + result.error();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.hasLegacySpatialIndex);
        if (UI::Button("Exportieren##idm")) {
            auto target = PrepareProjectClientRequestedOutput(
                state.project, std::filesystem::path(state.legacyIdmPath));
            if (!target) state.statusMessage = "Export blockiert: " + target.error();
            else {
                // Dieselbe IDM-Pflege wie beim Speichern der Karte (Zuordnungen folgen der SHMD-Reihenfolge).
                const auto remapped = BuildProjectFromState(state).spatialIndex;
                auto result = core::legacy::SerializeLegacyIdm(remapped, *target);
                state.statusMessage = result ? "Legacy-idm als Projekt-Override exportiert nach: " + target->string()
                                             : "Export fehlgeschlagen: " + result.error();
            }
        }
        ImGui::EndDisabled();

        ImGui::TextDisabled("Legacy-Zonen-Metadaten (.aid)");
        UI::InputText("Pfad##aid", state.legacyAidPath, sizeof(state.legacyAidPath));
        if (UI::Button("Importieren##aid")) {
            const auto inputPath = PreferProjectOverride(
                state.project, core::ProjectOutputSide::Client,
                std::filesystem::path(state.legacyAidPath));
            auto result = core::legacy::ParseLegacyAid(inputPath);
            if (result) {
                state.legacyZoneMetadata = *result;
                state.selectedZone = 0;
                state.hasLegacyZoneMetadata = true;
                std::snprintf(state.zoneNameBuf, sizeof(state.zoneNameBuf), "%s", state.legacyZoneMetadata.name.c_str());
                state.statusMessage = "Legacy-aid importiert (Zone '" + state.legacyZoneMetadata.name + "'): " + std::string(state.legacyAidPath);
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + result.error();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.hasLegacyZoneMetadata);
        if (UI::Button("Exportieren##aid")) {
            auto target = PrepareProjectClientRequestedOutput(
                state.project, std::filesystem::path(state.legacyAidPath));
            if (!target) state.statusMessage = "Export blockiert: " + target.error();
            else {
                auto result = core::legacy::SerializeLegacyAid(state.legacyZoneMetadata, *target);
                state.statusMessage = result ? "Legacy-aid als Projekt-Override exportiert nach: " + target->string()
                                             : "Export fehlgeschlagen: " + result.error();
            }
        }
        ImGui::EndDisabled();
        if (state.hasLegacyZoneMetadata) {
            auto& zones = state.legacyZoneMetadata;
            ImGui::Text("Zonen: %zu", zones.AreaCount());
            if (zones.AreaCount() > 0) {
                state.selectedZone = std::clamp(state.selectedZone, 0, static_cast<int>(zones.AreaCount()) - 1);
                if (ImGui::BeginCombo("Zone##aid", zones.Area(state.selectedZone).name.c_str())) {
                    for (std::size_t i = 0; i < zones.AreaCount(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Selectable(zones.Area(i).name.c_str(), state.selectedZone == static_cast<int>(i))) {
                            state.selectedZone = static_cast<int>(i);
                            std::snprintf(state.zoneNameBuf, sizeof(state.zoneNameBuf), "%s", zones.Area(i).name.c_str());
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                auto& area = zones.Area(state.selectedZone);
                if (UI::InputText("Zonenname", state.zoneNameBuf, 33)) area.name = state.zoneNameBuf;
                ImGui::TextDisabled("Datensatztyp %d; %d Werte", area.flag, area.flag == 0 ? 3 : 5);
            }
        }
}

} // namespace nge
