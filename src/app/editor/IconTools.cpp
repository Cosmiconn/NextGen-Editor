// IconTools.cpp
// Item- und Skill-Icons aus den Client-Atlanten (resmenu\Icon\<IconFile>.dds, 32x32-Zellen) für die
// Daten-Editoren: Zuordnung InxName -> IconFile/IconIndex aus ItemViewInfo.shn, Zeichnen einer
// Atlaszelle und ein Atlas-Picker.

#include "EditorShared.hpp"

#include "mapeditor/core/legacy/IconAtlas.hpp"

namespace nge {

void EnsureItemIcons(EditorState& state) {
    if (state.itemIconsTried) return;
    state.itemIconsTried = true;
    EnsureNpcDialogRoot(state);
    if (state.npcDialogRessystemRoot.empty()) return;
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Client,
                                            std::filesystem::path(state.npcDialogRessystemRoot) / "ItemViewInfo.shn");
    auto file = core::legacy::LoadShnFile(path);
    if (!file) return;
    const int cInx = ShnColumnIndexByName(*file, "InxName");
    const int cIdx = ShnColumnIndexByName(*file, "IconIndex");
    const int cFile = ShnColumnIndexByName(*file, "IconFile");
    if (cInx < 0 || cIdx < 0 || cFile < 0) return;
    for (const auto& row : file->rows) {
        const auto at = [&](int c) {
            return static_cast<std::size_t>(c) < row.values.size() ? core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(c)]) : std::string();
        };
        const std::string inx = TrimAscii(at(cInx)), icon = TrimAscii(at(cFile));
        if (inx.empty() || icon.empty() || icon == "-") continue;
        state.itemIconByInx[inx] = {icon, std::atoi(at(cIdx).c_str())};
    }
}

std::optional<std::pair<std::string, int>> ItemIconForInx(EditorState& state, const std::string& inx) {
    EnsureItemIcons(state);
    const auto it = state.itemIconByInx.find(inx);
    if (it == state.itemIconByInx.end()) return std::nullopt;
    return it->second;
}

std::optional<std::pair<std::string, int>> ItemIconForId(EditorState& state, long long id) {
    EnsureItemLookup(state);
    if (state.itemInxById.empty())
        for (const auto& e : state.itemEntries) state.itemInxById.emplace(e.id, e.inx);
    const auto it = state.itemInxById.find(id);
    if (it == state.itemInxById.end()) return std::nullopt;
    return ItemIconForInx(state, it->second);
}

namespace {
struct AtlasView {
    std::uint32_t tex = 0, width = 0, height = 0;
};
AtlasView LoadAtlas(EditorState& state, const std::string& iconFile) {
    AtlasView a;
    const auto path = GameResmenuPath(state, core::legacy::IconAtlasRelativePath(iconFile));
    if (path.empty()) return a;
    const std::string key = path.string();
    auto sz = state.iconAtlasSizes.find(key);
    if (sz == state.iconAtlasSizes.end()) {
        const auto s = core::legacy::ReadDdsSize(path);
        sz = state.iconAtlasSizes.emplace(key, s ? *s : std::pair<std::uint32_t, std::uint32_t>{0, 0}).first;
    }
    a.width = sz->second.first;
    a.height = sz->second.second;
    if (a.width == 0) return a;
    a.tex = GetOrLoadAssetThumbnail(state, path, false).tex;
    return a;
}
// DDS-Vorschaubilder liegen in OpenGL-Zeilenfolge vor (LoadDdsImage dreht): V spiegeln.
void AtlasImage(const AtlasView& a, const core::legacy::IconCell& c, float size) {
    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(a.tex)), ImVec2(size, size), ImVec2(c.u0, 1.0f - c.v0),
                 ImVec2(c.u1, 1.0f - c.v1));
}
} // namespace

bool DrawAtlasIcon(EditorState& state, const std::string& iconFile, int index, float size) {
    const AtlasView a = LoadAtlas(state, iconFile);
    const auto cell = core::legacy::IconAtlasCell(a.width, a.height, index);
    if (a.tex == 0 || !cell.valid) {
        ImGui::Dummy(ImVec2(size, size));
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(80, 92, 110, 160), 3.0f);
        if (ImGui::IsItemHovered() && !iconFile.empty())
            ImGui::SetTooltip("%s #%d\n%s", iconFile.c_str(), index,
                              a.width == 0 ? L("Atlas nicht gefunden (resmenu/Icon)", "Atlas not found (resmenu/Icon)")
                                           : L("Index außerhalb des Atlas", "Index outside the atlas"));
        return false;
    }
    AtlasImage(a, cell, size);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        AtlasImage(a, cell, 64.0f);
        ImGui::Text("%s #%d", iconFile.c_str(), index);
        ImGui::EndTooltip();
    }
    return true;
}

bool DrawItemIconByInx(EditorState& state, const std::string& inx, float size) {
    const auto icon = ItemIconForInx(state, inx);
    if (!icon) {
        ImGui::Dummy(ImVec2(size, size));
        return false;
    }
    return DrawAtlasIcon(state, icon->first, icon->second, size);
}

bool DrawItemIconById(EditorState& state, long long id, float size) {
    const auto icon = ItemIconForId(state, id);
    if (!icon) {
        ImGui::Dummy(ImVec2(size, size));
        return false;
    }
    return DrawAtlasIcon(state, icon->first, icon->second, size);
}

bool DrawIconAtlasPicker(EditorState& state, const std::string& iconFile, int& index) {
    const AtlasView a = LoadAtlas(state, iconFile);
    if (a.tex == 0) {
        ImGui::TextDisabled("%s", L("Atlas nicht gefunden (resmenu/Icon).", "Atlas not found (resmenu/Icon)."));
        return false;
    }
    const int capacity = core::legacy::IconAtlasCapacity(a.width, a.height);
    const int cols = 8; // alle Atlanten sind 8x8 (auch die 128er mit 16-px-Zellen)
    bool picked = false;
    for (int i = 0; i < capacity; ++i) {
        const auto cell = core::legacy::IconAtlasCell(a.width, a.height, i);
        if (i % cols != 0) ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushID(i);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        AtlasImage(a, cell, 32.0f);
        if (ImGui::IsItemClicked()) {
            index = i;
            picked = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("#%d", i);
        if (i == index)
            ImGui::GetWindowDrawList()->AddRect(p, ImVec2(p.x + 32, p.y + 32), IM_COL32(32, 221, 242, 255), 2.0f, 0, 2.0f);
        ImGui::PopID();
    }
    return picked;
}

} // namespace nge
