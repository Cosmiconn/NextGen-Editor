#include "mapeditor/core/EditOps.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace theseed::mapeditor::core {

namespace {

// Gitterbasiertes Value-Noise (kohärent, deterministisch über seed), Wertebereich 0..1.
float Hash01(std::int64_t x, std::int64_t z, std::uint32_t seed) {
    std::uint64_t h = static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ull ^
                      static_cast<std::uint64_t>(z) * 0xC2B2AE3D27D4EB4Full ^ (static_cast<std::uint64_t>(seed) << 32);
    h ^= h >> 33; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 33; h *= 0xC4CEB9FE1A85EC53ull; h ^= h >> 33;
    return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}
float ValueNoise(float x, float z, std::uint32_t seed) {
    const float fx = std::floor(x), fz = std::floor(z);
    const auto ix = static_cast<std::int64_t>(fx), iz = static_cast<std::int64_t>(fz);
    auto smooth = [](float t) { return t * t * (3.0f - 2.0f * t); };
    const float tx = smooth(x - fx), tz = smooth(z - fz);
    const float a = Hash01(ix, iz, seed), b = Hash01(ix + 1, iz, seed);
    const float c = Hash01(ix, iz + 1, seed), d = Hash01(ix + 1, iz + 1, seed);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * tz;
}
// Zwei Oktaven für etwas Detail.
float FractalNoise(float x, float z, std::uint32_t seed) {
    return ValueNoise(x, z, seed) * 0.67f + ValueNoise(x * 2.03f, z * 2.03f, seed + 17u) * 0.33f;
}

// 3x3-Box-Mittelwert um (x,z), Rand wird geklemmt statt umgebrochen.
float BoxAverage(const Heightmap& hm, std::uint32_t x, std::uint32_t z) {
    float sum = 0.0f;
    int count = 0;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const long nx = static_cast<long>(x) + dx;
            const long nz = static_cast<long>(z) + dz;
            if (nx < 0 || nz < 0 || nx >= static_cast<long>(hm.Width()) || nz >= static_cast<long>(hm.Height())) {
                continue;
            }
            sum += hm.At(static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(nz));
            ++count;
        }
    }
    return count > 0 ? sum / static_cast<float>(count) : hm.At(x, z);
}

} // namespace

float BrushFalloffWeight(BrushFalloff falloff, float normalizedDist) {
    const float d = std::clamp(normalizedDist, 0.0f, 1.0f);
    const float t = 1.0f - d;
    switch (falloff) {
        case BrushFalloff::Linear: return t;
        case BrushFalloff::Spherical: return std::sqrt(std::max(0.0f, 1.0f - d * d));
        case BrushFalloff::Tip: return t * t;
        case BrushFalloff::Constant: return d <= 1.0f ? 1.0f : 0.0f;
        case BrushFalloff::Smooth:
        default: return t * t * (3.0f - 2.0f * t);
    }
}

UndoPatch ApplyBrush(Heightmap& heightmap, BrushMode mode, const BrushSettings& settings,
                      float worldX, float worldZ) {
    UndoPatch patch;

    if (heightmap.Width() == 0 || heightmap.Height() == 0 || settings.radius <= 0.0f) {
        return patch;
    }

    // Bounding-Box des Pinsels in Gitterkoordinaten bestimmen, um nicht das ganze Gitter
    // durchlaufen zu müssen.
    const float radiusInVertsX = settings.radius / heightmap.BlockWidth();
    const float radiusInVertsZ = settings.radius / heightmap.BlockHeight();
    const float centerVertX = worldX / heightmap.BlockWidth();
    const float centerVertZ = worldZ / heightmap.BlockHeight();

    const auto x0 = static_cast<std::int64_t>(std::floor(centerVertX - radiusInVertsX));
    const auto x1 = static_cast<std::int64_t>(std::ceil(centerVertX + radiusInVertsX));
    const auto z0 = static_cast<std::int64_t>(std::floor(centerVertZ - radiusInVertsZ));
    const auto z1 = static_cast<std::int64_t>(std::ceil(centerVertZ + radiusInVertsZ));

    const auto strengthNorm = std::clamp(settings.strength >= 0.0f ? settings.strength : 0.0f, 0.0f, 1000.0f);

    for (std::int64_t z = std::max<std::int64_t>(0, z0); z <= std::min<std::int64_t>(heightmap.Height() - 1, z1); ++z) {
        for (std::int64_t x = std::max<std::int64_t>(0, x0); x <= std::min<std::int64_t>(heightmap.Width() - 1, x1); ++x) {
            const float worldVX = static_cast<float>(x) * heightmap.BlockWidth();
            const float worldVZ = static_cast<float>(z) * heightmap.BlockHeight();
            const float dx = worldVX - worldX;
            const float dz = worldVZ - worldZ;
            const float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > settings.radius) {
                continue;
            }

            const float falloff = BrushFalloffWeight(settings.falloff, dist / settings.radius);
            const auto ux = static_cast<std::uint32_t>(x);
            const auto uz = static_cast<std::uint32_t>(z);
            const float oldValue = heightmap.At(ux, uz);
            float newValue = oldValue;

            switch (mode) {
                case BrushMode::Raise:
                    newValue = oldValue + settings.strength * falloff;
                    break;
                case BrushMode::Lower:
                    newValue = oldValue - settings.strength * falloff;
                    break;
                case BrushMode::Smooth: {
                    const float avg = BoxAverage(heightmap, ux, uz);
                    const float blend = std::clamp(strengthNorm * 0.01f, 0.0f, 1.0f) * falloff;
                    newValue = oldValue + (avg - oldValue) * blend;
                    break;
                }
                case BrushMode::Flatten: {
                    const float blend = std::clamp(strengthNorm * 0.01f, 0.0f, 1.0f) * falloff;
                    const float delta = settings.flattenTarget - oldValue;
                    if ((settings.flattenSide == FlattenSide::RaiseOnly && delta < 0.0f) ||
                        (settings.flattenSide == FlattenSide::LowerOnly && delta > 0.0f)) break;
                    newValue = oldValue + delta * blend;
                    break;
                }
                case BrushMode::Noise: {
                    const float scale = std::max(settings.noiseScale, 1.0f);
                    const float n = FractalNoise(worldVX / scale, worldVZ / scale, settings.noiseSeed) * 2.0f - 1.0f;
                    newValue = oldValue + settings.strength * falloff * n;
                    break;
                }
                case BrushMode::Terrace: {
                    if (settings.terraceStep <= 0.0f) break;
                    const float target = std::round(oldValue / settings.terraceStep) * settings.terraceStep;
                    const float blend = std::clamp(strengthNorm * 0.01f, 0.0f, 1.0f) * falloff;
                    newValue = oldValue + (target - oldValue) * blend;
                    break;
                }
                case BrushMode::Sharpen: {
                    const float avg = BoxAverage(heightmap, ux, uz);
                    const float blend = std::clamp(strengthNorm * 0.01f, 0.0f, 1.0f) * falloff;
                    newValue = oldValue + (oldValue - avg) * blend;
                    break;
                }
                case BrushMode::Erode:
                case BrushMode::Ramp:
                    break; // eigene Durchläufe unten bzw. ApplyRamp
            }

            if (newValue != oldValue) {
                patch.entries.push_back({ux, uz, oldValue});
                heightmap.Set(ux, uz, newValue);
            }
        }
    }

    if (mode == BrushMode::Erode) {
        // Thermische Erosion auf einer Kopie: Für jeden Vertex im Radius zum tiefsten der 8
        // Nachbarn; ist der Unterschied grösser als talus, rutscht ein Teil des Überschusses ab.
        // Nachbarn ausserhalb des Radius können dabei Material erhalten (werden mitprotokolliert).
        const std::int64_t bx0 = std::max<std::int64_t>(0, x0 - 1), bz0 = std::max<std::int64_t>(0, z0 - 1);
        const std::int64_t bx1 = std::min<std::int64_t>(heightmap.Width() - 1, x1 + 1);
        const std::int64_t bz1 = std::min<std::int64_t>(heightmap.Height() - 1, z1 + 1);
        const std::int64_t bw = bx1 - bx0 + 1, bh = bz1 - bz0 + 1;
        if (bw <= 0 || bh <= 0) return patch;
        std::vector<float> delta(static_cast<std::size_t>(bw * bh), 0.0f);
        const float blend = std::clamp(strengthNorm * 0.01f, 0.0f, 1.0f);
        const float talus = std::max(settings.erodeTalus, 0.0f);
        for (std::int64_t z = std::max<std::int64_t>(0, z0); z <= std::min<std::int64_t>(heightmap.Height() - 1, z1); ++z) {
            for (std::int64_t x = std::max<std::int64_t>(0, x0); x <= std::min<std::int64_t>(heightmap.Width() - 1, x1); ++x) {
                const float dx = static_cast<float>(x) * heightmap.BlockWidth() - worldX;
                const float dz = static_cast<float>(z) * heightmap.BlockHeight() - worldZ;
                const float dist = std::sqrt(dx * dx + dz * dz);
                if (dist > settings.radius) continue;
                const float w = BrushFalloffWeight(settings.falloff, dist / settings.radius) * blend;
                const float h = heightmap.At(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
                std::int64_t lx = -1, lz = -1;
                float lowest = h;
                for (int oz = -1; oz <= 1; ++oz)
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (!ox && !oz) continue;
                        const std::int64_t nx = x + ox, nz = z + oz;
                        if (nx < 0 || nz < 0 || nx >= heightmap.Width() || nz >= heightmap.Height()) continue;
                        const float nh = heightmap.At(static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(nz));
                        if (nh < lowest) { lowest = nh; lx = nx; lz = nz; }
                    }
                if (lx < 0 || h - lowest <= talus) continue;
                const float moved = (h - lowest - talus) * 0.5f * w;
                delta[static_cast<std::size_t>((z - bz0) * bw + (x - bx0))] -= moved;
                delta[static_cast<std::size_t>((lz - bz0) * bw + (lx - bx0))] += moved;
            }
        }
        for (std::int64_t z = bz0; z <= bz1; ++z)
            for (std::int64_t x = bx0; x <= bx1; ++x) {
                const float d = delta[static_cast<std::size_t>((z - bz0) * bw + (x - bx0))];
                if (d == 0.0f) continue;
                const auto ux = static_cast<std::uint32_t>(x), uz = static_cast<std::uint32_t>(z);
                const float oldValue = heightmap.At(ux, uz);
                patch.entries.push_back({ux, uz, oldValue});
                heightmap.Set(ux, uz, oldValue + d);
            }
    }

    return patch;
}

UndoPatch ApplyRamp(Heightmap& heightmap, float startX, float startZ, float startHeight,
                    float endX, float endZ, float endHeight, float width, float falloffWidth) {
    UndoPatch patch;
    if (heightmap.Width() == 0 || heightmap.Height() == 0 || width <= 0.0f) return patch;
    const float ax = endX - startX, az = endZ - startZ;
    const float len2 = ax * ax + az * az;
    if (len2 <= 1.0e-6f) return patch;
    const float half = width * 0.5f;
    const float reach = half + std::max(falloffWidth, 0.0f);
    const float minX = std::min(startX, endX) - reach, maxX = std::max(startX, endX) + reach;
    const float minZ = std::min(startZ, endZ) - reach, maxZ = std::max(startZ, endZ) + reach;
    const auto x0 = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor(minX / heightmap.BlockWidth())));
    const auto x1 = std::min<std::int64_t>(heightmap.Width() - 1, static_cast<std::int64_t>(std::ceil(maxX / heightmap.BlockWidth())));
    const auto z0 = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor(minZ / heightmap.BlockHeight())));
    const auto z1 = std::min<std::int64_t>(heightmap.Height() - 1, static_cast<std::int64_t>(std::ceil(maxZ / heightmap.BlockHeight())));
    for (std::int64_t z = z0; z <= z1; ++z) {
        for (std::int64_t x = x0; x <= x1; ++x) {
            const float px = static_cast<float>(x) * heightmap.BlockWidth();
            const float pz = static_cast<float>(z) * heightmap.BlockHeight();
            const float t = std::clamp(((px - startX) * ax + (pz - startZ) * az) / len2, 0.0f, 1.0f);
            const float cx = startX + ax * t, cz = startZ + az * t;
            const float d = std::sqrt((px - cx) * (px - cx) + (pz - cz) * (pz - cz));
            if (d > reach) continue;
            float w = 1.0f;
            if (d > half) {
                const float u = (d - half) / std::max(falloffWidth, 1.0e-3f);
                w = 1.0f - u * u * (3.0f - 2.0f * u);
            }
            const auto ux = static_cast<std::uint32_t>(x), uz = static_cast<std::uint32_t>(z);
            const float oldValue = heightmap.At(ux, uz);
            const float target = startHeight + (endHeight - startHeight) * t;
            const float newValue = oldValue + (target - oldValue) * w;
            if (newValue != oldValue) {
                patch.entries.push_back({ux, uz, oldValue});
                heightmap.Set(ux, uz, newValue);
            }
        }
    }
    return patch;
}

void RevertPatch(Heightmap& heightmap, const UndoPatch& patch) {
    for (const auto& entry : patch.entries) {
        heightmap.Set(entry.x, entry.z, entry.oldValue);
    }
}

void UndoStack::Push(UndoPatch patch) {
    if (!patch.entries.empty()) {
        undo_.push_back(std::move(patch));
        redo_.clear(); // neuer Bearbeitungszweig -> alter Redo-Pfad ungültig
    }
}

bool UndoStack::Undo(Heightmap& heightmap) {
    if (undo_.empty()) {
        return false;
    }
    UndoPatch patch = std::move(undo_.back());
    undo_.pop_back();

    // Für Redo den aktuellen (noch nicht rückgängig gemachten) Zustand der betroffenen
    // Vertizes sichern, bevor er überschrieben wird.
    UndoPatch redoPatch;
    redoPatch.entries.reserve(patch.entries.size());
    for (const auto& entry : patch.entries) {
        redoPatch.entries.push_back({entry.x, entry.z, heightmap.At(entry.x, entry.z)});
    }
    redo_.push_back(std::move(redoPatch));

    RevertPatch(heightmap, patch);
    return true;
}

bool UndoStack::Redo(Heightmap& heightmap) {
    if (redo_.empty()) {
        return false;
    }
    UndoPatch patch = std::move(redo_.back());
    redo_.pop_back();

    UndoPatch undoPatch;
    undoPatch.entries.reserve(patch.entries.size());
    for (const auto& entry : patch.entries) {
        undoPatch.entries.push_back({entry.x, entry.z, heightmap.At(entry.x, entry.z)});
    }
    undo_.push_back(std::move(undoPatch));

    RevertPatch(heightmap, patch);
    return true;
}

void UndoStack::Clear() {
    undo_.clear();
    redo_.clear();
}

} // namespace theseed::mapeditor::core
