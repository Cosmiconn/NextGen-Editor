#include "mapeditor/core/legacy/Minimap.hpp"

#include <algorithm>
#include <cmath>

namespace theseed::mapeditor::core::legacy {

MinimapCrop MinimapCropOf(const MinimapView& view) {
    const auto norm = [](int v) { return std::clamp(static_cast<float>(v) / 512.0f, 0.0f, 1.0f); };
    // End ist der letzte enthaltene Wert (511 = ganze Karte); manche Zeilen nutzen 512.
    MinimapCrop c{norm(view.startX), norm(view.startY), norm(view.endX + 1), norm(view.endY + 1)};
    if (c.u1 - c.u0 < 1e-3f || c.v1 - c.v0 < 1e-3f) c = {};
    return c;
}

void MapToMinimapImage(const MinimapCrop& crop, float u, float v, float& outX, float& outY) {
    outX = (u - crop.u0) / (crop.u1 - crop.u0);
    outY = (v - crop.v0) / (crop.v1 - crop.v0);
}

void MinimapImageToMap(const MinimapCrop& crop, float x, float y, float& outU, float& outV) {
    outU = crop.u0 + x * (crop.u1 - crop.u0);
    outV = crop.v0 + y * (crop.v1 - crop.v0);
}

std::vector<std::uint8_t> CropResampleRgba(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
                                           const MinimapCrop& crop, std::uint32_t outW, std::uint32_t outH) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(outW) * outH * 4, 0);
    if (width == 0 || height == 0 || rgba.size() < static_cast<std::size_t>(width) * height * 4) return out;
    for (std::uint32_t y = 0; y < outH; ++y) {
        for (std::uint32_t x = 0; x < outW; ++x) {
            float u = 0.0f, v = 0.0f;
            MinimapImageToMap(crop, (x + 0.5f) / outW, (y + 0.5f) / outH, u, v);
            const float sx = std::clamp(u * width - 0.5f, 0.0f, static_cast<float>(width - 1));
            const float sy = std::clamp(v * height - 0.5f, 0.0f, static_cast<float>(height - 1));
            const auto x0 = static_cast<std::uint32_t>(sx), y0 = static_cast<std::uint32_t>(sy);
            const std::uint32_t x1 = std::min(x0 + 1, width - 1), y1 = std::min(y0 + 1, height - 1);
            const float fx = sx - x0, fy = sy - y0;
            for (int c = 0; c < 4; ++c) {
                const auto at = [&](std::uint32_t px, std::uint32_t py) {
                    return static_cast<float>(rgba[(static_cast<std::size_t>(py) * width + px) * 4 + c]);
                };
                const float top = at(x0, y0) * (1 - fx) + at(x1, y0) * fx;
                const float bot = at(x0, y1) * (1 - fx) + at(x1, y1) * fx;
                out[(static_cast<std::size_t>(y) * outW + x) * 4 + c] =
                    static_cast<std::uint8_t>(std::clamp(std::lround(top * (1 - fy) + bot * fy), 0L, 255L));
            }
        }
    }
    return out;
}

namespace {
// Deterministisches Rauschen für den ausgefransten Rand.
float Hash(int x, int y) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

float SmoothNoise(float x, float scale, int seed) {
    const float p = x / scale;
    const int i = static_cast<int>(std::floor(p));
    const float f = p - i, t = f * f * (3 - 2 * f);
    return Hash(i, seed) * (1 - t) + Hash(i + 1, seed) * t;
}
} // namespace

std::vector<std::uint8_t> StylizeMinimap(std::span<const std::uint8_t> rgba, std::span<const std::uint8_t> walkable,
                                         std::uint32_t size, const MinimapStyle& style) {
    const std::size_t n = static_cast<std::size_t>(size) * size;
    std::vector<std::uint8_t> out(n * 4, 255);
    if (size == 0 || rgba.size() < n * 4) return out;
    const bool hasMask = walkable.size() >= n;
    const auto walk = [&](int x, int y) {
        if (!hasMask) return true;
        x = std::clamp(x, 0, static_cast<int>(size) - 1);
        y = std::clamp(y, 0, static_cast<int>(size) - 1);
        return walkable[static_cast<std::size_t>(y) * size + x] != 0;
    };
    const int s = static_cast<int>(size);
    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            // Weichzeichnen (3x3), damit Texturdetails zu Farbflächen werden.
            float c[3] = {0, 0, 0};
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int sx = std::clamp(x + dx, 0, s - 1), sy = std::clamp(y + dy, 0, s - 1);
                    for (int k = 0; k < 3; ++k) c[k] += rgba[(static_cast<std::size_t>(sy) * size + sx) * 4 + k];
                }
            for (float& v : c) v /= 9.0f;
            // Sättigung anheben.
            const float grey = 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
            for (float& v : c) v = grey + (v - grey) * style.saturation;
            const bool w = walk(x, y);
            if (!w) {
                // Blockiert: abgedunkelt, leicht warm (wie die gemalten Felsen der Originale).
                c[0] = c[0] * style.blockedDarken + 12.0f;
                c[1] = c[1] * style.blockedDarken + 6.0f;
                c[2] = c[2] * style.blockedDarken;
            }
            if (style.outlines && hasMask) {
                bool edge = false;
                for (int dy = -1; dy <= 1 && !edge; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (walk(x + dx, y + dy) != w) { edge = true; break; }
                if (edge) {
                    const float a = 0.82f;
                    c[0] = c[0] * (1 - a) + 58.0f * a;
                    c[1] = c[1] * (1 - a) + 38.0f * a;
                    c[2] = c[2] * (1 - a) + 22.0f * a;
                }
            }
            if (style.parchmentFrame) {
                // Ausgefranster Rand: Breite schwankt per Rauschen entlang jeder Kante.
                const float d = static_cast<float>(std::min(std::min(x, y), std::min(s - 1 - x, s - 1 - y)));
                const float along = static_cast<float>((x < y) == (x < s - 1 - y) ? x : y);
                const float ragged = s * (0.022f + 0.02f * SmoothNoise(along, s * 0.03f, x < y ? 1 : 2));
                if (d < ragged) {
                    c[0] = 46.0f; c[1] = 32.0f; c[2] = 20.0f;
                } else if (d < ragged + s * 0.03f) {
                    const float t = (d - ragged) / (s * 0.03f); // 0 am Rand -> 1 innen
                    const float k = 0.55f + 0.45f * t;
                    c[0] = c[0] * k + 30.0f * (1 - k);
                    c[1] = c[1] * k + 20.0f * (1 - k);
                    c[2] = c[2] * k + 10.0f * (1 - k);
                }
            }
            std::uint8_t* o = &out[(static_cast<std::size_t>(y) * size + x) * 4];
            for (int k = 0; k < 3; ++k) o[k] = static_cast<std::uint8_t>(std::clamp(std::lround(c[k]), 0L, 255L));
            o[3] = 255;
        }
    }
    return out;
}

std::string MinimapRelativePath(const std::string& mapFolderName) { return "minimap/" + mapFolderName + ".dds"; }

} // namespace theseed::mapeditor::core::legacy
