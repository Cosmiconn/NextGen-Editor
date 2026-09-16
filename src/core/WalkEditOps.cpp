#include "mapeditor/core/WalkEditOps.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace theseed::mapeditor::core {

WalkUndoPatch ApplyWalkStamp(
    WalkGrid& grid,
    const WalkStampSettings& settings,
    float worldX, float worldZ,
    float blockWidth, float blockHeight) {

    WalkUndoPatch patch;
    if (grid.Width() == 0 || grid.Height() == 0 || settings.radius <= 0.0f) {
        return patch;
    }

    const float radiusInVertsX = settings.radius / blockWidth;
    const float radiusInVertsZ = settings.radius / blockHeight;
    const float centerVertX = worldX / blockWidth;
    const float centerVertZ = worldZ / blockHeight;

    const auto x0 = static_cast<std::int64_t>(std::floor(centerVertX - radiusInVertsX));
    const auto x1 = static_cast<std::int64_t>(std::ceil(centerVertX + radiusInVertsX));
    const auto z0 = static_cast<std::int64_t>(std::floor(centerVertZ - radiusInVertsZ));
    const auto z1 = static_cast<std::int64_t>(std::ceil(centerVertZ + radiusInVertsZ));

    const auto w = static_cast<std::int64_t>(grid.Width());
    const auto h = static_cast<std::int64_t>(grid.Height());

    for (std::int64_t z = std::max<std::int64_t>(0, z0); z <= std::min<std::int64_t>(h - 1, z1); ++z) {
        for (std::int64_t x = std::max<std::int64_t>(0, x0); x <= std::min<std::int64_t>(w - 1, x1); ++x) {
            const float worldVX = static_cast<float>(x) * blockWidth;
            const float worldVZ = static_cast<float>(z) * blockHeight;
            const float dx = worldVX - worldX;
            const float dz = worldVZ - worldZ;
            if (std::sqrt(dx * dx + dz * dz) > settings.radius) continue;

            const auto ux = static_cast<std::uint32_t>(x);
            const auto uz = static_cast<std::uint32_t>(z);
            const std::int16_t oldValue = grid.At(ux, uz);
            if (oldValue == settings.value) continue;

            patch.entries.push_back({ux, uz, oldValue});
            grid.Set(ux, uz, settings.value);
        }
    }

    return patch;
}

void RevertWalkPatch(WalkGrid& grid, const WalkUndoPatch& patch) {
    for (const auto& entry : patch.entries) {
        grid.Set(entry.x, entry.z, entry.oldValue);
    }
}

void WalkUndoStack::Push(WalkUndoPatch patch) {
    if (!patch.entries.empty()) {
        undo_.push_back(std::move(patch));
        redo_.clear();
    }
}

bool WalkUndoStack::Undo(WalkGrid& grid) {
    if (undo_.empty()) return false;
    WalkUndoPatch patch = std::move(undo_.back());
    undo_.pop_back();

    WalkUndoPatch redoPatch;
    redoPatch.entries.reserve(patch.entries.size());
    for (const auto& entry : patch.entries) {
        redoPatch.entries.push_back({entry.x, entry.z, grid.At(entry.x, entry.z)});
    }
    redo_.push_back(std::move(redoPatch));

    RevertWalkPatch(grid, patch);
    return true;
}

bool WalkUndoStack::Redo(WalkGrid& grid) {
    if (redo_.empty()) return false;
    WalkUndoPatch patch = std::move(redo_.back());
    redo_.pop_back();

    WalkUndoPatch undoPatch;
    undoPatch.entries.reserve(patch.entries.size());
    for (const auto& entry : patch.entries) {
        undoPatch.entries.push_back({entry.x, entry.z, grid.At(entry.x, entry.z)});
    }
    undo_.push_back(std::move(undoPatch));

    RevertWalkPatch(grid, patch);
    return true;
}

void WalkUndoStack::Clear() {
    undo_.clear();
    redo_.clear();
}

} // namespace theseed::mapeditor::core
