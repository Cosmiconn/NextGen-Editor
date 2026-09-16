#pragma once
// DdsImage.hpp
// GUI-freier DDS-Loader für die vom Editor benötigten komprimierten Texturen.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct DdsImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
};

std::expected<DdsImage, std::string> LoadDdsImage(const std::filesystem::path& file);
std::expected<DdsImage, std::string> DecodeBcImage(std::uint32_t width, std::uint32_t height,
                                                     std::uint32_t pixelFormat,
                                                     const std::vector<std::uint8_t>& data);

} // namespace theseed::mapeditor::core
