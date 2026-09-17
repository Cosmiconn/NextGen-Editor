#pragma once
// TextureLayerStack.hpp
// Geordnete Liste von Textur-Layern (Diffuse-Textur-Referenz + Blend-Gewichtsgitter je Layer),
// analog zum #Layer-Konzept aus dem Legacy-.ini-Format. Alle Layer teilen sich eine gemeinsame
// Gitterauflösung (i.d.R. identisch zur Heightmap-Auflösung).

#include "mapeditor/core/BlendMap.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct TextureLayer {
    std::string name;
    std::string diffuseFileName;
    float uvScaleDiffuse = 1.0f;
    BlendMap blend;
};

class TextureLayerStack {
public:
    TextureLayerStack() = default;
    TextureLayerStack(std::uint32_t width, std::uint32_t height) : width_(width), height_(height) {}

    // Setzt die gemeinsame Gitterauflösung; vorhandene Layer-Gewichte werden zurückgesetzt.
    void Resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }

    // Neuer Layer, Gewicht überall 0 - Ausnahme: der allererste Layer wird mit 1.0 vorbelegt
    // (Basis-Layer, sonst wäre die Normalisierung beim ersten Malen uneindeutig).
    std::size_t AddLayer(std::string name, std::string diffuseFileName, float uvScaleDiffuse = 1.0f);
    void RemoveLayer(std::size_t index);
    void MoveLayer(std::size_t fromIndex, std::size_t toIndex);

    [[nodiscard]] std::size_t LayerCount() const noexcept { return layers_.size(); }
    [[nodiscard]] const TextureLayer& Layer(std::size_t index) const { return layers_.at(index); }
    [[nodiscard]] TextureLayer& Layer(std::size_t index) { return layers_.at(index); }

    // Summe der Gewichte aller Layer an Zelle (x,z) - sollte nach jeder Malaktion ~1.0 sein
    // (Invariante, siehe TexturePaintOps); nützlich für Tests/Debug-Anzeige.
    [[nodiscard]] float WeightSumAt(std::uint32_t x, std::uint32_t z) const;

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<TextureLayer> layers_;
};

} // namespace theseed::mapeditor::core
