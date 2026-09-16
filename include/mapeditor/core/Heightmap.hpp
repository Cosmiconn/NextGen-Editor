#pragma once
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace theseed::mapeditor::core {

class Heightmap {
public:
    Heightmap() = default;
    Heightmap(std::uint32_t width, std::uint32_t height,
               float blockWidth = 50.0f, float blockHeight = 50.0f);
    void Resize(std::uint32_t width, std::uint32_t height, float fillValue = 0.0f);
    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }
    [[nodiscard]] float BlockWidth() const noexcept { return blockWidth_; }
    [[nodiscard]] float BlockHeight() const noexcept { return blockHeight_; }
    void SetBlockSize(float blockWidth, float blockHeight) noexcept {
        blockWidth_ = blockWidth; blockHeight_ = blockHeight;
    }
    [[nodiscard]] bool InBounds(std::uint32_t x, std::uint32_t z) const noexcept {
        return x < width_ && z < height_;
    }
    [[nodiscard]] float At(std::uint32_t x, std::uint32_t z) const noexcept;
    void Set(std::uint32_t x, std::uint32_t z, float value) noexcept;
    [[nodiscard]] float SampleWorld(float worldX, float worldZ) const noexcept;
    [[nodiscard]] std::span<const float> Data() const noexcept { return data_; }
    [[nodiscard]] std::span<float> MutableData() noexcept { return data_; }
    [[nodiscard]] std::pair<float, float> MinMax() const noexcept;
private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    float blockWidth_ = 50.0f;
    float blockHeight_ = 50.0f;
    std::vector<float> data_;
};

} // namespace theseed::mapeditor::core
