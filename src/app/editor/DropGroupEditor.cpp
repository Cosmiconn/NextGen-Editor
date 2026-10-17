// DropGroupEditor.cpp
// Editor für World/ItemDropGroup.txt: Gruppen (ItemGroupIdx) mit ihren Zeilen (ItemID, Menge,
// Verbesserungsstufen). Die Mob-Drop-Tabelle nennt in den DrItem-Slots Gruppennamen; eine
// Gruppenzeile nennt per ItemID entweder einen DropGroup-Namen aus ItemInfoServer (DropGroupA/B,
// eine Menge von Items) oder direkt einen InxName. CheckSum wird bei jeder Änderung nachgeführt
// (MinQtty + MaxQtty + Upgrade00..15, belegt an allen 970 NA2016-Zeilen).

#include "EditorShared.hpp"

namespace nge {

namespace {
const ImVec4 kGroupBad(1.0f, 0.48f, 0.34f, 1.0f);
const ImVec4 kGroupOk(0.42f, 0.86f, 0.62f, 1.0f);
const ImVec4 kGroupDim(0.62f, 0.66f, 0.72f, 1.0f);

core::legacy::ShineTable* GroupTable(EditorState& state) {
    return state.dropGroupLoaded ? state.dropGroupFile.FindTable("ItemDropGroup") : nullptr;
}
ShineTableAccess GroupAccess(EditorState& state) {
    EditorState* s = &state;
    return [s]() { return GroupTable(*s); };
}

struct RowIssues {
    bool noItem = false;      // ItemID leer
    bool unresolved = false;  // ItemID löst auf kein Item auf (Hinweis: in NA2016 v. a. Event-Gruppen)
    bool qtyRange = false;    // MinQtty > MaxQtty
    bool upgradeSum = false;  // Upgrade-Summe != 1000
    bool checksum = false;    // CheckSum passt nicht
    [[nodiscard]] bool Error() const { return noItem || qtyRange || upgradeSum || checksum; }
};
RowIssues CheckRow(const EditorState& state, const core::legacy::DropGroupColumns& c, const core::legacy::ShineRecord& r) {
    RowIssues out;
    const std::string item = ShineRecordValue(r, c.item);
    out.noItem = core::legacy::DropGroupCatalog::IsEmptySlot(item);
    out.unresolved = !out.noItem && state.dropGroups.ItemsForGroupItemId(item).empty();
    const std::string minQ = ShineRecordValue(r, c.minQty), maxQ = ShineRecordValue(r, c.maxQty);
    out.qtyRange = IsDropUnsignedInteger(minQ) && IsDropUnsignedInteger(maxQ) && std::atoll(minQ.c_str()) > std::atoll(maxQ.c_str());
    if (!c.upgrades.empty()) out.upgradeSum = core::legacy::DropGroupUpgradeSum(c, r) != 1000;
    if (c.checksum >= 0) {
        const auto expected = core::legacy::DropGroupChecksum(c, r);
        out.checksum = !expected || std::to_string(*expected) != ShineRecordValue(r, c.checksum);
    }
    return out;
}

// Gruppen in Dateireihenfolge mit den Indizes ihrer Zeilen.
struct GroupIndex {
    std::vector<std::string> order;
    std::unordered_map<std::string, std::vector<int>> rows;
};
GroupIndex BuildGroupIndex(const core::legacy::ShineTable& t, const core::legacy::DropGroupColumns& c) {
    GroupIndex g;
    for (int i = 0; i < static_cast<int>(t.records.size()); ++i) {
        const std::string name = ShineRecordValue(t.records[static_cast<std::size_t>(i)], c.group);
        auto [it, inserted] = g.rows.try_emplace(name);
        if (inserted) g.order.push_back(name);
        it->second.push_back(i);
    }
    return g;
}

// Schreibt einen Wert in eine Gruppenzeile (CheckSum nachgeführt) - ein Undo-Schritt.
void SetGroupValue(EditorState& state, int record, int column, const std::string& value, const std::string& label) {
    auto* t = GroupTable(state);
    if (!t || record < 0 || record >= static_cast<int>(t->records.size()) || column < 0) return;
    auto updated = t->records[static_cast<std::size_t>(record)];
    if (static_cast<std::size_t>(column) >= updated.values.size()) return;
    updated.values[static_cast<std::size_t>(column)] = value;
    core::legacy::UpdateDropGroupChecksum(core::legacy::FindDropGroupColumns(*t), updated);
    if (ReplaceShineRecordWithHistory(state, GroupAccess(state), record, std::move(updated), &state.dropGroupDirty, label))
        ++state.dropGroupEdits;
}

bool ValidGroupName(const std::string& name) {
    return !name.empty() && name.size() < 40 && name != "-" &&
           std::none_of(name.begin(), name.end(), [](unsigned char ch) { return ch <= ' ' || ch == ';' || ch == '#'; });
}
} // namespace

void EnsureDropGroupFileLoaded(EditorState& state) {
    if (state.dropGroupLoaded || !state.dropGroupLoadError.empty()) return;
    if (state.shineTextRoot.empty()) {
        state.dropGroupLoadError = "Server-Shine-Ordner nicht gefunden.";
        return;
    }
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server,
                                            std::filesystem::path(state.shineTextRoot) / "World" / "ItemDropGroup.txt");
    auto loaded = core::legacy::LoadShineTextFile(path);
    if (!loaded) { state.dropGroupLoadError = loaded.error(); return; }
    const auto* table = loaded->FindTable("ItemDropGroup");
    if (!table || !core::legacy::FindDropGroupColumns(*table).Valid()) {
        state.dropGroupLoadError = "Tabelle ItemDropGroup (ItemGroupIdx/ItemID) fehlt in ItemDropGroup.txt.";
        return;
    }
    state.dropGroupFile = std::move(*loaded);
    state.dropGroupLoaded = true;
    state.dropGroupDirty = false;
    state.dropGroupCatalogKey = ~std::uint64_t{0};
}

void SyncDropGroupCatalog(EditorState& state) {
    const auto* t = GroupTable(state);
    if (!t) return;
    const std::uint64_t key = state.dataHistory.Revision() * 1000003ull + state.dropGroupEdits;
    if (key == state.dropGroupCatalogKey) return;
    state.dropGroupCatalogKey = key;
    state.dropGroups.SetGroups(*t);
}

bool SaveDropGroupFile(EditorState& state) {
    if (!state.dropGroupLoaded) return false;
    auto saved = SaveProjectShineOverride(
        state.dropGroupFile, ProjectServerShinePath(state.project, std::filesystem::path("World") / "ItemDropGroup.txt"));
    if (!saved) {
        state.statusMessage = std::string(L("Drop-Gruppen speichern fehlgeschlagen: ", "Saving drop groups failed: ")) + saved.error();
        return false;
    }
    state.dropGroupDirty = false;
    state.statusMessage = std::string(L("Drop-Gruppen Projekt-Override gespeichert: ", "Drop groups project override saved: ")) + saved->string();
    return true;
}

bool DrawItemPickerPopup(EditorState& state, const char* popupId, std::string& chosen,
                         const std::vector<std::string>* extraNames, const char* extraLabel) {
    bool picked = false;
    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopup(popupId)) return false;
    EnsureItemLookup(state);
    DrawSearchInput("itemPickerFilter", L("Name oder InxName suchen", "Search name or InxName"),
                    state.itemPickerFilter, sizeof(state.itemPickerFilter), 520.0f, ImGui::IsWindowAppearing());
    const std::string needle = LowerAscii(state.itemPickerFilter);
    // Treffer: zuerst die zusätzlichen Namen (z. B. DropGroups), dann Items.
    struct Hit { bool extra; std::size_t index; };
    std::vector<Hit> hits;
    if (extraNames)
        for (std::size_t i = 0; i < extraNames->size(); ++i)
            if (needle.empty() || LowerAscii((*extraNames)[i]).find(needle) != std::string::npos) hits.push_back({true, i});
    for (std::size_t i = 0; i < state.itemEntries.size(); ++i) {
        const auto& e = state.itemEntries[i];
        if (needle.empty() || LowerAscii(e.inx).find(needle) != std::string::npos || LowerAscii(e.name).find(needle) != std::string::npos)
            hits.push_back({false, i});
    }
    ImGui::TextDisabled(L("%zu Treffer", "%zu matches"), hits.size());
    ImGui::BeginChild("##itemPickList", ImVec2(540.0f, 340.0f), true);
    const float icon = ImGui::GetTextLineHeight() + 4.0f;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(hits.size()), icon + ImGui::GetStyle().ItemSpacing.y);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const Hit& h = hits[static_cast<std::size_t>(row)];
            ImGui::PushID(row);
            if (h.extra) {
                const std::string& name = (*extraNames)[h.index];
                const float x = ImGui::GetCursorPosX();
                if (ImGui::Selectable(("##x" + name).c_str(), false, ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, icon))) { chosen = name; picked = true; }
                ImGui::SameLine(x + 4.0f);
                ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "%s", name.c_str());
                if (extraLabel) { ImGui::SameLine(); ImGui::TextDisabled("(%s)", extraLabel); }
            } else {
                const auto& e = state.itemEntries[h.index];
                const float x = ImGui::GetCursorPosX();
                if (ImGui::Selectable(("##i" + e.inx).c_str(), false, ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, icon))) { chosen = e.inx; picked = true; }
                ImGui::SameLine(x + 4.0f);
                DrawItemIconByInx(state, e.inx, icon);
                ImGui::SameLine();
                ImGui::TextUnformatted(e.name.empty() ? e.inx.c_str() : e.name.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s · #%lld", e.inx.c_str(), e.id);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (picked) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return picked;
}

void DrawDropGroupEditor(EditorState& state) {
    EnsureDropGroupCatalog(state); // lädt ItemDropGroup.txt mit
    EnsureDropGroupFileLoaded(state);
    DrawPanelHeader("dropGroupHeader", L("DROP-GRUPPEN / ITEMDROPGROUP", "DROP GROUPS / ITEMDROPGROUP"), DrawIconSpawn,
                    "module.droptable", state.dropGroupDirty ? L("geändert *", "modified *") : "World/ItemDropGroup.txt");
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.dropGroupLoaded || !state.dropGroupDirty);
    if (UI::SmallButton(L("Speichern##dropGroups", "Save##dropGroups"))) SaveDropGroupFile(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (UI::SmallButton(state.dropGroupDirty ? L("Neu laden / verwerfen##dropGroups", "Reload / discard##dropGroups")
                                             : L("Neu laden##dropGroups", "Reload##dropGroups"))) {
        state.dropGroupFile = core::legacy::ShineTextFile{};
        state.dropGroupLoaded = false;
        state.dropGroupDirty = false;
        state.dropGroupLoadError.clear();
        state.dataHistory.Clear(); // Zeilenindizes im Verlauf gelten nach dem Verwerfen nicht mehr
        EnsureDropGroupFileLoaded(state);
        state.dropGroupCatalogKey = ~std::uint64_t{0};
    }
    ImGui::SameLine();
    DrawDataHistoryButtons(state, "dropGroupHistory");
    ImGui::Separator();
    if (!state.dropGroupLoadError.empty()) { ImGui::TextWrapped("%s", state.dropGroupLoadError.c_str()); return; }
    auto* table = GroupTable(state);
    if (!table) { ImGui::TextDisabled("%s", L("ItemDropGroup.txt wird geladen...", "Loading ItemDropGroup.txt...")); return; }
    if (ShortcutPressed(state.shortcutSave) && state.dropGroupDirty) SaveDropGroupFile(state);
    SyncDropGroupCatalog(state);

    const auto cols = core::legacy::FindDropGroupColumns(*table);
    GroupIndex groups = BuildGroupIndex(*table, cols);
    EnsureDropTableLoaded(state);
    const core::legacy::ShineTable* dropTable = state.dropTableLoaded ? state.dropTableFile.FindTable("ItemGroup") : nullptr;
    const std::vector<std::string> dropGroupNames = state.dropGroups.ItemInfoDropGroupNames();
    const ShineTableAccess access = GroupAccess(state);

    // Neue Gruppe.
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##newDropGroup", L("Name der neuen Gruppe", "New group name"), state.dropGroupNewName,
                             sizeof(state.dropGroupNewName));
    ImGui::SameLine();
    const std::string newName = TrimAscii(state.dropGroupNewName);
    const bool nameFree = ValidGroupName(newName) && !groups.rows.contains(newName);
    ImGui::BeginDisabled(!nameFree);
    if (UI::SmallButton(L("+ Gruppe", "+ Group"))) {
        const int at = InsertShineRecordWithHistory(state, access, static_cast<int>(table->records.size()),
                                                    core::legacy::MakeDropGroupRecord(*table, newName, "-"),
                                                    &state.dropGroupDirty,
                                                    std::string(L("Drop-Gruppe angelegt: ", "Drop group added: ")) + newName);
        if (at >= 0) {
            state.dropGroupSelected = newName;
            state.dropGroupNewName[0] = '\0';
            ++state.dropGroupEdits;
        }
        table = GroupTable(state);
        groups = BuildGroupIndex(*table, cols);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", L("Legt eine Gruppe mit einer Zeile an (Menge 1, immer +0). Der Name ist der Wert, den die Mob-Drop-Tabelle in einem DrItem-Slot nennt; er muss eindeutig sein und darf keine Leerzeichen enthalten.",
                                  "Creates a group with one row (quantity 1, always +0). The name is the value the mob drop table names in a DrItem slot; it must be unique and must not contain spaces."));
    ImGui::SameLine();
    DrawSearchInput("dropGroupFilter", L("Gruppe, ItemID oder Item filtern...", "Filter group, ItemID or item..."),
                    state.dropGroupFilter, sizeof(state.dropGroupFilter), 260.0f);
    ImGui::SameLine();
    UI::Checkbox(L("Nur Probleme##dropGroups", "Problems only##dropGroups"), &state.dropGroupProblemsOnly);

    // Gruppenliste mit Status.
    const std::string needle = LowerAscii(state.dropGroupFilter);
    std::vector<const std::string*> visible;
    std::size_t problemGroups = 0;
    std::unordered_map<std::string, bool> groupHasError;
    for (const auto& name : groups.order) {
        bool error = false;
        std::string hay = name;
        for (const int ri : groups.rows[name]) {
            const auto& r = table->records[static_cast<std::size_t>(ri)];
            error = error || CheckRow(state, cols, r).Error();
            if (!needle.empty()) {
                const std::string item = ShineRecordValue(r, cols.item);
                hay += " " + item;
                for (const auto& inx : state.dropGroups.ItemsForGroupItemId(item)) hay += " " + inx + " " + state.dropGroups.DisplayName(inx);
            }
        }
        groupHasError[name] = error;
        if (error) ++problemGroups;
        if (state.dropGroupProblemsOnly && !error) continue;
        if (!needle.empty() && LowerAscii(hay).find(needle) == std::string::npos) continue;
        visible.push_back(&name);
    }
    ImGui::TextDisabled(L("%zu / %zu Gruppen · %zu Zeilen · %zu mit Problemen", "%zu / %zu groups · %zu rows · %zu with problems"),
                        visible.size(), groups.order.size(), table->records.size(), problemGroups);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float leftW = std::clamp(avail.x * 0.28f, 240.0f, 340.0f);
    ImGui::BeginChild("##dropGroupList", ImVec2(leftW, 0), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const std::string& name = *visible[static_cast<std::size_t>(row)];
            const std::size_t n = groups.rows[name].size();
            std::string label = (groupHasError[name] ? "!  " : "") + name;
            if (n > 1) label += "  (" + std::to_string(n) + ")";
            if (UI::Selectable((label + "##grp" + std::to_string(row)).c_str(), state.dropGroupSelected == name))
                state.dropGroupSelected = name;
        }
    }
    if (visible.empty()) ImGui::TextDisabled("%s", L("Keine passenden Gruppen.", "No matching groups."));
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##dropGroupDetails", ImVec2(0, 0), true);
    const auto selectedIt = groups.rows.find(state.dropGroupSelected);
    if (state.dropGroupSelected.empty() || selectedIt == groups.rows.end()) {
        ImGui::TextDisabled("%s", L("Gruppe auswählen oder anlegen.", "Select or create a group."));
        ImGui::EndChild();
        return;
    }
    const std::string group = state.dropGroupSelected;
    const std::vector<int> groupRows = selectedIt->second;
    const std::vector<std::size_t> users = dropTable ? core::legacy::DropTableRecordsUsingGroup(*dropTable, group)
                                                     : std::vector<std::size_t>{};

    ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "%s", group.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(L("· %zu Zeile(n) · von %zu Mob-Drop-Zeile(n) genutzt", "· %zu row(s) · used by %zu mob drop row(s)"),
                        groupRows.size(), users.size());

    // Umbenennen (alle Zeilen der Gruppe, optional die DrItem-Slots der Mob-Drop-Tabelle mit).
    {
        std::string renamed;
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", L("Name", "Name"));
        ImGui::SameLine(90.0f);
        if (DeferredInputText("##groupName", group, renamed, 220.0f)) {
            renamed = TrimAscii(renamed);
            if (!ValidGroupName(renamed) || groups.rows.contains(renamed)) {
                state.statusMessage = L("Name ungültig oder schon vergeben.", "Name invalid or already in use.");
            } else {
                DataHistoryBegin(state, std::string(L("Drop-Gruppe umbenannt: ", "Drop group renamed: ")) + group + " -> " + renamed);
                for (const int ri : groupRows) {
                    auto rec = table->records[static_cast<std::size_t>(ri)];
                    rec.values[static_cast<std::size_t>(cols.group)] = renamed;
                    ReplaceShineRecordWithHistory(state, access, ri, std::move(rec), &state.dropGroupDirty, {});
                }
                int slotsRenamed = 0;
                if (state.dropTableLoaded) {
                    EditorState* s = &state;
                    const ShineTableAccess dropAccess = [s]() { return s->dropTableFile.FindTable("ItemGroup"); };
                    if (auto* dt = dropAccess()) {
                        for (const std::size_t ri : users) {
                            auto rec = dt->records[ri];
                            for (std::size_t c = 0; c < dt->columns.size() && c < rec.values.size(); ++c) {
                                const std::string& colName = dt->columns[c].name;
                                const bool slot = colName.size() > 6 && colName.compare(0, 6, "DrItem") == 0 &&
                                                  std::all_of(colName.begin() + 6, colName.end(), [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
                                if (slot && rec.values[c] == group) { rec.values[c] = renamed; ++slotsRenamed; }
                            }
                            ReplaceShineRecordWithHistory(state, dropAccess, static_cast<int>(ri), std::move(rec), &state.dropTableDirty, {});
                        }
                        state.dropTableTracker = {}; // Schnappschuss der gewählten Mob-Zeile neu aufnehmen
                    }
                }
                DataHistoryCommit(state);
                ++state.dropGroupEdits;
                state.dropGroupSelected = renamed;
                state.statusMessage = std::string(L("Gruppe umbenannt; ", "Group renamed; ")) + std::to_string(slotsRenamed) +
                                      L(" DrItem-Slot(s) in der Mob-Drop-Tabelle mit umbenannt (beide Dateien speichern).",
                                        " DrItem slot(s) in the mob drop table renamed too (save both files).");
                ImGui::EndChild();
                return;
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && !ImGui::IsItemActive())
            ImGui::SetTooltip("%s", L("Umbenennen ändert alle Zeilen der Gruppe und die DrItem-Slots der Mob-Drop-Tabelle, die sie nennen - als ein Schritt (Strg+Z).",
                                      "Renaming changes every row of the group and the DrItem slots of the mob drop table that name it - as one step (Ctrl+Z)."));
        ImGui::SameLine();
        if (UI::SmallButton(L("Gruppe löschen", "Delete group"))) state.dropGroupDeleteOpen = true;
    }

    // Zeilen der Gruppe.
    ImGui::SeparatorText(L("Zeilen", "Rows"));
    const float iconSize = ImGui::GetTextLineHeight() + 2.0f;
    int deleteRow = -1;
    if (ImGui::BeginTable("##dropGroupRows", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn(L("ItemID", "ItemID"), ImGuiTableColumnFlags_WidthFixed, 230.0f);
        ImGui::TableSetupColumn(L("Löst auf", "Resolves to"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(L("Menge", "Qty"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Upgrade", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("CheckSum", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(L("Prüfung", "Check"), ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 26.0f);
        ImGui::TableHeadersRow();
        for (const int ri : groupRows) {
            const auto& rec = table->records[static_cast<std::size_t>(ri)];
            const RowIssues issues = CheckRow(state, cols, rec);
            ImGui::PushID(ri);
            ImGui::TableNextRow();
            const std::string label = std::string(L("Drop-Gruppe ", "Drop group ")) + group;

            ImGui::TableSetColumnIndex(0);
            std::string value;
            if (DeferredInputText("##item", ShineRecordValue(rec, cols.item), value, 170.0f))
                SetGroupValue(state, ri, cols.item, TrimAscii(value), label + ": ItemID");
            ImGui::SameLine();
            if (UI::SmallButton(L("Wählen...", "Pick..."))) {
                state.dropGroupPickerRecord = ri;
                state.itemPickerFilter[0] = '\0';
                ImGui::OpenPopup("##dropGroupItemPicker");
            }
            std::string chosen;
            if (state.dropGroupPickerRecord == ri &&
                DrawItemPickerPopup(state, "##dropGroupItemPicker", chosen, &dropGroupNames, "DropGroup")) {
                SetGroupValue(state, ri, cols.item, chosen, label + ": ItemID");
                state.dropGroupPickerRecord = -1;
            }

            ImGui::TableSetColumnIndex(1);
            const auto items = state.dropGroups.ItemsForGroupItemId(ShineRecordValue(rec, cols.item));
            for (std::size_t i = 0; i < items.size() && i < 8; ++i) {
                DrawItemIconByInx(state, items[i], iconSize);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s)", state.dropGroups.DisplayName(items[i]).c_str(), items[i].c_str());
                ImGui::SameLine(0.0f, 2.0f);
            }
            if (items.size() == 1) ImGui::TextUnformatted(state.dropGroups.DisplayName(items.front()).c_str());
            else if (!items.empty()) {
                ImGui::Text(L("%zu Items", "%zu items"), items.size());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    for (std::size_t i = 0; i < items.size() && i < 25; ++i)
                        tip += state.dropGroups.DisplayName(items[i]) + "  (" + items[i] + ")\n";
                    if (items.size() > 25) tip += "...";
                    ImGui::SetTooltip("%s", tip.c_str());
                }
            } else ImGui::TextColored(kGroupDim, "%s", L("kein Item", "no item"));

            ImGui::TableSetColumnIndex(2);
            if (DeferredInputText("##min", ShineRecordValue(rec, cols.minQty), value, 44.0f))
                SetGroupValue(state, ri, cols.minQty, TrimAscii(value), label + ": MinQtty");
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::TextDisabled("-");
            ImGui::SameLine(0.0f, 2.0f);
            if (DeferredInputText("##max", ShineRecordValue(rec, cols.maxQty), value, 44.0f))
                SetGroupValue(state, ri, cols.maxQty, TrimAscii(value), label + ": MaxQtty");

            ImGui::TableSetColumnIndex(3);
            {
                // Kurzfassung der Verteilung (+Stufe: Promille), Bearbeiten im Popup.
                std::string summary;
                for (std::size_t u = 0; u < cols.upgrades.size(); ++u) {
                    const std::string v = ShineRecordValue(rec, cols.upgrades[u]);
                    if (v.empty() || v == "0") continue;
                    if (!summary.empty()) summary += " ";
                    summary += "+" + std::to_string(u) + ":" + v;
                }
                if (UI::SmallButton((summary.empty() ? std::string("-") : summary).append("##upg").c_str()))
                    ImGui::OpenPopup("##upgradePopup");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", L("Verbesserungsstufe beim Drop: Promille je Stufe +0 bis +15, zusammen 1000.",
                                              "Upgrade level on drop: per mille for each level +0 to +15, 1000 in total."));
                if (ImGui::BeginPopup("##upgradePopup")) {
                    ImGui::TextDisabled("%s", L("Promille je Stufe (Summe 1000)", "Per mille per level (sum 1000)"));
                    for (std::size_t u = 0; u < cols.upgrades.size(); ++u) {
                        ImGui::PushID(static_cast<int>(u));
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("+%zu", u);
                        ImGui::SameLine(40.0f);
                        if (DeferredInputText("##u", ShineRecordValue(rec, cols.upgrades[u]), value, 70.0f))
                            SetGroupValue(state, ri, cols.upgrades[u], TrimAscii(value), label + ": Upgrade");
                        ImGui::PopID();
                        if (u % 2 == 0 && u + 1 < cols.upgrades.size()) ImGui::SameLine(140.0f);
                    }
                    const auto sum = core::legacy::DropGroupUpgradeSum(cols, rec);
                    ImGui::TextColored(sum == 1000 ? kGroupOk : kGroupBad, L("Summe %lld", "Sum %lld"), sum.value_or(-1));
                    ImGui::EndPopup();
                }
            }

            ImGui::TableSetColumnIndex(4);
            ImGui::TextColored(issues.checksum ? kGroupBad : kGroupDim, "%s", ShineRecordValue(rec, cols.checksum).c_str());

            ImGui::TableSetColumnIndex(5);
            if (issues.noItem) ImGui::TextColored(kGroupBad, "%s", L("ItemID fehlt", "ItemID missing"));
            else if (issues.qtyRange) ImGui::TextColored(kGroupBad, "%s", L("Min > Max", "Min > Max"));
            else if (issues.upgradeSum) ImGui::TextColored(kGroupBad, "%s", L("Upgrade-Summe != 1000", "Upgrade sum != 1000"));
            else if (issues.checksum) {
                if (UI::SmallButton(L("CheckSum korrigieren", "Fix CheckSum")))
                    SetGroupValue(state, ri, cols.checksum, ShineRecordValue(rec, cols.checksum), label + ": CheckSum");
            } else if (issues.unresolved) ImGui::TextColored(kGroupDim, "%s", L("löst nicht auf", "does not resolve"));
            else ImGui::TextColored(kGroupOk, "OK");
            if (issues.unresolved && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", L("Die ItemID ist weder ein DropGroup-Name aus ItemInfoServer noch ein InxName - in NA2016 vor allem bei Event-Gruppen, deren Items fehlen.",
                                          "The ItemID is neither a DropGroup name from ItemInfoServer nor an InxName - in NA2016 mostly event groups whose items are missing."));

            ImGui::TableSetColumnIndex(6);
            if (UI::SmallButton("x##delRow")) deleteRow = ri;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", L("Zeile löschen (Strg+Z macht es rückgängig). Die letzte Zeile löscht die ganze Gruppe.",
                                          "Delete row (Ctrl+Z undoes it). The last row deletes the whole group."));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (deleteRow >= 0) {
        if (groupRows.size() <= 1) state.dropGroupDeleteOpen = true;
        else if (DeleteShineRecordWithHistory(state, access, deleteRow, &state.dropGroupDirty,
                                              std::string(L("Zeile aus Drop-Gruppe gelöscht: ", "Row deleted from drop group: ")) + group))
            ++state.dropGroupEdits;
    }
    if (UI::SmallButton(L("+ Zeile##dropGroup", "+ Row##dropGroup"))) {
        const int at = groupRows.empty() ? static_cast<int>(table->records.size()) : groupRows.back() + 1;
        if (InsertShineRecordWithHistory(state, access, at, core::legacy::MakeDropGroupRecord(*table, group, "-"),
                                         &state.dropGroupDirty, std::string(L("Zeile zu Drop-Gruppe: ", "Row added to drop group: ")) + group) >= 0)
            ++state.dropGroupEdits;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", L("Weitere Zeile in dieser Gruppe (direkt hinter ihrer letzten). Jede Zeile nennt ein Item oder eine DropGroup.",
                                  "Another row in this group (right after its last one). Each row names an item or a DropGroup."));

    // Verwendung in der Mob-Drop-Tabelle.
    ImGui::SeparatorText(L("Verwendet von", "Used by"));
    if (!dropTable) ImGui::TextDisabled("%s", L("ItemDropTable.txt nicht geladen.", "ItemDropTable.txt not loaded."));
    else if (users.empty()) ImGui::TextDisabled("%s", L("Kein Mob droppt diese Gruppe.", "No mob drops this group."));
    else {
        const int cMob = FindShineColumn(*dropTable, "MobId");
        for (std::size_t i = 0; i < users.size() && i < 60; ++i) {
            ImGui::PushID(static_cast<int>(i));
            const std::string mob = ShineRecordValue(dropTable->records[users[i]], cMob);
            if (UI::SmallButton((mob.empty() ? std::string("?") : mob).c_str())) {
                state.dropTableSelectedRecord = static_cast<int>(users[i]);
                state.dropEditorTabRequest = 0;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", L("Diese Mob-Zeile in der Drop-Tabelle öffnen.", "Open this mob row in the drop table."));
            ImGui::PopID();
            if ((i + 1) % 6 != 0 && i + 1 < users.size()) ImGui::SameLine();
        }
        if (users.size() > 60) ImGui::TextDisabled(L("... und %zu weitere", "... and %zu more"), users.size() - 60);
    }

    // Gruppe löschen (alle Zeilen, ein Schritt).
    if (state.dropGroupDeleteOpen) {
        const char* title = L("Drop-Gruppe löschen###dropGroupDelete", "Delete drop group###dropGroupDelete");
        ImGui::OpenPopup(title);
        if (ImGui::BeginPopupModal(title, &state.dropGroupDeleteOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(L("Gruppe \"%s\" mit %zu Zeile(n) löschen?", "Delete group \"%s\" with %zu row(s)?"), group.c_str(), groupRows.size());
            if (!users.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f),
                                   L("%zu Mob-Drop-Zeile(n) nennen sie weiter - diese Slots zeigen danach ins Leere.",
                                     "%zu mob drop row(s) still name it - those slots will point to nothing."), users.size());
            ImGui::TextDisabled("%s", L("Strg+Z macht das Löschen rückgängig.", "Ctrl+Z undoes the deletion."));
            if (UI::Button(L("Löschen", "Delete"))) {
                DataHistoryBegin(state, std::string(L("Drop-Gruppe gelöscht: ", "Drop group deleted: ")) + group);
                for (auto it = groupRows.rbegin(); it != groupRows.rend(); ++it)
                    DeleteShineRecordWithHistory(state, access, *it, &state.dropGroupDirty, {});
                DataHistoryCommit(state);
                ++state.dropGroupEdits;
                state.dropGroupSelected.clear();
                state.dropGroupDeleteOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UI::Button(L("Abbrechen", "Cancel"))) { state.dropGroupDeleteOpen = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
}

} // namespace nge
