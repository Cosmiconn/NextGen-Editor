#pragma once
// ObjectScatter.hpp
// Objekte malen wie Unreals Foliage-Werkzeug: Bäume, Gras oder Steine mit einem Pinsel auf dem
// Terrain verteilen. Jede Anwendung versucht `density` Objekte je 100x100 Einheiten im Pinselkreis
// und verwirft Punkte, die zu nah an einem vorhandenen Objekt der gewählten Modelle liegen oder zu
// steil sind. Wiederholtes Malen füllt die Fläche deshalb bis zum Mindestabstand und nicht darüber.

#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct ScatterSettings {
    std::vector<std::string> models; // zufällig gewählt
    float radius = 300.0f;
    float density = 2.0f;            // Versuche je 100x100 Einheiten und Anwendung
    float minDistance = 80.0f;       // zu Objekten derselben Modellliste
    float minScale = 0.8f, maxScale = 1.25f;
    bool randomYaw = true;
    bool alignToSlope = false;       // Hochachse zur Terrain-Normalen neigen
    float maxSlope = 30.0f;          // Grad; steiler wird nicht bepflanzt
    float heightOffset = 0.0f;       // z.B. leicht einsinken lassen (negativ)
};

// Neue Objekte für eine Anwendung an (centerX, centerZ). Die Objekte sind noch nicht eingefügt.
[[nodiscard]] std::vector<PlacedObject> ScatterObjects(const Heightmap& heightmap, const ObjectPlacementSet& existing,
                                                       const ScatterSettings& settings, float centerX, float centerZ,
                                                       std::mt19937& rng);

// Indizes (absteigend, zum Löschen geeignet) der Objekte der Modellliste im Kreis.
[[nodiscard]] std::vector<std::size_t> ScatteredObjectsInRadius(const ObjectPlacementSet& set,
                                                                const std::vector<std::string>& models,
                                                                float centerX, float centerZ, float radius);

} // namespace theseed::mapeditor::core
