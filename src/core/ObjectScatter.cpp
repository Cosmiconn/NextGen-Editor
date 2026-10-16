#include "mapeditor/core/ObjectScatter.hpp"

#include "mapeditor/core/WalkGenerate.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>

namespace theseed::mapeditor::core {

namespace {
std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Quaternion-Produkt a*b (x, y, z, w).
void QuatMul(const float a[4], const float b[4], float out[4]) {
    out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}
} // namespace

std::vector<PlacedObject> ScatterObjects(const Heightmap& heightmap, const ObjectPlacementSet& existing,
                                         const ScatterSettings& settings, float centerX, float centerZ, std::mt19937& rng) {
    std::vector<PlacedObject> out;
    if (settings.models.empty() || settings.radius <= 0.0f || heightmap.Width() < 2) return out;
    const float spanX = static_cast<float>(heightmap.Width() - 1) * heightmap.BlockWidth();
    const float spanZ = static_cast<float>(heightmap.Height() - 1) * heightmap.BlockHeight();
    std::unordered_set<std::string> modelSet;
    for (const auto& m : settings.models) modelSet.insert(Lower(m));

    // Vorhandene Objekte der Modellliste in Reichweite (Kreis + Mindestabstand).
    std::vector<std::pair<float, float>> nearby;
    const float reach = settings.radius + settings.minDistance;
    for (const auto& o : existing.Objects()) {
        const float dx = o.posX - centerX, dz = o.posZ - centerZ;
        if (dx * dx + dz * dz > reach * reach) continue;
        if (modelSet.contains(Lower(o.modelPath))) nearby.emplace_back(o.posX, o.posZ);
    }

    const float area = 3.14159265f * settings.radius * settings.radius;
    const int attempts = std::max(1, static_cast<int>(std::lround(settings.density * area / 10000.0f)));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_int_distribution<std::size_t> pickModel(0, settings.models.size() - 1);
    const float minD2 = settings.minDistance * settings.minDistance;
    for (int a = 0; a < attempts; ++a) {
        const float r = settings.radius * std::sqrt(unit(rng));
        const float t = 6.2831853f * unit(rng);
        const float x = centerX + r * std::cos(t), z = centerZ + r * std::sin(t);
        if (x < 0.0f || z < 0.0f || x > spanX || z > spanZ) continue;
        if (TerrainSlopeDegrees(heightmap, x, z) > settings.maxSlope) continue;
        bool tooClose = false;
        for (const auto& [px, pz] : nearby)
            if ((px - x) * (px - x) + (pz - z) * (pz - z) < minD2) { tooClose = true; break; }
        if (tooClose) continue;
        nearby.emplace_back(x, z);

        PlacedObject o;
        o.modelPath = settings.models[pickModel(rng)];
        o.posX = x;
        o.posZ = z;
        o.posY = heightmap.SampleWorld(x, z) + settings.heightOffset;
        o.scale = settings.minScale + (settings.maxScale - settings.minScale) * unit(rng);
        const float yaw = settings.randomYaw ? 6.2831853f * unit(rng) : 0.0f;
        float q[4] = {0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f)};
        if (settings.alignToSlope) {
            // Normale aus dem Höhengradienten; Drehung der Hochachse (0,1,0) auf die Normale.
            const float d = heightmap.BlockWidth() * 0.5f;
            const float gx = (heightmap.SampleWorld(std::min(x + d, spanX), z) - heightmap.SampleWorld(std::max(x - d, 0.0f), z)) / (2.0f * d);
            const float gz = (heightmap.SampleWorld(x, std::min(z + d, spanZ)) - heightmap.SampleWorld(x, std::max(z - d, 0.0f))) / (2.0f * d);
            float n[3] = {-gx, 1.0f, -gz};
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            for (float& c : n) c /= len;
            // Achse = up x n = (n.z, 0, -n.x), Winkel = acos(n.y)
            const float angle = std::acos(std::clamp(n[1], -1.0f, 1.0f));
            float axis[3] = {n[2], 0.0f, -n[0]};
            const float alen = std::sqrt(axis[0] * axis[0] + axis[2] * axis[2]);
            if (alen > 1e-6f && angle > 1e-4f) {
                const float s = std::sin(angle * 0.5f) / alen;
                const float tilt[4] = {axis[0] * s, 0.0f, axis[2] * s, std::cos(angle * 0.5f)};
                float combined[4];
                QuatMul(tilt, q, combined);
                std::copy(combined, combined + 4, q);
            }
        }
        o.rotX = q[0];
        o.rotY = q[1];
        o.rotZ = q[2];
        o.rotW = q[3];
        out.push_back(std::move(o));
    }
    return out;
}

std::vector<std::size_t> ScatteredObjectsInRadius(const ObjectPlacementSet& set, const std::vector<std::string>& models,
                                                  float centerX, float centerZ, float radius) {
    std::unordered_set<std::string> modelSet;
    for (const auto& m : models) modelSet.insert(Lower(m));
    std::vector<std::size_t> out;
    for (std::size_t i = set.Count(); i-- > 0;) {
        const auto& o = set.At(i);
        const float dx = o.posX - centerX, dz = o.posZ - centerZ;
        if (dx * dx + dz * dz <= radius * radius && modelSet.contains(Lower(o.modelPath))) out.push_back(i);
    }
    return out;
}

} // namespace theseed::mapeditor::core
