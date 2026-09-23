// test_walk_grid.cpp
// GUI-freier Test. Prüft WalkGrid-Grundfunktionen, Stamp+Undo/Redo, .tswalk-Roundtrip und
// den Legacy-Import/Export gegen die echte Rou.shbd (Byte-für-Byte-Vergleich, analog zum
// Heightmap-Test).

#include "mapeditor/core/WalkEditOps.hpp"
#include "mapeditor/core/WalkGrid.hpp"
#include "mapeditor/core/WalkGridIO.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

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

void TestBasicWalkGrid() {
    WalkGrid grid(4, 4); // Default-Fill: -1
    Check(grid.At(0, 0) == -1, "Neues Gitter startet mit -1 (\"frei/unbelegt\")");
    grid.Set(1, 1, 0x01FF);
    Check(grid.At(1, 1) == static_cast<std::int16_t>(0x01FF), "Set/At funktioniert mit Bitmask-Werten");
}

void TestStampAndUndo() {
    WalkGrid grid(9, 9);
    WalkUndoStack undo;

    WalkStampSettings settings;
    settings.radius = 120.0f;
    settings.value = 0;

    auto patch = ApplyWalkStamp(grid, settings, 200.0f, 200.0f, 50.0f, 50.0f);
    undo.Push(patch);
    Check(grid.At(4, 4) == 0, "Stamp setzt Zellen im Radius auf den Zielwert");

    Check(undo.Undo(grid), "Undo erfolgreich");
    Check(grid.At(4, 4) == -1, "Undo stellt Ausgangswert (-1) wieder her");

    Check(undo.Redo(grid), "Redo erfolgreich");
    Check(grid.At(4, 4) == 0, "Redo stellt gestempelten Wert wieder her");
}

void TestCellLevel() {
    using namespace theseed::mapeditor::core;
    // Zell-Ebene (seit [0.44.29]): 16 Zellen je Wort, Bit 0 = linkeste Zelle, Bit gesetzt = blockiert.
    WalkGrid grid(4, 8, 0); // 64 x 8 Zellen, alles begehbar
    Check(grid.Cols() == 64 && grid.Rows() == 8, "Cols/Rows: 4 Woerter = 64 Zellen");
    grid.SetCellBlocked(17, 3, true);
    Check(static_cast<std::uint16_t>(grid.At(1, 3)) == 0x0002, "Zelle 17 = Bit 1 im Wort 1 (LSB-zuerst)");
    Check(grid.CellBlocked(17, 3) && !grid.CellBlocked(16, 3) && !grid.CellBlocked(18, 3), "nur die eine Zelle ist blockiert");
    grid.SetCellBlocked(17, 3, false);
    Check(grid.At(1, 3) == 0, "Freigeben loescht das Bit wieder");

    // Kreis-Stempel: Radius 1 an einer Zellmitte trifft genau EINE Zelle (Zelle = 6.25 Einheiten).
    WalkGrid g2(4, 8, 0);
    std::uint32_t changed[4] = {};
    auto patch = ApplyWalkBitStamp(g2, 1.0f, 5.0f * WalkGrid::kCellSize + 3.0f, 2.0f * WalkGrid::kCellSize + 3.0f, true, changed);
    Check(g2.CellBlocked(5, 2) && !g2.CellBlocked(4, 2) && !g2.CellBlocked(6, 2), "Radius 1 sperrt genau die Zelle unter dem Zeiger");
    Check(changed[0] == 5 && changed[2] == 5 && changed[1] == 2 && changed[3] == 2, "Aenderungs-Rechteck stimmt");
    RevertWalkPatch(g2, patch);
    Check(g2.At(0, 2) == 0, "Undo stellt das Wort wieder her");

    // Konvexes Polygon: Quadrat 10x10 Zellen -> 100 Zellen; erneutes Anwenden aendert nichts.
    WalkGrid g3(8, 32, 0);
    const float a = 2.0f * WalkGrid::kCellSize, b = 12.0f * WalkGrid::kCellSize;
    std::vector<std::pair<float, float>> square = {{a, a}, {b, a}, {b, b}, {a, b}};
    WalkUndoPatch pp;
    std::vector<std::uint64_t> seen;
    ApplyWalkConvexPolygon(g3, square, true, pp, seen);
    int blockedCells = 0;
    for (std::uint32_t z = 0; z < g3.Rows(); ++z) for (std::uint32_t x = 0; x < g3.Cols(); ++x) blockedCells += g3.CellBlocked(x, z) ? 1 : 0;
    Check(blockedCells == 100, "Polygon-Stempel sperrt 10x10 = 100 Zellen");
    WalkUndoPatch pp2;
    std::vector<std::uint64_t> seen2;
    ApplyWalkConvexPolygon(g3, square, true, pp2, seen2);
    Check(pp2.entries.empty(), "zweites Anwenden aendert nichts");
}

void TestTswalkRoundtrip() {
    WalkGrid grid(5, 3);
    grid.Set(2, 1, static_cast<std::int16_t>(0xFFE0));

    const auto path = std::filesystem::temp_directory_path() / "map_editor_tswalk_test.tswalk";
    auto saveResult = SaveTsWalk(grid, path);
    Check(saveResult.has_value(), "SaveTsWalk erfolgreich");

    auto loadResult = LoadTsWalk(path);
    Check(loadResult.has_value(), "LoadTsWalk erfolgreich");
    if (loadResult) {
        Check(loadResult->Width() == grid.Width() && loadResult->Height() == grid.Height(), "Dimensionen nach Roundtrip identisch");
        Check(loadResult->At(2, 1) == grid.At(2, 1), "Bitmask-Wert nach Roundtrip identisch");
    }
    std::filesystem::remove(path);
}

void TestLegacyShbdRoundtrip(const std::filesystem::path& shbdPath) {
    // KORRIGIERT: Gitter ist NICHT quadratisch - echte Rou.shbd ist 128x2048, nicht 512x512.
    // Die vorherige "512x512"-Annahme bestand nur den Gesamt-Byte-Test (512*512 == 128*2048 ==
    // 262144), verifizierte aber nie die tatsächliche Breite/Höhe-Aufteilung - siehe
    // docs/MAP_FORMAT.md und CHANGELOG.md (v0.11.0) für die Korrektur-Herleitung.
    constexpr std::uint32_t kWidth = 128;
    constexpr std::uint32_t kHeight = 2048;

    LegacyShbdHeader header{};
    auto imported = ImportLegacyShbd(shbdPath, kWidth, kHeight, &header);
    Check(imported.has_value(), "ImportLegacyShbd(Rou.shbd) erfolgreich (512x512 passt exakt zur Dateigr\u00f6\u00dfe)");
    if (!imported) return;

    std::printf("         Header-Bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                header.raw[0], header.raw[1], header.raw[2], header.raw[3],
                header.raw[4], header.raw[5], header.raw[6], header.raw[7]);

    const auto exportPath = std::filesystem::temp_directory_path() / "map_editor_legacy_shbd_export_test.shbd";
    auto exportResult = ExportLegacyShbd(*imported, exportPath, header);
    Check(exportResult.has_value(), "ExportLegacyShbd erfolgreich");

    std::ifstream original(shbdPath, std::ios::binary);
    std::ifstream exported(exportPath, std::ios::binary);
    const std::vector<char> originalBytes((std::istreambuf_iterator<char>(original)), std::istreambuf_iterator<char>());
    const std::vector<char> exportedBytes((std::istreambuf_iterator<char>(exported)), std::istreambuf_iterator<char>());

    Check(originalBytes.size() == exportedBytes.size(), "Exportierte Datei hat identische Gr\u00f6\u00dfe wie Original");
    Check(originalBytes == exportedBytes, "Exportierte Datei ist BYTE-F\u00dcR-BYTE IDENTISCH zum Original (Rou.shbd)");

    std::filesystem::remove(exportPath);
}

} // namespace

int main(int argc, char** argv) {
    std::printf("== WalkGrid Core Tests ==\n");
    TestBasicWalkGrid();
    TestStampAndUndo();
    TestCellLevel();
    TestTswalkRoundtrip();

    if (argc >= 2) {
        std::printf("\n== Legacy-shbd-Roundtrip (Byte-f\u00fcr-Byte gegen echte Datei) ==\n");
        TestLegacyShbdRoundtrip(argv[1]);
    } else {
        std::printf("\n(Legacy-shbd-Test \u00fcbersprungen - Aufruf mit: %s <Rou.shbd>)\n", argv[0]);
    }

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
