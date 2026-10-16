#include "mapeditor/core/TextureRules.hpp"

#include "mapeditor/core/WalkGenerate.hpp"

#include <algorithm>
#include <cmath>

namespace theseed::mapeditor::core {

namespace {
// 1 innerhalb [lo, hi], linear auf 0 über `feather` außerhalb.
float BandWeight(float value, float lo, float hi, float feather) {
    if (value >= lo && value <= hi) return 1.0f;
    if (feather <= 0.0f) return 0.0f;
    const float d = value < lo ? lo - value : value - hi;
    return std::clamp(1.0f - d / feather, 0.0f, 1.0f);
}
} // namespace

float TextureRuleWeight(const Heightmap& heightmap, const TextureRule& rule, float worldX, float worldZ) {
    float w = std::clamp(rule.strength, 0.0f, 1.0f);
    if (rule.useSlope)
        w *= BandWeight(TerrainSlopeDegrees(heightmap, worldX, worldZ), rule.minSlope, rule.maxSlope, rule.slopeFeather);
    if (rule.useHeight && w > 0.0f)
        w *= BandWeight(heightmap.SampleWorld(worldX, worldZ), rule.minHeight, rule.maxHeight, rule.heightFeather);
    return w;
}

TexturePaintPatch ApplyTextureRule(TextureLayerStack& stack, std::size_t layer, const Heightmap& heightmap,
                                   const TextureRule& rule, const TextureRuleArea& area) {
    TexturePaintPatch patch;
    if (layer >= stack.LayerCount() || stack.Width() == 0 || stack.Height() == 0 || heightmap.Width() < 2) return patch;
    auto& target = stack.Layer(layer);
    const float bw = heightmap.BlockWidth(), bh = heightmap.BlockHeight();
    // Weltfläche, die die Maske abdeckt (wie beim Malen: Pixel x liegt bei originX + x * cell).
    float originX = 0.0f, originZ = 0.0f;
    float spanX = static_cast<float>(heightmap.Width() - 1) * bw, spanZ = static_cast<float>(heightmap.Height() - 1) * bh;
    if (target.regionWidth > 0.0f && target.regionHeight > 0.0f) {
        originX = target.regionStartX * bw;
        originZ = target.regionStartY * bh;
        spanX = target.regionWidth * bw;
        spanZ = target.regionHeight * bh;
    }
    const float cellX = spanX / static_cast<float>(stack.Width()), cellZ = spanZ / static_cast<float>(stack.Height());
    std::uint32_t x0 = 0, x1 = stack.Width() - 1, z0 = 0, z1 = stack.Height() - 1;
    if (!area.wholeMap) {
        const auto clampCell = [](float v, std::uint32_t n) {
            return static_cast<std::uint32_t>(std::clamp(v, 0.0f, static_cast<float>(n - 1)));
        };
        x0 = clampCell(std::floor((area.centerX - area.radius - originX) / cellX), stack.Width());
        x1 = clampCell(std::ceil((area.centerX + area.radius - originX) / cellX), stack.Width());
        z0 = clampCell(std::floor((area.centerZ - area.radius - originZ) / cellZ), stack.Height());
        z1 = clampCell(std::ceil((area.centerZ + area.radius - originZ) / cellZ), stack.Height());
    }
    const std::size_t layerCount = stack.LayerCount();
    for (std::uint32_t z = z0; z <= z1; ++z) {
        for (std::uint32_t x = x0; x <= x1; ++x) {
            const float wx = originX + static_cast<float>(x) * cellX;
            const float wz = originZ + static_cast<float>(z) * cellZ;
            if (!area.wholeMap) {
                const float dx = wx - area.centerX, dz = wz - area.centerZ;
                if (dx * dx + dz * dz > area.radius * area.radius) continue;
            }
            const float ruleWeight = TextureRuleWeight(heightmap, rule, wx, wz);
            const float old = target.blend.At(x, z);
            const float next = rule.mode == TextureRuleMode::Replace ? ruleWeight : std::max(old, ruleWeight);
            if (std::abs(next - old) < 1e-4f) continue;
            TexturePaintPatch::Entry entry{x, z, std::vector<float>(layerCount)};
            for (std::size_t i = 0; i < layerCount; ++i) entry.oldWeights[i] = stack.Layer(i).blend.At(x, z);
            patch.entries.push_back(std::move(entry));
            target.blend.Set(x, z, next);
        }
    }
    return patch;
}

} // namespace theseed::mapeditor::core
