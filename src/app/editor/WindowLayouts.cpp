// WindowLayouts.cpp
// Fenster-Layouts: die Interface-NIFs unter resmenu (NA2016: 310 Dateien, Gamebryo 20.0.0.4) als
// 2D-Ansicht. Jedes Fensterelement ist ein texturiertes NiTriShape in der X/Z-Ebene (Y = 0, Z nach
// unten, Einheit = Bildschirmpixel); Teile ohne Textur sind Platzhalter für Inhalte, die der Client
// selbst zeichnet (Text, Slots). Bearbeitbar sind die Texturen: ganz ersetzen oder nur den
// UV-Ausschnitt eines Elements. Positionen und Größen werden nicht geschrieben - dafür fehlt ein
// geprüfter NIF-Writer.

#include "EditorShared.hpp"

namespace nge {

namespace {
struct LayoutCache {
    std::string key;
    std::optional<core::NifModel> model;
    std::string error;
};

std::vector<std::string> LayoutFiles(EditorState& state) {
    static std::string cachedKey;
    static std::vector<std::string> files;
    const std::string key = ResmenuSourceRoot(state).string() + "|" + std::to_string(state.resmenuRevision);
    if (key == cachedKey) return files;
    cachedKey = key;
    files.clear();
    std::set<std::string> seen;
    for (const auto& root : {ResmenuSourceRoot(state), InterfaceProjectOverrideRoot(state)}) {
        if (root.empty()) continue;
        for (const auto& rel : ListFilesByExtension(root, {".nif"}))
            if (seen.insert(LowerAscii(rel)).second) files.push_back(rel);
    }
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) { return LowerAscii(a) < LowerAscii(b); });
    return files;
}

const LayoutCache& Layout(EditorState& state, const std::string& rel) {
    static LayoutCache cache;
    const auto path = ResmenuEffectivePath(state, rel);
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(path, ec);
    const std::string key = path.string() + "|" + std::to_string(ec ? 0 : stamp.time_since_epoch().count());
    if (cache.key == key) return cache;
    cache = {};
    cache.key = key;
    if (auto model = core::LoadNifMesh(path)) cache.model = std::move(*model);
    else cache.error = model.error();
    return cache;
}

// Textur eines Elements: zuerst im Ordner des NIF, sonst irgendwo unter resmenu (Dateiname, Groß/Klein egal).
std::string ResolveTexture(EditorState& state, const std::string& nifRel, const std::string& texture) {
    if (texture.empty()) return {};
    static std::string indexKey;
    static std::unordered_map<std::string, std::vector<std::string>> byName;
    const std::string key = ResmenuSourceRoot(state).string() + "|" + std::to_string(state.resmenuRevision);
    if (key != indexKey) {
        indexKey = key;
        byName.clear();
        for (const auto& root : {ResmenuSourceRoot(state), InterfaceProjectOverrideRoot(state)}) {
            if (root.empty()) continue;
            for (const auto& rel : ListFilesByExtension(root, {".tga", ".dds", ".png", ".bmp", ".jpg"})) {
                auto& list = byName[LowerAscii(std::filesystem::path(rel).filename().string())];
                if (std::none_of(list.begin(), list.end(), [&](const std::string& r) { return LowerAscii(r) == LowerAscii(rel); }))
                    list.push_back(rel);
            }
        }
    }
    const auto it = byName.find(LowerAscii(std::filesystem::path(texture).filename().string()));
    if (it == byName.end()) return {};
    const std::string dir = LowerAscii(std::filesystem::path(nifRel).parent_path().generic_string());
    for (const auto& rel : it->second)
        if (LowerAscii(std::filesystem::path(rel).parent_path().generic_string()) == dir) return rel;
    return it->second.front();
}

struct Bounds {
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    void Add(float x, float y) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }
    [[nodiscard]] bool Valid() const { return maxX >= minX && maxY >= minY; }
};

Bounds PartBounds(const core::NifMeshPart& part) {
    Bounds b;
    for (const auto& p : part.positions) b.Add(p.x, p.z);
    return b;
}

ImU32 PartColor(const core::NifMeshPart& part, std::size_t vertex) {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = part.material.alpha;
    if (vertex < part.vertexColors.size()) {
        r = part.vertexColors[vertex].r;
        g = part.vertexColors[vertex].g;
        b = part.vertexColors[vertex].b;
        a *= part.vertexColors[vertex].a;
    }
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, std::clamp(a, 0.0f, 1.0f)));
}
} // namespace

// UV-Rechteck eines Elements in Texturpixeln (gerundet), für "nur diesen Ausschnitt ersetzen".
bool NifPartUvRect(const core::NifMeshPart& part, std::uint32_t texW, std::uint32_t texH, std::uint32_t& x, std::uint32_t& y,
                std::uint32_t& w, std::uint32_t& h) {
    if (part.uvs.empty() || texW == 0 || texH == 0) return false;
    float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
    for (const auto& t : part.uvs) {
        u0 = std::min(u0, t.u);
        v0 = std::min(v0, t.v);
        u1 = std::max(u1, t.u);
        v1 = std::max(v1, t.v);
    }
    u0 = std::clamp(u0, 0.0f, 1.0f);
    v0 = std::clamp(v0, 0.0f, 1.0f);
    u1 = std::clamp(u1, 0.0f, 1.0f);
    v1 = std::clamp(v1, 0.0f, 1.0f);
    x = static_cast<std::uint32_t>(std::floor(u0 * static_cast<float>(texW) + 0.01f));
    y = static_cast<std::uint32_t>(std::floor(v0 * static_cast<float>(texH) + 0.01f));
    const auto x1 = static_cast<std::uint32_t>(std::ceil(u1 * static_cast<float>(texW) - 0.01f));
    const auto y1 = static_cast<std::uint32_t>(std::ceil(v1 * static_cast<float>(texH) - 0.01f));
    if (x1 <= x || y1 <= y) return false;
    w = x1 - x;
    h = y1 - y;
    return true;
}

void DrawWindowLayoutViewer(EditorState& state) {
    if (ResmenuSourceRoot(state).empty()) {
        ImGui::TextDisabled("%s", L("Kein Client/resmenu gefunden.", "No Client/resmenu found."));
        return;
    }
    const auto files = LayoutFiles(state);
    DrawDataHistoryButtons(state, "layoutHistory");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu %s", files.size(), L("Interface-NIFs", "interface NIFs"));
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "%s",
                       L("Nur Texturen werden geändert; Positionen/Größen schreibt der Editor nicht (kein geprüfter NIF-Writer).",
                         "Only textures are changed; the editor does not write positions/sizes (no verified NIF writer)."));

    const float listW = std::clamp(ImGui::GetContentRegionAvail().x * 0.22f, 220.0f, 320.0f);
    ImGui::BeginChild("##layoutList", ImVec2(listW, 0.0f), true);
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputTextWithHint("##layoutFilter", L("Fenster filtern...", "Filter windows..."), state.layoutFilter, sizeof(state.layoutFilter));
    const std::string needle = LowerAscii(state.layoutFilter);
    for (const auto& rel : files) {
        if (!needle.empty() && LowerAscii(rel).find(needle) == std::string::npos) continue;
        const bool selected = LowerAscii(rel) == LowerAscii(state.layoutSelected);
        if (UI::Selectable((rel + "##layout").c_str(), selected)) {
            state.layoutSelected = rel;
            state.layoutPart = -1;
        }
        if (selected && state.layoutScrollToSelected) {
            ImGui::SetScrollHereY(0.3f);
            state.layoutScrollToSelected = false;
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##layoutMain", ImVec2(0.0f, 0.0f), false);
    for (const auto& rel : files)
        if (LowerAscii(rel) == LowerAscii(state.layoutSelected)) state.layoutSelected = rel;
    if (state.layoutSelected.empty()) {
        ImGui::TextDisabled("%s", L("Fenster-NIF auswählen.", "Select a window NIF."));
        ImGui::EndChild();
        return;
    }
    const std::string nifRel = state.layoutSelected;
    const auto& layout = Layout(state, nifRel);
    if (!layout.model) {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s: %s", nifRel.c_str(), layout.error.c_str());
        ImGui::EndChild();
        return;
    }
    const auto& model = *layout.model;
    Bounds all;
    for (const auto& part : model.parts) {
        const auto b = PartBounds(part);
        if (b.Valid()) {
            all.Add(b.minX, b.minY);
            all.Add(b.maxX, b.maxY);
        }
    }
    const float detailW = 330.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##layoutCanvas", ImVec2(std::max(200.0f, avail.x - detailW - 8.0f), 0.0f), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text("%s · %zu %s · %.0f x %.0f", nifRel.c_str(), model.parts.size(), L("Elemente", "elements"),
                all.Valid() ? all.maxX - all.minX : 0.0f, all.Valid() ? all.maxY - all.minY : 0.0f);
    UI::Checkbox(L("Platzhalter zeigen##layout", "Show placeholders##layout"), &state.layoutShowPlaceholders);
    ImGui::SameLine();
    UI::Checkbox(L("Umrisse##layout", "Outlines##layout"), &state.layoutShowOutlines);
    const ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
    if (!all.Valid() || canvasAvail.x < 50 || canvasAvail.y < 50) {
        ImGui::EndChild();
        ImGui::EndChild();
        return;
    }
    const float w = all.maxX - all.minX, h = all.maxY - all.minY;
    const float scale = std::min(4.0f, std::min((canvasAvail.x - 8.0f) / std::max(w, 1.0f), (canvasAvail.y - 8.0f) / std::max(h, 1.0f)));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const auto toScreen = [&](float x, float y) {
        return ImVec2(origin.x + 4.0f + (x - all.minX) * scale, origin.y + 4.0f + (y - all.minY) * scale);
    };
    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawCheckerboard(toScreen(all.minX, all.minY), toScreen(all.maxX, all.maxY));
    int hovered = -1;
    const ImVec2 mouse = ImGui::GetMousePos();
    const auto drawTextured = [&](const core::NifMeshPart& part, std::uint32_t tex) {
        dl->PushTextureID(static_cast<ImTextureID>(static_cast<intptr_t>(tex)));
        const std::size_t tris = part.triangleIndices.size() / 3;
        dl->PrimReserve(static_cast<int>(tris * 3), static_cast<int>(tris * 3));
        for (std::size_t t = 0; t < tris * 3; ++t) {
            const std::uint32_t vi = part.triangleIndices[t];
            const auto& p = vi < part.positions.size() ? part.positions[vi] : part.positions.front();
            const ImVec2 uv = vi < part.uvs.size() ? ImVec2(part.uvs[vi].u, part.uvs[vi].v) : ImVec2(0, 0);
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(dl->_VtxCurrentIdx));
            dl->PrimWriteVtx(toScreen(p.x, p.z), uv, PartColor(part, vi));
        }
        dl->PopTextureID();
    };
    for (std::size_t pi = 0; pi < model.parts.size(); ++pi) {
        const auto& part = model.parts[pi];
        if (part.appCulled || part.positions.empty()) continue;
        const std::string texRel = ResolveTexture(state, nifRel, part.diffuseTexture);
        const std::uint32_t tex = texRel.empty() ? 0 : ResmenuPreviewTexture(state, texRel);
        const auto b = PartBounds(part);
        const ImVec2 bmn = toScreen(b.minX, b.minY), bmx = toScreen(b.maxX, b.maxY);
        if (tex) {
            drawTextured(part, tex);
        } else if (state.layoutShowPlaceholders) {
            dl->AddRectFilled(bmn, bmx, IM_COL32(60, 140, 220, 50));
            dl->AddRect(bmn, bmx, IM_COL32(90, 170, 255, 160));
        }
        if (state.layoutShowOutlines && tex) dl->AddRect(bmn, bmx, IM_COL32(255, 255, 255, 40));
        if (ImGui::IsWindowHovered() && mouse.x >= bmn.x && mouse.x <= bmx.x && mouse.y >= bmn.y && mouse.y <= bmx.y &&
            (tex || state.layoutShowPlaceholders))
            hovered = static_cast<int>(pi); // spätere Elemente liegen oben
    }
    if (hovered >= 0) {
        const auto b = PartBounds(model.parts[static_cast<std::size_t>(hovered)]);
        dl->AddRect(toScreen(b.minX, b.minY), toScreen(b.maxX, b.maxY), IM_COL32(255, 220, 90, 220), 0.0f, 0, 1.5f);
        ImGui::SetTooltip("#%d %s\n%s", hovered, model.parts[static_cast<std::size_t>(hovered)].name.c_str(),
                          model.parts[static_cast<std::size_t>(hovered)].diffuseTexture.empty()
                              ? L("(ohne Textur: Platzhalter)", "(no texture: placeholder)")
                              : model.parts[static_cast<std::size_t>(hovered)].diffuseTexture.c_str());
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) state.layoutPart = hovered;
    }
    if (state.layoutPart >= 0 && state.layoutPart < static_cast<int>(model.parts.size())) {
        // Gewähltes Element obenauf: Zustände eines Knopfs (normal/hover/gedrückt) liegen übereinander.
        const auto& selPart = model.parts[static_cast<std::size_t>(state.layoutPart)];
        const std::string selTex = ResolveTexture(state, nifRel, selPart.diffuseTexture);
        if (const std::uint32_t tex = selTex.empty() ? 0 : ResmenuPreviewTexture(state, selTex); tex && !selPart.positions.empty())
            drawTextured(selPart, tex);
        const auto b = PartBounds(selPart);
        dl->AddRect(toScreen(b.minX, b.minY), toScreen(b.maxX, b.maxY), IM_COL32(32, 221, 242, 255), 0.0f, 0, 2.5f);
    }
    ImGui::Dummy(ImVec2(w * scale + 8.0f, h * scale + 8.0f));
    ImGui::EndChild();
    ImGui::SameLine();

    // Details und Elementliste.
    ImGui::BeginChild("##layoutDetail", ImVec2(0.0f, 0.0f), true);
    std::set<std::string> textures;
    for (const auto& part : model.parts)
        if (!part.diffuseTexture.empty()) textures.insert(part.diffuseTexture);
    ImGui::TextUnformatted(L("Texturen dieses Fensters:", "Textures of this window:"));
    for (const auto& texture : textures) {
        const std::string texRel = ResolveTexture(state, nifRel, texture);
        ImGui::PushID(texture.c_str());
        if (texRel.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s (%s)", texture.c_str(), L("nicht gefunden", "not found"));
        } else {
            ImGui::TextDisabled("%s", texRel.c_str());
            ImGui::SameLine();
            ImGui::BeginDisabled(state.project.projectFolder[0] == '\0');
            if (UI::SmallButton(L("Ersetzen...##layoutTex", "Replace...##layoutTex"))) OpenImageReplaceDialog(state, texRel, texRel);
            ImGui::EndDisabled();
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    const int sel = state.layoutPart;
    if (sel >= 0 && sel < static_cast<int>(model.parts.size())) {
        const auto& part = model.parts[static_cast<std::size_t>(sel)];
        const auto b = PartBounds(part);
        ImGui::Text("%s #%d %s", L("Element", "Element"), sel, part.name.c_str());
        ImGui::TextDisabled(L("Position %.0f, %.0f · Größe %.0f x %.0f · %zu Dreiecke", "Position %.0f, %.0f · size %.0f x %.0f · %zu triangles"),
                            b.minX, b.minY, b.maxX - b.minX, b.maxY - b.minY, part.triangleIndices.size() / 3);
        const std::string texRel = ResolveTexture(state, nifRel, part.diffuseTexture);
        if (part.diffuseTexture.empty()) {
            ImGui::TextDisabled("%s", L("Ohne Textur: Platzhalter für Inhalte, die der Client selbst zeichnet.",
                                        "No texture: placeholder for content the client draws itself."));
        } else if (texRel.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s %s", part.diffuseTexture.c_str(), L("nicht gefunden", "not found"));
        } else {
            auto fmt = core::InspectImageFile(ResmenuEffectivePath(state, texRel));
            std::uint32_t rx = 0, ry = 0, rw = 0, rh = 0;
            const bool hasRect = fmt && NifPartUvRect(part, fmt->width, fmt->height, rx, ry, rw, rh);
            ImGui::TextDisabled("%s", texRel.c_str());
            if (hasRect) {
                ImGui::TextDisabled(L("Ausschnitt %ux%u bei %u,%u", "Region %ux%u at %u,%u"), rw, rh, rx, ry);
                if (const auto tex = ResmenuPreviewTexture(state, texRel)) {
                    const float fw = static_cast<float>(fmt->width), fh = static_cast<float>(fmt->height);
                    const float maxSide = 140.0f, k = std::min(maxSide / static_cast<float>(rw), maxSide / static_cast<float>(rh));
                    const ImVec2 size(static_cast<float>(rw) * std::min(k, 4.0f), static_cast<float>(rh) * std::min(k, 4.0f));
                    const ImVec2 p = ImGui::GetCursorScreenPos();
                    DrawCheckerboard(p, ImVec2(p.x + size.x, p.y + size.y));
                    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), size, ImVec2(static_cast<float>(rx) / fw, static_cast<float>(ry) / fh),
                                 ImVec2(static_cast<float>(rx + rw) / fw, static_cast<float>(ry + rh) / fh));
                }
            }
            ImGui::BeginDisabled(state.project.projectFolder[0] == '\0' || !hasRect);
            const bool automation = !state.layoutAutomationReplaceSource.empty();
            if (UI::Button(L("Ausschnitt ersetzen...", "Replace region...")) || (automation && hasRect)) {
                OpenImageReplaceDialog(state, texRel, std::filesystem::path(nifRel).filename().string() + " #" + std::to_string(sel));
                auto& r = state.imageReplace;
                r.cellMode = true;
                r.cellX = rx;
                r.cellY = ry;
                r.cellW = rw;
                r.cellH = rh;
                r.fit = 0; // Strecken: der Ausschnitt ist genau die Elementgrafik
                if (automation) std::snprintf(r.sourcePath, sizeof(r.sourcePath), "%s", state.layoutAutomationReplaceSource.c_str());
                state.layoutAutomationReplaceSource.clear();
            }
            ImGui::EndDisabled();
            ImGui::TextDisabled("%s", L("Die Textur kann von weiteren Fenstern genutzt werden - der Ausschnitt ändert sich überall.",
                                        "The texture may be used by other windows - the region changes everywhere."));
        }
        ImGui::Separator();
    }
    ImGui::TextUnformatted(L("Elemente:", "Elements:"));
    for (std::size_t pi = 0; pi < model.parts.size(); ++pi) {
        const auto& part = model.parts[pi];
        const auto b = PartBounds(part);
        char label[192];
        std::snprintf(label, sizeof(label), "#%zu %s  %.0fx%.0f  %s##layoutPart", pi, part.name.c_str(), b.maxX - b.minX, b.maxY - b.minY,
                      part.diffuseTexture.empty() ? "-" : part.diffuseTexture.c_str());
        if (ImGui::Selectable(label, static_cast<int>(pi) == sel)) state.layoutPart = static_cast<int>(pi);
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

} // namespace nge
