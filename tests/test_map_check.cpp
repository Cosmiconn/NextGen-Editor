// test_map_check.cpp
// Kartenprüfung: blockierte Punkte, Spawn-Zonen, Kartengrenzen und SHBD-Laden mit Abmessungen aus
// dem Dateikopf (echte NA2016-Datei Rou.shbd, NPC-Positionen aus World/NPC.txt).

#include "mapeditor/core/MapCheck.hpp"

#include <cstdio>
#include <filesystem>

using namespace theseed::mapeditor::core;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "[FEHLER] %s\n", what);
        ++g_failures;
    } else {
        std::printf("[ok]     %s\n", what);
    }
}

// 4 Wörter breit (64 Zellen) x 64 Zeilen, alles frei; linke Hälfte (Zellen 0..31) blockiert.
WalkGrid HalfBlockedGrid() {
    WalkGrid grid(4, 64, 0);
    for (std::uint32_t z = 0; z < 64; ++z)
        for (std::uint32_t x = 0; x < 32; ++x) grid.SetCellBlocked(x, z, true);
    return grid;
}

void TestPoints() {
    const WalkGrid grid = HalfBlockedGrid();
    const float cell = WalkGrid::kCellSize;
    const MapCheckPoint points[] = {
        {"blocked", 10.0f * cell, 10.0f * cell, 1},
        {"free", 40.0f * cell, 10.0f * cell, 2},
        {"edge", 31.5f * cell, 20.0f * cell, 3},
    };
    std::vector<MapCheckIssue> issues;
    CheckPointsWalkable(grid, points, MapCheckCode::NpcOnBlockedCell, WalkableDistanceSeverity{}, issues);
    Check(issues.size() == 2, "zwei Punkte auf blockierten Zellen");
    Check(issues.size() == 2 && issues[0].severity == MapCheckSeverity::Error, "22 Zellen bis frei: Fehler");
    Check(issues.size() == 2 && issues[1].severity == MapCheckSeverity::Info, "1 Zelle bis frei: Hinweis");
    Check(issues.size() == 2 && issues[0].ref == 1 && issues[1].ref == 3, "richtige Punkte gemeldet");
    Check(issues.size() == 2 && issues[0].hasPosition && issues[0].x == points[0].x, "Position übernommen");
    // Nächste freie Zelle von Zelle 10 aus: Zelle 32 -> Abstand ~22 Zellen.
    Check(issues.size() == 2 && issues[0].value > 21.0 * cell && issues[0].value < 23.0 * cell,
          "Abstand zur nächsten freien Zelle");
    Check(issues.size() == 2 && issues[1].value > 0.0 && issues[1].value < 2.0 * cell, "Randpunkt: freie Zelle direkt daneben");
}

void TestZones() {
    const WalkGrid grid = HalfBlockedGrid();
    const float cell = WalkGrid::kCellSize;
    Check(BlockedFractionInCircle(grid, 10.0f * cell, 32.0f * cell, 5.0f * cell) == 1.0, "Kreis ganz im Block");
    Check(BlockedFractionInCircle(grid, 50.0f * cell, 32.0f * cell, 5.0f * cell) == 0.0, "Kreis ganz frei");
    const double half = BlockedFractionInCircle(grid, 32.0f * cell, 32.0f * cell, 10.0f * cell);
    Check(half > 0.4 && half < 0.6, "Kreis auf der Grenze ~50 %");

    const MapCheckZone zones[] = {
        {"wall", 10.0f * cell, 32.0f * cell, 5.0f * cell, 0},
        {"centerBlocked", 31.0f * cell, 32.0f * cell, 12.0f * cell, 1},
        {"ok", 50.0f * cell, 32.0f * cell, 5.0f * cell, 2},
        {"outside", -100.0f, 32.0f * cell, 5.0f * cell, 3},
    };
    std::vector<MapCheckIssue> issues;
    CheckSpawnZones(grid, zones, MapCheckThresholds{}, issues);
    Check(issues.size() == 3, "drei Zonen gemeldet");
    bool wall = false, center = false, outside = false;
    for (const auto& i : issues) {
        wall |= i.ref == 0 && i.code == MapCheckCode::SpawnZoneMostlyBlocked && i.severity == MapCheckSeverity::Error;
        center |= i.ref == 1 && i.code == MapCheckCode::SpawnZoneCenterBlocked;
        outside |= i.ref == 3 && i.code == MapCheckCode::SpawnZoneOutside;
    }
    Check(wall && center && outside, "Codes der Zonen");
}

void TestInsideMap() {
    const MapCheckPoint points[] = {{"in", 100.0f, 100.0f, 0}, {"out", 5000.0f, 10.0f, 1}, {"slightly", -5.0f, 10.0f, 2}};
    std::vector<MapCheckIssue> issues;
    CheckPointsInsideMap(points, 1000.0f, 1000.0f, 10.0f, MapCheckCode::ObjectOutsideMap, MapCheckSeverity::Warning, issues);
    Check(issues.size() == 1 && issues[0].ref == 1, "nur das Objekt außerhalb der Toleranz");
}

void TestSort() {
    std::vector<MapCheckIssue> issues(3);
    issues[0].severity = MapCheckSeverity::Info;
    issues[1].severity = MapCheckSeverity::Error;
    issues[2].severity = MapCheckSeverity::Warning;
    SortMapCheckIssues(issues);
    Check(issues[0].severity == MapCheckSeverity::Error && issues[2].severity == MapCheckSeverity::Info,
          "Fehler zuerst, Hinweise zuletzt");
}

void TestRealShbd(const std::filesystem::path& shbd) {
    auto grid = ImportLegacyShbdAutoSize(shbd);
    Check(grid.has_value(), "Rou.shbd mit Abmessungen aus dem Dateikopf gelesen");
    if (!grid) return;
    Check(grid->Rows() == 2048 && grid->Cols() == 2048, "Rou: 2048 x 2048 Zellen");
    // NPC-Positionen aus World/NPC.txt, Karte "Rou" (MapInfo: Ordner Rou; "RouN" ist eine eigene
    // Karte mit eigenem SHBD). 35 von 37 Rou-NPCs stehen auf begehbaren Zellen, die übrigen zwei
    // haben Koordinate 0/0.
    const MapCheckPoint npcs[] = {
        {"RouGaianMaria", 4932.0f, 4107.0f, 0},
        {"InvisibleMan", 3507.0f, 6973.0f, 1},
        {"InvisibleMan", 5799.0f, 6223.0f, 2},
    };
    std::vector<MapCheckIssue> issues;
    CheckPointsWalkable(*grid, npcs, MapCheckCode::NpcOnBlockedCell, WalkableDistanceSeverity{}, issues);
    Check(issues.empty(), "Original-NPCs von Roumen stehen auf begehbaren Zellen");
    Check(!ImportLegacyShbdAutoSize(shbd.parent_path() / "gibt_es_nicht.shbd").has_value(), "fehlende Datei -> Fehler");
}

} // namespace

int main(int argc, char** argv) {
    TestPoints();
    TestZones();
    TestInsideMap();
    TestSort();
    if (argc > 1) TestRealShbd(argv[1]);
    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
