// ItemSystemsEditor.cpp
// Item-Systeme (Datei Editor, Unterreiter 16): Sets, Set-Effekte, Aufwertung, Aufwertungs-Chancen,
// Item-Mix, Zufallsoptionen und Item-Pakete. Diese Tabellen sind gruppiert - mehrere Zeilen mit
// demselben Schlüssel bilden einen Eintrag (core::legacy::ShnGroups). Ein allgemeiner Gruppen-Editor
// zeigt je System die Gruppen links und alle Zeilen der gewählten Gruppe in allen beteiligten
// Tabellen rechts; Client- und Server-Kopie werden gemeinsam geändert (n-te Zeile der Gruppe), alles
// mit Strg+Z über den Daten-Verlauf.

#include "EditorShared.hpp"

#include "mapeditor/core/legacy/ShnGroups.hpp"

namespace nge {
namespace {

using Source = EditorState::ShnSource;
namespace legacy = core::legacy;

// Spalte, deren Wert in einer anderen Tabelle nachgeschlagen wird (Anzeige neben der Zelle).
struct Lookup {
    const char* column;
    const char* file;
    Source side;
    const char* keyColumn;
    const char* valueColumn;
    bool required; // unbekannter Wert ist ein Problem (rot)
};

struct GroupTable {
    const char* file;
    bool client;
    bool server;
    const char* keyColumn;
    const char* titleDe;
    const char* titleEn;
    std::vector<Lookup> lookups;
};

// Items, die auf die Gruppe verweisen (Spalte in ItemInfo bzw. ItemInfoServer).
struct ItemLink {
    const char* file = nullptr;
    Source side = Source::Client;
    const char* column = nullptr;
    const char* labelDe = nullptr;
    const char* labelEn = nullptr;
};

struct ItemSystem {
    const char* id;
    const char* nameDe;
    const char* nameEn;
    const char* noteDe;
    const char* noteEn;
    std::vector<GroupTable> tables;
    ItemLink link;
};

const Lookup ItemName(const char* column) {
    return {column, "ItemInfo.shn", Source::Client, "InxName", "Name", true};
}

const std::vector<ItemSystem>& Systems() {
    static const std::vector<ItemSystem> systems = {
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
    return systems;
}

struct Copies { int client = -1; int server = -1; };

int SystemDoc(EditorState& state, const char* file, Source side) {
    const std::string& root = side == Source::Client ? state.shnClientRoot : state.shnServerRoot;
    if (root.empty()) return -1;
    const std::string key = root + "|" + file;
    if (state.itemSystems.missing.contains(key)) return -1;
    const int doc = SharedShnDoc(state, file, side, root);
    if (doc < 0) state.itemSystems.missing.insert(key);
    return doc;
}

Copies CopiesOf(EditorState& state, const GroupTable& t) {
    Copies c;
    if (t.client) c.client = SystemDoc(state, t.file, Source::Client);
    if (t.server) c.server = SystemDoc(state, t.file, Source::Server);
    return c;
}
int MainOf(const Copies& c) { return c.client >= 0 ? c.client : c.server; }

// Nachschlagetabelle Schlüssel -> Wert, gecacht über den Bearbeitungsstand.
const std::unordered_map<std::string, std::string>& LookupMap(EditorState& state, const char* file, Source side,
                                                              const char* keyColumn, const char* valueColumn) {
    struct Cached { std::uint64_t edit = ~0ull; std::unordered_map<std::string, std::string> map; };
    static std::unordered_map<std::string, Cached> cache;
    auto& entry = cache[std::string(file) + "|" + keyColumn + "|" + valueColumn + "|" +
                        (side == Source::Client ? state.shnClientRoot : state.shnServerRoot) + "|" +
                        (app::CurrentLanguage() == app::Language::German ? "de" : "en")];
    if (entry.edit == state.shnEditCounter) return entry.map;
    entry.edit = state.shnEditCounter;
    entry.map.clear();
    const int doc = SystemDoc(state, file, side);
    if (doc < 0) return entry.map;
    const auto& f = ShnDocFile(state, doc);
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        entry.map.emplace(TrimAscii(ShnCellText(f, r, keyColumn)), DisplayText(ShnCellText(f, r, valueColumn)));
    return entry.map;
}

bool EmptyRef(const std::string& v) { return v.empty() || v == "-" || v == "0"; }

// Gruppen eines Systems: Schlüssel aller beteiligten Tabellen (Reihenfolge: erste Tabelle zuerst).
std::vector<std::string> SystemKeys(EditorState& state, const ItemSystem& sys) {
    std::vector<std::string> keys;
    std::unordered_set<std::string> seen;
    for (const auto& t : sys.tables) {
        const int doc = MainOf(CopiesOf(state, t));
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        for (const auto& g : legacy::GroupShnRows(f, ShnColumnIndexByName(f, t.keyColumn)))
            if (seen.insert(g.key).second) keys.push_back(g.key);
    }
    return keys;
}

// Probleme einer Gruppe: Verweise, deren Ziel fehlt.
std::vector<std::string> GroupProblems(EditorState& state, const ItemSystem& sys, const std::string& key) {
    std::vector<std::string> out;
    for (const auto& t : sys.tables) {
        const int doc = MainOf(CopiesOf(state, t));
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        for (const auto row : legacy::ShnGroupRows(f, ShnColumnIndexByName(f, t.keyColumn), key))
            for (const auto& lk : t.lookups) {
                if (!lk.required) continue;
                const std::string v = TrimAscii(ShnCellText(f, row, lk.column));
                if (EmptyRef(v)) continue;
                const auto& map = LookupMap(state, lk.file, lk.side, lk.keyColumn, lk.valueColumn);
                if (!map.empty() && !map.contains(v)) out.push_back(std::string(t.file) + " " + lk.column + ": " + v + L(" unbekannt", " unknown"));
            }
    }
    return out;
}

// Zeilenvorlage: letzte Zeile der Gruppe, sonst letzte Zeile der Datei, Schlüssel ersetzt.
std::optional<legacy::ShnRow> TemplateRow(const legacy::ShnFile& f, int keyCol, const std::string& key, const std::string& newKey) {
    if (f.rows.empty() || keyCol < 0) return std::nullopt;
    const auto rows = legacy::ShnGroupRows(f, keyCol, key);
    legacy::ShnRow row = f.rows[rows.empty() ? f.rows.size() - 1 : rows.back()];
    auto v = legacy::ParseShnValue(f.columns[static_cast<std::size_t>(keyCol)], newKey);
    if (!v) return std::nullopt;
    row.values[static_cast<std::size_t>(keyCol)] = *v;
    return row;
}

// Ändert eine Zelle der n-ten Gruppenzeile in beiden Kopien.
void WriteGroupCell(EditorState& state, const GroupTable& t, const Copies& c, const std::string& key, std::size_t nth,
                    const std::string& column, const std::string& text) {
    DataHistoryBegin(state, std::string(L("Geändert: ", "Changed: ")) + t.file + " " + key + " · " + column);
    for (const int doc : {c.client, c.server}) {
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        const auto rows = legacy::ShnGroupRows(f, ShnColumnIndexByName(f, t.keyColumn), key);
        const int col = ShnColumnIndexByName(f, column.c_str());
        if (nth < rows.size() && col >= 0)
            if (!ApplyShnCellText(state, doc, static_cast<int>(rows[nth]), col, text))
                state.itemSystems.report = {std::string(t.file) + ": " + state.shnStatus};
    }
    DataHistoryCommit(state);
}

void AddGroupRow(EditorState& state, const GroupTable& t, const Copies& c, const std::string& key) {
    DataHistoryBegin(state, std::string(L("Zeile hinzugefügt: ", "Row added: ")) + t.file + " " + key);
    for (const int doc : {c.client, c.server}) {
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        const int keyCol = ShnColumnIndexByName(f, t.keyColumn);
        if (auto row = TemplateRow(f, keyCol, key, key))
            InsertShnRowWithHistory(state, doc, static_cast<int>(legacy::ShnGroupInsertIndex(f, keyCol, key)), std::move(*row));
    }
    DataHistoryCommit(state);
}

void DeleteGroupRow(EditorState& state, const GroupTable& t, const Copies& c, const std::string& key, std::size_t nth) {
    DataHistoryBegin(state, std::string(L("Zeile gelöscht: ", "Row deleted: ")) + t.file + " " + key);
    for (const int doc : {c.client, c.server}) {
        if (doc < 0) continue;
        const auto& f = ShnDocFile(state, doc);
        const auto rows = legacy::ShnGroupRows(f, ShnColumnIndexByName(f, t.keyColumn), key);
        if (nth < rows.size()) DeleteShnRowWithHistory(state, doc, static_cast<int>(rows[nth]));
    }
    DataHistoryCommit(state);
}

// Neue Gruppe als Kopie der gewählten (in allen Tabellen und Kopien), bzw. Umbenennen/Löschen.
bool CopyGroup(EditorState& state, const ItemSystem& sys, const std::string& from, const std::string& to) {
    if (to.empty()) return false;
    for (const auto& k : SystemKeys(state, sys)) if (k == to) { state.itemSystems.report = {to + L(" existiert bereits.", " already exists.")}; return false; }
    DataHistoryBegin(state, std::string(L("Neu: ", "New: ")) + to + L(" (Kopie von ", " (copy of ") + from + ")");
    for (const auto& t : sys.tables) {
        const Copies c = CopiesOf(state, t);
        for (const int doc : {c.client, c.server}) {
            if (doc < 0) continue;
            const auto& f = ShnDocFile(state, doc);
            const int keyCol = ShnColumnIndexByName(f, t.keyColumn);
            const auto rows = legacy::ShnGroupRows(f, keyCol, from);
            std::vector<legacy::ShnRow> copies;
            for (const auto r : rows) {
                legacy::ShnRow row = f.rows[r];
                if (auto v = legacy::ParseShnValue(f.columns[static_cast<std::size_t>(keyCol)], to)) row.values[static_cast<std::size_t>(keyCol)] = *v;
                copies.push_back(std::move(row));
            }
            if (copies.empty())
                if (auto row = TemplateRow(f, keyCol, from, to)) copies.push_back(std::move(*row));
            for (auto& row : copies) InsertShnRowWithHistory(state, doc, static_cast<int>(ShnDocFile(state, doc).rows.size()), std::move(row));
        }
    }
    DataHistoryCommit(state);
    return true;
}

void RenameGroup(EditorState& state, const ItemSystem& sys, const std::string& from, const std::string& to) {
    if (to.empty() || to == from) return;
    for (const auto& k : SystemKeys(state, sys)) if (k == to) { state.itemSystems.report = {to + L(" existiert bereits.", " already exists.")}; return; }
    DataHistoryBegin(state, std::string(L("Umbenannt: ", "Renamed: ")) + from + " -> " + to);
    for (const auto& t : sys.tables) {
        const Copies c = CopiesOf(state, t);
        for (const int doc : {c.client, c.server}) {
            if (doc < 0) continue;
            const auto& f = ShnDocFile(state, doc);
            const int keyCol = ShnColumnIndexByName(f, t.keyColumn);
            for (const auto r : legacy::ShnGroupRows(f, keyCol, from)) ApplyShnCellText(state, doc, static_cast<int>(r), keyCol, to);
        }
    }
    DataHistoryCommit(state);
}

void DeleteGroup(EditorState& state, const ItemSystem& sys, const std::string& key) {
    DataHistoryBegin(state, std::string(L("Gelöscht: ", "Deleted: ")) + key);
    for (const auto& t : sys.tables) {
        const Copies c = CopiesOf(state, t);
        for (const int doc : {c.client, c.server}) {
            if (doc < 0) continue;
            const auto& f = ShnDocFile(state, doc);
            const auto rows = legacy::ShnGroupRows(f, ShnColumnIndexByName(f, t.keyColumn), key);
            for (auto it = rows.rbegin(); it != rows.rend(); ++it) DeleteShnRowWithHistory(state, doc, static_cast<int>(*it));
        }
    }
    DataHistoryCommit(state);
}

std::vector<std::string> LinkedItems(EditorState& state, const ItemLink& link, const std::string& key) {
    std::vector<std::string> out;
    if (!link.file) return out;
    const int doc = SystemDoc(state, link.file, link.side);
    if (doc < 0) return out;
    const auto& f = ShnDocFile(state, doc);
    const auto& names = LookupMap(state, "ItemInfo.shn", Source::Client, "InxName", "Name");
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        if (TrimAscii(ShnCellText(f, r, link.column)) != key) continue;
        const std::string inx = TrimAscii(ShnCellText(f, r, "InxName"));
        const auto it = names.find(inx);
        out.push_back(ShnCellText(f, r, "ID") + "  " + inx + (it != names.end() ? "  ·  " + it->second : std::string()));
    }
    return out;
}

void DrawGroupTable(EditorState& state, const GroupTable& t, const std::string& key) {
    const Copies c = CopiesOf(state, t);
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
    const int keyCol = ShnColumnIndexByName(f, t.keyColumn);
    const auto rows = legacy::ShnGroupRows(f, keyCol, key);
    std::vector<int> cols;
    for (std::size_t col = 0; col < f.columns.size(); ++col) if (static_cast<int>(col) != keyCol) cols.push_back(static_cast<int>(col));
    std::optional<std::size_t> remove;
    ImGui::PushID(t.file);
    if (!rows.empty() &&
        ImGui::BeginTable("##groupRows", static_cast<int>(cols.size()) + 1,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        for (const int col : cols) ImGui::TableSetupColumn(f.columns[static_cast<std::size_t>(col)].name.c_str());
        ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 26.0f);
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
                if (DeferredInputText("##c", current, edited, -1.0f)) WriteGroupCell(state, t, c, key, n, colDef.name, edited);
                for (const auto& lk : t.lookups) {
                    if (colDef.name != lk.column) continue;
                    const std::string v = TrimAscii(current);
                    if (EmptyRef(v)) continue;
                    const auto& map = LookupMap(state, lk.file, lk.side, lk.keyColumn, lk.valueColumn);
                    if (const auto it = map.find(v); it != map.end()) ImGui::TextDisabled("%s", it->second.c_str());
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

} // namespace

std::string ItemSystemsAutomation(EditorState& state, const std::string& cmd, std::istream& in) {
    auto& is = state.itemSystems;
    const auto& systems = Systems();
    const ItemSystem& current = systems[static_cast<std::size_t>(std::clamp(is.system, 0, static_cast<int>(systems.size()) - 1))];
    const auto table = [&](const std::string& file) -> const GroupTable* {
        for (const auto& t : current.tables) if (file == t.file) return &t;
        return nullptr;
    };
    if (cmd == "itemsystem") {
        std::string id, key; in >> id; std::getline(in >> std::ws, key);
        state.screen = AppScreen::ShnEditor;
        state.shnSubTab = 16;
        for (std::size_t s = 0; s < systems.size(); ++s) if (id == systems[s].id) is.system = static_cast<int>(s);
        is.selected = key;
        is.listKey.clear();
        return std::string(systems[static_cast<std::size_t>(is.system)].id) + " " + key + " problems=" +
               std::to_string(GroupProblems(state, systems[static_cast<std::size_t>(is.system)], key).size());
    }
    if (cmd == "itemsystemcopy") {
        std::string to; std::getline(in >> std::ws, to);
        const bool ok = CopyGroup(state, current, is.selected, to);
        if (ok) is.selected = to;
        return std::string("copy ") + (ok ? "ok" : "failed");
    }
    if (cmd == "itemsystemset") {
        std::string file, column, value; std::size_t nth = 0;
        in >> file >> nth >> column; std::getline(in >> std::ws, value);
        const GroupTable* t = table(file);
        if (!t) return "unknown table " + file;
        WriteGroupCell(state, *t, CopiesOf(state, *t), is.selected, nth, column, value);
        return "set " + file + " " + column + "=" + value;
    }
    if (cmd == "itemsystemrow") {
        std::string file; in >> file;
        const GroupTable* t = table(file);
        if (!t) return "unknown table " + file;
        AddGroupRow(state, *t, CopiesOf(state, *t), is.selected);
        return "row added " + file;
    }
    if (cmd == "itemsystemdelete") {
        DeleteGroup(state, current, is.selected);
        const std::string key = is.selected;
        is.selected.clear();
        return "deleted " + key;
    }
    return "unknown command";
}

void DrawItemSystemsEditor(EditorState& state) {
    auto& is = state.itemSystems;
    const auto& systems = Systems();
    is.system = std::clamp(is.system, 0, static_cast<int>(systems.size()) - 1);
    DrawPanelHeader("itemSystemsHeader", L("ITEM-SYSTEME", "ITEM SYSTEMS"), DrawIconGem, "module.prices",
                    L("Sets, Aufwertung, Mix, Zufallsoptionen, Pakete", "Sets, upgrade, mix, random options, packages"));
    if (UI::Button(L("Alle geänderten SHN speichern##itemSystems", "Save all changed SHN##itemSystems")) ||
        (ShortcutPressed(state.shortcutSave) && DirtyShnDocumentCount(state) > 0)) {
        const auto [saved, failed] = SaveAllDirtyShnDocuments(state);
        is.report = {std::string(L("Gespeichert: ", "Saved: ")) + std::to_string(saved) +
                     (failed ? std::string(", ") + L("Fehler: ", "errors: ") + std::to_string(failed) + " · " + state.shnStatus : std::string())};
    }
    ImGui::SameLine();
    DrawDataHistoryButtons(state, "itemSystemsHistory");
    for (int s = 0; s < static_cast<int>(systems.size()); ++s) {
        if (s > 0) ImGui::SameLine();
        if (SceneQuickFilterButton((std::string("itemSystem") + systems[static_cast<std::size_t>(s)].id).c_str(),
                                   L(systems[static_cast<std::size_t>(s)].nameDe, systems[static_cast<std::size_t>(s)].nameEn), is.system == s)) {
            is.system = s;
            is.selected.clear();
            is.listKey.clear();
        }
    }
    const ItemSystem& sys = systems[static_cast<std::size_t>(is.system)];
    ImGui::TextWrapped("%s", L(sys.noteDe, sys.noteEn));

    // Gruppenliste (gecacht über Filter, Bearbeitungsstand und System).
    const std::string listKey = std::to_string(is.system) + "|" + is.filter + "|" + std::to_string(is.onlyProblems) + "|" +
                                std::to_string(state.shnEditCounter) + "|" + state.shnClientRoot + "|" + state.shnServerRoot;
    if (listKey != is.listKey) {
        is.listKey = listKey;
        is.keys.clear();
        const std::string needle = LowerAscii(is.filter);
        for (const auto& k : SystemKeys(state, sys)) {
            if (!needle.empty() && LowerAscii(k).find(needle) == std::string::npos) continue;
            if (is.onlyProblems && GroupProblems(state, sys, k).empty()) continue;
            is.keys.push_back(k);
        }
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##itemSystemGroups", ImVec2(290.0f, avail.y), true);
    DrawSearchInput("itemSystemFilter", L("Filter", "Filter"), is.filter, sizeof(is.filter));
    UI::Checkbox(L("Nur mit Problemen##itemSystems", "Problems only##itemSystems"), &is.onlyProblems);
    ImGui::TextDisabled(L("%zu Einträge", "%zu entries"), is.keys.size());
    ImGui::BeginChild("##itemSystemGroupList", ImVec2(0, -118.0f), false);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(is.keys.size()));
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto& k = is.keys[static_cast<std::size_t>(i)];
            const std::string label = (k.empty() ? std::string(L("(leer)", "(empty)")) : k) + "##g" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), is.selected == k))
                is.selected = k;
        }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputTextWithHint("##itemSystemNewKey", L("Neuer Schlüssel", "New key"), is.newKey, sizeof(is.newKey));
    ImGui::BeginDisabled(is.newKey[0] == '\0');
    if (UI::Button(L("Als Kopie anlegen##itemSystems", "Create as copy##itemSystems"), ImVec2(-1, 0))) {
        if (CopyGroup(state, sys, is.selected, TrimAscii(is.newKey))) { is.selected = TrimAscii(is.newKey); is.newKey[0] = '\0'; }
    }
    ImGui::BeginDisabled(is.selected.empty());
    if (UI::Button(L("Gewählten umbenennen##itemSystems", "Rename selected##itemSystems"), ImVec2(-1, 0))) {
        RenameGroup(state, sys, is.selected, TrimAscii(is.newKey));
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
        const auto items = LinkedItems(state, sys.link, is.selected);
        if (!items.empty())
            ImGui::TextColored(UiTheme::Warning, L("%zu Item(s) verweisen noch darauf.", "%zu item(s) still reference it."), items.size());
        if (UI::Button(L("Löschen##itemSystemDelete", "Delete##itemSystemDelete"))) {
            DeleteGroup(state, sys, is.selected);
            is.selected.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (UI::Button(L("Abbrechen##itemSystemDelete", "Cancel##itemSystemDelete"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##itemSystemDetail", ImVec2(0, avail.y), true);
    if (is.selected.empty() && !is.keys.empty()) is.selected = is.keys.front();
    if (is.selected.empty()) {
        ImGui::TextDisabled("%s", L("Keine Einträge - Client-/Server-Ordner im Projekt prüfen.", "No entries - check the client/server folders of the project."));
    } else {
        ImGui::TextColored(UiTheme::AccentCyan, "%s", is.selected.c_str());
        const auto problems = GroupProblems(state, sys, is.selected);
        for (const auto& p : problems) ImGui::TextColored(UiTheme::Warning, "! %s", p.c_str());
        for (const auto& t : sys.tables) DrawGroupTable(state, t, is.selected);
        if (sys.link.file) {
            const auto items = LinkedItems(state, sys.link, is.selected);
            ImGui::SeparatorText((std::string(L(sys.link.labelDe, sys.link.labelEn)) + "  (" + std::to_string(items.size()) + ")").c_str());
            if (items.empty()) ImGui::TextDisabled("%s", L("Kein Item verweist darauf.", "No item references it."));
            for (std::size_t i = 0; i < items.size() && i < 200; ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(items[i].c_str())) {
                    state.itemForm.selectedId = std::atoll(items[i].c_str());
                    state.shnSubTab = 11;
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

} // namespace nge
