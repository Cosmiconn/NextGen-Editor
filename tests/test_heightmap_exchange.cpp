// test_heightmap_exchange.cpp
// Höhendaten-Austausch: 16-Bit-PNG und R16 hin und zurück mit der echten Rou.HTD, Lesen einer mit
// zlib komprimierten PNG (alle fünf Zeilenfilter, über zwei IDAT-Chunks verteilt), Umrechnen der Größe.

#include "mapeditor/core/HeightmapExchange.hpp"
#include "mapeditor/core/HeightmapIO.hpp"

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

float MaxError(const Heightmap& a, const Heightmap& b) {
    float e = 0.0f;
    for (std::uint32_t z = 0; z < a.Height(); ++z)
        for (std::uint32_t x = 0; x < a.Width(); ++x) e = std::max(e, std::abs(a.At(x, z) - b.At(x, z)));
    return e;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Aufruf: test_heightmap_exchange <Rou.HTD> <height_gradient16.png>\n");
        return 1;
    }
    auto rou = ImportLegacyHtd(argv[1], 257, 257);
    Check(rou.has_value(), "Rou.HTD geladen");
    if (!rou) return 1;
    const auto [lo, hi] = rou->MinMax();
    const float step = (hi - lo) / 65535.0f;
    const auto dir = std::filesystem::temp_directory_path() / "nextgen_height_exchange";
    std::filesystem::create_directories(dir);

    for (const char* name : {"rou.png", "rou.r16"}) {
        const auto file = dir / name;
        Check(ExportHeightmapImage(*rou, file).has_value(), name);
        auto img = ImportHeightImage(file);
        Check(img.has_value() && img->width == 257 && img->height == 257, "wieder gelesen, 257x257");
        if (!img) continue;
        Heightmap back(257, 257);
        const float a = img->range ? img->range->first : lo, b = img->range ? img->range->second : hi;
        ApplyHeightImage(back, *img, a, b);
        const float err = MaxError(*rou, back);
        std::printf("         %s: max. Abweichung %.4f (eine 16-Bit-Stufe = %.4f)\n", name, err, step);
        Check(err <= step * 0.51f + 1e-3f, "Abweichung höchstens eine halbe 16-Bit-Stufe");
    }
    {
        auto img = ImportHeightImage(dir / "rou.png");
        Check(img && img->range && std::abs(img->range->first - lo) < 0.01f && std::abs(img->range->second - hi) < 0.01f,
              "Höhenbereich im PNG mitgeschrieben");
    }

    auto gradient = ImportHeightImage(argv[2]);
    Check(gradient.has_value(), "komprimierte 16-Bit-PNG gelesen");
    if (gradient) {
        bool all = gradient->width == 37 && gradient->height == 23;
        for (std::uint32_t y = 0; all && y < 23; ++y)
            for (std::uint32_t x = 0; x < 37; ++x)
                if (gradient->values[y * 37 + x] != static_cast<std::uint16_t>((x * 1771 + y * 2903 + x * y * 37) & 0xFFFF)) all = false;
        Check(all, "alle Pixel exakt (Filter None/Sub/Up/Average/Paeth)");
    }

    // Andere Größe: 257er-Bild auf 129er-Karte, Ecken bleiben gleich.
    Heightmap small(129, 129);
    auto img = ImportHeightImage(dir / "rou.r16");
    if (img) {
        ApplyHeightImage(small, *img, lo, hi);
        Check(std::abs(small.At(0, 0) - rou->At(0, 0)) <= step && std::abs(small.At(128, 128) - rou->At(256, 256)) <= step,
              "auf andere Größe umgerechnet, Ecken stimmen");
    }
    Check(!ImportHeightImage(dir / "kaputt.png").has_value(), "fehlende Datei -> Fehler");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
