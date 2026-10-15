#include "mapeditor/core/WalkGenerate.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace theseed::mapeditor::core {

namespace {
float SpanX(const Heightmap& h) { return static_cast<float>(h.Width() > 1 ? h.Width() - 1 : 0) * h.BlockWidth(); }
float SpanZ(const Heightmap& h) { return static_cast<float>(h.Height() > 1 ? h.Height() - 1 : 0) * h.BlockHeight(); }
} // namespace

float TerrainSlopeDegrees(const Heightmap& heightmap, float worldX, float worldZ) {
    const float sx = SpanX(heightmap), sz = SpanZ(heightmap);
    if (sx <= 0.0f || sz <= 0.0f) return 0.0f;
    const float d = WalkGrid::kCellSize;
    const float x0 = std::clamp(worldX - d, 0.0f, sx), x1 = std::clamp(worldX + d, 0.0f, sx);
    const float z0 = std::clamp(worldZ - d, 0.0f, sz), z1 = std::clamp(worldZ + d, 0.0f, sz);
    const float dx = x1 > x0 ? (heightmap.SampleWorld(x1, worldZ) - heightmap.SampleWorld(x0, worldZ)) / (x1 - x0) : 0.0f;
    const float dz = z1 > z0 ? (heightmap.SampleWorld(worldX, z1) - heightmap.SampleWorld(worldX, z0)) / (z1 - z0) : 0.0f;
    return std::atan(std::sqrt(dx * dx + dz * dz)) * 57.29578f;
}

bool TerrainBlocksCell(const Heightmap& heightmap, const WalkGenerateSettings& settings, std::uint32_t cellX,
                       std::uint32_t cellZ) {
    const float x = (static_cast<float>(cellX) + 0.5f) * WalkGrid::kCellSize;
    const float z = (static_cast<float>(cellZ) + 0.5f) * WalkGrid::kCellSize;
    if (x >= SpanX(heightmap) || z >= SpanZ(heightmap)) return true;
    if (settings.useMaxHeight || settings.useMinHeight) {
        const float h = heightmap.SampleWorld(x, z);
        if (settings.useMaxHeight && h > settings.maxHeight) return true;
        if (settings.useMinHeight && h < settings.minHeight) return true;
    }
    return TerrainSlopeDegrees(heightmap, x, z) > settings.maxSlopeDegrees;
}

WalkRegion TerrainChangeRegion(const WalkGrid& grid, const Heightmap& before, const Heightmap& after, float eps,
                               float dilateWorld) {
    WalkRegion region(static_cast<std::size_t>(grid.Cols()) * grid.Rows(),
                      before.Width() != after.Width() || before.Height() != after.Height() ? 1 : 0);
    if (region.empty() || region.front() == 1) return region;
    const auto a = before.Data(), b = after.Data();
    for (std::uint32_t z = 0; z < after.Height(); ++z) {
        for (std::uint32_t x = 0; x < after.Width(); ++x) {
            const std::size_t i = static_cast<std::size_t>(z) * after.Width() + x;
            if (std::abs(a[i] - b[i]) <= eps) continue;
            // Ein Höhenpunkt beeinflusst die angrenzenden Blöcke (bilineare Interpolation).
            const float wx = static_cast<float>(x) * after.BlockWidth(), wz = static_cast<float>(z) * after.BlockHeight();
            AddRectToRegion(region, grid, wx - after.BlockWidth() - dilateWorld, wz - after.BlockHeight() - dilateWorld,
                            wx + after.BlockWidth() + dilateWorld, wz + after.BlockHeight() + dilateWorld);
        }
    }
    return region;
}

void AddRectToRegion(WalkRegion& region, const WalkGrid& grid, float x0, float z0, float x1, float z1) {
    const std::uint32_t cols = grid.Cols(), rows = grid.Rows();
    if (cols == 0 || rows == 0) return;
    if (region.size() != static_cast<std::size_t>(cols) * rows) region.assign(static_cast<std::size_t>(cols) * rows, 0);
    const auto cell = [](float w, std::uint32_t n) {
        return static_cast<std::uint32_t>(std::clamp(std::floor(w / WalkGrid::kCellSize), 0.0f, static_cast<float>(n - 1)));
    };
    if (x1 < 0.0f || z1 < 0.0f) return;
    const std::uint32_t cx0 = cell(std::min(x0, x1), cols), cx1 = cell(std::max(x0, x1), cols);
    const std::uint32_t cz0 = cell(std::min(z0, z1), rows), cz1 = cell(std::max(z0, z1), rows);
    for (std::uint32_t z = cz0; z <= cz1; ++z)
        std::fill(region.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(z) * cols + cx0),
                  region.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(z) * cols + cx1 + 1), std::uint8_t{1});
}

std::size_t RegionCellCount(const WalkRegion& region) {
    return static_cast<std::size_t>(std::count(region.begin(), region.end(), std::uint8_t{1}));
}

WalkGrid GenerateWalkGrid(const WalkGrid& base, const Heightmap& heightmap, const WalkGenerateSettings& settings,
                          const WalkRegion* region, const WalkGrid& extraBlocked) {
    WalkGrid out = base;
    const std::uint32_t cols = base.Cols(), rows = base.Rows();
    if (region && region->size() != static_cast<std::size_t>(cols) * rows) return out;
    const bool all = region == nullptr;
    const bool haveExtra = extraBlocked.Cols() == cols && extraBlocked.Rows() == rows;
    for (std::uint32_t z = 0; z < rows; ++z) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            if (!all && (*region)[static_cast<std::size_t>(z) * cols + x] == 0) continue;
            const bool blocked = TerrainBlocksCell(heightmap, settings, x, z) || (haveExtra && extraBlocked.CellBlocked(x, z));
            out.SetCellBlocked(x, z, blocked);
        }
    }
    return out;
}

WalkGrid UpdateWalkGridForChanges(const WalkGrid& current, const Heightmap& before, const Heightmap& after,
                                  const WalkGenerateSettings& settings, const WalkRegion& region,
                                  const WalkGrid& addBlocked, const WalkGrid& removeBlocked) {
    WalkGrid out = current;
    const std::uint32_t cols = current.Cols(), rows = current.Rows();
    if (region.size() != static_cast<std::size_t>(cols) * rows) return out;
    const bool haveAdd = addBlocked.Cols() == cols && addBlocked.Rows() == rows;
    const bool haveRemove = removeBlocked.Cols() == cols && removeBlocked.Rows() == rows;
    const bool sameTerrainSize = before.Width() == after.Width() && before.Height() == after.Height();
    for (std::uint32_t z = 0; z < rows; ++z) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            if (region[static_cast<std::size_t>(z) * cols + x] == 0) continue;
            const bool nowRule = TerrainBlocksCell(after, settings, x, z);
            if (nowRule || (haveAdd && addBlocked.CellBlocked(x, z))) {
                out.SetCellBlocked(x, z, true);
                continue;
            }
            const bool loadRule = sameTerrainSize && TerrainBlocksCell(before, settings, x, z);
            if (loadRule || (haveRemove && removeBlocked.CellBlocked(x, z))) out.SetCellBlocked(x, z, false);
        }
    }
    return out;
}

WalkGridDiff DiffWalkGrids(const WalkGrid& from, const WalkGrid& to) {
    WalkGridDiff diff;
    if (from.Cols() != to.Cols() || from.Rows() != to.Rows()) return diff;
    for (std::uint32_t z = 0; z < from.Rows(); ++z) {
        for (std::uint32_t w = 0; w < from.Width(); ++w) {
            const auto a = static_cast<std::uint16_t>(from.At(w, z)), b = static_cast<std::uint16_t>(to.At(w, z));
            const auto changed = static_cast<std::uint16_t>(a ^ b);
            const int nb = std::popcount(static_cast<std::uint16_t>(changed & b));
            const int nf = std::popcount(static_cast<std::uint16_t>(changed & a));
            diff.newlyBlocked += static_cast<std::size_t>(nb);
            diff.newlyFree += static_cast<std::size_t>(nf);
            diff.unchanged += static_cast<std::size_t>(16 - nb - nf);
        }
    }
    return diff;
}

} // namespace theseed::mapeditor::core
