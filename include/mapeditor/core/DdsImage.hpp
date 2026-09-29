#pragma once
// DdsImage.hpp
// GUI-free top-mip DDS decoder: BC1/BC2/BC3 and packed RGB/RGBA masks.
// NIF embedded textures share the same bounded pixel decoders.

#include <cstdint>
#include <array>
#include <expected>
#include <filesystem>
#include <string>
#include <span>
#include <vector>

namespace theseed::mapeditor::core {

struct DdsImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba; // width*height*4 Byte, bereits in OpenGL-V-Ausrichtung
};

struct DdsCubeImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // DDS legacy cube order: +X, -X, +Y, -Y, +Z, -Z.
    std::array<DdsImage, 6> faces;
};

std::expected<DdsImage, std::string> LoadDdsImage(const std::filesystem::path& file);
std::expected<DdsCubeImage, std::string> LoadDdsCubeImage(const std::filesystem::path& file);

// BMP-Loader fuer Legacy-Fiesta-Texturen. Unterstützt BI_RGB 8-bit Palette sowie
// 24/32-bit BGR(A), inklusive 4-Byte-Zeilenpadding und top-down/bottom-up DIBs.
std::expected<DdsImage, std::string> LoadBmpImage(const std::filesystem::path& file);

// TGA-Loader fuer Legacy-NIF-Texturen (24/32-bit, unkomprimiert und RLE).
std::expected<DdsImage, std::string> LoadTgaImage(const std::filesystem::path& file);

// Dekodiert die in NIF/NiPixelData verwendeten BC/DXT-Formate direkt aus dem
// eingebetteten Top-Mip. PixelFormat: 4=DXT1, 5=DXT3, 6=DXT5.
std::expected<DdsImage, std::string> DecodeBcImage(std::uint32_t width, std::uint32_t height,
                                                     std::uint32_t pixelFormat,
                                                     std::span<const std::uint8_t> data);

// Little-endian packed RGB/RGBA, using the format's actual masks (8/16/24/32 bits).
// Returns file row order; the DDS/NIF caller applies its own vertical convention.
std::expected<DdsImage, std::string> DecodePackedImage(
    std::uint32_t width, std::uint32_t height, std::uint32_t bitsPerPixel,
    const std::array<std::uint32_t, 4>& masks, std::span<const std::uint8_t> data,
    std::size_t rowPitch = 0);

} // namespace theseed::mapeditor::core
