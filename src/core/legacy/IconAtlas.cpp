#include "mapeditor/core/legacy/IconAtlas.hpp"

#include <fstream>

namespace theseed::mapeditor::core::legacy {

int IconAtlasCapacity(std::uint32_t atlasWidth, std::uint32_t atlasHeight, std::uint32_t cellSize) {
    if (cellSize == 0) return 0;
    return static_cast<int>((atlasWidth / cellSize) * (atlasHeight / cellSize));
}

IconCell IconAtlasCell(std::uint32_t atlasWidth, std::uint32_t atlasHeight, int index, std::uint32_t cellSize) {
    IconCell c;
    const std::uint32_t cols = cellSize ? atlasWidth / cellSize : 0;
    if (index < 0 || cols == 0 || index >= IconAtlasCapacity(atlasWidth, atlasHeight, cellSize)) return c;
    const auto col = static_cast<std::uint32_t>(index) % cols, row = static_cast<std::uint32_t>(index) / cols;
    c.u0 = static_cast<float>(col * cellSize) / atlasWidth;
    c.v0 = static_cast<float>(row * cellSize) / atlasHeight;
    c.u1 = static_cast<float>((col + 1) * cellSize) / atlasWidth;
    c.v1 = static_cast<float>((row + 1) * cellSize) / atlasHeight;
    c.valid = true;
    return c;
}

std::string IconAtlasRelativePath(const std::string& iconFile) {
    if (iconFile.empty() || iconFile == "-") return {};
    return "Icon/" + iconFile + ".dds";
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> ReadDdsSize(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    unsigned char h[20] = {};
    if (!in.read(reinterpret_cast<char*>(h), sizeof(h)) || h[0] != 'D' || h[1] != 'D' || h[2] != 'S' || h[3] != ' ') return std::nullopt;
    const auto u32 = [&](int at) { return h[at] | (h[at + 1] << 8) | (h[at + 2] << 16) | (static_cast<std::uint32_t>(h[at + 3]) << 24); };
    return std::make_pair(u32(16), u32(12)); // dwWidth, dwHeight
}

} // namespace theseed::mapeditor::core::legacy
