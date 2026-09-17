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
