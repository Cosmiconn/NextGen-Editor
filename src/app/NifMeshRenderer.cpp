#include "NifMeshRenderer.hpp"
#include <system_error>
#include <optional>
#include <expected>
#include "mapeditor/core/AvatarPreview.hpp"

#include "mapeditor/core/DdsImage.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#endif

namespace theseed::mapeditor::app {

namespace {

// NIF-Materialshader: klassische NiTexturingProperty-Slots plus Material-/Specular-/Emissive-
// Beleuchtung. Bis zu acht UV-Sets bleiben parallel im Vertexstream; jeder Texturslot waehlt
// sein eigenes Set und wendet seine NIF-Texture-Transform im Fragmentshader an.
const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv0;
layout(location = 3) in vec2 aUv1;
layout(location = 4) in vec2 aUv2;
layout(location = 5) in vec2 aUv3;
layout(location = 6) in vec2 aUv4;
layout(location = 7) in vec2 aUv5;
layout(location = 8) in vec2 aUv6;
layout(location = 9) in vec2 aUv7;
layout(location = 10) in vec4 aColor;

uniform mat4 uViewProj;
uniform mat4 uModel;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv0;
out vec2 vUv1;
out vec2 vUv2;
out vec2 vUv3;
out vec2 vUv4;
out vec2 vUv5;
out vec2 vUv6;
out vec2 vUv7;
out vec4 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUv0 = aUv0; vUv1 = aUv1; vUv2 = aUv2; vUv3 = aUv3;
    vUv4 = aUv4; vUv5 = aUv5; vUv6 = aUv6; vUv7 = aUv7;
    vColor = aColor;
    gl_Position = uViewProj * worldPos;
}
)";

const char* kFragmentShaderSrc = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUv0;
in vec2 vUv1;
in vec2 vUv2;
in vec2 vUv3;
in vec2 vUv4;
in vec2 vUv5;
in vec2 vUv6;
in vec2 vUv7;
in vec4 vColor;
out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform mat4 uView;
uniform vec3 uAmbientColor;
uniform vec3 uDiffuseColor;
uniform vec3 uSpecularColor;
uniform vec3 uEmissiveColor;
uniform float uGlossiness;
uniform bool uSpecularEnabled;
uniform int uApplyMode;
uniform bool uVcAlphaTextureBlender;
uniform int uVertexColorMode;
uniform bool uHasTex[10];
uniform int uUvSet[10];
uniform bool uHasTexTransform[10];
uniform vec2 uTexTranslation[10];
uniform vec2 uTexScale[10];
uniform float uTexRotation[10];
uniform int uTexTransformType[10];
uniform vec2 uTexCenter[10];
uniform sampler2D uTex0;
uniform sampler2D uTex1;
uniform sampler2D uTex2;
uniform sampler2D uTex3;
uniform sampler2D uTex4;
uniform sampler2D uTex5;
uniform sampler2D uTex6;
uniform sampler2D uTex7;
uniform sampler2D uTex8;
uniform sampler2D uTex9;
// NiTextureEffect is separate from NiTexturingProperty. The verified Fiesta path is
// TEX_ENVIRONMENT_MAP + CG_SPHERE_MAP. GL 3.3 guarantees 16 fragment texture units,
// so six effect samplers fit beside the ten classic slots.
uniform int uEnvironmentSphereCount;
uniform sampler2D uEnvironmentTex0;
uniform sampler2D uEnvironmentTex1;
uniform sampler2D uEnvironmentTex2;
uniform sampler2D uEnvironmentTex3;
uniform sampler2D uEnvironmentTex4;
uniform sampler2D uEnvironmentTex5;
uniform float uBumpLumaScale;
uniform float uBumpLumaOffset;
uniform mat2 uBumpMatrix;
uniform float uAlphaCutoff;
uniform float uMaterialAlpha;
uniform bool uAlphaTest;
uniform int uAlphaTestFunc;

vec2 pickUv(int setIndex) {
    if (setIndex == 1) return vUv1;
    if (setIndex == 2) return vUv2;
    if (setIndex == 3) return vUv3;
    if (setIndex == 4) return vUv4;
    if (setIndex == 5) return vUv5;
    if (setIndex == 6) return vUv6;
    if (setIndex == 7) return vUv7;
    return vUv0;
}

vec2 rotateTextureUv(vec2 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

vec2 slotUv(int slot) {
    vec2 uv = pickUv(clamp(uUvSet[slot], 0, 7));
    if (!uHasTexTransform[slot]) return uv;

    const int TM_MAYA_DEPRECATED = 0;
    const int TM_MAX = 1;
    const int TM_MAYA = 2;
    int method = uTexTransformType[slot];

    // nif.xml TransformMethod matrix order, applied right-to-left to the UV column vector:
    // 0: Center * Rotation * Back * Translate * Scale
    // 1: Center * Scale * Rotation * Translate * Back
    // 2: Center * Rotation * Back * FromMaya * Translate * Scale
    if (method == TM_MAX) {
        vec2 p = uv - uTexCenter[slot];          // Back
        p += uTexTranslation[slot];              // Translate
        p = rotateTextureUv(p, uTexRotation[slot]);
        p *= uTexScale[slot];                    // Scale
        return p + uTexCenter[slot];             // Center
    }

    vec2 p = uv * uTexScale[slot];               // Scale
    p += uTexTranslation[slot];                  // Translate
    if (method == TM_MAYA) p.y = 1.0 - p.y;     // FromMaya
    // Unknown values deliberately follow the format default (TM_MAYA_DEPRECATED).
    p -= uTexCenter[slot];                        // Back
    p = rotateTextureUv(p, uTexRotation[slot]);  // Rotation
    return p + uTexCenter[slot];                 // Center
}

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

vec3 bumpNormal(vec3 baseNormal) {
    if (!uHasTex[5]) return baseNormal;
    vec2 uv = slotUv(5);
    ivec2 sz = textureSize(uTex5, 0);
    vec2 texel = 1.0 / max(vec2(sz), vec2(1.0));
    float h0 = luma(texture(uTex5, uv).rgb) * uBumpLumaScale + uBumpLumaOffset;
    float hx = luma(texture(uTex5, uv + vec2(texel.x, 0.0)).rgb) * uBumpLumaScale + uBumpLumaOffset;
    float hy = luma(texture(uTex5, uv + vec2(0.0, texel.y)).rgb) * uBumpLumaScale + uBumpLumaOffset;
    vec2 grad = uBumpMatrix * vec2(hx - h0, hy - h0);

    vec3 dp1 = dFdx(vWorldPos);
    vec3 dp2 = dFdy(vWorldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) < 1e-8) return baseNormal;
    vec3 tangent = normalize((dp1 * duv2.y - dp2 * duv1.y) / det);
    vec3 bitangent = normalize((-dp1 * duv2.x + dp2 * duv1.x) / det);
    return normalize(baseNormal - tangent * grad.x - bitangent * grad.y);
}

vec2 environmentSphereUv(vec3 worldNormal) {
    // Classic GL_SPHERE_MAP is defined in eye coordinates: u points from the eye-space
    // origin to the fragment/vertex and n is the normal transformed to eye space.
    // Projecting a world-space reflection vector would make the environment pattern rotate
    // with the world instead of remaining camera-relative.
    vec3 eyePosition = (uView * vec4(vWorldPos, 1.0)).xyz;
    vec3 eyeNormal = normalize(mat3(uView) * worldNormal);
    float eyeLen2 = dot(eyePosition, eyePosition);
    if (eyeLen2 <= 1e-12) return vec2(0.5);
    vec3 u = eyePosition * inversesqrt(eyeLen2);
    vec3 r = normalize(reflect(u, eyeNormal));
    float m2 = r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0);
    if (m2 <= 1e-12) return vec2(0.5);
    float m = 2.0 * sqrt(m2);
    return r.xy / m + vec2(0.5);
}

vec3 environmentSphereColor(vec2 uv) {
    vec3 sum = vec3(0.0);
    if (uEnvironmentSphereCount > 0) sum += texture(uEnvironmentTex0, uv).rgb;
    if (uEnvironmentSphereCount > 1) sum += texture(uEnvironmentTex1, uv).rgb;
    if (uEnvironmentSphereCount > 2) sum += texture(uEnvironmentTex2, uv).rgb;
    if (uEnvironmentSphereCount > 3) sum += texture(uEnvironmentTex3, uv).rgb;
    if (uEnvironmentSphereCount > 4) sum += texture(uEnvironmentTex4, uv).rgb;
    if (uEnvironmentSphereCount > 5) sum += texture(uEnvironmentTex5, uv).rgb;
    return sum;
}

void main() {
    vec4 base = uHasTex[0] ? texture(uTex0, slotUv(0)) : vec4(1.0);

    // Classic NiVertexColorProperty follows OpenGL color-material semantics.
    // SRC_AMB_DIF replaces authored ambient+diffuse with the per-vertex color;
    // SRC_EMISSIVE replaces authored emission. The dedicated VCAlpha shader keeps
    // its original path below because its RGB/alpha inputs have different semantics.
    vec3 materialAmbient = uAmbientColor;
    vec3 materialDiffuse = uDiffuseColor;
    vec3 materialEmission = uEmissiveColor;
    if (!uVcAlphaTextureBlender) {
        if (uVertexColorMode == 2) {
            materialAmbient = vColor.rgb;
            materialDiffuse = vColor.rgb;
        } else if (uVertexColorMode == 1) {
            materialEmission = vColor.rgb;
        }
    }

    vec3 surface = materialDiffuse;
    if (uVcAlphaTextureBlender && uHasTex[0] && uHasTex[1] && uHasTex[2]) {
        // Original Gamebryo VCAlphaTextureBlender-P.hlsl:
        // Texture1/Texture2 are blended by vertex alpha, then multiplied by Detail*2.
        vec3 texture1 = texture(uTex0, slotUv(0)).rgb;
        vec3 texture2 = texture(uTex1, slotUv(1)).rgb;
        vec3 blended = mix(texture2, texture1, clamp(vColor.a, 0.0, 1.0));
        vec3 detail = texture(uTex2, slotUv(2)).rgb * 2.0;
        surface = uDiffuseColor * vColor.rgb * blended * detail;
    } else {
        if (uHasTex[0]) {
            if (uApplyMode == 0) surface = base.rgb;                         // APPLY_REPLACE
            else if (uApplyMode == 1) surface = mix(surface, base.rgb, base.a); // APPLY_DECAL
            else surface *= base.rgb;                                       // APPLY_MODULATE/HILIGHT fallback
        }
        if (uHasTex[1]) surface *= texture(uTex1, slotUv(1)).rgb; // Dark map
        if (uHasTex[2]) surface *= clamp(texture(uTex2, slotUv(2)).rgb * 2.0, 0.0, 2.0); // Detail map
    }

    // Decals are layered in file order. Their alpha controls only the sticker blend, not the
    // alpha of the underlying surface.
    if (uHasTex[6]) { vec4 d = texture(uTex6, slotUv(6)); surface = mix(surface, d.rgb, d.a); }
    if (uHasTex[7]) { vec4 d = texture(uTex7, slotUv(7)); surface = mix(surface, d.rgb, d.a); }
    if (uHasTex[8]) { vec4 d = texture(uTex8, slotUv(8)); surface = mix(surface, d.rgb, d.a); }
    if (uHasTex[9]) { vec4 d = texture(uTex9, slotUv(9)); surface = mix(surface, d.rgb, d.a); }

    float alpha = uMaterialAlpha;
    if (!uVcAlphaTextureBlender && uHasTex[0] && uApplyMode != 1) alpha *= base.a;
    if (uAlphaTest) {
        bool passAlpha = true;
        if (uAlphaTestFunc == 0) passAlpha = false;
        else if (uAlphaTestFunc == 1) passAlpha = alpha < uAlphaCutoff;
        else if (uAlphaTestFunc == 2) passAlpha = abs(alpha - uAlphaCutoff) < (1.0 / 255.0);
        else if (uAlphaTestFunc == 3) passAlpha = alpha <= uAlphaCutoff;
        else if (uAlphaTestFunc == 4) passAlpha = alpha > uAlphaCutoff;
        else if (uAlphaTestFunc == 5) passAlpha = abs(alpha - uAlphaCutoff) >= (1.0 / 255.0);
        else if (uAlphaTestFunc == 6) passAlpha = alpha >= uAlphaCutoff;
        else if (uAlphaTestFunc == 7) passAlpha = true;
        if (!passAlpha) discard;
    }

    vec3 n = bumpNormal(normalize(vNormal));
    vec3 l = normalize(-uLightDir);
    vec3 v = normalize(uCameraPos - vWorldPos);
    vec3 h = normalize(l + v);
    float ndl = max(dot(n, l), 0.0);
    float shininess = clamp(uGlossiness, 0.0, 128.0);
    float specPower = (uSpecularEnabled && ndl > 0.0) ? pow(max(dot(n, h), 0.0), max(shininess, 1.0)) : 0.0;
    float glossMask = uHasTex[3] ? luma(texture(uTex3, slotUv(3)).rgb) : 1.0;
    vec3 glow = uHasTex[4] ? texture(uTex4, slotUv(4)).rgb : vec3(0.0);

    vec3 ambient = surface * materialAmbient * 0.28;
    vec3 diffuse = surface * (0.22 + 0.78 * ndl);
    vec3 specular = uSpecularColor * specPower * glossMask;
    vec3 emissive = materialEmission + glow;
    // NIF TextureType::TEX_ENVIRONMENT_MAP is additive to the ordinary textured,
    // lit/decal result. It does not replace or multiply the base material.
    vec3 environment = uEnvironmentSphereCount > 0
        ? environmentSphereColor(environmentSphereUv(n))
        : vec3(0.0);
    FragColor = vec4(ambient + diffuse + specular + emissive + environment, alpha);
}
)";

std::uint32_t CompileShader(std::uint32_t type, const char* src) {
    const std::uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[NifMeshRenderer] Shader-Kompilierfehler: %s\n", log);
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
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[NifMeshRenderer] Programm-Linkfehler: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Berechnet pro Vertex gemittelte Normalen aus den Dreiecken, falls das Mesh keine eigenen
// Normalen mitbringt (in den bisher erfolgreich geladenen Dateien immer vorhanden, aber
// core::NifModel erlaubt leere normals[] - hier defensiv abgefangen).
#ifdef _WIN32
std::expected<core::DdsImage, std::string> LoadWicImage(const std::filesystem::path& file) {
    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninit = SUCCEEDED(initHr);
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr) || factory == nullptr) {
        if (uninit) CoUninitialize();
        return std::unexpected("Windows Imaging Component konnte nicht initialisiert werden");
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || decoder == nullptr) {
        factory->Release();
        if (uninit) CoUninitialize();
        return std::unexpected("WIC konnte die Rastertextur nicht oeffnen");
    }
    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || frame == nullptr) {
        decoder->Release(); factory->Release();
        if (uninit) CoUninitialize();
        return std::unexpected("WIC konnte Frame 0 nicht dekodieren");
    }
    UINT width = 0, height = 0;
    frame->GetSize(&width, &height);
    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr) && converter != nullptr) {
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
    }
    core::DdsImage out;
    if (FAILED(hr) || converter == nullptr || width == 0 || height == 0) {
        if (converter) converter->Release();
        frame->Release(); decoder->Release(); factory->Release();
        if (uninit) CoUninitialize();
        return std::unexpected("WIC konnte das Bild nicht nach RGBA8 konvertieren");
    }
    out.width = width; out.height = height;
    out.rgba.resize(static_cast<std::size_t>(width) * height * 4u);
    const UINT stride = width * 4u;
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(out.rgba.size()), out.rgba.data());
    converter->Release(); frame->Release(); decoder->Release(); factory->Release();
    if (uninit) CoUninitialize();
    if (FAILED(hr)) return std::unexpected("WIC CopyPixels ist fehlgeschlagen");

    // WIC liefert top-down; DdsImage/TGA/NiPixelData sind fuer den Renderer bereits vertikal
    // auf OpenGL ausgerichtet. Deshalb hier dieselbe Konvention herstellen.
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
    std::vector<std::uint8_t> tmp(rowBytes);
    for (std::uint32_t y = 0; y < height / 2u; ++y) {
        auto* a = out.rgba.data() + static_cast<std::size_t>(y) * rowBytes;
        auto* b = out.rgba.data() + static_cast<std::size_t>(height - 1u - y) * rowBytes;
        std::copy(a, a + rowBytes, tmp.data());
        std::copy(b, b + rowBytes, a);
        std::copy(tmp.data(), tmp.data() + rowBytes, b);
    }
    return out;
}
#endif


std::string TextureLookupKey(std::string value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isspace(ch)) continue;
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

bool IsTextureExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return ext == ".dds" || ext == ".tga" || ext == ".bmp" ||
           ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

// mapDir normally is <Client>/resmap/field/<Map>. NPC previews deliberately pass a fake
// two-level path below <Client>. Keep both layouts supported and never assume a global drive.
std::filesystem::path DeriveClientAssetRoot(const std::filesystem::path& mapDir) {
    if (mapDir.empty()) return {};
    const auto assetRoot = mapDir.parent_path().parent_path();
    if (assetRoot.empty()) return {};
    if (core::legacy::EqualsCaseInsensitive(assetRoot.filename().string(), "resmap"))
        return assetRoot.parent_path();
    return assetRoot;
}

struct ClientTextureIndex {
    bool built = false;
    std::unordered_map<std::string, std::filesystem::path> unique;
    std::unordered_set<std::string> ambiguous;

    void Build(const std::filesystem::path& clientRoot) {
        if (built) return;
        built = true;
        if (clientRoot.empty()) return;

        // Search only known Fiesta asset trees. This is deliberately narrower than a recursive
        // search of the whole client, so a stray backup/export file cannot silently become a
        // material texture.
        static constexpr const char* kRoots[] = {
            "resmap", "resitem", "reseffect", "reschar", "resmenu", "ressystem"
        };
        for (const char* rootName : kRoots) {
            auto resolvedRoot = core::legacy::ResolveCaseInsensitivePath(clientRoot, rootName);
            if (!resolvedRoot) continue;
            std::error_code ec;
            std::filesystem::recursive_directory_iterator it(
                *resolvedRoot, std::filesystem::directory_options::skip_permission_denied, ec);
            const std::filesystem::recursive_directory_iterator end;
            for (; it != end; it.increment(ec)) {
                if (ec) { ec.clear(); continue; }
                if (!it->is_regular_file(ec) || ec || !IsTextureExtension(it->path())) {
                    ec.clear();
                    continue;
                }
                const std::string key = TextureLookupKey(it->path().filename().string());
                if (key.empty() || ambiguous.contains(key)) continue;
                const auto [found, inserted] = unique.emplace(key, it->path());
                if (!inserted && found->second != it->path()) {
                    unique.erase(found);
                    ambiguous.insert(key);
                }
            }
        }
    }

    std::optional<std::filesystem::path> FindUnique(const std::filesystem::path& clientRoot,
                                                    const std::string& filename) {
        Build(clientRoot);
        const std::string key = TextureLookupKey(filename);
        if (key.empty() || ambiguous.contains(key)) return std::nullopt;
        const auto it = unique.find(key);
        return it == unique.end() ? std::nullopt
                                  : std::optional<std::filesystem::path>(it->second);
    }
};

std::vector<core::NifVec3> ComputeFallbackNormals(const core::NifMeshPart& part) {
    std::vector<core::NifVec3> normals(part.positions.size(), core::NifVec3{0.0f, 0.0f, 0.0f});
    for (std::size_t i = 0; i + 2 < part.triangleIndices.size(); i += 3) {
        const auto ia = part.triangleIndices[i];
        const auto ib = part.triangleIndices[i + 1];
        const auto ic = part.triangleIndices[i + 2];
        if (ia >= part.positions.size() || ib >= part.positions.size() || ic >= part.positions.size()) continue;
        const auto& a = part.positions[ia];
        const auto& b = part.positions[ib];
        const auto& c = part.positions[ic];
        const core::NifVec3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
        const core::NifVec3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
        const core::NifVec3 n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        for (auto idx : {ia, ib, ic}) {
            normals[idx].x += n.x;
            normals[idx].y += n.y;
            normals[idx].z += n.z;
        }
    }
    for (auto& n : normals) {
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 1e-6f) {
            n.x /= len; n.y /= len; n.z /= len;
        } else {
            n = {0.0f, 1.0f, 0.0f};
        }
    }
    return normals;
}

} // namespace

std::expected<core::DdsImage, std::string> LoadPlatformRasterImage(
    const std::filesystem::path& file) {
#ifdef _WIN32
    return LoadWicImage(file);
#else
    (void)file;
    return std::unexpected("Plattform-Rasterdecoder ist nur unter Windows verfügbar");
#endif
}

NifMeshRenderer::~NifMeshRenderer() { Shutdown(); }

void NifMeshRenderer::Init() {
    if (shaderProgram_) Shutdown();
    const std::uint32_t vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    const std::uint32_t fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    shaderProgram_ = LinkProgram(vs, fs);
    uniforms_.locViewProj = glGetUniformLocation(shaderProgram_, "uViewProj");
    uniforms_.locView = glGetUniformLocation(shaderProgram_, "uView");
    uniforms_.locModel = glGetUniformLocation(shaderProgram_, "uModel");
    uniforms_.locLightDir = glGetUniformLocation(shaderProgram_, "uLightDir");
    uniforms_.locCameraPos = glGetUniformLocation(shaderProgram_, "uCameraPos");
    uniforms_.locAmbientColor = glGetUniformLocation(shaderProgram_, "uAmbientColor");
    uniforms_.locDiffuseColor = glGetUniformLocation(shaderProgram_, "uDiffuseColor");
    uniforms_.locSpecularColor = glGetUniformLocation(shaderProgram_, "uSpecularColor");
    uniforms_.locEmissiveColor = glGetUniformLocation(shaderProgram_, "uEmissiveColor");
    uniforms_.locGlossiness = glGetUniformLocation(shaderProgram_, "uGlossiness");
    uniforms_.locSpecularEnabled = glGetUniformLocation(shaderProgram_, "uSpecularEnabled");
    uniforms_.locApplyMode = glGetUniformLocation(shaderProgram_, "uApplyMode");
    uniforms_.locVcAlphaTextureBlender = glGetUniformLocation(shaderProgram_, "uVcAlphaTextureBlender");
    uniforms_.locVertexColorMode = glGetUniformLocation(shaderProgram_, "uVertexColorMode");
    uniforms_.locBumpLumaScale = glGetUniformLocation(shaderProgram_, "uBumpLumaScale");
    uniforms_.locBumpLumaOffset = glGetUniformLocation(shaderProgram_, "uBumpLumaOffset");
    uniforms_.locBumpMatrix = glGetUniformLocation(shaderProgram_, "uBumpMatrix");
    uniforms_.locAlphaTest = glGetUniformLocation(shaderProgram_, "uAlphaTest");
    uniforms_.locAlphaCutoff = glGetUniformLocation(shaderProgram_, "uAlphaCutoff");
    uniforms_.locAlphaTestFunc = glGetUniformLocation(shaderProgram_, "uAlphaTestFunc");
    uniforms_.locMaterialAlpha = glGetUniformLocation(shaderProgram_, "uMaterialAlpha");
    uniforms_.locEnvironmentSphereCount = glGetUniformLocation(shaderProgram_, "uEnvironmentSphereCount");

    for (int slot = 0; slot < 10; ++slot) {
        char name[64];
        std::snprintf(name, sizeof(name), "uHasTex[%d]", slot); uniforms_.locHasTex[slot] = glGetUniformLocation(shaderProgram_, name);
        std::snprintf(name, sizeof(name), "uUvSet[%d]", slot); uniforms_.locUvSet[slot] = glGetUniformLocation(shaderProgram_, name);
        std::snprintf(name, sizeof(name), "uHasTexTransform[%d]", slot); uniforms_.locHasTransform[slot] = glGetUniformLocation(shaderProgram_, name);
        std::snprintf(name, sizeof(name), "uTexTranslation[%d]", slot); uniforms_.locTranslation[slot] = glGetUniformLocation(shaderProgram_, name);
        std::snprintf(name, sizeof(name), "uTexScale[%d]", slot); uniforms_.locScale[slot] = glGetUniformLocation(shaderProgram_, name);
        std::snprintf(name, sizeof(name), "uTexRotation[%d]", slot); uniforms_.locRotation[slot] = glGetUniformLocation(shaderProgram_, name);
        std::snprintf(name, sizeof(name), "uTexTransformType[%d]", slot); uniforms_.locTransformType[slot] = glGetUniformLocation(shaderProgram_, name);
        std::snprintf(name, sizeof(name), "uTexCenter[%d]", slot); uniforms_.locCenter[slot] = glGetUniformLocation(shaderProgram_, name);
        std::snprintf(name, sizeof(name), "uTex%d", slot); uniforms_.locSampler[slot] = glGetUniformLocation(shaderProgram_, name);
    }
    for (std::size_t effect = 0; effect < kMaxEnvironmentSphereEffects; ++effect) {
        char name[64];
        std::snprintf(name, sizeof(name), "uEnvironmentTex%zu", effect);
        uniforms_.locEnvironmentSampler[effect] = glGetUniformLocation(shaderProgram_, name);
    }

}

void NifMeshRenderer::ReleaseModel(LoadedModel& model) {
    for (auto& sub : model.subMeshes) {
        if (sub.vbo) glDeleteBuffers(1, &sub.vbo);
        if (sub.ebo) glDeleteBuffers(1, &sub.ebo);
        if (sub.vao) glDeleteVertexArrays(1, &sub.vao);
        // Slot-Texturen werden NICHT hier geloescht - sie leben im textureCache_ und werden
        // ggf. von mehreren SubMeshes/Modellen geteilt, siehe GetOrLoadTexture.
    }
    model.subMeshes.clear();
}

void NifMeshRenderer::Shutdown() {
    for (auto& [path, model] : modelCache_) {
        ReleaseModel(model);
    }
    modelCache_.clear();
    for (auto& [path, tex] : textureCache_) {
        if (tex) glDeleteTextures(1, &tex);
    }
    textureCache_.clear();
    perObjectModel_.clear();
    opaqueItems_.clear();
    blendedItems_.clear();
    if (shaderProgram_) glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
}

std::uint32_t NifMeshRenderer::GetOrLoadTexture(const std::filesystem::path& resolvedPath) {
    const std::string key = resolvedPath.string();
    auto it = textureCache_.find(key);
    if (it != textureCache_.end()) {
        return it->second;
    }

    std::uint32_t tex = 0;
    const auto ext = resolvedPath.extension().string();
    std::string lowerExt = ext;
    for (char& ch : lowerExt) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    std::expected<core::DdsImage, std::string> imageResult = std::unexpected("Nicht unterstuetztes Rasterformat");
    if (lowerExt == ".tga") imageResult = core::LoadTgaImage(resolvedPath);
    else if (lowerExt == ".dds") imageResult = core::LoadDdsImage(resolvedPath);
#ifdef _WIN32
    else if (lowerExt == ".jpg" || lowerExt == ".jpeg" || lowerExt == ".png" || lowerExt == ".bmp")
        imageResult = LoadWicImage(resolvedPath);
#endif
    if (imageResult) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<int>(imageResult->width), static_cast<int>(imageResult->height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, imageResult->rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else {
        std::fprintf(stderr, "[NifMeshRenderer] Objekt-Textur nicht ladbar (%s): %s\n",
                      resolvedPath.string().c_str(), imageResult.error().c_str());
    }
    textureCache_.emplace(key, tex);
    return tex;
}

std::uint32_t NifMeshRenderer::GetOrLoadEmbeddedTexture(const core::NifEmbeddedTexture& image,
                                                             const std::string& cacheKey) {
    auto it = textureCache_.find(cacheKey);
    if (it != textureCache_.end()) return it->second;

    std::uint32_t tex = 0;
    if (image.width == 0 || image.height == 0 ||
        image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4) {
        std::fprintf(stderr, "[NifMeshRenderer] Ungültige eingebettete Textur: %s\n", cacheKey.c_str());
        textureCache_.emplace(cacheKey, 0);
        return 0;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<int>(image.width), static_cast<int>(image.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    textureCache_.emplace(cacheKey, tex);
    return tex;
}

void NifMeshRenderer::LoadModelsForSet(const core::ObjectPlacementSet& set, const std::filesystem::path& mapDir,
                                       const std::unordered_map<std::string, core::NifModel>* custom) {
    for (auto& [path, model] : modelCache_) {
        ReleaseModel(model);
    }
    modelCache_.clear();
    // Textur-Cache bewusst NICHT geleert - Texturen sind unabhängig vom Modell-Cache gültig
    // und werden oft von Modellen auf verschiedenen Karten wiederverwendet (z.B. "grass.dds").
    perObjectModel_.assign(set.Count(), nullptr);
    std::unordered_map<std::string, std::optional<std::filesystem::path>> resolvedModels;
    std::unordered_map<std::string, std::optional<std::filesystem::path>> resolvedTextures;
    const std::filesystem::path clientAssetRoot = DeriveClientAssetRoot(mapDir);
    ClientTextureIndex clientTextureIndex;

    for (std::size_t i = 0; i < set.Count(); ++i) {
        const auto& obj = set.At(i);
        if (obj.modelPath.empty()) continue;

        const core::NifModel* customModel = nullptr;
        if (custom != nullptr) {
            if (const auto c = custom->find(obj.modelPath); c != custom->end()) customModel = &c->second;
        }
        std::optional<std::filesystem::path> resolved;
        if (customModel == nullptr) {
            auto [entry, inserted] = resolvedModels.try_emplace(obj.modelPath);
            if (inserted) entry->second = core::legacy::ResolveLegacyAssetPath(mapDir, obj.modelPath);
            resolved = entry->second;
            if (!resolved) {
                if (inserted) std::fprintf(stderr, "[NifMeshRenderer] NIF nicht gefunden: model=%s mapDir=%s\n",
                             obj.modelPath.c_str(), mapDir.string().c_str());
                continue;
            }
        }
        const std::string key = customModel != nullptr ? "custom:" + obj.modelPath : resolved->string();
        const std::filesystem::path modelDir = resolved ? resolved->parent_path() : std::filesystem::path();

        auto it = modelCache_.find(key);
        if (it == modelCache_.end()) {
            std::expected<core::NifModel, std::string> nifResult =
                customModel != nullptr ? std::expected<core::NifModel, std::string>(*customModel) : core::LoadNifMesh(*resolved);
            // Charakter-NIFs (reschar/...): Knochen-Huellen und Detailstufen-Duplikate ausblenden.
            if (nifResult && customModel == nullptr) {
                std::string lowerPath = obj.modelPath;
                for (char& ch : lowerPath) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (lowerPath.rfind("reschar", 0) == 0 || lowerPath.find("\\reschar\\") != std::string::npos || lowerPath.find("/reschar/") != std::string::npos)
                    core::SimplifyCharacterModel(*nifResult);
            }
            LoadedModel model;
            if (nifResult) {
                model.subMeshes.reserve(nifResult->parts.size());
                for (const auto& part : nifResult->parts) {
                    if (part.positions.empty() || part.triangleIndices.empty()) continue;

                    const std::vector<core::NifVec3>& normals =
                        part.normals.size() == part.positions.size() ? part.normals : ComputeFallbackNormals(part);

                    // Bis zu acht UV-Sets werden als eigene Vertex-Attribute erhalten. Die klassischen
                    // NiTexturingProperty-Slots waehlen ihr Set spaeter per Uniform; dadurch koennen
                    // Base/Detail/Decal unterschiedliche UV-Kanaele benutzen.
                    constexpr std::size_t kGpuUvSets = 8;
                    constexpr std::size_t kVertexStrideFloats = 6 + kGpuUvSets * 2 + 4;
                    std::vector<float> vertexData;
                    vertexData.reserve(part.positions.size() * kVertexStrideFloats);
                    for (std::size_t v = 0; v < part.positions.size(); ++v) {
                        vertexData.insert(vertexData.end(), {
                            part.positions[v].x, part.positions[v].y, part.positions[v].z,
                            normals[v].x, normals[v].y, normals[v].z,
                        });
                        for (std::size_t uvSet = 0; uvSet < kGpuUvSets; ++uvSet) {
                            float u = 0.0f, vv = 0.0f;
                            if (uvSet < part.uvSets.size() && part.uvSets[uvSet].size() == part.positions.size()) {
                                u = part.uvSets[uvSet][v].u;
                                vv = part.uvSets[uvSet][v].v;
                            } else if (uvSet == 0 && part.uvs.size() == part.positions.size()) {
                                u = part.uvs[v].u; vv = part.uvs[v].v;
                            }
                            vertexData.push_back(u); vertexData.push_back(vv);
                        }
                        const core::NifColor4 color =
                            (part.vertexColors.size() == part.positions.size()) ? part.vertexColors[v] : core::NifColor4{};
                        vertexData.insert(vertexData.end(), {color.r, color.g, color.b, color.a});
                    }

                    SubMesh sub;
                    sub.ambientColor = {part.material.ambient[0], part.material.ambient[1], part.material.ambient[2]};
                    sub.diffuseColor = {part.material.diffuse[0], part.material.diffuse[1], part.material.diffuse[2]};
                    sub.specularColor = {part.material.specular[0], part.material.specular[1], part.material.specular[2]};
                    sub.emissiveColor = {part.material.emissive[0], part.material.emissive[1], part.material.emissive[2]};
                    sub.glossiness = std::clamp(part.material.glossiness, 0.0f, 128.0f);
                    sub.specularEnabled = part.specularEnabled;
                    sub.textureApplyMode = part.textureApplyMode;
                    sub.vcAlphaTextureBlender = part.shaderName == "VCAlphaTextureBlender";
                    const bool hasVertexColors =
                        part.vertexColors.size() == part.positions.size();
                    if (hasVertexColors) {
                        if (!part.hasVertexColorProperty) {
                            // Classic NIF default: authored vertex colors feed ambient+diffuse.
                            sub.vertexColorMode = 2;
                        } else if (part.vertexColorMode == 0) {
                            sub.vertexColorMode = 0;
                        } else if (part.vertexColorMode == 1) {
                            sub.vertexColorMode = 1;
                        } else if (part.vertexColorMode == 2) {
                            // SRC_AMB_DIF + LIGHT_MODE_EMISSIVE disables color-material;
                            // EMI_AMB_DIF is the normal ambient+diffuse path.
                            sub.vertexColorMode = part.vertexLightingMode == 0 ? 0u : 2u;
                        } else {
                            // Unknown future enum: retain the classic safe default rather than
                            // dropping authored vertex colors entirely.
                            sub.vertexColorMode = 2;
                        }
                    }
                    sub.bumpMapLumaScale = part.bumpMapLumaScale;
                    sub.bumpMapLumaOffset = part.bumpMapLumaOffset;
                    // NIF Matrix22 is read row-major; glUniformMatrix2fv expects column-major.
                    sub.bumpMapMatrix = {part.bumpMapMatrix[0], part.bumpMapMatrix[2],
                                         part.bumpMapMatrix[1], part.bumpMapMatrix[3]};
                    // Lokales AABB-Zentrum ist eine robuste, billige Naeherung fuer die
                    // Sortiertiefe transparenter Submeshes. Es ist deutlich besser als nur
                    // die Objektposition, wenn ein NIF mehrere weit auseinanderliegende Teile hat.
                    core::NifVec3 boundsMin{
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max()};
                    core::NifVec3 boundsMax{
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest()};
                    for (const auto& pos : part.positions) {
                        boundsMin.x = std::min(boundsMin.x, pos.x); boundsMax.x = std::max(boundsMax.x, pos.x);
                        boundsMin.y = std::min(boundsMin.y, pos.y); boundsMax.y = std::max(boundsMax.y, pos.y);
                        boundsMin.z = std::min(boundsMin.z, pos.z); boundsMax.z = std::max(boundsMax.z, pos.z);
                    }
                    sub.localCenter = {
                        (boundsMin.x + boundsMax.x) * 0.5f,
                        (boundsMin.y + boundsMax.y) * 0.5f,
                        (boundsMin.z + boundsMax.z) * 0.5f};
                    sub.materialAlpha = std::clamp(part.material.alpha, 0.0f, 1.0f);
                    sub.alphaBlend = part.alphaBlend || sub.materialAlpha < 0.999f;
                    sub.alphaTest = part.alphaTest;
                    sub.alphaCutoff = static_cast<float>(part.alphaThreshold) / 255.0f;
                    sub.alphaSrcBlend = part.alphaSrcBlend;
                    sub.alphaDstBlend = part.alphaDstBlend;
                    sub.alphaTestFunc = part.alphaTestFunc;
                    sub.depthTest = part.depthTest;
                    sub.depthWrite = part.depthWrite;
                    sub.depthFunction = part.depthFunction;
                    sub.faceDrawMode = part.faceDrawMode;
                    sub.billboard = part.billboard;
                    sub.billboardMode = part.billboardMode;
                    sub.billboardPivot = {part.billboardPivot.x, part.billboardPivot.y, part.billboardPivot.z};
                    sub.billboardInverseRotation = part.billboardInverseRotation;
                    sub.lodControlled = part.lodControlled;
                    sub.lodNear = part.lodNear;
                    sub.lodFar = part.lodFar;
                    sub.lodCenter = {part.lodCenter.x, part.lodCenter.y, part.lodCenter.z};
                    std::vector<std::uint32_t> validIndices;
                    validIndices.reserve(part.triangleIndices.size());
                    for (std::size_t t = 0; t + 2 < part.triangleIndices.size(); t += 3) {
                        const auto a = part.triangleIndices[t];
                        const auto b = part.triangleIndices[t + 1];
                        const auto c = part.triangleIndices[t + 2];
                        if (a < part.positions.size() && b < part.positions.size() && c < part.positions.size()) {
                            validIndices.insert(validIndices.end(), {a, b, c});
                        }
                    }
                    if (validIndices.empty()) {
                        std::fprintf(stderr, "[NifMeshRenderer] Mesh ohne gueltige Dreiecke: %s\n", obj.modelPath.c_str());
                        continue;
                    }
                    sub.indexCount = static_cast<std::uint32_t>(validIndices.size());
                    sub.pickPositions = part.positions;
                    sub.pickIndices = validIndices;
                    sub.localBoundsMin = {boundsMin.x,boundsMin.y,boundsMin.z};
                    sub.localBoundsMax = {boundsMax.x,boundsMax.y,boundsMax.z};

                    // Alle klassischen Textur-Slots laden. Jeder Slot behaelt sein eigenes UV-Set,
                    // Clamp/Filter und seine optionale NIF-Texturtransformation.
                    auto findTexturePath = [&](const std::string& textureName) -> std::optional<std::filesystem::path> {
                        if (textureName.empty()) return std::nullopt;
                        const auto native = core::legacy::LegacyPathToNative(textureName);

                        // 1) NIF-relative reference. This is the most specific interpretation and
                        // therefore always wins when it exists.
                        if (!modelDir.empty()) {
                            if (auto local = core::legacy::ResolveCaseInsensitivePath(modelDir, native)) return local;
                            const auto stripped = core::legacy::StripResmapPrefix(native);
                            if (stripped != native) {
                                if (auto local = core::legacy::ResolveCaseInsensitivePath(modelDir, stripped)) return local;
                            }
                        }

                        // 2) Existing map/resmap resolver (map-local, shared resmap roots,
                        // case/whitespace tolerant and ambiguity-safe).
                        if (auto texPath = core::legacy::ResolveLegacyAssetPath(mapDir, textureName))
                            return texPath;

                        // 3) Explicit client-rooted paths such as resitem\..., reseffect\...,
                        // reschar\... or resmenu\.... Earlier code stopped at <Client>/resmap and
                        // therefore could not resolve a valid texture merely because the NIF lived
                        // in resmap while its material referenced a sibling Fiesta asset tree.
                        if (!clientAssetRoot.empty()) {
                            if (auto rooted = core::legacy::ResolveCaseInsensitivePath(clientAssetRoot, native))
                                return rooted;
                        }

                        // 4) Basename beside the NIF. Preserve the old whitespace/case tolerance.
                        std::string want = textureName;
                        if (const auto slash = want.find_last_of("\\/"); slash != std::string::npos)
                            want = want.substr(slash + 1);
                        const std::string wantLower = TextureLookupKey(want);
                        if (!modelDir.empty()) {
                            std::error_code fec;
                            for (const auto& entry : std::filesystem::directory_iterator(modelDir, fec)) {
                                if (fec) break;
                                if (TextureLookupKey(entry.path().filename().string()) == wantLower)
                                    return entry.path();
                            }
                        }

                        // 5) Only for a bare/stale reference: search the known Fiesta client asset
                        // trees by basename, but accept it ONLY if the result is unique across all
                        // of them. Ambiguous names remain unresolved instead of showing a plausible
                        // but wrong texture.
                        if (!clientAssetRoot.empty() && !want.empty()) {
                            if (auto unique = clientTextureIndex.FindUnique(clientAssetRoot, want))
                                return unique;
                        }
                        return std::nullopt;
                    };

                    const auto resolveTexturePath = [&](const std::string& textureName) {
                        const auto cacheKey = modelDir.string() + "\n" + textureName;
                        auto [entry, inserted] = resolvedTextures.try_emplace(cacheKey);
                        if (inserted) entry->second = findTexturePath(textureName);
                        return entry->second;
                    };

                    for (std::size_t slotIndex = 0; slotIndex < part.textureSlots.size(); ++slotIndex) {
                        const auto& src = part.textureSlots[slotIndex];
                        auto& dst = sub.textures[slotIndex];
                        dst.uvSet = std::min<std::uint32_t>(src.uvSet, 7u);
                        dst.clampMode = src.clampMode;
                        dst.filterMode = src.filterMode;
                        dst.hasTransform = src.hasTransform;
                        dst.translation = {src.translation.u, src.translation.v};
                        dst.scale = {src.scale.u, src.scale.v};
                        dst.rotation = src.rotation;
                        dst.transformType = src.transformType;
                        dst.center = {src.center.u, src.center.v};
                        if (!src.present) continue;
                        if (src.sourceUsesEmbeddedPixelData && !src.embeddedTexture) {
                            std::fprintf(stderr,
                                "[NifMeshRenderer] Eingebetteter Textur-Slot %zu ohne dekodierte PixelData #%d: %s\n",
                                slotIndex, src.sourcePixelDataRef, obj.modelPath.c_str());
                            continue;
                        }
                        if (!src.sourceUsesEmbeddedPixelData && src.texture.empty()) continue;

                        const bool hasSlotUvs = src.uvSet < part.uvSets.size() &&
                                                part.uvSets[src.uvSet].size() == part.positions.size();
                        const bool hasBaseFallbackUvs = part.uvs.size() == part.positions.size();
                        if (!hasSlotUvs && hasBaseFallbackUvs) {
                            // Some Fiesta exports reference an unavailable secondary UV set even
                            // though UV0 is valid. Dropping the complete texture made whole material
                            // layers disappear; render with UV0 as a deterministic fallback.
                            dst.uvSet = 0;
                        } else if (!hasSlotUvs) {
                            std::fprintf(stderr,
                                "[NifMeshRenderer] Textur-Slot %zu ohne brauchbares UV-Set (%u): %s\n",
                                slotIndex, src.uvSet, obj.modelPath.c_str());
                            continue;
                        }

                        if (src.sourceUsesEmbeddedPixelData) {
                            const std::string cacheKey = key + "#embedded:" + std::to_string(slotIndex) +
                                                         ":pixel:" + std::to_string(src.sourcePixelDataRef);
                            dst.texture = GetOrLoadEmbeddedTexture(*src.embeddedTexture, cacheKey);
                        } else if (auto texPath = resolveTexturePath(src.texture)) {
                            dst.texture = GetOrLoadTexture(*texPath);
                        } else {
                            std::fprintf(stderr, "[NifMeshRenderer] Objekt-Textur-Slot %zu nicht gefunden: %s\n",
                                         slotIndex, src.texture.c_str());
                        }
                    }

                    // Verified NiTextureEffect path: ENVIRONMENT_MAP + SPHERE_MAP. Other
                    // texture/coord-generation combinations remain preserved in NifModel and
                    // diagnostic-only until their exact Fiesta runtime semantics are proven.
                    for (std::size_t effectIndex = 0; effectIndex < part.textureEffects.size(); ++effectIndex) {
                        const auto& effect = part.textureEffects[effectIndex];
                        if (!effect.enabled || effect.textureType != 2u || effect.coordGenType != 2u) continue;
                        if (effect.clippingPlaneEnabled) {
                            std::fprintf(stderr,
                                "[NifMeshRenderer] Env/Sphere TextureEffect mit Clipping-Plane bleibt deaktiviert: %s\n",
                                obj.modelPath.c_str());
                            continue;
                        }
                        std::uint32_t textureId = 0;
                        if (effect.sourceUsesEmbeddedPixelData) {
                            if (effect.embeddedTexture) {
                                const std::string cacheKey = key + "#textureEffect:" +
                                    std::to_string(effectIndex) + ":pixel:" +
                                    std::to_string(effect.sourcePixelDataRef);
                                textureId = GetOrLoadEmbeddedTexture(*effect.embeddedTexture, cacheKey);
                            } else {
                                std::fprintf(stderr,
                                    "[NifMeshRenderer] TextureEffect ohne dekodierte eingebettete PixelData #%d: %s\n",
                                    effect.sourcePixelDataRef, obj.modelPath.c_str());
                            }
                        } else if (!effect.texture.empty()) {
                            if (auto texPath = resolveTexturePath(effect.texture)) {
                                textureId = GetOrLoadTexture(*texPath);
                            } else {
                                std::fprintf(stderr,
                                    "[NifMeshRenderer] TextureEffect-Textur nicht gefunden: %s (%s)\n",
                                    effect.texture.c_str(), obj.modelPath.c_str());
                            }
                        }
                        if (textureId == 0) continue;
                        if (sub.environmentSphereEffectCount >= kMaxEnvironmentSphereEffects) {
                            std::fprintf(stderr,
                                "[NifMeshRenderer] Mehr als %zu Env/Sphere-Effects an einem Mesh-Part; Rest bleibt diagnostisch: %s\n",
                                kMaxEnvironmentSphereEffects, obj.modelPath.c_str());
                            break;
                        }
                        auto& dstEffect = sub.environmentSphereEffects[sub.environmentSphereEffectCount++];
                        dstEffect.texture = textureId;
                        dstEffect.clampMode = effect.clampMode;
                        dstEffect.filterMode = effect.filterMode;
                    }

                    // Zeitabhaengige NiTextureTransformController-Spuren koennen direkt auf
                    // den statischen Slotparametern aufsetzen. NiFlipController braucht dagegen
                    // bereits aufgeloeste GL-Texturen fuer jedes Frame.
                    sub.textureTransformAnimations = part.textureTransformAnimations;
                    for (std::size_t ai = 0; ai < part.textureFlipAnimations.size(); ++ai) {
                        const auto& srcAnim = part.textureFlipAnimations[ai];
                        SubMesh::FlipAnimation dstAnim;
                        dstAnim.slot = srcAnim.slot;
                        dstAnim.track = srcAnim.track;
                        dstAnim.frameTextures.reserve(srcAnim.frames.size());
                        for (std::size_t fi = 0; fi < srcAnim.frames.size(); ++fi) {
                            const auto& frame = srcAnim.frames[fi];
                            std::uint32_t textureId = 0;
                            if (frame.sourceUsesEmbeddedPixelData) {
                                if (frame.embeddedTexture) {
                                    const std::string cacheKey = key + "#flip:" + std::to_string(ai) + ":" +
                                                                 std::to_string(fi) + ":pixel:" +
                                                                 std::to_string(frame.sourcePixelDataRef);
                                    textureId = GetOrLoadEmbeddedTexture(*frame.embeddedTexture, cacheKey);
                                } else {
                                    std::fprintf(stderr,
                                        "[NifMeshRenderer] Eingebettetes Flipbook-Frame ohne dekodierte PixelData #%d: %s\n",
                                        frame.sourcePixelDataRef, obj.modelPath.c_str());
                                }
                            } else if (!frame.texture.empty()) {
                                if (auto texPath = resolveTexturePath(frame.texture)) textureId = GetOrLoadTexture(*texPath);
                                else std::fprintf(stderr, "[NifMeshRenderer] Flipbook-Textur nicht gefunden: %s\n",
                                                  frame.texture.c_str());
                            }
                            if (textureId != 0) dstAnim.frameTextures.push_back(textureId);
                        }
                        if (!dstAnim.frameTextures.empty()) sub.textureFlipAnimations.push_back(std::move(dstAnim));
                    }

                    glGenVertexArrays(1, &sub.vao);
                    glBindVertexArray(sub.vao);

                    glGenBuffers(1, &sub.vbo);
                    glBindBuffer(GL_ARRAY_BUFFER, sub.vbo);
                    glBufferData(GL_ARRAY_BUFFER, static_cast<long>(vertexData.size() * sizeof(float)), vertexData.data(), GL_STATIC_DRAW);
                    constexpr GLsizei strideBytes = static_cast<GLsizei>(kVertexStrideFloats * sizeof(float));
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, strideBytes, reinterpret_cast<void*>(0));
                    glEnableVertexAttribArray(1);
                    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, strideBytes, reinterpret_cast<void*>(3 * sizeof(float)));
                    for (std::size_t uvSet = 0; uvSet < kGpuUvSets; ++uvSet) {
                        const GLuint location = static_cast<GLuint>(2 + uvSet);
                        glEnableVertexAttribArray(location);
                        glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, strideBytes,
                                              reinterpret_cast<void*>((6 + uvSet * 2) * sizeof(float)));
                    }
                    glEnableVertexAttribArray(10);
                    glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, strideBytes,
                                          reinterpret_cast<void*>((6 + kGpuUvSets * 2) * sizeof(float)));

                    glGenBuffers(1, &sub.ebo);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ebo);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<long>(validIndices.size() * sizeof(std::uint32_t)),
                                 validIndices.data(), GL_STATIC_DRAW);

                    glBindVertexArray(0);
                    model.subMeshes.push_back(sub);
                }
            }
            if (!nifResult) {
                std::fprintf(stderr, "[NifMeshRenderer] NIF-Laden fehlgeschlagen: %s: %s\n",
                             obj.modelPath.c_str(), nifResult.error().c_str());
            }
            it = modelCache_.emplace(key, std::move(model)).first;
        }

        if (!it->second.subMeshes.empty()) {
            perObjectModel_[i] = &it->second;
        }
    }
}

bool NifMeshRenderer::HasRealMesh(std::size_t objectIndex) const {
    return objectIndex < perObjectModel_.size() && perObjectModel_[objectIndex] != nullptr;
}

std::size_t NifMeshRenderer::RealMeshCount() const {
    std::size_t count = 0;
    for (const auto* m : perObjectModel_) {
        if (m != nullptr) ++count;
    }
    return count;
}

namespace {
std::optional<float> EvaluateFloatTrack(const core::NifFloatTrack& track, float sceneTime) {
    if (!track.active) return std::nullopt;

    float time = track.frequency * sceneTime + track.phase;
    if (!(time >= track.startTime && time <= track.stopTime)) {
        const float delta = track.stopTime - track.startTime;
        switch (track.extrapolation) {
            case 0: { // cyclic
                if (delta <= 0.0f) time = track.startTime;
                else {
                    const float x = (time - track.startTime) / delta;
                    const float y = (x - std::floor(x)) * delta;
                    time = track.startTime + y;
                }
                break;
            }
            case 1: { // reverse / ping-pong
                if (delta <= 0.0f) time = track.startTime;
                else {
                    const float x = (time - track.startTime) / delta;
                    const float y = (x - std::floor(x)) * delta;
                    const auto cycle = static_cast<long long>(std::fabs(std::floor(x)));
                    time = ((cycle & 1LL) == 0LL) ? (track.startTime + y) : (track.stopTime - y);
                }
                break;
            }
            case 2:
            default:
                time = std::clamp(time, track.startTime, track.stopTime);
                break;
        }
    }

    if (track.keys.empty()) return track.currentValue;
    if (time <= track.keys.front().time) return track.keys.front().value;
    if (time >= track.keys.back().time) return track.keys.back().value;

    auto upper = std::upper_bound(track.keys.begin(), track.keys.end(), time,
                                  [](float t, const core::NifFloatKey& k) { return t < k.time; });
    if (upper == track.keys.begin()) return upper->value;
    const auto& k2 = *upper;
    const auto& k1 = *(upper - 1);
    const float dt = k2.time - k1.time;
    const float x = dt > 1.0e-8f ? std::clamp((time - k1.time) / dt, 0.0f, 1.0f) : 0.0f;

    if (track.interpolation == 2u) {
        // NifSkope: Tangente des linken Keys = Backward, des rechten = Forward.
        const float x2 = x * x;
        const float x3 = x2 * x;
        return k1.value * (2.0f * x3 - 3.0f * x2 + 1.0f) +
               k2.value * (-2.0f * x3 + 3.0f * x2) +
               k1.backwardTangent * (x3 - 2.0f * x2 + x) +
               k2.forwardTangent * (x3 - x2);
    }
    if (track.interpolation == 5u) return x < 0.5f ? k1.value : k2.value;
    // LINEAR sowie TBC: entspricht dem aktuellen NifSkope-Rendererpfad.
    return k1.value + (k2.value - k1.value) * x;
}

Mat4 QuatToMat4Local(float x, float y, float z, float w) {
    Mat4 m = Mat4::Identity();
    m.m[0] = 1 - 2 * (y * y + z * z);
    m.m[1] = 2 * (x * y + w * z);
    m.m[2] = 2 * (x * z - w * y);
    m.m[4] = 2 * (x * y - w * z);
    m.m[5] = 1 - 2 * (x * x + z * z);
    m.m[6] = 2 * (y * z + w * x);
    m.m[8] = 2 * (x * z + w * y);
    m.m[9] = 2 * (y * z - w * x);
    m.m[10] = 1 - 2 * (x * x + y * y);
    return m;
}


Mat4 TranslationMatrix(float x, float y, float z) {
    Mat4 m = Mat4::Identity();
    m.m[12] = x; m.m[13] = y; m.m[14] = z;
    return m;
}

Mat4 UniformScaleMatrix(float scale) {
    Mat4 m = Mat4::Identity();
    m.m[0] = scale; m.m[5] = scale; m.m[10] = scale;
    return m;
}

Mat4 Mat3ToMat4(const std::array<float, 9>& r) {
    Mat4 m = Mat4::Identity();
    // r ist column-major.
    m.m[0] = r[0]; m.m[1] = r[1]; m.m[2] = r[2];
    m.m[4] = r[3]; m.m[5] = r[4]; m.m[6] = r[5];
    m.m[8] = r[6]; m.m[9] = r[7]; m.m[10] = r[8];
    return m;
}

std::array<float, 3> TransformPoint(const Mat4& m, const std::array<float, 3>& p) {
    return {
        m.m[0] * p[0] + m.m[4] * p[1] + m.m[8]  * p[2] + m.m[12],
        m.m[1] * p[0] + m.m[5] * p[1] + m.m[9]  * p[2] + m.m[13],
        m.m[2] * p[0] + m.m[6] * p[1] + m.m[10] * p[2] + m.m[14],
    };
}

std::array<float, 3> Normalize3(std::array<float, 3> v) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len < 1.0e-6f) return {0.0f, 0.0f, 1.0f};
    return {v[0] / len, v[1] / len, v[2] / len};
}

std::array<float, 3> Cross3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return {a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

float Dot3(const std::array<float,3>& a,const std::array<float,3>& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

std::array<float,3> Sub3(const std::array<float,3>& a,const std::array<float,3>& b) {
    return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};
}

bool RayTriangle(const std::array<float,3>& origin,const std::array<float,3>& dir,
                 const std::array<float,3>& a,const std::array<float,3>& b,
                 const std::array<float,3>& c,float& tOut) {
    // Möller-Trumbore, bewusst zweiseitig: viele Fiesta-NIFs rendern beide Seiten.
    const auto e1=Sub3(b,a), e2=Sub3(c,a);
    const auto p=Cross3(dir,e2);
    const float det=Dot3(e1,p);
    if(std::abs(det)<1.0e-7f) return false;
    const float inv=1.0f/det;
    const auto tv=Sub3(origin,a);
    const float u=Dot3(tv,p)*inv;
    if(u<0.0f||u>1.0f) return false;
    const auto q=Cross3(tv,e1);
    const float v=Dot3(dir,q)*inv;
    if(v<0.0f||u+v>1.0f) return false;
    const float t=Dot3(e2,q)*inv;
    if(t<0.0f) return false;
    tOut=t;
    return true;
}

bool RayWorldAabb(const std::array<float,3>& origin,const std::array<float,3>& dir,
                  const std::array<float,3>& bmin,const std::array<float,3>& bmax,
                  float maxDistance) {
    float tmin=0.0f, tmax=maxDistance;
    for(int axis=0;axis<3;++axis) {
        if(std::abs(dir[axis])<1.0e-8f) {
            if(origin[axis]<bmin[axis]||origin[axis]>bmax[axis]) return false;
            continue;
        }
        float a=(bmin[axis]-origin[axis])/dir[axis];
        float b=(bmax[axis]-origin[axis])/dir[axis];
        if(a>b) std::swap(a,b);
        tmin=std::max(tmin,a);
        tmax=std::min(tmax,b);
        if(tmin>tmax) return false;
    }
    return tmax>=0.0f;
}

Mat4 BillboardFacingRotation(const std::array<float, 3>& pivotWorld,
                             const OrbitCamera& camera, std::uint16_t mode) {
    // OrbitCamera speichert die Kamera im gespiegelten Anzeigeraum; fuer Weltkoordinaten
    // muss EyeZ daher zurueckgespiegelt werden (ViewMatrix spiegelt die Welt erst spaeter).
    const std::array<float, 3> eyeWorld{camera.EyeX(), camera.EyeY(), -camera.EyeZ()};
    std::array<float, 3> zAxis{
        eyeWorld[0] - pivotWorld[0],
        eyeWorld[1] - pivotWorld[1],
        eyeWorld[2] - pivotWorld[2],
    };

    // ROTATE_ABOUT_UP (1) behaelt die Welt-Up-Achse und dreht nur um Y. Die uebrigen
    // bekannten Modi werden wie NifSkope als voll kameraorientiert behandelt; dort wird
    // die View-Rotation des Billboard-Knotens vollstaendig auf Identitaet gesetzt.
    if (mode == 1u || mode == 4u) zAxis[1] = 0.0f;
    zAxis = Normalize3(zAxis);
    const std::array<float, 3> worldUp{0.0f, 1.0f, 0.0f};
    std::array<float, 3> xAxis = Cross3(worldUp, zAxis);
    const float xLenSq = xAxis[0] * xAxis[0] + xAxis[1] * xAxis[1] + xAxis[2] * xAxis[2];
    if (xLenSq < 1.0e-8f) xAxis = {1.0f, 0.0f, 0.0f};
    else xAxis = Normalize3(xAxis);
    std::array<float, 3> yAxis = (mode == 1u || mode == 4u) ? worldUp : Normalize3(Cross3(zAxis, xAxis));

    Mat4 m = Mat4::Identity();
    // Spalten = lokale X/Y/Z-Achse in Weltkoordinaten.
    m.m[0] = xAxis[0]; m.m[1] = xAxis[1]; m.m[2] = xAxis[2];
    m.m[4] = yAxis[0]; m.m[5] = yAxis[1]; m.m[6] = yAxis[2];
    m.m[8] = zAxis[0]; m.m[9] = zAxis[1]; m.m[10] = zAxis[2];
    return m;
}

Mat4 ApplyBillboard(const Mat4& objectModel, float objectScale,
                    const std::array<float, 3>& billboardPivot, std::uint16_t billboardMode,
                    const std::array<float, 9>& billboardInverseRotation, const OrbitCamera& camera) {
    const auto pivotWorld = TransformPoint(objectModel, billboardPivot);
    const Mat4 face = BillboardFacingRotation(pivotWorld, camera, billboardMode);
    const Mat4 undoBakedRotation = Mat3ToMat4(billboardInverseRotation);
    return TranslationMatrix(pivotWorld[0], pivotWorld[1], pivotWorld[2]) *
           face * UniformScaleMatrix(objectScale) * undoBakedRotation *
           TranslationMatrix(-billboardPivot[0], -billboardPivot[1], -billboardPivot[2]);
}
} // namespace

std::optional<float> NifMeshRenderer::RaycastObject(
    const core::ObjectPlacementSet& set, std::size_t objectIndex, const OrbitCamera& camera,
    const std::array<float,3>& rayOrigin, const std::array<float,3>& rayDirection) const {
    if(objectIndex>=perObjectModel_.size()||objectIndex>=set.Count()) return std::nullopt;
    const LoadedModel* model=perObjectModel_[objectIndex];
    if(model==nullptr) return std::nullopt;

    const auto& obj=set.At(objectIndex);
    if(!std::isfinite(obj.posX)||!std::isfinite(obj.posY)||!std::isfinite(obj.posZ)||
       !std::isfinite(obj.rotX)||!std::isfinite(obj.rotY)||!std::isfinite(obj.rotZ)||
       !std::isfinite(obj.rotW)||!std::isfinite(obj.scale)) return std::nullopt;

    Mat4 modelMat=QuatToMat4Local(obj.rotX,obj.rotY,obj.rotZ,obj.rotW);
    for(int col=0;col<3;++col) {
        modelMat.m[col*4+0]*=obj.scale;
        modelMat.m[col*4+1]*=obj.scale;
        modelMat.m[col*4+2]*=obj.scale;
    }
    modelMat.m[12]=obj.posX; modelMat.m[13]=obj.posY; modelMat.m[14]=obj.posZ;

    const Mat4 view=camera.ViewMatrix();
    float best=std::numeric_limits<float>::infinity();

    for(const auto& sub:model->subMeshes) {
        if(sub.pickPositions.empty()||sub.pickIndices.empty()) continue;
        if(sub.lodControlled) {
            const auto lodWorld=TransformPoint(modelMat,sub.lodCenter);
            const auto lodView=TransformPoint(view,lodWorld);
            const float distance=std::sqrt(lodView[0]*lodView[0]+lodView[1]*lodView[1]+lodView[2]*lodView[2]);
            if(!(sub.lodNear<=distance&&distance<sub.lodFar)) continue;
        }

        Mat4 effective=modelMat;
        if(sub.billboard)
            effective=ApplyBillboard(modelMat,obj.scale,sub.billboardPivot,sub.billboardMode,
                                     sub.billboardInverseRotation,camera);

        std::array<float,3> worldMin{
            std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
        std::array<float,3> worldMax{
            std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()};
        for(int mask=0;mask<8;++mask) {
            const std::array<float,3> local{
                (mask&1)?sub.localBoundsMax[0]:sub.localBoundsMin[0],
                (mask&2)?sub.localBoundsMax[1]:sub.localBoundsMin[1],
                (mask&4)?sub.localBoundsMax[2]:sub.localBoundsMin[2]};
            const auto p=TransformPoint(effective,local);
            for(int axis=0;axis<3;++axis) {
                worldMin[axis]=std::min(worldMin[axis],p[axis]);
                worldMax[axis]=std::max(worldMax[axis],p[axis]);
            }
        }
        if(!RayWorldAabb(rayOrigin,rayDirection,worldMin,worldMax,best)) continue;

        for(std::size_t ti=0;ti+2<sub.pickIndices.size();ti+=3) {
            const auto ia=sub.pickIndices[ti], ib=sub.pickIndices[ti+1], ic=sub.pickIndices[ti+2];
            if(ia>=sub.pickPositions.size()||ib>=sub.pickPositions.size()||ic>=sub.pickPositions.size()) continue;
            const auto toWorld=[&](const core::NifVec3& p){
                return TransformPoint(effective,{p.x,p.y,p.z});
            };
            const auto a=toWorld(sub.pickPositions[ia]);
            const auto b=toWorld(sub.pickPositions[ib]);
            const auto c=toWorld(sub.pickPositions[ic]);
            float t=0.0f;
            if(RayTriangle(rayOrigin,rayDirection,a,b,c,t)&&t<best) best=t;
        }
    }
    if(!std::isfinite(best)) return std::nullopt;
    return best;
}

void NifMeshRenderer::Draw(const core::ObjectPlacementSet& set, const OrbitCamera& camera, int width, int height,
                           const std::vector<char>* hidden) {
    if (width <= 0 || height <= 0 || perObjectModel_.empty()) return;

    // Der Renderer teilt sich den OpenGL-Kontext mit Terrain/Marker/UI. Deshalb nicht nur
    // "irgendeinen brauchbaren" Zustand am Ende hinterlassen, sondern genau die von uns
    // veraenderten Kern-States sichern und wiederherstellen.
    const GLboolean prevDepthTest = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean prevBlend = glIsEnabled(GL_BLEND);
    const GLboolean prevCull = glIsEnabled(GL_CULL_FACE);
    GLboolean prevDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
    GLint prevProgram = 0, prevVao = 0, prevActiveTexture = 0;
    std::array<GLint, 10 + kMaxEnvironmentSphereEffects> prevTextures{};
    GLint prevCullFace = GL_BACK, prevFrontFace = GL_CCW, prevDepthFunc = GL_LESS;
    GLint prevBlendSrcRgb = GL_ONE, prevBlendDstRgb = GL_ZERO;
    GLint prevBlendSrcAlpha = GL_ONE, prevBlendDstAlpha = GL_ZERO;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
    glGetIntegerv(GL_CULL_FACE_MODE, &prevCullFace);
    glGetIntegerv(GL_FRONT_FACE, &prevFrontFace);
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
    glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
    for (std::size_t unit = 0; unit < prevTextures.size(); ++unit) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTextures[unit]);
    }

    glUseProgram(shaderProgram_);
    const Mat4 view = camera.ViewMatrix();
    const Mat4 proj = OrbitCamera::PerspectiveMatrix(0.9f, static_cast<float>(width) / static_cast<float>(height), camera.NearPlane(), camera.FarPlane());
    const Mat4 viewProj = proj * view;
    static const auto animationEpoch = std::chrono::steady_clock::now();
    const float animationTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - animationEpoch).count();

    const auto& locViewProj = uniforms_.locViewProj;
    const auto& locView = uniforms_.locView;
    const auto& locModel = uniforms_.locModel;
    const auto& locLightDir = uniforms_.locLightDir;
    const auto& locCameraPos = uniforms_.locCameraPos;
    const auto& locAmbientColor = uniforms_.locAmbientColor;
    const auto& locDiffuseColor = uniforms_.locDiffuseColor;
    const auto& locSpecularColor = uniforms_.locSpecularColor;
    const auto& locEmissiveColor = uniforms_.locEmissiveColor;
    const auto& locGlossiness = uniforms_.locGlossiness;
    const auto& locSpecularEnabled = uniforms_.locSpecularEnabled;
    const auto& locApplyMode = uniforms_.locApplyMode;
    const auto& locVcAlphaTextureBlender = uniforms_.locVcAlphaTextureBlender;
    const auto& locVertexColorMode = uniforms_.locVertexColorMode;
    const auto& locBumpLumaScale = uniforms_.locBumpLumaScale;
    const auto& locBumpLumaOffset = uniforms_.locBumpLumaOffset;
    const auto& locBumpMatrix = uniforms_.locBumpMatrix;
    const auto& locAlphaTest = uniforms_.locAlphaTest;
    const auto& locAlphaCutoff = uniforms_.locAlphaCutoff;
    const auto& locAlphaTestFunc = uniforms_.locAlphaTestFunc;
    const auto& locMaterialAlpha = uniforms_.locMaterialAlpha;
    const auto& locEnvironmentSphereCount = uniforms_.locEnvironmentSphereCount;
    const auto& locEnvironmentSampler = uniforms_.locEnvironmentSampler;
    const auto& locHasTex = uniforms_.locHasTex;
    const auto& locUvSet = uniforms_.locUvSet;
    const auto& locHasTransform = uniforms_.locHasTransform;
    const auto& locTranslation = uniforms_.locTranslation;
    const auto& locScale = uniforms_.locScale;
    const auto& locRotation = uniforms_.locRotation;
    const auto& locTransformType = uniforms_.locTransformType;
    const auto& locCenter = uniforms_.locCenter;
    const auto& locSampler = uniforms_.locSampler;
    for (int slot = 0; slot < 10; ++slot) glUniform1i(locSampler[slot], slot);
    for (std::size_t effect = 0; effect < kMaxEnvironmentSphereEffects; ++effect)
        glUniform1i(locEnvironmentSampler[effect], 10 + static_cast<int>(effect));

    glUniformMatrix4fv(locViewProj, 1, GL_FALSE, viewProj.m);
    glUniformMatrix4fv(locView, 1, GL_FALSE, view.m);
    glUniform3f(locLightDir, -0.4f, -1.0f, -0.3f);
    glUniform3f(locCameraPos, camera.EyeX(), camera.EyeY(), -camera.EyeZ());
    glEnable(GL_DEPTH_TEST);

    auto& opaqueItems = opaqueItems_;
    auto& blendedItems = blendedItems_;
    opaqueItems.clear();
    blendedItems.clear();

    std::size_t estimatedItems = 0;
    for (const auto* model : perObjectModel_) if (model != nullptr) estimatedItems += model->subMeshes.size();
    opaqueItems.reserve(estimatedItems);
    blendedItems.reserve(estimatedItems / 4 + 1);

    for (std::size_t i = 0; i < perObjectModel_.size() && i < set.Count(); ++i) {
        const LoadedModel* model = perObjectModel_[i];
        if (model == nullptr) continue;
        if (hidden != nullptr && i < hidden->size() && (*hidden)[i] != 0) continue;

        const auto& obj = set.At(i);
        // Invalid source transforms stay in the document for lossless export, but
        // must not propagate NaNs/Infs into matrices or transparency sorting.
        if (!std::isfinite(obj.posX) || !std::isfinite(obj.posY) || !std::isfinite(obj.posZ) ||
            !std::isfinite(obj.rotX) || !std::isfinite(obj.rotY) || !std::isfinite(obj.rotZ) ||
            !std::isfinite(obj.rotW) || !std::isfinite(obj.scale)) continue;
        Mat4 modelMat = QuatToMat4Local(obj.rotX, obj.rotY, obj.rotZ, obj.rotW);
        for (int col = 0; col < 3; ++col) {
            modelMat.m[col * 4 + 0] *= obj.scale;
            modelMat.m[col * 4 + 1] *= obj.scale;
            modelMat.m[col * 4 + 2] *= obj.scale;
        }
        modelMat.m[12] = obj.posX;
        modelMat.m[13] = obj.posY;
        modelMat.m[14] = obj.posZ;

        for (const auto& sub : model->subMeshes) {
            // NifSkope wertet NiLODNode ueber die Entfernung des LOD-Centers im Viewraum aus:
            // near <= distance < far. Die Bereiche bleiben dadurch auch bei mehreren platzierten
            // Instanzen desselben NIF unabhaengig und kameraabhaengig.
            if (sub.lodControlled) {
                const auto lodWorld = TransformPoint(modelMat, sub.lodCenter);
                const auto lodView = TransformPoint(view, lodWorld);
                const float distance = std::sqrt(lodView[0] * lodView[0] + lodView[1] * lodView[1] + lodView[2] * lodView[2]);
                if (!(sub.lodNear <= distance && distance < sub.lodFar)) continue;
            }

            Mat4 effectiveModel = modelMat;
            if (sub.billboard) {
                effectiveModel = ApplyBillboard(modelMat, obj.scale, sub.billboardPivot, sub.billboardMode,
                                                sub.billboardInverseRotation, camera);
            }

            const auto worldCenter = TransformPoint(effectiveModel, sub.localCenter);
            // OpenGL-Viewraum schaut entlang -Z. camera.ViewMatrix() enthaelt bereits die
            // Editor/DirectX-Z-Spiegelung, daher liefert -viewZ direkt die richtige Sortiertiefe.
            const float viewZ = view.m[2] * worldCenter[0] + view.m[6] * worldCenter[1] +
                                view.m[10] * worldCenter[2] + view.m[14];
            DrawItem item{&sub, effectiveModel, -viewZ};
            (sub.alphaBlend ? blendedItems : opaqueItems).push_back(item);
        }
    }

    // Alpha-Blending ist reihenfolgeabhaengig: weit entfernte Flaechen zuerst. Opaque und
    // Alpha-Test-Geometrie bleibt absichtlich unsortiert und schreibt normal in den Depthbuffer.
    std::stable_sort(blendedItems.begin(), blendedItems.end(), [](const DrawItem& a, const DrawItem& b) {
        return a.depth > b.depth;
    });

    const auto blendFactor = [](std::uint8_t f) -> GLenum {
        switch (f) {
            case 0: return GL_ONE;
            case 1: return GL_ZERO;
            case 2: return GL_SRC_COLOR;
            case 3: return GL_ONE_MINUS_SRC_COLOR;
            case 4: return GL_DST_COLOR;
            case 5: return GL_ONE_MINUS_DST_COLOR;
            case 6: return GL_SRC_ALPHA;
            case 7: return GL_ONE_MINUS_SRC_ALPHA;
            case 8: return GL_DST_ALPHA;
            case 9: return GL_ONE_MINUS_DST_ALPHA;
            case 10: return GL_SRC_ALPHA_SATURATE;
            default: return GL_SRC_ALPHA;
        }
    };

    const auto depthFunction = [](std::uint32_t f) -> GLenum {
        switch (f) {
            case 0: return GL_ALWAYS;   // ZCOMP_ALWAYS
            case 1: return GL_LESS;     // ZCOMP_LESS
            case 2: return GL_EQUAL;    // ZCOMP_EQUAL
            case 3: return GL_LEQUAL;   // ZCOMP_LESS_EQUAL
            case 4: return GL_GREATER;  // ZCOMP_GREATER
            case 5: return GL_NOTEQUAL; // ZCOMP_NOT_EQUAL
            case 6: return GL_GEQUAL;   // ZCOMP_GREATER_EQUAL
            case 7: return GL_NEVER;    // ZCOMP_NEVER
            default: return GL_LEQUAL;
        }
    };

    const auto applyFaceDrawMode = [](std::uint32_t mode) {
        // FaceDrawMode laut NIF-Spezifikation:
        // 0 DRAW_CCW_OR_BOTH (anwendungsabhaengig), 1 DRAW_CCW, 2 DRAW_CW, 3 DRAW_BOTH.
        // Modus 0 und unbekannte Werte bleiben doppelseitig: fuer einen Editor ist "anzeigen"
        // sicherer als Geometrie wegen einer Spiel-/Renderer-Voreinstellung zu verlieren.
        if (mode == 1u) {
            glEnable(GL_CULL_FACE);
            glFrontFace(GL_CCW);
            glCullFace(GL_BACK);
        } else if (mode == 2u) {
            glEnable(GL_CULL_FACE);
            glFrontFace(GL_CW);
            glCullFace(GL_BACK);
        } else {
            glDisable(GL_CULL_FACE);
        }
    };

    const auto applyTextureSampling = [](const auto& tex) {
        if (tex.texture == 0) return;
        const GLint wrapS = (tex.clampMode == 0u || tex.clampMode == 1u) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
        const GLint wrapT = (tex.clampMode == 0u || tex.clampMode == 2u) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);

        GLint mag = GL_LINEAR;
        GLint min = GL_LINEAR_MIPMAP_LINEAR;
        switch (tex.filterMode) {
            case 0: mag = GL_NEAREST; min = GL_NEAREST; break;
            case 1: mag = GL_LINEAR;  min = GL_LINEAR; break;
            case 2: mag = GL_LINEAR;  min = GL_LINEAR_MIPMAP_LINEAR; break;
            case 3: mag = GL_NEAREST; min = GL_NEAREST_MIPMAP_NEAREST; break;
            case 4: mag = GL_NEAREST; min = GL_NEAREST_MIPMAP_LINEAR; break;
            case 5: mag = GL_LINEAR;  min = GL_LINEAR_MIPMAP_NEAREST; break;
            case 6: mag = GL_LINEAR;  min = GL_LINEAR_MIPMAP_LINEAR; break;
            default: break;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    };

    const auto drawItem = [&](const DrawItem& item, bool blendedPass) {
        const SubMesh& sub = *item.sub;
        glUniformMatrix4fv(locModel, 1, GL_FALSE, item.model.m);
        applyFaceDrawMode(sub.faceDrawMode);
        if (sub.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(sub.depthWrite ? GL_TRUE : GL_FALSE);
        glDepthFunc(depthFunction(sub.depthFunction));
        if (blendedPass) glBlendFunc(blendFactor(sub.alphaSrcBlend), blendFactor(sub.alphaDstBlend));

        glUniform1i(locAlphaTest, sub.alphaTest ? 1 : 0);
        glUniform1f(locAlphaCutoff, sub.alphaCutoff);
        glUniform1i(locAlphaTestFunc, static_cast<int>(sub.alphaTestFunc));
        glUniform1f(locMaterialAlpha, sub.materialAlpha);
        glUniform3f(locAmbientColor, sub.ambientColor[0], sub.ambientColor[1], sub.ambientColor[2]);
        glUniform3f(locDiffuseColor, sub.diffuseColor[0], sub.diffuseColor[1], sub.diffuseColor[2]);
        glUniform3f(locSpecularColor, sub.specularColor[0], sub.specularColor[1], sub.specularColor[2]);
        glUniform3f(locEmissiveColor, sub.emissiveColor[0], sub.emissiveColor[1], sub.emissiveColor[2]);
        glUniform1f(locGlossiness, sub.glossiness);
        glUniform1i(locSpecularEnabled, sub.specularEnabled ? 1 : 0);
        glUniform1i(locApplyMode, static_cast<int>(sub.textureApplyMode));
        glUniform1i(locVcAlphaTextureBlender, sub.vcAlphaTextureBlender ? 1 : 0);
        glUniform1i(locVertexColorMode, static_cast<int>(sub.vertexColorMode));
        glUniform1f(locBumpLumaScale, sub.bumpMapLumaScale);
        glUniform1f(locBumpLumaOffset, sub.bumpMapLumaOffset);
        glUniformMatrix2fv(locBumpMatrix, 1, GL_FALSE, sub.bumpMapMatrix.data());

        std::array<TextureBinding, 10> animatedTextures = sub.textures;
        for (const auto& anim : sub.textureTransformAnimations) {
            if (anim.slot >= animatedTextures.size()) continue;
            const auto value = EvaluateFloatTrack(anim.track, animationTime);
            if (!value) continue;
            auto& tex = animatedTextures[anim.slot];
            switch (anim.operation) {
                case 0: tex.translation[0] = *value; break;
                case 1: tex.translation[1] = *value; break;
                case 2: tex.rotation = *value; break;
                case 3: tex.scale[0] = *value; break;
                case 4: tex.scale[1] = *value; break;
                default: break;
            }
        }
        for (const auto& anim : sub.textureFlipAnimations) {
            if (anim.slot >= animatedTextures.size() || anim.frameTextures.empty()) continue;
            const auto value = EvaluateFloatTrack(anim.track, animationTime);
            if (!value) continue;
            long long frame = static_cast<long long>(*value); // NifSkope castet ebenfalls auf int
            frame = std::clamp<long long>(frame, 0, static_cast<long long>(anim.frameTextures.size() - 1));
            animatedTextures[anim.slot].texture = anim.frameTextures[static_cast<std::size_t>(frame)];
        }

        for (int slot = 0; slot < 10; ++slot) {
            const auto& tex = animatedTextures[static_cast<std::size_t>(slot)];
            const bool present = tex.texture != 0;
            glUniform1i(locHasTex[slot], present ? 1 : 0);
            glUniform1i(locUvSet[slot], static_cast<int>(std::min<std::uint32_t>(tex.uvSet, 7u)));
            glUniform1i(locHasTransform[slot], tex.hasTransform ? 1 : 0);
            glUniform2f(locTranslation[slot], tex.translation[0], tex.translation[1]);
            glUniform2f(locScale[slot], tex.scale[0], tex.scale[1]);
            glUniform1f(locRotation[slot], tex.rotation);
            glUniform1i(locTransformType[slot], static_cast<int>(tex.transformType));
            glUniform2f(locCenter[slot], tex.center[0], tex.center[1]);
            glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + slot));
            glBindTexture(GL_TEXTURE_2D, present ? tex.texture : 0);
            if (present) applyTextureSampling(tex);
        }

        const auto environmentCount =
            std::min(sub.environmentSphereEffectCount, kMaxEnvironmentSphereEffects);
        glUniform1i(locEnvironmentSphereCount, static_cast<int>(environmentCount));
        for (std::size_t effect = 0; effect < kMaxEnvironmentSphereEffects; ++effect) {
            const auto& env = sub.environmentSphereEffects[effect];
            const bool present = effect < environmentCount && env.texture != 0;
            glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + 10 + effect));
            glBindTexture(GL_TEXTURE_2D, present ? env.texture : 0);
            if (present) applyTextureSampling(env);
        }
        glBindVertexArray(sub.vao);
        glDrawElements(GL_TRIANGLES, static_cast<int>(sub.indexCount), GL_UNSIGNED_INT, nullptr);
    };

    // Pass 1: Opaque + Alpha-Test. Depth-Test/Write/Funktion werden pro Submesh aus
    // NiZBufferProperty angewandt; ohne Property gelten die konservativen Standardwerte.
    glDisable(GL_BLEND);
    for (const auto& item : opaqueItems) drawItem(item, false);

    // Pass 2: echte Transparenz back-to-front. Auch hier gewinnt der explizite NIF-Z-State:
    // manche Fiesta-Effekte sind absichtlich read-only, andere schreiben trotz Blending.
    if (!blendedItems.empty()) {
        glEnable(GL_BLEND);
        for (const auto& item : blendedItems) drawItem(item, true);
    }

    // Exakten vorgefundenen GL-Zustand wiederherstellen.
    glBindVertexArray(static_cast<GLuint>(prevVao));
    for (std::size_t unit = 0; unit < prevTextures.size(); ++unit) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTextures[unit]));
    }
    glActiveTexture(static_cast<GLenum>(prevActiveTexture));
    glUseProgram(static_cast<GLuint>(prevProgram));
    glDepthMask(prevDepthMask);
    glDepthFunc(static_cast<GLenum>(prevDepthFunc));
    glBlendFuncSeparate(static_cast<GLenum>(prevBlendSrcRgb), static_cast<GLenum>(prevBlendDstRgb),
                        static_cast<GLenum>(prevBlendSrcAlpha), static_cast<GLenum>(prevBlendDstAlpha));
    glCullFace(static_cast<GLenum>(prevCullFace));
    glFrontFace(static_cast<GLenum>(prevFrontFace));
    if (prevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (prevBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (prevCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

} // namespace theseed::mapeditor::app
