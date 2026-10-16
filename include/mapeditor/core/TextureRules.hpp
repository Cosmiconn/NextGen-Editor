#pragma once
// TextureRules.hpp
// Automatisches Texturieren nach Regeln (wie Unreals Auto-Material/Landscape-Layer-Regeln): ein Layer
// bekommt Gewicht, wo Neigung und/oder Höhe in einem Bereich liegen, mit weichem Übergang. Fiesta-
// Layer sind unabhängige Masken (siehe TexturePaintOps), daher ändert eine Regel nur ihren Layer.

#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/TexturePaintOps.hpp"

#include <cstddef>

namespace theseed::mapeditor::core {

enum class TextureRuleMode : std::uint8_t {
    Raise,   // Gewicht = max(alt, Regel) - malt nur hinzu
    Replace, // Gewicht = Regel - überschreibt den Layer im Bereich
};

struct TextureRule {
    bool useSlope = true;
    float minSlope = 35.0f;    // Grad
    float maxSlope = 90.0f;
    float slopeFeather = 5.0f; // weicher Übergang in Grad
    bool useHeight = false;
    float minHeight = 0.0f;
    float maxHeight = 1000.0f;
    float heightFeather = 20.0f;
    float strength = 1.0f;     // Gewicht, wo die Regel voll greift (0..1)
    TextureRuleMode mode = TextureRuleMode::Raise;
};

// Regelgewicht 0..1 an einer Weltposition.
[[nodiscard]] float TextureRuleWeight(const Heightmap& heightmap, const TextureRule& rule, float worldX, float worldZ);

// Wo die Regel angewendet wird: ganze Karte oder ein Kreis (Regel als Pinsel).
struct TextureRuleArea {
    bool wholeMap = true;
    float centerX = 0.0f, centerZ = 0.0f, radius = 0.0f;
};

// Wendet die Regel auf den Layer an (beachtet dessen Region, z.B. Adl-Hälften). Undo-fähig.
TexturePaintPatch ApplyTextureRule(TextureLayerStack& stack, std::size_t layer, const Heightmap& heightmap,
                                   const TextureRule& rule, const TextureRuleArea& area);

} // namespace theseed::mapeditor::core
