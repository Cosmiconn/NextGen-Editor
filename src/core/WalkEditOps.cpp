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

WalkUndoPatch ApplyWalkBitStamp(WalkGrid& grid, float radius, float worldX, float worldZ, bool blocked,
                                std::uint32_t* changedCells) {
    WalkUndoPatch patch;
    if (grid.Width() == 0 || grid.Height() == 0 || radius <= 0.0f) return patch;
    const float cs = WalkGrid::kCellSize;
    const auto cols = static_cast<std::int64_t>(grid.Cols());
    const auto rows = static_cast<std::int64_t>(grid.Rows());
    const auto cx0 = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor((worldX - radius) / cs)));
    const auto cx1 = std::min<std::int64_t>(cols - 1, static_cast<std::int64_t>(std::floor((worldX + radius) / cs)));
    const auto cz0 = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor((worldZ - radius) / cs)));
    const auto cz1 = std::min<std::int64_t>(rows - 1, static_cast<std::int64_t>(std::floor((worldZ + radius) / cs)));
    std::int64_t minX = cols, minZ = rows, maxX = -1, maxZ = -1;
    for (std::int64_t cz = cz0; cz <= cz1; ++cz) {
        for (std::int64_t cx = cx0; cx <= cx1; ++cx) {
            // Zellmittelpunkt gegen den Kreis; eine Zelle wird IMMER getroffen, wenn der Klick in ihr liegt.
            const float mx = (static_cast<float>(cx) + 0.5f) * cs, mz = (static_cast<float>(cz) + 0.5f) * cs;
            const bool clickInside = static_cast<std::int64_t>(std::floor(worldX / cs)) == cx && static_cast<std::int64_t>(std::floor(worldZ / cs)) == cz;
            const float dx = mx - worldX, dz = mz - worldZ;
            if (!clickInside && dx * dx + dz * dz > radius * radius) continue;
            if (grid.CellBlocked(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz)) == blocked) continue;
            const auto wx = static_cast<std::uint32_t>(cx / WalkGrid::kCellsPerWord);
            const auto wz = static_cast<std::uint32_t>(cz);
            bool recorded = false;
            for (const auto& e : patch.entries) if (e.x == wx && e.z == wz) { recorded = true; break; }
            if (!recorded) patch.entries.push_back({wx, wz, grid.At(wx, wz)});
            grid.SetCellBlocked(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz), blocked);
            minX = std::min(minX, cx); maxX = std::max(maxX, cx); minZ = std::min(minZ, cz); maxZ = std::max(maxZ, cz);
        }
    }
    if (changedCells != nullptr && maxX >= 0) {
        changedCells[0] = static_cast<std::uint32_t>(minX); changedCells[1] = static_cast<std::uint32_t>(minZ);
        changedCells[2] = static_cast<std::uint32_t>(maxX); changedCells[3] = static_cast<std::uint32_t>(maxZ);
    }
    return patch;
}

void ApplyWalkConvexPolygon(WalkGrid& grid, const std::vector<std::pair<float, float>>& polygon, bool blocked,
                            WalkUndoPatch& patch, std::vector<std::uint64_t>& seenWords) {
    if (polygon.size() < 3 || grid.Width() == 0 || grid.Height() == 0) return;
    const float cs = WalkGrid::kCellSize;
    float minX = polygon[0].first, maxX = minX, minZ = polygon[0].second, maxZ = minZ;
    for (const auto& p : polygon) {
        minX = std::min(minX, p.first); maxX = std::max(maxX, p.first);
        minZ = std::min(minZ, p.second); maxZ = std::max(maxZ, p.second);
    }
    const auto cx0 = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor(minX / cs)));
    const auto cx1 = std::min<std::int64_t>(static_cast<std::int64_t>(grid.Cols()) - 1, static_cast<std::int64_t>(std::floor(maxX / cs)));
    const auto cz0 = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor(minZ / cs)));
    const auto cz1 = std::min<std::int64_t>(static_cast<std::int64_t>(grid.Rows()) - 1, static_cast<std::int64_t>(std::floor(maxZ / cs)));
    for (std::int64_t cz = cz0; cz <= cz1; ++cz) {
        for (std::int64_t cx = cx0; cx <= cx1; ++cx) {
            const float px = (static_cast<float>(cx) + 0.5f) * cs, pz = (static_cast<float>(cz) + 0.5f) * cs;
            bool pos = false, neg = false;
            for (std::size_t i = 0; i < polygon.size() && !(pos && neg); ++i) {
                const auto& a = polygon[i];
                const auto& b = polygon[(i + 1) % polygon.size()];
                const float cr = (b.first - a.first) * (pz - a.second) - (b.second - a.second) * (px - a.first);
                if (cr > 0.0f) pos = true;
                if (cr < 0.0f) neg = true;
            }
            if (pos && neg) continue; // ausserhalb des konvexen Polygons
            if (grid.CellBlocked(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz)) == blocked) continue;
            const auto wx = static_cast<std::uint32_t>(cx / WalkGrid::kCellsPerWord);
            const auto wz = static_cast<std::uint32_t>(cz);
            const std::uint64_t key = (static_cast<std::uint64_t>(wz) << 32) | wx;
            if (std::find(seenWords.begin(), seenWords.end(), key) == seenWords.end()) {
                seenWords.push_back(key);
                patch.entries.push_back({wx, wz, grid.At(wx, wz)});
            }
            grid.SetCellBlocked(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz), blocked);
        }
    }
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
