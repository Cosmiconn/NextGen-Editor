// WindowLayouts.cpp
// Fenster-Layouts: die Interface-NIFs unter resmenu (NA2016: 310 Dateien, Gamebryo 20.0.0.4) als
// 2D-Ansicht. Jedes Fensterelement ist ein texturiertes NiTriShape in der X/Z-Ebene (Y = 0, Z nach
// unten, Einheit = Bildschirmpixel); Teile ohne Textur sind Platzhalter für Inhalte, die der Client
// selbst zeichnet (Text, Slots). Bearbeitbar sind die Texturen (ganz ersetzen oder nur den
// UV-Ausschnitt eines Elements) sowie Position und Größe der Elemente: core::PatchNifPartRect
// überschreibt nur die Vertex-Floats und die Bounding-Sphere des NiTriShapeData-Blocks und prüft das
// Ergebnis mit einem strikten Neu-Laden, bevor die Projektkopie geschrieben wird.

#include "EditorShared.hpp"
#include "mapeditor/core/NifPatch.hpp"
#include "mapeditor/core/NifWriter.hpp"

namespace nge {

namespace {
struct LayoutCache {
    std::string key;
    std::optional<core::NifModel> model;
    // Byte-Lage aus dem strikten Standardpfad (nur dann lassen sich Elemente verschieben).
    std::optional<core::NifByteLayout> byteLayout;
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
    std::ifstream in(path, std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (auto strict = core::LoadNifByteLayout(bytes)) {
        cache.model = strict->model;
        cache.byteLayout = std::move(*strict);
    } else if (auto model = core::LoadNifMesh(path)) {
        cache.model = std::move(*model);
    } else {
        cache.error = model.error();
    }
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
// Gerundete Verschiebung (ganze Bildschirmpixel), damit Elemente auf dem Pixelraster bleiben.
float Snap(float v) { return std::round(v); }
} // namespace

bool MoveLayoutElement(EditorState& state, const std::string& nifRel, int part, const float rect[4], std::string& message) {
    const auto path = ResmenuEffectivePath(state, nifRel);
    std::ifstream in(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    if (bytes.empty() || part < 0) {
        message = L("Datei nicht lesbar: ", "File not readable: ") + path.string();
        return false;
    }
    const core::NifRect target{rect[0], rect[1], std::max(rect[2], 0.0f), std::max(rect[3], 0.0f)};
    // Elemente mit genau demselben Rechteck (z. B. die Zustände eines Knopfs: normal, Maus darüber,
    // gedrückt) wandern auf Wunsch mit - als ein Undo-Schritt.
    std::vector<std::size_t> parts{static_cast<std::size_t>(part)};
    if (state.layoutMoveStacked) {
        if (auto layout = core::LoadNifByteLayout(bytes); layout && static_cast<std::size_t>(part) < layout->model.parts.size()) {
            const auto own = core::NifPartRect(layout->model.parts[static_cast<std::size_t>(part)]);
            for (std::size_t q = 0; q < layout->model.parts.size(); ++q) {
                if (q == static_cast<std::size_t>(part)) continue;
                const auto r = core::NifPartRect(layout->model.parts[q]);
                if (std::abs(r.x - own.x) < 0.01f && std::abs(r.y - own.y) < 0.01f && std::abs(r.width - own.width) < 0.01f &&
                    std::abs(r.height - own.height) < 0.01f && core::CheckNifPartPatchable(*layout, q).patchable)
                    parts.push_back(q);
            }
        }
    }
    for (const auto q : parts) {
        auto patched = core::PatchNifPartRect(bytes, q, target);
        if (!patched) {
            message = "#" + std::to_string(q) + ": " + patched.error();
            return false;
        }
        bytes = std::move(*patched);
    }
    char label[256];
    std::snprintf(label, sizeof(label), "%s #%d%s -> %.0f,%.0f %.0fx%.0f", std::filesystem::path(nifRel).filename().string().c_str(), part,
                  parts.size() > 1 ? (" (+" + std::to_string(parts.size() - 1) + ")").c_str() : "", target.x, target.y, target.width, target.height);
    if (!WriteResmenuOverride(state, nifRel, bytes, label)) {
        message = state.statusMessage;
        return false;
    }
    message = std::string(L("Element verschoben: ", "Element moved: ")) + label;
    return true;
}

// Texturdatei eines Fensters umstellen: alle NiSourceTexture-Blöcke mit diesem Namen (Groß/Klein egal)
// bekommen den neuen Dateinamen - NIF-Writer v1, jede Ausgabe gegengeprüft, Projektkopie mit Strg+Z.
bool AssignLayoutTexture(EditorState& state, const std::string& nifRel, const std::string& oldName, const std::string& newName,
                         std::string& message) {
    const auto path = ResmenuEffectivePath(state, nifRel);
    std::ifstream in(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    const auto layout = core::LoadNifByteLayout(bytes);
    if (!layout) {
        message = std::string(L("NIF nicht strikt lesbar: ", "NIF not strictly readable: ")) + layout.error();
        return false;
    }
    std::size_t changed = 0;
    for (const auto& tex : core::NifTextureSources(*layout)) {
        if (LowerAscii(tex.file) != LowerAscii(oldName)) continue;
        auto rewritten = core::RewriteNifTextureFile(bytes, tex.block, newName);
        if (!rewritten) {
            message = "#" + std::to_string(tex.block) + ": " + rewritten.error();
            return false;
        }
        bytes = std::move(*rewritten);
        ++changed;
    }
    if (changed == 0) {
        message = std::string(L("Keine externe Textur mit diesem Namen: ", "No external texture with this name: ")) + oldName;
        return false;
    }
    const std::string label = std::filesystem::path(nifRel).filename().string() + ": " + oldName + " -> " + newName;
    if (!WriteResmenuOverride(state, nifRel, bytes, label)) {
        message = state.statusMessage;
        return false;
    }
    message = std::string(L("Textur zugewiesen (", "Texture assigned (")) + std::to_string(changed) + "x): " + label;
    return true;
}

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
    ImGui::TextDisabled("%s", L("Element ziehen = verschieben, Ecken ziehen = Größe, Pfeiltasten = 1 px (Umschalt 10 px); Projektkopie, Strg+Z.",
                                "Drag an element to move it, drag its corners to resize, arrow keys = 1 px (Shift 10 px); project copy, Ctrl+Z."));

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
    // Optional an ein anderes Rechteck abgebildet (Vorschau beim Ziehen): from -> to.
    const auto drawTextured = [&](const core::NifMeshPart& part, std::uint32_t tex, const core::NifRect* from = nullptr,
                                  const core::NifRect* to = nullptr) {
        dl->PushTextureID(static_cast<ImTextureID>(static_cast<intptr_t>(tex)));
        const std::size_t tris = part.triangleIndices.size() / 3;
        dl->PrimReserve(static_cast<int>(tris * 3), static_cast<int>(tris * 3));
        for (std::size_t t = 0; t < tris * 3; ++t) {
            const std::uint32_t vi = part.triangleIndices[t];
            const auto& p = vi < part.positions.size() ? part.positions[vi] : part.positions.front();
            const ImVec2 uv = vi < part.uvs.size() ? ImVec2(part.uvs[vi].u, part.uvs[vi].v) : ImVec2(0, 0);
            float x = p.x, y = p.z;
            if (from && to) {
                x = to->x + (x - from->x) * (from->width > 1e-6f ? to->width / from->width : 1.0f);
                y = to->y + (y - from->y) * (from->height > 1e-6f ? to->height / from->height : 1.0f);
            }
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(dl->_VtxCurrentIdx));
            dl->PrimWriteVtx(toScreen(x, y), uv, PartColor(part, vi));
        }
        dl->PopTextureID();
    };
    // Gewähltes Element: verschiebbar? (nur bei strikt geladener Datei und eigenem NiTriShapeData-Block)
    const int selIndex = state.layoutPart >= 0 && state.layoutPart < static_cast<int>(model.parts.size()) ? state.layoutPart : -1;
    bool selPatchable = false;
    std::string selRefusal;
    if (selIndex >= 0) {
        if (layout.byteLayout) {
            const auto check = core::CheckNifPartPatchable(*layout.byteLayout, static_cast<std::size_t>(selIndex));
            selPatchable = check.patchable;
            selRefusal = app::CurrentLanguage() == app::Language::German ? check.reason : check.reasonEn;
        } else {
            selRefusal = L("Die Datei lädt nur über Wiederherstellungs-Heuristiken - Verschieben ist gesperrt.",
                           "The file only loads through recovery heuristics - moving is disabled.");
        }
    }
    if (selIndex < 0 || !selPatchable) state.layoutDragMode = 0;
    const core::NifRect selRect = selIndex >= 0 ? core::NifPartRect(model.parts[static_cast<std::size_t>(selIndex)]) : core::NifRect{};
    const bool dragging = state.layoutDragMode != 0;
    for (std::size_t pi = 0; pi < model.parts.size(); ++pi) {
        const auto& part = model.parts[pi];
        if (part.appCulled || part.positions.empty()) continue;
        if (dragging && static_cast<int>(pi) == selIndex) continue; // wird an der Vorschau-Position gezeichnet
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
    // Ziehen beginnen: Ecke des gewählten Elements = Größe, Fläche = verschieben.
    if (selPatchable && !dragging && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 mn = toScreen(selRect.x, selRect.y), mx = toScreen(selRect.x + selRect.width, selRect.y + selRect.height);
        const ImVec2 corners[4] = {mn, ImVec2(mx.x, mn.y), mx, ImVec2(mn.x, mx.y)};
        for (int k = 0; k < 4 && state.layoutDragMode == 0; ++k)
            if (std::abs(mouse.x - corners[k].x) <= 6.0f && std::abs(mouse.y - corners[k].y) <= 6.0f) state.layoutDragMode = 2 + k;
        if (state.layoutDragMode == 0 && mouse.x >= mn.x && mouse.x <= mx.x && mouse.y >= mn.y && mouse.y <= mx.y) state.layoutDragMode = 1;
        if (state.layoutDragMode != 0) {
            state.layoutDragStart[0] = mouse.x;
            state.layoutDragStart[1] = mouse.y;
            state.layoutDragRect[0] = selRect.x;
            state.layoutDragRect[1] = selRect.y;
            state.layoutDragRect[2] = selRect.width;
            state.layoutDragRect[3] = selRect.height;
        }
    }
    core::NifRect live = selRect;
    if (state.layoutDragMode != 0) {
        const float dx = Snap((mouse.x - state.layoutDragStart[0]) / scale), dy = Snap((mouse.y - state.layoutDragStart[1]) / scale);
        live = {state.layoutDragRect[0], state.layoutDragRect[1], state.layoutDragRect[2], state.layoutDragRect[3]};
        const float minW = live.width > 0.0f ? 1.0f : 0.0f, minH = live.height > 0.0f ? 1.0f : 0.0f;
        switch (state.layoutDragMode) {
        case 1: live.x += dx; live.y += dy; break;
        case 2: live.x += dx; live.y += dy; live.width -= dx; live.height -= dy; break;   // links oben
        case 3: live.y += dy; live.width += dx; live.height -= dy; break;                 // rechts oben
        case 4: live.width += dx; live.height += dy; break;                               // rechts unten
        case 5: live.x += dx; live.width -= dx; live.height += dy; break;                 // links unten
        default: break;
        }
        if (live.width < minW) { if (state.layoutDragMode == 2 || state.layoutDragMode == 5) live.x -= minW - live.width; live.width = minW; }
        if (live.height < minH) { if (state.layoutDragMode == 2 || state.layoutDragMode == 3) live.y -= minH - live.height; live.height = minH; }
        if (state.layoutDragMode == 1 || state.layoutDragMode == 0) {
            live.width = state.layoutDragRect[2];
            live.height = state.layoutDragRect[3];
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            state.layoutDragMode = 0;
        } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const bool changed = live.x != selRect.x || live.y != selRect.y || live.width != selRect.width || live.height != selRect.height;
            state.layoutDragMode = 0;
            if (changed) {
                const float r[4] = {live.x, live.y, live.width, live.height};
                MoveLayoutElement(state, nifRel, selIndex, r, state.layoutMoveStatus);
            }
        }
        ImGui::SetTooltip("%.0f, %.0f  %.0f x %.0f", live.x, live.y, live.width, live.height);
    } else if (selPatchable && ImGui::IsWindowHovered() && !ImGui::GetIO().WantTextInput) {
        // Pfeiltasten: 1 px, mit Umschalt 10 px - je Tastendruck ein Undo-Schritt.
        const float step = ImGui::GetIO().KeyShift ? 10.0f : 1.0f;
        float r[4] = {selRect.x, selRect.y, selRect.width, selRect.height};
        bool move = false;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) { r[0] -= step; move = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) { r[0] += step; move = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) { r[1] -= step; move = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) { r[1] += step; move = true; }
        if (move) MoveLayoutElement(state, nifRel, selIndex, r, state.layoutMoveStatus);
    }
    if (hovered >= 0 && !dragging && state.layoutDragMode == 0) {
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
        const bool preview = state.layoutDragMode != 0;
        if (const std::uint32_t tex = selTex.empty() ? 0 : ResmenuPreviewTexture(state, selTex); tex && !selPart.positions.empty())
            drawTextured(selPart, tex, preview ? &selRect : nullptr, preview ? &live : nullptr);
        else if (preview)
            dl->AddRectFilled(toScreen(live.x, live.y), toScreen(live.x + live.width, live.y + live.height), IM_COL32(60, 140, 220, 70));
        const ImVec2 mn = toScreen(live.x, live.y), mx = toScreen(live.x + live.width, live.y + live.height);
        dl->AddRect(mn, mx, IM_COL32(32, 221, 242, 255), 0.0f, 0, 2.5f);
        if (selPatchable) {
            // Eck-Griffe zum Ändern der Größe.
            for (const ImVec2 c : {mn, ImVec2(mx.x, mn.y), mx, ImVec2(mn.x, mx.y)})
                dl->AddRectFilled(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), IM_COL32(32, 221, 242, 255));
        }
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
        // Andere Datei zuweisen (schreibt den Namen in die NIF, auch mit anderer Länge).
        ImGui::SameLine();
        ImGui::BeginDisabled(state.project.projectFolder[0] == '\0');
        static char assignName[256] = "";
        if (UI::SmallButton(L("Andere Datei...##layoutTexAssign", "Other file...##layoutTexAssign"))) {
            std::snprintf(assignName, sizeof(assignName), "%s", texture.c_str());
            ImGui::OpenPopup("##layoutTexAssignPopup");
        }
        ImGui::EndDisabled();
        if (ImGui::BeginPopup("##layoutTexAssignPopup")) {
            ImGui::TextWrapped(L("Neuer Texturname für %s (wie in der NIF, z. B. relativ zum Fenster-Ordner).",
                                 "New texture name for %s (as stored in the NIF, e.g. relative to the window folder)."), texture.c_str());
            ImGui::SetNextItemWidth(320.0f);
            UI::InputText("##layoutTexAssignName", assignName, sizeof(assignName));
            if (UI::Button(L("Zuweisen##layoutTexAssign", "Assign##layoutTexAssign"))) {
                AssignLayoutTexture(state, nifRel, texture, TrimAscii(assignName), state.layoutMoveStatus);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
        // Position und Größe: Felder für dieses Element (und diesen Dateistand) füllen.
        const std::string editKey = nifRel + "|" + std::to_string(sel) + "|" + layout.key;
        if (state.layoutEditKey != editKey) {
            state.layoutEditKey = editKey;
            state.layoutEditRect[0] = b.minX;
            state.layoutEditRect[1] = b.minY;
            state.layoutEditRect[2] = b.maxX - b.minX;
            state.layoutEditRect[3] = b.maxY - b.minY;
        }
        ImGui::BeginDisabled(!selPatchable || state.project.projectFolder[0] == '\0');
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputFloat4("##layoutRect", state.layoutEditRect, "%.1f");
        UI::Tip("InputFloat", "##layoutRect");
        if (UI::Button(L("Übernehmen##layoutRect", "Apply##layoutRect")))
            MoveLayoutElement(state, nifRel, sel, state.layoutEditRect, state.layoutMoveStatus);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", L("X, Y, Breite, Höhe", "X, Y, width, height"));
        UI::Checkbox(L("Elemente an derselben Stelle mitnehmen##layout", "Move elements at the same place along##layout"), &state.layoutMoveStacked);
        if (!selPatchable && !selRefusal.empty()) ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "%s", selRefusal.c_str());
        if (!state.layoutMoveStatus.empty()) ImGui::TextWrapped("%s", state.layoutMoveStatus.c_str());
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
