#pragma once
// NifMeshRenderer.hpp
// Rendert ECHTE .nif-Meshes (siehe mapeditor/core/NifModel.hpp) für platzierte Objekte, bei
// denen der Parser erfolgreich war (Massentest über alle 3436 echten Dateien: 579 ladbar,
// siehe docs/MAP_FORMAT.md). Für alle anderen Objekte zeigt weiterhin ObjectMarkerRenderer den
// Platzhalter - beide Renderer arbeiten zusammen im selben 3D-Vorschau-Pass.
//
// Texturierung: externe NiSourceTexture-Dateien werden über LoadDdsImage geladen; bei
// Use External=0 wird die in derselben NIF gespeicherte NiPixelData-Textur direkt dekodiert
// und hochgeladen. Der Dateiname aus NiSourceTexture ist auch im Embedded-Fall nur Metadaten. Ohne Textur oder UVs fällt der Shader auf die extrahierte Materialfarbe zurück
// (unverändertes Verhalten).
//
// Kein GPU-Instancing (anders als ObjectMarkerRenderer): jedes geladene Modell hat eigene,
// unterschiedliche Geometrie, daher ein Draw-Call pro Objekt-Instanz.

#include "Camera.hpp"
#include "mapeditor/core/NifModel.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace theseed::mapeditor::app {

class NifMeshRenderer {
public:
    ~NifMeshRenderer();

    void Init();
    void Shutdown();

    // Versucht, alle vom Set referenzierten Modelle zu laden (dedupliziert nach aufgelöstem
    // Pfad - viele Objekte teilen sich oft dasselbe Modell). mapDir wird zur Pfadauflösung
    // benötigt (dieselbe Mehrfach-Wurzel-Logik wie bei den Diffuse-Texturen). Bei jedem
    // Öffnen einer Karte / Ändern der Objektliste erneut aufrufen.
    void LoadModelsForSet(const core::ObjectPlacementSet& set, const std::filesystem::path& mapDir);

    // true, wenn für dieses Objekt ein echtes Mesh geladen werden konnte - der Aufrufer kann
    // damit entscheiden, ob ObjectMarkerRenderer für dieses Objekt den Platzhalter zeichnen soll
    // (aktuell zeichnet ObjectMarkerRenderer weiterhin alle Objekte - siehe main.cpp für die
    // Kombination beider Renderer).
    [[nodiscard]] bool HasRealMesh(std::size_t objectIndex) const;
    [[nodiscard]] std::size_t RealMeshCount() const;

    // Zeichnet alle Objekte mit erfolgreich geladenem Mesh in den aktuell gebundenen
    // Framebuffer (siehe HeightmapRenderer::BeginScene/EndScene).
    void Draw(const core::ObjectPlacementSet& set, const OrbitCamera& camera, int width, int height);

private:
    struct SubMesh {
        std::uint32_t vao = 0;
        std::uint32_t vbo = 0;
        std::uint32_t ebo = 0;
        std::uint32_t indexCount = 0;
        std::array<float, 3> diffuseColor{1.0f, 1.0f, 1.0f};
        std::uint32_t diffuseTex = 0; // 0 = keine Textur, Shader nutzt dann diffuseColor
    };
    struct LoadedModel {
        std::vector<SubMesh> subMeshes;
    };

    void ReleaseModel(LoadedModel& model);
    // Lädt (oder liefert aus dem Cache) die GL-Textur für einen aufgelösten Dateipfad - mehrere
    // Mesh-Teile/Modelle teilen sich häufig dieselbe Textur (z.B. "grass.dds").
    std::uint32_t GetOrLoadTexture(const std::filesystem::path& resolvedPath);
    std::uint32_t GetOrLoadEmbeddedTexture(const core::NifEmbeddedTexture& image, const std::string& cacheKey);

    std::unordered_map<std::string, LoadedModel> modelCache_;   // Schlüssel: aufgelöster Pfad
    std::unordered_map<std::string, std::uint32_t> textureCache_; // Schlüssel: aufgelöster Textur-Pfad
    std::vector<const LoadedModel*> perObjectModel_;             // parallel zu set, nullptr = kein Mesh
    std::uint32_t shaderProgram_ = 0;
};

} // namespace theseed::mapeditor::app
