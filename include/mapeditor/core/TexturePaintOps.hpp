#pragma once
// TexturePaintOps.hpp
// Malen auf einem Textur-Layer-Gewicht. Kern-Invariante: die Gewichte aller Layer an einer
// Zelle summieren sich immer zu ~1.0 ("Splat-Map"-Prinzip) - Erhöhen des Ziel-Layers senkt die
// übrigen Layer proportional zu ihrem aktuellen Anteil ab (und umgekehrt beim Absenken).

#include "mapeditor/core/TextureLayerStack.hpp"

#include <cstdint>
#include <vector>

namespace theseed::mapeditor::core {

enum class PaintMode { Increase, Decrease };

struct TexturePaintSettings {
    float radius = 200.0f;
    float strength = 0.5f;
};

struct TexturePaintPatch {
    struct Entry {
        std::uint32_t x;
        std::uint32_t z;
        std::vector<float> oldWeights;
    };
    std::vector<Entry> entries;
};

TexturePaintPatch PaintLayerWeight(
    TextureLayerStack& stack,
    std::size_t targetLayer,
    PaintMode mode,
    const TexturePaintSettings& settings,
    float worldX, float worldZ,
    float blockWidth, float blockHeight);

void RevertTexturePatch(TextureLayerStack& stack, const TexturePaintPatch& patch);

class TexturePaintUndoStack {
public:
    void Push(TexturePaintPatch patch);
    bool Undo(TextureLayerStack& stack);
    bool Redo(TextureLayerStack& stack);
    void Clear();

    [[nodiscard]] bool CanUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !redo_.empty(); }

private:
    std::vector<TexturePaintPatch> undo_;
    std::vector<TexturePaintPatch> redo_;
};

} // namespace theseed::mapeditor::core
