#include "Renderer.hpp"

#include "mapeditor/core/DdsImage.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace theseed::mapeditor::app {

namespace {

// uUseTextures schaltet zwischen echtem Multi-Layer-Texturing (sofern geladen) und dem
// Höhen-Farbverlauf-Fallback um (z.B. direkt nach "Neu", bevor Texturen geladen wurden).
// Feste, einzeln benannte Sampler-Uniforms statt eines Sampler-Arrays: dynamische Indizierung
// von Sampler-Arrays ist im strikten GLSL-330-Core-Profil nicht garantiert (erst ab GLSL 400
// bzw. mit Erweiterung) - einzeln benannte Uniforms sind auf jeder GL-3.3-fähigen Hardware sicher.
const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uMvp;

out vec3 vNormal;
out vec3 vWorldPos;

void main() {
    vNormal = aNormal;
    vWorldPos = aPos;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kFragmentShaderSrc = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
out vec4 FragColor;

uniform vec3 uLightDir;
uniform float uMinHeight;
uniform float uMaxHeight;

uniform bool uUseTextures;
uniform int uLayerCount;
uniform float uUvScale[8];
uniform float uLayerVisible[8]; // 0 = Layer ausgeblendet (Map-Editor "Sichtbarkeit")
uniform vec4 uLayerRegion[8]; // Welt-X/Z-Start, Welt-Breite/Tiefe der Region, die die Blend-Map des Layers abdeckt
uniform vec2 uMapSpan;
uniform vec2 uBlockSize;

uniform sampler2D uDiffuse0;
uniform sampler2D uBlend0;
uniform sampler2D uDiffuse1;
uniform sampler2D uBlend1;
uniform sampler2D uDiffuse2;
uniform sampler2D uBlend2;
uniform sampler2D uDiffuse3;
uniform sampler2D uBlend3;
uniform sampler2D uDiffuse4;
uniform sampler2D uBlend4;
uniform sampler2D uDiffuse5;
uniform sampler2D uBlend5;
uniform sampler2D uDiffuse6;
uniform sampler2D uBlend6;
uniform sampler2D uDiffuse7;
uniform sampler2D uBlend7;

vec3 SampleLayer(sampler2D diffuseTex, sampler2D blendTex, float uvScale, vec4 region) {
    // Region des Layers (ini #StartPos/#Width/#Height): bei grossen Karten wie Adl deckt jeder Layer
    // nur einen Teil der Karte ab - Blend UND Diffuse-Kachelung beziehen sich auf diese Region
    // (vorher wurde jeder Layer ueber die ganze Karte gestreckt). Ausserhalb der Region Gewicht 0.
    vec2 mapUv = (vWorldPos.xz - region.xy) / region.zw;
    if (mapUv.x < 0.0 || mapUv.y < 0.0 || mapUv.x > 1.0 || mapUv.y > 1.0) return vec3(0.0);
    float weight = texture(blendTex, mapUv).r;
    // Diffuse detail density is a WORLD-space property, not a map-size property.
    // The previous mapUv*uvScale interpretation made one ground tile span thousands of
    // world units on large maps. Legacy maps use 50-unit terrain blocks by default; the
    // earlier renderer's 500-unit reference therefore corresponds to ten terrain blocks.
    // Keeping that reference relative to the actual block size preserves scale on maps
    // with non-default block dimensions while still honoring UVScaleDiffuse.
    vec2 localWorld = vWorldPos.xz - region.xy;
    vec2 referenceTile = max(uBlockSize * 10.0, vec2(1.0));
    vec2 diffuseUv = (localWorld / referenceTile) * max(uvScale, 0.0001);
    vec3 diffuseColor = texture(diffuseTex, diffuseUv).rgb;
    return diffuseColor * weight;
}

void main() {
    vec3 n = normalize(vNormal);
    float diff = max(dot(n, normalize(-uLightDir)), 0.0);
    vec3 baseColor;

    if (uUseTextures && uLayerCount > 0) {
        // Blend weights remain normalized to each layer region. Diffuse textures are
        // intentionally sampled in world scale inside SampleLayer(), so visual texel size does
        // not grow with the overall map dimensions.
        vec3 color = vec3(0.0);
        if (uLayerCount > 0) color += SampleLayer(uDiffuse0, uBlend0, uUvScale[0], uLayerRegion[0]) * uLayerVisible[0];
        if (uLayerCount > 1) color += SampleLayer(uDiffuse1, uBlend1, uUvScale[1], uLayerRegion[1]) * uLayerVisible[1];
        if (uLayerCount > 2) color += SampleLayer(uDiffuse2, uBlend2, uUvScale[2], uLayerRegion[2]) * uLayerVisible[2];
        if (uLayerCount > 3) color += SampleLayer(uDiffuse3, uBlend3, uUvScale[3], uLayerRegion[3]) * uLayerVisible[3];
        if (uLayerCount > 4) color += SampleLayer(uDiffuse4, uBlend4, uUvScale[4], uLayerRegion[4]) * uLayerVisible[4];
        if (uLayerCount > 5) color += SampleLayer(uDiffuse5, uBlend5, uUvScale[5], uLayerRegion[5]) * uLayerVisible[5];
        if (uLayerCount > 6) color += SampleLayer(uDiffuse6, uBlend6, uUvScale[6], uLayerRegion[6]) * uLayerVisible[6];
        if (uLayerCount > 7) color += SampleLayer(uDiffuse7, uBlend7, uUvScale[7], uLayerRegion[7]) * uLayerVisible[7];
        baseColor = color;
    } else {
        float t = clamp((vWorldPos.y - uMinHeight) / max(uMaxHeight - uMinHeight, 0.0001), 0.0, 1.0);
        vec3 low = vec3(0.25, 0.45, 0.20);   // Tiefland: grün
        vec3 high = vec3(0.85, 0.85, 0.82);  // Bergspitzen: hellgrau
        baseColor = mix(low, high, t);
    }

    vec3 color = baseColor * (0.35 + 0.65 * diff);
    FragColor = vec4(color, 1.0);
}
)";

std::uint32_t CompileShader(std::uint32_t type, const char* src) {
    const std::uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[Renderer] Shader-Kompilierfehler: %s\n", log);
    }
    return shader;
}

std::uint32_t LinkProgram(std::uint32_t vs, std::uint32_t fs) {
    const std::uint32_t program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[Renderer] Programm-Linkfehler: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Lädt Blend-Gewichte (0..1 float) als GL_R8-Graustufentextur - mit Swizzle, damit eine direkte
// Anzeige (nicht nur .r-Sampling im Shader) korrekt grau statt rot erscheint (derselbe Bug wie
// bei den 2D-Vorschauen in main.cpp, hier vorsorglich mitkorrigiert).
void UploadBlendTexture(std::uint32_t tex, const core::BlendMap& blend) {
    glBindTexture(GL_TEXTURE_2D, tex);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(blend.Width()) * blend.Height());
    const auto data = blend.Data();
    for (std::size_t i = 0; i < data.size(); ++i) {
        pixels[i] = static_cast<std::uint8_t>(std::clamp(data[i], 0.0f, 1.0f) * 255.0f);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(blend.Width()), static_cast<int>(blend.Height()),
                 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
}

void UploadGrayPlaceholder(std::uint32_t tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
    const std::uint8_t gray[4] = {160, 160, 160, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// Simpler Vollbild-Quad-Shader für DrawTopDownOverlay (z.B. Block&Walk-Heatmap halbtransparent
// über die texturierte Draufsicht legen). NDC-Koordinaten direkt im Vertex-Buffer, keine
// Transformation nötig - deckt immer den kompletten aktuellen Viewport ab.
const char* kOverlayVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* kOverlayFragmentShaderSrc = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTex;
uniform float uAlpha;
uniform vec2 uUvScale;
uniform vec2 uUvOffset;
void main() {
    // Rot-Einfärbung nach Helligkeit des Overlays (z.B. Block&Walk-Heatmap): höhere Werte
    // erscheinen deckender rot, niedrige nahezu transparent - macht das Muster auf der echten
    // Kartentextur gut erkennbar, ohne sie komplett zu verdecken.
    float v = texture(uTex, uUvOffset + vUv * uUvScale).r;
    FragColor = vec4(1.0, 0.25, 0.15, v * uAlpha);
}
)";

} // namespace

HeightmapRenderer::~HeightmapRenderer() { Shutdown(); }

void HeightmapRenderer::Init() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    const std::uint32_t vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    const std::uint32_t fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    shaderProgram_ = LinkProgram(vs, fs);

    // Vollbild-Quad (2 Dreiecke, NDC-Koordinaten + UV) für DrawTopDownOverlay.
    // WICHTIG: uv.y ist bewusst INVERS zu NDC.y (nicht gleichläufig wie man naiv erwarten
    // würde). Grund: walkPreviewTex wird direkt per glTexImage2D hochgeladen (Zeile 0 des
    // Puffers = WalkGrid-Zeile 0 = Textur-v=0, Standard-Sampling-Konvention, KEIN Rendering-
    // Flip). Dieses Quad wird aber selbst per NDC-Rasterisierung in den Top-Down-Framebuffer
    // gezeichnet, der anschließend komplett geflippt an ImGui übergeben wird (siehe
    // DrawEditor2D/main.cpp) - die Umkehrung hier gleicht genau diesen äußeren Flip aus, damit
    // WalkGrid-Zeile 0 auf derselben Bildseite landet wie Welt-Z=0 im Terrain darunter.
    const float quadVerts[] = {
        // x,    y,     u,    v
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 0.0f,
    };
    glGenVertexArrays(1, &overlayVao_);
    glBindVertexArray(overlayVao_);
    glGenBuffers(1, &overlayVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    const std::uint32_t ovs = CompileShader(GL_VERTEX_SHADER, kOverlayVertexShaderSrc);
    const std::uint32_t ofs = CompileShader(GL_FRAGMENT_SHADER, kOverlayFragmentShaderSrc);
    overlayShaderProgram_ = LinkProgram(ovs, ofs);
}

void HeightmapRenderer::Shutdown() {
    ClearTerrainTextures();
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (shaderProgram_) glDeleteProgram(shaderProgram_);
    if (fboColorTex_) glDeleteTextures(1, &fboColorTex_);
    if (fboDepthRbo_) glDeleteRenderbuffers(1, &fboDepthRbo_);
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    vbo_ = ebo_ = vao_ = shaderProgram_ = fboColorTex_ = fboDepthRbo_ = fbo_ = 0;

    if (fbo2dColorTex_) glDeleteTextures(1, &fbo2dColorTex_);
    if (fbo2dDepthRbo_) glDeleteRenderbuffers(1, &fbo2dDepthRbo_);
    if (fbo2d_) glDeleteFramebuffers(1, &fbo2d_);
    fbo2dColorTex_ = fbo2dDepthRbo_ = fbo2d_ = 0;
    fbo2dWidth_ = fbo2dHeight_ = 0;

    if (overlayVbo_) glDeleteBuffers(1, &overlayVbo_);
    if (overlayVao_) glDeleteVertexArrays(1, &overlayVao_);
    if (overlayShaderProgram_) glDeleteProgram(overlayShaderProgram_);
    overlayVbo_ = overlayVao_ = overlayShaderProgram_ = 0;
}

void HeightmapRenderer::RebuildMesh(const core::Heightmap& heightmap) {
    const std::uint32_t w = heightmap.Width();
    const std::uint32_t h = heightmap.Height();
    if (w < 2 || h < 2) {
        indexCount_ = 0;
        return;
    }

    const auto [lo, hi] = heightmap.MinMax();
    minHeight_ = lo;
    maxHeight_ = hi;
    blockW_ = heightmap.BlockWidth();
    blockH_ = heightmap.BlockHeight();
    mapSpanX_ = static_cast<float>(w - 1) * heightmap.BlockWidth();
    mapSpanZ_ = static_cast<float>(h - 1) * heightmap.BlockHeight();

    std::vector<float> vertices;
    vertices.reserve(static_cast<std::size_t>(w) * h * 6);

    for (std::uint32_t z = 0; z < h; ++z) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const float worldX = static_cast<float>(x) * heightmap.BlockWidth();
            const float worldZ = static_cast<float>(z) * heightmap.BlockHeight();
            const float worldY = heightmap.At(x, z);

            // Normale via zentraler Differenz (an Rändern geklemmt statt umgebrochen).
            const std::uint32_t xL = x > 0 ? x - 1 : x;
            const std::uint32_t xR = x < w - 1 ? x + 1 : x;
            const std::uint32_t zD = z > 0 ? z - 1 : z;
            const std::uint32_t zU = z < h - 1 ? z + 1 : z;

            const float hL = heightmap.At(xL, z);
            const float hR = heightmap.At(xR, z);
            const float hD = heightmap.At(x, zD);
            const float hU = heightmap.At(x, zU);

            const float nx = (hL - hR) / (2.0f * heightmap.BlockWidth());
            const float nz = (hD - hU) / (2.0f * heightmap.BlockHeight());
            constexpr float ny = 1.0f;
            const float invLen = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);

            vertices.insert(vertices.end(), {
                worldX, worldY, worldZ,
                nx * invLen, ny * invLen, nz * invLen,
            });
        }
    }

    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(w - 1) * (h - 1) * 6);
    for (std::uint32_t z = 0; z < h - 1; ++z) {
        for (std::uint32_t x = 0; x < w - 1; ++x) {
            const std::uint32_t i0 = z * w + x;
            const std::uint32_t i1 = z * w + x + 1;
            const std::uint32_t i2 = (z + 1) * w + x;
            const std::uint32_t i3 = (z + 1) * w + x + 1;
            indices.insert(indices.end(), {i0, i2, i1, i1, i2, i3});
        }
    }
    indexCount_ = static_cast<std::uint32_t>(indices.size());

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<long>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<long>(indices.size() * sizeof(std::uint32_t)), indices.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    glBindVertexArray(0);
}

void HeightmapRenderer::LoadTerrainTextures(const core::TextureLayerStack& stack, const std::filesystem::path& mapDir) {
    ClearTerrainTextures();

    const int count = std::min(static_cast<int>(stack.LayerCount()), kMaxTextureLayers);
    for (int i = 0; i < count; ++i) {
        const auto& layer = stack.Layer(static_cast<std::size_t>(i));
        layerUvScale_[i] = layer.uvScaleDiffuse > 0.0f ? layer.uvScaleDiffuse : 1.0f;
        layerRegionCells_[i][0] = layer.regionStartX;
        layerRegionCells_[i][1] = layer.regionStartY;
        layerRegionCells_[i][2] = layer.regionWidth;
        layerRegionCells_[i][3] = layer.regionHeight;

        glGenTextures(1, &diffuseTex_[i]);
        glBindTexture(GL_TEXTURE_2D, diffuseTex_[i]);
        bool loaded = false;
        auto resolvedPath = core::legacy::ResolveLegacyAssetPath(mapDir, layer.diffuseFileName);
        if (resolvedPath) {
            auto ddsResult = core::LoadDdsImage(*resolvedPath);
            if (ddsResult) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<int>(ddsResult->width), static_cast<int>(ddsResult->height),
                             0, GL_RGBA, GL_UNSIGNED_BYTE, ddsResult->rgba.data());
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                loaded = true;
            } else {
                std::fprintf(stderr, "[Renderer] Diffuse-DDS nicht ladbar (%s): %s\n",
                              resolvedPath->string().c_str(), ddsResult.error().c_str());
            }
        } else {
            std::fprintf(stderr, "[Renderer] Diffuse-Textur nicht gefunden: %s\n", layer.diffuseFileName.c_str());
        }
        if (!loaded) {
            UploadGrayPlaceholder(diffuseTex_[i]);
        }

        glGenTextures(1, &blendTex_[i]);
        UploadBlendTexture(blendTex_[i], layer.blend);
    }

    textureLayerCount_ = count;
}

void HeightmapRenderer::UpdateBlendTextures(const core::TextureLayerStack& stack) {
    const int count = std::min(static_cast<int>(stack.LayerCount()), textureLayerCount_);
    for (int i = 0; i < count; ++i) {
        UploadBlendTexture(blendTex_[i], stack.Layer(static_cast<std::size_t>(i)).blend);
    }
}

void HeightmapRenderer::ClearTerrainTextures() {
    for (int i = 0; i < kMaxTextureLayers; ++i) {
        if (diffuseTex_[i]) { glDeleteTextures(1, &diffuseTex_[i]); diffuseTex_[i] = 0; }
        if (blendTex_[i]) { glDeleteTextures(1, &blendTex_[i]); blendTex_[i] = 0; }
    }
    textureLayerCount_ = 0;
}

void HeightmapRenderer::EnsureFramebuffer(int width, int height) {
    if (fbo_ != 0 && width == fboWidth_ && height == fboHeight_) {
        return;
    }

    if (fboColorTex_) glDeleteTextures(1, &fboColorTex_);
    if (fboDepthRbo_) glDeleteRenderbuffers(1, &fboDepthRbo_);
    if (fbo_) glDeleteFramebuffers(1, &fbo_);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &fboColorTex_);
    glBindTexture(GL_TEXTURE_2D, fboColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboColorTex_, 0);

    glGenRenderbuffers(1, &fboDepthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, fboDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fboDepthRbo_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[Renderer] Framebuffer unvollständig!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fboWidth_ = width;
    fboHeight_ = height;
}

std::uint32_t HeightmapRenderer::RenderToTexture(const OrbitCamera& camera, int width, int height, bool wireframe) {
    BeginScene(camera, width, height, wireframe);
    return EndScene();
}

void HeightmapRenderer::BeginScene(const OrbitCamera& camera, int width, int height, bool wireframe) {
    if (width <= 0 || height <= 0) {
        return;
    }
    EnsureFramebuffer(width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (indexCount_ > 0) {
        const Mat4 view = camera.ViewMatrix();
        const Mat4 proj = OrbitCamera::PerspectiveMatrix(
            0.9f, static_cast<float>(width) / static_cast<float>(height), camera.NearPlane(), camera.FarPlane());
        DrawTerrainMesh(proj * view, wireframe);
    }
    // Framebuffer bleibt absichtlich gebunden - weitere Renderer (z.B. Objekt-Marker) können
    // jetzt in denselben Pass zeichnen. EndScene() schließt ab.
}

void HeightmapRenderer::DrawTerrainMesh(const Mat4& viewProj, bool wireframe) {
    if (!terrainVisible_) return;
    glUseProgram(shaderProgram_);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram_, "uMvp"), 1, GL_FALSE, viewProj.m);
    glUniform3f(glGetUniformLocation(shaderProgram_, "uLightDir"), -0.4f, -1.0f, -0.3f);
    glUniform1f(glGetUniformLocation(shaderProgram_, "uMinHeight"), minHeight_);
    glUniform1f(glGetUniformLocation(shaderProgram_, "uMaxHeight"), maxHeight_);
    glUniform1i(glGetUniformLocation(shaderProgram_, "uUseTextures"), textureLayerCount_ > 0 ? 1 : 0);
    glUniform2f(glGetUniformLocation(shaderProgram_, "uMapSpan"), mapSpanX_, mapSpanZ_);
    glUniform2f(glGetUniformLocation(shaderProgram_, "uBlockSize"),
                std::max(blockW_, 0.001f), std::max(blockH_, 0.001f));

    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    glBindVertexArray(vao_);
    // Layer in Gruppen zu je kLayersPerPass zeichnen (Textur-Einheiten-Limit) und additiv
    // zusammenfuehren: color = Summe(gewicht * diffuse), das ist linear - jeder Durchgang liefert
    // seinen Teil, alle Layer ausserhalb ihrer Region tragen 0 bei (siehe SampleLayer).
    const int passes = std::max(1, (textureLayerCount_ + kLayersPerPass - 1) / kLayersPerPass);
    for (int pass = 0; pass < passes; ++pass) {
        const int first = pass * kLayersPerPass;
        const int inPass = std::max(0, std::min(kLayersPerPass, textureLayerCount_ - first));
        float uvScales[kLayersPerPass] = {};
        float regions[kLayersPerPass * 4] = {};
        for (int k = 0; k < inPass; ++k) {
            const int li = first + k;
            uvScales[k] = layerUvScale_[li];
            const float* cells = layerRegionCells_[li];
            if (cells[2] > 0.0f && cells[3] > 0.0f) {
                regions[k * 4 + 0] = cells[0] * blockW_;
                regions[k * 4 + 1] = cells[1] * blockH_;
                regions[k * 4 + 2] = cells[2] * blockW_;
                regions[k * 4 + 3] = cells[3] * blockH_;
            } else {
                regions[k * 4 + 0] = 0.0f;
                regions[k * 4 + 1] = 0.0f;
                regions[k * 4 + 2] = mapSpanX_;
                regions[k * 4 + 3] = mapSpanZ_;
            }
        }
        glUniform1i(glGetUniformLocation(shaderProgram_, "uLayerCount"), inPass);
        glUniform1fv(glGetUniformLocation(shaderProgram_, "uUvScale"), kLayersPerPass, uvScales);
        glUniform4fv(glGetUniformLocation(shaderProgram_, "uLayerRegion"), kLayersPerPass, regions);
        float visible[kLayersPerPass] = {};
        for (int k = 0; k < inPass; ++k) visible[k] = layerVisible_[first + k];
        glUniform1fv(glGetUniformLocation(shaderProgram_, "uLayerVisible"), kLayersPerPass, visible);
        for (int k = 0; k < inPass; ++k) {
            const int li = first + k;
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(k * 2));
            glBindTexture(GL_TEXTURE_2D, diffuseTex_[li]);
            glUniform1i(glGetUniformLocation(shaderProgram_, ("uDiffuse" + std::to_string(k)).c_str()), k * 2);
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(k * 2 + 1));
            glBindTexture(GL_TEXTURE_2D, blendTex_[li]);
            glUniform1i(glGetUniformLocation(shaderProgram_, ("uBlend" + std::to_string(k)).c_str()), k * 2 + 1);
        }
        if (pass > 0) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);
        }
        glDrawElements(GL_TRIANGLES, static_cast<int>(indexCount_), GL_UNSIGNED_INT, nullptr);
    }
    if (passes > 1) {
        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
    }
    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glActiveTexture(GL_TEXTURE0); // zurücksetzen, damit andere Renderer nicht durcheinanderkommen
}

void HeightmapRenderer::EnsureFramebuffer2d(int width, int height) {
    if (fbo2d_ != 0 && width == fbo2dWidth_ && height == fbo2dHeight_) {
        return;
    }
    if (fbo2dColorTex_) glDeleteTextures(1, &fbo2dColorTex_);
    if (fbo2dDepthRbo_) glDeleteRenderbuffers(1, &fbo2dDepthRbo_);
    if (fbo2d_) glDeleteFramebuffers(1, &fbo2d_);

    glGenFramebuffers(1, &fbo2d_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo2d_);

    glGenTextures(1, &fbo2dColorTex_);
    glBindTexture(GL_TEXTURE_2D, fbo2dColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo2dColorTex_, 0);

    glGenRenderbuffers(1, &fbo2dDepthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, fbo2dDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo2dDepthRbo_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[Renderer] 2D-Framebuffer unvollständig!\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fbo2dWidth_ = width;
    fbo2dHeight_ = height;
}

void HeightmapRenderer::BeginTopDownScene(int width, int height) {
    if (width <= 0 || height <= 0) return;
    EnsureFramebuffer2d(width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo2d_);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (indexCount_ > 0) {
        const bool windowed = tdHalfW_ > 0.0f && tdHalfH_ > 0.0f;
        const float centerX = windowed ? tdCenterX_ : mapSpanX_ * 0.5f;
        const float centerZ = windowed ? tdCenterZ_ : mapSpanZ_ * 0.5f;
        const float halfW = windowed ? tdHalfW_ : std::max(mapSpanX_ * 0.5f, 1.0f);
        const float halfH = windowed ? tdHalfH_ : std::max(mapSpanZ_ * 0.5f, 1.0f);
        const float eyeHeight = maxHeight_ + 500.0f;
        const Mat4 viewProj = OrthoTopDownViewProj(centerX, centerZ, halfW, halfH, eyeHeight);
        DrawTerrainMesh(viewProj, false);
    }
}

void HeightmapRenderer::DrawTopDownOverlay(std::uint32_t overlayTexture, float alpha, float uvScaleX, float uvScaleY,
                                           float uvOffsetX, float uvOffsetY) {
    if (overlayTexture == 0) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(overlayShaderProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, overlayTexture);
    glUniform1i(glGetUniformLocation(overlayShaderProgram_, "uTex"), 0);
    glUniform1f(glGetUniformLocation(overlayShaderProgram_, "uAlpha"), alpha);
    glUniform2f(glGetUniformLocation(overlayShaderProgram_, "uUvScale"), uvScaleX, uvScaleY);
    glUniform2f(glGetUniformLocation(overlayShaderProgram_, "uUvOffset"), uvOffsetX, uvOffsetY);

    glBindVertexArray(overlayVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
}

std::uint32_t HeightmapRenderer::EndTopDownScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo2dColorTex_;
}

std::uint32_t HeightmapRenderer::EndScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fboColorTex_;
}

} // namespace theseed::mapeditor::app
