#pragma once
#include "mapeditor/core/Heightmap.hpp"
#include <cstdint>
#include <vector>

namespace theseed::mapeditor::core {

enum class BrushMode { Raise, Lower, Smooth, Flatten };

struct BrushSettings {
    float radius = 200.0f;
    float strength = 10.0f;
    float flattenTarget = 0.0f;
};

struct UndoPatch {
    struct Entry { std::uint32_t x; std::uint32_t z; float oldValue; };
    std::vector<Entry> entries;
};

UndoPatch ApplyBrush(Heightmap& heightmap, BrushMode mode, const BrushSettings& settings,
                     float worldX, float worldZ);
void RevertPatch(Heightmap& heightmap, const UndoPatch& patch);

class UndoStack {
public:
    void Push(UndoPatch patch);
    bool Undo(Heightmap& heightmap);
    bool Redo(Heightmap& heightmap);
    void Clear();
    [[nodiscard]] bool CanUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::size_t UndoDepth() const noexcept { return undo_.size(); }
private:
    std::vector<UndoPatch> undo_;
    std::vector<UndoPatch> redo_;
};

} // namespace theseed::mapeditor::core
