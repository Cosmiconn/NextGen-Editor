// DataEditing.cpp
// Gemeinsamer Undo/Redo-Verlauf der Daten-Editoren und Zeilenoperationen auf SHN-Dokumenten:
// Zellen ändern, Zeilen einfügen und löschen (mit Herkunftszuordnung für die Speicherprüfung),
// Löschen über alle Tabellen einer ID-Familie und die Verweissuche vor dem Löschen.

#include "EditorShared.hpp"

namespace nge {

namespace {
EditorState::ShnDocument* DocAt(EditorState& state, int document) {
    if (document < 0 || document >= static_cast<int>(state.shnFiles.size())) return nullptr;
    return &state.shnFiles[static_cast<std::size_t>(document)];
}
void Touch(EditorState& state, EditorState::ShnDocument& doc) {
    doc.dirty = true;
    ++state.shnEditCounter;
    state.shnVisibleKey.clear();
}
void SetCell(EditorState& state, int document, int row, int column, const core::legacy::ShnValue& value) {
    auto* doc = DocAt(state, document);
    if (!doc || row < 0 || row >= static_cast<int>(doc->file.rows.size())) return;
    auto& values = doc->file.rows[static_cast<std::size_t>(row)].values;
    if (column < 0 || column >= static_cast<int>(values.size())) return;
    values[static_cast<std::size_t>(column)] = value;
    EnsureCellStatusSize(*doc);
    doc->cellDirty[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = 1;
    Touch(state, *doc);
    state.shnSelectedFile = document;
    state.shnSelectedRow = row;
    state.shnSelectedColumn = column;
}
void EnsureRowOrigin(EditorState::ShnDocument& doc) {
    if (!doc.rowOrigin.empty()) return;
    doc.rowOrigin.resize(doc.file.rows.size());
    for (std::size_t i = 0; i < doc.rowOrigin.size(); ++i) doc.rowOrigin[i] = static_cast<int>(i);
}
struct RemovedRow {
    core::legacy::ShnRow row;
    std::vector<std::uint8_t> status;
    std::vector<std::uint8_t> dirty;
    int origin = -1;
};
RemovedRow TakeRow(EditorState::ShnDocument& doc, std::size_t index) {
    EnsureCellStatusSize(doc);
    EnsureRowOrigin(doc);
    RemovedRow out;
    out.row = std::move(doc.file.rows[index]);
    out.status = std::move(doc.cellStatus[index]);
    out.dirty = std::move(doc.cellDirty[index]);
    out.origin = doc.rowOrigin[index];
    doc.file.rows.erase(doc.file.rows.begin() + static_cast<std::ptrdiff_t>(index));
    doc.cellStatus.erase(doc.cellStatus.begin() + static_cast<std::ptrdiff_t>(index));
    doc.cellDirty.erase(doc.cellDirty.begin() + static_cast<std::ptrdiff_t>(index));
    doc.rowOrigin.erase(doc.rowOrigin.begin() + static_cast<std::ptrdiff_t>(index));
    return out;
}
void PutRow(EditorState::ShnDocument& doc, std::size_t index, const RemovedRow& r) {
    EnsureCellStatusSize(doc);
    EnsureRowOrigin(doc);
    index = std::min(index, doc.file.rows.size());
    doc.file.rows.insert(doc.file.rows.begin() + static_cast<std::ptrdiff_t>(index), r.row);
    doc.cellStatus.insert(doc.cellStatus.begin() + static_cast<std::ptrdiff_t>(index), r.status);
    doc.cellDirty.insert(doc.cellDirty.begin() + static_cast<std::ptrdiff_t>(index), r.dirty);
    doc.rowOrigin.insert(doc.rowOrigin.begin() + static_cast<std::ptrdiff_t>(index), r.origin);
    EnsureCellStatusSize(doc);
}
long long RowId(const core::legacy::ShnFile& f, int row, int* idColumnOut = nullptr) {
    int idColumn = FindShnColumnByName(f, "ID");
    if (idColumnOut) *idColumnOut = idColumn;
    if (idColumn < 0 || row < 0 || row >= static_cast<int>(f.rows.size())) return -1;
    long long id = -1;
    if (static_cast<std::size_t>(idColumn) < f.rows[static_cast<std::size_t>(row)].values.size())
        ShnValueAsInt(f.rows[static_cast<std::size_t>(row)].values[static_cast<std::size_t>(idColumn)], id);
    return id;
}
std::string RowLabel(const core::legacy::ShnFile& f, int row) {
    const int idColumn = FindShnColumnByName(f, "ID");
    const int inxColumn = FindShnColumnByName(f, "InxName");
    std::string label = f.FileName();
    if (row >= 0 && row < static_cast<int>(f.rows.size())) {
        const auto& v = f.rows[static_cast<std::size_t>(row)].values;
        if (idColumn >= 0 && static_cast<std::size_t>(idColumn) < v.size())
            label += " #" + core::legacy::ShnValueToString(v[static_cast<std::size_t>(idColumn)]);
        if (inxColumn >= 0 && static_cast<std::size_t>(inxColumn) < v.size())
            label += " " + TrimAscii(core::legacy::ShnValueToString(v[static_cast<std::size_t>(inxColumn)]));
        if (idColumn < 0 && inxColumn < 0) label += L(" Zeile ", " row ") + std::to_string(row);
    }
    return label;
}
} // namespace

void DataHistoryBegin(EditorState& state, const std::string& label) { state.dataHistory.Begin(label); }
void DataHistoryCommit(EditorState& state) { state.dataHistory.Commit(); }

bool DataUndo(EditorState& state) {
    const std::string label = state.dataHistory.UndoLabel();
    if (!state.dataHistory.Undo()) return false;
    const std::string msg = L("Rückgängig: ", "Undone: ") + (label.empty() ? L("Änderung", "change") : label);
    state.shnStatus = msg;
    state.statusMessage = msg;
    return true;
}
bool DataRedo(EditorState& state) {
    const std::string label = state.dataHistory.RedoLabel();
    if (!state.dataHistory.Redo()) return false;
    const std::string msg = L("Wiederholt: ", "Redone: ") + (label.empty() ? L("Änderung", "change") : label);
    state.shnStatus = msg;
    state.statusMessage = msg;
    return true;
}

void RecordShnCellChange(EditorState& state, int document, int row, int column,
                         core::legacy::ShnValue before, core::legacy::ShnValue after) {
    std::string label;
    if (const auto* doc = DocAt(state, document)) {
        label = L("Zelle geändert: ", "Cell changed: ") + doc->file.FileName();
        if (column >= 0 && column < static_cast<int>(doc->file.columns.size()))
            label += " · " + doc->file.columns[static_cast<std::size_t>(column)].name;
    }
    EditorState* s = &state;
    state.dataHistory.Record({
        [s, document, row, column, after]() { SetCell(*s, document, row, column, after); },
        [s, document, row, column, before]() { SetCell(*s, document, row, column, before); },
    }, label);
}

int InsertShnRowWithHistory(EditorState& state, int document, int index, core::legacy::ShnRow row) {
    auto* doc = DocAt(state, document);
    if (!doc) return -1;
    EnsureCellStatusSize(*doc);
    RemovedRow r;
    r.row = std::move(row);
    r.row.values.resize(doc->file.columns.size());
    r.status.assign(doc->file.columns.size(), kShnCellNormal);
    r.dirty.assign(doc->file.columns.size(), 1); // neue Zeile ganz als Änderung markiert
    r.origin = -1;
    if (index < 0 || index > static_cast<int>(doc->file.rows.size())) index = static_cast<int>(doc->file.rows.size());
    const bool hadOrigin = !doc->rowOrigin.empty();
    if (hadOrigin) PutRow(*doc, static_cast<std::size_t>(index), r);
    else {
        // Ohne Herkunftszuordnung nur anhängen (bleibt im einfachen Append-Modus).
        if (index != static_cast<int>(doc->file.rows.size())) PutRow(*doc, static_cast<std::size_t>(index), r);
        else {
            doc->file.rows.push_back(r.row);
            doc->cellStatus.push_back(r.status);
            doc->cellDirty.push_back(r.dirty);
        }
    }
    Touch(state, *doc);
    RecordRowsInserted(state, L("Zeile angelegt: ", "Row added: ") + RowLabel(doc->file, index), {{document, index}});
    return index;
}

void RecordRowsInserted(EditorState& state, const std::string& label, const std::vector<std::pair<int, int>>& rows) {
    if (rows.empty()) return;
    // Die Zeile wird erst beim Rückgängigmachen gesichert: so enthält ein späteres Wiederholen auch
    // Werte, die nach dem Einfügen ohne eigenen Verlaufseintrag gesetzt wurden (z. B. vom Assistenten).
    auto taken = std::make_shared<std::vector<RemovedRow>>(rows.size());
    EditorState* s = &state;
    state.dataHistory.Record({
        [s, rows, taken]() {
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (auto* d = DocAt(*s, rows[i].first)) {
                    PutRow(*d, static_cast<std::size_t>(rows[i].second), (*taken)[i]);
                    Touch(*s, *d);
                }
        },
        [s, rows, taken]() {
            for (std::size_t i = rows.size(); i-- > 0;)
                if (auto* d = DocAt(*s, rows[i].first); d && rows[i].second < static_cast<int>(d->file.rows.size())) {
                    (*taken)[i] = TakeRow(*d, static_cast<std::size_t>(rows[i].second));
                    Touch(*s, *d);
                }
        },
    }, label);
}

bool DeleteShnRowWithHistory(EditorState& state, int document, int row) {
    auto* doc = DocAt(state, document);
    if (!doc || row < 0 || row >= static_cast<int>(doc->file.rows.size())) return false;
    const std::string label = L("Zeile gelöscht: ", "Row deleted: ") + RowLabel(doc->file, row);
    auto removed = std::make_shared<RemovedRow>(TakeRow(*doc, static_cast<std::size_t>(row)));
    Touch(state, *doc);
    if (state.shnSelectedFile == document && state.shnSelectedRow >= static_cast<int>(doc->file.rows.size()))
        state.shnSelectedRow = static_cast<int>(doc->file.rows.size()) - 1;
    EditorState* s = &state;
    state.dataHistory.Record({
        [s, document, row]() {
            if (auto* d = DocAt(*s, document); d && row < static_cast<int>(d->file.rows.size())) {
                (void)TakeRow(*d, static_cast<std::size_t>(row));
                Touch(*s, *d);
            }
        },
        [s, document, row, removed]() {
            if (auto* d = DocAt(*s, document)) { PutRow(*d, static_cast<std::size_t>(row), *removed); Touch(*s, *d); }
        },
    }, label);
    return true;
}

int DeleteShnRecordEverywhere(EditorState& state, int document, int row) {
    auto* doc = DocAt(state, document);
    if (!doc || row < 0 || row >= static_cast<int>(doc->file.rows.size())) return 0;
    const std::string fileName = doc->file.FileName();
    const long long id = RowId(doc->file, row);
    DataHistoryBegin(state, L("Gelöscht: ", "Deleted: ") + RowLabel(doc->file, row));
    int count = DeleteShnRowWithHistory(state, document, row) ? 1 : 0;
    // Dieselbe ID in allen geladenen Familien-Tabellen beider Seiten (und der Gegenseite derselben Datei).
    if (id >= 0) {
        std::vector<std::string> files = {fileName};
        if (const auto* family = KnownShnFamilyFor(fileName))
            for (const char* member : family->files) files.emplace_back(member);
        for (int d = 0; d < static_cast<int>(state.shnFiles.size()); ++d) {
            if (d == document) continue;
            const auto& other = state.shnFiles[static_cast<std::size_t>(d)];
            const std::string otherName = LowerAscii(other.file.FileName());
            if (std::none_of(files.begin(), files.end(), [&](const std::string& f) { return LowerAscii(f) == otherName; }))
                continue;
            int idColumn = -1;
            for (int r = static_cast<int>(other.file.rows.size()) - 1; r >= 0; --r) {
                if (RowId(other.file, r, &idColumn) != id) continue;
                if (DeleteShnRowWithHistory(state, d, r)) ++count;
                break;
            }
        }
    }
    DataHistoryCommit(state);
    state.shnStatus = std::to_string(count) + L(" Zeile(n) gelöscht (noch nicht gespeichert, Strg+Z macht es rückgängig).",
                                                " row(s) deleted (not saved yet, Ctrl+Z undoes it).");
    return count;
}

std::vector<std::string> FindShnRowReferences(EditorState& state, int document, int row, std::size_t limit) {
    std::vector<std::string> out;
    auto* doc = DocAt(state, document);
    if (!doc || row < 0 || row >= static_cast<int>(doc->file.rows.size())) return out;
    const auto& f = doc->file;
    const long long id = RowId(f, row);
    const int inxColumn = FindShnColumnByName(f, "InxName");
    std::string inx;
    if (inxColumn >= 0 && static_cast<std::size_t>(inxColumn) < f.rows[static_cast<std::size_t>(row)].values.size())
        inx = TrimAscii(core::legacy::ShnValueToString(f.rows[static_cast<std::size_t>(row)].values[static_cast<std::size_t>(inxColumn)]));
    if (inx.empty() && id < 0) return out;
    // Spaltennamen, die eine numerische ID dieser Familie meinen (z. B. ItemID, MobID, SkillID).
    std::string idKeyword;
    if (const auto* family = KnownShnFamilyFor(f.FileName())) idKeyword = LowerAscii(family->label);
    if (idKeyword == "activeskill") idKeyword = "skill";
    std::vector<std::string> ownFiles = {LowerAscii(f.FileName())};
    if (const auto* family = KnownShnFamilyFor(f.FileName()))
        for (const char* m : family->files) ownFiles.push_back(LowerAscii(m));

    for (int d = 0; d < static_cast<int>(state.shnFiles.size()) && out.size() < limit; ++d) {
        const auto& other = state.shnFiles[static_cast<std::size_t>(d)].file;
        if (std::find(ownFiles.begin(), ownFiles.end(), LowerAscii(other.FileName())) != ownFiles.end()) continue;
        for (std::size_t c = 0; c < other.columns.size() && out.size() < limit; ++c) {
            const std::string colName = LowerAscii(other.columns[c].name);
            const bool stringColumn = other.columns[c].kind == core::legacy::ShnValueKind::String;
            const bool idColumn = !stringColumn && id > 0 && !idKeyword.empty() && colName.find(idKeyword) != std::string::npos;
            if (!(stringColumn && !inx.empty()) && !idColumn) continue;
            for (std::size_t r = 0; r < other.rows.size() && out.size() < limit; ++r) {
                if (c >= other.rows[r].values.size()) continue;
                const auto& v = other.rows[r].values[c];
                bool hit = false;
                if (stringColumn) hit = TrimAscii(core::legacy::ShnValueToString(v)) == inx;
                else { long long n = 0; hit = ShnValueAsInt(v, n) && n == id; }
                if (hit)
                    out.push_back(other.FileName() + " · " + other.columns[c].name + L(" in Zeile ", " in row ") +
                                  std::to_string(r) + (state.shnFiles[static_cast<std::size_t>(d)].source == EditorState::ShnSource::Client
                                                           ? " (Client)" : " (Server)"));
            }
        }
    }
    return out;
}

void RequestShnRowDelete(EditorState& state, int document, int row) {
    if (!DocAt(state, document)) return;
    state.shnDeleteDoc = document;
    state.shnDeleteRow = row;
    state.shnDeleteRefs = FindShnRowReferences(state, document, row);
    state.shnDeleteOpen = true;
}

void DrawShnDeleteConfirm(EditorState& state) {
    if (!state.shnDeleteOpen) return;
    const char* title = L("Zeile löschen###shnDeleteConfirm", "Delete row###shnDeleteConfirm");
    ImGui::OpenPopup(title);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(title, &state.shnDeleteOpen)) {
        auto* doc = DocAt(state, state.shnDeleteDoc);
        if (!doc || state.shnDeleteRow < 0 || state.shnDeleteRow >= static_cast<int>(doc->file.rows.size())) {
            state.shnDeleteOpen = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::TextWrapped("%s", RowLabel(doc->file, state.shnDeleteRow).c_str());
        if (const auto* family = KnownShnFamilyFor(doc->file.FileName()))
            ImGui::TextWrapped(L("Bekannte ID-Familie %s: dieselbe ID wird in allen geladenen Familien-Tabellen "
                                 "beider Seiten mitgelöscht.",
                                 "Known ID family %s: the same ID is also deleted in every loaded family table "
                                 "on both sides."), family->label);
        if (state.shnDeleteRefs.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%s",
                               L("Keine Verweise in den geladenen Tabellen gefunden.", "No references found in the loaded tables."));
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s",
                               L("Verweise in geladenen Tabellen (bleiben bestehen und zeigen danach ins Leere):",
                                 "References in loaded tables (they stay and will point to nothing):"));
            for (const auto& ref : state.shnDeleteRefs) ImGui::BulletText("%s", ref.c_str());
        }
        ImGui::TextDisabled("%s", L("Nicht geladene Tabellen sind nicht geprüft. Strg+Z macht das Löschen rückgängig.",
                                    "Tables that are not loaded are not checked. Ctrl+Z undoes the deletion."));
        ImGui::Separator();
        if (UI::Button(L("Löschen", "Delete"))) {
            DeleteShnRecordEverywhere(state, state.shnDeleteDoc, state.shnDeleteRow);
            state.shnDeleteOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (UI::Button(L("Abbrechen", "Cancel"))) {
            state.shnDeleteOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawDataHistoryButtons(EditorState& state, const char* id) {
    ImGui::PushID(id);
    ImGui::BeginDisabled(!state.dataHistory.CanUndo());
    if (UI::Button(L("Rückgängig", "Undo"))) DataUndo(state);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && state.dataHistory.CanUndo())
        ImGui::SetTooltip("%s (Strg+Z)", state.dataHistory.UndoLabel().c_str());
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.dataHistory.CanRedo());
    if (UI::Button(L("Wiederholen", "Redo"))) DataRedo(state);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && state.dataHistory.CanRedo())
        ImGui::SetTooltip("%s (Strg+Y)", state.dataHistory.RedoLabel().c_str());
    ImGui::EndDisabled();
    ImGui::PopID();
}

// ---------------------------------------------------------------- Quests
void ResyncQuestTracking(EditorState& state) {
    auto& quests = state.questDataFile.records;
    state.questTrackRevision = state.questRevision;
    state.questTrackIdx = state.selectedQuestIdx;
    if (state.questTrackIdx >= 0 && static_cast<std::size_t>(state.questTrackIdx) < quests.size())
        state.questTrackSnapshot = std::make_shared<core::legacy::QuestRecord>(quests[static_cast<std::size_t>(state.questTrackIdx)]);
    else
        state.questTrackSnapshot.reset();
}
namespace {
void QuestTouched(EditorState& state) {
    state.questDirty = true;
    ++state.questRevision;
    state.questListKey.clear();
    ResyncQuestTracking(state);
}
} // namespace
void TrackQuestEdits(EditorState& state) {
    auto& quests = state.questDataFile.records;
    if (state.questTrackRevision != state.questRevision) {
        if (ImGui::IsAnyItemActive()) return; // Tippen zu einem Eintrag zusammenfassen
        const int idx = state.questTrackIdx;
        if (idx >= 0 && static_cast<std::size_t>(idx) < quests.size() && state.questTrackSnapshot) {
            auto before = state.questTrackSnapshot;
            auto after = std::make_shared<core::legacy::QuestRecord>(quests[static_cast<std::size_t>(idx)]);
            EditorState* s = &state;
            const auto apply = [s, idx](const std::shared_ptr<core::legacy::QuestRecord>& rec) {
                auto& q = s->questDataFile.records;
                if (idx >= 0 && static_cast<std::size_t>(idx) < q.size()) q[static_cast<std::size_t>(idx)] = *rec;
                s->selectedQuestIdx = idx;
                QuestTouched(*s);
            };
            state.dataHistory.Record({[apply, after]() { apply(after); }, [apply, before]() { apply(before); }},
                                     L("Quest #", "Quest #") + std::to_string(after->id) + L(" geändert", " changed"));
        }
        ResyncQuestTracking(state);
        return;
    }
    if (state.selectedQuestIdx != state.questTrackIdx) ResyncQuestTracking(state);
}
void RecordQuestInserted(EditorState& state, int index) {
    auto& quests = state.questDataFile.records;
    if (index < 0 || static_cast<std::size_t>(index) >= quests.size()) return;
    auto rec = std::make_shared<core::legacy::QuestRecord>(quests[static_cast<std::size_t>(index)]);
    EditorState* s = &state;
    state.dataHistory.Record({
        [s, index, rec]() {
            auto& q = s->questDataFile.records;
            q.insert(q.begin() + std::min<std::ptrdiff_t>(index, static_cast<std::ptrdiff_t>(q.size())), *rec);
            s->selectedQuestIdx = index;
            QuestTouched(*s);
        },
        [s, index]() {
            auto& q = s->questDataFile.records;
            if (static_cast<std::size_t>(index) < q.size()) q.erase(q.begin() + index);
            s->selectedQuestIdx = q.empty() ? -1 : std::min(index, static_cast<int>(q.size()) - 1);
            QuestTouched(*s);
        },
    }, L("Quest #", "Quest #") + std::to_string(rec->id) + L(" angelegt", " added"));
    ResyncQuestTracking(state);
}
bool DeleteQuestWithHistory(EditorState& state, int index) {
    auto& quests = state.questDataFile.records;
    if (index < 0 || static_cast<std::size_t>(index) >= quests.size()) return false;
    TrackQuestEdits(state); // offene Bearbeitung zuerst als eigenen Eintrag sichern
    auto rec = std::make_shared<core::legacy::QuestRecord>(quests[static_cast<std::size_t>(index)]);
    quests.erase(quests.begin() + index);
    state.selectedQuestIdx = quests.empty() ? -1 : std::min(index, static_cast<int>(quests.size()) - 1);
    QuestTouched(state);
    EditorState* s = &state;
    state.dataHistory.Record({
        [s, index]() {
            auto& q = s->questDataFile.records;
            if (static_cast<std::size_t>(index) < q.size()) q.erase(q.begin() + index);
            s->selectedQuestIdx = q.empty() ? -1 : std::min(index, static_cast<int>(q.size()) - 1);
            QuestTouched(*s);
        },
        [s, index, rec]() {
            auto& q = s->questDataFile.records;
            q.insert(q.begin() + std::min<std::ptrdiff_t>(index, static_cast<std::ptrdiff_t>(q.size())), *rec);
            s->selectedQuestIdx = index;
            QuestTouched(*s);
        },
    }, L("Quest #", "Quest #") + std::to_string(rec->id) + L(" gelöscht", " deleted"));
    return true;
}

// ---------------------------------------------------------------- Shine-TXT-Einträge
void TrackShineRecordEdits(EditorState& state, ShineRecordTracker& tracker, const ShineTableAccess& table,
                           int selected, bool* dirty, const std::string& what) {
    auto* t = table();
    if (!t) { tracker = {}; return; }
    const bool trackedValid = tracker.index >= 0 && static_cast<std::size_t>(tracker.index) < t->records.size();
    if (trackedValid && tracker.snapshot && t->records[static_cast<std::size_t>(tracker.index)].values != tracker.snapshot->values) {
        if (ImGui::IsAnyItemActive()) return; // Tippen zu einem Eintrag zusammenfassen
        auto before = tracker.snapshot;
        auto after = std::make_shared<core::legacy::ShineRecord>(t->records[static_cast<std::size_t>(tracker.index)]);
        const int idx = tracker.index;
        ShineRecordTracker* tr = &tracker;
        const auto apply = [table, idx, dirty, tr](const std::shared_ptr<core::legacy::ShineRecord>& rec) {
            if (auto* tt = table(); tt && static_cast<std::size_t>(idx) < tt->records.size()) {
                tt->records[static_cast<std::size_t>(idx)] = *rec;
                if (dirty) *dirty = true;
                if (tr->index == idx) tr->snapshot = std::make_shared<core::legacy::ShineRecord>(*rec);
            }
        };
        state.dataHistory.Record({[apply, after]() { apply(after); }, [apply, before]() { apply(before); }},
                                 what + L(" geändert", " changed"));
        tracker.snapshot = after;
        return;
    }
    if (tracker.index != selected || !trackedValid || !tracker.snapshot) {
        tracker.index = selected;
        tracker.snapshot = (selected >= 0 && static_cast<std::size_t>(selected) < t->records.size())
            ? std::make_shared<core::legacy::ShineRecord>(t->records[static_cast<std::size_t>(selected)]) : nullptr;
    }
}
int InsertShineRecordWithHistory(EditorState& state, const ShineTableAccess& table, int index,
                                 core::legacy::ShineRecord record, bool* dirty, const std::string& label) {
    auto* t = table();
    if (!t) return -1;
    if (index < 0 || index > static_cast<int>(t->records.size())) index = static_cast<int>(t->records.size());
    t->records.insert(t->records.begin() + index, record);
    if (dirty) *dirty = true;
    auto rec = std::make_shared<core::legacy::ShineRecord>(std::move(record));
    state.dataHistory.Record({
        [table, index, rec, dirty]() {
            if (auto* tt = table()) {
                tt->records.insert(tt->records.begin() + std::min<std::ptrdiff_t>(index, static_cast<std::ptrdiff_t>(tt->records.size())), *rec);
                if (dirty) *dirty = true;
            }
        },
        [table, index, rec, dirty]() {
            if (auto* tt = table(); tt && static_cast<std::size_t>(index) < tt->records.size()) {
                *rec = tt->records[static_cast<std::size_t>(index)]; // spätere Werte für Wiederholen sichern
                tt->records.erase(tt->records.begin() + index);
                if (dirty) *dirty = true;
            }
        },
    }, label);
    return index;
}
bool DeleteShineRecordWithHistory(EditorState& state, const ShineTableAccess& table, int index,
                                  bool* dirty, const std::string& label) {
    auto* t = table();
    if (!t || index < 0 || static_cast<std::size_t>(index) >= t->records.size()) return false;
    auto rec = std::make_shared<core::legacy::ShineRecord>(t->records[static_cast<std::size_t>(index)]);
    t->records.erase(t->records.begin() + index);
    if (dirty) *dirty = true;
    state.dataHistory.Record({
        [table, index, dirty]() {
            if (auto* tt = table(); tt && static_cast<std::size_t>(index) < tt->records.size()) {
                tt->records.erase(tt->records.begin() + index);
                if (dirty) *dirty = true;
            }
        },
        [table, index, rec, dirty]() {
            if (auto* tt = table()) {
                tt->records.insert(tt->records.begin() + std::min<std::ptrdiff_t>(index, static_cast<std::ptrdiff_t>(tt->records.size())), *rec);
                if (dirty) *dirty = true;
            }
        },
    }, label);
    return true;
}

// ---------------------------------------------------------------- Portale
namespace {
bool SameRows(const core::legacy::ShnFile& a, const core::legacy::ShnFile& b) {
    if (a.rows.size() != b.rows.size()) return false;
    for (std::size_t r = 0; r < a.rows.size(); ++r)
        if (a.rows[r].values != b.rows[r].values) return false;
    return true;
}
bool SameRecords(const std::vector<core::legacy::ShineRecord>& a, const std::vector<core::legacy::ShineRecord>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].values != b[i].values) return false;
    return true;
}
} // namespace
void TrackPortalEdits(EditorState& state) {
    EditorState* s = &state;
    if (state.townPortalLoaded && state.townPortalDoc >= 0) {
        const auto& cur = ShnDocFile(state, state.townPortalDoc);
        if (!state.townPortalUndoSnapshot) state.townPortalUndoSnapshot = std::make_shared<core::legacy::ShnFile>(cur);
        else if (!SameRows(*state.townPortalUndoSnapshot, cur) && !ImGui::IsAnyItemActive()) {
            auto before = state.townPortalUndoSnapshot;
            auto after = std::make_shared<core::legacy::ShnFile>(cur);
            const auto apply = [s](const std::shared_ptr<core::legacy::ShnFile>& f) {
                if (!s->townPortalLoaded || s->townPortalDoc < 0) return;
                auto& live = ShnDocFile(*s, s->townPortalDoc);
                live.rows = f->rows;
                s->townPortalDirty = true;
                SyncTownPortalMarks(*s);
                s->townPortalUndoSnapshot = std::make_shared<core::legacy::ShnFile>(live);
                ++s->shnEditCounter;
            };
            state.dataHistory.Record({[apply, after]() { apply(after); }, [apply, before]() { apply(before); }},
                                     L("TownPortal geändert", "TownPortal changed"));
            state.townPortalUndoSnapshot = after;
        }
    }
    if (state.recallCoordLoaded) {
        if (auto* t = state.recallCoordFile.FindTable("RecallPoint")) {
            if (!state.recallUndoSnapshot) state.recallUndoSnapshot = std::make_shared<std::vector<core::legacy::ShineRecord>>(t->records);
            else if (!SameRecords(*state.recallUndoSnapshot, t->records) && !ImGui::IsAnyItemActive()) {
                auto before = state.recallUndoSnapshot;
                auto after = std::make_shared<std::vector<core::legacy::ShineRecord>>(t->records);
                const auto apply = [s](const std::shared_ptr<std::vector<core::legacy::ShineRecord>>& recs) {
                    if (auto* tt = s->recallCoordFile.FindTable("RecallPoint")) {
                        tt->records = *recs;
                        s->recallCoordDirty = true;
                        s->recallUndoSnapshot = std::make_shared<std::vector<core::legacy::ShineRecord>>(tt->records);
                    }
                };
                state.dataHistory.Record({[apply, after]() { apply(after); }, [apply, before]() { apply(before); }},
                                         L("RecallCoord geändert", "RecallCoord changed"));
                state.recallUndoSnapshot = after;
            }
        }
    }
}

} // namespace nge
