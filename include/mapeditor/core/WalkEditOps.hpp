#pragma once
// WalkEditOps.hpp
// Bearbeitung des Block&Walk-Gitters. Anders als Heightmap/Textur-Gewichte sind die Werte
// diskret (siehe WalkGrid.hpp) - deshalb kein Falloff-Blending, sondern ein einfacher
// "Stempel": alle Zellen im Radius werden auf einen festen Rohwert gesetzt.

#include "mapeditor/core/WalkGrid.hpp"

#include <cstdint>
#include <vector>

namespace theseed::mapeditor::core {

struct WalkStampSettings {
    float radius = 100.0f;      // Weltraum-Radius
    std::int16_t value = -1;    // Zielwert (-1 = in der Referenzdatei "frei/unbelegt")
};

struct WalkUndoPatch {
    struct Entry {
        std::uint32_t x;
        std::uint32_t z;
        std::int16_t oldValue;
    };
    std::vector<Entry> entries;
};

// Setzt alle Zellen im Kreis um (worldX, worldZ) auf settings.value (harte Kante, kein Falloff -
// passend für diskrete Flag-/Bitmask-Werte statt kontinuierlicher Höhen/Gewichte).
WalkUndoPatch ApplyWalkStamp(
    WalkGrid& grid,
    const WalkStampSettings& settings,
    float worldX, float worldZ,
    float blockWidth, float blockHeight);

void RevertWalkPatch(WalkGrid& grid, const WalkUndoPatch& patch);

class WalkUndoStack {
public:
    void Push(WalkUndoPatch patch);
    bool Undo(WalkGrid& grid);
    bool Redo(WalkGrid& grid);
    void Clear();

    [[nodiscard]] bool CanUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !redo_.empty(); }

private:
    std::vector<WalkUndoPatch> undo_;
    std::vector<WalkUndoPatch> redo_;
};

} // namespace theseed::mapeditor::core
