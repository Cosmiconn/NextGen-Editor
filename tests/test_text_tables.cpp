// test_text_tables.cpp
// Text-/Lokalisierungstabellen: Textspalten erkennen, Schlüssel, CSV-Austausch (Kommas,
// Anführungszeichen, Zeilenumbrüche), Abgleich nur geänderter Zellen und Speichern/Laden einer
// SHN mit variabler Zeichenkette (Typ 26). Optional mit echten NA2016-Dateien als Argumente.

#include "mapeditor/core/legacy/ShnFile.hpp"
#include "mapeditor/core/legacy/TextTables.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace theseed::mapeditor::core::legacy;

namespace {

void Require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "[FEHLER] %s\n", what);
    std::exit(1);
}

ShnColumn Column(const char* name, std::uint32_t type, std::uint32_t length, ShnValueKind kind) {
    ShnColumn c;
    c.name = name;
    c.type = type;
    c.length = length;
    c.kind = kind;
    return c;
}

// Wie ScriptMsg (ID, Inx, Nachricht fester Länge) plus eine variable Zeichenkette wie TextData.
ShnFile MakeScriptMsg() {
    ShnFile f;
    f.encrypted = false;
    f.columns = {Column("SM_ID", 2, 2, ShnValueKind::UInt16), Column("SM_Inx", 9, 32, ShnValueKind::String),
                 Column("SM_Msg", 9, 64, ShnValueKind::String), Column("acString", 26, 1, ShnValueKind::String)};
    const char* msgs[] = {"Intruder Alert !!", "Say \"hi\", then go", "Line one\nLine two"};
    for (std::uint16_t i = 0; i < 3; ++i) {
        ShnRow r;
        r.values = {i, std::string("Inx") + std::to_string(i), std::string(msgs[i]), std::string("var ") + std::to_string(i)};
        f.rows.push_back(std::move(r));
    }
    return f;
}

void Synthetic() {
    const ShnFile f = MakeScriptMsg();
    const auto columns = TextColumns(f);
    Require(columns.size() == 2 && columns[0] == 2 && columns[1] == 3, "Textspalten: SM_Msg (64 Byte) und acString (Typ 26), nicht SM_Inx");
    Require(TextRowKeys(f)[1] == "1", "Schlüssel = erste Spalte");
    ShnFile dup = f;
    dup.rows.push_back(dup.rows[1]);
    const auto dupKeys = TextRowKeys(dup);
    Require(dupKeys[1] == "1" && dupKeys[3] == "1#2", "doppelter Schlüssel: zweites Vorkommen 1#2");

    ShnFile onlyText;
    onlyText.columns = {Column("Desc", 26, 1, ShnValueKind::String)};
    onlyText.rows.resize(2);
    onlyText.rows[0].values = {std::string("a")};
    onlyText.rows[1].values = {std::string("b")};
    Require(TextRowKeys(onlyText)[1] == "#1", "ohne Schlüsselspalte: Zeilennummer");

    // CSV hin und zurück, inklusive Komma, Anführungszeichen und Zeilenumbruch.
    const auto rows = TextCsvRowsFor(f, "ScriptMsg.shn");
    Require(rows.size() == 6, "je Zeile zwei Textzellen");
    const std::string csv = WriteTextCsv(rows);
    const auto parsed = ParseTextCsv(csv);
    Require(parsed.has_value(), "CSV lässt sich lesen");
    Require(parsed->size() == rows.size(), "gleiche Anzahl Zeilen nach dem Lesen");
    for (std::size_t i = 0; i < rows.size(); ++i)
        Require((*parsed)[i].table == rows[i].table && (*parsed)[i].key == rows[i].key &&
                    (*parsed)[i].column == rows[i].column && (*parsed)[i].text == rows[i].text,
                "CSV-Zeile unverändert (Kommas, Anführungszeichen, Zeilenumbruch)");
    Require(ParseTextCsv("\xEF\xBB\xBF" + csv).has_value(), "UTF-8-BOM wird übersprungen");
    Require(!ParseTextCsv("table,key,column,text\r\nX,1,SM_Msg,\"offen\r\n").has_value(), "offenes Anführungszeichen wird gemeldet");
    Require(!ParseTextCsv("falsch\r\n").has_value(), "falsche Kopfzeile wird gemeldet");

    // Abgleich: unverändert -> keine Änderung.
    auto diff = DiffTextCsv(f, "ScriptMsg.shn", *parsed);
    Require(diff.changes.empty() && diff.problems.empty() && diff.unchanged == rows.size(), "unveränderte CSV ergibt keine Änderung");

    // Eine Übersetzung, ein unbekannter Schlüssel, eine unbekannte Spalte, ein zu langer Text, fremde Tabelle.
    auto edited = *parsed;
    edited[0].text = "Eindringling!";
    edited.push_back({"ScriptMsg.shn", "99", "SM_Msg", "x"});
    edited.push_back({"ScriptMsg.shn", "1", "SM_Inx", "x"});
    edited.push_back({"ScriptMsg.shn", "2", "SM_Msg", std::string(64, 'x')});
    edited.push_back({"Other.shn", "1", "SM_Msg", "ignoriert"});
    diff = DiffTextCsv(f, "ScriptMsg.shn", edited);
    Require(diff.changes.size() == 1 && diff.changes[0].row == 0 && diff.changes[0].column == 2 &&
                diff.changes[0].text == "Eindringling!",
            "genau die geänderte Zelle wird übernommen");
    Require(diff.problems.size() == 3, "unbekannter Schlüssel, Nicht-Textspalte und zu langer Text gemeldet");

    // Speichern und Laden mit geändertem Text in fester und variabler Spalte.
    ShnFile changed = f;
    changed.rows[0].values[2] = std::string("Eindringling!");
    changed.rows[1].values[3] = std::string("ein deutlich längerer variabler Text als vorher");
    const auto path = std::filesystem::temp_directory_path() / "test_text_tables.shn";
    Require(SaveShnFile(changed, path).has_value(), "SHN mit geänderten Texten gespeichert");
    const auto reloaded = LoadShnFile(path);
    Require(reloaded.has_value(), "gespeicherte SHN lädt");
    Require(ShnValueToString(reloaded->rows[0].values[2]) == "Eindringling!" &&
                ShnValueToString(reloaded->rows[1].values[3]) == "ein deutlich längerer variabler Text als vorher" &&
                ShnValueToString(reloaded->rows[2].values[2]) == "Line one\nLine two",
            "Texte kommen nach Speichern/Laden unverändert an");
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// Echte Tabelle: Export aller Textzellen, Abgleich ohne Änderung, Speichern bytegleich.
void RealTable(const std::filesystem::path& path) {
    const auto f = LoadShnFile(path);
    Require(f.has_value(), "echte Texttabelle lädt");
    const std::string name = path.filename().string();
    const auto rows = TextCsvRowsFor(*f, name);
    const auto parsed = ParseTextCsv(WriteTextCsv(rows));
    Require(parsed.has_value() && parsed->size() == rows.size(), "echte Tabelle: CSV hin und zurück");
    const auto diff = DiffTextCsv(*f, name, *parsed);
    std::printf("%s: %zu Zeilen, %zu Textzellen, %zu Abweichungen, %zu Hinweise\n", name.c_str(), f->rows.size(),
                rows.size(), diff.changes.size(), diff.problems.size());
    Require(diff.changes.empty(), "echte Tabelle: unveränderte CSV ändert nichts");
}

} // namespace

int main(int argc, char** argv) {
    Synthetic();
    for (int i = 1; i < argc; ++i) RealTable(argv[i]);
    std::printf("Texttabellen: ok\n");
    return 0;
}
