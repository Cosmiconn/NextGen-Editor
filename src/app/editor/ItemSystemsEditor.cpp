// ItemSystemsEditor.cpp
// Allgemeiner Gruppen-Editor (siehe GroupSystems.hpp) und die Kategorie "Item-Systeme" (Datei Editor,
// Unterreiter 16): Sets, Set-Effekte, Aufwertung, Aufwertungs-Chancen, Item-Mix, Zufallsoptionen und
// Item-Pakete. Diese Tabellen sind gruppiert - mehrere Zeilen mit demselben Schlüssel bilden einen
// Eintrag (core::legacy::ShnGroups). Je System stehen die Gruppen links und alle Zeilen der gewählten
// Gruppe in allen beteiligten Tabellen rechts; Client- und Server-Kopie werden gemeinsam geändert
// (n-te Zeile der Gruppe), alles mit Strg+Z über den Daten-Verlauf. Weitere Kategorien (Reittiere &
// Haustiere ...) benutzen dieselbe Oberfläche.

#include "GroupSystems.hpp"

#include "mapeditor/core/legacy/ShnGroups.hpp"

namespace nge::groups {
namespace {

namespace legacy = core::legacy;

using ItemSystemsState = EditorState::ItemSystemsState;

// Schlüssel der einzigen Gruppe eines Systems ohne Schlüsselspalte.
const std::string kAllRows = "*";

struct Copies { int client = -1; int server = -1; };

int SystemDoc(EditorState& state, ItemSystemsState& is, const char* file, Source side) {
    const std::string& root = side == Source::Client ? state.shnClientRoot : state.shnServerRoot;
    if (root.empty()) return -1;
    const std::string key = root + "|" + file;
    if (is.missing.contains(key)) return -1;
    const int doc = SharedShnDoc(state, file, side, root);
    if (doc < 0) is.missing.insert(key);
    return doc;
}

Copies CopiesOf(EditorState& state, ItemSystemsState& is, const GroupTable& t) {
    Copies c;
    if (t.client) c.client = SystemDoc(state, is, t.file, Source::Client);
    if (t.server) c.server = SystemDoc(state, is, t.file, Source::Server);
    return c;
}
int MainOf(const Copies& c) { return c.client >= 0 ? c.client : c.server; }

bool Global(const System& sys) {
    return std::none_of(sys.tables.begin(), sys.tables.end(), [](const GroupTable& t) { return t.keyColumn != nullptr; });
}

// Zeilen einer Gruppe; Tabellen ohne Schlüssel liefern alle Zeilen.
std::vector<std::size_t> RowsOf(const legacy::ShnFile& f, const GroupTable& t, const std::string& key) {
    if (!t.keyColumn) {
        std::vector<std::size_t> all(f.rows.size());
        for (std::size_t r = 0; r < all.size(); ++r) all[r] = r;
        return all;
    }
    return legacy::ShnGroupRows(f, ShnColumnIndexByName(f, t.keyColumn), key);
}
std::size_t InsertIndexOf(const legacy::ShnFile& f, const GroupTable& t, const std::string& key) {
    if (!t.keyColumn) return f.rows.size();
    return legacy::ShnGroupInsertIndex(f, ShnColumnIndexByName(f, t.keyColumn), key);
}

// Nachschlagetabelle Schlüssel -> Wert, gecacht über den Bearbeitungsstand.
const std::unordered_map<std::string, std::string>& LookupMap(EditorState& state, ItemSystemsState& is, const char* file, Source side,
                                                              const char* keyColumn, const char* valueColumn) {
    struct Cached { std::uint64_t edit = ~0ull; std::unordered_map<std::string, std::string> map; };
    static std::unordered_map<std::string, Cached> cache;
    auto& entry = cache[std::string(file) + "|" + keyColumn + "|" + valueColumn + "|" +
                        (side == Source::Client ? state.shnClientRoot : state.shnServerRoot) + "|" +
                        (app::CurrentLanguage() == app::Language::German ? "de" : "en")];
    if (entry.edit == state.shnEditCounter) return entry.map;
    entry.edit = state.shnEditCounter;
    entry.map.clear();
    const int doc = SystemDoc(state, is, file, side);
    if (doc < 0) return entry.map;
    const auto& f = ShnDocFile(state, doc);
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        entry.map.emplace(TrimAscii(ShnCellText(f, r, keyColumn)), DisplayText(ShnCellText(f, r, valueColumn)));
    return entry.map;
}

std::string NameOf(EditorState& state, ItemSystemsState& is, const NameSource& src, const std::string& key) {
    if (!src.file) return {};
    const auto& map = LookupMap(state, is, src.file, src.side, src.keyColumn, src.valueColumn);
    const auto it = map.find(key);
    return it == map.end() ? std::string() : it->second;
}

bool EmptyRef(const std::string& v) { return v.empty() || v == "-" || v == "0"; }

// Lädt die Nachschlagetabellen einer Tabelle vorab: Laden vergrößert state.shnFiles, danach gehaltene
// ShnFile-Referenzen bleiben so gültig.
void WarmLookups(EditorState& state, ItemSystemsState& is, const GroupTable& t) {
    for (const auto& lk : t.lookups) (void)LookupMap(state, is, lk.file, lk.side, lk.keyColumn, lk.valueColumn);
}

// Gruppen eines Systems: Schlüssel aller beteiligten Tabellen (Reihenfolge: erste Tabelle zuerst).
std::vector<std::string> SystemKeys(EditorState& state, ItemSystemsState& is, const System& sys) {
    if (Global(sys)) return {kAllRows};
    std::vector<std::string> keys;
    std::unordered_set<std::string> seen;
    for (const auto& t : sys.tables) {
        if (!t.keyColumn) continue;
        const int doc = MainOf(CopiesOf(state, is, t));
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        for (const auto& g : legacy::GroupShnRows(f, ShnColumnIndexByName(f, t.keyColumn)))
            if (seen.insert(g.key).second) keys.push_back(g.key);
    }
    return keys;
}

// Probleme einer Gruppe: Verweise, deren Ziel fehlt.
std::vector<std::string> GroupProblems(EditorState& state, ItemSystemsState& is, const System& sys, const std::string& key) {
    std::vector<std::string> out;
    for (const auto& t : sys.tables) {
        WarmLookups(state, is, t);
        const int doc = MainOf(CopiesOf(state, is, t));
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        if (t.distinctColumn && t.keyColumn) {
            const int keyCol = ShnColumnIndexByName(f, t.keyColumn);
            for (const auto row : RowsOf(f, t, key)) {
                const std::string v = TrimAscii(ShnCellText(f, row, t.distinctColumn));
                if (EmptyRef(v)) continue;
                for (std::size_t r = 0; r < f.rows.size(); ++r)
                    if (TrimAscii(ShnCellText(f, r, t.distinctColumn)) == v &&
                        TrimAscii(legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(keyCol)])) != key) {
                        out.push_back(std::string(t.file) + " " + t.distinctColumn + ": " + v + L(" wird auch von ", " is also used by ") +
                                      TrimAscii(legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(keyCol)])) + L(" benutzt", ""));
                        break;
                    }
            }
        }
        for (const auto row : RowsOf(f, t, key))
            for (const auto& lk : t.lookups) {
                if (!lk.required) continue;
                const std::string v = TrimAscii(ShnCellText(f, row, lk.column));
                if (EmptyRef(v)) continue;
                const auto& map = LookupMap(state, is, lk.file, lk.side, lk.keyColumn, lk.valueColumn);
                if (!map.empty() && !map.contains(v)) out.push_back(std::string(t.file) + " " + lk.column + ": " + v + L(" unbekannt", " unknown"));
            }
    }
    return out;
}

// Zeilenvorlage: letzte Zeile der Gruppe, sonst letzte Zeile der Datei, Schlüssel ersetzt.
std::optional<legacy::ShnRow> TemplateRow(const legacy::ShnFile& f, const GroupTable& t, const std::string& key, const std::string& newKey) {
    if (f.rows.empty()) return std::nullopt;
    const auto rows = RowsOf(f, t, key);
    legacy::ShnRow row = f.rows[rows.empty() ? f.rows.size() - 1 : rows.back()];
    if (!t.keyColumn) return row;
    const int keyCol = ShnColumnIndexByName(f, t.keyColumn);
    if (keyCol < 0) return std::nullopt;
    auto v = legacy::ParseShnValue(f.columns[static_cast<std::size_t>(keyCol)], newKey);
    if (!v) return std::nullopt;
    row.values[static_cast<std::size_t>(keyCol)] = *v;
    return row;
}

// Freie Werte der eindeutigen Spalte über beide Kopien (count Stück, aufsteigend).
std::vector<std::string> FreeUniqueValues(EditorState& state, const GroupTable& t, const Copies& c, std::size_t count) {
    std::vector<std::string> out;
    if (!t.uniqueColumn || count == 0) return out;
    std::set<long long> used;
    const legacy::ShnColumn* column = nullptr;
    for (const int doc : {c.client, c.server}) {
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        const int col = ShnColumnIndexByName(f, t.uniqueColumn);
        if (col < 0) continue;
        CollectShnInts(f, col, used);
        if (!column) column = &f.columns[static_cast<std::size_t>(col)];
    }
    if (!column) return out;
    for (std::size_t i = 0; i < count; ++i) {
        const long long v = SuggestFreeShnId(*column, used);
        if (v < 0) break;
        used.insert(v);
        out.push_back(std::to_string(v));
    }
    return out;
}

void SetUnique(const legacy::ShnFile& f, const GroupTable& t, legacy::ShnRow& row, const std::string& value) {
    if (!t.uniqueColumn || value.empty()) return;
    const int col = ShnColumnIndexByName(f, t.uniqueColumn);
    if (col < 0) return;
    if (auto v = legacy::ParseShnValue(f.columns[static_cast<std::size_t>(col)], value)) row.values[static_cast<std::size_t>(col)] = *v;
}

// Ändert eine Zelle der n-ten Gruppenzeile in beiden Kopien.
void WriteGroupCell(EditorState& state, ItemSystemsState& is, const GroupTable& t, const Copies& c, const std::string& key,
                    std::size_t nth, const std::string& column, const std::string& text) {
    DataHistoryBegin(state, std::string(L("Geändert: ", "Changed: ")) + t.file + " " + key + " · " + column);
    for (const int doc : {c.client, c.server}) {
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        const auto rows = RowsOf(f, t, key);
        const int col = ShnColumnIndexByName(f, column.c_str());
        if (nth < rows.size() && col >= 0)
            if (!ApplyShnCellText(state, doc, static_cast<int>(rows[nth]), col, text))
                is.report = {std::string(t.file) + ": " + state.shnStatus};
    }
    DataHistoryCommit(state);
}

void AddGroupRow(EditorState& state, const GroupTable& t, const Copies& c, const std::string& key) {
    DataHistoryBegin(state, std::string(L("Zeile hinzugefügt: ", "Row added: ")) + t.file + " " + key);
    const auto unique = FreeUniqueValues(state, t, c, 1);
    for (const int doc : {c.client, c.server}) {
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        if (auto row = TemplateRow(f, t, key, key)) {
            if (!unique.empty()) SetUnique(f, t, *row, unique.front());
            InsertShnRowWithHistory(state, doc, static_cast<int>(InsertIndexOf(f, t, key)), std::move(*row));
        }
    }
    DataHistoryCommit(state);
}

void DeleteGroupRow(EditorState& state, const GroupTable& t, const Copies& c, const std::string& key, std::size_t nth) {
    DataHistoryBegin(state, std::string(L("Zeile gelöscht: ", "Row deleted: ")) + t.file + " " + key);
    for (const int doc : {c.client, c.server}) {
        if (doc < 0) continue;
        const auto rows = RowsOf(ShnDocFile(state, doc), t, key);
        if (nth < rows.size()) DeleteShnRowWithHistory(state, doc, static_cast<int>(rows[nth]));
    }
    DataHistoryCommit(state);
}

// Neue Gruppe als Kopie der gewählten (in allen Tabellen und Kopien), bzw. Umbenennen/Löschen.
bool CopyGroup(EditorState& state, ItemSystemsState& is, const System& sys, const std::string& from, const std::string& to) {
    if (to.empty() || Global(sys)) return false;
    for (const auto& k : SystemKeys(state, is, sys)) if (k == to) { is.report = {to + L(" existiert bereits.", " already exists.")}; return false; }
    DataHistoryBegin(state, std::string(L("Neu: ", "New: ")) + to + L(" (Kopie von ", " (copy of ") + from + ")");
    for (const auto& t : sys.tables) {
        if (!t.keyColumn) continue;
        const Copies c = CopiesOf(state, is, t);
        const int main = MainOf(c);
        if (main < 0) continue;
        const std::size_t count = RowsOf(ShnDocFile(state, main), t, from).size();
        const auto unique = FreeUniqueValues(state, t, c, count); // gleiche neue Nummern in Client und Server
        for (const int doc : {c.client, c.server}) {
            if (doc < 0) continue;
            const auto& f = ShnDocFile(state, doc);
            const int keyCol = ShnColumnIndexByName(f, t.keyColumn);
            std::vector<legacy::ShnRow> copies;
            for (const auto r : RowsOf(f, t, from)) {
                legacy::ShnRow row = f.rows[r];
                if (auto v = legacy::ParseShnValue(f.columns[static_cast<std::size_t>(keyCol)], to)) row.values[static_cast<std::size_t>(keyCol)] = *v;
                copies.push_back(std::move(row));
            }
            // Tabellen ohne Zeile der Vorlage bleiben ohne Zeile (z. B. Möbel ohne Wirkung).
            for (std::size_t n = 0; n < copies.size(); ++n) {
                if (n < unique.size()) SetUnique(f, t, copies[n], unique[n]);
                InsertShnRowWithHistory(state, doc, static_cast<int>(ShnDocFile(state, doc).rows.size()), std::move(copies[n]));
            }
        }
    }
    DataHistoryCommit(state);
    return true;
}

void RenameGroup(EditorState& state, ItemSystemsState& is, const System& sys, const std::string& from, const std::string& to) {
    if (to.empty() || to == from || Global(sys)) return;
    for (const auto& k : SystemKeys(state, is, sys)) if (k == to) { is.report = {to + L(" existiert bereits.", " already exists.")}; return; }
    DataHistoryBegin(state, std::string(L("Umbenannt: ", "Renamed: ")) + from + " -> " + to);
    for (const auto& t : sys.tables) {
        if (!t.keyColumn) continue;
        const Copies c = CopiesOf(state, is, t);
        for (const int doc : {c.client, c.server}) {
            if (doc < 0) continue;
            const auto& f = ShnDocFile(state, doc);
            const int keyCol = ShnColumnIndexByName(f, t.keyColumn);
            for (const auto r : RowsOf(f, t, from)) ApplyShnCellText(state, doc, static_cast<int>(r), keyCol, to);
        }
    }
    DataHistoryCommit(state);
}

void DeleteGroup(EditorState& state, ItemSystemsState& is, const System& sys, const std::string& key) {
    if (Global(sys)) return;
    DataHistoryBegin(state, std::string(L("Gelöscht: ", "Deleted: ")) + key);
    for (const auto& t : sys.tables) {
        if (!t.keyColumn) continue;
        const Copies c = CopiesOf(state, is, t);
        for (const int doc : {c.client, c.server}) {
            if (doc < 0) continue;
            const auto rows = RowsOf(ShnDocFile(state, doc), t, key);
            for (auto it = rows.rbegin(); it != rows.rend(); ++it) DeleteShnRowWithHistory(state, doc, static_cast<int>(*it));
        }
    }
    DataHistoryCommit(state);
}

struct LinkedRow { std::string label; std::string target; };

std::vector<LinkedRow> LinkedRows(EditorState& state, ItemSystemsState& is, const Link& link, const std::string& key) {
    std::vector<LinkedRow> out;
    if (!link.file) return out;
    const auto& names = LookupMap(state, is, "ItemInfo.shn", Source::Client, "InxName", "Name");
    const int doc = SystemDoc(state, is, link.file, link.side);
    if (doc < 0) return out;
    const auto& f = ShnDocFile(state, doc);
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        if (TrimAscii(ShnCellText(f, r, link.column)) != key) continue;
        const std::string inx = TrimAscii(ShnCellText(f, r, link.resultKey));
        std::string name;
        if (link.resultName) name = DisplayText(ShnCellText(f, r, link.resultName));
        else if (const auto it = names.find(inx); it != names.end()) name = it->second;
        if (link.jumpSystem) out.push_back({inx + (name.empty() ? std::string() : "  ·  " + name), inx});
        else out.push_back({ShnCellText(f, r, "ID") + "  " + inx + (name.empty() ? std::string() : "  ·  " + name), ShnCellText(f, r, "ID")});
    }
    return out;
}

void DrawPortrait(EditorState& state, ItemSystemsState& is, const PortraitSource& src, const std::string& key) {
    if (!src.name.file || !src.folder) return;
    const std::string name = TrimAscii(NameOf(state, is, src.name, key));
    if (EmptyRef(name)) return;
    std::string rel;
    for (const char* ext : {".png", ".tga", ".PNG", ".TGA", ".dds"}) {
        std::error_code ec;
        const std::string candidate = std::string(src.folder) + "/" + name + ext;
        if (std::filesystem::exists(ResmenuEffectivePath(state, candidate), ec)) { rel = candidate; break; }
    }
    if (rel.empty()) {
        ImGui::TextColored(UiTheme::Warning, L("Bild %s/%s nicht gefunden", "Image %s/%s not found"), src.folder, name.c_str());
        return;
    }
    float aspect = 1.0f;
    const std::uint32_t tex = ResmenuPreviewTexture(state, rel, &aspect);
    if (!tex) return;
    const float side = 72.0f;
    const ImVec2 size = aspect > 1.0f ? ImVec2(side, side / aspect) : ImVec2(side * aspect, side);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    DrawCheckerboard(p, ImVec2(p.x + size.x, p.y + size.y));
    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), size);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("resmenu/%s", rel.c_str());
}

// Tabelle der Gruppenzeilen. Viele Spalten (z. B. KingdomQuestRew mit 33) bekommen feste Breiten und
// einen horizontalen Rollbalken, statt bis zur Unlesbarkeit gestaucht zu werden.
bool BeginGroupTable(const legacy::ShnFile& f, const std::vector<int>& cols, std::size_t rowCount) {
    const bool wide = cols.size() > 10;
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
    ImVec2 outer(0.0f, 0.0f);
    if (wide) {
        flags |= ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit;
        const float rowH = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing();
        outer.y = std::min(420.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f + rowH * static_cast<float>(rowCount) + ImGui::GetStyle().ScrollbarSize);
    } else {
        flags |= ImGuiTableFlags_SizingStretchProp;
    }
    if (!ImGui::BeginTable("##groupRows", static_cast<int>(cols.size()) + 1, flags, outer)) return false;
    for (const int col : cols) {
        const auto& column = f.columns[static_cast<std::size_t>(col)];
        const auto& name = column.name;
        // Texte nach Feldlänge breiter (Titel, Skriptpfade), Zahlen nach der Kopfzeile.
        const float textWidth = column.kind == legacy::ShnValueKind::String
            ? std::clamp(static_cast<float>(column.length) * 6.0f, 90.0f, 240.0f) : 0.0f;
        if (wide) ImGui::TableSetupColumn(name.c_str(), ImGuiTableColumnFlags_WidthFixed,
                                          std::max({70.0f, textWidth, ImGui::CalcTextSize(name.c_str()).x + 12.0f}));
        else ImGui::TableSetupColumn(name.c_str());
    }
    ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 26.0f);
    if (wide) ImGui::TableSetupScrollFreeze(0, 1);
    return true;
}

void DrawGroupTable(EditorState& state, ItemSystemsState& is, const GroupTable& t, const std::string& key) {
    WarmLookups(state, is, t);
    const Copies c = CopiesOf(state, is, t);
    const int main = MainOf(c);
    const char* sides = t.client && t.server ? "C+S" : t.client ? "C" : "S";
    ImGui::SeparatorText((std::string(L(t.titleDe, t.titleEn)) + "  [" + sides + "]").c_str());
    if (main < 0) {
        ImGui::TextColored(UiTheme::Warning, L("%s nicht gefunden.", "%s not found."), t.file);
        return;
    }
    if (t.client && t.server && (c.client < 0 || c.server < 0))
        ImGui::TextColored(UiTheme::Warning, "%s", L("Nur eine Kopie gefunden - es wird nur diese bearbeitet.", "Only one copy found - only that one is edited."));
    const auto& f = ShnDocFile(state, main);
    const int keyCol = t.keyColumn ? ShnColumnIndexByName(f, t.keyColumn) : -1;
    const auto rows = RowsOf(f, t, key);
    std::vector<int> cols;
    for (std::size_t col = 0; col < f.columns.size(); ++col) if (static_cast<int>(col) != keyCol) cols.push_back(static_cast<int>(col));
    std::optional<std::size_t> remove;
    ImGui::PushID(t.file);
    if (!rows.empty() &&
        BeginGroupTable(f, cols, rows.size())) {
        ImGui::TableHeadersRow();
        for (std::size_t n = 0; n < rows.size(); ++n) {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(n));
            for (const int col : cols) {
                ImGui::TableNextColumn();
                const auto& colDef = f.columns[static_cast<std::size_t>(col)];
                const std::string current = ShnCellText(f, rows[n], colDef.name.c_str());
                ImGui::PushID(col);
                std::string edited;
                if (DeferredInputText("##c", current, edited, -1.0f)) WriteGroupCell(state, is, t, c, key, n, colDef.name, edited);
                if (const std::string shown = DisplayText(current); shown != current) ImGui::TextDisabled("%s", shown.c_str());
                for (const auto& lk : t.lookups) {
                    if (colDef.name != lk.column) continue;
                    const std::string v = TrimAscii(current);
                    if (EmptyRef(v)) continue;
                    const auto& map = LookupMap(state, is, lk.file, lk.side, lk.keyColumn, lk.valueColumn);
                    if (const auto it = map.find(v); it != map.end()) { if (it->second != v) ImGui::TextDisabled("%s", it->second.c_str()); }
                    else if (lk.required && !map.empty()) ImGui::TextColored(UiTheme::Warning, "%s", L("unbekannt", "unknown"));
                }
                ImGui::PopID();
            }
            ImGui::TableNextColumn();
            if (UI::SmallButton("X##groupRow")) remove = n;
            ImGui::PopID();
        }
        ImGui::EndTable();
    } else if (rows.empty()) {
        ImGui::TextDisabled("%s", L("Keine Zeilen in dieser Tabelle.", "No rows in this table."));
    }
    if (UI::SmallButton(L("+ Zeile##groupRow", "+ Row##groupRow"))) AddGroupRow(state, t, c, key);
    ImGui::PopID();
    if (remove) DeleteGroupRow(state, t, c, key, *remove);
}

const System& CurrentSystem(const Category& cat, ItemSystemsState& is) {
    is.system = std::clamp(is.system, 0, static_cast<int>(cat.systems.size()) - 1);
    return cat.systems[static_cast<std::size_t>(is.system)];
}

} // namespace

const Category& ItemCategory() {
    static const std::vector<System> systems = {
        {"sets", "Sets", "Sets",
         "Ein Set besteht aus SetItem (Server: Teilezahl -> Effektname), SetEffect (Client und Server: Teilezahl -> Item-Aktion) "
         "und SetItemName (Client: Anzeigename). Items gehören über ItemInfo.SetItemIndex zum Set.",
         "A set consists of SetItem (server: piece count -> effect name), SetEffect (client and server: piece count -> item action) "
         "and SetItemName (client: display name). Items belong to the set via ItemInfo.SetItemIndex.",
         {{"SetItem.shn", false, true, "Index", "Effekte je Teilezahl (SetItem)", "Effects per piece count (SetItem)",
           {{"Effect", "SetItemEffect.shn", Source::Server, "Effect", "Desc", true}}},
          {"SetEffect.shn", true, true, "SetItemIndex", "Item-Aktionen je Teilezahl (SetEffect)", "Item actions per piece count (SetEffect)",
           {{"ItemActionID", "ItemActionEffectDesc.shn", Source::Client, "ItemActionID", "ItemActionDesc", false}}},
          {"SetItemName.shn", true, false, "SetItemIndex", "Anzeigename (SetItemName)", "Display name (SetItemName)", {}}},
         {"ItemInfo.shn", Source::Client, "SetItemIndex", "Items dieses Sets (ItemInfo.SetItemIndex)", "Items of this set (ItemInfo.SetItemIndex)"}},
        {"seteffects", "Set-Effekte", "Set effects",
         "Definition der Set-Effekte (SetItemEffect, Server): Beschreibung, Skill-Gruppe, Stufenbereich, Index und Argument. "
         "SetItem.Effect verweist hierher.",
         "Definition of set effects (SetItemEffect, server): description, skill group, level range, index and argument. "
         "SetItem.Effect points here.",
         {{"SetItemEffect.shn", false, true, "Effect", "Set-Effekt (SetItemEffect)", "Set effect (SetItemEffect)", {}}},
         {}},
        {"upgrade", "Aufwertung", "Upgrade",
         "UpgradeInfo (Client und Server): Werte je Aufwertungsstufe einer Gruppe (Updata und die folgenden Spalten = Stufe 1, 2, ...). "
         "Items verweisen vermutlich über ItemInfo.BasicUpInx auf die ID der Gruppe (vermutet).",
         "UpgradeInfo (client and server): values per upgrade level of a group (Updata and the following columns = level 1, 2, ...). "
         "Items probably reference the group ID via ItemInfo.BasicUpInx (assumed).",
         {{"UpgradeInfo.shn", true, true, "InxName", "Werte je Stufe (UpgradeInfo)", "Values per level (UpgradeInfo)", {}}},
         {}},
        {"upgradechance", "Aufwertungs-Chancen", "Upgrade chances",
         "ItemUpgrade (Server): je ID eine Zeile pro Aufwertungsstufe (vermutet) mit den Wahrscheinlichkeiten für kritischen Fehlschlag, "
         "Abstufung, normalen Fehlschlag sowie nCon und LuckySuc.",
         "ItemUpgrade (server): per ID one row per upgrade level (assumed) with the chances for critical failure, downgrade, "
         "normal failure as well as nCon and LuckySuc.",
         {{"ItemUpgrade.shn", false, true, "ID", "Stufen (ItemUpgrade)", "Levels (ItemUpgrade)", {}}},
         {}},
        {"mix", "Item-Mix", "Item mix",
         "ItemMix (Client und Server): Ausgangs-Item -> Ergebnis-Item.", "ItemMix (client and server): source item -> result item.",
         {{"ItemMix.shn", true, true, "Raw_ItemIDX", "Ergebnis (ItemMix)", "Result (ItemMix)", {ItemName("Mix_ItemIDX")}}},
         {}},
        {"random", "Zufallsoptionen", "Random options",
         "RandomOption (Server): mögliche Zusatzwerte einer Drop-Gruppe (Typ, Min, Max, Rate). RandomOptionCount (Server): wie viele "
         "Optionen mit welcher Rate. Items verweisen über ItemInfoServer.RandomOptionDropGroup.",
         "RandomOption (server): possible extra stats of a drop group (type, min, max, rate). RandomOptionCount (server): how many "
         "options at which rate. Items reference it via ItemInfoServer.RandomOptionDropGroup.",
         {{"RandomOption.shn", false, true, "DropItemIndex", "Optionen (RandomOption)", "Options (RandomOption)", {}},
          {"RandomOptionCount.shn", false, true, "InxName", "Anzahl (RandomOptionCount)", "Count (RandomOptionCount)", {}}},
         {"ItemInfoServer.shn", Source::Server, "RandomOptionDropGroup", "Items mit dieser Gruppe (ItemInfoServer)", "Items with this group (ItemInfoServer)"}},
        {"package", "Item-Pakete", "Item packages",
         "ItemPackage (Server): ein Paket-Item enthält Number Stück des Items Content.",
         "ItemPackage (server): a package item contains Number pieces of the item Content.",
         {{"ItemPackage.shn", false, true, "ItemID", "Inhalt (ItemPackage)", "Content (ItemPackage)", {ItemName("Content")}}},
         {}},
    };
    static const Category category{"items", "ITEM-SYSTEME", "ITEM SYSTEMS",
                                   "Sets, Aufwertung, Mix, Zufallsoptionen, Pakete", "Sets, upgrade, mix, random options, packages",
                                   DrawIconGem, "module.prices", 16, systems, &EditorState::itemSystems};
    return category;
}

std::string GroupSystemsAutomation(EditorState& state, const Category& cat, const std::string& cmd, std::istream& in) {
    auto& is = state.*cat.state;
    const System& current = CurrentSystem(cat, is);
    const auto table = [&](const std::string& file) -> const GroupTable* {
        for (const auto& t : current.tables) if (file == t.file) return &t;
        return nullptr;
    };
    if (cmd == "open") {
        std::string id, key; in >> id; std::getline(in >> std::ws, key);
        state.screen = AppScreen::ShnEditor;
        state.shnSubTab = cat.subTab;
        for (std::size_t s = 0; s < cat.systems.size(); ++s) if (id == cat.systems[s].id) is.system = static_cast<int>(s);
        const System& sys = CurrentSystem(cat, is);
        is.selected = Global(sys) ? kAllRows : key;
        is.listKey.clear();
        return std::string(sys.id) + " " + is.selected + " problems=" + std::to_string(GroupProblems(state, is, sys, is.selected).size()) +
               " keys=" + std::to_string(SystemKeys(state, is, sys).size());
    }
    if (cmd == "copy") {
        std::string to; std::getline(in >> std::ws, to);
        const bool ok = CopyGroup(state, is, current, is.selected, to);
        if (ok) is.selected = to;
        return std::string("copy ") + (ok ? "ok" : "failed");
    }
    if (cmd == "set") {
        std::string file, column, value; std::size_t nth = 0;
        in >> file >> nth >> column; std::getline(in >> std::ws, value);
        const GroupTable* t = table(file);
        if (!t) return "unknown table " + file;
        WriteGroupCell(state, is, *t, CopiesOf(state, is, *t), is.selected, nth, column, value);
        return "set " + file + " " + column + "=" + value;
    }
    if (cmd == "row") {
        std::string file; in >> file;
        const GroupTable* t = table(file);
        if (!t) return "unknown table " + file;
        AddGroupRow(state, *t, CopiesOf(state, is, *t), is.selected);
        return "row added " + file;
    }
    if (cmd == "delete") {
        DeleteGroup(state, is, current, is.selected);
        const std::string key = is.selected;
        is.selected.clear();
        return "deleted " + key;
    }
    return "unknown command";
}

void DrawGroupSystemsEditor(EditorState& state, const Category& cat) {
    auto& is = state.*cat.state;
    const std::string id = cat.id;
    const System& sys = CurrentSystem(cat, is);
    DrawPanelHeader((id + "GroupHeader").c_str(), L(cat.titleDe, cat.titleEn), cat.icon, cat.semanticIcon, L(cat.subtitleDe, cat.subtitleEn));
    if (UI::Button(L("Alle geänderten SHN speichern##itemSystems", "Save all changed SHN##itemSystems")) ||
        (ShortcutPressed(state.shortcutSave) && DirtyShnDocumentCount(state) > 0)) {
        const auto [saved, failed] = SaveAllDirtyShnDocuments(state);
        is.report = {std::string(L("Gespeichert: ", "Saved: ")) + std::to_string(saved) +
                     (failed ? std::string(", ") + L("Fehler: ", "errors: ") + std::to_string(failed) + " · " + state.shnStatus : std::string())};
    }
    ImGui::SameLine();
    DrawDataHistoryButtons(state, (id + "History").c_str());
    for (int s = 0; s < static_cast<int>(cat.systems.size()); ++s) {
        if (s > 0) ImGui::SameLine();
        const auto& candidate = cat.systems[static_cast<std::size_t>(s)];
        if (SceneQuickFilterButton((id + "System" + candidate.id).c_str(), L(candidate.nameDe, candidate.nameEn), is.system == s)) {
            is.system = s;
            is.selected.clear();
            is.listKey.clear();
        }
    }
    ImGui::TextWrapped("%s", L(sys.noteDe, sys.noteEn));
    const bool global = Global(sys);

    // Gruppenliste (gecacht über Filter, Bearbeitungsstand und System).
    const std::string listKey = std::to_string(is.system) + "|" + is.filter + "|" + std::to_string(is.onlyProblems) + "|" +
                                std::to_string(state.shnEditCounter) + "|" + state.shnClientRoot + "|" + state.shnServerRoot + "|" +
                                (app::CurrentLanguage() == app::Language::German ? "de" : "en");
    if (listKey != is.listKey) {
        is.listKey = listKey;
        is.keys.clear();
        is.labels.clear();
        const std::string needle = LowerAscii(is.filter);
        for (const auto& k : SystemKeys(state, is, sys)) {
            const std::string name = NameOf(state, is, sys.display, k);
            const std::string label = k == kAllRows ? std::string(L("(alle Zeilen)", "(all rows)"))
                                    : (k.empty() ? std::string(L("(leer)", "(empty)")) : k) + (name.empty() ? std::string() : "  ·  " + name);
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            if (is.onlyProblems && GroupProblems(state, is, sys, k).empty()) continue;
            is.keys.push_back(k);
            is.labels.push_back(label);
        }
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##itemSystemGroups", ImVec2(300.0f, avail.y), true);
    DrawSearchInput((id + "GroupFilter").c_str(), L("Filter", "Filter"), is.filter, sizeof(is.filter));
    UI::Checkbox(L("Nur mit Problemen##itemSystems", "Problems only##itemSystems"), &is.onlyProblems);
    ImGui::TextDisabled(L("%zu Einträge", "%zu entries"), is.keys.size());
    ImGui::BeginChild("##itemSystemGroupList", ImVec2(0, global ? 0.0f : -118.0f), false);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(is.keys.size()));
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto& k = is.keys[static_cast<std::size_t>(i)];
            const std::string label = is.labels[static_cast<std::size_t>(i)] + "##g" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), is.selected == k)) is.selected = k;
        }
    ImGui::EndChild();
    if (!global) {
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);
        UI::InputTextWithHint("##itemSystemNewKey", L("Neuer Schlüssel", "New key"), is.newKey, sizeof(is.newKey));
        ImGui::BeginDisabled(is.newKey[0] == '\0');
        if (UI::Button(L("Als Kopie anlegen##itemSystems", "Create as copy##itemSystems"), ImVec2(-1, 0))) {
            if (CopyGroup(state, is, sys, is.selected, TrimAscii(is.newKey))) { is.selected = TrimAscii(is.newKey); is.newKey[0] = '\0'; }
        }
        ImGui::BeginDisabled(is.selected.empty());
        if (UI::Button(L("Gewählten umbenennen##itemSystems", "Rename selected##itemSystems"), ImVec2(-1, 0))) {
            RenameGroup(state, is, sys, is.selected, TrimAscii(is.newKey));
            is.selected = TrimAscii(is.newKey);
            is.newKey[0] = '\0';
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::BeginDisabled(is.selected.empty());
        if (UI::Button(L("Gewählten löschen##itemSystems", "Delete selected##itemSystems"), ImVec2(-1, 0))) ImGui::OpenPopup("##itemSystemDelete");
        ImGui::EndDisabled();
        if (ImGui::BeginPopup("##itemSystemDelete")) {
            ImGui::Text(L("%s in allen Tabellen löschen?", "Delete %s in all tables?"), is.selected.c_str());
            const auto linked = LinkedRows(state, is, sys.link, is.selected);
            if (!linked.empty())
                ImGui::TextColored(UiTheme::Warning, L("%zu Zeile(n) verweisen noch darauf (%s).", "%zu row(s) still reference it (%s)."),
                                   linked.size(), sys.link.file);
            if (UI::Button(L("Löschen##itemSystemDelete", "Delete##itemSystemDelete"))) {
                DeleteGroup(state, is, sys, is.selected);
                is.selected.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UI::Button(L("Abbrechen##itemSystemDelete", "Cancel##itemSystemDelete"))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##itemSystemDetail", ImVec2(0, avail.y), true);
    if (is.selected.empty() && !is.keys.empty()) is.selected = is.keys.front();
    if (is.selected.empty()) {
        ImGui::TextDisabled("%s", L("Keine Einträge - Client-/Server-Ordner im Projekt prüfen.", "No entries - check the client/server folders of the project."));
    } else {
        DrawPortrait(state, is, sys.portrait, is.selected);
        if (is.selected != kAllRows) {
            const std::string name = NameOf(state, is, sys.display, is.selected);
            ImGui::TextColored(UiTheme::AccentCyan, "%s", is.selected.c_str());
            if (!name.empty()) { ImGui::SameLine(); ImGui::TextUnformatted(name.c_str()); }
        }
        const auto problems = GroupProblems(state, is, sys, is.selected);
        for (const auto& p : problems) ImGui::TextColored(UiTheme::Warning, "! %s", p.c_str());
        for (const auto& t : sys.tables) DrawGroupTable(state, is, t, is.selected);
        if (sys.link.file) {
            const auto linked = LinkedRows(state, is, sys.link, is.selected);
            ImGui::SeparatorText((std::string(L(sys.link.labelDe, sys.link.labelEn)) + "  (" + std::to_string(linked.size()) + ")").c_str());
            if (linked.empty()) ImGui::TextDisabled("%s", L("Nichts verweist darauf.", "Nothing references it."));
            for (std::size_t i = 0; i < linked.size() && i < 200; ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(linked[i].label.c_str())) {
                    if (sys.link.jumpSystem) {
                        for (std::size_t s = 0; s < cat.systems.size(); ++s)
                            if (std::string(cat.systems[s].id) == sys.link.jumpSystem) is.system = static_cast<int>(s);
                        is.selected = linked[i].target;
                        is.listKey.clear();
                    } else {
                        state.itemForm.selectedId = std::atoll(linked[i].target.c_str());
                        state.shnSubTab = 11;
                    }
                }
                ImGui::PopID();
            }
        }
    }
    if (!is.report.empty()) {
        ImGui::Separator();
        for (const auto& line : is.report) ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::EndChild();
}

} // namespace nge::groups

namespace nge {

std::string GroupSystemsAutomation(EditorState& state, const std::string& cmd, std::istream& in) {
    static const groups::Category* current = &groups::ItemCategory();
    std::string sub = cmd.substr(std::string("groupsystem").size());
    if (sub.empty()) {
        std::string category; in >> category;
        for (const auto* c : {&groups::ItemCategory(), &groups::MountCategory(), &groups::HouseCategory(), &groups::GuildCategory()})
            if (category == c->id) current = c;
        sub = "open";
    }
    return std::string(current->id) + " " + groups::GroupSystemsAutomation(state, *current, sub, in);
}

std::string ItemSystemsAutomation(EditorState& state, const std::string& cmd, std::istream& in) {
    // itemsystem / itemsystemcopy / itemsystemset / itemsystemrow / itemsystemdelete
    const std::string sub = cmd == "itemsystem" ? "open" : cmd.substr(std::string("itemsystem").size());
    return groups::GroupSystemsAutomation(state, groups::ItemCategory(), sub, in);
}

void DrawItemSystemsEditor(EditorState& state) { groups::DrawGroupSystemsEditor(state, groups::ItemCategory()); }

} // namespace nge
