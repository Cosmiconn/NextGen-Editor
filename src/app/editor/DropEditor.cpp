// DropEditor.cpp
// Drop-Tabellen-Editor.

#include "EditorShared.hpp"

namespace nge {

int FindShineColumn(const core::legacy::ShineTable& table, const std::string& name) {
    for (std::size_t i = 0; i < table.columns.size(); ++i)
        if (table.columns[i].name == name) return static_cast<int>(i);
    return -1;
}
std::string ShineRecordValue(const core::legacy::ShineRecord& record, int column) {
    if (column < 0 || static_cast<std::size_t>(column) >= record.values.size()) return {};
    return record.values[static_cast<std::size_t>(column)];
}
bool IsDropUnsignedInteger(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch >= '0' && ch <= '9';
    });
}
void EnsureDropTableLoaded(EditorState& state) {
    if (state.dropTableLoaded || !state.dropTableLoadError.empty()) return;
    if (state.shineTextRoot.empty()) {
        state.dropTableLoadError = "Server-Shine-Ordner nicht gefunden.";
        return;
    }
    const auto sourcePath = std::filesystem::path(state.shineTextRoot) / "World" / "ItemDropTable.txt";
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, sourcePath);
    auto loaded = core::legacy::LoadShineTextFile(path);
    if (!loaded) {
        state.dropTableLoadError = loaded.error();
        return;
    }
    auto* table = loaded->FindTable("ItemGroup");
    if (!table) {
        state.dropTableLoadError = "Tabelle ItemGroup fehlt in ItemDropTable.txt.";
        return;
    }
    if (table->columns.size() < 200) {
        state.dropTableLoadError = "ItemGroup-Schema unerwartet klein: " +
                                   std::to_string(table->columns.size()) + " Spalten.";
        return;
    }
    state.dropTableFile = std::move(*loaded);
    state.dropTableLoaded = true;
    state.dropTableDirty = false;
    state.dropTableSelectedRecord = table->records.empty() ? -1 : 0;
}
// Baut einmalig den Katalog für die Auflösung der DrItem-Gruppen auf: Gruppen aus der gemeinsamen
// Kopie von World/ItemDropGroup.txt (die der Gruppen-Editor bearbeitet), ItemInfoServer.shn
// (DropGroupA/B) und die ItemInfo-Namen. Fehlt eine Datei, bleibt die Prüfung "unbekannt".
void EnsureDropGroupCatalog(EditorState& state) {
    if (state.dropGroupsTried) { SyncDropGroupCatalog(state); return; }
    state.dropGroupsTried = true;
    if (state.shineTextRoot.empty()) return;
    EnsureDropGroupFileLoaded(state);
    if (!state.dropGroupLoaded) {
        state.dropGroupsStatus = "ItemDropGroup.txt: " + state.dropGroupLoadError;
        return;
    }
    const std::string serverRoot = state.shnServerRoot.empty() ? state.shineTextRoot : state.shnServerRoot;
    const int server = SharedShnDoc(state, "ItemInfoServer.shn", EditorState::ShnSource::Server, serverRoot);
    if (server >= 0) state.dropGroups.AddItemInfoServer(ShnDocFile(state, server));
    else state.dropGroupsStatus = "ItemInfoServer.shn nicht gefunden";
    EnsureItemLookup(state);
    for (const auto& item : state.itemEntries) state.dropGroups.AddItem(item.inx, item.name);
    SyncDropGroupCatalog(state);
}
bool SaveDropTable(EditorState& state) {
    if (!state.dropTableLoaded || state.dropTableFile.path.empty()) return false;
    auto saved = SaveProjectShineOverride(
        state.dropTableFile,
        ProjectServerShinePath(state.project, std::filesystem::path("World") / "ItemDropTable.txt"));
    if (!saved) {
        state.statusMessage = "Drop Table speichern fehlgeschlagen: " + saved.error();
        return false;
    }
    state.dropTableDirty = false;
    state.statusMessage = "Drop Table Projekt-Override gespeichert: " + saved->string();
    return true;
}
bool EditDropTableValue(const char* id, std::string& value, float width) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    if (width > 0.0f) ImGui::SetNextItemWidth(width);
    if (!UI::InputText(id, buffer, sizeof(buffer))) return false;
    value = buffer;
    return true;
}
void DrawDropTableEditor(EditorState& state) {
    // Zwei Ansichten: Mob-Drop-Tabelle (ItemDropTable.txt) und Drop-Gruppen (ItemDropGroup.txt).
    const int tabRequest = state.dropEditorTabRequest;
    state.dropEditorTabRequest = -1;
    if (ImGui::BeginTabBar("##dropTabs")) {
        if (ImGui::BeginTabItem(L("Mob-Drops","Mob drops"), nullptr, tabRequest == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            state.dropEditorTab = 0;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L("Drop-Gruppen","Drop groups"), nullptr, tabRequest == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            state.dropEditorTab = 1;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (state.dropEditorTab != 0) {
        DrawDropGroupEditor(state);
        return;
    }
    EnsureDropTableLoaded(state);
    DrawPanelHeader("dropTableHeader", "DROP TABLE / ITEMGROUP", DrawIconSpawn,
                    "module.droptable",
                    state.dropTableDirty ? L("geändert *","modified *") : L("290-Spalten-Schema","290-column schema"));
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.dropTableLoaded || !state.dropTableDirty);
    if (UI::SmallButton(L("Speichern##dropTable","Save##dropTable"))) SaveDropTable(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (UI::SmallButton(state.dropTableDirty ? L("Neu laden / verwerfen##dropTable","Reload / discard##dropTable") : L("Neu laden##dropTable","Reload##dropTable"))) {
        state.dropTableLoaded = false;
        state.dropTableDirty = false;
        state.dropTableFile = core::legacy::ShineTextFile{};
        state.dropTableLoadError.clear();
        state.dropTableSelectedRecord = -1;
        state.dropTableTracker = {};
        state.dataHistory.Clear(); // Zeilenindizes im Verlauf gelten nach dem Verwerfen nicht mehr
        EnsureDropTableLoaded(state);
    }
    ImGui::SameLine();
    DrawDataHistoryButtons(state, "dropHistory");
    ImGui::Separator();

    if (!state.dropTableLoadError.empty()) {
        ImGui::TextWrapped("%s", state.dropTableLoadError.c_str());
        return;
    }
    if (!state.dropTableLoaded) {
        ImGui::TextDisabled("%s",L("ItemDropTable.txt wird geladen...","Loading ItemDropTable.txt..."));
        return;
    }
    if (ShortcutPressed(state.shortcutSave) && state.dropTableDirty)
        SaveDropTable(state);

    auto* table = state.dropTableFile.FindTable("ItemGroup");
    if (!table) {
        ImGui::TextDisabled("ItemGroup-Tabelle fehlt.");
        return;
    }
    EditorState* statePtr = &state;
    const ShineTableAccess dropAccess = [statePtr]() { return statePtr->dropTableFile.FindTable("ItemGroup"); };
    TrackShineRecordEdits(state, state.dropTableTracker, dropAccess, state.dropTableSelectedRecord,
                          &state.dropTableDirty, L("Drop-Zeile", "Drop row"));
    // Zeilen anlegen (Kopie der gewählten oder leere Zeile) und löschen - beides mit Undo.
    if (UI::SmallButton(L("+ Drop-Zeile","+ Drop row"))) {
        core::legacy::ShineRecord rec;
        const bool fromSelected = state.dropTableSelectedRecord >= 0 &&
                                  state.dropTableSelectedRecord < static_cast<int>(table->records.size());
        if (fromSelected) rec = table->records[static_cast<std::size_t>(state.dropTableSelectedRecord)];
        else rec.values.assign(table->columns.size(), "-");
        rec.sourceLine = 0; // neue Zeile (wird beim Speichern hinter der Tabelle angefügt)
        const int at = InsertShineRecordWithHistory(state, dropAccess, static_cast<int>(table->records.size()), rec,
                                                    &state.dropTableDirty,
                                                    fromSelected ? L("Drop-Zeile kopiert","Drop row copied") : L("Drop-Zeile angelegt","Drop row added"));
        if (at >= 0) state.dropTableSelectedRecord = at;
        table = state.dropTableFile.FindTable("ItemGroup");
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", L("Neue Zeile am Ende der Tabelle: Kopie der gewählten Zeile (oder leer). MobId und Slots danach anpassen.",
                                  "New row at the end of the table: copy of the selected row (or empty). Then adjust MobId and slots."));
    ImGui::SameLine();
    ImGui::BeginDisabled(state.dropTableSelectedRecord < 0 || state.dropTableSelectedRecord >= static_cast<int>(table->records.size()));
    if (UI::SmallButton(L("Drop-Zeile löschen","Delete drop row"))) {
        state.dropTableDeleteRecord = state.dropTableSelectedRecord;
        state.dropTableDeleteOpen = true;
    }
    ImGui::EndDisabled();
    if (state.dropTableDeleteOpen) {
        const char* title = L("Drop-Zeile löschen###dropDelete","Delete drop row###dropDelete");
        ImGui::OpenPopup(title);
        if (ImGui::BeginPopupModal(title, &state.dropTableDeleteOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            const bool valid = state.dropTableDeleteRecord >= 0 && state.dropTableDeleteRecord < static_cast<int>(table->records.size());
            if (valid) {
                const int cMobId = FindShineColumn(*table, "MobId");
                ImGui::Text(L("Drop-Zeile für %s löschen?","Delete drop row for %s?"),
                            ShineRecordValue(table->records[static_cast<std::size_t>(state.dropTableDeleteRecord)], cMobId).c_str());
                ImGui::TextDisabled("%s", L("Der Mob droppt danach nichts mehr aus dieser Zeile. Strg+Z macht es rückgängig.",
                                            "The mob then no longer drops anything from this row. Ctrl+Z undoes it."));
            }
            if (UI::Button(L("Löschen","Delete")) && valid) {
                DeleteShineRecordWithHistory(state, dropAccess, state.dropTableDeleteRecord, &state.dropTableDirty,
                                             L("Drop-Zeile gelöscht","Drop row deleted"));
                state.dropTableSelectedRecord = -1;
                state.dropTableTracker = {};
                state.dropTableDeleteOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UI::Button(L("Abbrechen","Cancel"))) { state.dropTableDeleteOpen = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        table = state.dropTableFile.FindTable("ItemGroup");
        if (!table) return;
    }

    const int cMap = FindShineColumn(*table, "MapArea");
    const int cMob = FindShineColumn(*table, "MobId");
    const int cMinLevel = FindShineColumn(*table, "MinLevel");
    const int cMaxLevel = FindShineColumn(*table, "MaxLevel");
    const int cMinCen = FindShineColumn(*table, "MinCen");
    const int cMaxCen = FindShineColumn(*table, "MaxCen");
    const int cCenRate = FindShineColumn(*table, "CenRate");
    const int cChecksum = FindShineColumn(*table, "CheckSum");

    EnsureItemLookup(state);
    EnsureDropGroupCatalog(state);
    EnsureMobViewInfoLoaded(state);
    std::unordered_set<std::string> knownMobs;
    if (state.mobViewInfoLoaded) {
        for (const auto& row : ShnDocFile(state, state.mobViewInfoDoc).rows) {
            if (row.values.size() <= 1) continue;
            const std::string name = core::legacy::ShnValueToString(row.values[1]);
            if (!name.empty()) knownMobs.insert(name);
        }
    }

    std::array<int,45> dropItemColumns{};
    std::array<int,45> dropUpgradeMinColumns{};
    std::array<int,45> dropUpgradeMaxColumns{};
    dropItemColumns.fill(-1);
    dropUpgradeMinColumns.fill(-1);
    dropUpgradeMaxColumns.fill(-1);
    for (int slotNo=1; slotNo<=45; ++slotNo) {
        const std::size_t slotIndex = static_cast<std::size_t>(slotNo-1);
        dropItemColumns[slotIndex] = FindShineColumn(*table, "DrItem" + std::to_string(slotNo));
        char twoDigit[4];
        std::snprintf(twoDigit, sizeof(twoDigit), "%02d", slotNo);
        dropUpgradeMinColumns[slotIndex] =
            FindShineColumn(*table, "UpGradeMin" + std::string(twoDigit));
        dropUpgradeMaxColumns[slotIndex] =
            FindShineColumn(*table, "UpGradeMax" + std::string(twoDigit));
    }
    std::array<int,5> exclusionColumns{};
    exclusionColumns.fill(-1);
    for (int i=1; i<=5; ++i)
        exclusionColumns[static_cast<std::size_t>(i-1)] =
            FindShineColumn(*table, "ExcItem" + std::to_string(i));

    DrawSearchInput("dropTableFilter",
                    L("Mob, MapArea oder Drop-Item filtern...","Filter mob, MapArea or drop item..."),
                    state.dropTableFilter,sizeof(state.dropTableFilter),300.0f);
    ImGui::SameLine();
    UI::Checkbox(L("Nur Probleme##dropTable","Problems only##dropTable"), &state.dropTableProblemsOnly);
    const std::string needle = LowerAscii(state.dropTableFilter);

    struct DropRecordValidation {
        bool missingMob = false;
        int missingDropItems = 0;  // Gruppenname fehlt in ItemDropGroup.txt
        int emptyDropGroups = 0;   // Gruppe ohne auflösbares Item (Hinweis, kein Fehler)
        int missingExclusionItems = 0;
        bool levelRange = false;
        bool cenRange = false;
        int upgradeRanges = 0;
        bool checksum = false;
        bool Any() const {
            return missingMob || missingDropItems > 0 || missingExclusionItems > 0 ||
                   levelRange || cenRange || upgradeRanges > 0 || checksum;
        }
    };

    auto invalidOrderedRange = [&](const core::legacy::ShineRecord& candidate, int minCol, int maxCol) {
        const std::string minText = ShineRecordValue(candidate, minCol);
        const std::string maxText = ShineRecordValue(candidate, maxCol);
        return IsDropUnsignedInteger(minText) && IsDropUnsignedInteger(maxText) &&
               std::atoll(minText.c_str()) > std::atoll(maxText.c_str());
    };

    auto validateDropRecord = [&](const core::legacy::ShineRecord& candidate) {
        DropRecordValidation result;
        const std::string mob = ShineRecordValue(candidate,cMob);
        result.missingMob =
            !mob.empty() && mob != "-" && !knownMobs.empty() && knownMobs.count(mob)==0;

        for (std::size_t slot=0; slot<dropItemColumns.size(); ++slot) {
            const std::string item = ShineRecordValue(candidate,dropItemColumns[slot]);
            const auto status = state.dropGroups.Check(item);
            if (status == core::legacy::DropSlotStatus::Empty) continue;
            // DrItem nennt eine ItemDropGroup-Gruppe, kein Item (NA2016: 29009/29402 Slots).
            if (status == core::legacy::DropSlotStatus::MissingGroup) ++result.missingDropItems;
            else if (status == core::legacy::DropSlotStatus::GroupWithoutItems) ++result.emptyDropGroups;
            if (invalidOrderedRange(candidate,
                                    dropUpgradeMinColumns[slot],
                                    dropUpgradeMaxColumns[slot]))
                ++result.upgradeRanges;
        }
        if (!state.itemEntries.empty()) {
            for (const int col : exclusionColumns) {
                const std::string item = ShineRecordValue(candidate,col);
                if (!core::legacy::DropGroupCatalog::IsEmptySlot(item) && state.itemByInx.count(item)==0)
                    ++result.missingExclusionItems;
            }
        }

        result.levelRange = invalidOrderedRange(candidate,cMinLevel,cMaxLevel);
        result.cenRange = invalidOrderedRange(candidate,cMinCen,cMaxCen);

        const std::string maxLevelText=ShineRecordValue(candidate,cMaxLevel);
        const std::string checkText=ShineRecordValue(candidate,cChecksum);
        result.checksum =
            IsDropUnsignedInteger(maxLevelText) && IsDropUnsignedInteger(checkText) &&
            std::atoi(checkText.c_str()) != std::atoi(maxLevelText.c_str()) + 1;
        return result;
    };

    auto dropValidationSummary = [&](const DropRecordValidation& validation) {
        std::vector<std::string> issues;
        if (validation.missingMob) issues.push_back("MobViewInfo");
        if (validation.missingDropItems > 0)
            issues.push_back("DropGroup x" + std::to_string(validation.missingDropItems));
        if (validation.missingExclusionItems > 0)
            issues.push_back("ExcItem x" + std::to_string(validation.missingExclusionItems));
        if (validation.levelRange) issues.push_back("MinLevel>MaxLevel");
        if (validation.cenRange) issues.push_back("MinCen>MaxCen");
        if (validation.upgradeRanges > 0)
            issues.push_back("UpgradeRange x" + std::to_string(validation.upgradeRanges));
        if (validation.checksum) issues.push_back("CheckSum");
        std::string result;
        for (std::size_t i=0; i<issues.size(); ++i) {
            if (i) result += ", ";
            result += issues[i];
        }
        return result;
    };

    std::vector<std::size_t> visible;
    visible.reserve(table->records.size());
    std::size_t referenceProblemCount=0;
    for (std::size_t ri = 0; ri < table->records.size(); ++ri) {
        const auto& candidate = table->records[ri];
        const bool problem=validateDropRecord(candidate).Any();
        if(problem) ++referenceProblemCount;
        if(state.dropTableProblemsOnly && !problem) continue;

        const std::string mob = ShineRecordValue(candidate, cMob);
        const std::string map = ShineRecordValue(candidate, cMap);
        std::string haystack=mob+" "+map;
        if(!needle.empty()) {
            for(const int col:dropItemColumns) {
                const std::string item=ShineRecordValue(candidate,col);
                if(!item.empty()&&item!="-") haystack+=" "+item;
            }
        }
        if (needle.empty() || LowerAscii(haystack).find(needle) != std::string::npos)
            visible.push_back(ri);
    }

    ImGui::TextDisabled("%zu / %zu Mobs · %zu Schema-Spalten%s · %zu mit Problemen",
                        visible.size(), table->records.size(), table->columns.size(),
                        table->trailingSemicolonSentinel ? " · ; Sentinel normalisiert" : "",
                        referenceProblemCount);
    if (referenceProblemCount > 0) {
        ImGui::SameLine();
        if (UI::SmallButton(L("Problemliste kopieren##dropTable",
                              "Copy issue list##dropTable"))) {
            std::string report;
            for (std::size_t ri=0; ri<table->records.size(); ++ri) {
                const auto& candidate=table->records[ri];
                const auto validation=validateDropRecord(candidate);
                if (!validation.Any()) continue;
                const std::string mob=ShineRecordValue(candidate,cMob);
                const std::string map=ShineRecordValue(candidate,cMap);
                if (!report.empty()) report += "\n";
                report += mob.empty() ? ("Record " + std::to_string(ri)) : mob;
                if (!map.empty() && map != "-") report += " [" + map + "]";
                report += ": " + dropValidationSummary(validation);
            }
            ImGui::SetClipboardText(report.c_str());
            state.statusMessage =
                std::to_string(referenceProblemCount) +
                L(" Drop-Table-Probleme kopiert."," drop table issues copied.");
        }
    }
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float leftW = std::clamp(avail.x * 0.30f, 280.0f, 390.0f);

    ImGui::BeginChild("##dropMobList", ImVec2(leftW, 0), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const std::size_t ri = visible[static_cast<std::size_t>(row)];
            const auto& record = table->records[ri];
            const std::string mob = ShineRecordValue(record, cMob);
            const std::string map = ShineRecordValue(record, cMap);
            std::string label = mob.empty() ? ("Record " + std::to_string(ri)) : mob;
            if (!map.empty() && map != "-") label += "  ·  " + map;
            if (validateDropRecord(record).Any()) label = "!  " + label;
            if (UI::Selectable((label + "##dropMob").c_str(),
                               state.dropTableSelectedRecord == static_cast<int>(ri))) {
                state.dropTableSelectedRecord = static_cast<int>(ri);
            }
        }
    }
    if (visible.empty()) ImGui::TextDisabled("%s",L("Keine passenden Mobs.","No matching mobs."));
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##dropDetails", ImVec2(0,0), true);
    if (state.dropTableSelectedRecord < 0 ||
        state.dropTableSelectedRecord >= static_cast<int>(table->records.size())) {
        ImGui::TextDisabled("%s",L("Mob auswählen.","Select a mob."));
        ImGui::EndChild();
        return;
    }

    auto& record = table->records[static_cast<std::size_t>(state.dropTableSelectedRecord)];
    const std::string mob = ShineRecordValue(record, cMob);
    ImGui::TextColored(ImVec4(0.55f,0.82f,1.0f,1.0f), "%s",
                       mob.empty() ? "(unbenannter Mob)" : mob.c_str());
    if (!mob.empty() && mob != "-" && !knownMobs.empty()) {
        ImGui::SameLine();
        if (knownMobs.count(mob))
            ImGui::TextColored(ImVec4(0.42f,0.86f,0.62f,1.0f), "MobViewInfo OK");
        else
            ImGui::TextColored(ImVec4(1.0f,0.48f,0.34f,1.0f), "%s",L("nicht in MobViewInfo","not in MobViewInfo"));
    }
    ImGui::SameLine();
    const std::string map = ShineRecordValue(record, cMap);
    if (!map.empty() && map != "-") ImGui::TextDisabled("MapArea %s", map.c_str());

    ImGui::TextDisabled("Level %s-%s · Cen %s-%s · CenRate %s",
                        ShineRecordValue(record,cMinLevel).c_str(),
                        ShineRecordValue(record,cMaxLevel).c_str(),
                        ShineRecordValue(record,cMinCen).c_str(),
                        ShineRecordValue(record,cMaxCen).c_str(),
                        ShineRecordValue(record,cCenRate).c_str());
    ImGui::SameLine();
    if (cChecksum >= 0)
        ImGui::TextDisabled("· CheckSum %s", ShineRecordValue(record,cChecksum).c_str());

    const DropRecordValidation selectedValidation = validateDropRecord(record);
    if (!state.dropGroups.HasGroups())
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f), "%s",
                           (std::string(L("Drop-Gruppen nicht prüfbar: ","Drop groups cannot be checked: ")) +
                            (state.dropGroupsStatus.empty() ? std::string("ItemDropGroup.txt") : state.dropGroupsStatus)).c_str());
    if (selectedValidation.emptyDropGroups > 0)
        ImGui::TextDisabled(L("%d Slot(s) nennen eine Gruppe ohne auflösbares Item (in NA2016 v. a. inaktive Event-Gruppen).",
                              "%d slot(s) name a group without a resolvable item (in NA2016 mostly inactive event groups)."),
                            selectedValidation.emptyDropGroups);
    if (selectedValidation.Any()) {
        ImGui::SeparatorText(L("Validierung","Validation"));
        const ImVec4 issueColor(1.0f,0.48f,0.34f,1.0f);
        if (selectedValidation.missingMob)
            ImGui::TextColored(issueColor,"%s",L("MobId fehlt in MobViewInfo.shn","MobId is missing from MobViewInfo.shn"));
        if (selectedValidation.missingDropItems > 0)
            ImGui::TextColored(issueColor,L("%d Drop-Gruppe(n) fehlen in ItemDropGroup.txt",
                                             "%d drop group(s) are missing from ItemDropGroup.txt"),
                               selectedValidation.missingDropItems);
        if (selectedValidation.missingExclusionItems > 0)
            ImGui::TextColored(issueColor,L("%d Ausschluss-Item-Referenz(en) fehlen in ItemInfo.shn",
                                             "%d exclusion item reference(s) are missing from ItemInfo.shn"),
                               selectedValidation.missingExclusionItems);
        if (selectedValidation.levelRange)
            ImGui::TextColored(issueColor,"%s",L("MinLevel ist größer als MaxLevel","MinLevel is greater than MaxLevel"));
        if (selectedValidation.cenRange)
            ImGui::TextColored(issueColor,"%s",L("MinCen ist größer als MaxCen","MinCen is greater than MaxCen"));
        if (selectedValidation.upgradeRanges > 0)
            ImGui::TextColored(issueColor,L("%d Upgrade-Spanne(n) haben Min > Max",
                                             "%d upgrade range(s) have min > max"),
                               selectedValidation.upgradeRanges);
        if (selectedValidation.checksum)
            ImGui::TextColored(issueColor,"%s",L("CheckSum entspricht nicht MaxLevel + 1",
                                                  "CheckSum does not equal MaxLevel + 1"));
    }

    if (UI::CollapsingHeader(L("Basiswerte##dropTableBase","Base values##dropTableBase"))) {
        const struct BaseField { const char* label; int col; float width; } fields[] = {
            {"MapArea", cMap, 150.0f}, {"MobId", cMob, 180.0f},
            {"MinLevel", cMinLevel, 80.0f}, {"MaxLevel", cMaxLevel, 80.0f},
            {"MinCen", cMinCen, 90.0f}, {"MaxCen", cMaxCen, 90.0f},
            {"CenRate", cCenRate, 90.0f}
        };
        for (const auto& field : fields) {
            if (field.col < 0 || static_cast<std::size_t>(field.col) >= record.values.size()) continue;
            ImGui::PushID(field.label);
            ImGui::TextDisabled("%s", field.label);
            ImGui::SameLine(105.0f);
            if (EditDropTableValue("##value", record.values[static_cast<std::size_t>(field.col)], field.width)) {
                state.dropTableDirty = true;
                if (field.col == cMaxLevel && cChecksum >= 0 &&
                    static_cast<std::size_t>(cChecksum) < record.values.size()) {
                    const std::string& maxText=record.values[static_cast<std::size_t>(cMaxLevel)];
                    if (IsDropUnsignedInteger(maxText))
                        record.values[static_cast<std::size_t>(cChecksum)] =
                            std::to_string(std::atoi(maxText.c_str()) + 1);
                }
            }
            ImGui::PopID();
        }
        if (cChecksum >= 0 && static_cast<std::size_t>(cChecksum) < record.values.size()) {
            const std::string maxText=ShineRecordValue(record,cMaxLevel);
            const std::string checkText=ShineRecordValue(record,cChecksum);
            const bool checksumMatches =
                IsDropUnsignedInteger(maxText) && IsDropUnsignedInteger(checkText) &&
                std::atoi(checkText.c_str()) == std::atoi(maxText.c_str()) + 1;
            if (checksumMatches)
                ImGui::TextColored(ImVec4(0.42f,0.86f,0.62f,1.0f),
                                   "CheckSum %s · NA2016: MaxLevel + 1 OK",checkText.c_str());
            else
                ImGui::TextColored(ImVec4(1.0f,0.48f,0.34f,1.0f),
                                   "CheckSum %s · erwartet MaxLevel + 1",checkText.c_str());
        }
    }

    UI::Checkbox(L("Nur belegte Drop-Slots##dropTable","Active drop slots only##dropTable"), &state.dropTableOnlyActive);
    ImGui::Separator();

    struct DropSlot {
        int number = 0;
        int itemCol = -1;
        int rateCol = -1;
        int upgradeMinCol = -1;
        int upgradeMaxCol = -1;
        int ruleCol = -1;
        int amountCol = -1;
    };
    std::vector<DropSlot> slots;
    slots.reserve(45);
    for (int slotNo = 1; slotNo <= 45; ++slotNo) {
        const std::string suffix = std::to_string(slotNo);
        char twoDigit[4];
        std::snprintf(twoDigit, sizeof(twoDigit), "%02d", slotNo);
        DropSlot slot;
        slot.number = slotNo;
        slot.itemCol = FindShineColumn(*table, "DrItem" + suffix);
        slot.rateCol = FindShineColumn(*table, "DrItem" + suffix + "R");
        slot.upgradeMinCol = FindShineColumn(*table, "UpGradeMin" + std::string(twoDigit));
        slot.upgradeMaxCol = FindShineColumn(*table, "UpGradeMax" + std::string(twoDigit));
        slot.ruleCol = FindShineColumn(*table, "Rule" + suffix);
        slot.amountCol = FindShineColumn(*table, "Num" + suffix);
        const std::string item = ShineRecordValue(record, slot.itemCol);
        const bool active = !item.empty() && item != "-";
        if (!state.dropTableOnlyActive || active) slots.push_back(slot);
    }

    if (ImGui::BeginTable("##dropSlots", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_Resizable, ImVec2(0, std::max(180.0f, ImGui::GetContentRegionAvail().y - 100.0f)))) {
        ImGui::TableSetupScrollFreeze(0,1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 36.0f);
        ImGui::TableSetupColumn(L("Drop-Gruppe","Drop group"), ImGuiTableColumnFlags_WidthStretch, 0.32f);
        ImGui::TableSetupColumn("Rate", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn(L("Anzahl","Amount"), ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("Upgrade", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn("Rule", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("ItemDropGroup", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableHeadersRow();

        for (const auto& slot : slots) {
            auto valueRef = [&](int column) -> std::string* {
                if (column < 0 || static_cast<std::size_t>(column) >= record.values.size()) return nullptr;
                return &record.values[static_cast<std::size_t>(column)];
            };
            ImGui::PushID(slot.number);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", slot.number);

            ImGui::TableSetColumnIndex(1);
            std::string* itemValue = valueRef(slot.itemCol);
            if (itemValue && EditDropTableValue("##item", *itemValue, -1.0f))
                state.dropTableDirty = true;
            const bool active = itemValue && !itemValue->empty() && *itemValue != "-";

            ImGui::TableSetColumnIndex(2);
            if (auto* v=valueRef(slot.rateCol); v && EditDropTableValue("##rate",*v,-1.0f))
                state.dropTableDirty=true;
            ImGui::TableSetColumnIndex(3);
            if (auto* v=valueRef(slot.amountCol); v && EditDropTableValue("##amount",*v,-1.0f))
                state.dropTableDirty=true;
            ImGui::TableSetColumnIndex(4);
            if (auto* minV=valueRef(slot.upgradeMinCol)) {
                ImGui::SetNextItemWidth(34.0f);
                if (EditDropTableValue("##upgradeMin",*minV,34.0f)) state.dropTableDirty=true;
            }
            ImGui::SameLine(0,2);
            ImGui::TextDisabled("-");
            ImGui::SameLine(0,2);
            if (auto* maxV=valueRef(slot.upgradeMaxCol)) {
                if (EditDropTableValue("##upgradeMax",*maxV,34.0f)) state.dropTableDirty=true;
            }
            ImGui::TableSetColumnIndex(5);
            if (auto* v=valueRef(slot.ruleCol); v && EditDropTableValue("##rule",*v,-1.0f))
                state.dropTableDirty=true;

            ImGui::TableSetColumnIndex(6);
            if (active) {
                switch (state.dropGroups.Check(*itemValue)) {
                case core::legacy::DropSlotStatus::Resolved: {
                    const auto items = state.dropGroups.ItemsForGroup(*itemValue);
                    const auto* rows = state.dropGroups.Group(*itemValue);
                    std::string qty;
                    if (rows && !rows->empty()) qty = rows->front().minQty + "-" + rows->front().maxQty;
                    ImGui::BeginGroup();
                    for (std::size_t i = 0; i < items.size() && i < 6; ++i) {
                        DrawItemIconByInx(state, items[i], ImGui::GetTextLineHeight());
                        ImGui::SameLine(0.0f, 2.0f);
                    }
                    if (items.size() == 1) ImGui::TextUnformatted(state.dropGroups.DisplayName(items.front()).c_str());
                    else ImGui::Text(L("%zu Items","%zu items"), items.size());
                    if (!qty.empty()) { ImGui::SameLine(); ImGui::TextDisabled(L("· Menge %s","· qty %s"), qty.c_str()); }
                    ImGui::EndGroup();
                    if (ImGui::IsItemHovered()) {
                        std::string tip = std::string(L("Gruppe ","Group ")) + *itemValue + "\n";
                        for (std::size_t i = 0; i < items.size() && i < 20; ++i)
                            tip += "\n" + state.dropGroups.DisplayName(items[i]) + "  (" + items[i] + ")";
                        if (items.size() > 20) tip += "\n...";
                        ImGui::SetTooltip("%s", tip.c_str());
                    }
                    break;
                }
                case core::legacy::DropSlotStatus::GroupWithoutItems:
                    ImGui::TextDisabled("%s", L("Gruppe ohne Items","group without items"));
                    break;
                case core::legacy::DropSlotStatus::MissingGroup:
                    ImGui::TextColored(ImVec4(1.0f,0.48f,0.34f,1.0f), "%s",L("fehlt in ItemDropGroup","missing from ItemDropGroup"));
                    break;
                default: break;
                }
                if (state.dropGroups.Group(*itemValue)) {
                    ImGui::SameLine();
                    if (UI::SmallButton(L("Öffnen##dropGroupOpen","Open##dropGroupOpen"))) {
                        state.dropGroupSelected = *itemValue;
                        state.dropEditorTabRequest = 1;
                    }
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        ImGui::SetTooltip("%s", L("Diese Gruppe im Drop-Gruppen-Editor öffnen.", "Open this group in the drop group editor."));
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    std::vector<std::string> exclusions;
    for (const int col : exclusionColumns) {
        const std::string value = ShineRecordValue(record, col);
        if (!value.empty() && value != "-") exclusions.push_back(value);
    }
    if (!exclusions.empty()) {
        ImGui::SeparatorText(L("Ausgeschlossene Items","Excluded items"));
        for (std::size_t i = 0; i < exclusions.size(); ++i) {
            if (i) ImGui::SameLine();
            const bool known = state.itemEntries.empty() || state.itemByInx.count(exclusions[i]) != 0;
            if (known)
                ImGui::TextDisabled("%s%s", i ? "· " : "", exclusions[i].c_str());
            else
                ImGui::TextColored(ImVec4(1.0f,0.48f,0.34f,1.0f),"%s%s",
                                   i ? "· " : "", exclusions[i].c_str());
            if (!known && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s",L("Nicht in ItemInfo.shn","Missing from ItemInfo.shn"));
        }
    }
    ImGui::EndChild();
}

} // namespace nge
