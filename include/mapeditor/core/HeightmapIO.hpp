#pragma once
#include "mapeditor/core/Heightmap.hpp"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

std::expected<Heightmap, std::string> LoadTshm(const std::filesystem::path& file);
std::expected<void, std::string> SaveTshm(const Heightmap& heightmap, const std::filesystem::path& file);

struct LegacyHtdHeader { std::uint8_t raw[4]{}; };

std::expected<Heightmap, std::string> ImportLegacyHtd(
    const std::filesystem::path& file, std::uint32_t width, std::uint32_t height,
    float blockWidth = 50.0f, float blockHeight = 50.0f,
    LegacyHtdHeader* outHeader = nullptr,
    std::vector<std::uint8_t>* outTrailingBytes = nullptr);

std::expected<void, std::string> ExportLegacyHtd(
    const Heightmap& heightmap, const std::filesystem::path& file,
    LegacyHtdHeader header = LegacyHtdHeader{{0x01, 0x02, 0x01, 0x00}},
    const std::vector<std::uint8_t>& trailingBytes = {});

} // namespace theseed::mapeditor::core
