#pragma once
// Renderer.hpp
// Baut aus der aktuellen Heightmap ein Dreiecksnetz und rendert es in einen Offscreen-
// Framebuffer, der dann per ImGui::Image() im 3D-Vorschau-Panel angezeigt wird.
// Bewusst getrennt vom 2D-Editier-Panel (siehe main.cpp): Bearbeitung erfolgt auf dem
// Graustufenbild, dieser Renderer ist reine Visualisierung.
//
// Terrain-Texturierung: bis zu kMaxTextureLayers echte Diffuse-Texturen (DDS, siehe
// mapeditor/core/DdsImage.hpp) + deren Blend-Gewichte werden geladen und im Fragment-Shader
// gemischt. Ohne geladene Texturen (z.B. direkt nach "Neu") fällt der Shader auf den
// Höhen-Farbverlauf zurück.

#include "Camera.hpp"
#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/TextureLayerStack.hpp"

#include <cstdint>
#include <filesystem>

namespace theseed::mapeditor::app {

class HeightmapRenderer {
public:
    HeightmapRenderer() = default;
    ~HeightmapRenderer();

    HeightmapRenderer(const HeightmapRenderer&) = delete;
    HeightmapRenderer& operator=(const HeightmapRenderer&) = delete;

    // Muss nach dem OpenGL-Kontext-Setup einmalig aufgerufen werden.
    void Init();
    void Shutdown();

    // Baut VBO/EBO aus dem aktuellen Zustand der Heightmap neu auf (bei jeder Änderung
    // durch einen Pinselstrich aufrufen - für die Zielgröße von 257x257 unproblematisch
    // performant genug für interaktives Arbeiten; bei deutlich größeren Karten später
    // ggf. auf partielle Updates umstellen).
    void RebuildMesh(const core::Heightmap& heightmap);

    // Lädt bis zu kMaxTextureLayers Diffuse-DDS + Blend-Gewichte. mapDir wird zur Pfadauflösung
    // der Diffuse-Texturen gebraucht (dieselbe Mehrfach-Wurzel-Logik wie beim Blend-BMP-Import).
    // Layer, deren DDS nicht geladen werden kann, bekommen eine graue Platzhalter-Textur (kein
    // Abbruch). Sollte einmalig beim Öffnen einer Karte aufgerufen werden (nicht pro Frame).
    void LoadTerrainTextures(const core::TextureLayerStack& stack, const std::filesystem::path& mapDir);

    // Lädt NUR die Blend-Gewichte neu hoch (z.B. nach einem Textur-Pinselstrich) - deutlich
    // billiger als LoadTerrainTextures, da keine DDS-Dateien neu gelesen werden.
    void UpdateBlendTextures(const core::TextureLayerStack& stack);

    void ClearTerrainTextures();

    static constexpr int kMaxTextureLayers = 8;

    // Rendert die Szene in einen Offscreen-Framebuffer fester Größe und liefert die
    // resultierende Farbtextur (GL-Textur-ID) zur Anzeige via ImGui::Image() zurück.
    [[nodiscard]] std::uint32_t RenderToTexture(const OrbitCamera& camera, int width, int height, bool wireframe);

    // Aufgeteilte Variante von RenderToTexture, damit weitere Renderer (z.B. Objekt-Marker)
    // in denselben Framebuffer-Pass zeichnen können, bevor er abgeschlossen wird.
    void BeginScene(const OrbitCamera& camera, int width, int height, bool wireframe);
    [[nodiscard]] std::uint32_t EndScene();

    // Orthographische Draufsicht (direkt von oben, deckt die komplette Kartenfläche ab) mit
    // demselben Multi-Layer-Textur-Shader wie die 3D-Ansicht - liefert ein Bild, das wie die
    // "echte" Kartentextur aussieht, für den 2D-Editor (statt reiner Graustufen-Vorschau).
    // EIGENER Framebuffer (nicht derselbe wie RenderToTexture/BeginScene) - beide Ansichten
    // können sonst nicht im selben Frame unabhängig dargestellt werden.
    void BeginTopDownScene(int width, int height);
    // Zeichnet eine zusätzliche Textur halbtransparent über das zuletzt in BeginTopDownScene
    // gezeichnete Bild (z.B. die Block&Walk-Heatmap) - muss VOR EndTopDownScene aufgerufen
    // werden, im selben Frame.
    void DrawTopDownOverlay(std::uint32_t overlayTexture, float alpha);
    [[nodiscard]] std::uint32_t EndTopDownScene();

private:
    void EnsureFramebuffer(int width, int height);
    // Gemeinsame Zeichenlogik für 3D-Perspektiv- und 2D-Draufsicht (Shader-Uniforms setzen,
    // Texturen binden, Mesh zeichnen) - nur die View-Projektions-Matrix und der aufrufende
    // Framebuffer unterscheiden sich.
    void DrawTerrainMesh(const Mat4& viewProj, bool wireframe);

    std::uint32_t vao_ = 0;
    std::uint32_t vbo_ = 0;
    std::uint32_t ebo_ = 0;
    std::uint32_t indexCount_ = 0;
    std::uint32_t shaderProgram_ = 0;
    float minHeight_ = 0.0f;
    float maxHeight_ = 1.0f;
    float mapSpanX_ = 1.0f;
    float mapSpanZ_ = 1.0f;

    std::uint32_t diffuseTex_[kMaxTextureLayers] = {};
    std::uint32_t blendTex_[kMaxTextureLayers] = {};
    float layerUvScale_[kMaxTextureLayers] = {};
    int textureLayerCount_ = 0;

    std::uint32_t fbo_ = 0;
    std::uint32_t fboColorTex_ = 0;
    std::uint32_t fboDepthRbo_ = 0;
    int fboWidth_ = 0;
    int fboHeight_ = 0;

    // Zweiter, unabhängiger Framebuffer für die orthographische Draufsicht (2D-Editor-
    // Hintergrund) - siehe BeginTopDownScene.
    std::uint32_t fbo2d_ = 0;
    std::uint32_t fbo2dColorTex_ = 0;
    std::uint32_t fbo2dDepthRbo_ = 0;
    int fbo2dWidth_ = 0;
    int fbo2dHeight_ = 0;
    void EnsureFramebuffer2d(int width, int height);

    // Einfaches Vollbild-Quad (2 Dreiecke) für DrawTopDownOverlay.
    std::uint32_t overlayVao_ = 0;
    std::uint32_t overlayVbo_ = 0;
    std::uint32_t overlayShaderProgram_ = 0;
};

} // namespace theseed::mapeditor::app
