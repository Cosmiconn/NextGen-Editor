// MapDocument.cpp
// Karte speichern, SHMD-Kategorien, Undo/Redo-Verlauf, Editor-Metadaten der Objekte.

#include "EditorShared.hpp"

namespace nge {

// Die Textur-Layer-Auflösung bleibt unabhängig von der Heightmap. Neue Karten verwenden
// standardmäßig 1024x1024, während beim Laden die tatsächlich gespeicherte Auflösung erhalten bleibt.

// Baut aus dem aktuellen Editor-Zustand ein LegacyMapProject für SaveLegacyMap zusammen -
// Kehrseite dessen, was ApplyProjectToState (siehe unten) beim Öffnen verteilt.
core::legacy::LegacyMapProject BuildProjectFromState(const EditorState& state,
                                                     core::legacy::SpatialIndexRemapReport* idmReport) {
    core::legacy::LegacyMapProject project;
    project.ini = state.legacyIniMeta;
    project.preservedFiles = state.preservedMapFiles;
    project.sourceStem = state.legacySourceStem;
    project.sourceFileNames = state.legacySourceFileNames;
    project.heightmap = state.heightmap;
    project.hasHeightmap = MapSupportsTerrainEditing(state);
    project.htdHeader = state.htdHeader;
    project.htdTrailingBytes = state.htdTrailingBytes;
    project.textureStack = state.textureStack;
    project.walkGrid = state.walkGrid;
    project.hasWalkGrid = state.walkGrid.Width() > 0 && state.walkGrid.Height() > 0;
    project.shbdHeader = state.shbdHeader;
    project.objects = state.placementSet;
    project.hasObjects = true;
    // IDM-Indizes zeigen auf die SHMD-Schreibreihenfolge. Nach Löschen/Duplizieren verschiebt sich
    // diese; RemapSpatialIndex bildet jede erhaltene Zuordnung auf die neue Position ab (unverändert
    // -> byte-gleich). Neue Objekte bleiben wie im Original-Client ohne IDM-Eintrag.
    if (state.hasLegacySpatialIndex) {
        std::vector<std::int32_t> writtenSources;
        for (const std::size_t i : core::legacy::ShmdWrittenOrder(state.placementSet))
            writtenSources.push_back(state.placementSet.At(i).sourceIndex);
        project.spatialIndex = core::legacy::RemapSpatialIndex(state.legacySpatialIndex, writtenSources, idmReport,
                                                               state.legacyShmdSourceCount);
    } else {
        project.spatialIndex = state.legacySpatialIndex;
    }
    project.hasSpatialIndex = state.hasLegacySpatialIndex;
    project.zone = state.legacyZoneMetadata;
    project.hasZone = state.hasLegacyZoneMetadata;
    return project;
}
bool SaveLegacyMapProject(EditorState& state) {
    if (state.project.projectFolder[0] == '\0') {
        state.statusMessage = L("Karte speichern blockiert: kein Projektordner konfiguriert.",
                                "Map save blocked: no project folder configured.");
        return false;
    }
    if (state.legacySaveStem[0] == '\0') {
        state.statusMessage = L("Karte speichern blockiert: kein Kartenname.",
                                "Map save blocked: no map name.");
        return false;
    }

    std::filesystem::path outputDir;
    const std::filesystem::path requested(state.legacySaveDir);
    if (!requested.empty()) {
        if (!IsProjectSidePath(state.project, core::ProjectOutputSide::Client, requested)) {
            state.statusMessage = L(
                "Karte speichern blockiert: Ausgabeverzeichnis liegt außerhalb von <Projekt>/Client.",
                "Map save blocked: output directory is outside <Project>/Client.");
            return false;
        }
        outputDir = requested;
    }

    if (outputDir.empty() && state.legacyMapIniPath[0] != '\0') {
        const std::filesystem::path workingOrSource(state.legacyMapIniPath);
        if (IsProjectSidePath(state.project, core::ProjectOutputSide::Client, workingOrSource)) {
            outputDir = workingOrSource.parent_path();
        } else {
            auto target = ProjectPathForSource(
                state.project, core::ProjectOutputSide::Client, workingOrSource);
            if (!target) {
                state.statusMessage = L("Karte speichern blockiert: ","Map save blocked: ") + target.error();
                return false;
            }
            outputDir = target->parent_path();
        }
    }

    if (outputDir.empty()) {
        auto target = ProjectPathForRelative(
            state.project, core::ProjectOutputSide::Client,
            std::filesystem::path("resmap") / state.legacySaveStem /
            (std::string(state.legacySaveStem) + ".ini"));
        std::filesystem::path outputIni;
        std::string error;
        if (!PrepareProjectOutput(target, outputIni, &error)) {
            state.statusMessage = "Karte speichern blockiert: " + error;
            return false;
        }
        outputDir = outputIni.parent_path();
    } else {
        std::string error;
        std::filesystem::path outputIni;
        auto validated = ProjectPathForRelative(
            state.project, core::ProjectOutputSide::Client,
            std::filesystem::relative(
                outputDir / (std::string(state.legacySaveStem) + ".ini"),
                std::filesystem::path(state.project.projectFolder) / "Client") );
        if (!PrepareProjectOutput(validated, outputIni, &error)) {
            state.statusMessage = "Karte speichern blockiert: " + error;
            return false;
        }
    }

    std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", outputDir.string().c_str());
    core::legacy::SpatialIndexRemapReport idmReport;
    auto project = BuildProjectFromState(state, &idmReport);
    auto result = core::legacy::SaveLegacyMap(project, outputDir, state.legacySaveStem);
    if (!result) {
        state.statusMessage = L("Fehler: ","Error: ") + result.error();
        return false;
    }

    state.legacyIniMeta = project.ini;
    state.mapDirty = false;
    // Der Server hat eine eigene Kopie der Begehbarkeit: 9Data/Shine/BlockInfo/<Servername>.shbd
    // (in NA2016 bei 6 von 7 geprüften Karten bytegleich mit der Client-Datei). Ohne sie wirkt eine
    // geänderte Begehbarkeit im Spiel nur im Client. Geschrieben wird, wenn das Gitter geändert wurde
    // oder die Karte neu ist; eine unveränderte Server-Datei (z.B. Rou, die abweicht) bleibt unberührt.
    std::string serverWalkNote;
    if (state.walkGrid.Width() > 0 && state.walkGrid.Height() > 0) {
        const bool newMap = state.walkGridAtLoad.Width() == 0;
        const bool walkChanged = newMap || state.walkGridAtLoad.Width() != state.walkGrid.Width() ||
                                 state.walkGridAtLoad.Height() != state.walkGrid.Height() ||
                                 core::DiffWalkGrids(state.walkGridAtLoad, state.walkGrid).Agreement() < 1.0;
        if (walkChanged) {
            const std::string serverName = CurrentMapServerName(state);
            std::filesystem::path serverShbd;
            std::string error;
            if (!PrepareProjectOutput(ProjectServerShinePath(state.project, std::filesystem::path("BlockInfo") / (serverName + ".shbd")),
                                      serverShbd, &error)) {
                serverWalkNote = L("\nServer-BlockInfo nicht geschrieben: ", "\nServer BlockInfo not written: ") + error;
            } else if (auto written = core::ExportLegacyShbd(state.walkGrid, serverShbd, state.shbdHeader); !written) {
                serverWalkNote = L("\nServer-BlockInfo nicht geschrieben: ", "\nServer BlockInfo not written: ") + written.error();
            } else {
                serverWalkNote = L("\nBegehbarkeit auch für den Server: ", "\nWalkability also for the server: ") + serverShbd.string();
                // Neue Karten brauchen auch die .aid auf dem Server; vorhandene bleiben unberührt.
                const auto clientAid = core::legacy::FindSiblingFileByStem(outputDir, state.legacySaveStem, ".aid");
                std::filesystem::path serverAid;
                if (clientAid && PrepareProjectOutput(ProjectServerShinePath(state.project, std::filesystem::path("BlockInfo") / (serverName + ".aid")),
                                                      serverAid, nullptr)) {
                    std::error_code ec;
                    const std::string serverRoot = PortalServerRoot(state);
                    const bool sourceHasAid = !serverRoot.empty() &&
                        core::legacy::ResolveCaseInsensitivePath(std::filesystem::path(serverRoot) / "BlockInfo", serverName + ".aid").has_value();
                    if (!sourceHasAid && !std::filesystem::exists(serverAid, ec))
                        std::filesystem::copy_file(*clientAid, serverAid, std::filesystem::copy_options::skip_existing, ec);
                }
            }
        }
    }
    FinishMapAutosave(state, true);
    core::RemoveAutosave(state.project.projectFolder, state.legacySaveStem); // Projekt ist jetzt aktuell
    state.autosaveSavedRevision = state.mapEditRevision;
    const auto ini = outputDir / (std::string(state.legacySaveStem) + ".ini");
    TouchRecentMap(state, ini.string());
    state.statusMessage = std::string(T("workspace.savedas")) + outputDir.string();
    state.statusMessage += serverWalkNote;
    if (state.hasLegacySpatialIndex && !idmReport.identity) {
        state.statusMessage += std::string(L("\nIDM angepasst: ", "\nIDM updated: ")) +
            std::to_string(idmReport.movedObjects) + L(" Objekte neu zugeordnet, ", " objects remapped, ") +
            std::to_string(idmReport.removedObjects) + L(" gelöschte entfernt (", " deleted removed (") +
            std::to_string(idmReport.droppedReferences) + L(" Einträge). ", " entries). ") +
            std::to_string(idmReport.uncoveredObjects) +
            L(" Objekte ohne IDM-Eintrag (wie im Original-Client, z. B. Rou: 502).",
              " objects without IDM entry (as in the original client, e.g. Rou: 502).");
    }
    return true;
}
int ShmdCategoryKind(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower == "sky") return 0;
    if (lower == "water") return 1;
    if (lower == "groundobject") return 2;
    return -1;
}
void RebuildShmdCategoryRenderSet(EditorState& state, const std::filesystem::path& mapDir) {
    state.shmdCategoryRenderSet = core::ObjectPlacementSet{};
    state.shmdCategoryRenderKind.clear();
    state.shmdCategorySource.clear();

    // Kategorieeinträge haben absichtlich keinen Placement-Transform. Die zugehörigen NIFs
    // tragen ihre Szenengeometrie selbst; für die Vorschau werden sie deshalb einmal mit
    // Identity-Transform gerendert. Das Original-ObjectPlacementSet bleibt unverändert.
    for (std::size_t categoryIndex = 0; categoryIndex < state.placementSet.categories.size(); ++categoryIndex) {
        const auto& category = state.placementSet.categories[categoryIndex];
        const int kind = ShmdCategoryKind(category.name);
        for (std::size_t pathIndex = 0; pathIndex < category.modelPaths.size(); ++pathIndex) {
            const auto& modelPath = category.modelPaths[pathIndex];
            if (modelPath.empty()) continue;
            core::PlacedObject obj;
            obj.modelPath = modelPath;
            state.shmdCategoryRenderSet.AddObject(std::move(obj));
            state.shmdCategoryRenderKind.push_back(kind);
            state.shmdCategorySource.emplace_back(categoryIndex, pathIndex);
        }
    }

    state.shmdCategoryHidden.assign(state.shmdCategoryRenderSet.Count(), 0);
    state.shmdCategoryMeshRenderer.LoadModelsForSet(state.shmdCategoryRenderSet, mapDir);
}
int ShmdSelectionId(std::size_t renderIndex) {
    return -2 - static_cast<int>(renderIndex);
}
bool IsShmdSelection(int selectionId) {
    return selectionId <= -2;
}
std::optional<std::size_t> ShmdRenderIndex(int selectionId) {
    if (!IsShmdSelection(selectionId)) return std::nullopt;
    return static_cast<std::size_t>(-2 - selectionId);
}
std::filesystem::path CurrentObjectAssetMapDir(const EditorState& state) {
    if (state.legacyMapIniPath[0] != '\0') return std::filesystem::path(state.legacyMapIniPath).parent_path();
    if (state.legacyShmdPath[0] != '\0') return std::filesystem::path(state.legacyShmdPath).parent_path();
    if (state.legacySaveDir[0] != '\0') return std::filesystem::path(state.legacySaveDir);
    return {};
}
core::PlacedObject* EditableObject(EditorState& state, int selectionId) {
    if (selectionId >= 0) {
        const auto index = static_cast<std::size_t>(selectionId);
        return index < state.placementSet.Count() ? &state.placementSet.At(index) : nullptr;
    }
    const auto renderIndex = ShmdRenderIndex(selectionId);
    if (!renderIndex || *renderIndex >= state.shmdCategoryRenderSet.Count()) return nullptr;
    return &state.shmdCategoryRenderSet.At(*renderIndex);
}
const core::PlacedObject* EditableObject(const EditorState& state, int selectionId) {
    if (selectionId >= 0) {
        const auto index = static_cast<std::size_t>(selectionId);
        return index < state.placementSet.Count() ? &state.placementSet.At(index) : nullptr;
    }
    const auto renderIndex = ShmdRenderIndex(selectionId);
    if (!renderIndex || *renderIndex >= state.shmdCategoryRenderSet.Count()) return nullptr;
    return &state.shmdCategoryRenderSet.At(*renderIndex);
}
std::string ShmdSelectionCategoryName(const EditorState& state, int selectionId) {
    const auto renderIndex = ShmdRenderIndex(selectionId);
    if (!renderIndex || *renderIndex >= state.shmdCategorySource.size()) return {};
    const auto [categoryIndex, pathIndex] = state.shmdCategorySource[*renderIndex];
    (void)pathIndex;
    if (categoryIndex >= state.placementSet.categories.size()) return {};
    return state.placementSet.categories[categoryIndex].name;
}
void ReloadObjectRenderers(EditorState& state) {
    const auto mapDir = CurrentObjectAssetMapDir(state);
    state.nifMeshRenderer.LoadModelsForSet(state.placementSet, mapDir);
    RebuildShmdCategoryRenderSet(state, mapDir);
    state.objectVisKey.clear();
}
ObjectEditSnapshot CaptureObjectEditSnapshot(const EditorState& state) {
    ObjectEditSnapshot snapshot;
    snapshot.placementSet = state.placementSet;
    snapshot.selectedObjects = state.selectedObjects;
    snapshot.selectedObject = state.selectedObject;
    snapshot.editorHidden = state.objectEditorHidden;
    snapshot.editorLocked = state.objectEditorLocked;
    snapshot.editorLabels = state.objectEditorLabels;
    snapshot.editorGroups = state.objectEditorGroups;
    snapshot.shmdHiddenKeys = state.shmdEditorHiddenKeys;
    snapshot.shmdLockedKeys = state.shmdEditorLockedKeys;
    snapshot.shmdLabels = state.shmdEditorLabels;
    snapshot.shmdGroups = state.shmdEditorGroups;
    return snapshot;
}
void RestoreObjectEditSnapshot(EditorState& state, ObjectEditSnapshot snapshot) {
    state.placementSet = std::move(snapshot.placementSet);
    state.selectedObjects = std::move(snapshot.selectedObjects);
    state.selectedObject = snapshot.selectedObject;
    state.objectEditorHidden = std::move(snapshot.editorHidden);
    state.objectEditorLocked = std::move(snapshot.editorLocked);
    state.objectEditorLabels = std::move(snapshot.editorLabels);
    state.objectEditorGroups = std::move(snapshot.editorGroups);
    state.shmdEditorHiddenKeys = std::move(snapshot.shmdHiddenKeys);
    state.shmdEditorLockedKeys = std::move(snapshot.shmdLockedKeys);
    state.shmdEditorLabels = std::move(snapshot.shmdLabels);
    state.shmdEditorGroups = std::move(snapshot.shmdGroups);
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
    state.objectGizmoMatrixValid = false;
    ReloadObjectRenderers(state);
    state.mapDirty = true;
}
TextureLayerMetadata CaptureTextureLayerMetadata(const core::TextureLayer& layer) {
    return {layer.name, layer.diffuseFileName, layer.uvScaleDiffuse,
            layer.regionStartX, layer.regionStartY, layer.regionWidth, layer.regionHeight};
}
void RestoreTextureLayerMetadata(core::TextureLayer& layer, const TextureLayerMetadata& meta) {
    layer.name = meta.name;
    layer.diffuseFileName = meta.diffuseFileName;
    layer.uvScaleDiffuse = meta.uvScaleDiffuse;
    layer.regionStartX = meta.regionStartX;
    layer.regionStartY = meta.regionStartY;
    layer.regionWidth = meta.regionWidth;
    layer.regionHeight = meta.regionHeight;
}
void RefreshTextureLayerHistoryState(EditorState& state, int selectedLayer) {
    if (state.textureStack.LayerCount() == 0) state.selectedLayer = -1;
    else state.selectedLayer = std::clamp(selectedLayer, 0,
        static_cast<int>(state.textureStack.LayerCount()) - 1);
    state.layerHidden.resize(state.textureStack.LayerCount(), 0);
    state.renderer.LoadTerrainTextures(
        state.textureStack,
        !state.textureAssetRoot.empty() ? state.textureAssetRoot : CurrentObjectAssetMapDir(state));
    state.layerPreviewDirty = true;
    state.mapDirty = true;
}
std::optional<TextureLayerHistoryPatch> ApplyTextureLayerHistoryPatch(
    EditorState& state, TextureLayerHistoryPatch patch) {

    TextureLayerHistoryPatch inverse;
    inverse.selectedLayer = state.selectedLayer;

    switch (patch.kind) {
        case TextureLayerHistoryPatch::Kind::Remove: {
            if (patch.a >= state.textureStack.LayerCount()) return std::nullopt;
            auto removed = state.textureStack.TakeLayer(patch.a);
            if (!removed) return std::nullopt;
            inverse.kind = TextureLayerHistoryPatch::Kind::Insert;
            inverse.a = patch.a;
            inverse.layer = std::move(*removed);
            break;
        }
        case TextureLayerHistoryPatch::Kind::Insert: {
            if (!patch.layer) return std::nullopt;
            inverse.kind = TextureLayerHistoryPatch::Kind::Remove;
            inverse.a = patch.a;
            state.textureStack.InsertLayer(patch.a, std::move(*patch.layer));
            break;
        }
        case TextureLayerHistoryPatch::Kind::Move: {
            if (patch.a >= state.textureStack.LayerCount() || patch.b >= state.textureStack.LayerCount())
                return std::nullopt;
            inverse.kind = TextureLayerHistoryPatch::Kind::Move;
            inverse.a = patch.b;
            inverse.b = patch.a;
            state.textureStack.MoveLayer(patch.a, patch.b);
            break;
        }
        case TextureLayerHistoryPatch::Kind::Metadata: {
            if (patch.a >= state.textureStack.LayerCount()) return std::nullopt;
            inverse.kind = TextureLayerHistoryPatch::Kind::Metadata;
            inverse.a = patch.a;
            inverse.metadata = CaptureTextureLayerMetadata(state.textureStack.Layer(patch.a));
            RestoreTextureLayerMetadata(state.textureStack.Layer(patch.a), patch.metadata);
            break;
        }
        case TextureLayerHistoryPatch::Kind::ReplaceStack: {
            if (!patch.stack) return std::nullopt;
            inverse.kind = TextureLayerHistoryPatch::Kind::ReplaceStack;
            inverse.stack = std::move(state.textureStack);
            state.textureStack = std::move(*patch.stack);
            break;
        }
    }

    RefreshTextureLayerHistoryState(state, patch.selectedLayer);
    return inverse;
}
void PushTextureLayerHistory(EditorState& state, TextureLayerHistoryPatch patch) {
    RegisterMapHistoryAction(state, MapHistoryDomain::TextureLayers);
    state.textureLayerUndo.push_back(std::move(patch));
}
void RecordTextureLayerAdded(EditorState& state, std::size_t index, int selectedBefore) {
    TextureLayerHistoryPatch patch;
    patch.kind = TextureLayerHistoryPatch::Kind::Remove;
    patch.a = index;
    patch.selectedLayer = selectedBefore;
    PushTextureLayerHistory(state, std::move(patch));
}
void RecordTextureLayerRemoved(EditorState& state, std::size_t index,
                               core::TextureLayer layer, int selectedBefore) {
    TextureLayerHistoryPatch patch;
    patch.kind = TextureLayerHistoryPatch::Kind::Insert;
    patch.a = index;
    patch.layer = std::move(layer);
    patch.selectedLayer = selectedBefore;
    PushTextureLayerHistory(state, std::move(patch));
}
void RecordTextureLayerMove(EditorState& state, std::size_t from, std::size_t to, int selectedBefore) {
    TextureLayerHistoryPatch patch;
    patch.kind = TextureLayerHistoryPatch::Kind::Move;
    patch.a = to;
    patch.b = from;
    patch.selectedLayer = selectedBefore;
    PushTextureLayerHistory(state, std::move(patch));
}
void RecordTextureLayerMetadata(EditorState& state, std::size_t index,
                                TextureLayerMetadata before, int selectedBefore) {
    TextureLayerHistoryPatch patch;
    patch.kind = TextureLayerHistoryPatch::Kind::Metadata;
    patch.a = index;
    patch.metadata = std::move(before);
    patch.selectedLayer = selectedBefore;
    PushTextureLayerHistory(state, std::move(patch));
}
void RecordTextureStackReplacement(EditorState& state, core::TextureLayerStack before, int selectedBefore) {
    TextureLayerHistoryPatch patch;
    patch.kind = TextureLayerHistoryPatch::Kind::ReplaceStack;
    patch.stack = std::move(before);
    patch.selectedLayer = selectedBefore;
    PushTextureLayerHistory(state, std::move(patch));
}
void InvalidateMapHistoryRedo(EditorState& state) {
    state.mapHistoryRedo.clear();
    state.undo.ClearRedo();
    state.textureUndo.ClearRedo();
    state.walkUndo.ClearRedo();
    state.objectRedo.clear();
    state.textureLayerRedo.clear();
    state.environmentRedo.clear();
}
void RegisterMapHistoryAction(EditorState& state, MapHistoryDomain domain) {
    ++state.mapEditRevision;
    InvalidateMapHistoryRedo(state);
    state.mapHistoryUndo.push_back(domain);
}
void ClearMapHistory(EditorState& state) {
    state.undo.Clear();
    state.textureUndo.Clear();
    state.walkUndo.Clear();
    state.mapHistoryUndo.clear();
    state.mapHistoryRedo.clear();
    state.objectUndo.clear();
    state.objectRedo.clear();
    state.textureLayerUndo.clear();
    state.textureLayerRedo.clear();
    state.pendingObjectEdit.reset();
    state.pendingObjectEditChanged = false;
    state.environmentUndo.clear();
    state.environmentRedo.clear();
    state.pendingEnvironmentEdit.reset();
}
void PushHeightmapHistory(EditorState& state, core::UndoPatch patch) {
    if (patch.entries.empty()) return;
    RegisterMapHistoryAction(state, MapHistoryDomain::Heightmap);
    state.undo.Push(std::move(patch));
}
void PushTexturePaintHistory(EditorState& state, core::TexturePaintPatch patch) {
    if (patch.entries.empty()) return;
    RegisterMapHistoryAction(state, MapHistoryDomain::TexturePaint);
    state.textureUndo.Push(std::move(patch));
}
void PushWalkHistory(EditorState& state, core::WalkUndoPatch patch) {
    if (patch.entries.empty()) return;
    RegisterMapHistoryAction(state, MapHistoryDomain::Walk);
    state.walkUndo.Push(std::move(patch));
}
void PushObjectHistory(EditorState& state, ObjectEditSnapshot before) {
    RegisterMapHistoryAction(state, MapHistoryDomain::Objects);
    state.objectUndo.push_back(std::move(before));
}
void BeginObjectEditTransaction(EditorState& state) {
    if (state.pendingObjectEdit) return;
    state.pendingObjectEdit = CaptureObjectEditSnapshot(state);
    state.pendingObjectEditChanged = false;
}
void CommitObjectEditTransaction(EditorState& state) {
    if (!state.pendingObjectEdit) return;
    if (state.pendingObjectEditChanged)
        PushObjectHistory(state, std::move(*state.pendingObjectEdit));
    state.pendingObjectEdit.reset();
    state.pendingObjectEditChanged = false;
}
std::optional<ObjectEditSnapshot> BeginObjectMutation(EditorState& state) {
    if (state.pendingObjectEdit) return std::nullopt;
    return CaptureObjectEditSnapshot(state);
}
void FinishObjectMutation(EditorState& state, std::optional<ObjectEditSnapshot> before, bool changed) {
    if (!changed) return;
    if (state.pendingObjectEdit) state.pendingObjectEditChanged = true;
    else if (before) PushObjectHistory(state, std::move(*before));
    state.mapDirty = true;
}
static bool SameEnvironment(const core::SceneEnvironment& a, const core::SceneEnvironment& b) {
    return std::equal(std::begin(a.globalLight), std::end(a.globalLight), std::begin(b.globalLight)) &&
           std::equal(std::begin(a.fog), std::end(a.fog), std::begin(b.fog)) &&
           std::equal(std::begin(a.backgroundColor), std::end(a.backgroundColor), std::begin(b.backgroundColor)) &&
           a.frustumFar == b.frustumFar &&
           std::equal(std::begin(a.directionLightAmbient), std::end(a.directionLightAmbient), std::begin(b.directionLightAmbient)) &&
           std::equal(std::begin(a.directionLightDiffuse), std::end(a.directionLightDiffuse), std::begin(b.directionLightDiffuse));
}
void SetSceneEnvironment(EditorState& state, const core::SceneEnvironment& value) {
    if (SameEnvironment(state.placementSet.environment, value)) return;
    if (!state.pendingEnvironmentEdit) {
        RegisterMapHistoryAction(state, MapHistoryDomain::Environment);
        state.environmentUndo.push_back(state.placementSet.environment);
    } else {
        ++state.mapEditRevision;
    }
    state.placementSet.environment = value;
    state.mapDirty = true;
}
void CommitEnvironmentEdit(EditorState& state) {
    if (!state.pendingEnvironmentEdit) return;
    const core::SceneEnvironment before = *state.pendingEnvironmentEdit;
    state.pendingEnvironmentEdit.reset();
    if (SameEnvironment(before, state.placementSet.environment)) return;
    RegisterMapHistoryAction(state, MapHistoryDomain::Environment);
    state.environmentUndo.push_back(before);
}
bool CanUndoMapEdit(const EditorState& state) { return !state.mapHistoryUndo.empty(); }
bool CanRedoMapEdit(const EditorState& state) { return !state.mapHistoryRedo.empty(); }
bool UndoMapEdit(EditorState& state) {
    CommitEnvironmentEdit(state);
    if (state.mapHistoryUndo.empty()) return false;
    ++state.mapEditRevision;
    const auto domain = state.mapHistoryUndo.back();
    bool ok = false;
    switch (domain) {
        case MapHistoryDomain::Heightmap:
            ok = state.undo.Undo(state.heightmap);
            if (ok) state.meshDirty = true;
            break;
        case MapHistoryDomain::TexturePaint:
            ok = state.textureUndo.Undo(state.textureStack);
            if (ok) {
                state.layerPreviewDirty = true;
                state.renderer.UpdateBlendTextures(state.textureStack);
            }
            break;
        case MapHistoryDomain::TextureLayers:
            if (!state.textureLayerUndo.empty()) {
                auto patch = std::move(state.textureLayerUndo.back());
                state.textureLayerUndo.pop_back();
                if (auto inverse = ApplyTextureLayerHistoryPatch(state, std::move(patch))) {
                    state.textureLayerRedo.push_back(std::move(*inverse));
                    ok = true;
                }
            }
            break;
        case MapHistoryDomain::Walk:
            ok = state.walkUndo.Undo(state.walkGrid);
            if (ok) state.walkPreviewDirty = true;
            break;
        case MapHistoryDomain::Objects:
            if (!state.objectUndo.empty()) {
                auto before = std::move(state.objectUndo.back());
                state.objectUndo.pop_back();
                state.objectRedo.push_back(CaptureObjectEditSnapshot(state));
                RestoreObjectEditSnapshot(state, std::move(before));
                ok = true;
            }
            break;
        case MapHistoryDomain::Environment:
            if (!state.environmentUndo.empty()) {
                state.environmentRedo.push_back(state.placementSet.environment);
                state.placementSet.environment = state.environmentUndo.back();
                state.environmentUndo.pop_back();
                ok = true;
            }
            break;
    }
    if (!ok) return false;
    state.mapHistoryUndo.pop_back();
    state.mapHistoryRedo.push_back(domain);
    state.mapDirty = true;
    return true;
}
bool RedoMapEdit(EditorState& state) {
    if (state.mapHistoryRedo.empty()) return false;
    ++state.mapEditRevision;
    const auto domain = state.mapHistoryRedo.back();
    bool ok = false;
    switch (domain) {
        case MapHistoryDomain::Heightmap:
            ok = state.undo.Redo(state.heightmap);
            if (ok) state.meshDirty = true;
            break;
        case MapHistoryDomain::TexturePaint:
            ok = state.textureUndo.Redo(state.textureStack);
            if (ok) {
                state.layerPreviewDirty = true;
                state.renderer.UpdateBlendTextures(state.textureStack);
            }
            break;
        case MapHistoryDomain::TextureLayers:
            if (!state.textureLayerRedo.empty()) {
                auto patch = std::move(state.textureLayerRedo.back());
                state.textureLayerRedo.pop_back();
                if (auto inverse = ApplyTextureLayerHistoryPatch(state, std::move(patch))) {
                    state.textureLayerUndo.push_back(std::move(*inverse));
                    ok = true;
                }
            }
            break;
        case MapHistoryDomain::Walk:
            ok = state.walkUndo.Redo(state.walkGrid);
            if (ok) state.walkPreviewDirty = true;
            break;
        case MapHistoryDomain::Objects:
            if (!state.objectRedo.empty()) {
                auto after = std::move(state.objectRedo.back());
                state.objectRedo.pop_back();
                state.objectUndo.push_back(CaptureObjectEditSnapshot(state));
                RestoreObjectEditSnapshot(state, std::move(after));
                ok = true;
            }
            break;
        case MapHistoryDomain::Environment:
            if (!state.environmentRedo.empty()) {
                state.environmentUndo.push_back(state.placementSet.environment);
                state.placementSet.environment = state.environmentRedo.back();
                state.environmentRedo.pop_back();
                ok = true;
            }
            break;
    }
    if (!ok) return false;
    state.mapHistoryRedo.pop_back();
    state.mapHistoryUndo.push_back(domain);
    state.mapDirty = true;
    return true;
}
void SyncObjectEditorMetadata(EditorState& state) {
    state.objectEditorHidden.resize(state.placementSet.Count(), 0);
    state.objectEditorLocked.resize(state.placementSet.Count(), 0);
    state.objectEditorLabels.resize(state.placementSet.Count());
    state.objectEditorGroups.resize(state.placementSet.Count());
}
std::string ShmdEditorObjectKey(const EditorState& state, int id) {
    const auto renderIndex=ShmdRenderIndex(id);
    if(!renderIndex || *renderIndex>=state.shmdCategorySource.size() ||
       *renderIndex>=state.shmdCategoryRenderSet.Count()) return {};
    const auto [categoryIndex,pathIndex]=state.shmdCategorySource[*renderIndex];
    if(categoryIndex>=state.placementSet.categories.size()) return {};
    const auto& category=state.placementSet.categories[categoryIndex];
    if(pathIndex>=category.modelPaths.size()) return {};
    return category.name+"|"+category.modelPaths[pathIndex];
}
std::string ObjectEditorLabel(const EditorState& state, int id) {
    if (id >= 0) {
        const std::size_t index=static_cast<std::size_t>(id);
        return index<state.objectEditorLabels.size()?state.objectEditorLabels[index]:std::string{};
    }
    const std::string key=ShmdEditorObjectKey(state,id);
    if (key.empty()) return {};
    const auto it=state.shmdEditorLabels.find(key);
    return it==state.shmdEditorLabels.end()?std::string{}:it->second;
}
std::string ObjectEditorGroup(const EditorState& state, int id) {
    if (id >= 0) {
        const std::size_t index=static_cast<std::size_t>(id);
        return index<state.objectEditorGroups.size()?state.objectEditorGroups[index]:std::string{};
    }
    const std::string key=ShmdEditorObjectKey(state,id);
    if (key.empty()) return {};
    const auto it=state.shmdEditorGroups.find(key);
    return it==state.shmdEditorGroups.end()?std::string{}:it->second;
}
void SetObjectEditorLabel(EditorState& state, int id, std::string value) {
    if (id >= 0) {
        SyncObjectEditorMetadata(state);
        const std::size_t index=static_cast<std::size_t>(id);
        if(index<state.objectEditorLabels.size()) state.objectEditorLabels[index]=std::move(value);
        return;
    }
    const std::string key=ShmdEditorObjectKey(state,id);
    if(key.empty()) return;
    if(value.empty()) state.shmdEditorLabels.erase(key);
    else state.shmdEditorLabels[key]=std::move(value);
}
void SetObjectEditorGroup(EditorState& state, int id, std::string value) {
    if (id >= 0) {
        SyncObjectEditorMetadata(state);
        const std::size_t index=static_cast<std::size_t>(id);
        if(index<state.objectEditorGroups.size()) state.objectEditorGroups[index]=std::move(value);
        return;
    }
    const std::string key=ShmdEditorObjectKey(state,id);
    if(key.empty()) return;
    if(value.empty()) state.shmdEditorGroups.erase(key);
    else state.shmdEditorGroups[key]=std::move(value);
}
void TransferShmdEditorMetadataKey(EditorState& state, const std::string& oldKey,
                                   const std::string& newKey) {
    if(oldKey.empty() || newKey.empty() || oldKey==newKey) return;
    if(state.shmdEditorHiddenKeys.erase(oldKey)>0) state.shmdEditorHiddenKeys.insert(newKey);
    if(state.shmdEditorLockedKeys.erase(oldKey)>0) state.shmdEditorLockedKeys.insert(newKey);
    if(const auto it=state.shmdEditorLabels.find(oldKey);it!=state.shmdEditorLabels.end()) {
        state.shmdEditorLabels[newKey]=std::move(it->second);
        state.shmdEditorLabels.erase(it);
    }
    if(const auto it=state.shmdEditorGroups.find(oldKey);it!=state.shmdEditorGroups.end()) {
        state.shmdEditorGroups[newKey]=std::move(it->second);
        state.shmdEditorGroups.erase(it);
    }
}
bool IsObjectEditorLocked(const EditorState& state, int id) {
    if(id>=0)
        return static_cast<std::size_t>(id)<state.objectEditorLocked.size() &&
               state.objectEditorLocked[static_cast<std::size_t>(id)]!=0;
    const std::string key=ShmdEditorObjectKey(state,id);
    return !key.empty() && state.shmdEditorLockedKeys.contains(key);
}
bool IsObjectEditorHidden(const EditorState& state, int id) {
    if(id>=0)
        return static_cast<std::size_t>(id)<state.objectEditorHidden.size() &&
               state.objectEditorHidden[static_cast<std::size_t>(id)]!=0;
    const std::string key=ShmdEditorObjectKey(state,id);
    return !key.empty() && state.shmdEditorHiddenKeys.contains(key);
}

} // namespace nge
