#include "mapeditor/core/TextureLayerStack.hpp"

#include <algorithm>
#include <utility>

namespace theseed::mapeditor::core {

void TextureLayerStack::Resize(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;
    for (auto& layer : layers_) {
        layer.blend.Resize(width, height, 0.0f);
    }
    if (!layers_.empty()) {
        auto data = layers_.front().blend.MutableData();
        std::fill(data.begin(), data.end(), 1.0f);
    }
}

std::size_t TextureLayerStack::AddLayer(std::string name, std::string diffuseFileName, float uvScaleDiffuse) {
    TextureLayer layer;
    layer.name = std::move(name);
    layer.diffuseFileName = std::move(diffuseFileName);
    layer.uvScaleDiffuse = uvScaleDiffuse;
    layer.blend = BlendMap(width_, height_, layers_.empty() ? 1.0f : 0.0f);
    layers_.push_back(std::move(layer));
    return layers_.size() - 1;
}

void TextureLayerStack::RemoveLayer(std::size_t index) {
    if (index >= layers_.size()) return;
    layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(index));
}

void TextureLayerStack::MoveLayer(std::size_t fromIndex, std::size_t toIndex) {
    if (fromIndex >= layers_.size() || toIndex >= layers_.size() || fromIndex == toIndex) return;
    TextureLayer moved = std::move(layers_[fromIndex]);
    layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(fromIndex));
    layers_.insert(layers_.begin() + static_cast<std::ptrdiff_t>(toIndex), std::move(moved));
}

float TextureLayerStack::WeightSumAt(std::uint32_t x, std::uint32_t z) const {
    float sum = 0.0f;
    for (const auto& layer : layers_) {
        sum += layer.blend.At(x, z);
    }
    return sum;
}

} // namespace theseed::mapeditor::core
