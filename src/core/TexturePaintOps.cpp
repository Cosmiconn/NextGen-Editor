#include "mapeditor/core/TexturePaintOps.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace theseed::mapeditor::core {
namespace {
float Falloff(float normalizedDist) {
    const float t = std::clamp(1.0f - normalizedDist, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

TexturePaintPatch PaintLayerWeight(TextureLayerStack& stack, std::size_t targetLayer, PaintMode mode, const TexturePaintSettings& settings, float worldX, float worldZ, float blockWidth, float blockHeight) {
    TexturePaintPatch patch;
    const std::size_t layerCount = stack.LayerCount();
    if (layerCount == 0 || targetLayer >= layerCount || settings.radius <= 0.0f) return patch;
    const float radiusInVertsX = settings.radius / blockWidth;
    const float radiusInVertsZ = settings.radius / blockHeight;
    const float centerVertX = worldX / blockWidth;
    const float centerVertZ = worldZ / blockHeight;
    const auto x0 = static_cast<std::int64_t>(std::floor(centerVertX - radiusInVertsX));
    const auto x1 = static_cast<std::int64_t>(std::ceil(centerVertX + radiusInVertsX));
    const auto z0 = static_cast<std::int64_t>(std::floor(centerVertZ - radiusInVertsZ));
    const auto z1 = static_cast<std::int64_t>(std::ceil(centerVertZ + radiusInVertsZ));
    const auto w = static_cast<std::int64_t>(stack.Width());
    const auto h = static_cast<std::int64_t>(stack.Height());
    for (std::int64_t z = std::max<std::int64_t>(0, z0); z <= std::min<std::int64_t>(h - 1, z1); ++z) {
        for (std::int64_t x = std::max<std::int64_t>(0, x0); x <= std::min<std::int64_t>(w - 1, x1); ++x) {
            const float dx = static_cast<float>(x) * blockWidth - worldX;
            const float dz = static_cast<float>(z) * blockHeight - worldZ;
            const float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > settings.radius) continue;
            const auto ux = static_cast<std::uint32_t>(x);
            const auto uz = static_cast<std::uint32_t>(z);
            const float falloff = Falloff(dist / settings.radius);
            std::vector<float> oldWeights(layerCount);
            for (std::size_t i = 0; i < layerCount; ++i) oldWeights[i] = stack.Layer(i).blend.At(ux, uz);
            const float targetOld = oldWeights[targetLayer];
            const float delta = settings.strength * falloff * (mode == PaintMode::Increase ? 1.0f : -1.0f);
            const float targetNew = std::clamp(targetOld + delta, 0.0f, 1.0f);
            const float actualDelta = targetNew - targetOld;
            if (std::abs(actualDelta) < 1e-6f) continue;
            const float otherSumOld = std::accumulate(oldWeights.begin(), oldWeights.end(), 0.0f) - targetOld;
            stack.Layer(targetLayer).blend.Set(ux, uz, targetNew);
            if (otherSumOld > 1e-6f) {
                for (std::size_t i = 0; i < layerCount; ++i) {
                    if (i == targetLayer) continue;
                    const float share = oldWeights[i] / otherSumOld;
                    stack.Layer(i).blend.Set(ux, uz, std::clamp(oldWeights[i] - actualDelta * share, 0.0f, 1.0f));
                }
            }
            patch.entries.push_back({ux, uz, std::move(oldWeights)});
        }
    }
    return patch;
}

void RevertTexturePatch(TextureLayerStack& stack, const TexturePaintPatch& patch) {
    for (const auto& entry : patch.entries)
        for (std::size_t i = 0; i < entry.oldWeights.size() && i < stack.LayerCount(); ++i)
            stack.Layer(i).blend.Set(entry.x, entry.z, entry.oldWeights[i]);
}

void TexturePaintUndoStack::Push(TexturePaintPatch patch) {
    if (!patch.entries.empty()) { undo_.push_back(std::move(patch)); redo_.clear(); }
}

bool TexturePaintUndoStack::Undo(TextureLayerStack& stack) {
    if (undo_.empty()) return false;
    TexturePaintPatch patch = std::move(undo_.back()); undo_.pop_back();
    TexturePaintPatch redoPatch; redoPatch.entries.reserve(patch.entries.size());
    for (const auto& entry : patch.entries) {
        std::vector<float> current(stack.LayerCount());
        for (std::size_t i = 0; i < stack.LayerCount(); ++i) current[i] = stack.Layer(i).blend.At(entry.x, entry.z);
        redoPatch.entries.push_back({entry.x, entry.z, std::move(current)});
    }
    redo_.push_back(std::move(redoPatch)); RevertTexturePatch(stack, patch); return true;
}

bool TexturePaintUndoStack::Redo(TextureLayerStack& stack) {
    if (redo_.empty()) return false;
    TexturePaintPatch patch = std::move(redo_.back()); redo_.pop_back();
    TexturePaintPatch undoPatch; undoPatch.entries.reserve(patch.entries.size());
    for (const auto& entry : patch.entries) {
        std::vector<float> current(stack.LayerCount());
        for (std::size_t i = 0; i < stack.LayerCount(); ++i) current[i] = stack.Layer(i).blend.At(entry.x, entry.z);
        undoPatch.entries.push_back({entry.x, entry.z, std::move(current)});
    }
    undo_.push_back(std::move(undoPatch)); RevertTexturePatch(stack, patch); return true;
}

void TexturePaintUndoStack::Clear() { undo_.clear(); redo_.clear(); }
} // namespace theseed::mapeditor::core
