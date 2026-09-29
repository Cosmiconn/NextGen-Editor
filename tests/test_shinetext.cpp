#include "mapeditor/core/legacy/ShineText.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace theseed::mapeditor::core::legacy;

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: test_shinetext file.txt (z.B. World/NPC.txt oder MobRegen/<Karte>.txt)\n"; return 2; }
    auto src = std::filesystem::path(argv[1]);
    auto loaded = LoadShineTextFile(src);
    assert(loaded);
    auto& f = *loaded;
    assert(!f.tables.empty() && "mindestens eine #Table erwartet");
    for (auto& t : f.tables) {
        assert(t.columns.size() > 0);
        for (auto& r : t.records) {
            assert(r.values.size() == t.columns.size() &&
                   "Werteanzahl muss nach der Leer-Tab-Normalisierung zur Spaltenzahl passen");
        }
    }
    // Hinweis: manche echten Dateien haben ALLE Records auskommentiert (z.B. Event-Karten
    // ohne aktive Mob-Spawns) - 0 Records ist dann korrekt, kein Parserfehler. Deshalb hier
    // keine "mindestens ein Record"-Pflicht mehr, nur dass die Struktur (Tabellen/Spalten)
    // grundsätzlich erkannt wurde.

    // Unverändert speichern muss byte-identisch zum Original sein (reines Round-Trip, siehe
    // ShnFile-Pendant test_shn_file.cpp) - abgesehen von Zeilenende-Normalisierung auf CRLF,
    // die hier bewusst NICHT geprüft wird, da Quelldateien mit gemischten Zeilenenden vorkommen.
    auto out = std::filesystem::temp_directory_path() / "nextgen_shinetext_roundtrip_test.txt";
    auto saved = SaveShineTextFile(f, out);
    assert(saved);
    {
        // Seit [0.44.25]: unveraendert gespeicherte Datei ist BYTE-IDENTISCH zum Original
        // (Zeilenenden je Zeile + Auffuell-Tabs bleiben erhalten). Gegen alle 677 Textdateien
        // aus Server/9Data/Shine der NA2016-Daten geprueft.
        std::ifstream a(src, std::ios::binary), b(out, std::ios::binary);
        std::vector<char> av((std::istreambuf_iterator<char>(a)), {}), bv((std::istreambuf_iterator<char>(b)), {});
        assert(av == bv && "unveraendert gespeicherte ShineText-Datei muss byte-identisch bleiben");
    }
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

    // Bestehenden Record bearbeiten, speichern, neu laden, Änderung muss ankommen - Rest der
    // Datei (andere Records, Kommentare) darf sich nicht verändern.
    auto edited = f;
    assert(!edited.tables.front().records.empty());
    auto& firstRecord = edited.tables.front().records.front();
    assert(!firstRecord.values.empty());
    firstRecord.values.front() += "_test";
    auto editedPath = std::filesystem::temp_directory_path() / "nextgen_shinetext_edit_test.txt";
    auto editedSave = SaveShineTextFile(edited, editedPath);
    assert(editedSave);
    auto reloaded = LoadShineTextFile(editedPath);
    assert(reloaded);
    assert(reloaded->tables.front().records.front().values.front() ==
           firstRecord.values.front());
    if (reloaded->tables.front().records.size() > 1) {
        assert(reloaded->tables.front().records[1].values == f.tables.front().records[1].values &&
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
        if (last != ti) {
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
