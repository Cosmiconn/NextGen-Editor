// DataFormEditor.cpp
// Allgemeiner Formular-Editor für ID-Familien (Item, Mob ...). Die Spec nennt die Tabellen mit
// ihren Client-/Server-Kopien und die Felder; gelesen wird die erste Kopie, geschrieben werden alle
// Kopien gemeinsam (ein Undo-Schritt). Eingaben werden beim Verlassen des Felds übernommen.

#include "EditorShared.hpp"

namespace nge {

namespace {
const ImVec4 kFormWarn(1.0f, 0.68f, 0.25f, 1.0f);
const ImVec4 kFormBad(1.0f, 0.48f, 0.34f, 1.0f);
const ImVec4 kFormOk(0.42f, 0.86f, 0.62f, 1.0f);

std::string TableStem(const char* file) {
    std::string stem = file;
    if (const auto dot = stem.rfind('.'); dot != std::string::npos) stem.resize(dot);
    return stem;
}
bool De() { return app::CurrentLanguage() == app::Language::German; }

const core::legacy::ShnColumn* ColumnOf(EditorState& state, const dataform::Docs& docs, int table, const char* col) {
    if (table < 0 || table >= static_cast<int>(docs.byTable.size()) || docs.byTable[table].empty()) return nullptr;
    const auto& f = ShnDocFile(state, docs.byTable[table].front());
    const int ci = ShnColumnIndexByName(f, col);
    return ci < 0 ? nullptr : &f.columns[static_cast<std::size_t>(ci)];
}
bool NumericColumn(const core::legacy::ShnColumn* c) { return c && c->kind != core::legacy::ShnValueKind::String; }
// Zahlen ohne Leerraum, Text unverändert (Leerzeichen am Rand kommen in echten Namen vor).
std::string CleanValue(const core::legacy::ShnColumn* c, const std::string& text) { return NumericColumn(c) ? TrimAscii(text) : text; }

// Auswahllisten (Wert, Beschriftung) - nur aufgebaut, solange ein Picker offen ist.
struct PickList { std::vector<std::string> values, labels; };
PickList& CachedPickList(const std::string& key) {
    static std::unordered_map<std::string, PickList> cache;
    return cache[key];
}
// InxName-Liste einer Tabelle (Beschriftung "InxName · Name (#ID)").
const PickList& InxPickList(EditorState& state, const char* file, EditorState::ShnSource side) {
    const std::string& root = side == EditorState::ShnSource::Client ? state.shnClientRoot : state.shnServerRoot;
    const int doc = SharedShnDoc(state, file, side, root);
    auto& list = CachedPickList(std::string(file) + "|" + std::to_string(doc) + "|" + std::to_string(state.shnEditCounter));
    if (!list.values.empty() || doc < 0) return list;
    const auto& f = ShnDocFile(state, doc);
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        const std::string inx = TrimAscii(ShnCellText(f, r, "InxName"));
        if (inx.empty() || (!list.values.empty() && list.values.back() == inx)) continue; // SubAbState: Stufen derselben InxName hintereinander
        const std::string name = DisplayText(ShnCellText(f, r, "Name"));
        list.values.push_back(inx);
        list.labels.push_back(inx + (name.empty() ? std::string() : "  ·  " + name) + "  (#" + ShnCellText(f, r, "ID") + ")");
    }
    return list;
}
// Alle verwendeten Werte einer Spalte mit Häufigkeit (häufigste zuerst).
const PickList& ValuePickList(EditorState& state, const dataform::Docs& docs, int table, const char* col) {
    auto& list = CachedPickList(std::string("values|") + std::to_string(table) + "|" + col + "|" +
                                std::to_string(docs.byTable.empty() || docs.byTable[table].empty() ? -1 : docs.byTable[table][0]) +
                                "|" + std::to_string(state.shnEditCounter));
    if (!list.values.empty() || docs.byTable[table].empty()) return list;
    const auto& f = ShnDocFile(state, docs.byTable[table].front());
    const int ci = ShnColumnIndexByName(f, col);
    if (ci < 0) return list;
    std::unordered_map<std::string, int> counts;
    for (const auto& row : f.rows)
        if (static_cast<std::size_t>(ci) < row.values.size()) ++counts[core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(ci)])];
    std::vector<std::pair<int, std::string>> sorted;
    for (auto& [v, n] : counts) sorted.emplace_back(n, v);
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    for (auto& [n, v] : sorted) { list.labels.push_back(v + "   (" + std::to_string(n) + "x)"); list.values.push_back(std::move(v)); }
    return list;
}

std::string FieldLabel(const dataform::Field& f) {
    const char* l = De() ? f.labelDe : f.labelEn;
    return l ? std::string(l) : std::string(f.col);
}
std::string FieldTip(const dataform::Spec& spec, const dataform::Field& f) {
    const char* t = De() ? f.tipDe : f.tipEn;
    std::string tip;
    if (t) tip = t;
    else if (f.table >= 0 && f.table < static_cast<int>(spec.tables.size())) {
        if (const auto* doc = core::manual::FindColumn(TableStem(spec.tables[static_cast<std::size_t>(f.table)].file), f.col)) {
            tip = De() ? doc->de : doc->en;
            if (!doc->certain) tip += De() ? " (vermutet)" : " (assumed)";
        }
    }
    const std::string table = f.table >= 0 && f.table < static_cast<int>(spec.tables.size())
        ? TableStem(spec.tables[static_cast<std::size_t>(f.table)].file) : std::string();
    return tip + (tip.empty() ? "" : "\n") + "[" + table + "." + f.col + "]";
}

// Name eines Mobs/Items (über die InxName-Liste) für die Anzeige neben Verweisfeldern.
std::string RefDisplay(EditorState& state, const char* file, EditorState::ShnSource side, const std::string& value, bool byId) {
    if (value.empty() || value == "-" || (byId && value == "0")) return {};
    const std::string& root = side == EditorState::ShnSource::Client ? state.shnClientRoot : state.shnServerRoot;
    const int doc = SharedShnDoc(state, file, side, root);
    if (doc < 0) return {};
    const auto& f = ShnDocFile(state, doc);
    if (byId) {
        const long long row = FormRowIn(state, doc, std::atoll(value.c_str()));
        if (row < 0) return L("nicht gefunden", "not found");
        return ShnCellText(f, static_cast<std::size_t>(row), "InxName") + "  " + DisplayText(ShnCellText(f, static_cast<std::size_t>(row), "Name"));
    }
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        if (TrimAscii(ShnCellText(f, r, "InxName")) == value) return ShnCellText(f, r, "Name").empty() ? value : DisplayText(ShnCellText(f, r, "Name"));
    return L("nicht gefunden", "not found");
}

void DrawFormField(EditorState& state, const dataform::Spec& spec, const dataform::Docs& docs, DataFormState& form,
                   const dataform::Field& fld, long long id) {
    using K = dataform::Kind;
    const std::string current = FormCellRead(state, docs, fld.table, fld.col, id);
    const auto* column = ColumnOf(state, docs, fld.table, fld.col);
    const std::string label = FieldLabel(fld);
    const std::string historyLabel = std::string(spec.id) + " #" + std::to_string(id) + " · " + fld.col;
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    std::string details;
    const bool differ = FormCopiesDiffer(state, docs, fld.table, fld.col, id, &details);
    if (!column) ImGui::TextDisabled("%s", label.c_str());
    else if (differ) ImGui::TextColored(kFormWarn, "%s !", label.c_str());
    else ImGui::TextUnformatted(label.c_str());
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        std::string tip = FieldTip(spec, fld);
        if (!column) tip += De() ? "\n(Spalte fehlt in den geladenen Daten)" : "\n(column missing in the loaded data)";
        if (differ) tip += std::string("\n\n") + L("Client/Server unterschiedlich:", "Client/server differ:") + "\n" + details;
        ImGui::SetTooltip("%s", tip.c_str());
    }
    ImGui::TableSetColumnIndex(1);
    if (!column) { ImGui::TextDisabled("-"); return; }
    ImGui::PushID(fld.col);
    ImGui::PushID(fld.table);
    auto write = [&](const std::string& value) { FormCellWrite(state, docs, fld.table, fld.col, id, value, historyLabel); };
    std::string edited;
    switch (fld.kind) {
    case K::Bool: {
        bool b = std::atoll(current.c_str()) != 0;
        if (ImGui::Checkbox("##v", &b)) write(b ? "1" : "0");
        break;
    }
    case K::Enum: {
        const long long v = std::atoll(current.c_str());
        std::string preview = current;
        for (const auto& o : fld.options) if (o.value == v) preview = current + " · " + (De() ? o.de : o.en);
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("##v", preview.c_str())) {
            for (const auto& o : fld.options) {
                const std::string item = std::to_string(o.value) + " · " + (De() ? o.de : o.en);
                if (ImGui::Selectable(item.c_str(), o.value == v)) write(std::to_string(o.value));
            }
            ImGui::EndCombo();
        }
        break;
    }
    default: {
        const bool isRef = fld.kind != K::Int && fld.kind != K::Text;
        const float width = fld.kind == K::Int ? 140.0f : fld.kind == K::Text ? 420.0f : 220.0f;
        if (DeferredInputText("##v", current, edited, width)) write(CleanValue(column, edited));
        if (!isRef) break;
        ImGui::SameLine();
        if (ImGui::SmallButton(L("Wählen...", "Pick..."))) {
            form.pickFilter[0] = '\0';
            state.itemPickerFilter[0] = '\0';
            ImGui::OpenPopup("##pick");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", L("Aus vorhandenen Werten wählen.", "Choose from existing values."));
        const bool byId = NumericColumn(column);
        std::string chosen;
        std::string display;
        const bool open = ImGui::IsPopupOpen("##pick");
        switch (fld.kind) {
        case K::ItemRef:
            if (open && DrawItemPickerPopup(state, "##pick", chosen)) {
                if (byId) {
                    EnsureItemLookup(state);
                    if (const auto it = state.itemByInx.find(chosen); it != state.itemByInx.end())
                        write(std::to_string(state.itemEntries[it->second].id));
                } else write(chosen);
            }
            ImGui::SameLine();
            if (byId) DrawItemIconById(state, std::atoll(current.c_str()), ImGui::GetTextLineHeight());
            else DrawItemIconByInx(state, current, ImGui::GetTextLineHeight());
            display = RefDisplay(state, "ItemInfo.shn", EditorState::ShnSource::Client, current, byId);
            break;
        case K::MobRef: {
            if (open) {
                const auto& list = InxPickList(state, "MobInfo.shn", EditorState::ShnSource::Client);
                if (StringPickerPopup("##pick", list.values, form.pickFilter, sizeof(form.pickFilter), chosen, &list.labels)) {
                    if (byId) {
                        const int doc = SharedShnDoc(state, "MobInfo.shn", EditorState::ShnSource::Client, state.shnClientRoot);
                        const auto& f = ShnDocFile(state, doc);
                        for (std::size_t r = 0; r < f.rows.size(); ++r)
                            if (TrimAscii(ShnCellText(f, r, "InxName")) == chosen) { write(ShnCellText(f, r, "ID")); break; }
                    } else write(chosen);
                }
            }
            display = RefDisplay(state, "MobInfo.shn", EditorState::ShnSource::Client, current, byId);
            break;
        }
        case K::SkillRef:
        case K::StateRef: {
            const char* file = fld.kind == K::SkillRef ? "ActiveSkill.shn" : "AbState.shn";
            const auto side = fld.kind == K::SkillRef ? EditorState::ShnSource::Client : EditorState::ShnSource::Server;
            if (open) {
                const auto& list = InxPickList(state, file, side);
                if (StringPickerPopup("##pick", list.values, form.pickFilter, sizeof(form.pickFilter), chosen, &list.labels)) write(chosen);
            }
            display = RefDisplay(state, file, side, current, byId);
            break;
        }
        case K::DropGroupRef: {
            EnsureDropGroupCatalog(state);
            if (open) {
                auto& list = CachedPickList("dropgroups|" + std::to_string(state.shnEditCounter));
                if (list.values.empty()) { list.values = state.dropGroups.ItemInfoDropGroupNames(); list.labels = list.values; }
                if (StringPickerPopup("##pick", list.values, form.pickFilter, sizeof(form.pickFilter), chosen, &list.labels)) write(chosen);
            }
            if (!core::legacy::DropGroupCatalog::IsEmptySlot(current) && state.dropGroupLoaded) {
                // Welche ItemDropGroup-Zeilen nennen diesen Namen als ItemID?
                const auto* t = state.dropGroupFile.FindTable("ItemDropGroup");
                const auto cols = t ? core::legacy::FindDropGroupColumns(*t) : core::legacy::DropGroupColumns{};
                std::string firstGroup;
                int uses = 0;
                if (t)
                    for (const auto& r : t->records)
                        if (ShineRecordValue(r, cols.item) == current) { if (!uses++) firstGroup = ShineRecordValue(r, cols.group); }
                ImGui::SameLine();
                if (uses == 0) ImGui::TextColored(kFormWarn, "%s", L("in keiner Drop-Gruppe", "in no drop group"));
                else {
                    ImGui::TextDisabled(L("in %d Drop-Gruppen-Zeile(n)", "in %d drop group row(s)"), uses);
                    ImGui::SameLine();
                    if (ImGui::SmallButton(L("Öffnen##dg", "Open##dg"))) {
                        state.dropGroupSelected = firstGroup;
                        state.dropEditorTabRequest = 1;
                        state.shnSubTab = 10;
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", L("Gruppen in World/ItemDropGroup.txt, deren ItemID diesen DropGroup-Namen nennt (die Mobs droppen dann eines der Items mit diesem Namen).",
                                              "Groups in World/ItemDropGroup.txt whose ItemID names this DropGroup name (mobs then drop one of the items with this name)."));
            }
            break;
        }
        case K::InxRef: {
            const auto side = fld.refServer ? EditorState::ShnSource::Server : EditorState::ShnSource::Client;
            if (open && fld.refFile) {
                const auto& list = InxPickList(state, fld.refFile, side);
                if (StringPickerPopup("##pick", list.values, form.pickFilter, sizeof(form.pickFilter), chosen, &list.labels)) write(chosen);
            }
            if (fld.refFile) display = RefDisplay(state, fld.refFile, side, current, byId);
            break;
        }
        case K::Values:
            if (open) {
                const auto& list = ValuePickList(state, docs, fld.table, fld.col);
                if (StringPickerPopup("##pick", list.values, form.pickFilter, sizeof(form.pickFilter), chosen, &list.labels)) write(chosen);
            }
            break;
        default: break;
        }
        if (!display.empty()) {
            ImGui::SameLine();
            const bool missing = display == L("nicht gefunden", "not found");
            ImGui::TextColored(missing ? kFormBad : kFormOk, "%s", display.c_str());
        }
        break;
    }
    }
    ImGui::PopID();
    ImGui::PopID();
}
} // namespace

dataform::Docs ResolveFormDocs(EditorState& state, const dataform::Spec& spec) {
    dataform::Docs docs;
    for (const auto& t : spec.tables) {
        std::vector<int> copies;
        if (t.client && !state.shnClientRoot.empty())
            if (const int d = SharedShnDoc(state, t.file, EditorState::ShnSource::Client, state.shnClientRoot); d >= 0) copies.push_back(d);
        if (t.server && !state.shnServerRoot.empty())
            if (const int d = SharedShnDoc(state, t.file, EditorState::ShnSource::Server, state.shnServerRoot); d >= 0) copies.push_back(d);
        docs.byTable.push_back(std::move(copies));
    }
    return docs;
}

long long FormRowIn(EditorState& state, int document, long long id) {
    if (document < 0 || document >= static_cast<int>(state.shnFiles.size())) return -1;
    return SkillRowIn(state, document, id); // Suche über die Spalte "ID" mit Cache je Dokument
}

std::string FormCellRead(EditorState& state, const dataform::Docs& docs, int table, const char* col, long long id) {
    if (table < 0 || table >= static_cast<int>(docs.byTable.size())) return {};
    for (const int d : docs.byTable[static_cast<std::size_t>(table)]) {
        const long long row = FormRowIn(state, d, id);
        if (row >= 0) return ShnCellText(ShnDocFile(state, d), static_cast<std::size_t>(row), col);
    }
    return {};
}

bool FormCellWrite(EditorState& state, const dataform::Docs& docs, int table, const char* col, long long id,
                   const std::string& text, const std::string& label) {
    if (table < 0 || table >= static_cast<int>(docs.byTable.size())) return false;
    bool any = false;
    DataHistoryBegin(state, label);
    for (const int d : docs.byTable[static_cast<std::size_t>(table)]) {
        const long long row = FormRowIn(state, d, id);
        const int column = ShnColumnIndexByName(ShnDocFile(state, d), col);
        if (row < 0 || column < 0) continue;
        if (ShnCellText(ShnDocFile(state, d), static_cast<std::size_t>(row), col) == text) continue;
        any = ApplyShnCellText(state, d, static_cast<int>(row), column, text) || any;
    }
    DataHistoryCommit(state);
    return any;
}

bool FormCopiesDiffer(EditorState& state, const dataform::Docs& docs, int table, const char* col, long long id, std::string* details) {
    if (details) details->clear();
    if (table < 0 || table >= static_cast<int>(docs.byTable.size()) || docs.byTable[table].size() < 2) return false;
    bool haveFirst = false, differ = false;
    std::string first;
    for (const int d : docs.byTable[static_cast<std::size_t>(table)]) {
        const auto& doc = state.shnFiles[static_cast<std::size_t>(d)];
        const long long row = FormRowIn(state, d, id);
        std::string value;
        if (row < 0 || ShnColumnIndexByName(doc.file, col) < 0) { value = L("(Zeile/Feld fehlt)", "(row/field missing)"); differ = true; }
        else {
            value = ShnCellText(doc.file, static_cast<std::size_t>(row), col);
            if (!haveFirst) { first = value; haveFirst = true; }
            else if (value != first) differ = true;
        }
        if (details) *details += (details->empty() ? "" : "\n") + std::string(ShnSourceName(doc.source)) + ": " + value;
    }
    return differ;
}

long long CloneFormRecord(EditorState& state, const dataform::Spec& spec, const dataform::Docs& docs,
                          long long templateId, const std::string& newInx, const std::string& newName,
                          std::vector<std::string>& report) {
    report.clear();
    const int mainDoc = docs.Main();
    if (newInx.empty() || newInx.find_first_of(" \t") != std::string::npos) {
        report.push_back(L("Bitte einen InxName ohne Leerzeichen angeben.", "Please enter an InxName without spaces."));
        return -1;
    }
    if (mainDoc < 0) { report.push_back(L("Haupttabelle nicht geladen.", "Main table not loaded.")); return -1; }
    const long long templateRow = FormRowIn(state, mainDoc, templateId);
    if (templateRow < 0) { report.push_back(L("Vorlage nicht gefunden.", "Template not found.")); return -1; }
    const std::string templateInx = TrimAscii(ShnCellText(ShnDocFile(state, mainDoc), static_cast<std::size_t>(templateRow), "InxName"));
    std::set<long long> used;
    const core::legacy::ShnColumn* idColumn = nullptr;
    for (const auto& copies : docs.byTable)
        for (const int d : copies) {
            const auto& f = ShnDocFile(state, d);
            const int c = ShnColumnIndexByName(f, "ID");
            CollectShnInts(f, c, used);
            if (!idColumn && d == mainDoc && c >= 0) idColumn = &f.columns[static_cast<std::size_t>(c)];
            for (std::size_t r = 0; r < f.rows.size(); ++r)
                if (TrimAscii(ShnCellText(f, r, "InxName")) == newInx) {
                    report.push_back(std::string(L("InxName existiert bereits: ", "InxName already exists: ")) + newInx + " (" + f.FileName() + ")");
                    return -1;
                }
        }
    const long long newId = idColumn ? SuggestFreeShnId(*idColumn, used) : -1;
    if (newId < 0) { report.push_back(L("Keine freie ID.", "No free ID.")); return -1; }
    DataHistoryBegin(state, std::string(spec.id) + " " + newInx + L(" angelegt", " created"));
    for (std::size_t t = 0; t < docs.byTable.size(); ++t) {
        for (const int d : docs.byTable[t]) {
            auto& doc = state.shnFiles[static_cast<std::size_t>(d)];
            // Vorlage dieser Tabelle über die ID (der InxName kann je Tabelle abweichen, z. B. AbStateView).
            const long long tRow = FormRowIn(state, d, templateId);
            if (tRow < 0) {
                report.push_back(std::string("- ") + ShnSourceName(doc.source) + " " + spec.tables[t].file + L(": keine Vorlage-Zeile (übersprungen)", ": no template row (skipped)"));
                continue;
            }
            const std::string rowInx = ShnCellText(doc.file, static_cast<std::size_t>(tRow), spec.tables[t].inxColumn);
            const auto created = CloneRowsById(state, spec.tables[t].file, doc.source, spec.tables[t].inxColumn, templateId,
                                               rowInx, newId, newInx, report);
            if (created.empty()) continue;
            const int nameColumn = ShnColumnIndexByName(doc.file, "Name");
            if (!newName.empty() && nameColumn >= 0)
                for (const std::size_t r : created) ApplyShnCellText(state, d, static_cast<int>(r), nameColumn, newName, false);
        }
    }
    DataHistoryCommit(state);
    report.insert(report.begin(), std::string(L("Neue ID ", "New ID ")) + std::to_string(newId) + " · " + newInx + "  (" +
                                      L("Vorlage ", "template ") + templateInx + L(") - noch nicht gespeichert.", ") - not saved yet."));
    return newId;
}

void DrawDataFormEditor(EditorState& state, const dataform::Spec& spec, DataFormState& form,
                        const char* title, IconDrawFn icon, const char* semanticIcon,
                        const std::function<void(const dataform::Docs&, long long)>& header,
                        const std::function<std::vector<std::string>(const dataform::Docs&, long long)>& problems,
                        const std::function<void(long long, float)>& listIcon) {
    const dataform::Docs docs = ResolveFormDocs(state, spec);
    const int mainDoc = docs.Main();
    DrawPanelHeader((std::string(spec.id) + "FormHeader").c_str(), title, icon, semanticIcon,
                    L("Client + Server gemeinsam bearbeiten", "Edit client + server together"));
    if (const std::size_t dirty = DirtyShnDocumentCount(state); dirty > 0) {
        ImGui::SameLine();
        ImGui::TextColored(kFormWarn, L("* %zu SHN geändert", "* %zu SHN modified"), dirty);
    }
    if (mainDoc < 0) {
        ImGui::TextColored(kFormBad, L("%s nicht gefunden - Client-/Server-Ordner im Projekt prüfen.", "%s not found - check the client/server folders of the project."),
                           spec.tables.front().file);
        return;
    }
    for (std::size_t t = 0; t < spec.tables.size(); ++t) {
        const auto& tb = spec.tables[t];
        const std::size_t expected = static_cast<std::size_t>(tb.client && !state.shnClientRoot.empty()) + static_cast<std::size_t>(tb.server && !state.shnServerRoot.empty());
        if (docs.byTable[t].size() < expected)
            ImGui::TextColored(kFormWarn, L("! %s: nicht jede Kopie gefunden (Client/Server) - es wird nur die vorhandene bearbeitet.",
                                            "! %s: not every copy found (client/server) - only the existing one is edited."), tb.file);
    }
    const auto& mf = ShnDocFile(state, mainDoc);

    if (UI::Button(L("Alle geänderten SHN speichern", "Save all changed SHN")) ||
        (ShortcutPressed(state.shortcutSave) && DirtyShnDocumentCount(state) > 0)) {
        const auto [saved, failed] = SaveAllDirtyShnDocuments(state);
        form.report = {std::string(L("Gespeichert: ", "Saved: ")) + std::to_string(saved) +
                       (failed ? std::string(", ") + L("Fehler: ", "errors: ") + std::to_string(failed) + " · " + state.shnStatus : std::string())};
    }
    ImGui::SameLine();
    DrawDataHistoryButtons(state, (std::string(spec.id) + "History").c_str());
    ImGui::SameLine();
    DrawSearchInput((std::string(spec.id) + "Filter").c_str(), L("Suche: Name, InxName oder ID", "Search: name, InxName or ID"),
                    form.filter, sizeof(form.filter), 240.0f);
    const char* quick[] = {L("Alle", "All"), L("Geändert", "Modified"), L("Client/Server", "Client/server"), L("Probleme", "Problems")};
    for (int q = 0; q < 4; ++q) {
        ImGui::SameLine();
        if (SceneQuickFilterButton((std::string(spec.id) + "Quick" + std::to_string(q)).c_str(), quick[q], form.quickFilter == q)) form.quickFilter = q;
    }

    // Liste (gecacht über Filter, Revision und Dokumente).
    std::string key = std::string(form.filter) + "|" + std::to_string(form.quickFilter) + "|" + std::to_string(state.shnEditCounter) +
                      "|" + std::to_string(mf.rows.size());
    for (const auto& copies : docs.byTable) for (const int d : copies) key += "," + std::to_string(d);
    if (key != form.listKey) {
        form.listKey = key;
        form.visible.clear();
        const std::string needle = LowerAscii(form.filter);
        std::unordered_set<long long> modified;
        if (form.quickFilter == 1)
            for (const auto& copies : docs.byTable)
                for (const int d : copies) {
                    const auto& doc = state.shnFiles[static_cast<std::size_t>(d)];
                    for (std::size_t r = 0; r < doc.file.rows.size() && r < doc.cellDirty.size(); ++r)
                        if (std::any_of(doc.cellDirty[r].begin(), doc.cellDirty[r].end(), [](std::uint8_t v) { return v != 0; }))
                            modified.insert(std::atoll(ShnCellText(doc.file, r, "ID").c_str()));
                }
        for (std::size_t r = 0; r < mf.rows.size(); ++r) {
            const std::string idText = ShnCellText(mf, r, "ID");
            const long long id = std::atoll(idText.c_str());
            if (!needle.empty() &&
                LowerAscii(ShnCellText(mf, r, "InxName") + " " + DisplayText(ShnCellText(mf, r, "Name")) + " " + idText).find(needle) == std::string::npos)
                continue;
            if (form.quickFilter == 1 && !modified.contains(id)) continue;
            if (form.quickFilter == 2) {
                bool differ = false;
                for (std::size_t t = 0; t < docs.byTable.size() && !differ; ++t) {
                    if (docs.byTable[t].size() < 2) continue;
                    const auto& a = ShnDocFile(state, docs.byTable[t][0]);
                    const auto& b = ShnDocFile(state, docs.byTable[t][1]);
                    const long long ra = FormRowIn(state, docs.byTable[t][0], id), rb = FormRowIn(state, docs.byTable[t][1], id);
                    if (ra < 0 || rb < 0) { differ = true; break; }
                    differ = a.rows[static_cast<std::size_t>(ra)].values != b.rows[static_cast<std::size_t>(rb)].values;
                }
                if (!differ) continue;
            }
            if (form.quickFilter == 3 && (!problems || problems(docs, id).empty())) continue;
            form.visible.push_back(r);
        }
    }

    const float listW = std::clamp(ImGui::GetContentRegionAvail().x * 0.28f, 300.0f, 420.0f);
    ImGui::BeginChild((std::string("##formList") + spec.id).c_str(), ImVec2(listW, 0.0f), true);
    ImGui::TextDisabled(L("%zu / %zu Einträge", "%zu / %zu entries"), form.visible.size(), mf.rows.size());
    const float rowH = ImGui::GetTextLineHeight() + 4.0f;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(form.visible.size()), rowH + ImGui::GetStyle().ItemSpacing.y);
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const std::size_t r = form.visible[static_cast<std::size_t>(i)];
            const long long id = std::atoll(ShnCellText(mf, r, "ID").c_str());
            ImGui::PushID(static_cast<int>(r));
            const float x = ImGui::GetCursorPosX();
            if (ImGui::Selectable("##row", form.selectedId == id, ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, rowH))) {
                form.selectedId = id;
                form.report.clear();
            }
            ImGui::SameLine(x + 2.0f);
            if (listIcon) { listIcon(id, rowH); ImGui::SameLine(); }
            const std::string name = DisplayText(ShnCellText(mf, r, "Name"));
            ImGui::Text("#%lld  %s", id, ShnCellText(mf, r, "InxName").c_str());
            if (!name.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", name.c_str()); }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild((std::string("##formDetail") + spec.id).c_str(), ImVec2(0.0f, 0.0f), true);
    const long long sel = form.selectedId;
    const long long selRow = sel >= 0 ? FormRowIn(state, mainDoc, sel) : -1;
    if (selRow < 0) {
        ImGui::TextDisabled("%s", L("Links einen Eintrag wählen.", "Choose an entry on the left."));
        ImGui::EndChild();
        return;
    }
    const std::string inx = ShnCellText(mf, static_cast<std::size_t>(selRow), "InxName");
    ImGui::TextColored(UiTheme::AccentCyan, "%s", inx.c_str());
    ImGui::SameLine();
    ImGui::Text("#%lld  %s", sel, DisplayText(ShnCellText(mf, static_cast<std::size_t>(selRow), "Name")).c_str());
    if (UI::Button(L("Löschen##form", "Delete##form"))) RequestShnRowDelete(state, mainDoc, static_cast<int>(selRow));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", L("Löscht den Eintrag in allen geladenen Tabellen der Familie (beide Seiten); vorher werden Verweise gezeigt. Strg+Z macht es rückgängig.",
                                  "Deletes the entry in every loaded table of the family (both sides); references are shown first. Ctrl+Z undoes it."));
    ImGui::SameLine();
    if (UI::Button(L("Im SHN-Raster öffnen", "Open in SHN grid"))) {
        std::vector<std::string> files;
        for (const auto& t : spec.tables) files.emplace_back(t.file);
        OpenShnRecordById(state, files, EditorState::ShnSource::Client, sel);
        state.shnSubTab = 0;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", L("Zeigt die Zeile im Single-SHN-Raster (alle Spalten als Tabelle).", "Shows the row in the single SHN grid (all columns as a table)."));
    ImGui::TextDisabled("%s", L("Der InxName eines vorhandenen Eintrags bleibt fest (andere Tabellen verweisen darauf). Für einen neuen: 'Neu aus diesem anlegen'.",
                                "The InxName of an existing entry stays fixed (other tables refer to it). For a new one use 'Create new from this one'."));
    ImGui::Separator();

    if (header) header(docs, sel);
    if (problems) {
        const auto found = problems(docs, sel);
        if (found.empty()) ImGui::TextColored(kFormOk, "%s", L("Prüfung: keine Probleme gefunden.", "Check: no problems found."));
        for (const auto& p : found) ImGui::TextColored(kFormWarn, "! %s", p.c_str());
    }

    if (UI::CollapsingHeader(L("Neu aus diesem anlegen (Vorlage)", "Create new from this one (template)"))) {
        ImGui::TextWrapped("%s", L("Klont alle Zeilen dieses Eintrags (jede Tabelle der Familie, Client und Server) unter einer freien ID. Danach die Werte unten anpassen.",
                                   "Clones every row of this entry (each table of the family, client and server) under a free ID. Then adjust the values below."));
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##newInx", L("Neuer InxName (eindeutig)", "New InxName (unique)"), form.newInx, sizeof(form.newInx));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##newName", L("Neuer Anzeigename (leer = Vorlage)", "New display name (empty = template)"), form.newName, sizeof(form.newName));
        ImGui::SameLine();
        if (UI::Button(L("Anlegen##form", "Create##form"))) {
            const long long created = CloneFormRecord(state, spec, docs, sel, TrimAscii(form.newInx), TrimAscii(form.newName), form.report);
            if (created >= 0) { form.selectedId = created; form.newInx[0] = form.newName[0] = '\0'; }
        }
    }
    for (const auto& line : form.report) ImGui::TextWrapped("%s", line.c_str());

    std::set<std::string> curated = {"ID", "InxName"};
    for (std::size_t t = 0; t < spec.tables.size(); ++t) curated.insert(std::to_string(t) + "|" + spec.tables[t].inxColumn);
    for (const auto& sec : spec.sections) {
        for (const auto& f : sec.fields) curated.insert(std::to_string(f.table) + "|" + f.col);
        if (!UI::CollapsingHeader(De() ? sec.titleDe : sec.titleEn, sec.defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0)) continue;
        ImGui::PushID(sec.titleEn);
        if (ImGui::BeginTable("##form", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 230.0f);
            ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
            for (const auto& f : sec.fields) DrawFormField(state, spec, docs, form, f, sel);
            ImGui::EndTable();
        }
        ImGui::PopID();
    }
    if (UI::CollapsingHeader(L("Alle übrigen Spalten (roh)", "All remaining columns (raw)"))) {
        ImGui::TextDisabled("%s", L("Spalten ohne eigenes Formularfeld; Werte als Text, geschrieben in alle Kopien.",
                                    "Columns without their own form field; values as text, written to every copy."));
        if (ImGui::BeginTable("##raw", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn(L("Tabelle", "Table"), ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn(L("Spalte", "Column"), ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn(L("Wert", "Value"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (std::size_t t = 0; t < docs.byTable.size(); ++t) {
                if (docs.byTable[t].empty()) continue;
                const auto& f = ShnDocFile(state, docs.byTable[t].front());
                for (const auto& col : f.columns) {
                    if (curated.contains(col.name) || curated.contains(std::to_string(t) + "|" + col.name)) continue;
                    ImGui::PushID(static_cast<int>(t));
                    ImGui::PushID(col.name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(TableStem(spec.tables[t].file).c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(col.name.c_str());
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        if (const auto* doc = core::manual::FindColumn(TableStem(spec.tables[t].file), col.name))
                            ImGui::SetTooltip("%s", De() ? doc->de : doc->en);
                    ImGui::TableSetColumnIndex(2);
                    std::string edited;
                    if (DeferredInputText("##v", FormCellRead(state, docs, static_cast<int>(t), col.name.c_str(), sel), edited, 280.0f))
                        FormCellWrite(state, docs, static_cast<int>(t), col.name.c_str(), sel, CleanValue(&col, edited),
                                      std::string(spec.id) + " #" + std::to_string(sel) + " · " + col.name);
                    ImGui::PopID();
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

} // namespace nge
