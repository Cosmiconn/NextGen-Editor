#pragma once
// WalkGrid.hpp
// Kern-Datencontainer des Block&Walk-Moduls: feineres Sub-Gitter als die Heightmap (in der
// Legacy-Referenzdatei 512x512 bei einer 64x64-Quad-Heightmap, siehe docs/MAP_FORMAT.md), pro
// Zelle ein roher 16-bit-Wert.
//
// BEWUSST kein Enum/Bool für "begehbar/blockiert": die Analyse der echten Rou.shbd zeigt 340
// unterschiedliche Werte mit dem Muster zusammenhängender Bitmasken (z.B. 0x01FF, 0x0FFF,
// 0xFFE0) - vermutlich 16 Richtungs- oder Höhen-Sektoren, von denen ein zusammenhängender
// Bereich blockiert ist. Die exakte Bit-Semantik ist NICHT verifiziert (keine Tool-Doku
// verfügbar) - der rohe int16-Wert wird daher verlustfrei durchgereicht statt auf eine
// vermutete Bedeutung reduziert.

#include <cstdint>
#include <span>
#include <vector>

namespace theseed::mapeditor::core {

class WalkGrid {
public:
    WalkGrid() = default;
    WalkGrid(std::uint32_t width, std::uint32_t height, std::int16_t fillValue = -1)
        : width_(width), height_(height), data_(static_cast<std::size_t>(width) * height, fillValue) {}

    void Resize(std::uint32_t width, std::uint32_t height, std::int16_t fillValue = -1) {
        width_ = width;
        height_ = height;
        data_.assign(static_cast<std::size_t>(width) * height, fillValue);
    }

    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }
    [[nodiscard]] bool InBounds(std::uint32_t x, std::uint32_t z) const noexcept {
        return x < width_ && z < height_;
    }

    [[nodiscard]] std::int16_t At(std::uint32_t x, std::uint32_t z) const noexcept {
        return data_[static_cast<std::size_t>(z) * width_ + x];
    }
    void Set(std::uint32_t x, std::uint32_t z, std::int16_t value) noexcept {
        data_[static_cast<std::size_t>(z) * width_ + x] = value;
    }

    // ---- Zell-Ebene (verifiziert an Rou, Adl, Cypian; CHANGELOG [0.44.29]) ----
    // Jedes 16-Bit-Wort enthaelt 16 nebeneinanderliegende ZELLEN (Bit 0 = linkeste Zelle, LSB-zuerst).
    // Bit gesetzt = BLOCKIERT, Bit 0 = begehbar (Serverpunkte - NPCs, Waypoints - liegen zu 100 % auf
    // Bit 0). Eine Zelle ist IMMER 6.25 Welteinheiten gross, das Gitter quadratisch (Kantenlaenge der
    // laengeren Kartenseite): Adl 951x476 Bloecke -> 7600x7600 Zellen, die Karte belegt nur die ersten
    // 3800 Zeilen. Die frueher angenommene Streckung auf die Kartenform war falsch.
    static constexpr float kCellSize = 6.25f;
    static constexpr std::uint32_t kCellsPerWord = 16;
    [[nodiscard]] std::uint32_t Cols() const noexcept { return width_ * kCellsPerWord; }
    [[nodiscard]] std::uint32_t Rows() const noexcept { return height_; }
    [[nodiscard]] bool CellBlocked(std::uint32_t cx, std::uint32_t cz) const noexcept {
        if (cx >= Cols() || cz >= height_) return true;
        return ((static_cast<std::uint16_t>(At(cx / kCellsPerWord, cz)) >> (cx % kCellsPerWord)) & 1u) != 0;
    }
    void SetCellBlocked(std::uint32_t cx, std::uint32_t cz, bool blocked) noexcept {
        if (cx >= Cols() || cz >= height_) return;
        auto w = static_cast<std::uint16_t>(At(cx / kCellsPerWord, cz));
        const auto mask = static_cast<std::uint16_t>(1u << (cx % kCellsPerWord));
        w = blocked ? static_cast<std::uint16_t>(w | mask) : static_cast<std::uint16_t>(w & ~mask);
        Set(cx / kCellsPerWord, cz, static_cast<std::int16_t>(w));
    }

    [[nodiscard]] std::span<const std::int16_t> Data() const noexcept { return data_; }
    [[nodiscard]] std::span<std::int16_t> MutableData() noexcept { return data_; }

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<std::int16_t> data_;
};

} // namespace theseed::mapeditor::core
