// test_dds_write.cpp
// DDS-Schreiber: BC1/BC2/BC3-Roundtrip, Header und Dateigrößen wie in den NA2016-Originalen.
// Optional: Pfad zu den NA2016-Daten (Client/resmenu/minimap/Rou.DDS) für einen Real-Data-Roundtrip.

#include "mapeditor/core/DdsImage.hpp"
#include "mapeditor/core/DdsWrite.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

using namespace theseed::mapeditor::core;

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

double Psnr(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b, int channels) {
    double sum = 0.0;
    std::size_t n = 0;
    for (std::size_t i = 0; i + 3 < a.size() && i + 3 < b.size(); i += 4)
        for (int c = 0; c < channels; ++c) {
            const double d = static_cast<double>(a[i + c]) - b[i + c];
            sum += d * d;
            ++n;
        }
    const double mse = sum / static_cast<double>(n);
    return mse <= 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

std::vector<std::uint8_t> TestImage(std::uint32_t w, std::uint32_t h) {
    std::vector<std::uint8_t> img(static_cast<std::size_t>(w) * h * 4);
    for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            std::uint8_t* p = &img[(static_cast<std::size_t>(y) * w + x) * 4];
            p[0] = static_cast<std::uint8_t>(x * 255 / (w - 1));
            p[1] = static_cast<std::uint8_t>(y * 255 / (h - 1));
            p[2] = static_cast<std::uint8_t>(128 + 100 * std::sin(x * 0.05) * std::cos(y * 0.05));
            p[3] = static_cast<std::uint8_t>((x + y) * 255 / (w + h - 2));
        }
    return img;
}

std::uint32_t Read32(const std::vector<std::uint8_t>& v, std::size_t at) {
    return v[at] | (v[at + 1] << 8) | (v[at + 2] << 16) | (static_cast<std::uint32_t>(v[at + 3]) << 24);
}

} // namespace

int main(int argc, char** argv) {
    const std::uint32_t w = 64, h = 48;
    const auto img = TestImage(w, h);
    struct Case { DdsFormat f; std::uint32_t pf; const char* name; };
    for (const Case c : {Case{DdsFormat::Dxt1, 4, "DXT1"}, Case{DdsFormat::Dxt3, 5, "DXT3"}, Case{DdsFormat::Dxt5, 6, "DXT5"}}) {
        const auto blocks = EncodeBcImage(img, w, h, c.f);
        const auto dec = DecodeBcImage(w, h, c.pf, blocks);
        Check(dec.has_value(), std::string(c.name) + ": dekodierbar");
        if (!dec) continue;
        const double psnr = Psnr(img, dec->rgba, 3);
        Check(psnr > 34.0, std::string(c.name) + ": Farbe PSNR " + std::to_string(psnr) + " dB > 34");
        if (c.f != DdsFormat::Dxt1) {
            const double pa = [&] {
                double s = 0;
                for (std::size_t i = 3; i < img.size(); i += 4) { const double d = static_cast<double>(img[i]) - dec->rgba[i]; s += d * d; }
                const double mse = s / (img.size() / 4);
                return mse <= 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
            }();
            Check(pa > (c.f == DdsFormat::Dxt3 ? 28.0 : 36.0), std::string(c.name) + ": Alpha PSNR " + std::to_string(pa));
        }
    }

    // Einfarbige Blöcke bleiben exakt (565-darstellbare Farbe).
    std::vector<std::uint8_t> flat(16 * 16 * 4);
    for (std::size_t i = 0; i < flat.size(); i += 4) { flat[i] = 255; flat[i + 1] = 0; flat[i + 2] = 255; flat[i + 3] = 255; }
    const auto flatDec = DecodeBcImage(16, 16, 4, EncodeBcImage(flat, 16, 16, DdsFormat::Dxt1));
    Check(flatDec && flatDec->rgba == flat, "Einfarbig (Magenta) bleibt exakt");

    // Dateigrößen und Header wie in den Originalen.
    const auto big = TestImage(512, 512);
    const auto mini = BuildDdsFile(big, 512, 512, DdsFormat::Dxt1, false);
    Check(mini.size() == 131200 && Read32(mini, 8) == 0x81007u && Read32(mini, 108) == 0x1000u && Read32(mini, 28) == 0,
          "Minimap 512x512 DXT1 ohne Mips: 131200 Byte, Flags 0x81007, Caps 0x1000");
    const auto tp = TestImage(256, 256);
    const auto portal = BuildDdsFile(tp, 256, 256, DdsFormat::Dxt5, true);
    Check(portal.size() == 87536 && Read32(portal, 8) == 0xA1007u && Read32(portal, 108) == 0x401008u && Read32(portal, 28) == 9,
          "Portalbild 256x256 DXT5 mit Mips: 87536 Byte, 9 Stufen, Flags 0xA1007, Caps 0x401008");
    const auto odd = TestImage(200, 200);
    Check(BuildDdsFile(odd, 200, 200, DdsFormat::Dxt3, true).size() == 53856, "200x200 DXT3 mit Mips: 53856 Byte wie Rou_disable.DDS");

    // Über Datei und LoadDdsImage (dreht für OpenGL): obere Zeile landet unten.
    const auto tmp = std::filesystem::temp_directory_path() / "nextgen_test_dds_write.dds";
    Check(WriteDdsFile(tmp, img, w, h, DdsFormat::Dxt5, true).has_value(), "Datei geschrieben");
    const auto loaded = LoadDdsImage(tmp);
    Check(loaded && loaded->width == w && loaded->height == h, "LoadDdsImage liest die Datei");
    if (loaded) {
        // Zeile 0 der Quelle (Grün = 0) entspricht der letzten Zeile im OpenGL-Bild.
        const std::uint8_t g0 = loaded->rgba[(static_cast<std::size_t>(h - 1) * w + 10) * 4 + 1];
        Check(g0 < 16, "Ausrichtung: obere Bildkante = erste Dateizeile");
    }
    std::filesystem::remove(tmp);

    if (argc > 1 && argv[1][0] != '\0') {
        const auto rou = std::filesystem::path(argv[1]) / "Client" / "resmenu" / "minimap" / "Rou.DDS";
        if (std::filesystem::exists(rou)) {
            const auto orig = LoadDdsImage(rou);
            Check(orig.has_value(), "Original Rou.DDS lesbar");
            if (orig) {
                const auto re = DecodeBcImage(orig->width, orig->height, 5, EncodeBcImage(orig->rgba, orig->width, orig->height, DdsFormat::Dxt3));
                Check(re && Psnr(orig->rgba, re->rgba, 3) > 36.0, "Original-Minimap neu komprimiert: PSNR > 36 dB");
            }
        } else {
            std::printf("[skip]   %s fehlt\n", rou.string().c_str());
        }
    }
    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
