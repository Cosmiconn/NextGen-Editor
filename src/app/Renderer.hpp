#pragma once
// Renderer.hpp
// Baut aus der aktuellen Heightmap ein Dreiecksnetz und rendert es in einen Offscreen-Framebuffer.
// Terrain-Texturierung: bis zu kMaxTextureLayers echte Diffuse-Texturen + Blend-Gewichte.

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

    void Init();
    void Shutdown();
    void RebuildMesh(const core::Heightmap& heightmap);
    void LoadTerrainTextures(const core::TextureLayerStack& stack, const std::filesystem::path& mapDir);
    void UpdateBlendTextures(const core::TextureLayerStack& stack);
    void ClearTerrainTextures();

    static constexpr int kMaxTextureLayers = 8;

    [[nodiscard]] std::uint32_t RenderToTexture(const OrbitCamera& camera, int width, int height, bool wireframe);
    void BeginScene(const OrbitCamera& camera, int width, int height, bool wireframe);
    [[nodiscard]] std::uint32_t EndScene();

    void BeginTopDownScene(int width, int height);
    void DrawTopDownOverlay(std::uint32_t overlayTexture, float alpha);
    [[nodiscard]] std::uint32_t EndTopDownScene();

private:
    void EnsureFramebuffer(int width, int height);
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

    std::uint32_t fbo2d_ = 0;
    std::uint32_t fbo2dColorTex_ = 0;
    std::uint32_t fbo2dDepthRbo_ = 0;
    int fbo2dWidth_ = 0;
    int fbo2dHeight_ = 0;
    void EnsureFramebuffer2d(int width, int height);

    std::uint32_t overlayVao_ = 0;
    std::uint32_t overlayVbo_ = 0;
    std::uint32_t overlayShaderProgram_ = 0;
};

} // namespace theseed::mapeditor::app
