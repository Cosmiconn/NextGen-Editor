// test_image_io.cpp
// Bild-Pipeline: jedes Zielformat der Interface-Dateien schreiben und wieder lesen, Format-
// Erkennung, Größenmodi, Ersatz im Format des Originals, Ausschnitt/Einsetzen. Mit Pfad zu einem
// resmenu-Ordner: zusätzlich jede echte Bilddatei erkennen und dekodieren.

#include "mapeditor/core/ImageIO.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

using namespace theseed::mapeditor::core;

namespace {
int g_failures = 0;
void Check(bool ok, const std::string& what) {
    if (ok) std::printf("[ok]     %s\n", what.c_str());
    else { std::fprintf(stderr, "[FEHLER] %s\n", what.c_str()); ++g_failures; }
}

RgbaImage Gradient(std::uint32_t w, std::uint32_t h) {
    RgbaImage img;
    img.width = w;
    img.height = h;
    img.rgba.resize(std::size_t{w} * h * 4);
    for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            auto* p = &img.rgba[(std::size_t{y} * w + x) * 4];
            p[0] = static_cast<std::uint8_t>(x * 255 / (w - 1));
            p[1] = static_cast<std::uint8_t>(y * 255 / (h - 1));
            p[2] = static_cast<std::uint8_t>(128);
            p[3] = static_cast<std::uint8_t>(x < w / 2 ? 255 : 0); // linke Hälfte deckend
        }
    return img;
}
// Größte Abweichung je Kanal (Alpha getrennt), um verlustbehaftete Formate zu prüfen.
int MaxDiff(const RgbaImage& a, const RgbaImage& b, bool alpha) {
    if (a.width != b.width || a.height != b.height) return 999;
    int worst = 0;
    for (std::size_t i = 0; i < a.rgba.size(); ++i) {
        if ((i % 4 == 3) != alpha) continue;
        worst = std::max(worst, std::abs(int(a.rgba[i]) - int(b.rgba[i])));
    }
    return worst;
}

void RoundTrips() {
    const RgbaImage src = Gradient(64, 32);
    struct Case { const char* name; ImageFormat format; int colorTolerance; int alphaTolerance; };
    auto fmt = [](ImageContainer c) { ImageFormat f; f.container = c; f.width = 64; f.height = 32; return f; };
    std::vector<Case> cases;
    cases.push_back({"PNG RGBA", fmt(ImageContainer::Png), 0, 0});
    { auto f = fmt(ImageContainer::Png); f.pngAlpha = false; cases.push_back({"PNG RGB", f, 0, 255}); }
    { auto f = fmt(ImageContainer::Tga); f.tgaBits = 32; cases.push_back({"TGA 32 Bit", f, 0, 0}); }
    { auto f = fmt(ImageContainer::Tga); f.tgaBits = 24; cases.push_back({"TGA 24 Bit", f, 0, 255}); }
    { auto f = fmt(ImageContainer::Tga); f.tgaBits = 16; cases.push_back({"TGA 16 Bit A1R5G5B5", f, 8, 0}); }
    { auto f = fmt(ImageContainer::Dds); f.dds = DdsFormat::Bgra32; cases.push_back({"DDS A8R8G8B8", f, 0, 0}); }
    { auto f = fmt(ImageContainer::Dds); f.dds = DdsFormat::Dxt5; f.mipmaps = true; cases.push_back({"DDS DXT5 +Mipmaps", f, 24, 8}); }
    { auto f = fmt(ImageContainer::Dds); f.dds = DdsFormat::Dxt3; cases.push_back({"DDS DXT3", f, 24, 17}); }
    { auto f = fmt(ImageContainer::Dds); f.dds = DdsFormat::Dxt1; cases.push_back({"DDS DXT1", f, 24, 255}); }
    { auto f = fmt(ImageContainer::Jpg); f.jpgQuality = 95; cases.push_back({"JPG", f, 40, 255}); }
    for (const auto& c : cases) {
        auto bytes = EncodeImage(src, c.format);
        Check(bytes.has_value() && !bytes->empty(), std::string(c.name) + ": geschrieben");
        if (!bytes) continue;
        auto inspected = InspectImageBytes(*bytes);
        Check(inspected && inspected->container == c.format.container && inspected->width == 64 && inspected->height == 32 &&
                  (c.format.container != ImageContainer::Dds || (inspected->dds == c.format.dds && inspected->mipmaps == c.format.mipmaps)) &&
                  (c.format.container != ImageContainer::Tga || inspected->tgaBits == c.format.tgaBits) &&
                  (c.format.container != ImageContainer::Png || inspected->pngAlpha == c.format.pngAlpha),
              std::string(c.name) + ": Format aus dem Kopf erkannt (" + (inspected ? DescribeImageFormat(*inspected) : "?") + ")");
        auto back = DecodeImage(*bytes);
        Check(back.has_value(), std::string(c.name) + ": gelesen");
        if (!back) continue;
        const int dc = MaxDiff(src, *back, false), da = MaxDiff(src, *back, true);
        Check(dc <= c.colorTolerance && (c.alphaTolerance == 255 || da <= c.alphaTolerance),
              std::string(c.name) + ": Pixel erhalten (Farbe ±" + std::to_string(dc) + ", Alpha ±" + std::to_string(da) + ")");
        // Orientierung: oben links rot = 0, unten rechts rot = 255 (auch bei unten beginnendem TGA).
        Check(back->rgba[1] < 40 && back->rgba[(std::size_t{31} * 64 + 63) * 4 + 1] > 215, std::string(c.name) + ": Zeile 0 ist oben");
    }
}

// 16-Bit-TGA wie die NA2016-Ladebilder: Alpha-Bit gemeldet, aber in keinem Pixel gesetzt -> deckend;
// ein deckendes Bild wird ebenso geschrieben (Bit 0), damit Ersatzbilder den Originalen gleichen.
void Tga16Opaque() {
    RgbaImage opaque = Gradient(8, 4);
    for (std::size_t i = 3; i < opaque.rgba.size(); i += 4) opaque.rgba[i] = 255;
    ImageFormat f;
    f.container = ImageContainer::Tga;
    f.width = 8;
    f.height = 4;
    f.tgaBits = 16;
    auto bytes = EncodeImage(opaque, f);
    bool bitClear = bytes.has_value() && (*bytes)[17] == 1;
    for (std::size_t i = 18; bytes && i + 1 < bytes->size(); i += 2) bitClear = bitClear && ((*bytes)[i + 1] & 0x80) == 0;
    Check(bitClear, "TGA 16 Bit deckend: Alpha-Bit wie in den Originalen überall 0");
    auto back = bytes ? DecodeImage(*bytes) : std::unexpected(std::string("-"));
    bool allOpaque = back.has_value();
    for (std::size_t i = 3; back && i < back->rgba.size(); i += 4) allOpaque = allOpaque && back->rgba[i] == 255;
    Check(allOpaque, "TGA 16 Bit mit Alpha-Bit überall 0 wird deckend gelesen");
}

void Resizing() {
    const RgbaImage src = Gradient(200, 100);
    const auto stretch = ResizeImage(src, 64, 64, FitMode::Stretch);
    Check(stretch.width == 64 && stretch.height == 64 && stretch.Valid(), "Strecken: Zielgröße");
    const auto fit = ResizeImage(src, 64, 64, FitMode::Fit);
    Check(fit.width == 64 && fit.height == 64 && fit.rgba[3] == 0 && fit.rgba[(std::size_t{32} * 64 + 2) * 4 + 3] == 255,
          "Einpassen: oben transparenter Rand, Mitte links deckend");
    const auto fill = ResizeImage(src, 64, 64, FitMode::Fill);
    Check(fill.width == 64 && fill.height == 64 && fill.rgba[3] == 255 && fill.rgba[(std::size_t{63} * 64 + 63) * 4 + 3] == 0,
          "Füllen: keine Ränder, mittiger Ausschnitt");

    ImageFormat original;
    original.container = ImageContainer::Dds;
    original.dds = DdsFormat::Dxt3;
    original.mipmaps = true;
    original.width = 128;
    original.height = 64;
    auto rep = BuildReplacement(src, original, {});
    Check(rep && rep->format.width == 128 && rep->format.height == 64, "Ersatz übernimmt die Größe des Originals");
    auto inspected = rep ? InspectImageBytes(rep->bytes) : std::unexpected(std::string("-"));
    Check(inspected && inspected->dds == DdsFormat::Dxt3 && inspected->mipmaps, "Ersatz übernimmt DXT3 mit Mipmaps");
    ReplaceOptions own;
    own.keepOriginalSize = false;
    auto ownSize = BuildReplacement(Gradient(30, 18), original, own);
    Check(ownSize && ownSize->format.width == 32 && ownSize->format.height == 20, "eigene Größe: DDS auf Vielfache von 4 gerundet");

    RgbaImage atlas = Gradient(64, 64);
    RgbaImage cell = CropImage(atlas, 16, 16, 16, 16);
    Check(cell.width == 16 && cell.height == 16 && cell.rgba[0] == atlas.rgba[(std::size_t{16} * 64 + 16) * 4], "Ausschnitt");
    RgbaImage white;
    white.width = white.height = 16;
    white.rgba.assign(16 * 16 * 4, 255);
    PasteImage(atlas, white, 48, 48);
    Check(atlas.rgba[(std::size_t{50} * 64 + 50) * 4 + 2] == 255 && atlas.rgba[(std::size_t{47} * 64 + 47) * 4 + 2] == 128,
          "Einsetzen nur im Zielrechteck");
}

// Atlaszelle in einem DXT5-Atlas mit Mipmaps ersetzen: nur die Blöcke der Zelle ändern sich.
void AtlasCellSplice() {
    RgbaImage atlas = Gradient(64, 64);
    ImageFormat f;
    f.container = ImageContainer::Dds;
    f.dds = DdsFormat::Dxt5;
    f.mipmaps = true;
    f.width = f.height = 64;
    const auto original = EncodeImage(atlas, f);
    RgbaImage white;
    white.width = white.height = 16;
    white.rgba.assign(16 * 16 * 4, 255);
    RgbaImage changed = atlas;
    PasteImage(changed, white, 16, 32);
    const auto updated = EncodeImage(changed, f);
    auto spliced = SpliceDdsRegion(*original, *updated, 16, 32, 16, 16);
    Check(spliced.has_value() && spliced->size() == original->size(), "DDS-Zelle eingesetzt (gleiche Dateigröße)");
    if (!spliced) return;
    // Oberste Stufe: 16x16 Blöcke à 16 Bytes; Zelle = Blockspalten 4-7, Blockzeilen 8-11.
    bool outsideSame = true, insideNew = true;
    for (std::uint32_t by = 0; by < 16; ++by)
        for (std::uint32_t bx = 0; bx < 16; ++bx) {
            const std::size_t at = 128 + (std::size_t{by} * 16 + bx) * 16;
            const bool inside = bx >= 4 && bx < 8 && by >= 8 && by < 12;
            const bool same = std::equal(spliced->begin() + static_cast<std::ptrdiff_t>(at), spliced->begin() + static_cast<std::ptrdiff_t>(at + 16),
                                         original->begin() + static_cast<std::ptrdiff_t>(at));
            const bool fromUpdated = std::equal(spliced->begin() + static_cast<std::ptrdiff_t>(at), spliced->begin() + static_cast<std::ptrdiff_t>(at + 16),
                                                updated->begin() + static_cast<std::ptrdiff_t>(at));
            if (!inside && !same) outsideSame = false;
            if (inside && !fromUpdated) insideNew = false;
        }
    Check(outsideSame, "Blöcke außerhalb der Zelle bitgleich zum Original");
    Check(insideNew, "Blöcke der Zelle aus der neuen Kodierung");
    auto back = DecodeImage(*spliced);
    Check(back && back->rgba[(std::size_t{40} * 64 + 24) * 4] > 240, "neue Zelle sichtbar (weiß)");
    Check(!SpliceDdsRegion(*original, *EncodeImage(Gradient(32, 32), [&] { auto g = f; g.width = g.height = 32; return g; }()), 0, 0, 8, 8),
          "abweichende Größe wird abgelehnt");
}

void RealResmenu(const std::filesystem::path& root) {
    std::map<std::string, int> ok, failed;
    std::string firstFailure;
    for (const auto& e : std::filesystem::recursive_directory_iterator(root)) {
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".png" && ext != ".tga" && ext != ".dds" && ext != ".jpg" && ext != ".bmp") continue;
        auto fmt = InspectImageFile(e.path());
        auto img = fmt ? LoadImageFile(e.path()) : std::unexpected(fmt.error());
        const bool good = fmt && img && img->width == fmt->width && img->height == fmt->height;
        (good ? ok : failed)[ext]++;
        if (!good && firstFailure.empty()) firstFailure = e.path().string() + ": " + (fmt ? (img ? "Größe" : img.error()) : fmt.error());
    }
    int total = 0, bad = 0;
    for (auto& [k, v] : ok) { total += v; std::printf("         %s: %d\n", k.c_str(), v); }
    for (auto& [k, v] : failed) { bad += v; std::printf("         %s FEHLER: %d\n", k.c_str(), v); }
    if (!firstFailure.empty()) std::printf("         erster Fehler: %s\n", firstFailure.c_str());
    Check(total > 4000 && bad == 0, "NA2016 resmenu: jede Bilddatei erkannt und dekodiert (" + std::to_string(total) + ")");
}
} // namespace

int main(int argc, char** argv) {
    RoundTrips();
    Tga16Opaque();
    Resizing();
    AtlasCellSplice();
    if (argc > 1 && argv[1][0] != '\0') RealResmenu(argv[1]);
    if (g_failures) { std::fprintf(stderr, "%d Fehler\n", g_failures); return 1; }
    std::printf("Alle Bild-Tests bestanden.\n");
    return 0;
}
