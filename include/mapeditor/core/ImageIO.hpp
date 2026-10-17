#pragma once
// ImageIO.hpp
// Bilder lesen, umrechnen und im Format eines Originals schreiben - Grundlage für das Ersetzen
// von Interface-Texturen, Ladebildern, Icons und Porträts. Gemessen an den 5602 Dateien in
// NA2016 Client/resmenu:
//   PNG  3349 (RGBA 8 Bit 2881, RGB 8 Bit 468), alle 214 Ladebilder 1024x768
//   TGA   747 (unkomprimiert, unten beginnend: 32 Bit mit 8 Alpha-Bits 660, 16 Bit A1R5G5B5 84, 24 Bit 3)
//   DDS   478 (DXT5/DXT3/DXT1 mit und ohne Mipmaps, ein unkomprimiertes A8R8G8B8)
//   JPG   160
// Ein Ersatzbild übernimmt deshalb immer Behälter, Pixelformat, Mipmaps und (wahlweise) die Größe
// des Originals, damit der Client die Datei genauso lädt wie vorher.
//
// Pixel immer RGBA8, Zeile 0 = obere Bildkante.

#include "mapeditor/core/DdsWrite.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct RgbaImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
    [[nodiscard]] bool Valid() const { return width > 0 && height > 0 && rgba.size() == std::size_t{width} * height * 4; }
};

enum class ImageContainer : std::uint8_t { Png, Jpg, Tga, Dds, Bmp };

// Format einer Datei, so wie es beim Schreiben wieder entstehen soll.
struct ImageFormat {
    ImageContainer container = ImageContainer::Png;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    DdsFormat dds = DdsFormat::Dxt5; // nur DDS
    bool mipmaps = false;            // nur DDS
    int tgaBits = 32;                // nur TGA: 16 (A1R5G5B5), 24, 32
    bool tgaTopDown = false;         // nur TGA: Bildursprung oben (die Originale beginnen unten)
    bool pngAlpha = true;            // nur PNG: RGBA (sonst RGB)
    int jpgQuality = 92;             // nur JPG
};

[[nodiscard]] const char* ImageContainerName(ImageContainer container);
// Lesbare Beschreibung, z. B. "DDS DXT5 mit Mipmaps, 256x256" oder "TGA 32 Bit, 64x64".
[[nodiscard]] std::string DescribeImageFormat(const ImageFormat& format);

// Format aus dem Dateikopf (ohne die Pixel zu dekodieren).
[[nodiscard]] std::expected<ImageFormat, std::string> InspectImageFile(const std::filesystem::path& file);
[[nodiscard]] std::expected<ImageFormat, std::string> InspectImageBytes(std::span<const std::uint8_t> bytes);

// Lädt PNG, JPG, BMP, TGA (16/24/32 Bit, auch RLE) und DDS (DXT1/3/5, unkomprimiert).
[[nodiscard]] std::expected<RgbaImage, std::string> LoadImageFile(const std::filesystem::path& file);
[[nodiscard]] std::expected<RgbaImage, std::string> DecodeImage(std::span<const std::uint8_t> bytes);

// Schreibt das Bild im angegebenen Format (Größe muss zum Bild passen; DDS auf Wunsch mit Mipmaps).
[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> EncodeImage(const RgbaImage& image, const ImageFormat& format);

enum class FitMode : std::uint8_t {
    Stretch, // auf die Zielgröße verzerren
    Fit,     // ganz sichtbar, Ränder transparent
    Fill,    // Ziel ganz füllen, Überstand mittig abschneiden
};
[[nodiscard]] const char* FitModeName(FitMode mode, bool german);
// Hochwertig umrechnen (stb_image_resize2, Alpha vormultipliziert, Kanten geklemmt).
[[nodiscard]] RgbaImage ResizeImage(const RgbaImage& image, std::uint32_t width, std::uint32_t height, FitMode mode);

struct ReplaceOptions {
    FitMode fit = FitMode::Fill;
    bool keepOriginalSize = true; // sonst Größe der Quelle (DDS: auf Vielfache von 4 gerundet)
};
// Ersatz für ein Original: Quelle laden, auf die Größe des Originals bringen und im Format des
// Originals kodieren. Rückgabe: Dateibytes und das tatsächlich geschriebene Format.
struct ReplaceResult {
    std::vector<std::uint8_t> bytes;
    ImageFormat format;
    RgbaImage image; // das kodierte Bild (vor verlustbehafteter Kompression) für die Vorschau
};
[[nodiscard]] std::expected<ReplaceResult, std::string> BuildReplacement(const RgbaImage& source, const ImageFormat& original,
                                                                         const ReplaceOptions& options);

// Ausschnitt kopieren bzw. einsetzen (für Icon-Atlanten): Rechteck in Pixeln, wird geklemmt.
[[nodiscard]] RgbaImage CropImage(const RgbaImage& image, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height);
void PasteImage(RgbaImage& target, const RgbaImage& piece, std::uint32_t x, std::uint32_t y);

// Übernimmt aus "updated" nur die Blöcke, die das Rechteck (Pixel der obersten Stufe) berühren - in
// jeder Mipmap-Stufe; alle anderen Bytes bleiben die von "original". So bleiben beim Ersetzen einer
// Atlaszelle die übrigen Icons bitgleich (DXT neu zu kodieren wäre verlustbehaftet). Beide Dateien
// müssen dasselbe Format, dieselbe Größe und Mipmap-Zahl haben.
[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> SpliceDdsRegion(std::span<const std::uint8_t> original,
                                                                                   std::span<const std::uint8_t> updated,
                                                                                   std::uint32_t x, std::uint32_t y,
                                                                                   std::uint32_t width, std::uint32_t height);

} // namespace theseed::mapeditor::core
