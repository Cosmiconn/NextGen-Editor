#include "mapeditor/core/EditOps.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace theseed::mapeditor::core {

namespace {

// Kubischer Smoothstep-Falloff: 1.0 im Zentrum, 0.0 am Radiusrand, weicher Übergang.
float Falloff(float normalizedDist) {
    const float t = std::clamp(1.0f - normalizedDist, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
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

            const float falloff = Falloff(dist / settings.radius);
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
                    newValue = oldValue + (settings.flattenTarget - oldValue) * blend;
                    break;
                }
            }

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
