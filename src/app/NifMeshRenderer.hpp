#pragma once
// NifMeshRenderer.hpp
// Rendert ECHTE .nif-Meshes (siehe mapeditor/core/NifModel.hpp) für platzierte Objekte, bei
// denen der Parser erfolgreich war (Massentest über alle 3436 echten Dateien: 579 ladbar,
// siehe docs/MAP_FORMAT.md). Für alle anderen Objekte zeigt weiterhin ObjectMarkerRenderer den
// Platzhalter - beide Renderer arbeiten zusammen im selben 3D-Vorschau-Pass.
//
// Texturierung: klassische NiTexturingProperty-Slots Base/Dark/Detail/Gloss/Glow/Bump/Decal
// werden gleichzeitig ausgewertet, inklusive eigener UV-Sets und Texture-Transforms. DDS/TGA
// werden intern dekodiert; unter Windows ergaenzt WIC JPG/PNG/BMP (relevant z.B. fuer echte
// Fiesta-Bumpmaps). Use External=0 verwendet weiterhin eingebettete NiPixelData.
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
    // custom: optional - Modelle, die NICHT aus einer Datei kommen (z.B. zusammengesetzte Spieler-Avatare):
    // Objekte, deren modelPath ein Schluessel dieser Tabelle ist, verwenden das NifModel daraus.
    void LoadModelsForSet(const core::ObjectPlacementSet& set, const std::filesystem::path& mapDir,
                          const std::unordered_map<std::string, core::NifModel>* custom = nullptr);

    // true, wenn für dieses Objekt ein echtes Mesh geladen werden konnte - der Aufrufer kann
    // damit entscheiden, ob ObjectMarkerRenderer für dieses Objekt den Platzhalter zeichnen soll
    // (aktuell zeichnet ObjectMarkerRenderer weiterhin alle Objekte - siehe main.cpp für die
    // Kombination beider Renderer).
    [[nodiscard]] bool HasRealMesh(std::size_t objectIndex) const;
    [[nodiscard]] std::size_t RealMeshCount() const;

    // Zeichnet alle Objekte mit erfolgreich geladenem Mesh in den aktuell gebundenen
    // Framebuffer (siehe HeightmapRenderer::BeginScene/EndScene).
    // hidden: optional, je Objektindex != 0 -> Objekt wird nicht gezeichnet (Sichtbarkeit/Kategorien).
    void Draw(const core::ObjectPlacementSet& set, const OrbitCamera& camera, int width, int height,
              const std::vector<char>* hidden = nullptr);

private:
    struct TextureBinding {
        std::uint32_t texture = 0;
        std::uint32_t uvSet = 0;
        std::uint32_t clampMode = 3;
        std::uint32_t filterMode = 2;
        bool hasTransform = false;
        std::array<float, 2> translation{0.0f, 0.0f};
        std::array<float, 2> scale{1.0f, 1.0f};
        float rotation = 0.0f;
        std::array<float, 2> center{0.5f, 0.5f};
    };

    struct SubMesh {
        std::uint32_t vao = 0;
        std::uint32_t vbo = 0;
        std::uint32_t ebo = 0;
        std::uint32_t indexCount = 0;
        std::array<TextureBinding, 10> textures{};
        std::vector<core::NifTextureTransformAnimation> textureTransformAnimations;
        struct FlipAnimation {
            std::uint32_t slot = 0;
            core::NifFloatTrack track;
            std::vector<std::uint32_t> frameTextures;
        };
        std::vector<FlipAnimation> textureFlipAnimations;
        std::uint32_t textureApplyMode = 2;
        std::array<float, 3> ambientColor{1.0f, 1.0f, 1.0f};
        std::array<float, 3> diffuseColor{1.0f, 1.0f, 1.0f};
        std::array<float, 3> specularColor{1.0f, 1.0f, 1.0f};
        std::array<float, 3> emissiveColor{0.0f, 0.0f, 0.0f};
        float glossiness = 10.0f;
        bool specularEnabled = true;
        float bumpMapLumaScale = 1.0f;
        float bumpMapLumaOffset = 0.0f;
        std::array<float, 4> bumpMapMatrix{1.0f, 0.0f, 0.0f, 1.0f};
        std::array<float, 3> localCenter{0.0f, 0.0f, 0.0f}; // fuer transparente Back-to-front-Sortierung
        float materialAlpha = 1.0f;
        bool alphaBlend = false;
        bool alphaTest = false;
        float alphaCutoff = 0.0f;
        std::uint8_t alphaSrcBlend = 6;
        std::uint8_t alphaDstBlend = 7;
        std::uint8_t alphaTestFunc = 4;
        std::uint32_t faceDrawMode = 3;

        bool billboard = false;
        std::uint16_t billboardMode = 0;
        std::array<float, 3> billboardPivot{0.0f, 0.0f, 0.0f};
        std::array<float, 9> billboardInverseRotation{1.0f, 0.0f, 0.0f,
                                                     0.0f, 1.0f, 0.0f,
                                                     0.0f, 0.0f, 1.0f};
        bool lodControlled = false;
        float lodNear = 0.0f;
        float lodFar = 0.0f;
        std::array<float, 3> lodCenter{0.0f, 0.0f, 0.0f};
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
