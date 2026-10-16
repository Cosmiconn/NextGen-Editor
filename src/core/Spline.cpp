#include "mapeditor/core/Spline.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace theseed::mapeditor::core {

namespace {
SplinePoint CatmullRom(const SplinePoint& p0, const SplinePoint& p1, const SplinePoint& p2, const SplinePoint& p3, float t) {
    const float t2 = t * t, t3 = t2 * t;
    const auto f = [&](float a, float b, float c, float d) {
        return 0.5f * (2.0f * b + (-a + c) * t + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2 + (-a + 3.0f * b - 3.0f * c + d) * t3);
    };
    return {f(p0.x, p1.x, p2.x, p3.x), f(p0.z, p1.z, p2.z, p3.z)};
}

// Abstand eines Punkts zur Kurve: nächstes Sample (Samples liegen dicht, Abstand <= spacing).
struct Nearest {
    float distance = std::numeric_limits<float>::max();
    std::size_t index = 0;
};
Nearest NearestSample(std::span<const SplineSample> samples, float x, float z) {
    Nearest n;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const float dx = samples[i].x - x, dz = samples[i].z - z;
        const float d = dx * dx + dz * dz;
        if (d < n.distance) { n.distance = d; n.index = i; }
    }
    n.distance = std::sqrt(n.distance);
    return n;
}

float Profile(float distance, const SplineWidth& w) {
    const float half = w.width * 0.5f;
    if (distance <= half) return 1.0f;
    if (w.falloff <= 0.0f) return 0.0f;
    const float t = std::clamp((distance - half) / w.falloff, 0.0f, 1.0f);
    return 1.0f - t * t * (3.0f - 2.0f * t); // weicher Rand
}

void Bounds(std::span<const SplineSample> samples, float margin, float& x0, float& z0, float& x1, float& z1) {
    x0 = z0 = std::numeric_limits<float>::max();
    x1 = z1 = std::numeric_limits<float>::lowest();
    for (const auto& s : samples) {
        x0 = std::min(x0, s.x); x1 = std::max(x1, s.x);
        z0 = std::min(z0, s.z); z1 = std::max(z1, s.z);
    }
    x0 -= margin; z0 -= margin; x1 += margin; z1 += margin;
}
} // namespace

std::vector<SplineSample> SampleSpline(std::span<const SplinePoint> points, float spacing) {
    std::vector<SplineSample> out;
    if (points.size() < 2 || spacing <= 0.0f) return out;
    // Dichte Polylinie durch Catmull-Rom, danach gleichmäßig nach Bogenlänge abtasten.
    std::vector<SplinePoint> dense;
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const auto& p0 = points[i == 0 ? 0 : i - 1];
        const auto& p1 = points[i];
        const auto& p2 = points[i + 1];
        const auto& p3 = points[std::min(i + 2, points.size() - 1)];
        const float len = std::hypot(p2.x - p1.x, p2.z - p1.z);
        const int steps = std::max(4, static_cast<int>(len / std::max(1.0f, spacing * 0.25f)));
        for (int s = 0; s < steps; ++s) dense.push_back(CatmullRom(p0, p1, p2, p3, static_cast<float>(s) / static_cast<float>(steps)));
    }
    dense.push_back(points.back());
    float travelled = 0.0f, nextAt = 0.0f;
    for (std::size_t i = 0; i + 1 < dense.size(); ++i) {
        const float segX = dense[i + 1].x - dense[i].x, segZ = dense[i + 1].z - dense[i].z;
        const float segLen = std::hypot(segX, segZ);
        if (segLen <= 1e-6f) continue;
        while (nextAt <= travelled + segLen) {
            const float t = (nextAt - travelled) / segLen;
            out.push_back({dense[i].x + segX * t, dense[i].z + segZ * t, segX / segLen, segZ / segLen, nextAt});
            nextAt += spacing;
        }
        travelled += segLen;
    }
    if (out.empty() || out.back().distance < travelled - 1e-3f) {
        const auto& a = dense[dense.size() - 2];
        const auto& b = dense.back();
        const float len = std::max(1e-6f, std::hypot(b.x - a.x, b.z - a.z));
        out.push_back({b.x, b.z, (b.x - a.x) / len, (b.z - a.z) / len, travelled});
    }
    return out;
}

UndoPatch FlattenAlongSpline(Heightmap& heightmap, std::span<const SplineSample> samples, const SplineWidth& width,
                             float smoothDistance) {
    UndoPatch patch;
    if (samples.size() < 2 || heightmap.Width() < 2) return patch;
    // Weghöhe je Sample: Geländehöhe, über +-smoothDistance gemittelt.
    std::vector<float> raw(samples.size()), road(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) raw[i] = heightmap.SampleWorld(samples[i].x, samples[i].z);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        float sum = 0.0f;
        int n = 0;
        for (std::size_t j = 0; j < samples.size(); ++j)
            if (std::abs(samples[j].distance - samples[i].distance) <= smoothDistance) { sum += raw[j]; ++n; }
        road[i] = n ? sum / static_cast<float>(n) : raw[i];
    }
    float x0, z0, x1, z1;
    Bounds(samples, width.width * 0.5f + width.falloff, x0, z0, x1, z1);
    const float bw = heightmap.BlockWidth(), bh = heightmap.BlockHeight();
    const auto clampIndex = [](float v, std::uint32_t n) { return static_cast<std::uint32_t>(std::clamp(v, 0.0f, static_cast<float>(n - 1))); };
    for (std::uint32_t z = clampIndex(std::floor(z0 / bh), heightmap.Height()); z <= clampIndex(std::ceil(z1 / bh), heightmap.Height()); ++z) {
        for (std::uint32_t x = clampIndex(std::floor(x0 / bw), heightmap.Width()); x <= clampIndex(std::ceil(x1 / bw), heightmap.Width()); ++x) {
            const auto nearest = NearestSample(samples, static_cast<float>(x) * bw, static_cast<float>(z) * bh);
            const float w = Profile(nearest.distance, width);
            if (w <= 0.0f) continue;
            const float old = heightmap.At(x, z);
            const float next = old + (road[nearest.index] - old) * w;
            if (std::abs(next - old) < 1e-4f) continue;
            patch.entries.push_back({x, z, old});
            heightmap.Set(x, z, next);
        }
    }
    return patch;
}

TexturePaintPatch PaintAlongSpline(TextureLayerStack& stack, std::size_t layer, const Heightmap& heightmap,
                                   std::span<const SplineSample> samples, const SplineWidth& width) {
    TexturePaintPatch patch;
    if (layer >= stack.LayerCount() || samples.size() < 2 || heightmap.Width() < 2 || stack.Width() == 0) return patch;
    auto& target = stack.Layer(layer);
    const float bw = heightmap.BlockWidth(), bh = heightmap.BlockHeight();
    float originX = 0.0f, originZ = 0.0f;
    float spanX = static_cast<float>(heightmap.Width() - 1) * bw, spanZ = static_cast<float>(heightmap.Height() - 1) * bh;
    if (target.regionWidth > 0.0f && target.regionHeight > 0.0f) {
        originX = target.regionStartX * bw;
        originZ = target.regionStartY * bh;
        spanX = target.regionWidth * bw;
        spanZ = target.regionHeight * bh;
    }
    const float cellX = spanX / static_cast<float>(stack.Width()), cellZ = spanZ / static_cast<float>(stack.Height());
    float x0, z0, x1, z1;
    Bounds(samples, width.width * 0.5f + width.falloff, x0, z0, x1, z1);
    const auto clampIndex = [](float v, std::uint32_t n) { return static_cast<std::uint32_t>(std::clamp(v, 0.0f, static_cast<float>(n - 1))); };
    const std::size_t layerCount = stack.LayerCount();
    for (std::uint32_t z = clampIndex(std::floor((z0 - originZ) / cellZ), stack.Height()); z <= clampIndex(std::ceil((z1 - originZ) / cellZ), stack.Height()); ++z) {
        for (std::uint32_t x = clampIndex(std::floor((x0 - originX) / cellX), stack.Width()); x <= clampIndex(std::ceil((x1 - originX) / cellX), stack.Width()); ++x) {
            const float w = Profile(NearestSample(samples, originX + static_cast<float>(x) * cellX, originZ + static_cast<float>(z) * cellZ).distance, width);
            const float old = target.blend.At(x, z);
            if (w <= old + 1e-4f) continue;
            TexturePaintPatch::Entry entry{x, z, std::vector<float>(layerCount)};
            for (std::size_t i = 0; i < layerCount; ++i) entry.oldWeights[i] = stack.Layer(i).blend.At(x, z);
            patch.entries.push_back(std::move(entry));
            target.blend.Set(x, z, w);
        }
    }
    return patch;
}

std::vector<PlacedObject> ObjectsAlongSpline(std::span<const SplinePoint> points, const Heightmap& heightmap,
                                             const SplineObjectSettings& settings) {
    std::vector<PlacedObject> out;
    if (settings.model.empty() || settings.spacing <= 0.0f) return out;
    for (const auto& s : SampleSpline(points, settings.spacing)) {
        // Links = Richtung um 90° gegen den Uhrzeigersinn (von oben, +Z = Norden): (-dz, dx).
        const float nx = -s.dirZ, nz = s.dirX;
        std::vector<float> offsets;
        switch (settings.side) {
            case SplineSide::Center: offsets = {0.0f}; break;
            case SplineSide::Left: offsets = {settings.sideOffset}; break;
            case SplineSide::Right: offsets = {-settings.sideOffset}; break;
            case SplineSide::Both: offsets = {settings.sideOffset, -settings.sideOffset}; break;
        }
        // Gierwinkel so, dass die lokale +X-Achse entlang des Wegs zeigt (Drehung um +Y).
        const float yaw = std::atan2(-s.dirZ, s.dirX) + settings.yawOffsetDegrees * 3.14159265f / 180.0f;
        for (const float off : offsets) {
            PlacedObject o;
            o.modelPath = settings.model;
            o.posX = s.x + nx * off;
            o.posZ = s.z + nz * off;
            o.posY = heightmap.Width() > 1 ? heightmap.SampleWorld(o.posX, o.posZ) : 0.0f;
            o.rotY = std::sin(yaw * 0.5f);
            o.rotW = std::cos(yaw * 0.5f);
            o.scale = settings.scale;
            out.push_back(std::move(o));
        }
    }
    return out;
}

} // namespace theseed::mapeditor::core
