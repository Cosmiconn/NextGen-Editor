// BrushTools.cpp
// Foliage, Prefabs, Weg-Werkzeug, Texturregeln und Pinsel in der Welt.

#include "EditorShared.hpp"

namespace nge {

// Ein Pinselschritt an einer Weltposition für Terrain, Texturen und Block&Walk - gemeinsam für
// die 2D-Ansicht und den 3D-Viewport (dort über PickTerrainFrom3D).
// Eine Anwendung des Objekt-Pinsels (Malen oder, mit Strg bzw. 'Löschen', Entfernen).
bool ScatterAtWorld(EditorState& state, float worldX, float worldZ) {
    if (!RequireWritableMapGeometry(state) || !MapSupportsTerrainEditing(state)) return false;
    const double now = ImGui::GetTime();
    if (state.scatterStrokeActive && now - state.scatterLastTime < 0.08) return false; // ca. 12 Anwendungen/s
    state.scatterLastTime = now;
    if (!state.scatterStrokeActive) {
        BeginObjectEditTransaction(state);
        state.scatterStrokeActive = true;
    }
    const bool erase = state.scatterErase || ImGui::GetIO().KeyCtrl;
    bool changed = false;
    if (erase) {
        for (const std::size_t index : core::ScatteredObjectsInRadius(state.placementSet, state.scatter.models, worldX, worldZ,
                                                                      state.scatter.radius)) {
            if (IsObjectEditorLocked(state, static_cast<int>(index))) continue;
            state.placementSet.RemoveObject(index);
            const auto at = static_cast<std::ptrdiff_t>(index);
            if (index < state.objectEditorHidden.size()) state.objectEditorHidden.erase(state.objectEditorHidden.begin() + at);
            if (index < state.objectEditorLocked.size()) state.objectEditorLocked.erase(state.objectEditorLocked.begin() + at);
            if (index < state.objectEditorLabels.size()) state.objectEditorLabels.erase(state.objectEditorLabels.begin() + at);
            if (index < state.objectEditorGroups.size()) state.objectEditorGroups.erase(state.objectEditorGroups.begin() + at);
            changed = true;
        }
        if (changed) ClearObjectSelection(state);
    } else {
        auto objs = core::ScatterObjects(state.heightmap, state.placementSet, state.scatter, worldX, worldZ, state.scatterRng);
        for (auto& o : objs) state.placementSet.AddObject(std::move(o));
        changed = !objs.empty();
        SyncObjectEditorMetadata(state);
    }
    if (!changed) return false;
    state.pendingObjectEditChanged = true;
    state.mapDirty = true;
    // Nur der Objekt-Renderer: geladene Modelle bleiben im Cache, neue Instanzen erscheinen sofort.
    state.nifMeshRenderer.LoadModelsForSet(state.placementSet, CurrentObjectAssetMapDir(state));
    state.objectVisKey.clear();
    return true;
}
std::filesystem::path PrefabDir(const EditorState& state) {
    return std::filesystem::path(state.project.projectFolder) / "Prefabs";
}
bool PlacePrefabAtWorld(EditorState& state, float worldX, float worldZ) {
    if (!RequireWritableMapGeometry(state) || state.prefabSelected.empty()) return false;
    auto prefab = core::LoadPrefab(state.prefabSelected);
    if (!prefab) {
        state.prefabStatus = prefab.error();
        return false;
    }
    auto historyBefore = BeginObjectMutation(state);
    std::vector<int> created;
    for (auto& o : core::InstantiatePrefab(*prefab, state.heightmap, worldX, worldZ, state.newObjectRotDeg))
        created.push_back(static_cast<int>(state.placementSet.AddObject(std::move(o))));
    SyncObjectEditorMetadata(state);
    state.selectedObjects = created;
    state.selectedObject = created.empty() ? kNoObjectSelection : created.front();
    state.objectGizmoMatrixValid = false;
    state.nifMeshRenderer.LoadModelsForSet(state.placementSet, CurrentObjectAssetMapDir(state));
    state.objectVisKey.clear();
    FinishObjectMutation(state, std::move(historyBefore), !created.empty());
    state.prefabStatus = prefab->name + ": " + std::to_string(created.size()) + L(" Objekte eingesetzt", " objects placed");
    return !created.empty();
}
void DrawPrefabControls(EditorState& state) {
    ImGui::SeparatorText("Prefabs");
    if (state.project.projectFolder[0] == '\0') {
        ImGui::TextDisabled("%s", L("Prefabs liegen im Projektordner - erst ein Projekt festlegen.", "Prefabs live in the project folder - set a project first."));
        return;
    }
    std::vector<int> selected;
    for (const int id : state.selectedObjects)
        if (id >= 0 && static_cast<std::size_t>(id) < state.placementSet.Count()) selected.push_back(id);
    ImGui::SetNextItemWidth(160.0f);
    UI::InputText(L("Name##prefab", "Name##prefab"), state.prefabName, sizeof(state.prefabName));
    ImGui::SameLine();
    ImGui::BeginDisabled(selected.empty() || state.prefabName[0] == '\0');
    if (UI::Button(L("Auswahl speichern##prefab", "Save selection##prefab"))) {
        std::string name = state.prefabName;
        for (char& c : name) if (c == '/' || c == '\\' || c == ':' || c == '.') c = '_';
        std::vector<core::PlacedObject> objects;
        for (const int id : selected) objects.push_back(state.placementSet.At(static_cast<std::size_t>(id)));
        const auto file = PrefabDir(state) / (name + ".prefab");
        auto r = core::SavePrefab(core::MakePrefab(name, objects, state.heightmap), file);
        state.prefabStatus = r ? name + ": " + std::to_string(objects.size()) + L(" Objekte gespeichert", " objects saved") : r.error();
        if (r) state.prefabSelected = file.string();
    }
    ImGui::EndDisabled();
    const auto files = core::ListPrefabs(PrefabDir(state));
    if (files.empty()) ImGui::TextDisabled("%s", L("Noch keine Prefabs: Objekte auswählen, Namen eingeben, speichern.",
                                                    "No prefabs yet: select objects, enter a name, save."));
    for (const auto& f : files) {
        const bool active = state.prefabSelected == f.string();
        if (ImGui::Selectable((f.stem().string() + "##prefab" + f.string()).c_str(), active && state.objectPlaceMode == 3)) {
            state.prefabSelected = f.string();
            state.objectPlaceMode = 3;
        }
    }
    if (state.objectPlaceMode == 3) {
        UI::SliderFloat(L("Drehung (°)##prefab", "Rotation (°)##prefab"), &state.newObjectRotDeg, -180.0f, 180.0f);
        ImGui::TextDisabled("%s", L("Klick in 2D oder 3D setzt das Prefab; Höhen folgen dem Terrain.",
                                    "Click in 2D or 3D places the prefab; heights follow the terrain."));
    }
    if (!state.prefabStatus.empty()) ImGui::TextDisabled("%s", state.prefabStatus.c_str());
}
void DrawSplineControls(EditorState& state) {
    ImGui::SeparatorText(L("Weg (Spline)", "Path (spline)"));
    UI::Checkbox(L("Punkte setzen (Klick in 2D/3D)", "Place points (click in 2D/3D)"), &state.splineEdit);
    ImGui::Text(L("%zu Punkte", "%zu points"), state.splinePoints.size());
    ImGui::SameLine();
    if (UI::SmallButton(L("Letzten entfernen##spline", "Remove last##spline")) && !state.splinePoints.empty()) state.splinePoints.pop_back();
    ImGui::SameLine();
    if (UI::SmallButton(L("Alle löschen##spline", "Clear##spline"))) state.splinePoints.clear();
    UI::SliderFloat(L("Breite##spline", "Width##spline"), &state.splineWidth.width, 20.0f, 1000.0f);
    UI::SliderFloat(L("Weicher Rand##spline", "Soft edge##spline"), &state.splineWidth.falloff, 0.0f, 500.0f);
    const bool ready = state.splinePoints.size() >= 2 && MapSupportsTerrainEditing(state);
    const auto samples = ready ? core::SampleSpline(state.splinePoints, 25.0f) : std::vector<core::SplineSample>{};
    if (ready) ImGui::TextDisabled(L("Länge %.0f Einheiten", "Length %.0f units"), samples.empty() ? 0.0f : samples.back().distance);

    ImGui::BeginDisabled(!ready);
    UI::SliderFloat(L("Glättung der Weghöhe##spline", "Path height smoothing##spline"), &state.splineSmooth, 0.0f, 2000.0f);
    if (UI::Button(L("Terrain entlang ebnen", "Flatten terrain along path"))) {
        auto patch = core::FlattenAlongSpline(state.heightmap, samples, state.splineWidth, state.splineSmooth);
        const std::size_t n = patch.entries.size();
        if (n) { PushHeightmapHistory(state, std::move(patch)); state.meshDirty = true; state.mapDirty = true; }
        state.splineStatus = std::to_string(n) + L(" Höhenpunkte geändert", " height points changed");
    }
    // Layer zum Malen
    if (state.splineLayer < 0 || static_cast<std::size_t>(state.splineLayer) >= state.textureStack.LayerCount())
        state.splineLayer = state.textureStack.LayerCount() > 0 ? 0 : -1;
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo(L("Layer##spline", "Layer##spline"), state.splineLayer >= 0 ? state.textureStack.Layer(static_cast<std::size_t>(state.splineLayer)).name.c_str() : "-")) {
        for (std::size_t i = 0; i < state.textureStack.LayerCount(); ++i)
            if (ImGui::Selectable(state.textureStack.Layer(i).name.c_str(), state.splineLayer == static_cast<int>(i))) state.splineLayer = static_cast<int>(i);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(state.splineLayer < 0 || TextureLayerLocked(state, state.splineLayer));
    if (UI::Button(L("Layer malen##spline", "Paint layer##spline"))) {
        auto patch = core::PaintAlongSpline(state.textureStack, static_cast<std::size_t>(state.splineLayer), state.heightmap, samples, state.splineWidth);
        const std::size_t n = patch.entries.size();
        if (n) {
            PushTexturePaintHistory(state, std::move(patch));
            state.renderer.UpdateBlendTextures(state.textureStack);
            state.layerPreviewDirty = true;
            state.mapDirty = true;
        }
        state.splineStatus = std::to_string(n) + L(" Maskenpixel gemalt", " mask pixels painted");
    }
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(-1.0f);
    UI::InputText("##splineModel", state.splineModel, sizeof(state.splineModel));
    ImGui::TextDisabled("%s", L("Modell für Objekte entlang des Wegs (z.B. resmap\\nifs\\Common\\FenceWood1_1.nif)",
                                "Model for objects along the path (e.g. resmap\\nifs\\Common\\FenceWood1_1.nif)"));
    auto& so = state.splineObjects;
    UI::SliderFloat(L("Abstand##splineObj", "Spacing##splineObj"), &so.spacing, 20.0f, 2000.0f);
    UI::SliderFloat(L("Seitlicher Versatz##splineObj", "Side offset##splineObj"), &so.sideOffset, 0.0f, 1000.0f);
    int side = static_cast<int>(so.side);
    ImGui::RadioButton(L("Mitte##splineSide", "Centre##splineSide"), &side, 0); ImGui::SameLine();
    ImGui::RadioButton(L("Links##splineSide", "Left##splineSide"), &side, 1); ImGui::SameLine();
    ImGui::RadioButton(L("Rechts##splineSide", "Right##splineSide"), &side, 2); ImGui::SameLine();
    ImGui::RadioButton(L("Beide##splineSide", "Both##splineSide"), &side, 3);
    so.side = static_cast<core::SplineSide>(side);
    UI::SliderFloat(L("Zusatzdrehung (°)##splineObj", "Extra rotation (°)##splineObj"), &so.yawOffsetDegrees, -180.0f, 180.0f);
    UI::SliderFloat(L("Skalierung##splineObj", "Scale##splineObj"), &so.scale, 0.1f, 5.0f);
    ImGui::BeginDisabled(state.splineModel[0] == '\0');
    if (UI::Button(L("Objekte entlang setzen", "Place objects along path"))) {
        so.model = state.splineModel;
        auto objs = core::ObjectsAlongSpline(state.splinePoints, state.heightmap, so);
        auto historyBefore = BeginObjectMutation(state);
        for (auto& o : objs) state.placementSet.AddObject(std::move(o));
        SyncObjectEditorMetadata(state);
        state.nifMeshRenderer.LoadModelsForSet(state.placementSet, CurrentObjectAssetMapDir(state));
        state.objectVisKey.clear();
        FinishObjectMutation(state, std::move(historyBefore), !objs.empty());
        state.splineStatus = std::to_string(objs.size()) + L(" Objekte gesetzt", " objects placed");
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (!state.splineStatus.empty()) ImGui::TextDisabled("%s", state.splineStatus.c_str());
}
void DrawSpline2D(const EditorState& state, ImDrawList* drawList, const ImVec2& origin, const ImVec2& size, float spanX, float spanZ) {
    if (state.splinePoints.empty() || spanX <= 0.0f || spanZ <= 0.0f) return;
    const auto toScreen = [&](float x, float z) { return ImVec2(origin.x + x / spanX * size.x, origin.y + (1.0f - z / spanZ) * size.y); };
    const auto samples = core::SampleSpline(state.splinePoints, 25.0f);
    for (std::size_t i = 1; i < samples.size(); ++i)
        drawList->AddLine(toScreen(samples[i - 1].x, samples[i - 1].z), toScreen(samples[i].x, samples[i].z), IM_COL32(255, 210, 60, 230), 2.0f);
    for (const auto& p : state.splinePoints) drawList->AddCircleFilled(toScreen(p.x, p.z), 4.0f, IM_COL32(255, 240, 120, 255));
}
void FinishScatterStrokeIfReleased(EditorState& state) {
    if (!state.scatterStrokeActive || ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
    state.scatterStrokeActive = false;
    CommitObjectEditTransaction(state);
}
void DrawScatterControls(EditorState& state) {
    ImGui::SeparatorText(L("Objekte malen", "Paint objects"));
    auto& sc = state.scatter;
    for (std::size_t i = 0; i < sc.models.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (UI::SmallButton("x")) {
            sc.models.erase(sc.models.begin() + static_cast<std::ptrdiff_t>(i));
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(std::filesystem::path(core::legacy::LegacyPathToNative(sc.models[i])).filename().string().c_str());
        ImGui::PopID();
    }
    if (sc.models.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", L("Noch kein Modell - oben eines wählen und hinzufügen.",
                                                                   "No model yet - choose one above and add it."));
    ImGui::BeginDisabled(state.newObjectModelPath[0] == '\0');
    if (UI::Button(L("Modellpfad oben hinzufügen", "Add model path above"))) {
        const std::string model = state.newObjectModelPath;
        if (std::find(sc.models.begin(), sc.models.end(), model) == sc.models.end()) sc.models.push_back(model);
    }
    ImGui::EndDisabled();
    UI::SliderFloat(L("Radius##scatter", "Radius##scatter"), &sc.radius, 20.0f, 2000.0f);
    UI::SliderFloat(L("Dichte (je 100x100)##scatter", "Density (per 100x100)##scatter"), &sc.density, 0.1f, 20.0f);
    UI::SliderFloat(L("Mindestabstand##scatter", "Min. distance##scatter"), &sc.minDistance, 0.0f, 500.0f);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::DragFloatRange2(L("Skalierung##scatter", "Scale##scatter"), &sc.minScale, &sc.maxScale, 0.01f, 0.05f, 10.0f, "%.2f", "%.2f");
    UI::SliderFloat(L("Max. Neigung (°)##scatter", "Max. slope (°)##scatter"), &sc.maxSlope, 0.0f, 90.0f);
    UI::SliderFloat(L("Höhenversatz##scatter", "Height offset##scatter"), &sc.heightOffset, -100.0f, 100.0f);
    UI::Checkbox(L("Zufällige Drehung##scatter", "Random rotation##scatter"), &sc.randomYaw);
    ImGui::SameLine();
    UI::Checkbox(L("Am Hang ausrichten##scatter", "Align to slope##scatter"), &sc.alignToSlope);
    UI::Checkbox(L("Löschen statt malen (oder Strg halten)##scatter", "Erase instead of paint (or hold Ctrl)##scatter"), &state.scatterErase);
    ImGui::TextDisabled("%s", L("Ziehen im 2D- oder 3D-View; ein Strich = ein Undo-Schritt. Löschen entfernt nur Objekte der Liste.",
                                "Drag in the 2D or 3D view; one stroke = one undo step. Erase removes only objects of the list."));
}
bool TextureLayerLocked(const EditorState& state, int layer) {
    return layer >= 0 && static_cast<std::size_t>(layer) < state.textureStack.LayerCount() &&
           state.lockedTextureLayers.contains(state.textureStack.Layer(static_cast<std::size_t>(layer)).name);
}
// Pinselform aus einem Graustufenbild (PNG 8/16 Bit oder R16) laden.
bool LoadBrushAlpha(EditorState& state, const std::filesystem::path& file) {
    auto img = core::ImportHeightImage(file);
    if (!img) {
        state.brushAlphaStatus = img.error();
        return false;
    }
    state.brushAlpha.width = img->width;
    state.brushAlpha.height = img->height;
    state.brushAlpha.values.resize(img->values.size());
    for (std::size_t i = 0; i < img->values.size(); ++i) state.brushAlpha.values[i] = static_cast<float>(img->values[i]) / 65535.0f;
    state.brushAlphaEnabled = true;
    state.brushAlphaStatus = std::to_string(img->width) + "x" + std::to_string(img->height) + L(" geladen", " loaded");
    return true;
}
void DrawBrushAlphaControls(EditorState& state, const char* id) {
    ImGui::PushID(id);
    ImGui::SeparatorText(L("Pinselform aus Bild", "Brush shape from image"));
    UI::Checkbox(L("Bildform statt Falloff", "Image shape instead of falloff"), &state.brushAlphaEnabled);
    ImGui::SetNextItemWidth(-90.0f);
    UI::InputText("##brushAlphaPath", state.brushAlphaPath, sizeof(state.brushAlphaPath));
    ImGui::SameLine();
    if (UI::Button(L("Laden", "Load")) && state.brushAlphaPath[0] != '\0') LoadBrushAlpha(state, state.brushAlphaPath);
    ImGui::TextDisabled("%s", L("Graustufen-PNG (8/16 Bit) oder R16; weiß = volle Wirkung, Bildoberkante = Norden.",
                                "Grayscale PNG (8/16 bit) or R16; white = full effect, top edge = north."));
    if (!state.brushAlphaStatus.empty()) ImGui::TextDisabled("%s", state.brushAlphaStatus.c_str());
    ImGui::PopID();
}
void DrawTextureRuleControls(EditorState& state) {
    ImGui::SeparatorText(L("Nach Regeln texturieren", "Texture by rules"));
    auto& r = state.texRule;
    UI::Checkbox(L("Neigung##texRule", "Slope##texRule"), &r.useSlope);
    if (r.useSlope) {
        ImGui::SetNextItemWidth(200.0f);
        ImGui::DragFloatRange2(L("Grad##texRuleSlope", "Degrees##texRuleSlope"), &r.minSlope, &r.maxSlope, 0.5f, 0.0f, 90.0f, "%.0f°", "%.0f°");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat(L("Übergang##texRuleSlopeF", "Feather##texRuleSlopeF"), &r.slopeFeather, 0.0f, 20.0f, "%.0f°");
    }
    UI::Checkbox(L("Höhe##texRule", "Height##texRule"), &r.useHeight);
    if (r.useHeight) {
        ImGui::SetNextItemWidth(200.0f);
        ImGui::DragFloatRange2(L("Einheiten##texRuleHeight", "Units##texRuleHeight"), &r.minHeight, &r.maxHeight, 5.0f, -10000.0f, 10000.0f, "%.0f", "%.0f");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat(L("Übergang##texRuleHeightF", "Feather##texRuleHeightF"), &r.heightFeather, 0.0f, 500.0f, "%.0f");
    }
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat(L("Gewicht##texRule", "Weight##texRule"), &r.strength, 0.0f, 1.0f, "%.2f");
    int mode = r.mode == core::TextureRuleMode::Replace ? 1 : 0;
    ImGui::RadioButton(L("Nur hinzufügen##texRuleMode", "Add only##texRuleMode"), &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton(L("Ersetzen##texRuleMode", "Replace##texRuleMode"), &mode, 1);
    r.mode = mode == 1 ? core::TextureRuleMode::Replace : core::TextureRuleMode::Raise;
    UI::Checkbox(L("Pinsel malt nach dieser Regel", "Brush paints by this rule"), &state.texRuleBrush);
    const bool layerOk = state.selectedLayer >= 0 && static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount();
    ImGui::BeginDisabled(!layerOk || TextureLayerLocked(state, state.selectedLayer));
    if (UI::Button(L("Auf ganze Karte anwenden (ein Undo-Schritt)", "Apply to whole map (one undo step)"))) {
        auto patch = core::ApplyTextureRule(state.textureStack, static_cast<std::size_t>(state.selectedLayer), state.heightmap, r, {});
        const std::size_t changed = patch.entries.size();
        if (changed) {
            PushTexturePaintHistory(state, std::move(patch));
            state.renderer.UpdateBlendTextures(state.textureStack);
            state.layerPreviewDirty = true;
            state.mapDirty = true;
        }
        state.texRuleStatus = std::to_string(changed) + L(" Maskenpixel geändert", " mask pixels changed");
    }
    ImGui::EndDisabled();
    if (!state.texRuleStatus.empty()) ImGui::TextDisabled("%s", state.texRuleStatus.c_str());
}
bool ApplyBrushAtWorld(EditorState& state, float worldX, float worldZ) {
    const bool texMode = state.editMode == EditMode::TexturePaint;
    const bool walkMode = state.editMode == EditMode::BlockWalk;
    const core::BrushAlpha* alpha = state.brushAlphaEnabled && !state.brushAlpha.Empty() ? &state.brushAlpha : nullptr;
    state.brush.alpha = alpha;
    state.paintSettings.alpha = alpha;
    if (state.editMode == EditMode::ObjectPlacement && state.objectPlaceMode == 2) return ScatterAtWorld(state, worldX, worldZ);
    if (state.editMode == EditMode::Heightmap && state.splineEdit) {
        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return false;
        state.splinePoints.push_back({worldX, worldZ});
        return true;
    }
    if (texMode && TextureLayerLocked(state, state.selectedLayer)) {
        state.statusMessage = L("Layer ist gesperrt.", "Layer is locked.");
        return false;
    }
    if (texMode && state.texRuleBrush && state.selectedLayer >= 0 &&
        static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount()) {
        core::TextureRuleArea area;
        area.wholeMap = false;
        area.centerX = worldX;
        area.centerZ = worldZ;
        area.radius = state.paintSettings.radius;
        auto patch = core::ApplyTextureRule(state.textureStack, static_cast<std::size_t>(state.selectedLayer), state.heightmap,
                                            state.texRule, area);
        if (patch.entries.empty()) return false;
        PushTexturePaintHistory(state, std::move(patch));
        state.renderer.UpdateBlendTextures(state.textureStack);
        state.layerPreviewDirty = true;
        state.mapDirty = true;
        return true;
    }
    if (texMode && (state.selectedLayer < 0 || static_cast<std::size_t>(state.selectedLayer) >= state.textureStack.LayerCount()))
        return false;
    const float spanX = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth();
    const float spanZ = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight();
    if (state.editMode == EditMode::Heightmap) {
        const bool strokeStart = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (state.brushMode == core::BrushMode::Ramp) {
            // Rampe: nur Klicks zählen, kein Ziehen.
            if (!strokeStart) return false;
            const float h = state.heightmap.SampleWorld(worldX, worldZ);
            if (!state.rampStart) {
                state.rampStart = std::array<float, 3>{worldX, h, worldZ};
                state.statusMessage = L("Rampenstart gesetzt - zweiter Klick setzt das Ende.",
                                        "Ramp start set - a second click sets the end.");
                return false;
            }
            const auto start = *state.rampStart;
            state.rampStart.reset();
            core::UndoPatch patch = core::ApplyRamp(state.heightmap, start[0], start[2], start[1], worldX, worldZ, h,
                                                    state.brush.radius, state.brush.radius * 0.5f);
            if (!patch.entries.empty()) {
                PushHeightmapHistory(state, std::move(patch));
                state.meshDirty = true;
                state.mapDirty = true;
            }
            return true;
        }
        if (strokeStart && state.brushMode == core::BrushMode::Flatten && state.flattenPickOnStroke)
            state.brush.flattenTarget = state.heightmap.SampleWorld(worldX, worldZ);
        core::UndoPatch patch = core::ApplyBrush(state.heightmap, state.brushMode, state.brush, worldX, worldZ);
        if (!patch.entries.empty()) {
            PushHeightmapHistory(state, std::move(patch));
            state.meshDirty = true;
            state.mapDirty = true;
        }
    } else if (texMode) {
        // Eigene Zellgröße des Textur-Gitters (unabhängige Auflösung, siehe
        // docs/MAP_FORMAT.md) statt der Heightmap-Blockgröße - sonst wäre der Pinsel bei
        // z.B. 512x512-Textur-Layern auf einer 257x257-Heightmap deutlich daneben.
        float texCellW = spanX / static_cast<float>(state.textureStack.Width());
        float texCellH = spanZ / static_cast<float>(state.textureStack.Height());
        // Layer mit eigener Region (z.B. Adl: linke/rechte Kartenhaelfte, siehe
        // TextureLayer::regionStartX): Welt-Position in die Region umrechnen, das Gitter
        // deckt nur die Region ab; nur Layer mit derselben Region nehmen an der
        // (früheren) Normalisierung teil. Ohne Region (0) bleibt alles wie bisher.
        float paintX = worldX, paintZ = worldZ;
        core::TexturePaintSettings paintSettings = state.paintSettings;
        std::vector<char> sameRegion;
        if (state.selectedLayer >= 0 && static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount()) {
            const auto& active = state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer));
            if (active.regionWidth > 0.0f && active.regionHeight > 0.0f) {
                const float bw = state.heightmap.BlockWidth(), bh = state.heightmap.BlockHeight();
                paintX = worldX - active.regionStartX * bw;
                paintZ = worldZ - active.regionStartY * bh;
                texCellW = active.regionWidth * bw / static_cast<float>(state.textureStack.Width());
                texCellH = active.regionHeight * bh / static_cast<float>(state.textureStack.Height());
            }
            sameRegion.assign(state.textureStack.LayerCount(), 0);
            for (std::size_t li = 0; li < state.textureStack.LayerCount(); ++li) {
                const auto& other = state.textureStack.Layer(li);
                sameRegion[li] = other.regionStartX == active.regionStartX && other.regionStartY == active.regionStartY &&
                                 other.regionWidth == active.regionWidth && other.regionHeight == active.regionHeight;
            }
            paintSettings.participating = &sameRegion;
        }
        core::TexturePaintPatch patch = core::PaintLayerWeight(
            state.textureStack, static_cast<std::size_t>(state.selectedLayer), state.paintMode,
            paintSettings, paintX, paintZ, texCellW, texCellH);
        if (!patch.entries.empty()) {
            PushTexturePaintHistory(state, std::move(patch));
            state.layerPreviewDirty = true;
            state.mapDirty = true;
            state.renderer.UpdateBlendTextures(state.textureStack);
        }
    } else if (walkMode) {
        // Zell-genauer Stempel (Zelle = 6.25 Einheiten, Bit gesetzt = blockiert) - siehe
        // WalkGrid::kCellSize; die Weltposition entspricht direkt der Zellposition.
        std::uint32_t changed[4] = {0, 0, 0, 0};
        core::WalkUndoPatch patch = core::ApplyWalkBitStamp(
            state.walkGrid, state.walkSettings.radius, worldX, worldZ, state.walkBlockMode, changed);
        if (!patch.entries.empty()) {
            PushWalkHistory(state, std::move(patch));
            state.mapDirty = true;
            if (!state.walkPreviewDirty) UpdateWalkPreviewRect(state, changed[0], changed[1], changed[2], changed[3]);
        }
    }
    return true;
}

} // namespace nge
