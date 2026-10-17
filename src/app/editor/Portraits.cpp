// Portraits.cpp
// Porträts im Interface-Bereich: Mob-Porträts (resmenu/game/MobPortrait/<MobViewInfo.MobPortrait>.png,
// NA2016: 2878 Zeilen, 680 Namen) und NPC-Gesichter im Dialog (resmenu/game/NPCFaceCut/
// <NpcDialogData.FaceCutFile>.png). Liste aus Dateien und Verweisen, fehlende Dateien, Verwendungen,
// ersetzen (alle Varianten), fehlende Datei anlegen, neues Porträt mit Zuweisung an einen Mob/NPC.

#include "EditorShared.hpp"

namespace nge {

namespace {
struct PortraitKind {
    const char* folder;
    const char* table;
    const char* keyColumn;
    const char* valueColumn;
    bool serverCopy;
};
const PortraitKind kKinds[2] = {
    {"game/MobPortrait", "MobViewInfo.shn", "InxName", "MobPortrait", true},
    {"game/NPCFaceCut", "NpcDialogData.shn", "MobIDX", "FaceCutFile", false},
};

struct PortraitUser {
    std::string key;
    long long id = -1;
};
struct PortraitEntry {
    std::string name;              // Schreibweise der Datei bzw. des ersten Verweises
    std::vector<std::string> rels; // vorhandene Dateien, PNG zuerst
    std::vector<PortraitUser> users;
};
struct PortraitCatalog {
    std::string key;
    std::map<std::string, PortraitEntry> byLower;
};

int Rank(const std::string& rel) {
    const std::string ext = LowerAscii(std::filesystem::path(rel).extension().string());
    return ext == ".png" ? 0 : ext == ".tga" ? 1 : 2;
}

const PortraitCatalog& Catalog(EditorState& state, int kindIndex) {
    static PortraitCatalog catalogs[2];
    const auto& kind = kKinds[kindIndex];
    auto& c = catalogs[kindIndex];
    const int doc = SharedShnDoc(state, kind.table, EditorState::ShnSource::Client);
    const std::string key = ResmenuSourceRoot(state).string() + "|" + std::to_string(state.resmenuRevision) + "|" +
                            std::to_string(state.dataHistory.Revision()) + "|" + std::to_string(state.shnEditCounter) + "|" +
                            std::to_string(doc);
    if (c.key == key) return c;
    c.key = key;
    c.byLower.clear();
    for (const auto& root : {ResmenuSourceRoot(state), InterfaceProjectOverrideRoot(state)}) {
        if (root.empty()) continue;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(root / kind.folder, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            const std::string ext = LowerAscii(entry.path().extension().string());
            if (ext != ".png" && ext != ".tga" && ext != ".dds") continue;
            auto& e = c.byLower[LowerAscii(entry.path().stem().string())];
            if (e.name.empty()) e.name = entry.path().stem().string();
            const std::string rel = std::string(kind.folder) + "/" + entry.path().filename().string();
            if (std::none_of(e.rels.begin(), e.rels.end(), [&](const std::string& r) { return LowerAscii(r) == LowerAscii(rel); }))
                e.rels.push_back(rel);
        }
    }
    for (auto& [lower, e] : c.byLower)
        std::stable_sort(e.rels.begin(), e.rels.end(), [](const std::string& a, const std::string& b) { return Rank(a) < Rank(b); });
    if (doc >= 0) {
        const auto& f = ShnDocFile(state, doc);
        const bool hasId = ShnColumnIndexByName(f, "ID") >= 0;
        for (std::size_t r = 0; r < f.rows.size(); ++r) {
            const std::string value = TrimAscii(ShnCellText(f, r, kind.valueColumn));
            if (value.empty() || value == "-") continue;
            auto& e = c.byLower[LowerAscii(value)];
            if (e.name.empty()) e.name = value;
            PortraitUser u;
            u.key = TrimAscii(ShnCellText(f, r, kind.keyColumn));
            if (hasId) u.id = std::atoll(TrimAscii(ShnCellText(f, r, "ID")).c_str());
            e.users.push_back(std::move(u));
        }
    }
    return c;
}

// Spalte valueColumn aller Zeilen mit keyColumn == key setzen (Client und ggf. Server, ein Undo-Schritt).
int AssignPortrait(EditorState& state, int kindIndex, const std::string& key, const std::string& value) {
    const auto& kind = kKinds[kindIndex];
    int written = 0;
    DataHistoryBegin(state, std::string(kind.valueColumn) + " " + key + " -> " + value);
    for (const auto source : {EditorState::ShnSource::Client, EditorState::ShnSource::Server}) {
        if (source == EditorState::ShnSource::Server && !kind.serverCopy) continue;
        const int doc = SharedShnDoc(state, kind.table, source);
        if (doc < 0) continue;
        auto& f = ShnDocFile(state, doc);
        const int col = ShnColumnIndexByName(f, kind.valueColumn);
        if (col < 0) continue;
        for (std::size_t r = 0; r < f.rows.size(); ++r)
            if (LowerAscii(TrimAscii(ShnCellText(f, r, kind.keyColumn))) == LowerAscii(key) &&
                ApplyShnCellText(state, doc, static_cast<int>(r), col, value, true))
                ++written;
    }
    DataHistoryCommit(state);
    state.statusMessage = std::string(kind.valueColumn) + " " + key + " -> " + value + " (" + std::to_string(written) + ")";
    return written;
}

std::string TemplateFor(const PortraitCatalog& c, const PortraitEntry* preferred) {
    if (preferred)
        for (const auto& rel : preferred->rels)
            if (Rank(rel) == 0) return rel;
    for (const auto& [lower, e] : c.byLower)
        for (const auto& rel : e.rels)
            if (Rank(rel) == 0) return rel;
    return {};
}

void DrawPreview(EditorState& state, const std::string& rel, float maxSide) {
    float aspect = 1.0f;
    const std::uint32_t tex = rel.empty() ? 0 : ResmenuPreviewTexture(state, rel, &aspect);
    ImVec2 size(maxSide, maxSide);
    if (aspect > 1.0f) size.y = maxSide / aspect;
    else size.x = maxSide * aspect;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (tex) {
        DrawCheckerboard(p, ImVec2(p.x + size.x, p.y + size.y));
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), size);
    } else {
        ImGui::GetWindowDrawList()->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(200, 90, 70, 255));
        ImGui::Dummy(size);
    }
}
} // namespace

int AssignPortraitName(EditorState& state, int kind, const std::string& key, const std::string& name) {
    return AssignPortrait(state, std::clamp(kind, 0, 1), key, name);
}

std::vector<std::string> PortraitUsers(EditorState& state, int kind, const std::string& name) {
    std::vector<std::string> out;
    const auto& c = Catalog(state, std::clamp(kind, 0, 1));
    if (const auto it = c.byLower.find(LowerAscii(name)); it != c.byLower.end())
        for (const auto& u : it->second.users) out.push_back(u.key);
    return out;
}

void OpenPortraitEditor(EditorState& state, int kind, const std::string& name) {
    state.screen = AppScreen::ShnEditor;
    state.shnSubTab = 9;
    state.interfaceMode = 3;
    state.portraitKind = std::clamp(kind, 0, 1);
    state.portraitSelected = name;
    state.portraitScrollToSelected = true;
}

void DrawPortraitEditor(EditorState& state) {
    if (ResmenuSourceRoot(state).empty()) {
        ImGui::TextDisabled("%s", L("Kein Client/resmenu gefunden.", "No Client/resmenu found."));
        return;
    }
    if (UI::RadioButton(L("Mob-Porträts##portraitKind", "Mob portraits##portraitKind"), state.portraitKind == 0)) state.portraitKind = 0;
    ImGui::SameLine();
    if (UI::RadioButton(L("NPC-Gesichter (Dialog)##portraitKind", "NPC faces (dialog)##portraitKind"), state.portraitKind == 1)) state.portraitKind = 1;
    const int kindIndex = std::clamp(state.portraitKind, 0, 1);
    const auto& kind = kKinds[kindIndex];
    const auto& catalog = Catalog(state, kindIndex);
    int missing = 0, unused = 0;
    for (const auto& [lower, e] : catalog.byLower) {
        if (e.rels.empty()) ++missing;
        if (e.users.empty()) ++unused;
    }
    ImGui::SameLine();
    DrawDataHistoryButtons(state, "portraitHistory");
    ImGui::SameLine();
    ImGui::BeginDisabled(DirtyShnDocumentCount(state) == 0);
    if (UI::Button(L("Alle geänderten SHN speichern##portrait", "Save all changed SHN##portrait"))) SaveAllDirtyShnDocuments(state);
    ImGui::EndDisabled();
    ImGui::TextDisabled("%s/%s.png · %s.%s · %zu %s · %d %s · %d %s", kind.folder, "<Name>", kind.table, kind.valueColumn,
                        catalog.byLower.size(), L("Namen", "names"), missing, L("ohne Datei", "without file"), unused,
                        L("Dateien ohne Verweis", "files without reference"));

    const float listW = std::clamp(ImGui::GetContentRegionAvail().x * 0.3f, 240.0f, 380.0f);
    ImGui::BeginChild("##portraitList", ImVec2(listW, 0.0f), true);
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputTextWithHint("##portraitFilter", L("Name, Mob oder NPC filtern...", "Filter name, mob or NPC..."), state.portraitFilter,
                          sizeof(state.portraitFilter));
    UI::Checkbox(L("Nur fehlende Dateien##portrait", "Missing files only##portrait"), &state.portraitMissingOnly);
    const std::string needle = LowerAscii(state.portraitFilter);
    for (const auto& [lower, e] : catalog.byLower) {
        if (state.portraitMissingOnly && !e.rels.empty()) continue;
        if (!needle.empty()) {
            bool hit = lower.find(needle) != std::string::npos;
            for (std::size_t u = 0; !hit && u < e.users.size(); ++u) hit = LowerAscii(e.users[u].key).find(needle) != std::string::npos;
            if (!hit) continue;
        }
        const bool selected = LowerAscii(state.portraitSelected) == lower;
        if (UI::Selectable((e.name + "##portrait").c_str(), selected, 0, ImVec2(listW - 90.0f, 0.0f))) state.portraitSelected = e.name;
        if (selected && state.portraitScrollToSelected) {
            ImGui::SetScrollHereY(0.3f);
            state.portraitScrollToSelected = false;
        }
        ImGui::SameLine(listW - 80.0f);
        if (e.rels.empty()) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", L("fehlt", "missing"));
        else if (e.users.empty()) ImGui::TextDisabled("%s", L("unbenutzt", "unused"));
        else ImGui::TextDisabled("x%zu", e.users.size());
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##portraitDetail", ImVec2(0.0f, 0.0f), true);
    const auto it = catalog.byLower.find(LowerAscii(state.portraitSelected));
    const PortraitEntry* entry = it == catalog.byLower.end() ? nullptr : &it->second;
    const bool hasProject = state.project.projectFolder[0] != '\0';
    if (entry) {
        ImGui::Text("%s", entry->name.c_str());
        DrawPreview(state, entry->rels.empty() ? std::string() : entry->rels.front(), 220.0f);
        ImGui::SameLine();
        ImGui::BeginGroup();
        for (const auto& rel : entry->rels) {
            auto fmt = core::InspectImageFile(ResmenuEffectivePath(state, rel));
            ImGui::TextDisabled("%s · %s%s", rel.c_str(), fmt ? core::DescribeImageFormat(*fmt).c_str() : "?",
                                IsRegularFileNoThrow(InterfaceProjectOverridePath(state, rel)) ? L(" · Projektkopie", " · project copy") : "");
        }
        ImGui::BeginDisabled(!hasProject);
        if (!entry->rels.empty()) {
            if (UI::Button(L("Ersetzen...##portrait", "Replace...##portrait"))) {
                OpenImageReplaceDialog(state, entry->rels.front(), entry->name);
                state.imageReplace.extraTargets.assign(entry->rels.begin() + 1, entry->rels.end());
            }
            if (entry->users.size() > 1) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), L("betrifft %zu Einträge", "affects %zu entries"), entry->users.size());
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s %s/%s.png", L("Datei fehlt:", "File missing:"), kind.folder, entry->name.c_str());
            if (UI::Button(L("Datei anlegen...##portrait", "Create file...##portrait")))
                OpenImageReplaceDialog(state, std::string(kind.folder) + "/" + entry->name + ".png", entry->name, TemplateFor(catalog, nullptr));
        }
        ImGui::EndDisabled();
        ImGui::EndGroup();
        if (!entry->users.empty()) {
            ImGui::Text("%s (%zu):", L("Verwendet von", "Used by"), entry->users.size());
            const std::size_t shown = std::min<std::size_t>(entry->users.size(), 60);
            for (std::size_t u = 0; u < shown; ++u) {
                const auto& user = entry->users[u];
                if (u % 4 != 0) ImGui::SameLine();
                ImGui::PushID(static_cast<int>(u));
                if (kindIndex == 0 && user.id >= 0) {
                    if (ImGui::SmallButton(user.key.c_str())) {
                        state.mobForm.selectedId = user.id;
                        state.shnSubTab = 12;
                    }
                } else {
                    ImGui::TextUnformatted(user.key.c_str());
                }
                ImGui::PopID();
            }
            if (shown < entry->users.size()) ImGui::TextDisabled("... +%zu", entry->users.size() - shown);
        }
        ImGui::Separator();
    } else {
        ImGui::TextDisabled("%s", L("Porträt auswählen.", "Select a portrait."));
    }

    // Neues Porträt: Datei <Ordner>/<Name>.png im Format eines vorhandenen PNG; optional gleich einem
    // Mob/NPC zuweisen (ein Undo-Schritt für Datei und Tabelle).
    ImGui::TextUnformatted(L("Neues Porträt", "New portrait"));
    ImGui::SetNextItemWidth(180.0f);
    UI::InputTextWithHint("##portraitNewName", L("Dateiname", "File name"), state.portraitNewName, sizeof(state.portraitNewName));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    UI::InputTextWithHint("##portraitAssignTo", kindIndex == 0 ? L("für Mob (InxName)", "for mob (InxName)") : L("für NPC (MobIDX)", "for NPC (MobIDX)"),
                          state.portraitAssignTo, sizeof(state.portraitAssignTo));
    ImGui::SameLine();
    const std::string newName = TrimAscii(state.portraitNewName);
    const std::string assignTo = TrimAscii(state.portraitAssignTo);
    const auto existing = catalog.byLower.find(LowerAscii(newName));
    const bool nameOk = !newName.empty() &&
                        std::all_of(newName.begin(), newName.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-'; }) &&
                        (existing == catalog.byLower.end() || existing->second.rels.empty());
    ImGui::BeginDisabled(!nameOk || !hasProject);
    if (UI::Button(L("Anlegen...##portraitNew", "Create...##portraitNew"))) {
        OpenImageReplaceDialog(state, std::string(kind.folder) + "/" + newName + ".png", newName, TemplateFor(catalog, entry));
        if (!assignTo.empty())
            state.imageReplace.onApply = [kindIndex, assignTo, newName](EditorState& s) { AssignPortrait(s, kindIndex, assignTo, newName); };
        state.portraitSelected = newName;
    }
    ImGui::EndDisabled();
    if (!newName.empty() && !nameOk)
        ImGui::TextDisabled("%s", L("Name vergeben oder ungültig (Buchstaben, Ziffern, _ und -).", "Name taken or invalid (letters, digits, _ and -)."));
    ImGui::EndChild();
}

} // namespace nge
