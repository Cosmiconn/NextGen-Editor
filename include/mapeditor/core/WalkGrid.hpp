#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace theseed::mapeditor::core {

class WalkGrid {
public:
    WalkGrid() = default;
    WalkGrid(std::uint32_t width, std::uint32_t height, std::int16_t fillValue = -1)
        : width_(width), height_(height), data_(static_cast<std::size_t>(width) * height, fillValue) {}

    void Resize(std::uint32_t width, std::uint32_t height, std::int16_t fillValue = -1) {
        width_ = width; height_ = height;
        data_.assign(static_cast<std::size_t>(width) * height, fillValue);
    }
    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }
    [[nodiscard]] bool InBounds(std::uint32_t x, std::uint32_t z) const noexcept { return x < width_ && z < height_; }
    [[nodiscard]] std::int16_t At(std::uint32_t x, std::uint32_t z) const noexcept { return data_[static_cast<std::size_t>(z) * width_ + x]; }
    void Set(std::uint32_t x, std::uint32_t z, std::int16_t value) noexcept { data_[static_cast<std::size_t>(z) * width_ + x] = value; }
    [[nodiscard]] std::span<const std::int16_t> Data() const noexcept { return data_; }
    [[nodiscard]] std::span<std::int16_t> MutableData() noexcept { return data_; }
private:
    std::uint32_t width_ = 0, height_ = 0;
    std::vector<std::int16_t> data_;
};

} // namespace theseed::mapeditor::core
