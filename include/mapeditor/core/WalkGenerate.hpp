#pragma once
// WalkGenerate.hpp
// Block&Walk (SHBD) aus dem Terrain neu berechnen. Die Originalkarten lassen sich nicht vollständig
// aus Regeln ableiten (Meer, Objekte und Handarbeit sind ebenfalls gesperrt), wohl aber die Hänge:
// auf Roumen und Uruga sind ab 30° Neigung 82-95 % der Zellen blockiert (Messung siehe CHANGELOG).
// Deshalb rechnet der Editor standardmäßig nur dort neu, wo sich Terrain oder Objekte geändert
// haben, und lässt das Original sonst unangetastet.

#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/WalkGrid.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace theseed::mapeditor::core {

struct WalkGenerateSettings {
    float maxSlopeDegrees = 30.0f; // steiler = blockiert
    bool useMaxHeight = false;     // höher als maxHeight = blockiert (z.B. Gipfel)
    float maxHeight = 0.0f;
    bool useMinHeight = false;     // tiefer als minHeight = blockiert (z.B. unter Wasser)
    float minHeight = 0.0f;
};

// Neigung des Terrains an einer Weltposition in Grad (Zentraldifferenz über eine SHBD-Zelle).
[[nodiscard]] float TerrainSlopeDegrees(const Heightmap& heightmap, float worldX, float worldZ);

// true, wenn die Terrain-Regeln die Zelle sperren; Zellen außerhalb der Terrainfläche sind gesperrt.
[[nodiscard]] bool TerrainBlocksCell(const Heightmap& heightmap, const WalkGenerateSettings& settings,
                                     std::uint32_t cellX, std::uint32_t cellZ);

// Zellmaske (Cols*Rows, 1 = neu berechnen).
using WalkRegion = std::vector<std::uint8_t>;

// Zellen im Umkreis `dilateWorld` um jeden Heightmap-Punkt, dessen Höhe sich um mehr als eps geändert
// hat. Immer Cols*Rows groß (ohne Änderung alles 0). Abmessungen von before/after müssen gleich
// sein, sonst ganze Karte.
[[nodiscard]] WalkRegion TerrainChangeRegion(const WalkGrid& grid, const Heightmap& before, const Heightmap& after,
                                             float eps = 0.5f, float dilateWorld = 12.5f);

// Fügt ein Weltrechteck [x0,x1] x [z0,z1] zur Region hinzu (legt sie bei Bedarf an).
void AddRectToRegion(WalkRegion& region, const WalkGrid& grid, float x0, float z0, float x1, float z1);

[[nodiscard]] std::size_t RegionCellCount(const WalkRegion& region);

// Neu berechnet nur Zellen in `region` (nullptr = ganze Karte): blockiert = Terrain-Regel ODER Zelle
// in `extraBlocked` gesetzt (z.B. Objekt-Grundflächen; leeres Gitter = keine). Übrige Zellen bleiben
// wie in `base`.
[[nodiscard]] WalkGrid GenerateWalkGrid(const WalkGrid& base, const Heightmap& heightmap,
                                        const WalkGenerateSettings& settings, const WalkRegion* region,
                                        const WalkGrid& extraBlocked);

// Änderungsbezogen (Standard): nur Zellen in `region`. Sperrt eine Zelle, wenn die Terrain-Regel sie
// jetzt sperrt oder `addBlocked` sie setzt (Grundflächen neuer/verschobener Objekte); gibt sie nur
// frei, wenn sie beim Laden wegen der Terrain-Regel gesperrt war und es jetzt nicht mehr ist, oder
// wenn `removeBlocked` sie setzt (Grundflächen entfernter/verschobener Objekte, die kein anderes
// Objekt mehr abdeckt). Alles andere - z.B. von Hand gesperrtes Meer - bleibt wie in `current`.
[[nodiscard]] WalkGrid UpdateWalkGridForChanges(const WalkGrid& current, const Heightmap& before, const Heightmap& after,
                                                const WalkGenerateSettings& settings, const WalkRegion& region,
                                                const WalkGrid& addBlocked, const WalkGrid& removeBlocked);

struct WalkGridDiff {
    std::size_t newlyBlocked = 0;
    std::size_t newlyFree = 0;
    std::size_t unchanged = 0;
    [[nodiscard]] double Agreement() const {
        const std::size_t total = newlyBlocked + newlyFree + unchanged;
        return total ? static_cast<double>(unchanged) / static_cast<double>(total) : 1.0;
    }
};
[[nodiscard]] WalkGridDiff DiffWalkGrids(const WalkGrid& from, const WalkGrid& to);

} // namespace theseed::mapeditor::core
