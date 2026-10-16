// test_walk_generate.cpp
// SHBD-Neuberechnung: Hangneigung, Höhengrenzen, Änderungsbereich, Vergleich; mit echter
// NA2016-Karte Rou (Rou.HTD/Rou.shbd): unverändertes Terrain lässt das Original unangetastet.

#include "mapeditor/core/WalkGenerate.hpp"
#include "mapeditor/core/MapCheck.hpp"
#include "mapeditor/core/legacy/LegacyMapProject.hpp"

#include <cmath>
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

// 17x17 Punkte à 50 Einheiten = 800x800; Rampe mit 45° in der rechten Hälfte.
Heightmap RampMap() {
    Heightmap h(17, 17, 50.0f, 50.0f);
    for (std::uint32_t z = 0; z < 17; ++z)
        for (std::uint32_t x = 0; x < 17; ++x) h.Set(x, z, x >= 8 ? static_cast<float>(x - 8) * 50.0f : 0.0f);
    return h;
}

void TestSlope() {
    const Heightmap h = RampMap();
    Check(std::abs(TerrainSlopeDegrees(h, 200.0f, 400.0f)) < 0.1f, "flach: 0°");
    Check(std::abs(TerrainSlopeDegrees(h, 600.0f, 400.0f) - 45.0f) < 0.5f, "Rampe: 45°");
    WalkGenerateSettings s;
    s.maxSlopeDegrees = 30.0f;
    // Zelle 32 = x 200..206 (flach), Zelle 96 = x 600 (Rampe), Zelle 200 = x 1250 (außerhalb)
    Check(!TerrainBlocksCell(h, s, 32, 64), "flache Zelle frei");
    Check(TerrainBlocksCell(h, s, 96, 64), "steile Zelle blockiert");
    Check(TerrainBlocksCell(h, s, 200, 64), "Zelle außerhalb der Terrainfläche blockiert");
    s.maxSlopeDegrees = 50.0f;
    Check(!TerrainBlocksCell(h, s, 96, 64), "Schwelle 50°: Rampe frei");
    s.useMaxHeight = true;
    s.maxHeight = 100.0f;
    Check(TerrainBlocksCell(h, s, 112, 64), "über Maximalhöhe blockiert");
    s.useMaxHeight = false;
    s.useMinHeight = true;
    s.minHeight = 10.0f;
    Check(TerrainBlocksCell(h, s, 32, 64), "unter Mindesthöhe blockiert");
}

void TestRegionAndGenerate() {
    const Heightmap before(17, 17, 50.0f, 50.0f);
    Heightmap after = before;
    after.Set(8, 8, 200.0f); // spitzer Hügel in der Mitte
    WalkGrid base(8, 128, 0); // 128x128 Zellen = 800x800, alles frei
    const WalkRegion none = TerrainChangeRegion(base, before, before);
    Check(RegionCellCount(none) == 0 && none.size() == 128u * 128u, "ohne Änderung leere Region");
    Check(DiffWalkGrids(base, GenerateWalkGrid(base, after, WalkGenerateSettings{}, &none, WalkGrid{})).Agreement() == 1.0,
          "leere Region ändert nichts");
    const WalkRegion region = TerrainChangeRegion(base, before, after);
    const std::size_t count = RegionCellCount(region);
    Check(count > 0 && count < 128u * 128u / 4, "Region umfasst nur die Umgebung des Hügels");
    // Handarbeit außerhalb der Region bleibt erhalten.
    base.SetCellBlocked(5, 5, true);
    const WalkGrid generated = GenerateWalkGrid(base, after, WalkGenerateSettings{}, &region, WalkGrid{});
    Check(generated.CellBlocked(5, 5), "Zelle außerhalb der Region unverändert");
    Check(generated.CellBlocked(62, 62), "Hang des Hügels blockiert");
    const WalkGridDiff diff = DiffWalkGrids(base, generated);
    Check(diff.newlyBlocked > 0 && diff.newlyFree == 0, "nur neu blockierte Zellen");
    // Änderungsbezogen: Hügel wieder abgetragen -> Hangzellen frei, von Hand gesperrtes "Meer" bleibt.
    WalkGrid withHill = UpdateWalkGridForChanges(base, before, after, WalkGenerateSettings{}, region, WalkGrid{}, WalkGrid{});
    Check(withHill.CellBlocked(62, 62) && withHill.CellBlocked(5, 5), "Änderung: Hang gesperrt, Handarbeit erhalten");
    withHill.SetCellBlocked(54, 64, true); // "Meer" am Rand der Region, flach
    const WalkGrid flattened = UpdateWalkGridForChanges(withHill, after, before, WalkGenerateSettings{}, region, WalkGrid{}, WalkGrid{});
    Check(!flattened.CellBlocked(62, 62), "abgetragener Hang wieder frei");
    Check(flattened.CellBlocked(54, 64), "flache, von Hand gesperrte Zelle bleibt gesperrt");
    WalkGrid removed(8, 128, 0);
    removed.SetCellBlocked(54, 64, true);
    const WalkGrid objectGone = UpdateWalkGridForChanges(withHill, after, before, WalkGenerateSettings{}, region, WalkGrid{}, removed);
    Check(!objectGone.CellBlocked(54, 64), "Grundfläche eines entfernten Objekts wieder frei");

    // Objekt-Grundflächen werden ergänzt.
    WalkGrid extra(8, 128, 0);
    extra.SetCellBlocked(64, 20, true);
    const WalkGrid withExtra = GenerateWalkGrid(base, before, WalkGenerateSettings{}, nullptr, extra);
    Check(withExtra.CellBlocked(64, 20) && !withExtra.CellBlocked(5, 5), "ganze Karte: Grundfläche gesperrt, flach frei");
}

void TestReal(const std::filesystem::path& ini) {
    auto map = legacy::OpenLegacyMap(ini);
    Check(map.has_value() && map->hasHeightmap && map->hasWalkGrid, "Rou geladen");
    if (!map || !map->hasHeightmap || !map->hasWalkGrid) return;
    const WalkRegion region = TerrainChangeRegion(map->walkGrid, map->heightmap, map->heightmap);
    const WalkGrid same = GenerateWalkGrid(map->walkGrid, map->heightmap, WalkGenerateSettings{}, &region, WalkGrid{});
    const WalkGridDiff unchanged = DiffWalkGrids(map->walkGrid, same);
    Check(unchanged.newlyBlocked == 0 && unchanged.newlyFree == 0, "Rou unverändert: Original bleibt bitgleich");
    // Ganze Karte nur aus Hängen: wie gut treffen 30° die steilen Originalzellen?
    std::size_t steep = 0, steepBlocked = 0;
    for (std::uint32_t z = 0; z < map->walkGrid.Rows(); z += 4)
        for (std::uint32_t x = 0; x < map->walkGrid.Cols(); x += 4)
            if (TerrainBlocksCell(map->heightmap, WalkGenerateSettings{}, x, z)) {
                ++steep;
                steepBlocked += map->walkGrid.CellBlocked(x, z) ? 1 : 0;
            }
    const double share = steep ? static_cast<double>(steepBlocked) / static_cast<double>(steep) : 0.0;
    std::printf("         Rou: %zu Zellen steiler als 30°, davon %.1f %% im Original blockiert\n", steep, share * 100.0);
    Check(share > 0.95, "steiler als 30° ist im Original fast immer blockiert");
}

} // namespace

int main(int argc, char** argv) {
    TestSlope();
    TestRegionAndGenerate();
    if (argc > 1) TestReal(argv[1]);
    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
