// EnvironmentPanel.cpp
// Umgebung einer Karte bearbeiten: die Szene-Parameter der SHMD (GlobalLight, DirectionLight-
// Ambient/-Diffuse, Fog, BackGroundColor, Frustum) und die Modelllisten Sky/Water/GroundObject.
// Die 3D-Ansicht liest die Werte jedes Bild neu (SceneLightingFromEnvironment), Änderungen sind
// also sofort sichtbar. Gespeichert wird mit der Karte über SerializeLegacyShmd.

#include "EditorShared.hpp"

namespace nge {
namespace {

// Beschriftung über dem Regler: das Panel ist schmal, Beschriftungen rechts würden abgeschnitten.
// Der Regler nutzt die volle Breite; der Tooltip kommt aus dem Handbuch (Schlüssel = id).
void EnvLabel(const char* text) {
    ImGui::TextWrapped("%s", text);
    ImGui::SetNextItemWidth(-1.0f);
}
// Farbregler. HDR: GlobalLight/Diffuse dürfen über 1 liegen.
bool EnvColor(const char* text, const char* id, float* rgb) {
    EnvLabel(text);
    const bool changed = ImGui::ColorEdit3(id, rgb, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    UI::Tip("ColorEdit3", id);
    return changed;
}

void DrawCategoryLists(EditorState& state) {
    auto& categories = state.placementSet.categories;
    if (categories.empty()) {
        ImGui::TextDisabled("%s", L("Diese SHMD enthält keine Modelllisten.", "This SHMD has no model lists."));
        return;
    }
    std::optional<std::pair<std::size_t, std::size_t>> remove;
    std::optional<std::size_t> addTo;
    for (std::size_t c = 0; c < categories.size(); ++c) {
        const auto& category = categories[c];
        ImGui::PushID(static_cast<int>(c));
        const std::string header = category.name + " (" + std::to_string(category.modelPaths.size()) + ")";
        if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (std::size_t p = 0; p < category.modelPaths.size(); ++p) {
                ImGui::PushID(static_cast<int>(p));
                if (UI::SmallButton("X##envModel")) remove = std::make_pair(c, p);
                ImGui::SameLine();
                ImGui::TextUnformatted(category.modelPaths[p].c_str());
                ImGui::PopID();
            }
            if (UI::SmallButton(L("Modell hinzufügen##envModel", "Add model##envModel"))) addTo = c;
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    UI::InputTextWithHint("##envModelPath", L("resmap\\field\\<Karte>\\<Modell>.nif", "resmap\\field\\<map>\\<model>.nif"),
                          state.environmentModelPath, sizeof(state.environmentModelPath));

    if (!remove && !addTo) return;
    if (!RequireWritableMapGeometry(state)) return;
    std::string path = state.environmentModelPath;
    if (addTo && path.empty()) {
        state.statusMessage = L("Erst einen Modellpfad eingeben.", "Enter a model path first.");
        return;
    }
    auto before = BeginObjectMutation(state);
    if (remove) {
        auto& paths = categories[remove->first].modelPaths;
        paths.erase(paths.begin() + static_cast<std::ptrdiff_t>(remove->second));
    } else {
        categories[*addTo].modelPaths.push_back(path);
        state.environmentModelPath[0] = '\0';
    }
    RebuildShmdCategoryRenderSet(state, CurrentObjectAssetMapDir(state));
    FinishObjectMutation(state, std::move(before));
}

} // namespace

void DrawEnvironmentPanel(EditorState& state) {
    DrawPanelHeader("environmentHeader", L("UMGEBUNG", "ENVIRONMENT"), DrawIconGlobe, "world.light",
                    L("Licht, Nebel, Himmel (SHMD)", "Light, fog, sky (SHMD)"));
    if (!state.sceneEnvironmentFromShmd) {
        ImGui::TextWrapped("%s", L("Für diese Karte ist keine SHMD geladen. Licht, Nebel und Himmel stehen in der "
                                   "<Karte>.shmd; ohne sie gibt es nichts zu bearbeiten.",
                                   "No SHMD is loaded for this map. Light, fog and sky live in <map>.shmd; "
                                   "without it there is nothing to edit."));
        return;
    }

    core::SceneEnvironment env = state.placementSet.environment;
    bool changed = false;

    ImGui::SeparatorText(L("Licht", "Light"));
    changed |= EnvColor(L("GlobalLight (Umgebungslicht)", "GlobalLight (ambient light)"), "##envGlobalLight", env.globalLight);
    if (state.placementSet.hasLightingFooter) {
        changed |= EnvColor(L("DirectionLightAmbient (zusätzliches Umgebungslicht)", "DirectionLightAmbient (extra ambient)"),
                            "##envDirAmbient", env.directionLightAmbient);
        changed |= EnvColor(L("DirectionLightDiffuse (Sonnenfarbe)", "DirectionLightDiffuse (sun color)"),
                            "##envDirDiffuse", env.directionLightDiffuse);
    } else {
        ImGui::TextDisabled("%s", L("Diese SHMD hat keinen DirectionLight-Abschnitt.", "This SHMD has no DirectionLight section."));
    }

    ImGui::SeparatorText(L("Nebel und Sichtweite", "Fog and view distance"));
    EnvLabel(L("Nebeltiefe (Anteil der Sichtweite)", "Fog depth (share of view distance)"));
    changed |= UI::SliderFloat("##envFogDepth", &env.fog[0], 0.0f, 1.0f, "%.3f");
    changed |= EnvColor(L("Nebelfarbe", "Fog color"), "##envFogColor", &env.fog[1]);
    EnvLabel(L("Sichtweite (Frustum)", "View distance (frustum)"));
    changed |= UI::DragFloat("##envFrustum", &env.frustumFar, 10.0f, 100.0f, 100000.0f, "%.0f");
    if (UI::SmallButton(L("Nebelfarbe = Hintergrund##env", "Fog color = background##env"))) {
        std::copy(std::begin(env.backgroundColor), std::end(env.backgroundColor), env.fog + 1);
        changed = true;
    }

    ImGui::SeparatorText(L("Himmel", "Sky"));
    changed |= EnvColor(L("Hintergrundfarbe (BackGroundColor)", "Background color (BackGroundColor)"), "##envBackground",
                        env.backgroundColor);

    if (changed) {
        // Ein Reglerzug bzw. eine Texteingabe ergibt genau einen Undo-Schritt.
        if (!state.pendingEnvironmentEdit && ImGui::IsAnyItemActive())
            state.pendingEnvironmentEdit = state.placementSet.environment;
        SetSceneEnvironment(state, env);
    }
    if (state.pendingEnvironmentEdit && !ImGui::IsAnyItemActive()) CommitEnvironmentEdit(state);

    if (UI::Button(L("Werte beim Öffnen wiederherstellen##env", "Restore values at open##env"), ImVec2(-1.0f, 0.0f)))
        SetSceneEnvironment(state, state.environmentAtLoad);

    ImGui::SeparatorText(L("Vorschau in der 3D-Ansicht", "Preview in the 3D view"));
    bool preview = false;
    preview |= UI::Checkbox(L("Kartenlicht##env", "Map lighting##env"), &state.viewportMapLighting);
    preview |= UI::Checkbox(L("Hintergrundfarbe##envPreview", "Background color##envPreview"), &state.viewportMapBackground);
    preview |= UI::Checkbox(L("Nebel##env", "Fog##env"), &state.viewportFog);
    if (preview) state.viewportSettingsDirty = true;

    ImGui::SeparatorText("Sky / Water / GroundObject");
    DrawCategoryLists(state);
}

} // namespace nge
