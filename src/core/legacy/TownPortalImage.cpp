#include "mapeditor/core/legacy/TownPortalImage.hpp"

#include <algorithm>
#include <cmath>

namespace theseed::mapeditor::core::legacy {

void ApplyRoundedCorners(std::vector<std::uint8_t>& rgba, std::uint32_t size, float radius) {
    if (size == 0 || rgba.size() < static_cast<std::size_t>(size) * size * 4 || radius <= 0.0f) return;
    const float s = static_cast<float>(size);
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            const float px = x + 0.5f, py = y + 0.5f;
            // Abstand zum Mittelpunkt des Eckkreises, nur im Eckquadrat.
            const float cx = px < radius ? radius : (px > s - radius ? s - radius : px);
            const float cy = py < radius ? radius : (py > s - radius ? s - radius : py);
            const float d = std::hypot(px - cx, py - cy);
            const float cover = std::clamp(radius - d + 0.5f, 0.0f, 1.0f);
            if (cover >= 1.0f) continue;
            auto& a = rgba[(static_cast<std::size_t>(y) * size + x) * 4 + 3];
            a = static_cast<std::uint8_t>(std::lround(a * cover));
        }
    }
}

std::vector<std::uint8_t> MakeTownPortalDisabled(std::span<const std::uint8_t> rgba, float saturation, float brightness) {
    std::vector<std::uint8_t> out(rgba.begin(), rgba.end());
    for (std::size_t i = 0; i + 3 < out.size(); i += 4) {
        const float r = out[i], g = out[i + 1], b = out[i + 2];
        const float grey = 0.299f * r + 0.587f * g + 0.114f * b;
        const float c[3] = {r, g, b};
        for (int k = 0; k < 3; ++k) {
            const float v = (grey + (c[k] - grey) * saturation) * brightness;
            out[i + k] = static_cast<std::uint8_t>(std::clamp(std::lround(v), 0L, 255L));
        }
    }
    return out;
}

std::string TownPortalImageRelativePath(const std::string& mapName, bool disabled) {
    return "TownPortal/" + mapName + (disabled ? "_disable" : "") + ".dds";
}

} // namespace theseed::mapeditor::core::legacy
