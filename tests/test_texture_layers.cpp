// test_texture_layers.cpp
// GUI-freier Test, baubar direkt mit g++ (siehe README.md). Prüft:
//   1) TextureLayerStack Grundfunktionen
//   2) PaintLayerWeight inkl. Normalisierungs-Invariante + Undo/Redo
//   3) .tstex Save/Load-Roundtrip
//   4) LegacyMapIni-Parser gegen den echten Inhalt der hochgeladenen Rou.ini
//      (tests/fixtures/Rou.ini - inhaltlich identisch, Kommentartexte vereinfacht, da das
//      Original nicht UTF-8-kodiert war; das Parsing ist davon nicht betroffen, Kommentare
//      werden ohnehin verworfen)

#include "mapeditor/core/TextureLayerIO.hpp"
#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/TexturePaintOps.hpp"
#include "mapeditor/core/legacy/LegacyMapIni.hpp"

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

void TestLayerStackBasics() {
    TextureLayerStack stack(4, 4);
    const auto base = stack.AddLayer("Base", "base.dds", 4.0f);
    Check(stack.LayerCount() == 1, "AddLayer erh\u00f6ht LayerCount");
    Check(stack.Layer(base).blend.At(0, 0) == 1.0f, "Erster Layer startet voll belegt (1.0)");

    const auto second = stack.AddLayer("Rock", "rock.dds", 5.0f);
    Check(stack.Layer(second).blend.At(0, 0) == 0.0f, "Zweiter Layer startet leer (0.0)");
    Check(std::abs(stack.WeightSumAt(0, 0) - 1.0f) < 1e-6f, "Gewichtssumme direkt nach Anlage = 1.0");
}

void TestPaintNormalization() {
    TextureLayerStack stack(9, 9);
    stack.AddLayer("Base", "base.dds");
    const auto rock = stack.AddLayer("Rock", "rock.dds");

    TexturePaintUndoStack undo;
    TexturePaintSettings settings;
    settings.radius = 300.0f;
    settings.strength = 0.8f;

    for (int i = 0; i < 5; ++i) {
        auto patch = PaintLayerWeight(stack, rock, PaintMode::Increase, settings, 200.0f, 200.0f, 50.0f, 50.0f);
        undo.Push(std::move(patch));
    }

    const float rockWeight = stack.Layer(rock).blend.At(4, 4);
    Check(rockWeight > 0.9f, "Rock-Layer nach mehrfachem Malen stark erh\u00f6ht");
    Check(std::abs(stack.WeightSumAt(4, 4) - 1.0f) < 1e-4f, "Gewichtssumme bleibt nach Malen ~1.0 (Normalisierung)");

    Check(undo.Undo(stack), "Undo (Textur) erfolgreich");
    Check(std::abs(stack.WeightSumAt(4, 4) - 1.0f) < 1e-4f, "Gewichtssumme bleibt auch nach Undo ~1.0");
}

void TestTstexRoundtrip() {
    TextureLayerStack stack(5, 3);
    stack.AddLayer("Base", "base.dds", 4.0f);
    const auto rock = stack.AddLayer("Rock", "rock.dds", 5.0f);
    stack.Layer(rock).blend.Set(2, 1, 0.5f);

    const auto tmpPath = std::filesystem::temp_directory_path() / "map_editor_tstex_test.tstex";
    auto saveResult = SaveTsTex(stack, tmpPath);
    Check(saveResult.has_value(), "SaveTsTex erfolgreich");

    auto loadResult = LoadTsTex(tmpPath);
    Check(loadResult.has_value(), "LoadTsTex erfolgreich");
    if (loadResult) {
        Check(loadResult->LayerCount() == 2, "LayerCount nach Roundtrip identisch");
        Check(loadResult->Layer(1).name == "Rock", "Layer-Name nach Roundtrip identisch");
        Check(std::abs(loadResult->Layer(1).blend.At(2, 1) - 0.5f) < 0.01f, "Blend-Gewicht nach Roundtrip identisch (Quantisierungsfehler < 0.01)");
    }
    std::filesystem::remove(tmpPath);
}

void TestLegacyIniParser(const std::filesystem::path& iniPath) {
    auto result = legacy::ParseLegacyMapIni(iniPath);
    Check(result.has_value(), "ParseLegacyMapIni(Rou.ini) erfolgreich");
    if (!result) {
        std::fprintf(stderr, "         Fehler: %s\n", result.error().c_str());
        return;
    }

    const auto& ini = *result;
    Check(ini.heightmapWidth == 257 && ini.heightmapHeight == 257, "Heightmap-Dimensionen korrekt geparst (257x257)");
    Check(std::abs(ini.oneBlockWidth - 50.0f) < 0.001f, "OneBlockWidth korrekt geparst (50.0)");
    Check(ini.quadsWide == 64 && ini.quadsHigh == 64, "QuadsWide/High korrekt geparst (64x64)");
    Check(ini.heightFileName == ".\\resmap\\field\\Rou\\Rou.HTD", "HeightFileName korrekt geparst (inkl. Backslashes)");
    Check(ini.layers.size() == 4, "Alle 4 Layer-Bl\u00f6cke gefunden");

    if (ini.layers.size() == 4) {
        Check(ini.layers[0].name == "01 ground", "Layer 0 Name korrekt");
        Check(ini.layers[0].diffuseFileName == ".\\resmap\\field\\Rou\\grasst1.dds", "Layer 0 DiffuseFileName korrekt");
        Check(std::abs(ini.layers[0].uvScaleDiffuse - 4.0f) < 0.001f, "Layer 0 UVScaleDiffuse korrekt (4f -> 4.0)");
        Check(ini.layers[3].name == "04 grass", "Layer 3 Name korrekt");
        Check(std::abs(ini.layers[3].uvScaleDiffuse - 5.0f) < 0.001f, "Layer 3 UVScaleDiffuse korrekt (5f -> 5.0)");
    }

    auto stack = BuildTextureLayerStackFromLegacyIni(ini);
    Check(stack.LayerCount() == 4, "TextureLayerStack aus Legacy-ini hat 4 Layer");
    Check(stack.Width() == 512 && stack.Height() == 512, "TextureLayerStack nutzt unabhängige Platzhalter-Auflösung (512x512), NICHT die Heightmap-Auflösung (korrigiert - siehe docs/MAP_FORMAT.md)");
    Check(std::abs(stack.WeightSumAt(0, 0) - 1.0f) < 1e-6f, "Basis-Layer nach Konvertierung voll belegt (Summe 1.0)");

    // Werte-Roundtrip: parse -> serialize -> reparse muss inhaltlich identisch sein
    // (NICHT byte-identisch zum Original - Kommentare/Whitespace gehen bewusst verloren).
    const auto reserializedPath = std::filesystem::temp_directory_path() / "map_editor_ini_roundtrip_test.ini";
    auto serializeResult = legacy::SerializeLegacyMapIni(ini, reserializedPath);
    Check(serializeResult.has_value(), "SerializeLegacyMapIni erfolgreich");

    auto reparsed = legacy::ParseLegacyMapIni(reserializedPath);
    Check(reparsed.has_value(), "Reparse der serialisierten .ini erfolgreich");
    if (reparsed) {
        Check(reparsed->heightmapWidth == ini.heightmapWidth && reparsed->heightmapHeight == ini.heightmapHeight,
              "Reparse: Heightmap-Dimensionen identisch zum Original-Parse");
        Check(reparsed->heightFileName == ini.heightFileName, "Reparse: HeightFileName identisch");
        Check(reparsed->layers.size() == ini.layers.size(), "Reparse: gleiche Layer-Anzahl");
        bool layersMatch = reparsed->layers.size() == ini.layers.size();
        for (std::size_t i = 0; layersMatch && i < ini.layers.size(); ++i) {
            layersMatch = reparsed->layers[i].name == ini.layers[i].name &&
                          reparsed->layers[i].diffuseFileName == ini.layers[i].diffuseFileName &&
                          std::abs(reparsed->layers[i].uvScaleDiffuse - ini.layers[i].uvScaleDiffuse) < 0.001f;
        }
        Check(layersMatch, "Reparse: alle Layer-Metadaten wertgleich zum Original-Parse");
    }
    std::filesystem::remove(reserializedPath);
}

} // namespace

int main() {
    std::printf("== TextureLayerStack / TexturePaintOps / .tstex Tests ==\n");
    TestLayerStackBasics();
    TestPaintNormalization();
    TestTstexRoundtrip();

    std::printf("\n== Legacy-ini-Parser gegen echten Rou.ini-Inhalt ==\n");
    TestLegacyIniParser("tests/fixtures/Rou.ini");

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
