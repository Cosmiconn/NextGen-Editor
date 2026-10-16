#include "mapeditor/core/ViewCulling.hpp"

#include <cmath>

namespace theseed::mapeditor::core {

FrustumPlanes ExtractFrustumPlanes(const Mat4f& m) {
    // Zeile r der Matrix: (m[r], m[4+r], m[8+r], m[12+r]).
    const auto row = [&m](int r) { return std::array<float, 4>{m[r], m[4 + r], m[8 + r], m[12 + r]}; };
    const auto r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
    FrustumPlanes f;
    for (int i = 0; i < 4; ++i) {
        f.planes[0][i] = r3[i] + r0[i]; // links
        f.planes[1][i] = r3[i] - r0[i]; // rechts
        f.planes[2][i] = r3[i] + r1[i]; // unten
        f.planes[3][i] = r3[i] - r1[i]; // oben
        f.planes[4][i] = r3[i] + r2[i]; // nah
        f.planes[5][i] = r3[i] - r2[i]; // fern
    }
    for (auto& p : f.planes) {
        const float len = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        if (len > 1e-12f)
            for (float& v : p) v /= len;
    }
    return f;
}

void TransformAabb(const Mat4f& model, const Vec3f& localMin, const Vec3f& localMax, Vec3f& worldMin, Vec3f& worldMax) {
    for (int r = 0; r < 3; ++r) {
        float lo = model[static_cast<std::size_t>(12 + r)], hi = lo;
        for (int c = 0; c < 3; ++c) {
            const float e = model[static_cast<std::size_t>(c * 4 + r)];
            const float a = e * localMin[static_cast<std::size_t>(c)], b = e * localMax[static_cast<std::size_t>(c)];
            lo += a < b ? a : b;
            hi += a < b ? b : a;
        }
        worldMin[static_cast<std::size_t>(r)] = lo;
        worldMax[static_cast<std::size_t>(r)] = hi;
    }
}

bool AabbInFrustum(const FrustumPlanes& frustum, const Vec3f& worldMin, const Vec3f& worldMax) {
    for (const auto& p : frustum.planes) {
        // Ecke, die am weitesten in Richtung der Ebenennormale liegt.
        const float x = p[0] >= 0.0f ? worldMax[0] : worldMin[0];
        const float y = p[1] >= 0.0f ? worldMax[1] : worldMin[1];
        const float z = p[2] >= 0.0f ? worldMax[2] : worldMin[2];
        if (p[0] * x + p[1] * y + p[2] * z + p[3] < 0.0f) return false;
    }
    return true;
}

} // namespace theseed::mapeditor::core
