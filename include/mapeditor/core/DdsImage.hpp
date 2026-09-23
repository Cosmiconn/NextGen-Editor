#pragma once
// DdsImage.hpp
// GUI-freier DDS-Loader (nur Top-Mip wird dekodiert - für die Terrain-Textur-Vorschau reicht
// das, Mipmaps sind hier keine echte Voraussetzung). Deckt die drei in den echten
// Field-Texturen tatsächlich vorkommenden Formate ab (empirisch geprüft, 160 echte Dateien):
// BC1/DXT1 (140), BC2/DXT3 (17), BC3/DXT5 (3). Kein Support für unkomprimierte DDS oder andere
// FourCCs - kamen in den Referenzdaten nicht vor.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct DdsImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba; // width*height*4 Byte, bereits in OpenGL-V-Ausrichtung
};

std::expected<DdsImage, std::string> LoadDdsImage(const std::filesystem::path& file);

// TGA-Loader fuer Legacy-NIF-Texturen (24/32-bit, unkomprimiert und RLE).
std::expected<DdsImage, std::string> LoadTgaImage(const std::filesystem::path& file);

// Dekodiert die in NIF/NiPixelData verwendeten BC/DXT-Formate direkt aus dem
// eingebetteten Top-Mip. PixelFormat: 4=DXT1, 5=DXT3, 6=DXT5.
std::expected<DdsImage, std::string> DecodeBcImage(std::uint32_t width, std::uint32_t height,
                                                     std::uint32_t pixelFormat,
                                                     const std::vector<std::uint8_t>& data);

} // namespace theseed::mapeditor::core
