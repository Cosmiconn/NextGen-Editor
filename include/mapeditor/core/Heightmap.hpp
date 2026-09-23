#pragma once
// Heightmap.hpp
// Kern-Datencontainer des Heightmap-Moduls. Bewusst frei von Rendering-/IO-Abhängigkeiten,
// damit er sowohl vom Editor (GUI) als auch von Tests/Tools/Import-Konvertern ohne
// Grafik-Kontext genutzt werden kann.

#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace theseed::mapeditor::core {

// Repräsentiert ein rechteckiges Vertex-Gitter mit Höhenwerten (row-major: z-major, x-minor).
// Weltposition eines Vertex (x,z): { x * BlockWidth(), At(x,z), z * BlockHeight() }.
class Heightmap {
public:
    Heightmap() = default;
    Heightmap(std::uint32_t width, std::uint32_t height,
               float blockWidth = 50.0f, float blockHeight = 50.0f);

    // Setzt neue Dimensionen; vorhandene Daten gehen verloren, Gitter wird mit fillValue befüllt.
    void Resize(std::uint32_t width, std::uint32_t height, float fillValue = 0.0f);

    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }
    [[nodiscard]] float BlockWidth() const noexcept { return blockWidth_; }
    [[nodiscard]] float BlockHeight() const noexcept { return blockHeight_; }
    // Ungueltige Werte (0, negativ, NaN/Infinity) werden ignoriert (vorheriger Wert bleibt stehen)
    // statt uebernommen zu werden: eine Blockgroesse von 0 fuehrt in SampleWorld() bei Weltkoordinate
    // 0 zu 0/0 = NaN, und das Casten von NaN nach uint32_t ist undefiniertes Verhalten - das loeste
    // den Heightmap::At-Assert aus, ausgeloest durch reine Mausbewegung ueber die Karte
    // (CHANGELOG [0.44.34]). Ursache war meist eine .ini ohne gueltiges OneBlockWidth/-Height -
    // das ist dort inzwischen ebenfalls abgesichert (siehe LegacyMapIni.cpp), diese Pruefung ist
    // die zweite, generelle Verteidigungslinie fuer jeden (auch kuenftigen) Aufrufer.
    void SetBlockSize(float blockWidth, float blockHeight) noexcept {
        if (blockWidth > 0.0f && std::isfinite(blockWidth)) blockWidth_ = blockWidth;
        if (blockHeight > 0.0f && std::isfinite(blockHeight)) blockHeight_ = blockHeight;
    }

    [[nodiscard]] bool InBounds(std::uint32_t x, std::uint32_t z) const noexcept {
        return x < width_ && z < height_;
    }

    // Unchecked im Release-Build (nur assert), da Hot-Path beim Malen/Rendern.
    [[nodiscard]] float At(std::uint32_t x, std::uint32_t z) const noexcept;
    void Set(std::uint32_t x, std::uint32_t z, float value) noexcept;

    // Bilinear interpolierte Höhe an beliebiger Weltposition. Wird später vom
    // Block&Walk-Modul (Bodenhöhe unter einer Kollisionszelle) und vom
    // Objekt-Placement-Modul (Objekt auf Terrain "einrasten") wiederverwendet.
    [[nodiscard]] float SampleWorld(float worldX, float worldZ) const noexcept;

    [[nodiscard]] std::span<const float> Data() const noexcept { return data_; }
    [[nodiscard]] std::span<float> MutableData() noexcept { return data_; }

    // Min/Max der aktuellen Höhenwerte, u.a. für die Graustufen-Vorschau und Kameraeinstellung.
    [[nodiscard]] std::pair<float, float> MinMax() const noexcept;

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    float blockWidth_ = 50.0f;
    float blockHeight_ = 50.0f;
    std::vector<float> data_; // Index = z * width_ + x
};

} // namespace theseed::mapeditor::core
