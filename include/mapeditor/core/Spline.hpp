#pragma once
// Spline.hpp
// Weg-Werkzeug: Kontrollpunkte -> Catmull-Rom-Kurve. Entlang der Kurve lässt sich das Terrain zu
// einem Weg ebnen (Höhe folgt geglättet dem Gelände), ein Textur-Layer malen und ein Modell in festen
// Abständen setzen (Zäune, Laternen; mittig, links, rechts oder beidseitig, entlang der Richtung).

#include "mapeditor/core/EditOps.hpp"
#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"
#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/TexturePaintOps.hpp"

#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct SplinePoint {
    float x = 0.0f, z = 0.0f;
};

struct SplineSample {
    float x = 0.0f, z = 0.0f;
    float dirX = 1.0f, dirZ = 0.0f; // normierte Richtung
    float distance = 0.0f;          // Bogenlänge ab dem Start
};

// Gleichmäßig (Abstand `spacing`) abgetastete Catmull-Rom-Kurve durch alle Punkte.
[[nodiscard]] std::vector<SplineSample> SampleSpline(std::span<const SplinePoint> points, float spacing);

struct SplineWidth {
    float width = 200.0f;   // volle Wirkung
    float falloff = 100.0f; // weicher Rand je Seite
};

// Ebnet das Terrain entlang des Wegs. Die Weghöhe ist die über `smoothDistance` geglättete
// Geländehöhe entlang der Kurve.
UndoPatch FlattenAlongSpline(Heightmap& heightmap, std::span<const SplineSample> samples, const SplineWidth& width,
                             float smoothDistance = 300.0f);

// Malt den Layer entlang des Wegs (Gewicht = max(alt, Wegprofil)); beachtet die Layer-Region.
TexturePaintPatch PaintAlongSpline(TextureLayerStack& stack, std::size_t layer, const Heightmap& heightmap,
                                   std::span<const SplineSample> samples, const SplineWidth& width);

enum class SplineSide : std::uint8_t { Center, Left, Right, Both };

struct SplineObjectSettings {
    std::string model;
    float spacing = 300.0f;
    float sideOffset = 150.0f;
    SplineSide side = SplineSide::Both;
    float yawOffsetDegrees = 0.0f; // zusätzlich zur Wegrichtung
    float scale = 1.0f;
};

[[nodiscard]] std::vector<PlacedObject> ObjectsAlongSpline(std::span<const SplinePoint> points, const Heightmap& heightmap,
                                                           const SplineObjectSettings& settings);

} // namespace theseed::mapeditor::core
