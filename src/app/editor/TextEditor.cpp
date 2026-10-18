// TextEditor.cpp
// Texte und Lokalisierung (Datei Editor, Unterreiter 15): alle Texttabellen von Client und Server an
// einer Stelle - Suche über alle Tabellen, Bearbeiten (Client und Server gemeinsam, Strg+Z),
// "verwendet von", nächste freie ID, neue Zeilen, Löschen und CSV-Export/-Import für Übersetzungen.
// Textspalten und Schlüssel bestimmt core::legacy::TextTables; geschrieben wird über die gemeinsame
// SHN-Datenschicht (ApplyShnCellText, InsertShnRowWithHistory, DeleteShnRowWithHistory).

#include "EditorShared.hpp"

#include "mapeditor/core/legacy/TextTables.hpp"

#include <fstream>
#include <sstream>

namespace nge {
namespace {

using Source = EditorState::ShnSource;
namespace legacy = core::legacy;

enum class TextUse { None, ScriptMsg, MobKillAnnounce, AbState };

struct TextTableDef {
    const char* file;
    bool client;
    bool server;
    bool canAdd;   // neue Zeilen sinnvoll (Schlüssel wird in Daten/Skripten vergeben, nicht im Client-Code)
    TextUse use;
    const char* noteDe;
    const char* noteEn;
};

const TextTableDef kTables[] = {
    {"TextData.shn", true, false, false, TextUse::None,
     "Oberflächentexte des Clients. Die Text-ID ist ein Hash aus dem Client-Code - Texte ändern ja, neue Zeilen wirken nicht.",
     "Client interface texts. The text ID is a hash from the client code - change texts, new rows have no effect."},
    {"TextData2.shn", true, false, false, TextUse::None,
     "Weitere Oberflächentexte des Clients (Text-ID aus dem Client-Code).", "More client interface texts (text ID from the client code)."},
    {"TextData3.shn", true, false, false, TextUse::None,
     "Weitere Oberflächentexte des Clients (Text-ID aus dem Client-Code).", "More client interface texts (text ID from the client code)."},
    {"ScriptMsg.shn", true, false, true, TextUse::ScriptMsg,
     "Meldungen aus Server-KI-Skripten: SM_Inx wird in LuaScript über cMessage/cNPCChat/ScriptMsg aufgerufen; Farbe, Größe und Dauer je Zeile.",
     "Messages from server AI scripts: SM_Inx is called in LuaScript via cMessage/cNPCChat/ScriptMsg; color, size and duration per row."},
    {"QuestDialog.shn", true, true, true, TextUse::None,
     "Quest-Texte (Titel, Beschreibung, SAY). Der Quest-Editor zeigt sie im Zusammenhang einer Quest.",
     "Quest texts (title, description, SAY). The quest editor shows them in the context of a quest."},
    {"MobKillAnnounceText.shn", true, false, true, TextUse::MobKillAnnounce,
     "Ansage beim Besiegen eines Bosses; MobKillAnnounce.shn (Server) ordnet Mob und TextIndex zu.",
     "Announcement when a boss is defeated; MobKillAnnounce.shn (server) maps mob to TextIndex."},
    {"AbStateMsg.shn", true, false, true, TextUse::AbState,
     "Meldung zu einem Zustand (AbStateIDX = InxName aus AbState.shn).", "Message for a state (AbStateIDX = InxName from AbState.shn)."},
    {"BasicInfoTip.shn", true, false, true, TextUse::None,
     "Tipps nach Stufenbereich (BIT_MinLv bis BIT_MaxLv).", "Tips by level range (BIT_MinLv to BIT_MaxLv)."},
    {"ItemActionEffectDesc.shn", true, false, true, TextUse::None,
     "Beschreibung von Item-Aktionen (ItemActionID).", "Description of item actions (ItemActionID)."},
    {"SetItemName.shn", true, false, true, TextUse::None,
     "Namen der Item-Sets (SetItemIndex).", "Names of item sets (SetItemIndex)."},
    {"KingdomQuestDesc.shn", true, false, false, TextUse::None,
     "Beschreibungen der Kingdom Quests (nach Zeile adressiert).", "Kingdom quest descriptions (addressed by row)."},
    {"KQVoteDesc.shn", true, false, false, TextUse::None,
     "Abstimmungstexte der Kingdom Quests.", "Kingdom quest vote texts."},
    {"CollectCardGroupDesc.shn", true, true, true, TextUse::None,
     "Namen der Sammelkarten-Mobgruppen (CC_CardMobGroup).", "Names of collect card mob groups (CC_CardMobGroup)."},
    {"AnnounceData.shn", false, true, false, TextUse::None,
     "Server-Ansagen; Type ist fest im Server-Code, %s/%d sind Platzhalter und müssen erhalten bleiben.",
     "Server announcements; Type is fixed in the server code, %s/%d are placeholders and must be kept."},
    {"MsgWorldManager.shn", false, true, false, TextUse::None,
     "Meldungen des World-Managers (nach Zeile adressiert; Platzhalter erhalten).",
     "World manager messages (addressed by row; keep placeholders)."},
    {"ErrorCodeTable.shn", false, true, false, TextUse::None,
     "Fehlercode -> Meldungsschlüssel (Server).", "Error code -> message key (server)."},
};
constexpr int kTableCount = static_cast<int>(std::size(kTables));

struct TableDocs {
    int main = -1;  // Client, sonst Server
    int peer = -1;  // Server-Kopie bei Client+Server-Tabellen
};

// Gemeinsames Dokument; fehlende Dateien werden je Ordner nur einmal gesucht.
int TextDoc(EditorState& state, const char* file, Source side) {
    const std::string& root = side == Source::Client ? state.shnClientRoot : state.shnServerRoot;
    if (root.empty()) return -1;
    const std::string key = root + "|" + file;
    if (state.textEditor.missing.contains(key)) return -1;
    const int doc = SharedShnDoc(state, file, side, root);
    if (doc < 0) state.textEditor.missing.insert(key);
    return doc;
}

TableDocs DocsFor(EditorState& state, const TextTableDef& def) {
    TableDocs d;
    if (def.client) d.main = TextDoc(state, def.file, Source::Client);
    if (def.server) {
        const int server = TextDoc(state, def.file, Source::Server);
        if (d.main < 0) d.main = server;
        else d.peer = server;
    }
    return d;
}

std::string OneLine(std::string text, std::size_t limit) {
    for (char& c : text) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    if (text.size() > limit) { text.resize(limit); text += "..."; }
    return text;
}

bool NumericKey(const legacy::ShnFile& f) {
    return !f.columns.empty() && f.columns.front().kind != legacy::ShnValueKind::String;
}

// Zeile mit demselben Schlüssel in einer anderen Datei (Server-Kopie).
int RowForKey(const legacy::ShnFile& f, const std::string& key) {
    const auto keys = legacy::TextRowKeys(f);
    for (std::size_t r = 0; r < keys.size(); ++r) if (keys[r] == key) return static_cast<int>(r);
    return -1;
}

// Prüft die Länge für feste Zeichenketten (eine 0 bleibt als Abschluss).
bool FitsColumn(const legacy::ShnColumn& c, const std::string& text, std::string& why) {
    if (c.kind != legacy::ShnValueKind::String || c.type == 26 || c.length == 0 || text.size() <= c.length - 1) return true;
    why = c.name + ": " + std::to_string(text.size()) + " > " + std::to_string(c.length - 1) + L(" Byte erlaubt", " bytes allowed");
    return false;
}

// Schreibt eine Zelle in Hauptdokument und Server-Kopie (gleicher Schlüssel, gleiche Spalte), ein Undo-Schritt.
bool WriteText(EditorState& state, const TableDocs& docs, int row, int column, const std::string& text) {
    auto& mf = ShnDocFile(state, docs.main);
    const auto& col = mf.columns[static_cast<std::size_t>(column)];
    std::string why;
    if (!FitsColumn(col, text, why)) { state.textEditor.report = {why}; return false; }
    const std::string key = legacy::TextRowKeys(mf)[static_cast<std::size_t>(row)];
    DataHistoryBegin(state, std::string(L("Text geändert: ", "Text changed: ")) + mf.FileName() + " " + key + " · " + col.name);
    bool ok = ApplyShnCellText(state, docs.main, row, column, text);
    if (docs.peer >= 0) {
        auto& pf = ShnDocFile(state, docs.peer);
        const int prow = RowForKey(pf, key);
        const int pcol = ShnColumnIndexByName(pf, col.name);
        if (prow >= 0 && pcol >= 0) ok = ApplyShnCellText(state, docs.peer, prow, pcol, text) && ok;
    }
    DataHistoryCommit(state);
    return ok;
}

void BuildLuaIndex(EditorState& state) {
    auto& te = state.textEditor;
    if (te.luaIndexRoot == state.shnServerRoot) return;
    te.luaIndexRoot = state.shnServerRoot;
    te.luaLiterals.clear();
    if (state.shnServerRoot.empty()) return;
    const std::filesystem::path root = std::filesystem::path(state.shnServerRoot) / "LuaScript";
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return;
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        if (!it->is_regular_file(ec) || LowerAscii(it->path().extension().string()) != ".lua") continue;
        std::ifstream in(it->path(), std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        const std::string text = buffer.str();
        const std::string rel = std::filesystem::relative(it->path(), root, ec).generic_string();
        for (std::size_t p = text.find('"'); p != std::string::npos;) {
            const std::size_t q = text.find('"', p + 1);
            if (q == std::string::npos) break;
            auto& files = te.luaLiterals[text.substr(p + 1, q - p - 1)];
            if (files.empty() || files.back() != rel) files.push_back(rel);
            p = text.find('"', q + 1);
        }
    }
}

std::vector<std::string> UsesOf(EditorState& state, const TextTableDef& def, const legacy::ShnFile& f, int row) {
    std::vector<std::string> out;
    switch (def.use) {
        case TextUse::None: break;
        case TextUse::ScriptMsg: {
            const std::string inx = TrimAscii(ShnCellText(f, static_cast<std::size_t>(row), "SM_Inx"));
            BuildLuaIndex(state);
            if (const auto it = state.textEditor.luaLiterals.find(inx); it != state.textEditor.luaLiterals.end())
                for (const auto& file : it->second) out.push_back("LuaScript/" + file);
            if (const int face = SharedShnDoc(state, "FaceCutMsg.shn", Source::Client, state.shnClientRoot); face >= 0) {
                const auto& ff = ShnDocFile(state, face);
                for (std::size_t r = 0; r < ff.rows.size(); ++r)
                    if (TrimAscii(ShnCellText(ff, r, "SM_Inx")) == inx)
                        out.push_back("FaceCutMsg.shn: " + ShnCellText(ff, r, "FaceCutFile"));
            }
            break;
        }
        case TextUse::MobKillAnnounce: {
            const std::string index = ShnCellText(f, static_cast<std::size_t>(row), "TextIndex");
            const int ann = SharedShnDoc(state, "MobKillAnnounce.shn", Source::Server, state.shnServerRoot);
            const int mobs = SharedShnDoc(state, "MobInfo.shn", Source::Client, state.shnClientRoot);
            if (ann < 0) break;
            const auto& af = ShnDocFile(state, ann);
            for (std::size_t r = 0; r < af.rows.size(); ++r) {
                if (ShnCellText(af, r, "TextIndex") != index) continue;
                const std::string mobId = ShnCellText(af, r, "MobID");
                std::string name;
                if (mobs >= 0) {
                    const auto& mf = ShnDocFile(state, mobs);
                    for (std::size_t m = 0; m < mf.rows.size() && name.empty(); ++m)
                        if (ShnCellText(mf, m, "ID") == mobId) name = ShnCellText(mf, m, "Name");
                }
                out.push_back("MobKillAnnounce.shn: Mob " + mobId + (name.empty() ? std::string() : " · " + name));
            }
            break;
        }
        case TextUse::AbState: {
            const std::string inx = TrimAscii(ShnCellText(f, static_cast<std::size_t>(row), "AbStateIDX"));
            if (const int ab = SharedShnDoc(state, "AbState.shn", Source::Client, state.shnClientRoot); ab >= 0) {
                const auto& abf = ShnDocFile(state, ab);
                for (std::size_t r = 0; r < abf.rows.size(); ++r)
                    if (TrimAscii(ShnCellText(abf, r, "InxName")) == inx) out.push_back("AbState.shn: ID " + ShnCellText(abf, r, "ID") + " · " + inx);
            }
            break;
        }
    }
    return out;
}

long long NextFreeKey(EditorState& state, const TableDocs& docs) {
    const auto& mf = ShnDocFile(state, docs.main);
    std::set<long long> used;
    CollectShnInts(mf, 0, used);
    if (docs.peer >= 0) CollectShnInts(ShnDocFile(state, docs.peer), 0, used);
    return SuggestFreeShnId(mf.columns.front(), used);
}

// Neue Zeile: Kopie der gewählten (sonst letzten) Zeile mit freier ID, leeren Texten; Client und Server.
int AddTextRow(EditorState& state, const TextTableDef& def, const TableDocs& docs, int templateRow) {
    auto& mf = ShnDocFile(state, docs.main);
    if (mf.rows.empty() || !NumericKey(mf)) return -1;
    const int src = templateRow >= 0 && templateRow < static_cast<int>(mf.rows.size()) ? templateRow : static_cast<int>(mf.rows.size()) - 1;
    const long long id = NextFreeKey(state, docs);
    const auto textColumns = legacy::TextColumns(mf);
    const auto makeRow = [&](const legacy::ShnFile& f, const legacy::ShnRow& base) {
        legacy::ShnRow row = base;
        if (auto v = legacy::ParseShnValue(f.columns.front(), std::to_string(id))) row.values.front() = *v;
        for (std::size_t c = 0; c < f.columns.size() && c < row.values.size(); ++c) {
            const auto& col = f.columns[c];
            const bool text = std::find(textColumns.begin(), textColumns.end(), static_cast<int>(c)) != textColumns.end();
            if (text) row.values[c] = std::string();
            else if (col.name == "SM_Inx") row.values[c] = std::string("NewMsg_") + std::to_string(id);
        }
        return row;
    };
    DataHistoryBegin(state, std::string(L("Neue Textzeile: ", "New text row: ")) + def.file + " " + std::to_string(id));
    const int created = InsertShnRowWithHistory(state, docs.main, static_cast<int>(mf.rows.size()),
                                                makeRow(mf, mf.rows[static_cast<std::size_t>(src)]));
    if (docs.peer >= 0) {
        auto& pf = ShnDocFile(state, docs.peer);
        if (!pf.rows.empty()) {
            const int psrc = RowForKey(pf, legacy::TextRowKeys(mf)[static_cast<std::size_t>(src)]);
            InsertShnRowWithHistory(state, docs.peer, static_cast<int>(pf.rows.size()),
                                    makeRow(pf, pf.rows[static_cast<std::size_t>(psrc >= 0 ? psrc : static_cast<int>(pf.rows.size()) - 1)]));
        }
    }
    DataHistoryCommit(state);
    return created;
}

bool DeleteTextRow(EditorState& state, const TableDocs& docs, int row) {
    auto& mf = ShnDocFile(state, docs.main);
    if (row < 0 || row >= static_cast<int>(mf.rows.size())) return false;
    const std::string key = legacy::TextRowKeys(mf)[static_cast<std::size_t>(row)];
    DataHistoryBegin(state, std::string(L("Textzeile gelöscht: ", "Text row deleted: ")) + mf.FileName() + " " + key);
    int prow = -1;
    if (docs.peer >= 0) prow = RowForKey(ShnDocFile(state, docs.peer), key);
    bool ok = DeleteShnRowWithHistory(state, docs.main, row);
    if (prow >= 0) ok = DeleteShnRowWithHistory(state, docs.peer, prow) && ok;
    DataHistoryCommit(state);
    return ok;
}

std::filesystem::path DefaultCsvPath(const EditorState& state, const std::string& stem) {
    const std::filesystem::path base = state.project.projectFolder[0] != '\0'
                                           ? std::filesystem::path(state.project.projectFolder) / "Texte"
                                           : std::filesystem::temp_directory_path();
    return base / (stem + ".csv");
}

} // namespace

// CSV aller (oder einer) Texttabellen schreiben; Rückgabe: Anzahl Zeilen oder Fehler.
std::expected<std::size_t, std::string> ExportTextCsv(EditorState& state, const std::filesystem::path& path, int onlyTable) {
    std::vector<legacy::TextCsvRow> rows;
    for (int t = 0; t < kTableCount; ++t) {
        if (onlyTable >= 0 && t != onlyTable) continue;
        const TableDocs docs = DocsFor(state, kTables[t]);
        if (docs.main < 0) continue;
        auto part = legacy::TextCsvRowsFor(ShnDocFile(state, docs.main), kTables[t].file);
        rows.insert(rows.end(), part.begin(), part.end());
    }
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    const std::string csv = legacy::WriteTextCsv(rows);
    out.write(csv.data(), static_cast<std::streamsize>(csv.size()));
    if (!out) return std::unexpected(std::string(L("Schreiben fehlgeschlagen: ", "Write failed: ")) + path.string());
    return rows.size();
}

// CSV einlesen und nur geänderte Zellen übernehmen (Client und Server, ein Undo-Schritt).
std::expected<std::size_t, std::string> ImportTextCsv(EditorState& state, const std::filesystem::path& path,
                                                      std::vector<std::string>& problems) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::unexpected(std::string(L("Datei nicht lesbar: ", "File not readable: ")) + path.string());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const auto rows = legacy::ParseTextCsv(buffer.str());
    if (!rows) return std::unexpected(rows.error());
    std::size_t applied = 0;
    DataHistoryBegin(state, std::string(L("Texte importiert: ", "Texts imported: ")) + path.filename().string());
    for (int t = 0; t < kTableCount; ++t) {
        const bool mentioned = std::any_of(rows->begin(), rows->end(), [&](const auto& r) { return r.table == kTables[t].file; });
        if (!mentioned) continue;
        const TableDocs docs = DocsFor(state, kTables[t]);
        if (docs.main < 0) { problems.push_back(std::string(kTables[t].file) + L(": Tabelle nicht geladen", ": table not loaded")); continue; }
        const auto diff = legacy::DiffTextCsv(ShnDocFile(state, docs.main), kTables[t].file, *rows);
        problems.insert(problems.end(), diff.problems.begin(), diff.problems.end());
        for (const auto& change : diff.changes)
            if (WriteText(state, docs, static_cast<int>(change.row), change.column, change.text)) ++applied;
    }
    for (const auto& r : *rows)
        if (std::none_of(std::begin(kTables), std::end(kTables), [&](const TextTableDef& d) { return r.table == d.file; })) {
            problems.push_back(r.table + L(": keine bekannte Texttabelle", ": not a known text table"));
            break;
        }
    DataHistoryCommit(state);
    return applied;
}

int TextTableIndex(const std::string& file) {
    for (int t = 0; t < kTableCount; ++t) if (LowerAscii(kTables[t].file) == LowerAscii(file)) return t;
    return -1;
}

void DrawTextEditor(EditorState& state) {
    auto& te = state.textEditor;
    te.table = std::clamp(te.table, 0, kTableCount - 1);
    DrawPanelHeader("textEditorHeader", L("TEXTE / LOKALISIERUNG", "TEXTS / LOCALIZATION"), DrawIconBook, "module.shn.single",
                    L("Alle Texttabellen von Client und Server", "All text tables of client and server"));
    if (UI::Button(L("Alle geänderten SHN speichern##texts", "Save all changed SHN##texts")) ||
        (ShortcutPressed(state.shortcutSave) && DirtyShnDocumentCount(state) > 0)) {
        const auto [saved, failed] = SaveAllDirtyShnDocuments(state);
        te.report = {std::string(L("Gespeichert: ", "Saved: ")) + std::to_string(saved) +
                     (failed ? std::string(", ") + L("Fehler: ", "errors: ") + std::to_string(failed) + " · " + state.shnStatus : std::string())};
    }
    ImGui::SameLine();
    DrawDataHistoryButtons(state, "textHistory");
    ImGui::SameLine();
    DrawSearchInput("textSearchAll", L("In allen Texten suchen (ID oder Inhalt)", "Search all texts (ID or content)"),
                    te.search, sizeof(te.search), 320.0f);

    // Suche über alle Tabellen (gecacht über Suchbegriff und Bearbeitungsstand).
    const std::string needle = LowerAscii(te.search);
    const std::string searchKey = needle + "|" + std::to_string(state.shnEditCounter) + "|" + state.shnClientRoot + "|" + state.shnServerRoot;
    if (needle.size() >= 2 && searchKey != te.searchKey) {
        te.searchKey = searchKey;
        te.hits.clear();
        for (int t = 0; t < kTableCount && te.hits.size() < 500; ++t) {
            const TableDocs docs = DocsFor(state, kTables[t]);
            if (docs.main < 0) continue;
            const auto& f = ShnDocFile(state, docs.main);
            const auto keys = legacy::TextRowKeys(f);
            const auto columns = legacy::TextColumns(f);
            for (std::size_t r = 0; r < f.rows.size() && te.hits.size() < 500; ++r) {
                std::string all = keys[r];
                for (const int c : columns) all += " " + legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(c)]);
                if (LowerAscii(all).find(needle) == std::string::npos) continue;
                const std::string first = columns.empty() ? std::string() : legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(columns.front())]);
                te.hits.push_back({t, static_cast<int>(r), keys[r], OneLine(first, 90)});
            }
        }
    } else if (needle.size() < 2) {
        te.hits.clear();
        te.searchKey.clear();
    }
    if (!te.hits.empty()) {
        ImGui::BeginChild("##textHits", ImVec2(0, std::min(180.0f, 24.0f + 20.0f * static_cast<float>(te.hits.size()))), true);
        ImGui::TextDisabled(L("%zu Treffer%s", "%zu hits%s"), te.hits.size(), te.hits.size() >= 500 ? L(" (erste 500)", " (first 500)") : "");
        for (std::size_t h = 0; h < te.hits.size(); ++h) {
            const auto& hit = te.hits[h];
            const std::string label = std::string(kTables[hit.table].file) + "  " + hit.key + "  " + hit.text + "##hit" + std::to_string(h);
            if (ImGui::Selectable(label.c_str(), te.table == hit.table && te.selectedRow == hit.row)) {
                te.table = hit.table;
                te.selectedRow = hit.row;
                te.filter[0] = '\0';
                te.listKey.clear();
                te.scrollToRow = true;
            }
        }
        ImGui::EndChild();
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    // Tabellenliste
    ImGui::BeginChild("##textTables", ImVec2(285.0f, avail.y), true);
    ImGui::TextDisabled("%s", L("Texttabellen", "Text tables"));
    for (int t = 0; t < kTableCount; ++t) {
        const TableDocs docs = DocsFor(state, kTables[t]);
        const char* sides = kTables[t].client && kTables[t].server ? "C+S" : kTables[t].client ? "C" : "S";
        std::string label = std::string(kTables[t].file) + "  [" + sides + "]";
        if (docs.main >= 0) label += "  " + std::to_string(ShnDocFile(state, docs.main).rows.size());
        else label += L("  (fehlt)", "  (missing)");
        if (docs.main >= 0 && state.shnFiles[static_cast<std::size_t>(docs.main)].dirty) label += " *";
        ImGui::BeginDisabled(docs.main < 0);
        if (ImGui::Selectable((label + "##table" + std::to_string(t)).c_str(), te.table == t)) {
            te.table = t;
            te.selectedRow = -1;
            te.listKey.clear();
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    ImGui::TextDisabled("%s", L("CSV (Übersetzung)", "CSV (translation)"));
    if (te.csvPath[0] == '\0')
        std::snprintf(te.csvPath, sizeof(te.csvPath), "%s", DefaultCsvPath(state, "texte").string().c_str());
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputText("##textCsvPath", te.csvPath, sizeof(te.csvPath));
    if (UI::Button(L("Diese Tabelle exportieren##textCsv", "Export this table##textCsv"), ImVec2(-1, 0))) {
        const auto n = ExportTextCsv(state, te.csvPath, te.table);
        te.report = {n ? std::string(L("Exportiert: ", "Exported: ")) + std::to_string(*n) + L(" Textzellen nach ", " text cells to ") + te.csvPath : n.error()};
    }
    if (UI::Button(L("Alle Tabellen exportieren##textCsv", "Export all tables##textCsv"), ImVec2(-1, 0))) {
        const auto n = ExportTextCsv(state, te.csvPath, -1);
        te.report = {n ? std::string(L("Exportiert: ", "Exported: ")) + std::to_string(*n) + L(" Textzellen nach ", " text cells to ") + te.csvPath : n.error()};
    }
    if (UI::Button(L("CSV importieren (nur Änderungen)##textCsv", "Import CSV (changes only)##textCsv"), ImVec2(-1, 0))) {
        std::vector<std::string> problems;
        const auto n = ImportTextCsv(state, te.csvPath, problems);
        te.report = {n ? std::string(L("Übernommen: ", "Applied: ")) + std::to_string(*n) + L(" Zellen (Strg+Z nimmt alles zurück)", " cells (Ctrl+Z reverts all)") : n.error()};
        for (std::size_t p = 0; p < problems.size() && p < 20; ++p) te.report.push_back("! " + problems[p]);
        if (problems.size() > 20) te.report.push_back("! ... +" + std::to_string(problems.size() - 20));
    }
    ImGui::EndChild();
    ImGui::SameLine();

    const TextTableDef& def = kTables[te.table];
    const TableDocs docs = DocsFor(state, def);
    ImGui::BeginChild("##textMain", ImVec2(0, avail.y), false);
    ImGui::TextWrapped("%s", L(def.noteDe, def.noteEn));
    if (docs.main < 0) {
        ImGui::TextColored(UiTheme::Warning, L("%s nicht gefunden - Client-/Server-Ordner im Projekt prüfen.", "%s not found - check the client/server folders of the project."), def.file);
        ImGui::EndChild();
        return;
    }
    if (def.client && def.server && docs.peer < 0 && !state.shnServerRoot.empty())
        ImGui::TextColored(UiTheme::Warning, "%s", L("Server-Kopie nicht gefunden - nur der Client wird bearbeitet.", "Server copy not found - only the client is edited."));
    auto& mf = ShnDocFile(state, docs.main);
    const auto keys = legacy::TextRowKeys(mf);
    const auto textColumns = legacy::TextColumns(mf);
    if (te.selectedRow >= static_cast<int>(mf.rows.size())) te.selectedRow = -1;

    // Zeilenliste
    const float listW = std::clamp(ImGui::GetContentRegionAvail().x * 0.42f, 280.0f, 560.0f);
    ImGui::BeginChild("##textRows", ImVec2(listW, 0), true);
    DrawSearchInput("textRowFilter", L("Filter: Schlüssel oder Text", "Filter: key or text"), te.filter, sizeof(te.filter));
    const std::string rowKey = std::to_string(te.table) + "|" + te.filter + "|" + std::to_string(state.shnEditCounter) + "|" +
                               std::to_string(docs.main) + "|" + std::to_string(mf.rows.size());
    if (rowKey != te.listKey) {
        te.listKey = rowKey;
        te.visible.clear();
        const std::string filter = LowerAscii(te.filter);
        for (std::size_t r = 0; r < mf.rows.size(); ++r) {
            if (!filter.empty()) {
                std::string all = keys[r];
                for (const int c : textColumns) all += " " + legacy::ShnValueToString(mf.rows[r].values[static_cast<std::size_t>(c)]);
                if (LowerAscii(all).find(filter) == std::string::npos) continue;
            }
            te.visible.push_back(static_cast<int>(r));
        }
    }
    ImGui::TextDisabled(L("%zu von %zu Zeilen", "%zu of %zu rows"), te.visible.size(), mf.rows.size());
    ImGui::BeginChild("##textRowList", ImVec2(0, 0), false);
    if (te.scrollToRow) {
        te.scrollToRow = false;
        const auto it = std::find(te.visible.begin(), te.visible.end(), te.selectedRow);
        if (it != te.visible.end())
            ImGui::SetScrollY(static_cast<float>(it - te.visible.begin()) * ImGui::GetTextLineHeightWithSpacing());
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(te.visible.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const int r = te.visible[static_cast<std::size_t>(i)];
            const std::string first = textColumns.empty() ? std::string()
                : legacy::ShnValueToString(mf.rows[static_cast<std::size_t>(r)].values[static_cast<std::size_t>(textColumns.front())]);
            const std::string label = keys[static_cast<std::size_t>(r)] + "  " + OneLine(first, 70) + "##row" + std::to_string(r);
            if (ImGui::Selectable(label.c_str(), te.selectedRow == r)) te.selectedRow = r;
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::SameLine();

    // Bearbeitung der gewählten Zeile
    ImGui::BeginChild("##textEdit", ImVec2(0, 0), true);
    if (NumericKey(mf)) {
        ImGui::TextDisabled(L("Nächste freie ID: %lld", "Next free ID: %lld"), NextFreeKey(state, docs));
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(!def.canAdd || !NumericKey(mf));
    if (UI::Button(L("Neue Zeile##texts", "New row##texts"))) {
        const int created = AddTextRow(state, def, docs, te.selectedRow);
        if (created >= 0) { te.selectedRow = created; te.filter[0] = '\0'; te.listKey.clear(); }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(te.selectedRow < 0);
    if (UI::Button(L("Zeile löschen##texts", "Delete row##texts"))) ImGui::OpenPopup("##textDeleteConfirm");
    ImGui::EndDisabled();
    if (ImGui::BeginPopup("##textDeleteConfirm")) {
        ImGui::TextUnformatted(L("Diese Zeile löschen (Client und Server)?", "Delete this row (client and server)?"));
        const auto uses = te.selectedRow >= 0 ? UsesOf(state, def, mf, te.selectedRow) : std::vector<std::string>{};
        if (!uses.empty()) {
            ImGui::TextColored(UiTheme::Warning, L("Noch verwendet von %zu Stelle(n):", "Still used by %zu place(s):"), uses.size());
            for (std::size_t u = 0; u < uses.size() && u < 12; ++u) ImGui::BulletText("%s", uses[u].c_str());
        }
        if (UI::Button(L("Löschen##textDelete", "Delete##textDelete"))) {
            DeleteTextRow(state, docs, te.selectedRow);
            te.selectedRow = -1;
            te.listKey.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (UI::Button(L("Abbrechen##textDelete", "Cancel##textDelete"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::Separator();

    if (te.selectedRow < 0) {
        ImGui::TextDisabled("%s", L("Links eine Zeile wählen oder oben suchen.", "Pick a row on the left or search above."));
    } else {
        const int row = te.selectedRow;
        ImGui::Text("%s  %s", mf.columns.front().name.c_str(), keys[static_cast<std::size_t>(row)].c_str());
        const float width = ImGui::GetContentRegionAvail().x;
        for (const int c : textColumns) {
            const auto& col = mf.columns[static_cast<std::size_t>(c)];
            const std::string current = legacy::ShnValueToString(mf.rows[static_cast<std::size_t>(row)].values[static_cast<std::size_t>(c)]);
            if (col.type == 26) ImGui::Text("%s  (%zu Byte)", col.name.c_str(), current.size());
            else ImGui::Text("%s  (%zu / %u Byte)", col.name.c_str(), current.size(), col.length - 1);
            ImGui::PushID(c);
            std::string edited;
            if (DeferredInputText("##text", current, edited, width, 110.0f)) WriteText(state, docs, row, c, edited);
            ImGui::PopID();
        }
        // Übrige Spalten (Inx, Farbe, Dauer ...) einzeilig.
        bool header = false;
        for (std::size_t c = 1; c < mf.columns.size(); ++c) {
            if (std::find(textColumns.begin(), textColumns.end(), static_cast<int>(c)) != textColumns.end()) continue;
            if (!header) { ImGui::SeparatorText(L("Weitere Spalten", "Other columns")); header = true; }
            const auto& col = mf.columns[c];
            const std::string current = legacy::ShnValueToString(mf.rows[static_cast<std::size_t>(row)].values[c]);
            ImGui::PushID(static_cast<int>(1000 + c));
            ImGui::TextUnformatted(col.name.c_str());
            ImGui::SameLine(160.0f);
            std::string edited;
            if (DeferredInputText("##other", current, edited, std::max(120.0f, width - 160.0f))) {
                std::string why;
                if (!FitsColumn(col, edited, why)) te.report = {why};
                else WriteText(state, docs, row, static_cast<int>(c), col.kind == legacy::ShnValueKind::String ? edited : TrimAscii(edited));
            }
            ImGui::PopID();
        }
        const auto uses = UsesOf(state, def, mf, row);
        if (def.use != TextUse::None) {
            ImGui::SeparatorText(L("Verwendet von", "Used by"));
            if (uses.empty()) ImGui::TextDisabled("%s", L("Keine Verwendung gefunden.", "No usage found."));
            for (const auto& use : uses) ImGui::BulletText("%s", use.c_str());
        }
    }
    if (!te.report.empty()) {
        ImGui::Separator();
        for (const auto& line : te.report) ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

} // namespace nge
