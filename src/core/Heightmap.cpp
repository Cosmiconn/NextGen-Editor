#include "mapeditor/core/Heightmap.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

namespace theseed::mapeditor::core {

Heightmap::Heightmap(std::uint32_t width, std::uint32_t height, float blockWidth, float blockHeight)
    : width_(width), height_(height), blockWidth_(blockWidth), blockHeight_(blockHeight),
      data_(static_cast<std::size_t>(width) * height, 0.0f) {}

void Heightmap::Resize(std::uint32_t width, std::uint32_t height, float fillValue) {
    width_ = width;
    height_ = height;
    data_.assign(static_cast<std::size_t>(width) * height, fillValue);
}

float Heightmap::At(std::uint32_t x, std::uint32_t z) const noexcept {
    assert(InBounds(x, z) && "Heightmap::At au\u00dferhalb des Gitters");
    return data_[static_cast<std::size_t>(z) * width_ + x];
}

void Heightmap::Set(std::uint32_t x, std::uint32_t z, float value) noexcept {
    assert(InBounds(x, z) && "Heightmap::Set au\u00dferhalb des Gitters");
    data_[static_cast<std::size_t>(z) * width_ + x] = value;
}

float Heightmap::SampleWorld(float worldX, float worldZ) const noexcept {
    if (width_ == 0 || height_ == 0) {
        return 0.0f;
    }

    // Weltkoordinate -> gebrochene Gitterkoordinate.
    const float gx = std::clamp(worldX / blockWidth_, 0.0f, static_cast<float>(width_ - 1));
    const float gz = std::clamp(worldZ / blockHeight_, 0.0f, static_cast<float>(height_ - 1));

    const auto x0 = static_cast<std::uint32_t>(gx);
    const auto z0 = static_cast<std::uint32_t>(gz);
    const std::uint32_t x1 = std::min(x0 + 1, width_ - 1);
    const std::uint32_t z1 = std::min(z0 + 1, height_ - 1);

    const float tx = gx - static_cast<float>(x0);
    const float tz = gz - static_cast<float>(z0);

    const float h00 = At(x0, z0);
    const float h10 = At(x1, z0);
    const float h01 = At(x0, z1);
    const float h11 = At(x1, z1);

    const float hx0 = h00 + (h10 - h00) * tx;
    const float hx1 = h01 + (h11 - h01) * tx;
    return hx0 + (hx1 - hx0) * tz;
}

std::pair<float, float> Heightmap::MinMax() const noexcept {
    if (data_.empty()) {
        return {0.0f, 0.0f};
    }
    float lo = std::numeric_limits<float>::max();
    float hi = std::numeric_limits<float>::lowest();
    for (const float v : data_) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    return {lo, hi};
}

} // namespace theseed::mapeditor::core
