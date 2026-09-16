#include "mapeditor/core/BlendMap.hpp"

namespace theseed::mapeditor::core {

BlendMap ResampleBlendMap(const BlendMap& src, std::uint32_t newWidth, std::uint32_t newHeight) {
    BlendMap dst(newWidth, newHeight);
    if (src.Width() == 0 || src.Height() == 0 || newWidth == 0 || newHeight == 0) {
        return dst;
    }
    if (src.Width() == newWidth && src.Height() == newHeight) {
        for (std::uint32_t z = 0; z < newHeight; ++z) {
            for (std::uint32_t x = 0; x < newWidth; ++x) {
                dst.Set(x, z, src.At(x, z));
            }
        }
        return dst;
    }

    for (std::uint32_t y = 0; y < newHeight; ++y) {
        const float srcY = (newHeight > 1)
            ? (static_cast<float>(y) * static_cast<float>(src.Height() - 1) / static_cast<float>(newHeight - 1))
            : 0.0f;
        const auto y0 = static_cast<std::uint32_t>(srcY);
        const std::uint32_t y1 = std::min(y0 + 1, src.Height() - 1);
        const float ty = srcY - static_cast<float>(y0);

        for (std::uint32_t x = 0; x < newWidth; ++x) {
            const float srcX = (newWidth > 1)
                ? (static_cast<float>(x) * static_cast<float>(src.Width() - 1) / static_cast<float>(newWidth - 1))
                : 0.0f;
            const auto x0 = static_cast<std::uint32_t>(srcX);
            const std::uint32_t x1 = std::min(x0 + 1, src.Width() - 1);
            const float tx = srcX - static_cast<float>(x0);

            const float v00 = src.At(x0, y0);
            const float v10 = src.At(x1, y0);
            const float v01 = src.At(x0, y1);
            const float v11 = src.At(x1, y1);
            const float vx0 = v00 + (v10 - v00) * tx;
            const float vx1 = v01 + (v11 - v01) * tx;
            dst.Set(x, y, vx0 + (vx1 - vx0) * ty);
        }
    }
    return dst;
}

} // namespace theseed::mapeditor::core
