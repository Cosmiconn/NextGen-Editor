#pragma once
// BlendMap.hpp
// Gewichts-Gitter (0..1) für einen einzelnen Textur-Layer. Strukturell an Heightmap angelehnt,
// aber bewusst eigenständig: andere Semantik (Gewicht statt Höhe), kein SampleWorld/MinMax nötig,
// dafür braucht der aufrufende Code (TexturePaintOps) Zugriff über mehrere Layer hinweg gleichzeitig
// (Normalisierung) - das passt nicht sauber in Heightmap's Schnittstelle.

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace theseed::mapeditor::core {

class BlendMap {
public:
    BlendMap() = default;
    BlendMap(std::uint32_t width, std::uint32_t height, float fillValue = 0.0f)
        : width_(width), height_(height), data_(static_cast<std::size_t>(width) * height, fillValue) {}

    void Resize(std::uint32_t width, std::uint32_t height, float fillValue = 0.0f) {
        width_ = width;
        height_ = height;
        data_.assign(static_cast<std::size_t>(width) * height, fillValue);
    }

    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }
    [[nodiscard]] bool InBounds(std::uint32_t x, std::uint32_t z) const noexcept {
        return x < width_ && z < height_;
    }

    [[nodiscard]] float At(std::uint32_t x, std::uint32_t z) const noexcept {
        return data_[static_cast<std::size_t>(z) * width_ + x];
    }
    void Set(std::uint32_t x, std::uint32_t z, float value) noexcept {
        data_[static_cast<std::size_t>(z) * width_ + x] = value;
    }

    [[nodiscard]] std::span<const float> Data() const noexcept { return data_; }
    [[nodiscard]] std::span<float> MutableData() noexcept { return data_; }

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<float> data_;
};

// Bilineares Resampling auf eine Zielauflösung. Für Layer, deren echte Blend-BMP von der
// gemeinsamen Stack-Auflösung abweicht (beobachtet bei der echten Adl-Karte: 8 von 10 Layern
// nutzen 476x476 statt 512x512 wie der Rest) - Alternative zum Verwerfen dieser Layer, siehe
// docs/MAP_FORMAT.md. Kein Rundungsfehler-freier Vorgang, aber deutlich besser als leere
// Gewichte für diese Layer.
BlendMap ResampleBlendMap(const BlendMap& src, std::uint32_t newWidth, std::uint32_t newHeight);

} // namespace theseed::mapeditor::core
