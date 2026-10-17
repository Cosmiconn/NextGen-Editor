// test_data_history.cpp
// Gemeinsamer Undo/Redo-Verlauf der Daten-Editoren (core::DataHistory) und die SHN-Speicherprüfung
// mit Zeilenherkunft (core::legacy::CheckShnPatch): gelöschte Zeilen sind erlaubt, solange jede
// verbliebene Zeile ihrer Zeile auf der Platte zugeordnet ist und Änderungen markiert sind.

#include "mapeditor/core/DataHistory.hpp"
#include "mapeditor/core/legacy/ShnPatchCheck.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace theseed::mapeditor::core;
using namespace theseed::mapeditor::core::legacy;

namespace {
int g_failures = 0;
void Check(bool ok, const std::string& what) {
    if (ok) std::printf("[ok]     %s\n", what.c_str());
    else { std::fprintf(stderr, "[FEHLER] %s\n", what.c_str()); ++g_failures; }
}
// Eine Änderung "value = to" aufzeichnen, die bereits passiert ist.
void Set(DataHistory& h, int& value, int to, const std::string& label = {}) {
    const int from = value;
    value = to;
    h.Record({[&value, to]() { value = to; }, [&value, from]() { value = from; }}, label);
}
ShnFile Table(std::size_t rows) {
    ShnFile f;
    f.encrypted = false;
    ShnColumn id; id.name = "ID"; id.type = 3; id.length = 4; id.kind = ShnValueKind::UInt32;
    ShnColumn name; name.name = "InxName"; name.type = 9; name.length = 16; name.kind = ShnValueKind::String;
    f.columns = {id, name};
    for (std::size_t i = 0; i < rows; ++i) {
        ShnRow r;
        r.values = {static_cast<std::uint32_t>(i + 1), std::string("Row") + std::to_string(i + 1)};
        f.rows.push_back(r);
    }
    return f;
}
std::vector<std::vector<std::uint8_t>> Clean(std::size_t rows) { return std::vector<std::vector<std::uint8_t>>(rows, std::vector<std::uint8_t>(2, 0)); }
} // namespace

int main() {
    std::printf("== DataHistory ==\n");
    {
        DataHistory h;
        int a = 0, b = 0;
        Set(h, a, 1, "a=1");
        h.Begin("a und b");
        Set(h, a, 2);
        Set(h, b, 5);
        h.Commit();
        Check(h.UndoCount() == 2 && h.UndoLabel() == "a und b", "zwei Einträge, der letzte ist die Transaktion");
        Check(h.Undo() && a == 1 && b == 0, "Undo nimmt die ganze Transaktion zurück (a und b)");
        Check(h.Undo() && a == 0, "zweites Undo nimmt die einzelne Änderung zurück");
        Check(!h.Undo() && !h.CanUndo(), "nichts mehr rückgängig zu machen");
        Check(h.Redo() && a == 1 && h.Redo() && a == 2 && b == 5, "Redo stellt beide Schritte wieder her");
        Set(h, a, 9);
        Check(!h.CanRedo(), "eine neue Änderung verwirft Redo");
    }
    {
        DataHistory h;
        int a = 0;
        h.Begin("außen");
        h.Begin("innen");
        Set(h, a, 1);
        h.Commit();
        Check(h.UndoCount() == 0 && h.InTransaction(), "verschachtelt: erst das äußere Commit schließt ab");
        Set(h, a, 2);
        h.Commit();
        Check(h.UndoCount() == 1 && h.UndoLabel() == "außen", "eine Transaktion mit der äußeren Beschriftung");
        h.Undo();
        Check(a == 0, "Undo in umgekehrter Reihenfolge bis zum Ausgangswert");
        h.Begin("leer");
        Check(!h.Commit() && h.UndoCount() == 0, "leere Transaktion wird verworfen");
        h.Begin("abbrechen");
        Set(h, a, 7);
        h.Abort();
        Check(a == 0 && !h.InTransaction() && h.UndoCount() == 0, "Abort nimmt bereits Ausgeführtes zurück");
    }
    {
        DataHistory h(3);
        int a = 0;
        for (int i = 1; i <= 5; ++i) Set(h, a, i);
        Check(h.UndoCount() == 3, "Grenze: nur die letzten 3 Einträge bleiben");
        while (h.Undo()) {}
        Check(a == 2, "ältere Einträge sind verworfen (Stand nach Schritt 2 bleibt)");
        const auto rev = h.Revision();
        h.Redo();
        Check(h.Revision() > rev, "Revision steigt mit jeder Verlaufsänderung");
    }

    std::printf("\n== CheckShnPatch ==\n");
    const ShnFile base = Table(4);
    {
        auto cur = base;
        Check(CheckShnPatch(base, cur, Clean(4), {}).has_value(), "unverändert: gültig");
        cur.rows[1].values[1] = std::string("Changed");
        Check(!CheckShnPatch(base, cur, Clean(4), {}).has_value(), "unmarkierte Änderung wird abgelehnt");
        auto dirty = Clean(4); dirty[1][1] = 1;
        Check(CheckShnPatch(base, cur, dirty, {}).has_value(), "markierte Änderung ist erlaubt");
    }
    {
        auto cur = base;
        cur.rows.erase(cur.rows.begin() + 1);
        Check(!CheckShnPatch(base, cur, Clean(3), {}).has_value(), "gelöschte Zeile ohne Zuordnung wird abgelehnt");
        Check(CheckShnPatch(base, cur, Clean(3), {0, 2, 3}).has_value(), "gelöschte Zeile mit Zuordnung ist erlaubt");
        Check(!CheckShnPatch(base, cur, Clean(3), {0, 1, 2}).has_value(), "falsche Zuordnung fällt als unmarkierte Änderung auf");
        Check(!CheckShnPatch(base, cur, Clean(3), {0, 0, 3}).has_value(), "doppelt zugeordnete Zeile wird abgelehnt");
        Check(!CheckShnPatch(base, cur, Clean(3), {0, 2}).has_value(), "Zuordnung mit falscher Länge wird abgelehnt");
    }
    {
        auto cur = base;
        ShnRow added; added.values = {std::uint32_t{99}, std::string("New")};
        cur.rows.insert(cur.rows.begin() + 2, added);
        auto dirty = Clean(5);
        Check(!CheckShnPatch(base, cur, dirty, {0, 1, -1, 2, 3}).has_value(), "neue Zeile ohne Markierung wird abgelehnt");
        dirty[2] = {1, 1};
        Check(CheckShnPatch(base, cur, dirty, {0, 1, -1, 2, 3}).has_value(), "eingefügte, ganz markierte Zeile ist erlaubt");
    }
    {
        auto cur = base;
        cur.columns[1].name = "Other";
        Check(!CheckShnPatch(base, cur, Clean(4), {}).has_value(), "geändertes Spaltenschema wird abgelehnt");
    }

    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
