// test_map_render_settings.cpp
// Prüft das Lesen der kartenlokalen Render-Daten gegen die echten NA2016-Dateien
// Rou.conf und Rouvertexcolor2.bmp (Fixtures) sowie Robustheit gegen fehlerhafte Eingaben.

#include "mapeditor/core/legacy/BmpBlendMap.hpp"
#include "mapeditor/core/legacy/MapRenderSettings.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace theseed::mapeditor::core::legacy;

namespace {
int g_failures = 0;
void Check(bool ok, const char* what) {
    if (ok) std::printf("[ok]     %s\n", what);
    else { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; }
}
bool Near(float a, float b) { return std::abs(a - b) < 1.0e-5f; }
} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path fixtures = argc > 1 ? argv[1] : "tests/fixtures";

    // --- Rou.conf (echte Datei, CRLF) ---
    const auto conf = LoadMapRenderConfig(fixtures / "Rou.conf");
    Check(conf.has_value(), "Rou.conf geladen");
    if (conf) {
        Check(conf->groundDirectionalLightPresent && conf->groundDirectionalLight, "Rou.conf: Ground_DL_Enable=TRUE");
        Check(conf->glow.present, "Rou.conf: GlowScreenEffect vorhanden");
        Check(Near(conf->glow.glowness, 0.38f) && Near(conf->glow.blendFactor, 0.55f) &&
              Near(conf->glow.gaussFactor, 1.0f) && Near(conf->glow.downScaling, 8.0f) && conf->glow.numBlurring == 3,
              "Rou.conf: Glow-Werte 0.38 / 0.55 / 1.0 / 8.0 / 3");
        Check(conf->warnings.empty(), "Rou.conf: keine Warnungen");
        Check(conf->sections.size() == 2 && conf->sections.at("GlowScreenEffect").size() == 5,
              "Rou.conf: Rohabschnitte erhalten");
    }

    // --- Robustheit ---
    const auto odd = ParseMapRenderConfig(
        "; comment\r\n[worldsetting]\r\n Ground_DL_Enable = false \r\n[GlowScreenEffect]\nGlowness=abc\n"
        "NumBlurring=99\nUnknownKey=7\nno equals here\n[Extra]\nA=B\n");
    Check(odd.groundDirectionalLightPresent && !odd.groundDirectionalLight, "Bool klein/mit Leerzeichen: false");
    Check(odd.glow.numBlurring == 32, "NumBlurring auf 32 begrenzt");
    Check(Near(odd.glow.glowness, 0.0f), "ungültige Zahl behält Default");
    Check(odd.warnings.size() == 2, "zwei Warnungen (ungültige Zahl, fehlendes '=')");
    Check(odd.sections.at("GlowScreenEffect").at("UnknownKey") == "7" && odd.sections.at("Extra").at("A") == "B",
          "unbekannte Schlüssel/Abschnitte bleiben roh erhalten");
    const auto empty = ParseMapRenderConfig("");
    Check(empty.groundDirectionalLight && !empty.groundDirectionalLightPresent && !empty.glow.present,
          "leere Datei: Defaults (Bodenlicht an, kein Glow)");

    // --- Rouvertexcolor2.bmp (echte Datei, 257x257 = HTD-Vertexgitter) ---
    const auto vc = ReadBmpRgb(fixtures / "Rouvertexcolor2.bmp");
    Check(vc.has_value() && vc->width == 257 && vc->height == 257, "Vertex-Color-BMP: 257x257 wie Rou.HTD");
    if (vc) {
        // Gleiche Zeilenkonvention wie die Blend-BMPs: jede Zeile muss mit ReadBlendMapBmp übereinstimmen
        // (die Blend-Variante liest den B-Kanal als Graustufe).
        const auto gray = ReadBlendMapBmp(fixtures / "Rouvertexcolor2.bmp");
        bool sameRows = gray.has_value() && gray->Width() == vc->width && gray->Height() == vc->height;
        for (std::uint32_t z = 0; sameRows && z < vc->height; z += 17)
            for (std::uint32_t x = 0; sameRows && x < vc->width; x += 13)
                sameRows = std::lround(gray->At(x, z) * 255.0f) == vc->At(x, z)[2];
        Check(sameRows, "Vertex-Color-BMP: Zeilen identisch zur Blend-BMP-Konvention (Zeile y == z)");
        long long sum[3] = {0, 0, 0};
        for (std::size_t i = 0; i < vc->rgb.size(); i += 3)
            for (int c = 0; c < 3; ++c) sum[c] += vc->rgb[i + static_cast<std::size_t>(c)];
        const double n = static_cast<double>(vc->rgb.size() / 3);
        std::printf("         Mittelwert RGB %.1f / %.1f / %.1f\n", static_cast<double>(sum[0]) / n, static_cast<double>(sum[1]) / n, static_cast<double>(sum[2]) / n);
        Check(sum[2] > sum[1] && sum[1] > sum[0], "Vertex-Color-BMP: kühler Farbton (B > G > R), wie in den Daten gemessen");
    }
    Check(!ReadBmpRgb(fixtures / "Rou.conf").has_value(), "Nicht-BMP wird abgelehnt");

    if (g_failures) { std::fprintf(stderr, "%d Fehler\n", g_failures); return 1; }
    std::printf("Alle Map-Render-Settings-Tests bestanden.\n");
    return 0;
}
