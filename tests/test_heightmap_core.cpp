// test_heightmap_core.cpp
// Eigenständiger Smoke-/Regressionstest ohne GUI-Abhängigkeiten (kein GLFW/ImGui nötig).
// Baubar direkt mit g++, siehe README.md. Prüft:
//   1) Heightmap-Grundfunktionen (At/Set/SampleWorld/MinMax)
//   2) .tshm Save/Load-Roundtrip
//   3) Legacy-Import gegen die echten, vom Nutzer bereitgestellten Rou.HTD/Rou.HTDG-Dateien
//      (Pfad wird als Kommandozeilenargument übergeben, Test wird sonst übersprungen).

#include "mapeditor/core/EditOps.hpp"
#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/HeightmapIO.hpp"

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

void TestBasicHeightmap() {
    Heightmap hm(4, 4, 50.0f, 50.0f);
    hm.Set(1, 1, 10.0f);
    hm.Set(2, 1, 20.0f);
    hm.Set(1, 2, 30.0f);
    hm.Set(2, 2, 40.0f);

    Check(hm.At(1, 1) == 10.0f, "At() liefert gesetzten Wert zurück");

    // SampleWorld auf exaktem Vertex 1,1 -> 50,50
    Check(hm.SampleWorld(50.0f, 50.0f) == 10.0f, "SampleWorld exakt auf Vertex");

    // Bilinear in der Mitte des Quads (1,1)-(2,2) -> Mittelwert der 4 Ecken
    const float mid = hm.SampleWorld(75.0f, 75.0f);
    Check(std::abs(mid - 25.0f) < 0.001f, "SampleWorld bilinear im Quad-Zentrum");

    const auto [lo, hi] = hm.MinMax();
    Check(lo == 0.0f && hi == 40.0f, "MinMax über gemischtes Gitter");
}

void TestUndoRedo() {
    Heightmap hm(9, 9, 50.0f, 50.0f);
    UndoStack undo;

    BrushSettings settings;
    settings.radius = 120.0f;
    settings.strength = 10.0f;

    const float before = hm.At(4, 4);
    UndoPatch patch = ApplyBrush(hm, BrushMode::Raise, settings, 200.0f, 200.0f);
    undo.Push(patch);
    const float afterRaise = hm.At(4, 4);
    Check(afterRaise > before, "Raise-Pinsel erhöht Höhe im Zentrum");

    Check(undo.Undo(hm), "Undo erfolgreich");
    Check(hm.At(4, 4) == before, "Undo stellt Ausgangswert wieder her");

    Check(undo.Redo(hm), "Redo erfolgreich");
    Check(hm.At(4, 4) == afterRaise, "Redo stellt bearbeiteten Wert wieder her");
}

void TestFlattenConverges() {
    Heightmap hm(9, 9, 50.0f, 50.0f);
    for (auto& v : hm.MutableData()) v = 100.0f;

    BrushSettings settings;
    settings.radius = 300.0f;
    settings.strength = 100.0f; // starker Blend -> soll sich schnell an flattenTarget annähern
    settings.flattenTarget = 0.0f;

    for (int i = 0; i < 20; ++i) {
        ApplyBrush(hm, BrushMode::Flatten, settings, 200.0f, 200.0f);
    }
    Check(std::abs(hm.At(4, 4) - 0.0f) < 1.0f, "Flatten konvergiert gegen Zielhöhe");
}

void TestTshmRoundtrip() {
    Heightmap hm(5, 3, 25.0f, 30.0f);
    for (std::uint32_t z = 0; z < hm.Height(); ++z) {
        for (std::uint32_t x = 0; x < hm.Width(); ++x) {
            hm.Set(x, z, static_cast<float>(x) * 1.5f - static_cast<float>(z));
        }
    }

    const auto tmpPath = std::filesystem::temp_directory_path() / "map_editor_roundtrip_test.tshm";
    auto saveResult = SaveTshm(hm, tmpPath);
    Check(saveResult.has_value(), "SaveTshm erfolgreich");

    auto loadResult = LoadTshm(tmpPath);
    Check(loadResult.has_value(), "LoadTshm erfolgreich");
    if (loadResult) {
        const Heightmap& loaded = *loadResult;
        Check(loaded.Width() == hm.Width() && loaded.Height() == hm.Height(), "Dimensionen nach Roundtrip identisch");
        bool dataMatches = true;
        for (std::uint32_t z = 0; z < hm.Height() && dataMatches; ++z) {
            for (std::uint32_t x = 0; x < hm.Width() && dataMatches; ++x) {
                dataMatches = (loaded.At(x, z) == hm.At(x, z));
            }
        }
        Check(dataMatches, "Höhenwerte nach Roundtrip identisch");
    }
    std::filesystem::remove(tmpPath);
}

void TestLegacyImport(const std::filesystem::path& htdPath, const std::filesystem::path& htdgPath) {
    // Dimensionen laut mitgelieferter Rou.ini: 257 x 257, Blockgröße 50.0f
    constexpr std::uint32_t kWidth = 257;
    constexpr std::uint32_t kHeight = 257;

    LegacyHtdHeader header{};
    auto htd = ImportLegacyHtd(htdPath, kWidth, kHeight, 50.0f, 50.0f, &header);
    Check(htd.has_value(), "ImportLegacyHtd(Rou.HTD) erfolgreich (Dimensionen passen exakt zur Dateigröße)");
    if (htd) {
        std::printf("         Header-Bytes: %02X %02X %02X %02X\n", header.raw[0], header.raw[1], header.raw[2], header.raw[3]);
        const auto [lo, hi] = htd->MinMax();
        std::printf("         Höhen-Range in Rou.HTD: [%.3f, %.3f]\n", lo, hi);
        Check(hi > lo, "Rou.HTD enthält nicht-triviale Höhendaten (nicht nur Nullen)");
    }

    auto htdg = ImportLegacyHtd(htdgPath, kWidth, kHeight, 50.0f, 50.0f);
    Check(htdg.has_value(), "ImportLegacyHtd(Rou.HTDG) erfolgreich (gleiche Dimensionen/Größe wie .HTD)");
    if (htdg && htd) {
        // Prüfen, ob HTDG identisch zu HTD ist (Arbeitskopie) oder tatsächlich andere Werte
        // enthält (z.B. Gradient/Backup) - reines Beobachtungsergebnis, kein Pass/Fail-Kriterium.
        int diffCount = 0;
        for (std::size_t i = 0; i < htd->Data().size(); ++i) {
            if (htd->Data()[i] != htdg->Data()[i]) ++diffCount;
        }
        std::printf("         Rou.HTD vs Rou.HTDG: %d von %zu Werten unterschiedlich (%.1f%%)\n",
                    diffCount, htd->Data().size(), 100.0 * diffCount / static_cast<double>(htd->Data().size()));
    }
}

void TestLegacyExportRoundtrip(const std::filesystem::path& htdPath) {
    constexpr std::uint32_t kWidth = 257;
    constexpr std::uint32_t kHeight = 257;

    LegacyHtdHeader header{};
    auto imported = ImportLegacyHtd(htdPath, kWidth, kHeight, 50.0f, 50.0f, &header);
    Check(imported.has_value(), "Import für Export-Roundtrip-Test erfolgreich");
    if (!imported) return;

    const auto exportPath = std::filesystem::temp_directory_path() / "map_editor_legacy_export_test.HTD";
    auto exportResult = ExportLegacyHtd(*imported, exportPath, header);
    Check(exportResult.has_value(), "ExportLegacyHtd erfolgreich");

    // Byte-für-Byte-Vergleich der exportierten Datei gegen das Original.
    std::ifstream original(htdPath, std::ios::binary);
    std::ifstream exported(exportPath, std::ios::binary);
    Check(static_cast<bool>(original) && static_cast<bool>(exported), "Beide Dateien zum Vergleich öffenbar");

    const std::vector<char> originalBytes((std::istreambuf_iterator<char>(original)), std::istreambuf_iterator<char>());
    const std::vector<char> exportedBytes((std::istreambuf_iterator<char>(exported)), std::istreambuf_iterator<char>());

    Check(originalBytes.size() == exportedBytes.size(), "Exportierte Datei hat identische Größe wie Original");
    Check(originalBytes == exportedBytes, "Exportierte Datei ist BYTE-FÜR-BYTE IDENTISCH zum Original (Rou.HTD)");

    std::filesystem::remove(exportPath);
}

} // namespace

int main(int argc, char** argv) {
    std::printf("== Heightmap Core Tests ==\n");
    TestBasicHeightmap();
    TestUndoRedo();
    TestFlattenConverges();
    TestTshmRoundtrip();

    if (argc >= 3) {
        std::printf("\n== Legacy-Import gegen echte Referenzdateien ==\n");
        TestLegacyImport(argv[1], argv[2]);
        std::printf("\n== Legacy-EXPORT-Roundtrip (Byte-für-Byte gegen Original) ==\n");
        TestLegacyExportRoundtrip(argv[1]);
    } else {
        std::printf("\n(Legacy-Import/Export-Test übersprungen - Aufruf mit: %s <Rou.HTD> <Rou.HTDG>)\n", argv[0]);
    }

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
