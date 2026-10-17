// QuestTexts.cpp
// Quest-Texte in QuestDialog.shn bearbeiten: Titel, Beschreibung und die SAY-Texte der Skripte.
// Die Texte liegen im gemeinsamen SHN-Dokument (Client, ressystem/); gespeichert wird über
// SaveShnDocument, das die bytegleiche Server-Kopie (9Data/Shine/QuestDialog.shn) mitschreibt.

#include "EditorShared.hpp"

#include "mapeditor/core/legacy/QuestText.hpp"

namespace nge {

namespace {
constexpr int kQuestTextIdColumn = 0;
constexpr int kQuestTextColumn = 1;

bool DialogReady(EditorState& state) {
    return state.questDialogLoaded && state.questDialogDoc >= 0 &&
           state.questDialogDoc < static_cast<int>(state.shnFiles.size());
}
void EnsureTextIndex(EditorState& state) { (void)QuestTextOf(state, 0); } // baut questTextMap/questTextRow bei Bedarf neu

void EnsureTextUsage(EditorState& state) {
    if (state.questTextUsageRevision == state.questRevision) return;
    state.questTextUsageRevision = state.questRevision;
    state.questTextUsage.clear();
    for (const auto& q : state.questDataFile.records)
        for (const std::uint32_t id : core::legacy::QuestTextIds(q)) ++state.questTextUsage[id];
}
std::uint32_t NextFreeTextId(EditorState& state) {
    EnsureTextIndex(state);
    std::set<std::uint32_t> used;
    for (const auto& [id, row] : state.questTextRow) used.insert(static_cast<std::uint32_t>(id));
    const std::uint32_t after = used.empty() ? 1u : *used.rbegin() + 1u;
    return core::legacy::FindFreeTextBlock(used, 1, after).value_or(0u);
}
} // namespace

int QuestTextRow(EditorState& state, std::uint32_t textId) {
    if (!DialogReady(state)) return -1;
    EnsureTextIndex(state);
    const auto it = state.questTextRow.find(static_cast<int>(textId));
    return it == state.questTextRow.end() ? -1 : it->second;
}

std::string QuestTextRaw(EditorState& state, std::uint32_t textId) {
    if (!DialogReady(state)) return {};
    EnsureTextIndex(state);
    const auto it = state.questTextMap.find(static_cast<int>(textId));
    return it == state.questTextMap.end() ? std::string() : it->second;
}

int QuestTextUseCount(EditorState& state, std::uint32_t textId) {
    EnsureTextUsage(state);
    const auto it = state.questTextUsage.find(textId);
    return it == state.questTextUsage.end() ? 0 : it->second;
}

bool SetQuestText(EditorState& state, std::uint32_t textId, const std::string& text) {
    if (!DialogReady(state) || textId == 0) return false;
    const int row = QuestTextRow(state, textId);
    if (row >= 0) return ApplyShnCellText(state, state.questDialogDoc, row, kQuestTextColumn, text);
    // Text-ID ohne Zeile (z. B. von Hand eingetragen): Zeile anlegen.
    auto& file = ShnDocFile(state, state.questDialogDoc);
    if (file.columns.size() < 2) return false;
    auto id = core::legacy::ParseShnValue(file.columns[kQuestTextIdColumn], std::to_string(textId));
    auto value = core::legacy::ParseShnValue(file.columns[kQuestTextColumn], text);
    if (!id || !value) {
        state.statusMessage = std::string("QuestDialog: ") + (!id ? id.error() : value.error());
        return false;
    }
    core::legacy::ShnRow newRow;
    newRow.values.resize(file.columns.size());
    newRow.values[kQuestTextIdColumn] = std::move(*id);
    newRow.values[kQuestTextColumn] = std::move(*value);
    return InsertShnRowWithHistory(state, state.questDialogDoc, static_cast<int>(file.rows.size()), std::move(newRow)) >= 0;
}

std::uint32_t CreateQuestText(EditorState& state, const std::string& text) {
    const std::uint32_t id = NextFreeTextId(state);
    if (id == 0 || !SetQuestText(state, id, text)) return 0;
    return id;
}

void CopyQuestTexts(EditorState& state, const std::map<std::uint32_t, std::uint32_t>& remap) {
    if (!DialogReady(state)) return;
    // Erst alle alten Texte lesen: das Einfügen baut den Index neu auf.
    std::vector<std::pair<std::uint32_t, std::string>> copies;
    for (const auto& [oldId, newId] : remap) copies.emplace_back(newId, QuestTextRaw(state, oldId));
    std::sort(copies.begin(), copies.end());
    for (const auto& [newId, text] : copies) SetQuestText(state, newId, text);
}

bool QuestDialogDirty(const EditorState& state) {
    return state.questDialogDoc >= 0 && state.questDialogDoc < static_cast<int>(state.shnFiles.size()) &&
           state.shnFiles[static_cast<std::size_t>(state.questDialogDoc)].dirty;
}

bool SaveQuestDialogTexts(EditorState& state) {
    if (!QuestDialogDirty(state)) return true;
    // Server-Kopie laden, damit SaveShnDocument sie gekoppelt mitschreiben kann (bytegleich in NA2016).
    if (!state.shnServerRoot.empty())
        (void)SharedShnDoc(state, "QuestDialog.shn", EditorState::ShnSource::Server, state.shnServerRoot);
    const bool ok = SaveShnDocument(state, state.questDialogDoc);
    state.statusMessage = std::string("QuestDialog.shn: ") + state.shnStatus;
    return ok;
}

bool DrawQuestTextField(EditorState& state, const char* key, std::uint32_t textId, bool multiline) {
    if (!DialogReady(state)) {
        ImGui::TextDisabled("%s", L("QuestDialog.shn nicht geladen", "QuestDialog.shn not loaded"));
        return false;
    }
    if (textId == 0) {
        ImGui::TextDisabled("%s", L("(keine Text-ID)", "(no text ID)"));
        return false;
    }
    const std::string text = QuestTextRaw(state, textId);
    const std::string id = std::string("##") + key + std::to_string(textId);
    std::string edited;
    const float lines = std::clamp(static_cast<float>(std::count(text.begin(), text.end(), '\n') + 2), 3.0f, 8.0f);
    const bool changed = DeferredInputText(id.c_str(), text, edited, -1.0f,
                                           multiline ? ImGui::GetTextLineHeight() * lines + 8.0f : 0.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && !ImGui::IsItemActive())
        ImGui::SetTooltip(L("Text-ID %u in QuestDialog.shn. Übernommen wird beim Verlassen des Felds (Strg+Z macht es rückgängig).",
                            "Text ID %u in QuestDialog.shn. Applied when leaving the field (Ctrl+Z undoes it)."), textId);
    return changed && SetQuestText(state, textId, edited);
}

bool DrawQuestTextList(EditorState& state, core::legacy::QuestRecord& quest) {
    if (!DialogReady(state)) {
        ImGui::TextDisabled("%s", L("QuestDialog.shn nicht geladen - Texte sind nicht bearbeitbar.",
                                    "QuestDialog.shn not loaded - texts cannot be edited."));
        return false;
    }
    struct Entry {
        std::string role;
        std::uint32_t id;
        int field; // 0 Titel, 1 Beschreibung, 2.. Skript (für "eigene ID")
        core::legacy::QuestScript* script;
    };
    std::vector<Entry> entries;
    std::set<std::uint32_t> seen;
    auto add = [&](std::string role, std::uint32_t id, int field, core::legacy::QuestScript* script) {
        if (id == 0 || !seen.insert(id).second) return;
        entries.push_back({std::move(role), id, field, script});
    };
    add(L("Titel", "Title"), quest.title, 0, nullptr);
    add(L("Beschreibung", "Description"), quest.description, 1, nullptr);
    const std::pair<const char*, core::legacy::QuestScript*> scripts[] = {
        {"Start", &quest.start}, {"Action", &quest.action}, {"Finish", &quest.finish}};
    for (const auto& [name, script] : scripts) {
        for (const auto& ref : core::legacy::FindSayReferences(script->text)) {
            // Sprecher (NPC/ME) steht hinter der ID.
            std::size_t p = ref.offset + ref.length;
            while (p < script->text.size() && (script->text[p] == ' ' || script->text[p] == '\t' || script->text[p] == ',')) ++p;
            std::size_t e = p;
            while (e < script->text.size() && script->text[e] != ' ' && script->text[e] != '\t' &&
                   script->text[e] != '\r' && script->text[e] != '\n') ++e;
            add(std::string("SAY ") + name + (e > p ? " · " + script->text.substr(p, e - p) : std::string()), ref.id, 2, script);
        }
    }
    if (entries.empty()) {
        ImGui::TextDisabled("%s", L("Diese Quest hat keine Texte.", "This quest has no texts."));
        return false;
    }
    bool questChanged = false;
    if (ImGui::BeginTable("##questTexts", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(L("Stelle", "Where"), ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn(L("Text", "Text"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();
        for (const auto& entry : entries) {
            ImGui::PushID(static_cast<int>(entry.id));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.role.c_str());
            ImGui::TextDisabled("ID %u", entry.id);
            const bool missing = QuestTextRow(state, entry.id) < 0;
            if (missing) ImGui::TextColored(ImVec4(1.0f, 0.48f, 0.34f, 1.0f), "%s", L("fehlt in QuestDialog", "missing in QuestDialog"));
            ImGui::TableSetColumnIndex(1);
            DrawQuestTextField(state, "questText", entry.id, entry.field != 0);
            ImGui::TableSetColumnIndex(2);
            const int uses = QuestTextUseCount(state, entry.id);
            if (uses > 1) {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), L("%d Quests", "%d quests"), uses);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", L("Dieser Text wird von mehreren Quests genutzt - eine Änderung gilt für alle.",
                                              "This text is used by several quests - a change applies to all of them."));
                if (UI::SmallButton(L("Eigene ID", "Own ID"))) {
                    TrackQuestEdits(state); // offene Quest-Bearbeitung zuerst als eigenen Schritt sichern
                    DataHistoryBegin(state, L("Eigene Text-ID", "Own text ID"));
                    const std::uint32_t newId = CreateQuestText(state, QuestTextRaw(state, entry.id));
                    if (newId != 0) {
                        if (entry.field == 0) quest.title = newId;
                        else if (entry.field == 1) quest.description = newId;
                        else entry.script->text = core::legacy::RemapSayIds(entry.script->text, {{entry.id, newId}});
                        state.questDirty = true;
                        ++state.questRevision;
                        TrackQuestEdits(state);
                        questChanged = true;
                        state.statusMessage = std::string(L("Neue Text-ID ", "New text ID ")) + std::to_string(newId) +
                                              L(" mit Kopie des Texts angelegt.", " created with a copy of the text.");
                    }
                    DataHistoryCommit(state);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", L("Kopiert den Text auf eine neue, freie Text-ID und stellt nur diese Quest darauf um.",
                                              "Copies the text to a new free text ID and switches only this quest to it."));
            } else if (missing) {
                ImGui::TextDisabled("%s", L("wird beim Bearbeiten angelegt", "created when edited"));
            }
            ImGui::PopID();
            if (questChanged) break; // Einträge zeigen auf die alte ID - im nächsten Frame neu aufbauen
        }
        ImGui::EndTable();
    }
    return questChanged;
}

} // namespace nge
