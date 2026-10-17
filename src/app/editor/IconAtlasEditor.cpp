// IconAtlasEditor.cpp
// Icon-Atlas-Editor im Interface-Bereich: alle Atlanten unter resmenu/Icon (8x8-Raster, Zelle =
// Atlasbreite / 8), welche Zelle von welchem Item/Skill/Passiv/Zustand benutzt wird (ItemViewInfo
// Icon/SubIcon/PeriodIcon, ActiveSkillView, PassiveSkillView, AbStateView), freie und leere Zellen,
// eine Zelle ersetzen (nur deren DDS-Blöcke ändern sich), ganzen Atlas ersetzen, neuen leeren Atlas
// im Format eines vorhandenen anlegen. Sprung in die Daten-Editoren und zurück.

#include "EditorShared.hpp"

#include "mapeditor/core/legacy/IconAtlas.hpp"

namespace nge {

namespace {
struct IconUse {
    int kind = 0; // 0 Item, 1 Skill, 2 Passiv, 3 Zustand
    long long id = -1;
    std::string inx;
    const char* column = "";
};

struct UsageIndex {
    std::string key;
    // kleingeschriebener Atlasname (ohne Endung) -> Index -> Verwendungen
    std::unordered_map<std::string, std::map<int, std::vector<IconUse>>> byAtlas;
};

const UsageIndex& Usage(EditorState& state) {
    static UsageIndex index;
    const std::string key = std::to_string(state.dataHistory.Revision()) + "|" + std::to_string(state.shnEditCounter) + "|" +
                            std::to_string(state.shnFiles.size());
    if (index.key == key) return index;
    index.key = key;
    index.byAtlas.clear();
    struct Source {
        const char* file;
        int kind;
        const char* inxColumn;
        std::vector<std::pair<const char*, const char*>> pairs; // (Datei-Spalte, Index-Spalte)
    };
    const Source sources[] = {
        {"ItemViewInfo.shn", 0, "InxName", {{"IconFile", "IconIndex"}, {"SubIconFile", "SubIconIndex"}, {"PeriodIconFile", "PeriodIconIndex"}}},
        {"ActiveSkillView.shn", 1, "InxName", {{"IconFile", "IconIndex"}}},
        {"PassiveSkillView.shn", 2, "InxName", {{"IconFile", "Icon"}}},
        {"AbStateView.shn", 3, "inxName", {{"iconFile", "icon"}}},
    };
    for (const auto& src : sources) {
        const int doc = SharedShnDoc(state, src.file, EditorState::ShnSource::Client);
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        for (const auto& [fileCol, indexCol] : src.pairs) {
            if (ShnColumnIndexByName(f, fileCol) < 0 || ShnColumnIndexByName(f, indexCol) < 0) continue;
            for (std::size_t r = 0; r < f.rows.size(); ++r) {
                const std::string atlas = TrimAscii(ShnCellText(f, r, fileCol));
                if (atlas.empty() || atlas == "-") continue;
                IconUse use;
                use.kind = src.kind;
                use.id = std::atoll(TrimAscii(ShnCellText(f, r, "ID")).c_str());
                use.inx = TrimAscii(ShnCellText(f, r, src.inxColumn));
                use.column = fileCol;
                index.byAtlas[LowerAscii(atlas)][std::atoi(TrimAscii(ShnCellText(f, r, indexCol)).c_str())].push_back(std::move(use));
            }
        }
    }
    return index;
}

// Atlasdateien unter resmenu/Icon (und Icon/Png), Client und Projekt.
std::vector<std::string> AtlasFiles(EditorState& state) {
    static std::string cachedKey;
    static std::vector<std::string> files;
    const std::string key = ResmenuSourceRoot(state).string() + "|" + InterfaceProjectOverrideRoot(state).string() + "|" +
                            std::to_string(state.resmenuRevision);
    if (key == cachedKey) return files;
    cachedKey = key;
    files.clear();
    std::set<std::string> seen;
    for (const auto& root : {ResmenuSourceRoot(state), InterfaceProjectOverrideRoot(state)}) {
        if (root.empty()) continue;
        for (const char* sub : {"Icon", "Icon/Png"}) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(root / sub, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                const std::string ext = LowerAscii(entry.path().extension().string());
                if (ext != ".dds" && ext != ".png" && ext != ".tga") continue;
                const std::string rel = std::string(sub) + "/" + entry.path().filename().string();
                if (seen.insert(LowerAscii(rel)).second) files.push_back(rel);
            }
        }
    }
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) { return LowerAscii(a) < LowerAscii(b); });
    return files;
}

std::string AtlasStem(const std::string& rel) { return std::filesystem::path(rel).stem().string(); }

// Größe und leere Zellen (alle Pixel transparent) eines Atlas; neu bei jeder Dateiänderung.
struct AtlasInfo {
    std::string key;
    std::uint32_t width = 0, height = 0, cell = 0;
    std::vector<bool> empty;
    std::optional<core::ImageFormat> format;
};
const AtlasInfo& Info(EditorState& state, const std::string& rel) {
    static std::unordered_map<std::string, AtlasInfo> cache;
    const auto path = ResmenuEffectivePath(state, rel);
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(path, ec);
    const std::string key = path.string() + "|" + std::to_string(ec ? 0 : stamp.time_since_epoch().count());
    auto& info = cache[LowerAscii(rel)];
    if (info.key == key) return info;
    info = {};
    info.key = key;
    if (auto fmt = core::InspectImageFile(path)) info.format = *fmt;
    auto image = core::LoadImageFile(path);
    if (!image) return info;
    info.width = image->width;
    info.height = image->height;
    info.cell = std::max<std::uint32_t>(1, image->width / 8);
    const int capacity = core::legacy::IconAtlasCapacity(info.width, info.height);
    info.empty.assign(static_cast<std::size_t>(std::max(capacity, 0)), true);
    for (int i = 0; i < capacity; ++i) {
        const std::uint32_t cx = static_cast<std::uint32_t>(i % 8) * info.cell, cy = static_cast<std::uint32_t>(i / 8) * info.cell;
        bool empty = true;
        for (std::uint32_t y = cy; y < cy + info.cell && y < info.height && empty; ++y)
            for (std::uint32_t x = cx; x < cx + info.cell && x < info.width; ++x)
                if (image->rgba[(std::size_t{y} * info.width + x) * 4 + 3] != 0) {
                    empty = false;
                    break;
                }
        info.empty[static_cast<std::size_t>(i)] = empty;
    }
    return info;
}

const char* KindName(int kind) {
    switch (kind) {
        case 0: return L("Item", "Item");
        case 1: return L("Skill", "Skill");
        case 2: return L("Passiv", "Passive");
        default: return L("Zustand", "State");
    }
}

void JumpToUser(EditorState& state, const IconUse& use) {
    switch (use.kind) {
        case 0: state.itemForm.selectedId = use.id; state.shnSubTab = 11; break;
        case 1: state.skill.selectedId = use.id; state.shnSubTab = 7; break;
        case 2: state.passiveForm.selectedId = use.id; state.shnSubTab = 13; break;
        default: state.stateForm.selectedId = use.id; state.shnSubTab = 14; break;
    }
}

std::string SuggestAtlasName(EditorState& state, const std::string& base) {
    // Ziffern am Ende hochzählen (ItemEqu012 -> ItemEqu013 ...), bis der Name frei ist.
    std::string stem = base;
    std::size_t digits = 0;
    while (digits < stem.size() && std::isdigit(static_cast<unsigned char>(stem[stem.size() - 1 - digits]))) ++digits;
    const std::string prefix = stem.substr(0, stem.size() - digits);
    int number = digits ? std::atoi(stem.substr(stem.size() - digits).c_str()) : 0;
    const auto files = AtlasFiles(state);
    for (int n = number + 1; n < number + 1000; ++n) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%0*d", static_cast<int>(std::max<std::size_t>(digits, 2)), n);
        const std::string candidate = prefix + buf;
        const bool taken = std::any_of(files.begin(), files.end(), [&](const std::string& f) { return LowerAscii(AtlasStem(f)) == LowerAscii(candidate); });
        if (!taken) return candidate;
    }
    return prefix + "_new";
}
} // namespace

std::vector<std::string> IconAtlasUsers(EditorState& state, const std::string& iconFile, int index) {
    std::vector<std::string> out;
    const auto& usage = Usage(state);
    const auto atlas = usage.byAtlas.find(LowerAscii(TrimAscii(iconFile)));
    if (atlas == usage.byAtlas.end()) return out;
    const auto cell = atlas->second.find(index);
    if (cell == atlas->second.end()) return out;
    for (const auto& u : cell->second) out.push_back(std::string(KindName(u.kind)) + " " + u.inx);
    return out;
}

void OpenIconInAtlasEditor(EditorState& state, const std::string& iconFile, int index) {
    state.screen = AppScreen::ShnEditor;
    state.shnSubTab = 9;
    state.interfaceMode = 2;
    state.iconEditorAtlas = core::legacy::IconAtlasRelativePath(iconFile);
    state.iconEditorCell = index;
    state.iconEditorScrollToSelected = true;
}

void DrawIconAtlasEditor(EditorState& state) {
    if (ResmenuSourceRoot(state).empty()) {
        ImGui::TextDisabled("%s", L("Kein Client/resmenu gefunden.", "No Client/resmenu found."));
        return;
    }
    const auto files = AtlasFiles(state);
    const auto& usage = Usage(state);

    DrawDataHistoryButtons(state, "iconHistory");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu %s", files.size(), L("Atlanten unter resmenu/Icon", "atlases below resmenu/Icon"));

    const float listW = std::clamp(ImGui::GetContentRegionAvail().x * 0.24f, 220.0f, 320.0f);
    ImGui::BeginChild("##iconAtlasList", ImVec2(listW, 0.0f), true);
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputTextWithHint("##iconAtlasFilter", L("Atlas filtern...", "Filter atlas..."), state.iconEditorFilter, sizeof(state.iconEditorFilter));
    const std::string needle = LowerAscii(state.iconEditorFilter);
    for (const auto& rel : files) {
        if (!needle.empty() && LowerAscii(rel).find(needle) == std::string::npos) continue;
        const auto it = usage.byAtlas.find(LowerAscii(AtlasStem(rel)));
        const std::size_t used = it == usage.byAtlas.end() ? 0 : it->second.size();
        const bool selected = LowerAscii(rel) == LowerAscii(state.iconEditorAtlas);
        if (UI::Selectable((rel + "##iconAtlas").c_str(), selected, 0, ImVec2(listW - 70.0f, 0.0f))) {
            state.iconEditorAtlas = rel;
            state.iconEditorCell = -1;
        }
        if (selected && state.iconEditorScrollToSelected) ImGui::SetScrollHereY(0.3f);
        ImGui::SameLine(listW - 60.0f);
        ImGui::TextDisabled("%zu/64", used);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##iconAtlasDetail", ImVec2(0.0f, 0.0f), true);
    // Auswahl auf die tatsächliche Schreibweise der Datei bringen (IconFile ist ohne Endung/Groß-Klein).
    for (const auto& rel : files)
        if (LowerAscii(rel) == LowerAscii(state.iconEditorAtlas)) state.iconEditorAtlas = rel;
    const std::string rel = state.iconEditorAtlas;
    const bool exists = std::any_of(files.begin(), files.end(), [&](const std::string& f) { return f == rel; });
    if (!exists) {
        ImGui::TextDisabled("%s", rel.empty() ? L("Atlas auswählen.", "Select an atlas.") : (rel + L(" nicht gefunden.", " not found.")).c_str());
        ImGui::EndChild();
        return;
    }
    state.iconEditorScrollToSelected = false;
    const auto& info = Info(state, rel);
    const auto usageIt = usage.byAtlas.find(LowerAscii(AtlasStem(rel)));
    const std::map<int, std::vector<IconUse>> noUse;
    const auto& uses = usageIt == usage.byAtlas.end() ? noUse : usageIt->second;
    const int capacity = core::legacy::IconAtlasCapacity(info.width, info.height);
    int freeCount = 0, emptyFree = 0;
    for (int i = 0; i < capacity; ++i)
        if (!uses.count(i)) {
            ++freeCount;
            if (info.empty[static_cast<std::size_t>(i)]) ++emptyFree;
        }
    ImGui::Text("%s", rel.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s · %s %u px · %d %s · %d %s (%d %s)", info.format ? core::DescribeImageFormat(*info.format).c_str() : "?",
                        L("Zelle", "cell"), info.cell, capacity - freeCount, L("belegt", "used"), freeCount, L("frei", "free"), emptyFree,
                        L("davon leer", "of them empty"));
    if (uses.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "%s",
                           L("In keiner der geprüften Tabellen verwendet (ItemViewInfo, ActiveSkillView, PassiveSkillView, AbStateView) - andere Stellen des Clients können ihn trotzdem nutzen.",
                             "Not used by any checked table (ItemViewInfo, ActiveSkillView, PassiveSkillView, AbStateView) - other parts of the client may still use it."));

    const bool hasProject = state.project.projectFolder[0] != '\0';
    ImGui::BeginDisabled(!hasProject);
    if (UI::SmallButton(L("Atlas ersetzen...", "Replace atlas..."))) OpenImageReplaceDialog(state, rel, rel);
    ImGui::SameLine();
    if (UI::SmallButton(L("Als PNG exportieren##iconAtlas", "Export as PNG##iconAtlas"))) ExportResmenuAsPng(state, rel);
    ImGui::SameLine();
    if (state.iconEditorNewName[0] == '\0' || state.iconEditorNewNameFor != rel) {
        std::snprintf(state.iconEditorNewName, sizeof(state.iconEditorNewName), "%s", SuggestAtlasName(state, AtlasStem(rel)).c_str());
        state.iconEditorNewNameFor = rel;
    }
    ImGui::SetNextItemWidth(150.0f);
    UI::InputText("##iconAtlasNewName", state.iconEditorNewName, sizeof(state.iconEditorNewName));
    ImGui::SameLine();
    const std::string newName = TrimAscii(state.iconEditorNewName);
    const std::string newRel = std::string(std::filesystem::path(rel).parent_path().generic_string()) + "/" + newName +
                               std::filesystem::path(rel).extension().string();
    const bool nameOk = !newName.empty() &&
                        std::all_of(newName.begin(), newName.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }) &&
                        std::none_of(files.begin(), files.end(), [&](const std::string& f) { return LowerAscii(f) == LowerAscii(newRel); });
    ImGui::BeginDisabled(!nameOk || !info.format);
    if (UI::SmallButton(L("Neuer leerer Atlas", "New empty atlas")) && info.format) {
        core::RgbaImage blank;
        blank.width = info.format->width;
        blank.height = info.format->height;
        blank.rgba.assign(std::size_t{blank.width} * blank.height * 4, 0);
        if (auto bytes = core::EncodeImage(blank, *info.format)) {
            if (WriteResmenuOverride(state, newRel, *bytes, L("Neuer Icon-Atlas: ", "New icon atlas: ") + newRel)) {
                state.iconEditorAtlas = newRel;
                state.iconEditorCell = 0;
            }
        } else {
            state.statusMessage = bytes.error();
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    // Raster: Zellen vergrößert; frei = in keiner Tabelle verwendet (grüne Ecke), leer = transparent.
    float aspect = 1.0f;
    const std::uint32_t tex = ResmenuPreviewTexture(state, rel, &aspect);
    const int rows = capacity / 8;
    const float detailW = 300.0f;
    const float cellPx = std::clamp((ImGui::GetContentRegionAvail().x - detailW - 16.0f) / 8.0f, 24.0f, 72.0f);
    ImGui::BeginChild("##iconAtlasGrid", ImVec2(cellPx * 8 + 16.0f, 0.0f), false);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    for (int i = 0; i < capacity && tex; ++i) {
        const float x = origin.x + static_cast<float>(i % 8) * (cellPx + 2.0f), y = origin.y + static_cast<float>(i / 8) * (cellPx + 2.0f);
        const auto cell = core::legacy::IconAtlasCell(info.width, info.height, i);
        const ImVec2 mn(x, y), mx(x + cellPx, y + cellPx);
        DrawCheckerboard(mn, mx);
        dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), mn, mx, ImVec2(cell.u0, cell.v0), ImVec2(cell.u1, cell.v1));
        const bool used = uses.count(i) != 0;
        if (!used) dl->AddTriangleFilled(ImVec2(mx.x - 10, mn.y), ImVec2(mx.x, mn.y), ImVec2(mx.x, mn.y + 10), IM_COL32(70, 220, 120, 230));
        if (i == state.iconEditorCell) dl->AddRect(mn, mx, IM_COL32(32, 221, 242, 255), 2.0f, 0, 2.5f);
        ImGui::SetCursorScreenPos(mn);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##cell", ImVec2(cellPx, cellPx))) state.iconEditorCell = i;
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("#%d", i);
            if (used)
                for (const auto& u : uses.at(i)) ImGui::Text("%s %s (%s)", KindName(u.kind), u.inx.c_str(), u.column);
            else
                ImGui::TextUnformatted(info.empty[static_cast<std::size_t>(i)] ? L("frei und leer", "free and empty") : L("frei (Bild vorhanden)", "free (has an image)"));
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + static_cast<float>(rows) * (cellPx + 2.0f)));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##iconCellDetail", ImVec2(0.0f, 0.0f), true);
    const int sel = state.iconEditorCell;
    if (sel < 0 || sel >= capacity) {
        ImGui::TextDisabled("%s", L("Zelle anklicken.", "Click a cell."));
    } else {
        const std::uint32_t cx = static_cast<std::uint32_t>(sel % 8) * info.cell, cy = static_cast<std::uint32_t>(sel / 8) * info.cell;
        const auto cell = core::legacy::IconAtlasCell(info.width, info.height, sel);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        DrawCheckerboard(p, ImVec2(p.x + 96, p.y + 96));
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), ImVec2(96, 96), ImVec2(cell.u0, cell.v0), ImVec2(cell.u1, cell.v1));
        ImGui::Text(L("%s #%d · %ux%u px bei %u,%u", "%s #%d · %ux%u px at %u,%u"), AtlasStem(rel).c_str(), sel, info.cell, info.cell, cx, cy);
        ImGui::BeginDisabled(!hasProject);
        const bool automation = !state.iconAutomationReplaceSource.empty();
        if (UI::Button(L("Zelle ersetzen...", "Replace cell...")) || automation) {
            OpenImageReplaceDialog(state, rel, AtlasStem(rel) + " #" + std::to_string(sel));
            auto& r = state.imageReplace;
            r.cellMode = true;
            r.cellX = cx;
            r.cellY = cy;
            r.cellW = r.cellH = info.cell;
            r.fit = 1; // Einpassen: Icons nicht beschneiden
            if (automation) std::snprintf(r.sourcePath, sizeof(r.sourcePath), "%s", state.iconAutomationReplaceSource.c_str());
            state.iconAutomationReplaceSource.clear();
        }
        ImGui::EndDisabled();
        const auto it = uses.find(sel);
        if (it == uses.end()) {
            ImGui::TextColored(ImVec4(0.45f, 0.86f, 0.55f, 1.0f), "%s", info.empty[static_cast<std::size_t>(sel)]
                ? L("Frei und leer: kann für ein neues Icon genutzt werden.", "Free and empty: can be used for a new icon.")
                : L("Frei (in keiner Tabelle verwendet), enthält aber ein Bild.", "Free (used by no table), but contains an image."));
        } else {
            ImGui::Text("%s (%zu):", L("Verwendet von", "Used by"), it->second.size());
            for (std::size_t u = 0; u < it->second.size(); ++u) {
                const auto& use = it->second[u];
                ImGui::PushID(static_cast<int>(u));
                if (ImGui::SmallButton((std::string(KindName(use.kind)) + " " + use.inx).c_str())) JumpToUser(state, use);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("#%lld · %s", use.id, use.column);
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

} // namespace nge
