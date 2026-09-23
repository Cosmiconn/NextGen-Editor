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
    float radius = 200.0f;   // Weltraum-Radius
    float strength = 0.5f;   // Gewichtsänderung im Zentrum pro Anwendung (0..1)
    // Optional: nur Layer mit mask[i] != 0 nehmen an Normalisierung/Umverteilung teil (Karten mit
    // Layern verschiedener Regionen, z.B. Adl: linke/rechte Haelfte). nullptr = alle Layer.
    const std::vector<char>* participating = nullptr;
};

// Ein rückgängig machbarer Malschritt: pro betroffener Zelle die Gewichte ALLER Layer vor der
// Anwendung (nicht nur des Ziel-Layers, da Normalisierung auch andere Layer verändert).
struct TexturePaintPatch {
    struct Entry {
        std::uint32_t x;
        std::uint32_t z;
        std::vector<float> oldWeights; // Größe = LayerCount() zum Zeitpunkt der Anwendung
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
