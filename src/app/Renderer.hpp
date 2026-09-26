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

    // Bis zu 24 Layer (Cypian hat 12, Adl 10): das Terrain wird in Durchgaengen zu je 8 Layern
    // gezeichnet (Textur-Einheiten-Limit des Shaders) und additiv zusammengefuehrt.
    static constexpr int kMaxTextureLayers = 24;
    static constexpr int kLayersPerPass = 8;

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
    // Sichtfenster der Draufsicht (2D-Zoom): Mittelpunkt und halbe Ausdehnung in WELTeinheiten.
    // Ohne Aufruf (oder nach ResetTopDownWindow) wird die ganze Karte gezeigt.
    void SetTopDownWindow(float centerX, float centerZ, float halfW, float halfH) { tdCenterX_ = centerX; tdCenterZ_ = centerZ; tdHalfW_ = halfW; tdHalfH_ = halfH; }
    void ResetTopDownWindow() { tdHalfW_ = 0.0f; tdHalfH_ = 0.0f; }
    // Sichtbarkeit (Map-Editor "Sichtbarkeit"): einzelne Terrain-Layer und das ganze Terrain
    // ein-/ausblenden. Wirkt auf 3D UND 2D (gleicher Shader).
    void SetLayerVisible(int index, bool visible) { if (index >= 0 && index < kMaxTextureLayers) layerVisible_[index] = visible ? 1.0f : 0.0f; }
    void SetTerrainVisible(bool visible) { terrainVisible_ = visible; }
    // Zeichnet eine zusätzliche Textur halbtransparent über das zuletzt in BeginTopDownScene
    // gezeichnete Bild (z.B. die Block&Walk-Heatmap) - muss VOR EndTopDownScene aufgerufen
    // werden, im selben Frame.
    // uvScaleX/Y: welcher Teil der Overlay-Textur die Karte abdeckt (Block&Walk-Gitter ist quadratisch,
    // bei nicht-quadratischen Karten deckt die Karte nur einen Teil ab).
    void DrawTopDownOverlay(std::uint32_t overlayTexture, float alpha, float uvScaleX = 1.0f, float uvScaleY = 1.0f,
                            float uvOffsetX = 0.0f, float uvOffsetY = 0.0f);
    [[nodiscard]] std::uint32_t EndTopDownScene();

    // Unabhängige, nicht-interaktive Ganzkarten-Draufsicht für das Minimap-Dock. Verwendet
    // bewusst einen dritten FBO, damit 2D-Editor-Zoom und Minimap im selben ImGui-Frame nicht
    // dieselbe GL-Textur überschreiben. Diese API exportiert KEINE Fiesta-Datei.
    [[nodiscard]] std::uint32_t RenderTopDownOverview(int width, int height);

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
    float layerRegionCells_[kMaxTextureLayers][4] = {}; // startX, startY, width, height (Vertex-Einheiten; width 0 = ganze Karte)
    float blockW_ = 50.0f;
    float blockH_ = 50.0f;
    float layerVisible_[kMaxTextureLayers] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                              1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    bool terrainVisible_ = true;
    float tdCenterX_ = 0.0f, tdCenterZ_ = 0.0f, tdHalfW_ = 0.0f, tdHalfH_ = 0.0f; // Draufsicht-Fenster (halfW <= 0: ganze Karte)
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

    // Dritter FBO nur für die Minimap-/Overview-Vorschau. Die Trennung ist notwendig, weil
    // ImGui Textur-IDs erst am Frame-Ende zeichnet und eine später neu gerenderte 2D-Textur
    // sonst rückwirkend auch die bereits eingereihte Minimap-Anzeige verändern würde.
    std::uint32_t fboOverview_ = 0;
    std::uint32_t fboOverviewColorTex_ = 0;
    std::uint32_t fboOverviewDepthRbo_ = 0;
    int fboOverviewWidth_ = 0;
    int fboOverviewHeight_ = 0;
    void EnsureFramebufferOverview(int width, int height);

    // Einfaches Vollbild-Quad (2 Dreiecke) für DrawTopDownOverlay.
    std::uint32_t overlayVao_ = 0;
    std::uint32_t overlayVbo_ = 0;
    std::uint32_t overlayShaderProgram_ = 0;
};

} // namespace theseed::mapeditor::app
