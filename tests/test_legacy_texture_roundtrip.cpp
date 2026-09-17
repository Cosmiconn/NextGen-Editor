// test_legacy_texture_roundtrip.cpp
// GUI-freier Test. Prüft:
//   1) BmpBlendMap-Codec im Selbsttest (Write->Read, keine Original-.BMP verfügbar - siehe
//      docs/MAP_FORMAT.md für die entsprechende Einschränkung)
//   2) Kompletter Legacy-Texturing-Rundlauf: echte Rou.ini parsen -> Blend-Gewichte malen ->
//      als komplettes Legacy-Set (ini + BMPs) exportieren -> wieder importieren -> vergleichen

#include "mapeditor/core/TexturePaintOps.hpp"
#include "mapeditor/core/legacy/BmpBlendMap.hpp"
#include "mapeditor/core/legacy/LegacyTextureSetIO.hpp"

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

void TestBmpCodecSelfRoundtrip() {
    BlendMap blend(64, 48);
    for (std::uint32_t z = 0; z < blend.Height(); ++z) {
        for (std::uint32_t x = 0; x < blend.Width(); ++x) {
            // Diagonaler Verlauf, deckt den vollen 0..1-Bereich ab.
            const float v = static_cast<float>(x + z) / static_cast<float>(blend.Width() + blend.Height() - 2);
            blend.Set(x, z, v);
        }
    }

    const auto path = std::filesystem::temp_directory_path() / "map_editor_bmp_selftest.bmp";
    auto writeResult = legacy::WriteBlendMapBmp(blend, path);
    Check(writeResult.has_value(), "WriteBlendMapBmp erfolgreich");

    auto readResult = legacy::ReadBlendMapBmp(path);
    Check(readResult.has_value(), "ReadBlendMapBmp erfolgreich");
    if (readResult) {
        Check(readResult->Width() == blend.Width() && readResult->Height() == blend.Height(),
              "BMP-Roundtrip: Dimensionen identisch");
        bool allClose = true;
        float maxDiff = 0.0f;
        for (std::uint32_t z = 0; z < blend.Height() && allClose; ++z) {
            for (std::uint32_t x = 0; x < blend.Width() && allClose; ++x) {
                const float diff = std::abs(readResult->At(x, z) - blend.At(x, z));
                maxDiff = std::max(maxDiff, diff);
                if (diff > 1.0f / 255.0f + 1e-4f) allClose = false; // 8-bit-Quantisierungstoleranz
            }
        }
        std::printf("         Max. Abweichung nach BMP-Roundtrip: %.5f (Toleranz: %.5f)\n", maxDiff, 1.0f / 255.0f);
        Check(allClose, "BMP-Roundtrip: alle Werte innerhalb der 8-bit-Quantisierungstoleranz");
    }
    std::filesystem::remove(path);
}

void TestFullLegacyTextureRoundtrip(const std::filesystem::path& iniPath) {
    // 1) Echte Rou.ini importieren (ohne Blend-BMPs - die gibt es nicht, siehe report).
    legacy::TextureSetImportReport importReport;
    auto stackResult = legacy::ImportLegacyTextureSet(iniPath, &importReport);
    Check(stackResult.has_value(), "ImportLegacyTextureSet(Rou.ini) erfolgreich");
    if (!stackResult) return;

    std::printf("         Fehlende Blend-BMPs beim Import (erwartet, da nicht bereitgestellt): %zu von %zu Layern\n",
                importReport.missingBlendFiles.size(), stackResult->LayerCount());
    // +1: seit der Aufloesungs-Entkopplung von der Heightmap wird zusaetzlich ein Hinweis
    // eingetragen, wenn mangels lesbarer BMP eine Platzhalter-Aufloesung geraten wurde.
    Check(importReport.missingBlendFiles.size() == stackResult->LayerCount() + 1,
          "Alle 4 Blend-BMPs korrekt als fehlend erkannt + Fallback-Aufl\u00f6sungs-Hinweis (keine BMP im Referenzdatensatz enthalten)");
    Check(stackResult->Width() == 512 && stackResult->Height() == 512,
          "Ohne lesbare BMP wird die dokumentierte Platzhalter-Aufl\u00f6sung 512x512 verwendet (NICHT die Heightmap-Aufl\u00f6sung 257x257)");

    // 2) Ein paar Gewichte "malen", damit der Rundlauf nicht nur Nullen transportiert.
    TexturePaintSettings settings;
    settings.radius = 400.0f;
    settings.strength = 0.9f;
    for (std::size_t layer = 1; layer < stackResult->LayerCount(); ++layer) {
        PaintLayerWeight(*stackResult, layer, PaintMode::Increase, settings,
                          static_cast<float>(layer) * 2000.0f, 3000.0f, 50.0f, 50.0f);
    }

    // 3) Als komplettes Legacy-Set exportieren (ini + BMPs).
    auto iniMetaResult = legacy::ParseLegacyMapIni(iniPath);
    Check(iniMetaResult.has_value(), "Erneutes Parsen der ini f\u00fcr Export-Metadaten erfolgreich");
    if (!iniMetaResult) return;

    const auto outDir = std::filesystem::temp_directory_path() / "map_editor_legacy_texture_export_test";
    std::filesystem::remove_all(outDir);
    auto exportResult = legacy::ExportLegacyTextureSet(*stackResult, *iniMetaResult, outDir, "Rou.ini");
    Check(exportResult.has_value(), "ExportLegacyTextureSet erfolgreich");
    if (!exportResult) {
        std::fprintf(stderr, "         Fehler: %s\n", exportResult.error().c_str());
        return;
    }

    Check(std::filesystem::exists(outDir / "Rou.ini"), "Exportierte Rou.ini existiert");
    Check(std::filesystem::exists(outDir / "fieldtexture/L1_A.BMP"), "Exportierte Blend-BMP liegt an der aus dem Legacy-Pfad abgeleiteten Stelle (ohne virtuelles 'resmap'-Präfix, siehe docs/MAP_FORMAT.md)");

    // 4) Wieder importieren und mit dem Zustand vor dem Export vergleichen.
    legacy::TextureSetImportReport reimportReport;
    auto reimported = legacy::ImportLegacyTextureSet(outDir / "Rou.ini", &reimportReport);
    Check(reimported.has_value(), "Re-Import des exportierten Sets erfolgreich");
    Check(reimportReport.missingBlendFiles.empty(), "Beim Re-Import fehlt KEINE Blend-BMP mehr (alle wurden ja gerade exportiert)");

    if (reimported) {
        Check(reimported->LayerCount() == stackResult->LayerCount(), "Re-Import: gleiche Layer-Anzahl");
        bool weightsMatch = true;
        float maxDiff = 0.0f;
        for (std::size_t i = 0; i < reimported->LayerCount() && weightsMatch; ++i) {
            const auto& a = stackResult->Layer(i).blend;
            const auto& b = reimported->Layer(i).blend;
            for (std::uint32_t z = 0; z < a.Height() && weightsMatch; ++z) {
                for (std::uint32_t x = 0; x < a.Width() && weightsMatch; ++x) {
                    const float diff = std::abs(a.At(x, z) - b.At(x, z));
                    maxDiff = std::max(maxDiff, diff);
                    if (diff > 1.0f / 255.0f + 1e-4f) weightsMatch = false;
                }
            }
        }
        std::printf("         Max. Gewichtsabweichung nach vollem Export+Re-Import: %.5f\n", maxDiff);
        Check(weightsMatch, "Re-Import: alle Blend-Gewichte innerhalb der 8-bit-Toleranz identisch zum Export-Zustand");
    }

    std::filesystem::remove_all(outDir);
}

} // namespace

int main() {
    std::printf("== BMP-Blend-Codec Selbsttest ==\n");
    TestBmpCodecSelfRoundtrip();

    std::printf("\n== Voller Legacy-Texturing-Rundlauf (echte Rou.ini) ==\n");
    TestFullLegacyTextureRoundtrip("tests/fixtures/Rou.ini");

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
