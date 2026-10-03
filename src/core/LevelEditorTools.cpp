#include "mapeditor/core/LevelEditorTools.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace theseed::mapeditor::core::level {

float SnapToStep(float value, float step) noexcept {
    if (!(step > 0.0f) || !std::isfinite(value)) return value;
    return std::round(value / step) * step;
}

std::size_t NearestPresetIndex(std::span<const float> presets, float value) noexcept {
    std::size_t best = 0;
    float bestDiff = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const float d = std::abs(presets[i] - value);
        if (d < bestDiff) { bestDiff = d; best = i; }
    }
    return best;
}

float CameraSpeedMultiplier(int setting) noexcept {
    const int s = std::clamp(setting, kMinCameraSpeedSetting, kMaxCameraSpeedSetting);
    return std::ldexp(1.0f, s - kDefaultCameraSpeedSetting);
}

// ---- Lesezeichen -----------------------------------------------------------------------------

std::size_t CameraBookmarkSet::ValidCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(slots.begin(), slots.end(),
                                                  [](const CameraBookmark& b) { return b.valid; }));
}

std::string CameraBookmarkSet::Serialize() const {
    std::string out = "# NextGen-Editor camera bookmarks v1\n";
    char line[256];
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto& b = slots[i];
        if (!b.valid) continue;
        std::snprintf(line, sizeof(line), "bookmark %zu %.4f %.4f %.4f %.6f %.6f %.4f\n", i,
                      b.targetX, b.targetY, b.targetZ, b.yaw, b.pitch, b.distance);
        out += line;
    }
    return out;
}

namespace {
std::vector<std::string_view> SplitWs(std::string_view s) {
    std::vector<std::string_view> parts;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) ++i;
        const std::size_t start = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\t' && s[i] != '\r') ++i;
        if (i > start) parts.push_back(s.substr(start, i - start));
    }
    return parts;
}

bool ParseFloat(std::string_view s, float& out) {
    // std::from_chars fuer float ist in GCC 13 / MSVC vorhanden; Ergebnis muss endlich sein.
    const auto* first = s.data();
    const auto* last = s.data() + s.size();
    const auto res = std::from_chars(first, last, out);
    return res.ec == std::errc{} && res.ptr == last && std::isfinite(out);
}
} // namespace

CameraBookmarkSet CameraBookmarkSet::Parse(std::string_view text) {
    CameraBookmarkSet set;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::string_view line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;
        if (line.empty() || line.front() == '#') continue;
        const auto parts = SplitWs(line);
        if (parts.size() != 8 || parts[0] != "bookmark") continue;
        unsigned slot = 0;
        const auto sr = std::from_chars(parts[1].data(), parts[1].data() + parts[1].size(), slot);
        if (sr.ec != std::errc{} || slot >= kSlots) continue;
        CameraBookmark b;
        if (!ParseFloat(parts[2], b.targetX) || !ParseFloat(parts[3], b.targetY) ||
            !ParseFloat(parts[4], b.targetZ) || !ParseFloat(parts[5], b.yaw) ||
            !ParseFloat(parts[6], b.pitch) || !ParseFloat(parts[7], b.distance) || !(b.distance > 0.0f))
            continue;
        b.valid = true;
        set.slots[slot] = b;
    }
    return set;
}

std::string BookmarkFileStem(std::string_view mapStem) {
    std::string out;
    out.reserve(mapStem.size());
    for (const char c : mapStem) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-';
        out.push_back(ok ? c : '_');
    }
    if (out.empty()) out = "_unnamed";
    return out;
}

// ---- Ausgabeprotokoll ------------------------------------------------------------------------

LogSeverity ClassifyLogMessage(std::string_view text) {
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto has = [&](std::string_view needle) { return lower.find(needle) != std::string::npos; };
    if (has("fehler") || has("fehlgeschlagen") || has("error") || has("failed") ||
        has("nicht gefunden") || has("not found") || has("ungültig") || has("invalid"))
        return LogSeverity::Error;
    if (has("warnung") || has("warning") || has("achtung") || has("nicht verfügbar") ||
        has("unavailable") || has("deaktiviert") || has("disabled") || has("blockiert") || has("blocked"))
        return LogSeverity::Warning;
    return LogSeverity::Info;
}

const LogEntry& EditorLog::Push(LogSeverity severity, std::string category, std::string text, double timeSeconds) {
    if (!entries_.empty()) {
        auto& last = entries_.back();
        if (last.severity == severity && last.category == category && last.text == text) {
            ++last.repeat;
            last.timeSeconds = timeSeconds;
            return last;
        }
    }
    LogEntry e;
    e.sequence = nextSequence_++;
    e.timeSeconds = timeSeconds;
    e.severity = severity;
    e.category = std::move(category);
    e.text = std::move(text);
    entries_.push_back(std::move(e));
    while (entries_.size() > capacity_) entries_.pop_front();
    return entries_.back();
}

std::size_t EditorLog::Count(LogSeverity severity) const noexcept {
    return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
                                                  [&](const LogEntry& e) { return e.severity == severity; }));
}

std::string EditorLog::ExportText() const {
    std::string out;
    char head[96];
    for (const auto& e : entries_) {
        const char* sev = e.severity == LogSeverity::Error ? "Error"
                        : e.severity == LogSeverity::Warning ? "Warning" : "Info";
        std::snprintf(head, sizeof(head), "[%8.1fs] [%s] ", e.timeSeconds, sev);
        out += head;
        if (!e.category.empty()) out += "[" + e.category + "] ";
        out += e.text;
        if (e.repeat > 1) out += " (x" + std::to_string(e.repeat) + ")";
        out += '\n';
    }
    return out;
}

// ---- Marquee ---------------------------------------------------------------------------------

ScreenRect ScreenRect::FromCorners(ScreenPoint a, ScreenPoint b) noexcept {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
}

bool ScreenRect::Contains(ScreenPoint p) const noexcept {
    return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY;
}

bool ScreenRect::Intersects(const ScreenRect& o) const noexcept {
    return minX <= o.maxX && maxX >= o.minX && minY <= o.maxY && maxY >= o.minY;
}

bool MarqueeHits(const ScreenRect& marquee, std::span<const ScreenPoint> projected, MarqueeMode mode) noexcept {
    if (projected.empty()) return false;
    if (mode == MarqueeMode::Inside) {
        return std::all_of(projected.begin(), projected.end(),
                           [&](const ScreenPoint& p) { return marquee.Contains(p); });
    }
    ScreenRect hull{projected[0].x, projected[0].y, projected[0].x, projected[0].y};
    for (const auto& p : projected) {
        hull.minX = std::min(hull.minX, p.x); hull.maxX = std::max(hull.maxX, p.x);
        hull.minY = std::min(hull.minY, p.y); hull.maxY = std::max(hull.maxY, p.y);
    }
    return marquee.Intersects(hull);
}

std::vector<int> CombineSelection(std::span<const int> current, std::span<const int> hits, SelectionCombine mode) {
    std::vector<int> out;
    const auto contains = [](const std::vector<int>& v, int id) {
        return std::find(v.begin(), v.end(), id) != v.end();
    };
    switch (mode) {
        case SelectionCombine::Replace:
            for (const int id : hits) if (!contains(out, id)) out.push_back(id);
            break;
        case SelectionCombine::Add:
            for (const int id : current) if (!contains(out, id)) out.push_back(id);
            for (const int id : hits) if (!contains(out, id)) out.push_back(id);
            break;
        case SelectionCombine::Remove: {
            const std::vector<int> hitVec(hits.begin(), hits.end());
            for (const int id : current) if (!contains(hitVec, id) && !contains(out, id)) out.push_back(id);
            break;
        }
    }
    return out;
}

// ---- Ausrichten / Verteilen ------------------------------------------------------------------

std::vector<float> AlignValues(std::span<const float> values, AlignTarget target, std::size_t activeIndex) {
    std::vector<float> out(values.begin(), values.end());
    if (values.empty()) return out;
    const auto [mnIt, mxIt] = std::minmax_element(values.begin(), values.end());
    float t = 0.0f;
    switch (target) {
        case AlignTarget::Min: t = *mnIt; break;
        case AlignTarget::Max: t = *mxIt; break;
        case AlignTarget::Center: t = (*mnIt + *mxIt) * 0.5f; break;
        case AlignTarget::Active: t = values[std::min(activeIndex, values.size() - 1)]; break;
    }
    std::fill(out.begin(), out.end(), t);
    return out;
}

std::vector<float> DistributeValues(std::span<const float> values) {
    std::vector<float> out(values.begin(), values.end());
    if (values.size() < 3) return out;
    std::vector<std::size_t> order(values.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return values[a] < values[b]; });
    const float lo = values[order.front()];
    const float hi = values[order.back()];
    const float step = (hi - lo) / static_cast<float>(values.size() - 1);
    for (std::size_t rank = 0; rank < order.size(); ++rank)
        out[order[rank]] = lo + step * static_cast<float>(rank);
    out[order.back()] = hi; // exakt, ohne Rundungsdrift
    return out;
}

// ---- Spieltest -------------------------------------------------------------------------------

bool WalkBlockedAtWorld(const WalkGrid& grid, float worldX, float worldZ) noexcept {
    if (!std::isfinite(worldX) || !std::isfinite(worldZ) || worldX < 0.0f || worldZ < 0.0f) return true;
    const auto cx = static_cast<std::uint64_t>(worldX / WalkGrid::kCellSize);
    const auto cz = static_cast<std::uint64_t>(worldZ / WalkGrid::kCellSize);
    if (cx >= grid.Cols() || cz >= grid.Rows()) return true;
    return grid.CellBlocked(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz));
}

std::optional<std::array<float, 2>> FindNearestWalkable(const WalkGrid& grid, float worldX, float worldZ,
                                                         int maxRadiusCells) {
    if (grid.Cols() == 0 || grid.Rows() == 0) return std::nullopt;
    const auto clampCell = [](float w, std::uint32_t n) {
        const float c = std::floor(w / WalkGrid::kCellSize);
        return static_cast<long long>(std::clamp(c, 0.0f, static_cast<float>(n) - 1.0f));
    };
    const long long ccx = clampCell(worldX, grid.Cols());
    const long long ccz = clampCell(worldZ, grid.Rows());
    const auto test = [&](long long x, long long z) -> std::optional<std::array<float, 2>> {
        if (x < 0 || z < 0 || x >= static_cast<long long>(grid.Cols()) || z >= static_cast<long long>(grid.Rows()))
            return std::nullopt;
        if (grid.CellBlocked(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z))) return std::nullopt;
        return std::array<float, 2>{(static_cast<float>(x) + 0.5f) * WalkGrid::kCellSize,
                                    (static_cast<float>(z) + 0.5f) * WalkGrid::kCellSize};
    };
    if (auto hit = test(ccx, ccz)) return hit;
    for (int r = 1; r <= maxRadiusCells; ++r) {
        // Ring mit Chebyshev-Abstand r; der naechste Treffer nach euklidischem Abstand gewinnt.
        std::optional<std::array<float, 2>> best;
        long long bestD = 0;
        for (long long dz = -r; dz <= r; ++dz) {
            for (long long dx = -r; dx <= r; ++dx) {
                if (std::max(std::llabs(dx), std::llabs(dz)) != r) continue;
                if (auto hit = test(ccx + dx, ccz + dz)) {
                    const long long d = dx * dx + dz * dz;
                    if (!best || d < bestD) { best = hit; bestD = d; }
                }
            }
        }
        if (best) return best;
    }
    return std::nullopt;
}

double WalkableFraction(const WalkGrid& grid, std::size_t sampleStride) {
    if (grid.Cols() == 0 || grid.Rows() == 0) return 0.0;
    const std::size_t stride = std::max<std::size_t>(1, sampleStride);
    std::size_t total = 0, walkable = 0;
    for (std::size_t z = 0; z < grid.Rows(); z += stride) {
        for (std::size_t x = 0; x < grid.Cols(); x += stride) {
            ++total;
            if (!grid.CellBlocked(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z))) ++walkable;
        }
    }
    return total == 0 ? 0.0 : static_cast<double>(walkable) / static_cast<double>(total);
}

bool CircleBlocked(const BlockedQuery& blocked, float x, float z, float radius) {
    if (!blocked) return false;
    if (blocked(x, z)) return true;
    if (!(radius > 0.0f)) return false;
    constexpr float kDiag = 0.70710678f;
    const std::array<std::array<float, 2>, 8> dirs{{
        {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
        {kDiag, kDiag}, {kDiag, -kDiag}, {-kDiag, kDiag}, {-kDiag, -kDiag}}};
    for (const auto& d : dirs)
        if (blocked(x + d[0] * radius, z + d[1] * radius)) return true;
    return false;
}

void StepPlaytest(PlaytestPawn& pawn, const PlaytestInput& input, float dt, const PlaytestConfig& config,
                  const BlockedQuery& blocked, const HeightQuery& height) {
    pawn.lastStepBlocked = false;
    float f = std::clamp(input.forward, -1.0f, 1.0f);
    float r = std::clamp(input.right, -1.0f, 1.0f);
    const float len = std::sqrt(f * f + r * r);
    if (len > 1.0f) { f /= len; r /= len; }
    if (len < 1.0e-4f || !(dt > 0.0f)) {
        if (height) pawn.y = height(pawn.x, pawn.z);
        return;
    }
    // Vorwaerts = (sin(yaw), cos(yaw)); rechts = (cos(yaw), -sin(yaw)) - rechtshaendig in X/Z
    // mit Blick von oben (Norden = +Z oben, Osten = +X rechts).
    const float sy = std::sin(input.viewYaw), cy = std::cos(input.viewYaw);
    const float dirX = sy * f + cy * r;
    const float dirZ = cy * f - sy * r;
    const float speed = config.runSpeed * (input.walk ? config.walkSpeedFactor : 1.0f);
    const float step = speed * std::min(dt, 0.1f);
    const float mx = dirX * step, mz = dirZ * step;
    pawn.facingYaw = std::atan2(dirX, dirZ);

    const bool collide = config.collideWithWalkGrid && static_cast<bool>(blocked);
    const auto freeAt = [&](float x, float z) { return !collide || !CircleBlocked(blocked, x, z, config.radius); };

    const float ox = pawn.x, oz = pawn.z;
    if (freeAt(pawn.x + mx, pawn.z + mz)) {
        pawn.x += mx; pawn.z += mz;
    } else {
        pawn.lastStepBlocked = true;
        // Gleiten entlang der freien Achse (groessere Komponente zuerst).
        const bool xFirst = std::abs(mx) >= std::abs(mz);
        const auto tryAxis = [&](bool xAxis) {
            const float nx = pawn.x + (xAxis ? mx : 0.0f);
            const float nz = pawn.z + (xAxis ? 0.0f : mz);
            if ((xAxis ? std::abs(mx) : std::abs(mz)) < 1.0e-6f) return false;
            if (!freeAt(nx, nz)) return false;
            pawn.x = nx; pawn.z = nz;
            return true;
        };
        if (!tryAxis(xFirst)) tryAxis(!xFirst);
    }
    pawn.distanceTravelled += std::sqrt((pawn.x - ox) * (pawn.x - ox) + (pawn.z - oz) * (pawn.z - oz));
    if (height) pawn.y = height(pawn.x, pawn.z);
}

} // namespace theseed::mapeditor::core::level
