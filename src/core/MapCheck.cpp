#include "mapeditor/core/MapCheck.hpp"

#include "mapeditor/core/LevelEditorTools.hpp"
#include "mapeditor/core/WalkGridIO.hpp"

#include <algorithm>
#include <cmath>

namespace theseed::mapeditor::core {

double BlockedFractionInCircle(const WalkGrid& grid, float x, float z, float radius) {
    const float cell = WalkGrid::kCellSize;
    if (radius < cell) return level::WalkBlockedAtWorld(grid, x, z) ? 1.0 : 0.0;
    const long long cx0 = static_cast<long long>(std::floor((x - radius) / cell));
    const long long cx1 = static_cast<long long>(std::floor((x + radius) / cell));
    const long long cz0 = static_cast<long long>(std::floor((z - radius) / cell));
    const long long cz1 = static_cast<long long>(std::floor((z + radius) / cell));
    // Große Zonen grob abtasten (höchstens ~200x200 Proben), das Ergebnis ist ein Anteil.
    const long long span = std::max(cx1 - cx0, cz1 - cz0) + 1;
    const long long step = std::max(1LL, span / 200);
    const float r2 = radius * radius;
    long long total = 0, blocked = 0;
    for (long long cz = cz0; cz <= cz1; cz += step) {
        for (long long cx = cx0; cx <= cx1; cx += step) {
            const float wx = (static_cast<float>(cx) + 0.5f) * cell;
            const float wz = (static_cast<float>(cz) + 0.5f) * cell;
            if ((wx - x) * (wx - x) + (wz - z) * (wz - z) > r2) continue;
            ++total;
            const bool outside = cx < 0 || cz < 0 || cx >= static_cast<long long>(grid.Cols()) ||
                                 cz >= static_cast<long long>(grid.Rows());
            if (outside || grid.CellBlocked(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz))) ++blocked;
        }
    }
    if (total == 0) return level::WalkBlockedAtWorld(grid, x, z) ? 1.0 : 0.0;
    return static_cast<double>(blocked) / static_cast<double>(total);
}

void CheckPointsWalkable(const WalkGrid& grid, std::span<const MapCheckPoint> points, MapCheckCode code,
                         const WalkableDistanceSeverity& severity, std::vector<MapCheckIssue>& out) {
    for (const auto& p : points) {
        if (!level::WalkBlockedAtWorld(grid, p.x, p.z)) continue;
        MapCheckIssue issue;
        issue.code = code;
        issue.subject = p.label;
        issue.hasPosition = true;
        issue.x = p.x;
        issue.z = p.z;
        issue.ref = p.ref;
        issue.value = -1.0;
        if (const auto free = level::FindNearestWalkable(grid, p.x, p.z, 64))
            issue.value = std::hypot((*free)[0] - p.x, (*free)[1] - p.z);
        const auto d = static_cast<float>(issue.value);
        issue.severity = d < 0.0f || d > severity.warnDistance ? MapCheckSeverity::Error
                         : d > severity.infoDistance          ? MapCheckSeverity::Warning
                                                               : MapCheckSeverity::Info;
        out.push_back(std::move(issue));
    }
}

void CheckSpawnZones(const WalkGrid& grid, std::span<const MapCheckZone> zones, const MapCheckThresholds& thresholds,
                     std::vector<MapCheckIssue>& out) {
    const float sizeX = static_cast<float>(grid.Cols()) * WalkGrid::kCellSize;
    const float sizeZ = static_cast<float>(grid.Rows()) * WalkGrid::kCellSize;
    for (const auto& zone : zones) {
        MapCheckIssue issue;
        issue.subject = zone.label;
        issue.hasPosition = true;
        issue.x = zone.x;
        issue.z = zone.z;
        issue.ref = zone.ref;
        if (zone.x < 0.0f || zone.z < 0.0f || zone.x >= sizeX || zone.z >= sizeZ) {
            issue.severity = MapCheckSeverity::Error;
            issue.code = MapCheckCode::SpawnZoneOutside;
            out.push_back(std::move(issue));
            continue;
        }
        const double fraction = BlockedFractionInCircle(grid, zone.x, zone.z, zone.radius);
        issue.value = fraction;
        if (fraction >= thresholds.zoneMostlyBlocked) {
            issue.severity = MapCheckSeverity::Error;
            issue.code = MapCheckCode::SpawnZoneMostlyBlocked;
            out.push_back(std::move(issue));
        } else if (level::WalkBlockedAtWorld(grid, zone.x, zone.z)) {
            issue.severity = MapCheckSeverity::Info;
            issue.code = MapCheckCode::SpawnZoneCenterBlocked;
            out.push_back(std::move(issue));
        }
    }
}

void CheckPointsInsideMap(std::span<const MapCheckPoint> points, float sizeX, float sizeZ, float margin,
                          MapCheckCode code, MapCheckSeverity severity, std::vector<MapCheckIssue>& out) {
    for (const auto& p : points) {
        if (!std::isfinite(p.x) || !std::isfinite(p.z) || p.x < -margin || p.z < -margin ||
            p.x > sizeX + margin || p.z > sizeZ + margin) {
            MapCheckIssue issue;
            issue.severity = severity;
            issue.code = code;
            issue.subject = p.label;
            issue.hasPosition = std::isfinite(p.x) && std::isfinite(p.z);
            issue.x = p.x;
            issue.z = p.z;
            issue.ref = p.ref;
            out.push_back(std::move(issue));
        }
    }
}

void SortMapCheckIssues(std::vector<MapCheckIssue>& issues) {
    std::stable_sort(issues.begin(), issues.end(), [](const MapCheckIssue& a, const MapCheckIssue& b) {
        if (a.severity != b.severity) return a.severity < b.severity;
        if (a.code != b.code) return a.code < b.code;
        return a.subject < b.subject;
    });
}

std::expected<WalkGrid, std::string> ImportLegacyShbdAutoSize(const std::filesystem::path& file) {
    auto header = PeekLegacyShbdHeader(file);
    if (!header) return std::unexpected(header.error());
    if (header->height == 0) return std::unexpected("SHBD-Kopf ohne Gitterhöhe: " + file.string());
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size <= 8) return std::unexpected("SHBD leer oder nicht lesbar: " + file.string());
    const std::uint64_t elements = (size - 8) / 2;
    if (elements % header->height != 0) return std::unexpected("SHBD-Größe passt nicht zur Gitterhöhe: " + file.string());
    return ImportLegacyShbd(file, static_cast<std::uint32_t>(elements / header->height), header->height);
}

} // namespace theseed::mapeditor::core
