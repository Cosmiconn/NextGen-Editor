// test_minimap.cpp
// Minimap-Konvention (MapViewInfo-Ausschnitt), Ausschnitt/Resampling und Stilisierung.

#include "mapeditor/core/legacy/Minimap.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace theseed::mapeditor::core::legacy;

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& what) {
    if (!condition) {
        std::fprintf(stderr, "[FEHLER] %s\n", what.c_str());
        ++g_failures;
    } else {
        std::printf("[ok]     %s\n", what.c_str());
    }
}

bool Near(float a, float b, float eps = 1e-3f) { return std::abs(a - b) <= eps; }

} // namespace

int main() {
    const auto full = MinimapCropOf({0, 0, 511, 511});
    Check(Near(full.u0, 0) && Near(full.v0, 0) && Near(full.u1, 1) && Near(full.v1, 1), "0,0-511,511 = ganze Karte");
    const auto rou = MinimapCropOf({79, 86, 474, 482});
    Check(Near(rou.u0, 79 / 512.0f) && Near(rou.v1, 483 / 512.0f), "Roumen-Ausschnitt in Kartenanteilen");
    float x = 0, y = 0, u = 0, v = 0;
    MapToMinimapImage(rou, 0.5f, 0.5f, x, y);
    MinimapImageToMap(rou, x, y, u, v);
    Check(Near(u, 0.5f) && Near(v, 0.5f), "Karte -> Bild -> Karte ist verlustfrei");
    MapToMinimapImage(rou, rou.u0, rou.v0, x, y);
    Check(Near(x, 0) && Near(y, 0), "Nordwest-Ecke des Ausschnitts = Bildecke oben links");
    const auto bad = MinimapCropOf({300, 300, 100, 100});
    Check(Near(bad.u1, 1) && Near(bad.u0, 0), "Ungültiger Ausschnitt fällt auf die ganze Karte zurück");

    // Ausschnitt aus einem 4x4-Bild: rechte untere Hälfte.
    std::vector<std::uint8_t> img(4 * 4 * 4);
    for (int py = 0; py < 4; ++py)
        for (int px = 0; px < 4; ++px) {
            img[(py * 4 + px) * 4 + 0] = static_cast<std::uint8_t>(px * 60);
            img[(py * 4 + px) * 4 + 1] = static_cast<std::uint8_t>(py * 60);
            img[(py * 4 + px) * 4 + 3] = 255;
        }
    const auto part = CropResampleRgba(img, 4, 4, {0.5f, 0.5f, 1.0f, 1.0f}, 2, 2);
    Check(part[0] == 120 && part[1] == 120 && part[(3) * 4 + 0] == 180 && part[3 * 4 + 1] == 180,
          "Ausschnitt rechts unten liefert die Pixel (2,2)..(3,3)");

    // Stilisierung: Kontur an der Grenze, Rand dunkel, Innenfläche hell.
    const std::uint32_t n = 128;
    std::vector<std::uint8_t> base(n * n * 4, 200), mask(n * n, 0);
    for (std::uint32_t py = 32; py < 96; ++py)
        for (std::uint32_t px = 32; px < 96; ++px) mask[py * n + px] = 1;
    const auto styled = StylizeMinimap(base, mask, n);
    const auto lum = [&](std::uint32_t px, std::uint32_t py) {
        const auto* p = &styled[(py * n + px) * 4];
        return 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
    };
    Check(styled.size() == n * n * 4, "Größe bleibt");
    Check(lum(64, 64) > 180.0f, "Begehbare Fläche bleibt hell");
    Check(lum(20, 64) < lum(64, 64) * 0.85f, "Blockierte Fläche ist dunkler");
    Check(lum(32, 64) < 90.0f, "Kontur an der Grenze ist dunkel");
    Check(lum(0, 64) < 60.0f && lum(64, 0) < 60.0f, "Pergament-Rand ist dunkel");
    Check(MinimapRelativePath("NewTown") == "minimap/NewTown.dds", "Dateipfad relativ zu resmenu");

    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
