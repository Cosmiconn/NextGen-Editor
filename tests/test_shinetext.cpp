#include "mapeditor/core/legacy/ShineText.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace theseed::mapeditor::core::legacy;

// Prüfung unabhängig von NDEBUG (die CI baut Release, dort entfällt assert samt Inhalt).
static void Require(bool ok, const char* what) {
    if (ok) return;
    std::cerr << "[FEHLER] " << what << "\n";
    std::exit(1);
}

// Ganze Datei lesen; der Stream ist danach geschlossen (unter Windows lässt sich eine offene Datei
// nicht löschen).
static std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

// Wie Require, zeigt bei Abweichung aber beide Texte (Tabs/Zeilenumbrüche sichtbar).
static void RequireText(const std::string& actual, const std::string& expected, const char* what) {
    if (actual == expected) return;
    auto show = [](const std::string& s) {
        std::string o;
        for (char c : s) o += c == '\t' ? std::string("\\t") : c == '\n' ? std::string("\\n\n") : std::string(1, c);
        return o;
    };
    std::cerr << "[FEHLER] " << what << "\n--- erwartet:\n" << show(expected) << "--- erhalten:\n" << show(actual) << "\n";
    std::exit(1);
}

static void WriteAll(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

// Neue Records übernehmen die Form einer Musterzeile (führende Tabs, "#Record"/"#recordin",
// Leerzeichen hinter Zahlen, Auffüll-Tabs) - wie World/Field.txt und World/NPC.txt in NA2016.
static void NewRecordStyle() {
    const auto path = std::filesystem::temp_directory_path() / "nextgen_shinetext_style.txt";
    const std::string original =
        "#Table\tFieldList\t\t\t\n"
        "#ColumnType\tString[13]\tDWRD\tDWRD\t\t\n"
        "#ColumnName\tMapIDClient\tSerial\tRegenX\t\t\n"
        "#Record\tRou\t0 \t4134\t\t\n"
        "#Record\tEld\t1 \t5000\t\t\n"
        ";\tKommentar\n"
        "#Table\tLinkTable\n"
        "#ColumnType\tString[32]\tDWRD\n"
        "#ColumnName\tArgument\tX\n"
        "\t#recordin\tLinkTable\tGateRou1\t6477\t\t\t\n";
    WriteAll(path, original);
    auto file = LoadShineTextFile(path);
    Require(file.has_value(), "Musterdatei laden");
    auto* field = file->FindTable("FieldList");
    auto* links = file->FindTable("LinkTable");
    Require(field && links && field->records.size() == 2 && links->records.size() == 1, "Tabellen der Musterdatei erkannt");
    // Klon der Vorlagenzeile "Eld" mit neuen Werten.
    ShineRecord clone = field->records[1];
    clone.styleLine = clone.sourceLine;
    clone.sourceLine = 0;
    clone.values = {"TestLand", "155", "6400"};
    field->records.push_back(clone);
    // Neuer Record ohne Musterzeile: Form der letzten Zeile seiner Tabelle.
    links->records.push_back(ShineRecord{{"GateTestLand1", "5000"}, 0});
    const bool saved1 = SaveShineTextFile(*file, path).has_value();
    Require(saved1, "Musterdatei speichern");
    const std::string expected =
        "#Table\tFieldList\t\t\t\n"
        "#ColumnType\tString[13]\tDWRD\tDWRD\t\t\n"
        "#ColumnName\tMapIDClient\tSerial\tRegenX\t\t\n"
        "#Record\tRou\t0 \t4134\t\t\n"
        "#Record\tEld\t1 \t5000\t\t\n"
        "#Record\tTestLand\t155 \t6400\t\t\n" // "#Record", "155 " wie "1 ", "6400" wie "5000", Auffüllung
        ";\tKommentar\n"
        "#Table\tLinkTable\n"
        "#ColumnType\tString[32]\tDWRD\n"
        "#ColumnName\tArgument\tX\n"
        "\t#recordin\tLinkTable\tGateRou1\t6477\t\t\t\n"
        "\t#recordin\tLinkTable\tGateTestLand1\t5000\t\t\t\n"; // führendes Tab, #recordin, Auffüllung
    RequireText(ReadAll(path), expected, "neue Records muessen die Form der Musterzeile haben");

    // Musterzeile "#Record", Einfügestelle aber hinter einem anderen #Table: "#record" würde dort zur
    // falschen Tabelle gehören - also "#recordin <Tabelle>" (Rest der Form bleibt).
    const std::string mixed =
        "#Table\tA\n#ColumnType\tDWRD\tDWRD\n#ColumnName\tX\tY\n"
        "#Record\t1 \t2\t\n"
        "#Table\tB\n#ColumnType\tDWRD\n#ColumnName\tZ\n"
        "#recordin\tA\t3 \t4\t\n";
    WriteAll(path, mixed);
    auto m = LoadShineTextFile(path);
    Require(m.has_value() && m->FindTable("A") && m->FindTable("A")->records.size() == 2, "gemischte Datei laden");
    ShineRecord r{{"5", "6"}, 0};
    r.styleLine = m->FindTable("A")->records[0].sourceLine;
    m->FindTable("A")->records.push_back(r);
    const bool saved2 = SaveShineTextFile(*m, path).has_value();
    Require(saved2, "gemischte Datei speichern");
    RequireText(ReadAll(path), mixed + "#recordin\tA\t5 \t6\t\n", "hinter fremdem #Table muss #recordin geschrieben werden");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::cout << "Neue Records im Stil der Musterzeile: ok\n";
}

// Konstrukte aus echten NA2016-Dateien, die früher falsch gelesen wurden: Kommentare mitten in der
// Zeile (Kopf und Record), Werte jenseits der Spaltenzahl, mehrzeilige Texte, leerzeichengetrennte
// Records unter "#delimiter \x20" und Records ohne Werte.
static void SpecialLines() {
    const auto path = std::filesystem::temp_directory_path() / "nextgen_shinetext_special.txt";
    const std::string original =
        "\t#delimiter\t\\x20\t\t; Space is delimiter\n"
        "#Table\tSkill\t\t; Kommentar zur Tabelle\n"
        "#ColumnType\tString[32]\tString[32]\t\t\n"
        "#ColumnName\tName\tEffect\t; *.ps\t\n"                         // Kommentar im Kopf
        "#Record\tAtk\tKarenWingAtk\t; \xb1\xe2\xbc\xfa \xbc\xb3\xb8\xed\t\t\n" // Kommentar im Record (EUC-KR)
        "#Record\tDef\t-\t\t\t\n"
        "#Record\t\t\t\n"                                               // Record ohne Werte
        "#Table\tScript\n"
        "#ColumnType\tINDEX\n"
        "#ColumnName\tScrIndex\n"
        "#Record\tKQ60\tText jenseits der Spalten\n"                    // Daten breiter als der Kopf
        "#Table\tScenario\n"
        "#ColumnType\tINDEX\tSTRING[128]\n"
        "#ColumnName\tScrIndex\tScrString\n"
        "#Record\tShort\tEine Zeile\t;\t1\n"
        "#Record\tLong\tErste Zeile\n"
        "\t\tzweite Zeile\n"                                            // Folgezeile eines Texts
        "\t\tdritte Zeile\t;\t7\n"
        "#Table\tShineNPC\n"
        "#ColumnType\tSTRING[33]\tSTRING[20]\tDWRD\n"
        "#ColumnName\tMobName\tMap\tX\n"
        "\t#recordin\tShineNPC\tAlpha\tRou\t100\t\t\n"
        "    #recordin    ShineNPC    Xiaoming        Eld    11683\t\t\n"; // Leerzeichen statt Tabs
    WriteAll(path, original);
    auto file = LoadShineTextFile(path);
    Require(file.has_value(), "Sonderzeilen-Datei laden");
    Require(file->spaceDelimiter, "#delimiter \\x20 erkannt");
    auto* skill = file->FindTable("Skill");
    auto* script = file->FindTable("Script");
    auto* scenario = file->FindTable("Scenario");
    auto* npc = file->FindTable("ShineNPC");
    Require(skill && script && scenario && npc, "alle vier Tabellen erkannt");
    Require(skill->columns.size() == 2, "Kommentar im Kopf ist keine Spalte");
    Require(skill->records.size() == 3, "drei Skill-Records");
    Require(skill->records[0].values == std::vector<std::string>{"Atk", "KarenWingAtk"}, "Kommentar im Record ist kein Wert");
    Require(skill->records[2].values.empty(), "Record ohne Werte");
    Require(script->columns.size() == 1 && script->records.size() == 1, "Script-Tabelle");
    Require(script->records[0].values == std::vector<std::string>{"KQ60"}, "Werte jenseits der Spaltenzahl gehören nicht zu den Werten");
    Require(scenario->records.size() == 2, "Scenario-Tabelle");
    Require(scenario->records[0].values == std::vector<std::string>{"Short", "Eine Zeile"}, "Kommentar hinter einem Text");
    Require(scenario->records[1].continuationLines == 2, "zwei Folgezeilen erkannt");
    Require(scenario->records[1].values == std::vector<std::string>{"Long", "Erste Zeile\n\t\tzweite Zeile\n\t\tdritte Zeile"},
            "mehrzeiliger Text steht vollständig im letzten Wert");
    Require(npc->records.size() == 2, "leerzeichengetrennter Record wird gelesen");
    Require(npc->records[1].values == std::vector<std::string>{"Xiaoming", "Eld", "11683"}, "Werte der leerzeichengetrennten Zeile");

    // Unverändert speichern: byte-identisch.
    Require(SaveShineTextFile(*file, path).has_value(), "Sonderzeilen unverändert speichern");
    RequireText(ReadAll(path), original, "unverändert gespeicherte Sonderzeilen bleiben byte-identisch");

    // Bearbeiten: Kommentar, Überzähliges, Auffüllung und Leerzeichen-Trenner bleiben stehen.
    skill->records[0].values[1] = "KarenWingAtk2";
    skill->records[2].values = {"Neu", "X"};
    script->records[0].values[0] = "KQ61";
    npc->records[1].values[2] = "12000";
    Require(SaveShineTextFile(*file, path).has_value(), "Sonderzeilen bearbeitet speichern");
    std::string expected = original;
    auto replace = [&](const std::string& from, const std::string& to) {
        const auto pos = expected.find(from);
        Require(pos != std::string::npos, "Testvorlage enthält die Zeile");
        expected.replace(pos, from.size(), to);
    };
    replace("#Record\tAtk\tKarenWingAtk\t;", "#Record\tAtk\tKarenWingAtk2\t;");
    replace("#Record\t\t\t\n", "#Record\tNeu\tX\t\t\t\n"); // Auffüllung bleibt stehen
    replace("#Record\tKQ60\tText", "#Record\tKQ61\tText");
    replace("Eld    11683\t\t\n", "Eld    12000\t\t\n");
    RequireText(ReadAll(path), expected, "bearbeitete Sonderzeilen behalten Kommentar, Überzähliges, Auffüllung und Trenner");

    // Ein Record ohne Werte bleibt beim Speichern exakt (früher kam ein Tab hinzu).
    auto again = LoadShineTextFile(path);
    Require(again.has_value(), "bearbeitete Datei neu laden");
    again->FindTable("Skill")->records[2].values.clear();
    again->FindTable("Skill")->records[1].values = {"Def", "-"}; // unverändert
    Require(SaveShineTextFile(*again, path).has_value(), "Record ohne Werte speichern");
    std::string cleared = expected;
    const std::string filled = "#Record\tNeu\tX\t\t\t\n";
    cleared.replace(cleared.find(filled), filled.size(), "#Record\t\t\t\n");
    RequireText(ReadAll(path), cleared, "geleerter Record behält seine Auffüllung ohne zusätzliches Tab");

    // Mehrzeiliger Text: nur unverändert speicherbar, Bearbeiten wird abgelehnt.
    auto multi = LoadShineTextFile(path);
    Require(multi.has_value(), "Datei für mehrzeiligen Test laden");
    multi->FindTable("Scenario")->records[1].values[0] = "Changed";
    Require(!SaveShineTextFile(*multi, path).has_value(), "Bearbeiten eines mehrzeiligen Texts wird abgelehnt");
    RequireText(ReadAll(path), cleared, "abgelehntes Speichern lässt die Datei unverändert");

    // Löschen eines mehrzeiligen Records nimmt seine Folgezeilen mit.
    auto del = LoadShineTextFile(path);
    Require(del.has_value(), "Datei für Löschtest laden");
    auto& scenarioRecords = del->FindTable("Scenario")->records;
    scenarioRecords.erase(scenarioRecords.begin() + 1);
    Require(SaveShineTextFile(*del, path).has_value(), "mehrzeiligen Record löschen");
    std::string withoutLong = cleared;
    const std::string longBlock = "#Record\tLong\tErste Zeile\n\t\tzweite Zeile\n\t\tdritte Zeile\t;\t7\n";
    withoutLong.erase(withoutLong.find(longBlock), longBlock.size());
    RequireText(ReadAll(path), withoutLong, "Folgezeilen verschwinden mit ihrem Record");

    // Neuer ShineNPC-Record nimmt die tab-getrennte Zeile als Muster, nicht die leerzeichengetrennte,
    // und kommt hinter die letzte Zeile der Tabelle.
    auto add = LoadShineTextFile(path);
    Require(add.has_value(), "Datei für neuen NPC laden");
    add->FindTable("ShineNPC")->records.push_back(ShineRecord{{"Beta", "Eld", "200"}, 0});
    Require(SaveShineTextFile(*add, path).has_value(), "neuen NPC speichern");
    RequireText(ReadAll(path), withoutLong + "\t#recordin\tShineNPC\tBeta\tEld\t200\t\t\n", "neuer NPC in Form der tab-getrennten Musterzeile");

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::cout << "Kommentare, Ueberzaehliges, mehrzeilige Texte, Leerzeichen-Trenner: ok\n";
}

int main(int argc, char** argv) {
    NewRecordStyle();
    SpecialLines();
    if (argc < 2) { std::cerr << "usage: test_shinetext file.txt (z.B. World/NPC.txt oder MobRegen/<Karte>.txt)\n"; return 2; }
    auto src = std::filesystem::path(argv[1]);
    auto loaded = LoadShineTextFile(src);
    Require(loaded.has_value(), "Datei laden");
    auto& f = *loaded;

    // Unverändert speichern muss byte-identisch zum Original sein (reines Round-Trip, siehe
    // ShnFile-Pendant test_shn_file.cpp), auch bei gemischten Zeilenenden. Seit [0.44.25] gegen alle
    // Textdateien aus Server/9Data/Shine der NA2016-Daten geprueft (inzwischen 680).
    auto out = std::filesystem::temp_directory_path() / "nextgen_shinetext_roundtrip_test.txt";
    Require(SaveShineTextFile(f, out).has_value(), "unverändert speichern");
    Require(ReadAll(src) == ReadAll(out), "unveraendert gespeicherte ShineText-Datei muss byte-identisch bleiben");

    // Skript-Dateien ohne Tabelle (DefaultCharacterData.txt, World/QuestParser.txt) sind kein
    // Tabellenformat - sie bleiben nur unverändert erhalten.
    if (f.tables.empty()) {
        std::error_code ec;
        std::filesystem::remove(out, ec);
        std::cout << "keine #Table - Datei bleibt unveraendert erhalten: ok\n";
        return 0;
    }
    for (auto& t : f.tables) {
        Require(t.columns.size() > 0, "jede Tabelle hat Spalten");
        for (auto& r : t.records) {
            // Fehlende Werte am Ende sind leer (z. B. "#Record\t\t\t;" in Script/Scenario.txt); mehr
            // Werte als Spalten darf es nach dem Abtrennen von Kommentaren nicht geben.
            Require(r.values.size() <= t.columns.size(), "nicht mehr Werte als Spalten (Kommentare abgetrennt)");
        }
    }
    // Hinweis: manche echten Dateien haben ALLE Records auskommentiert (z.B. Event-Karten
    // ohne aktive Mob-Spawns) - 0 Records ist dann korrekt, kein Parserfehler. Die Bearbeitungs-
    // pruefungen unten laufen nur, wenn es Records gibt.
    bool anyRecords = false;
    for (const auto& t : f.tables) anyRecords = anyRecords || !t.records.empty();
    auto reloadedUnchanged = LoadShineTextFile(out);
    assert(reloadedUnchanged);
    assert(reloadedUnchanged->tables.size() == f.tables.size());
    for (std::size_t ti = 0; ti < f.tables.size(); ++ti) {
        assert(reloadedUnchanged->tables[ti].records.size() == f.tables[ti].records.size());
        for (std::size_t ri = 0; ri < f.tables[ti].records.size(); ++ri) {
            assert(reloadedUnchanged->tables[ti].records[ri].values == f.tables[ti].records[ri].values &&
                   "unveraendertes Speichern+Neuladen muss dieselben Werte liefern");
        }
    }

    // Regression: echte World/ItemDropTable.txt-Dateien besitzen 290 Datenspalten plus ein
    // "\t;"-Sentinel im Header. Manche Records tragen dort ";" und andere nur ein leeres
    // Auffuellfeld. Beides darf NICHT als 291. Datenspalte erscheinen; Speichern muss das
    // Sentinel trotzdem byte-/formatgetreu erhalten.
    {
        constexpr std::size_t kWideColumns = 290;
        const auto widePath = std::filesystem::temp_directory_path() / "nextgen_shinetext_wide_sentinel.txt";
        const auto wideRoundtrip = std::filesystem::temp_directory_path() / "nextgen_shinetext_wide_roundtrip.txt";
        const auto wideEdited = std::filesystem::temp_directory_path() / "nextgen_shinetext_wide_edit.txt";

        std::ofstream wideOut(widePath, std::ios::binary | std::ios::trunc);
        assert(wideOut);
        wideOut << "#TABLE\tItemGroup\r\n";
        wideOut << "#COLUMNTYPE";
        for (std::size_t i = 0; i < kWideColumns; ++i) wideOut << "\tDWord";
        wideOut << "\t;\r\n";
        wideOut << "#COLUMNNAME";
        for (std::size_t i = 0; i < kWideColumns; ++i) wideOut << "\tC" << i;
        wideOut << "\t;\r\n";

        auto writeWideRecord = [&](const char* prefix, bool semicolon) {
            wideOut << "#RECORD";
            for (std::size_t i = 0; i < kWideColumns; ++i)
                wideOut << "\t" << (i == 0 ? prefix : std::to_string(i));
            wideOut << "\t";
            if (semicolon) wideOut << ";";
            wideOut << "\r\n";
        };
        writeWideRecord("A", true);
        writeWideRecord("B", false);
        wideOut.close();

        auto wide = LoadShineTextFile(widePath);
        assert(wide && wide->tables.size() == 1);
        assert(wide->tables[0].trailingSemicolonSentinel);
        assert(wide->tables[0].columns.size() == kWideColumns);
        assert(wide->tables[0].records.size() == 2);
        assert(wide->tables[0].records[0].values.size() == kWideColumns);
        assert(wide->tables[0].records[1].values.size() == kWideColumns);

        assert(SaveShineTextFile(*wide, wideRoundtrip));
        {
            std::ifstream a(widePath, std::ios::binary), b(wideRoundtrip, std::ios::binary);
            std::vector<char> av((std::istreambuf_iterator<char>(a)), {}), bv((std::istreambuf_iterator<char>(b)), {});
            assert(av == bv && "wide Shine sentinel roundtrip must remain byte-identical");
        }

        wide->tables[0].records[0].values[0] = "EDIT";
        ShineRecord added;
        added.values.assign(kWideColumns, "0");
        added.values[0] = "NEW";
        added.sourceLine = 0;
        wide->tables[0].records.push_back(std::move(added));
        assert(SaveShineTextFile(*wide, wideEdited));

        auto wideReloaded = LoadShineTextFile(wideEdited);
        assert(wideReloaded && wideReloaded->tables.size() == 1);
        assert(wideReloaded->tables[0].columns.size() == kWideColumns);
        assert(wideReloaded->tables[0].records.size() == 3);
        assert(wideReloaded->tables[0].records[0].values[0] == "EDIT");
        assert(wideReloaded->tables[0].records.back().values[0] == "NEW");
        for (const auto& record : wideReloaded->tables[0].records)
            assert(record.values.size() == kWideColumns);

        std::filesystem::remove(widePath);
        std::filesystem::remove(wideRoundtrip);
        std::filesystem::remove(wideEdited);
    }

    if (!anyRecords) {
        std::error_code ec;
        std::filesystem::remove(out, ec);
        std::cout << "alle Records auskommentiert - nur Round-Trip geprueft: ok\n";
        return 0;
    }

    // Bestehenden Record bearbeiten, speichern, neu laden, Änderung muss ankommen - Rest der
    // Datei (andere Records, Kommentare) darf sich nicht verändern. Genommen wird der erste
    // einzeilige Record mit Werten (mehrzeilige Texte sind nur unverändert speicherbar).
    std::size_t editTable = f.tables.size(), editRecord = 0;
    for (std::size_t ti = 0; ti < f.tables.size() && editTable == f.tables.size(); ++ti)
        for (std::size_t ri = 0; ri < f.tables[ti].records.size(); ++ri)
            if (f.tables[ti].records[ri].continuationLines == 0 && !f.tables[ti].records[ri].values.empty()) {
                editTable = ti;
                editRecord = ri;
                break;
            }
    Require(editTable < f.tables.size(), "ein bearbeitbarer Record vorhanden");
    auto edited = f;
    auto& firstRecord = edited.tables[editTable].records[editRecord];
    firstRecord.values.front() += "_test";
    auto editedPath = std::filesystem::temp_directory_path() / "nextgen_shinetext_edit_test.txt";
    Require(SaveShineTextFile(edited, editedPath).has_value(), "bearbeitet speichern");
    auto reloaded = LoadShineTextFile(editedPath);
    Require(reloaded.has_value(), "bearbeitete Datei neu laden");
    Require(reloaded->tables[editTable].records[editRecord].values == firstRecord.values, "Aenderung kommt an");
    for (std::size_t ti = 0; ti < f.tables.size(); ++ti) {
        Require(reloaded->tables[ti].records.size() == f.tables[ti].records.size(), "Recordzahl bleibt beim Bearbeiten gleich");
        for (std::size_t ri = 0; ri < f.tables[ti].records.size(); ++ri)
            if (ti != editTable || ri != editRecord)
                Require(reloaded->tables[ti].records[ri].values == f.tables[ti].records[ri].values,
                        "andere Records duerfen beim minimal-invasiven Speichern unveraendert bleiben");
    }

    // Loeschen (seit [0.44.27]): ein aus ShineTable::records entfernter Record verschwindet auch
    // aus der Datei, alle anderen Records aller Tabellen bleiben gleich.
    {
        std::size_t ti = 0;
        while (ti < f.tables.size() && f.tables[ti].records.empty()) ++ti;
        assert(ti < f.tables.size());
        auto withDelete = f;
        withDelete.tables[ti].records.erase(withDelete.tables[ti].records.begin());
        auto delPath = std::filesystem::temp_directory_path() / "nextgen_shinetext_delete_test.txt";
        assert(SaveShineTextFile(withDelete, delPath));
        auto afterDelete = LoadShineTextFile(delPath);
        assert(afterDelete && afterDelete->tables[ti].records.size() == f.tables[ti].records.size() - 1);
        for (std::size_t k = 1; k < f.tables[ti].records.size(); ++k) {
            assert(afterDelete->tables[ti].records[k - 1].values == f.tables[ti].records[k].values);
        }
        for (std::size_t other = 0; other < f.tables.size(); ++other) {
            if (other == ti) continue;
            assert(afterDelete->tables[other].records.size() == f.tables[other].records.size());
        }
        // Einfuegen in die erste Tabelle darf Aenderungen in einer SPAETEREN Tabelle nicht
        // verschieben (mehrtabellige Dateien wie MobRegen/<Karte>.txt).
        std::size_t last = ti;
        for (std::size_t i = 0; i < f.tables.size(); ++i) if (!f.tables[i].records.empty()) last = i;
        if (last != ti && !f.tables[last].records.front().values.empty() && f.tables[last].records.front().continuationLines == 0) {
            auto both = f;
            auto extra = both.tables[ti].records.front();
            extra.sourceLine = 0;
            both.tables[ti].records.push_back(extra);
            both.tables[last].records.front().values[0] = "EDIT_MARKER";
            auto bothPath = std::filesystem::temp_directory_path() / "nextgen_shinetext_multitable_test.txt";
            assert(SaveShineTextFile(both, bothPath));
            auto r2 = LoadShineTextFile(bothPath);
            assert(r2 && r2->tables[last].records.front().values[0] == "EDIT_MARKER");
            assert(r2->tables[last].records.size() == f.tables[last].records.size());
            assert(r2->tables[ti].records.size() == f.tables[ti].records.size() + 1);
        }
    }

    // Tabellen (seit [0.44.28]): neue Tabelle anlegen (isNew) und - bei mehreren Tabellen - die
    // erste entfernen; Kopfzeilen verschwinden mit, alle anderen Tabellen bleiben gleich.
    {
        auto withNew = f;
        ShineTable nt;
        nt.name = "TabNEUTEST";
        nt.isNew = true;
        nt.columns = f.tables.front().columns;
        ShineRecord a;
        a.sourceLine = 0;
        a.values = f.tables.front().records.empty() ? std::vector<std::string>(nt.columns.size(), "x") : f.tables.front().records.front().values;
        nt.records.push_back(a);
        withNew.tables.push_back(nt);
        auto newPath = std::filesystem::temp_directory_path() / "nextgen_shinetext_newtable_test.txt";
        assert(SaveShineTextFile(withNew, newPath));
        auto rn = LoadShineTextFile(newPath);
        assert(rn && rn->tables.size() == f.tables.size() + 1 && rn->tables.back().name == "TabNEUTEST" && rn->tables.back().records.size() == 1);
        if (f.tables.size() > 1) {
            auto withoutFirst = f;
            withoutFirst.tables.erase(withoutFirst.tables.begin());
            auto delPath = std::filesystem::temp_directory_path() / "nextgen_shinetext_deltable_test.txt";
            assert(SaveShineTextFile(withoutFirst, delPath));
            auto rd = LoadShineTextFile(delPath);
            assert(rd && rd->tables.size() == f.tables.size() - 1 && rd->tables.front().name == f.tables[1].name);
        }
        // Neue Datei aus Vorlage (Kopfzeilen der Vorlage, eigene Tabelle)
        auto fresh = MakeShineFileWithHeaderOf(f, std::filesystem::temp_directory_path() / "nextgen_shinetext_fresh_test.txt");
        fresh.tables.push_back(nt);
        assert(SaveShineTextFile(fresh, fresh.path));
        auto rf = LoadShineTextFile(fresh.path);
        assert(rf && rf->tables.size() == 1 && rf->tables.front().records.size() == 1);
    }

    // Neuen Record anhängen (sourceLine=0), speichern, neu laden - muss als zusätzlicher
    // Record am Ende der jeweiligen Tabelle ankommen.
    auto withNew = f;
    std::size_t origCount = withNew.tables.front().records.size();
    std::vector<std::string> newValues(withNew.tables.front().columns.size(), "-");
    newValues.front() = "TestRecordNeu";
    withNew.tables.front().records.push_back(ShineRecord{newValues, 0});
    auto newPath = std::filesystem::temp_directory_path() / "nextgen_shinetext_new_test.txt";
    auto newSave = SaveShineTextFile(withNew, newPath);
    assert(newSave);
    auto reloadedNew = LoadShineTextFile(newPath);
    assert(reloadedNew);
    assert(reloadedNew->tables.front().records.size() == origCount + 1);
    assert(reloadedNew->tables.front().records.back().values.front() == "TestRecordNeu");

    std::filesystem::remove(out);
    std::filesystem::remove(editedPath);
    std::filesystem::remove(newPath);

    std::cout << "0 Fehler.\n";
    return 0;
}
