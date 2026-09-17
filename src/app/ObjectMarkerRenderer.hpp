#pragma once
// ObjectMarkerRenderer.hpp
// Rendert einen Platzhalter (kleine Pfeil-/Pyramidenform, zeigt Position UND Blickrichtung) pro
// platziertem Objekt in den 3D-Vorschau-Pass. KEINE echten .nif-Meshes (siehe
// docs/MAP_FORMAT.md - Gamebryo-Format-Import ist eigenes, größeres Folgeprojekt) - Zweck ist,
// Lage/Dichte/Ausrichtung der Objekt-Placements im Kontext des Terrains einschätzen zu können.
//
// GPU-Instancing, da reale Karten mehrere Tausend Objekt-Instanzen enthalten können (RouVal01:
// 3902) - ein Draw-Call pro Objekt wäre bei dieser Größenordnung ein Performance-Problem.

#include "Camera.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"

#include <cstdint>
#include <functional>

namespace theseed::mapeditor::app {

class ObjectMarkerRenderer {
public:
    ObjectMarkerRenderer() = default;
    ~ObjectMarkerRenderer();

    ObjectMarkerRenderer(const ObjectMarkerRenderer&) = delete;
    ObjectMarkerRenderer& operator=(const ObjectMarkerRenderer&) = delete;

    void Init();
    void Shutdown();

    // Baut die Instanzdaten (Modellmatrix + Highlight-Flag pro Objekt) neu auf. Bei jeder
    // Änderung der Objektliste aufrufen (Hinzufügen/Löschen/Transform-Edit/Import).
    // skipIndex(i) == true überspringt ein Objekt (z.B. weil dafür bereits ein echtes Mesh via
    // NifMeshRenderer gezeichnet wird, siehe main.cpp) - vermeidet doppelte Darstellung.
    void RebuildInstances(const core::ObjectPlacementSet& set, int selectedIndex,
                          const std::function<bool(std::size_t)>& skipIndex = nullptr);

    // Zeichnet alle Instanzen in den AKTUELL GEBUNDENEN Framebuffer (siehe
    // HeightmapRenderer::BeginScene/EndScene - beide Renderer teilen sich denselben Pass, damit
    // Terrain und Objekt-Marker gemeinsam sichtbar sind und sich gegenseitig per Depth-Test
    // korrekt verdecken).
    void Draw(const OrbitCamera& camera, int width, int height);

private:
    std::uint32_t vao_ = 0;
    std::uint32_t vbo_ = 0;      // Unit-Marker-Mesh: Position + Normale
    std::uint32_t ebo_ = 0;
    std::uint32_t indexCount_ = 0;

    std::uint32_t instanceVbo_ = 0; // pro Instanz: mat4 Modellmatrix (4x vec4) + float highlight
    std::uint32_t instanceCount_ = 0;

    std::uint32_t shaderProgram_ = 0;
};

} // namespace theseed::mapeditor::app
