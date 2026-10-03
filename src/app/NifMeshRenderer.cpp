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
uniform int uViewMode; // 0 beleuchtet, 1 unbeleuchtet, 2 nur Licht, 3 Normalen, 4 Vertexfarbe, 5 UV0, 6 Alpha, 7 LOD
uniform vec3 uLodTint;  // LOD-Ansicht: Farbe des gerade gezeigten NiLODNode-Bereichs
uniform bool uSceneLightFromMap; // SHMD GlobalLight/DirectionLight* vorhanden
uniform vec3 uSceneAmbient;
uniform vec3 uSunColor;
uniform bool uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
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
uniform bool uAlphaTextureBlender11;
uniform bool uAlphaTextureBlender;
uniform bool uPgTerrain;
uniform int uVertexColorMode;
uniform bool uParticleMode;
uniform vec4 uParticleColor;
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
    vec4 vertexColor = uParticleMode ? uParticleColor : vColor;

    // Classic NiVertexColorProperty follows OpenGL color-material semantics.
    // SRC_AMB_DIF replaces authored ambient+diffuse with the per-vertex color;
    // SRC_EMISSIVE replaces authored emission. The dedicated VCAlpha shader keeps
    // its original path below because its RGB/alpha inputs have different semantics.
    vec3 materialAmbient = uAmbientColor;
    vec3 materialDiffuse = uDiffuseColor;
    vec3 materialEmission = uEmissiveColor;
    if (!uVcAlphaTextureBlender) {
        if (uVertexColorMode == 2) {
            materialAmbient = vertexColor.rgb;
            materialDiffuse = vertexColor.rgb;
        } else if (uVertexColorMode == 1) {
            materialEmission = vertexColor.rgb;
        }
    }

    vec3 surface = materialDiffuse;
    if (uVcAlphaTextureBlender && uHasTex[0] && uHasTex[1] && uHasTex[2]) {
        // Original Gamebryo VCAlphaTextureBlender-P.hlsl:
        // Texture1/Texture2 are blended by vertex alpha, then multiplied by Detail*2.
        vec3 texture1 = texture(uTex0, slotUv(0)).rgb;
        vec3 texture2 = texture(uTex1, slotUv(1)).rgb;
        vec3 blended = mix(texture2, texture1, clamp(vertexColor.a, 0.0, 1.0));
        vec3 detail = texture(uTex2, slotUv(2)).rgb * 2.0;
        surface = uDiffuseColor * vertexColor.rgb * blended * detail;
    } else if (uAlphaTextureBlender11 && uHasTex[0] && uHasTex[1] && uHasTex[2]) {
        // Fiesta AlphaTextureBlender11 corpus contract:
        // maps 0/1 are the two color layers; map 2 is a dedicated alpha mask.
        // Real BFGate assets have opaque color layers and either no vertex colors or
        // constant vertex alpha=1, so the authored varying blend signal is map2.a.
        vec3 texture1 = texture(uTex0, slotUv(0)).rgb;
        vec3 texture2 = texture(uTex1, slotUv(1)).rgb;
        float blendMask = clamp(texture(uTex2, slotUv(2)).a, 0.0, 1.0);
        surface = materialDiffuse * mix(texture1, texture2, blendMask);
    } else if (uAlphaTextureBlender && uHasTex[0] && uHasTex[1] && uHasTex[2]) {
        // Stock Gamebryo 2.6 AlphaTextureBlender.psh:
        // r0=t0; lrp r0,t2.aaaa,t1,r0 -> (1-mask)*Texture1 + mask*Texture2.
        vec4 texture1 = texture(uTex0, slotUv(0));
        vec4 texture2 = texture(uTex1, slotUv(1));
        float blendMask = clamp(texture(uTex2, slotUv(2)).a, 0.0, 1.0);
        surface = mix(texture1.rgb, texture2.rgb, blendMask);
    } else if (uPgTerrain && uHasTex[0]) {
        // Fiesta PgTerrain corpus contract: shader map 0 is the visible color map.
        // Map 1 is a grayscale/alpha coverage map; it is intentionally not multiplied
        // into RGB. NiAlphaProperty decides whether that coverage is blended/tested.
        surface = materialDiffuse * texture(uTex0, slotUv(0)).rgb;
    } else {
        if (uHasTex[0]) {
            if (uApplyMode == 0) surface = base.rgb;                         // APPLY_REPLACE
            else if (uApplyMode == 1) surface = mix(surface, base.rgb, base.a); // APPLY_DECAL
            else surface *= base.rgb;                                       // APPLY_MODULATE / unknown future fallback
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
    if (uAlphaTextureBlender && uHasTex[0] && uHasTex[1] && uHasTex[2]) {
        vec4 texture1 = texture(uTex0, slotUv(0));
        vec4 texture2 = texture(uTex1, slotUv(1));
        float blendMask = clamp(texture(uTex2, slotUv(2)).a, 0.0, 1.0);
        alpha *= mix(texture1.a, texture2.a, blendMask);
    } else if (uPgTerrain && uHasTex[1]) {
        // PgTerrain's authored second map is coverage. Preserve the material alpha too:
        // several real Fiesta water/terrain parts intentionally set it below 1.
        alpha *= texture(uTex1, slotUv(1)).a;
    } else if (!uVcAlphaTextureBlender && uHasTex[0] && uApplyMode != 1) {
        alpha *= base.a;
    }
    if (uParticleMode) alpha *= vertexColor.a;
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
    if (uSceneLightFromMap) {
        // Kartendaten (SHMD): Material-Ambient * Umgebungslicht + Sonne * N·L, wie bei fester
        // Funktionsbeleuchtung auf 1 gesättigt (Materialdiffus steckt bereits in surface).
        vec3 lightSum = clamp(materialAmbient * uSceneAmbient + uSunColor * ndl, 0.0, 1.0);
        ambient = vec3(0.0);
        diffuse = surface * lightSum;
    }
    vec3 specular = uSpecularColor * specPower * glossMask;
    vec3 emissive = materialEmission + glow;
    // NIF TextureType::TEX_ENVIRONMENT_MAP is additive to the ordinary textured,
    // lit/decal result. It does not replace or multiply the base material.
    vec3 environment = uEnvironmentSphereCount > 0
        ? environmentSphereColor(environmentSphereUv(n))
        : vec3(0.0);
    if (uViewMode == 1) {
        FragColor = vec4(surface, alpha);
    } else if (uViewMode == 2) {
        FragColor = vec4(materialAmbient * 0.28 + vec3(0.22 + 0.78 * ndl) + specular, alpha);
    } else if (uViewMode == 3) {
        FragColor = vec4(n * 0.5 + 0.5, alpha);
    } else if (uViewMode == 4) {
        FragColor = vec4(vertexColor.rgb, 1.0);                 // Vertexfarben der NIF
    } else if (uViewMode == 5) {
        FragColor = vec4(fract(vUv0), 0.0, 1.0);                // UV0 (Rot = U, Grün = V)
    } else if (uViewMode == 6) {
        FragColor = vec4(vec3(alpha), 1.0);                     // effektives Alpha
    } else if (uViewMode == 7) {
        FragColor = vec4(uLodTint * (0.45 + 0.55 * ndl), 1.0);  // LOD-Bereich
    } else if (uAlphaTextureBlender) {
        FragColor = vec4(surface, alpha);
    } else {
        FragColor = vec4(ambient + diffuse + specular + emissive + environment, alpha);
    }
    if (uFogEnabled && uViewMode == 0 && !uParticleMode) {
        float d = length(vWorldPos - uCameraPos);
        float f = clamp((d - uFogStart) / max(uFogEnd - uFogStart, 1.0), 0.0, 1.0);
        FragColor.rgb = mix(FragColor.rgb, uFogColor, f);
    }
}
)";

const char* kGlassVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uCameraPos;

out vec3 gNormal;
out vec3 gViewVec;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gNormal = mat3(uModel) * aNormal;
    gViewVec = uCameraPos - worldPos.xyz;
    gl_Position = uViewProj * worldPos;
}
)";

const char* kGlassFragmentShaderSrc = R"(
#version 330 core
in vec3 gNormal;
in vec3 gViewVec;
out vec4 FragColor;

uniform samplerCube uGlassEnvironment;
uniform sampler2D uGlassRainbow;
uniform vec4 uGlassBaseColor;
uniform float uGlassRefractionScale;
uniform float uGlassReflectionScale;
uniform float uGlassIorRatio;
uniform float uGlassAmbient;
uniform float uGlassRainbowSpread;
uniform float uGlassRainbowScale;
uniform bool uGlassAlphaTest;
uniform float uGlassAlphaCutoff;
uniform int uGlassAlphaTestFunc;

void main() {
    vec3 normal = normalize(gNormal);
    vec3 viewVec = normalize(gViewVec);

    // Gamebryo's stock Glass.hlsl swizzles .xzy before cube lookup because authored
    // Gamebryo space is right-handed while Direct3D cube lookup is left-handed.
    // NifModel already performs that same x/z-y remap when importing positions/normals,
    // so these editor-space vectors are already in the lookup space expected by the
    // legacy DDS cube map. Do not swizzle a second time here.
    vec3 reflVec = reflect(-viewVec, normal);
    vec4 reflection = texture(uGlassEnvironment, reflVec);

    float cosine = clamp(dot(viewVec, normal), -1.0, 1.0);
    float sine = sqrt(max(0.0, 1.0 - cosine * cosine));
    float sine2 = clamp(uGlassIorRatio * sine, 0.0, 1.0);
    float cosine2 = sqrt(max(0.0, 1.0 - sine2 * sine2));

    vec3 tangent = cross(cross(viewVec, normal), normal);
    float tangentLen2 = dot(tangent, tangent);
    vec3 y = tangentLen2 > 1.0e-12
        ? tangent * inversesqrt(tangentLen2)
        : vec3(0.0);
    vec3 refrVec = -normal * cosine2 + y * sine2;
    vec4 refraction = texture(uGlassEnvironment, refrVec);

    float rainbowU = uGlassRainbowSpread == 0.0
        ? 1.0
        : pow(max(cosine, 0.0), uGlassRainbowSpread);
    vec4 rainbow = texture(uGlassRainbow, vec2(clamp(rainbowU, 0.0, 1.0), 0.5));

    vec4 rain = uGlassRainbowScale * rainbow * uGlassBaseColor;
    vec4 refl = uGlassReflectionScale * reflection;
    vec4 refr = uGlassRefractionScale * refraction * uGlassBaseColor;

    // Exact stock Gamebryo 2.6 ComplexGlassPS composition.
    vec4 result = sine * refl + (1.0 - sine2) * refr + sine2 * rain + vec4(uGlassAmbient);
    if (uGlassAlphaTest) {
        bool passAlpha = true;
        if (uGlassAlphaTestFunc == 0) passAlpha = false;
        else if (uGlassAlphaTestFunc == 1) passAlpha = result.a < uGlassAlphaCutoff;
        else if (uGlassAlphaTestFunc == 2) passAlpha = abs(result.a - uGlassAlphaCutoff) < (1.0 / 255.0);
        else if (uGlassAlphaTestFunc == 3) passAlpha = result.a <= uGlassAlphaCutoff;
        else if (uGlassAlphaTestFunc == 4) passAlpha = result.a > uGlassAlphaCutoff;
        else if (uGlassAlphaTestFunc == 5) passAlpha = abs(result.a - uGlassAlphaCutoff) >= (1.0 / 255.0);
        else if (uGlassAlphaTestFunc == 6) passAlpha = result.a >= uGlassAlphaCutoff;
        else if (uGlassAlphaTestFunc == 7) passAlpha = true;
        if (!passAlpha) discard;
    }
    FragColor = result;
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
    if (shaderProgram_ || glassShaderProgram_) Shutdown();
    const std::uint32_t vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    const std::uint32_t fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    shaderProgram_ = LinkProgram(vs, fs);

    const std::uint32_t glassVs = CompileShader(GL_VERTEX_SHADER, kGlassVertexShaderSrc);
    const std::uint32_t glassFs = CompileShader(GL_FRAGMENT_SHADER, kGlassFragmentShaderSrc);
    glassShaderProgram_ = LinkProgram(glassVs, glassFs);
    uniforms_.locViewProj = glGetUniformLocation(shaderProgram_, "uViewProj");
    uniforms_.locView = glGetUniformLocation(shaderProgram_, "uView");
    uniforms_.locModel = glGetUniformLocation(shaderProgram_, "uModel");
    uniforms_.locLightDir = glGetUniformLocation(shaderProgram_, "uLightDir");
    uniforms_.locViewMode = glGetUniformLocation(shaderProgram_, "uViewMode");
    uniforms_.locLodTint = glGetUniformLocation(shaderProgram_, "uLodTint");
    uniforms_.locSceneLightFromMap = glGetUniformLocation(shaderProgram_, "uSceneLightFromMap");
    uniforms_.locSceneAmbient = glGetUniformLocation(shaderProgram_, "uSceneAmbient");
    uniforms_.locSunColor = glGetUniformLocation(shaderProgram_, "uSunColor");
    uniforms_.locFogEnabled = glGetUniformLocation(shaderProgram_, "uFogEnabled");
    uniforms_.locFogColor = glGetUniformLocation(shaderProgram_, "uFogColor");
    uniforms_.locFogStart = glGetUniformLocation(shaderProgram_, "uFogStart");
    uniforms_.locFogEnd = glGetUniformLocation(shaderProgram_, "uFogEnd");
    uniforms_.locCameraPos = glGetUniformLocation(shaderProgram_, "uCameraPos");
    uniforms_.locAmbientColor = glGetUniformLocation(shaderProgram_, "uAmbientColor");
    uniforms_.locDiffuseColor = glGetUniformLocation(shaderProgram_, "uDiffuseColor");
    uniforms_.locSpecularColor = glGetUniformLocation(shaderProgram_, "uSpecularColor");
    uniforms_.locEmissiveColor = glGetUniformLocation(shaderProgram_, "uEmissiveColor");
    uniforms_.locGlossiness = glGetUniformLocation(shaderProgram_, "uGlossiness");
    uniforms_.locSpecularEnabled = glGetUniformLocation(shaderProgram_, "uSpecularEnabled");
    uniforms_.locApplyMode = glGetUniformLocation(shaderProgram_, "uApplyMode");
    uniforms_.locVcAlphaTextureBlender = glGetUniformLocation(shaderProgram_, "uVcAlphaTextureBlender");
    uniforms_.locAlphaTextureBlender11 = glGetUniformLocation(shaderProgram_, "uAlphaTextureBlender11");
    uniforms_.locAlphaTextureBlender = glGetUniformLocation(shaderProgram_, "uAlphaTextureBlender");
    uniforms_.locPgTerrain = glGetUniformLocation(shaderProgram_, "uPgTerrain");
    uniforms_.locVertexColorMode = glGetUniformLocation(shaderProgram_, "uVertexColorMode");
    uniforms_.locBumpLumaScale = glGetUniformLocation(shaderProgram_, "uBumpLumaScale");
    uniforms_.locBumpLumaOffset = glGetUniformLocation(shaderProgram_, "uBumpLumaOffset");
    uniforms_.locBumpMatrix = glGetUniformLocation(shaderProgram_, "uBumpMatrix");
    uniforms_.locAlphaTest = glGetUniformLocation(shaderProgram_, "uAlphaTest");
    uniforms_.locAlphaCutoff = glGetUniformLocation(shaderProgram_, "uAlphaCutoff");
    uniforms_.locAlphaTestFunc = glGetUniformLocation(shaderProgram_, "uAlphaTestFunc");
    uniforms_.locMaterialAlpha = glGetUniformLocation(shaderProgram_, "uMaterialAlpha");
    uniforms_.locEnvironmentSphereCount = glGetUniformLocation(shaderProgram_, "uEnvironmentSphereCount");
    uniforms_.locParticleMode = glGetUniformLocation(shaderProgram_, "uParticleMode");
    uniforms_.locParticleColor = glGetUniformLocation(shaderProgram_, "uParticleColor");

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

    glassUniforms_.locViewProj = glGetUniformLocation(glassShaderProgram_, "uViewProj");
    glassUniforms_.locModel = glGetUniformLocation(glassShaderProgram_, "uModel");
    glassUniforms_.locCameraPos = glGetUniformLocation(glassShaderProgram_, "uCameraPos");
    glassUniforms_.locEnvironment = glGetUniformLocation(glassShaderProgram_, "uGlassEnvironment");
    glassUniforms_.locRainbow = glGetUniformLocation(glassShaderProgram_, "uGlassRainbow");
    glassUniforms_.locBaseColor = glGetUniformLocation(glassShaderProgram_, "uGlassBaseColor");
    glassUniforms_.locRefractionScale = glGetUniformLocation(glassShaderProgram_, "uGlassRefractionScale");
    glassUniforms_.locReflectionScale = glGetUniformLocation(glassShaderProgram_, "uGlassReflectionScale");
    glassUniforms_.locIorRatio = glGetUniformLocation(glassShaderProgram_, "uGlassIorRatio");
    glassUniforms_.locAmbient = glGetUniformLocation(glassShaderProgram_, "uGlassAmbient");
    glassUniforms_.locRainbowSpread = glGetUniformLocation(glassShaderProgram_, "uGlassRainbowSpread");
    glassUniforms_.locRainbowScale = glGetUniformLocation(glassShaderProgram_, "uGlassRainbowScale");
    glassUniforms_.locAlphaTest = glGetUniformLocation(glassShaderProgram_, "uGlassAlphaTest");
    glassUniforms_.locAlphaCutoff = glGetUniformLocation(glassShaderProgram_, "uGlassAlphaCutoff");
    glassUniforms_.locAlphaTestFunc = glGetUniformLocation(glassShaderProgram_, "uGlassAlphaTestFunc");

    // NifSkope's classic particle renderer draws a camera-space +/-size quad with the same
    // UVs for every texture stage. Keep one immutable unit quad and vary only the model/color.
    constexpr std::size_t kGpuUvSets = 8;
    constexpr std::size_t kStrideFloats = 6 + kGpuUvSets * 2 + 4;
    std::vector<float> particleVertices;
    particleVertices.reserve(4 * kStrideFloats);
    const auto appendParticleVertex = [&](float x, float y, float u, float v) {
        particleVertices.insert(particleVertices.end(), {x, y, 0.0f, 0.0f, 0.0f, -1.0f});
        for (std::size_t set = 0; set < kGpuUvSets; ++set)
            particleVertices.insert(particleVertices.end(), {u, v});
        particleVertices.insert(particleVertices.end(), {1.0f, 1.0f, 1.0f, 1.0f});
    };
    appendParticleVertex(+1.0f, +1.0f, 1.0f, 1.0f);
    appendParticleVertex(-1.0f, +1.0f, 0.0f, 1.0f);
    appendParticleVertex(+1.0f, -1.0f, 1.0f, 0.0f);
    appendParticleVertex(-1.0f, -1.0f, 0.0f, 0.0f);
    constexpr std::array<std::uint32_t, 6> particleIndices{0, 1, 2, 2, 1, 3};

    glGenVertexArrays(1, &particleVao_);
    glBindVertexArray(particleVao_);
    glGenBuffers(1, &particleVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<long>(particleVertices.size() * sizeof(float)),
                 particleVertices.data(), GL_STATIC_DRAW);
    constexpr GLsizei strideBytes = static_cast<GLsizei>(kStrideFloats * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, strideBytes, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, strideBytes, reinterpret_cast<void*>(3 * sizeof(float)));
    for (std::size_t set = 0; set < kGpuUvSets; ++set) {
        const GLuint location = static_cast<GLuint>(2 + set);
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, strideBytes,
                              reinterpret_cast<void*>((6 + set * 2) * sizeof(float)));
    }
    glEnableVertexAttribArray(10);
    glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, strideBytes,
                          reinterpret_cast<void*>((6 + kGpuUvSets * 2) * sizeof(float)));
    glGenBuffers(1, &particleEbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, particleEbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<long>(particleIndices.size() * sizeof(std::uint32_t)),
                 particleIndices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
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
    for (auto& [path, tex] : cubeTextureCache_) {
        if (tex) glDeleteTextures(1, &tex);
    }
    cubeTextureCache_.clear();
    perObjectModel_.clear();
    perObjectParticleRuntime_.clear();
    opaqueItems_.clear();
    blendedItems_.clear();
    if (particleEbo_) glDeleteBuffers(1, &particleEbo_);
    if (particleVbo_) glDeleteBuffers(1, &particleVbo_);
    if (particleVao_) glDeleteVertexArrays(1, &particleVao_);
    particleEbo_ = particleVbo_ = particleVao_ = 0;
    if (shaderProgram_) glDeleteProgram(shaderProgram_);
    if (glassShaderProgram_) glDeleteProgram(glassShaderProgram_);
    shaderProgram_ = 0;
    glassShaderProgram_ = 0;
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
    else if (lowerExt == ".bmp") imageResult = core::LoadBmpImage(resolvedPath);
#ifdef _WIN32
    else if (lowerExt == ".jpg" || lowerExt == ".jpeg" || lowerExt == ".png")
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

std::uint32_t NifMeshRenderer::GetOrLoadCubeTexture(
    const std::filesystem::path& resolvedPath) {
    const std::string key = resolvedPath.string();
    if (const auto it = cubeTextureCache_.find(key); it != cubeTextureCache_.end())
        return it->second;

    std::uint32_t tex = 0;
    const auto cube = core::LoadDdsCubeImage(resolvedPath);
    if (!cube) {
        std::fprintf(stderr, "[NifMeshRenderer] Cube-Map nicht ladbar (%s): %s\n",
                     resolvedPath.string().c_str(), cube.error().c_str());
        cubeTextureCache_.emplace(key, 0);
        return 0;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (std::size_t face = 0; face < cube->faces.size(); ++face) {
        const auto& image = cube->faces[face];
        if (image.width != cube->width || image.height != cube->height ||
            image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4u) {
            std::fprintf(stderr, "[NifMeshRenderer] Ungueltige Cube-Map-Flaeche %zu: %s\n",
                         face, resolvedPath.string().c_str());
            glDeleteTextures(1, &tex);
            cubeTextureCache_.emplace(key, 0);
            return 0;
        }
        glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face),
                     0, GL_RGBA8, static_cast<GLsizei>(image.width),
                     static_cast<GLsizei>(image.height), 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    }
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    cubeTextureCache_.emplace(key, tex);
    return tex;
}

std::uint32_t NifMeshRenderer::GetOrLoadEmbeddedCubeTexture(
    const core::NifEmbeddedTexture& image, const std::string& cacheKey) {
    if (const auto it = cubeTextureCache_.find(cacheKey); it != cubeTextureCache_.end())
        return it->second;

    std::uint32_t tex = 0;
    const std::size_t faceBytes =
        static_cast<std::size_t>(image.width) * image.height * 4u;
    if (image.faces != 6u || image.width == 0 || image.height == 0) {
        std::fprintf(stderr,
            "[NifMeshRenderer] Eingebettete Cube-Map braucht exakt 6 Faces: %s (faces=%u)\n",
            cacheKey.c_str(), image.faces);
        cubeTextureCache_.emplace(cacheKey, 0);
        return 0;
    }
    for (std::size_t face = 0; face < image.cubeFaceRgba.size(); ++face) {
        if (image.cubeFaceRgba[face].size() != faceBytes) {
            std::fprintf(stderr,
                "[NifMeshRenderer] Eingebettete Cube-Map-Flaeche %zu unvollstaendig: %s\n",
                face, cacheKey.c_str());
            cubeTextureCache_.emplace(cacheKey, 0);
            return 0;
        }
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (std::size_t face = 0; face < image.cubeFaceRgba.size(); ++face) {
        glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face),
                     0, GL_RGBA8, static_cast<GLsizei>(image.width),
                     static_cast<GLsizei>(image.height), 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, image.cubeFaceRgba[face].data());
    }
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    cubeTextureCache_.emplace(cacheKey, tex);
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
    perObjectParticleRuntime_.assign(set.Count(), {});
    std::unordered_map<std::string, std::optional<std::filesystem::path>> resolvedModels;
    std::unordered_map<std::string, std::optional<std::filesystem::path>> resolvedTextures;
    std::unordered_map<std::string, core::NifSiblingEmbeddedTextureResolution>
        siblingEmbeddedTextures;
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
                const auto findParticleTexturePath = [&](const std::string& textureName)
                    -> std::optional<std::filesystem::path> {
                    if (textureName.empty()) return std::nullopt;
                    const auto native = core::legacy::LegacyPathToNative(textureName);
                    if (!modelDir.empty()) {
                        if (auto local = core::legacy::ResolveCaseInsensitivePath(modelDir, native)) return local;
                        const auto stripped = core::legacy::StripResmapPrefix(native);
                        if (stripped != native) {
                            if (auto local = core::legacy::ResolveCaseInsensitivePath(modelDir, stripped)) return local;
                        }
                    }
                    if (auto texPath = core::legacy::ResolveLegacyAssetPath(mapDir, textureName)) return texPath;
                    if (!clientAssetRoot.empty()) {
                        if (auto rooted = core::legacy::ResolveCaseInsensitivePath(clientAssetRoot, native)) return rooted;
                    }
                    std::string want = textureName;
                    if (const auto slash = want.find_last_of("\\/"); slash != std::string::npos)
                        want = want.substr(slash + 1);
                    const std::string wantLower = TextureLookupKey(want);
                    if (!modelDir.empty()) {
                        std::error_code fec;
                        for (const auto& entry : std::filesystem::directory_iterator(modelDir, fec)) {
                            if (fec) break;
                            if (TextureLookupKey(entry.path().filename().string()) == wantLower) return entry.path();
                        }
                    }
                    if (!clientAssetRoot.empty() && !want.empty())
                        return clientTextureIndex.FindUnique(clientAssetRoot, want);
                    return std::nullopt;
                };
                const auto resolveParticleTexturePath = [&](const std::string& textureName) {
                    const auto cacheKey = std::string("particle:") + modelDir.string() + "\n" + textureName;
                    auto [entry, inserted] = resolvedTextures.try_emplace(cacheKey);
                    if (inserted) entry->second = findParticleTexturePath(textureName);
                    return entry->second;
                };
                const auto resolveSiblingEmbeddedTexture =
                    [&](const std::string& textureName,
                        const core::NifMeshPart* geometryHint = nullptr,
                        std::size_t geometryHintIndex = std::numeric_limits<std::size_t>::max())
                    -> std::shared_ptr<const core::NifEmbeddedTexture> {
                    if (!resolved || textureName.empty()) return {};
                    std::string cacheKey =
                        std::string("sibling:") + resolved->string() + "\n" + textureName;
                    if (geometryHint != nullptr)
                        cacheKey += "\npart:" + std::to_string(geometryHintIndex);
                    auto [entry, inserted] =
                        siblingEmbeddedTextures.try_emplace(cacheKey);
                    if (inserted) {
                        entry->second =
                            core::ResolveSiblingEmbeddedTexture(*resolved, textureName, geometryHint);
                        if (entry->second.texture) {
                            std::fprintf(stderr,
                                "[NifMeshRenderer] Fiesta sibling-embedded fallback: %s -> %s (%s)\n",
                                textureName.c_str(),
                                entry->second.matchedTextureName.c_str(),
                                entry->second.sourceNif.string().c_str());
                        } else if (entry->second.ambiguous) {
                            std::fprintf(stderr,
                                "[NifMeshRenderer] Mehrdeutiger sibling-embedded Fallback: %s (%s)\n",
                                textureName.c_str(), resolved->string().c_str());
                        }
                    }
                    return entry->second.texture;
                };

                model.subMeshes.reserve(nifResult->parts.size());
                std::vector<std::size_t> corePartToSubMesh(
                    nifResult->parts.size(), std::numeric_limits<std::size_t>::max());
                std::unordered_set<std::size_t> meshParticleTemplateParts;
                for (const auto& particleSystem : nifResult->particleSystems) {
                    for (const auto& master : particleSystem.meshParticleMasters) {
                        meshParticleTemplateParts.insert(
                            master.partIndices.begin(), master.partIndices.end());
                    }
                }
                for (std::size_t corePartIndex = 0;
                     corePartIndex < nifResult->parts.size(); ++corePartIndex) {
                    const auto& part = nifResult->parts[corePartIndex];
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
                    sub.alphaTextureBlender11 = part.shaderName == "AlphaTextureBlender11";
                    sub.alphaTextureBlender = part.shaderName == "AlphaTextureBlender";
                    sub.pgTerrain = part.shaderName == "PgTerrain";
                    sub.glass = part.shaderName == "Glass";
                    if (sub.glass && part.glassShader)
                        sub.glassParameters = *part.glassShader;
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
                    if (sub.glass) {
                        // Stock Gamebryo Glass.NSF explicitly overrides these two render states.
                        sub.alphaBlend = true;
                        sub.alphaSrcBlend = 6; // SRC_ALPHA
                        sub.alphaDstBlend = 7; // INV_SRC_ALPHA
                    }
                    sub.depthTest = part.depthTest;
                    sub.depthWrite = part.depthWrite;
                    sub.depthFunction = part.depthFunction;
                    sub.stencilEnabled = part.stencilEnabled;
                    sub.stencilFunction = part.stencilFunction;
                    sub.stencilReference = part.stencilReference;
                    sub.stencilMask = part.stencilMask;
                    sub.stencilFailAction = part.stencilFailAction;
                    sub.stencilZFailAction = part.stencilZFailAction;
                    sub.stencilPassAction = part.stencilPassAction;
                    sub.faceDrawMode = part.faceDrawMode;
                    sub.billboard = part.billboard;
                    sub.billboardMode = part.billboardMode;
                    sub.billboardPivot = {part.billboardPivot.x, part.billboardPivot.y, part.billboardPivot.z};
                    sub.billboardInverseRotation = part.billboardInverseRotation;
                    sub.meshParticleTemplate =
                        meshParticleTemplateParts.contains(corePartIndex);
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

                        // Glass.NSF packs only position+normal. EnvMap is sampled as a cube
                        // direction and RainbowMap from the view/normal angle, so neither authored
                        // shader texture consumes mesh UVs.
                        if (sub.glass && slotIndex == 0u && src.sourceIsCubeMap) {
                            if (src.sourceUsesEmbeddedPixelData) {
                                if (src.embeddedTexture) {
                                    const std::string cubeKey =
                                        key + "#glass-cube:pixel:" +
                                        std::to_string(src.sourcePixelDataRef);
                                    sub.glassEnvironmentCube =
                                        GetOrLoadEmbeddedCubeTexture(*src.embeddedTexture, cubeKey);
                                } else {
                                    std::fprintf(stderr,
                                        "[NifMeshRenderer] Glass-Cube-Map ohne dekodierte PixelData #%d: %s\n",
                                        src.sourcePixelDataRef, obj.modelPath.c_str());
                                }
                            } else {
                                if (auto texPath = resolveTexturePath(src.texture))
                                    sub.glassEnvironmentCube = GetOrLoadCubeTexture(*texPath);
                                if (sub.glassEnvironmentCube == 0) {
                                    if (auto sibling = resolveSiblingEmbeddedTexture(src.texture, &part, corePartIndex);
                                        sibling && sibling->faces == 6u) {
                                        const std::string cubeKey =
                                            key + "#sibling-glass-cube:" + src.texture;
                                        sub.glassEnvironmentCube =
                                            GetOrLoadEmbeddedCubeTexture(*sibling, cubeKey);
                                    }
                                }
                                if (sub.glassEnvironmentCube == 0) {
                                    std::fprintf(stderr,
                                        "[NifMeshRenderer] Glass-Cube-Map nicht gefunden/dekodierbar: %s (%s)\n",
                                        src.texture.c_str(), obj.modelPath.c_str());
                                }
                            }
                            continue;
                        }

                        const bool glassAngleTexture = sub.glass && slotIndex == 1u;
                        const bool hasSlotUvs = src.uvSet < part.uvSets.size() &&
                                                part.uvSets[src.uvSet].size() == part.positions.size();
                        const bool hasBaseFallbackUvs = part.uvs.size() == part.positions.size();
                        if (glassAngleTexture) {
                            // RainbowMap is sampled as a 1D lookup encoded in a 2D texture.
                            dst.uvSet = 0;
                        } else if (!hasSlotUvs && hasBaseFallbackUvs) {
                            // Some Fiesta exports reference an unavailable secondary UV set even
                            // though UV0 is valid. Dropping the complete texture made whole material
                            // layers disappear; render with UV0 as a deterministic fallback.
                            dst.uvSet = 0;
                        } else if (!hasSlotUvs && src.uvSet == 0u &&
                                   part.uvSets.empty() && part.uvs.empty()) {
                            // A handful of authored Fiesta meshes bind a base texture while the
                            // geometry carries no texture-coordinate array at all. The vertex buffer
                            // is already initialized with (0,0) for absent UV attributes, matching
                            // the constant default coordinate used by the legacy pipeline rather
                            // than dropping the authored texture stage completely.
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
                        } else {
                            if (auto texPath = resolveTexturePath(src.texture))
                                dst.texture = GetOrLoadTexture(*texPath);
                            if (dst.texture == 0) {
                                if (auto sibling =
                                        resolveSiblingEmbeddedTexture(src.texture, &part, corePartIndex);
                                    sibling && sibling->faces == 1u) {
                                    const std::string fallbackKey =
                                        key + "#sibling-slot:" + std::to_string(slotIndex) + ":" + src.texture;
                                    dst.texture = GetOrLoadEmbeddedTexture(*sibling, fallbackKey);
                                }
                            }
                            if (dst.texture == 0) {
                                std::fprintf(stderr, "[NifMeshRenderer] Objekt-Textur-Slot %zu nicht gefunden/dekodierbar: %s\n",
                                             slotIndex, src.texture.c_str());
                            }
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
                            if (auto texPath = resolveTexturePath(effect.texture))
                                textureId = GetOrLoadTexture(*texPath);
                            if (textureId == 0) {
                                if (auto sibling = resolveSiblingEmbeddedTexture(effect.texture, &part, corePartIndex);
                                    sibling && sibling->faces == 1u) {
                                    const std::string fallbackKey =
                                        key + "#sibling-effect:" + std::to_string(effectIndex) +
                                        ":" + effect.texture;
                                    textureId = GetOrLoadEmbeddedTexture(*sibling, fallbackKey);
                                }
                            }
                            if (textureId == 0) {
                                std::fprintf(stderr,
                                    "[NifMeshRenderer] TextureEffect-Textur nicht gefunden/dekodierbar: %s (%s)\n",
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
                                if (auto texPath = resolveTexturePath(frame.texture))
                                    textureId = GetOrLoadTexture(*texPath);
                                if (textureId == 0) {
                                    if (auto sibling = resolveSiblingEmbeddedTexture(frame.texture, &part, corePartIndex);
                                        sibling && sibling->faces == 1u) {
                                        const std::string fallbackKey =
                                            key + "#sibling-flip:" + std::to_string(ai) + ":" +
                                            std::to_string(fi) + ":" + frame.texture;
                                        textureId = GetOrLoadEmbeddedTexture(*sibling, fallbackKey);
                                    }
                                }
                                if (textureId == 0)
                                    std::fprintf(stderr, "[NifMeshRenderer] Flipbook-Textur nicht gefunden/dekodierbar: %s\n",
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
                    corePartToSubMesh[corePartIndex] = model.subMeshes.size();
                    model.subMeshes.push_back(sub);
                }

                model.particleSystems.reserve(nifResult->particleSystems.size());
                for (std::size_t systemIndex = 0; systemIndex < nifResult->particleSystems.size(); ++systemIndex) {
                    const auto& srcSystem = nifResult->particleSystems[systemIndex];
                    if (!srcSystem.hasParticleData || srcSystem.particleData.particles.empty()) continue;

                    ParticleSystem dstSystem;
                    dstSystem.particles = srcSystem.particleData.particles;
                    dstSystem.activeCount = std::min<std::uint16_t>(
                        srcSystem.particleData.activeCount,
                        static_cast<std::uint16_t>(std::min<std::size_t>(dstSystem.particles.size(), 65535u)));
                    dstSystem.meshParticles = srcSystem.meshParticles;
                    dstSystem.worldSpace = srcSystem.worldSpace;
                    dstSystem.hasColors = srcSystem.particleData.hasColors;
                    dstSystem.hasRadii = srcSystem.particleData.hasRadii;
                    dstSystem.hasSizes = srcSystem.particleData.hasSizes;
                    dstSystem.hasRotationAngles = srcSystem.particleData.hasRotationAngles;
                    dstSystem.hasRotationAxes = srcSystem.particleData.hasRotationAxes;
                    dstSystem.capacity = srcSystem.particleData.capacity;
                    dstSystem.modifiers = srcSystem.modifiers;
                    dstSystem.controllers = srcSystem.controllers;
                    dstSystem.sceneTransform = srcSystem.sceneTransform;
                    dstSystem.meshMasters.reserve(srcSystem.meshParticleMasters.size());
                    for (const auto& sourceMaster : srcSystem.meshParticleMasters) {
                        ParticleSystem::MeshMaster master;
                        master.inverseSceneTransform = sourceMaster.inverseSceneTransform;
                        for (const auto partIndex : sourceMaster.partIndices) {
                            if (partIndex >= corePartToSubMesh.size()) continue;
                            const auto subMeshIndex = corePartToSubMesh[partIndex];
                            if (subMeshIndex != std::numeric_limits<std::size_t>::max())
                                master.subMeshIndices.push_back(subMeshIndex);
                        }
                        dstSystem.meshMasters.push_back(std::move(master));
                    }

                    auto& sub = dstSystem.material;
                    sub.vao = particleVao_;
                    sub.indexCount = 6;
                    sub.ambientColor = {srcSystem.material.ambient[0], srcSystem.material.ambient[1], srcSystem.material.ambient[2]};
                    sub.diffuseColor = {srcSystem.material.diffuse[0], srcSystem.material.diffuse[1], srcSystem.material.diffuse[2]};
                    sub.specularColor = {srcSystem.material.specular[0], srcSystem.material.specular[1], srcSystem.material.specular[2]};
                    sub.emissiveColor = {srcSystem.material.emissive[0], srcSystem.material.emissive[1], srcSystem.material.emissive[2]};
                    sub.glossiness = std::clamp(srcSystem.material.glossiness, 0.0f, 128.0f);
                    sub.specularEnabled = srcSystem.specularEnabled;
                    sub.textureApplyMode = srcSystem.textureApplyMode;
                    sub.vcAlphaTextureBlender = srcSystem.shaderName == "VCAlphaTextureBlender";
                    sub.alphaTextureBlender11 = srcSystem.shaderName == "AlphaTextureBlender11";
                    sub.alphaTextureBlender = srcSystem.shaderName == "AlphaTextureBlender";
                    sub.pgTerrain = srcSystem.shaderName == "PgTerrain";
                    if (dstSystem.hasColors) {
                        if (!srcSystem.hasVertexColorProperty) sub.vertexColorMode = 2;
                        else if (srcSystem.vertexColorMode == 0u) sub.vertexColorMode = 0;
                        else if (srcSystem.vertexColorMode == 1u) sub.vertexColorMode = 1;
                        else if (srcSystem.vertexColorMode == 2u)
                            sub.vertexColorMode = srcSystem.vertexLightingMode == 0u ? 0u : 2u;
                        else sub.vertexColorMode = 2;
                    }
                    sub.bumpMapLumaScale = srcSystem.bumpMapLumaScale;
                    sub.bumpMapLumaOffset = srcSystem.bumpMapLumaOffset;
                    sub.bumpMapMatrix = {srcSystem.bumpMapMatrix[0], srcSystem.bumpMapMatrix[2],
                                         srcSystem.bumpMapMatrix[1], srcSystem.bumpMapMatrix[3]};
                    sub.materialAlpha = std::clamp(srcSystem.material.alpha, 0.0f, 1.0f);
                    sub.alphaBlend = srcSystem.alphaBlend || sub.materialAlpha < 0.999f;
                    sub.alphaTest = srcSystem.alphaTest;
                    sub.alphaCutoff = static_cast<float>(srcSystem.alphaThreshold) / 255.0f;
                    sub.alphaSrcBlend = srcSystem.alphaSrcBlend;
                    sub.alphaDstBlend = srcSystem.alphaDstBlend;
                    sub.alphaTestFunc = srcSystem.alphaTestFunc;
                    sub.depthTest = srcSystem.depthTest;
                    sub.depthWrite = srcSystem.depthWrite;
                    sub.depthFunction = srcSystem.depthFunction;
                    sub.stencilEnabled = srcSystem.stencilEnabled;
                    sub.stencilFunction = srcSystem.stencilFunction;
                    sub.stencilReference = srcSystem.stencilReference;
                    sub.stencilMask = srcSystem.stencilMask;
                    sub.stencilFailAction = srcSystem.stencilFailAction;
                    sub.stencilZFailAction = srcSystem.stencilZFailAction;
                    sub.stencilPassAction = srcSystem.stencilPassAction;
                    sub.faceDrawMode = srcSystem.faceDrawMode;
                    sub.textureTransformAnimations = srcSystem.textureTransformAnimations;

                    for (std::size_t slotIndex = 0; slotIndex < srcSystem.textureSlots.size(); ++slotIndex) {
                        const auto& src = srcSystem.textureSlots[slotIndex];
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
                        if (src.sourceUsesEmbeddedPixelData) {
                            if (src.embeddedTexture) {
                                const std::string cacheKey = key + "#particle:" + std::to_string(systemIndex) +
                                                             ":slot:" + std::to_string(slotIndex) + ":pixel:" +
                                                             std::to_string(src.sourcePixelDataRef);
                                dst.texture = GetOrLoadEmbeddedTexture(*src.embeddedTexture, cacheKey);
                            }
                        } else if (!src.texture.empty()) {
                            if (auto texPath = resolveParticleTexturePath(src.texture))
                                dst.texture = GetOrLoadTexture(*texPath);
                            if (dst.texture == 0) {
                                if (auto sibling = resolveSiblingEmbeddedTexture(src.texture);
                                    sibling && sibling->faces == 1u) {
                                    const std::string fallbackKey =
                                        key + "#sibling-particle:" + std::to_string(systemIndex) +
                                        ":slot:" + std::to_string(slotIndex) + ":" + src.texture;
                                    dst.texture = GetOrLoadEmbeddedTexture(*sibling, fallbackKey);
                                }
                            }
                            if (dst.texture == 0)
                                std::fprintf(stderr, "[NifMeshRenderer] Particle-Textur-Slot %zu nicht gefunden/dekodierbar: %s\n",
                                             slotIndex, src.texture.c_str());
                        }
                    }

                    for (std::size_t ai = 0; ai < srcSystem.textureFlipAnimations.size(); ++ai) {
                        const auto& srcAnim = srcSystem.textureFlipAnimations[ai];
                        SubMesh::FlipAnimation dstAnim;
                        dstAnim.slot = srcAnim.slot;
                        dstAnim.track = srcAnim.track;
                        dstAnim.frameTextures.reserve(srcAnim.frames.size());
                        for (std::size_t fi = 0; fi < srcAnim.frames.size(); ++fi) {
                            const auto& frame = srcAnim.frames[fi];
                            std::uint32_t textureId = 0;
                            if (frame.sourceUsesEmbeddedPixelData) {
                                if (frame.embeddedTexture) {
                                    const std::string cacheKey = key + "#particleFlip:" + std::to_string(systemIndex) +
                                                                 ":" + std::to_string(ai) + ":" + std::to_string(fi) +
                                                                 ":pixel:" + std::to_string(frame.sourcePixelDataRef);
                                    textureId = GetOrLoadEmbeddedTexture(*frame.embeddedTexture, cacheKey);
                                }
                            } else if (!frame.texture.empty()) {
                                if (auto texPath = resolveParticleTexturePath(frame.texture))
                                    textureId = GetOrLoadTexture(*texPath);
                                if (textureId == 0) {
                                    if (auto sibling = resolveSiblingEmbeddedTexture(frame.texture);
                                        sibling && sibling->faces == 1u) {
                                        const std::string fallbackKey =
                                            key + "#sibling-particle-flip:" + std::to_string(systemIndex) +
                                            ":" + std::to_string(ai) + ":" + std::to_string(fi) +
                                            ":" + frame.texture;
                                        textureId = GetOrLoadEmbeddedTexture(*sibling, fallbackKey);
                                    }
                                }
                            }
                            if (textureId != 0) dstAnim.frameTextures.push_back(textureId);
                        }
                        if (!dstAnim.frameTextures.empty()) sub.textureFlipAnimations.push_back(std::move(dstAnim));
                    }

                    model.particleSystems.push_back(std::move(dstSystem));
                }
            }
            if (!nifResult) {
                std::fprintf(stderr, "[NifMeshRenderer] NIF-Laden fehlgeschlagen: %s: %s\n",
                             obj.modelPath.c_str(), nifResult.error().c_str());
            }
            it = modelCache_.emplace(key, std::move(model)).first;
        }

        if (!it->second.subMeshes.empty() || !it->second.particleSystems.empty()) {
            perObjectModel_[i] = &it->second;
            auto& runtimeSystems = perObjectParticleRuntime_[i];
            runtimeSystems.resize(it->second.particleSystems.size());
            for (std::size_t systemIndex = 0; systemIndex < it->second.particleSystems.size(); ++systemIndex) {
                const auto& source = it->second.particleSystems[systemIndex];
                auto& runtime = runtimeSystems[systemIndex];
                runtime.particles = source.particles;
                runtime.activeCount = source.activeCount;
                runtime.emitterAccumulators.assign(source.modifiers.size(), 0.0f);
                runtime.emitterRandomStates.resize(source.modifiers.size());
                runtime.forceRandomState = 0x51f15e5du ^
                    static_cast<std::uint32_t>((i + 1u) * 0x9e3779b9u) ^
                    static_cast<std::uint32_t>((systemIndex + 1u) * 0x85ebca6bu);
                if (runtime.forceRandomState == 0u) runtime.forceRandomState = 1u;
                for (std::size_t modifierIndex = 0; modifierIndex < source.modifiers.size(); ++modifierIndex) {
                    std::uint32_t seed = 0x9e3779b9u;
                    seed ^= static_cast<std::uint32_t>((i + 1u) * 0x85ebca6bu);
                    seed ^= static_cast<std::uint32_t>((systemIndex + 1u) * 0xc2b2ae35u);
                    seed ^= static_cast<std::uint32_t>((modifierIndex + 1u) * 0x27d4eb2du);
                    runtime.emitterRandomStates[modifierIndex] = seed != 0u ? seed : 1u;
                }
            }
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

std::optional<bool> EvaluateBoolTrack(const core::NifBoolTrack& track, float sceneTime) {
    if (!track.active) return std::nullopt;

    float time = track.frequency * sceneTime + track.phase;
    if (!(time >= track.startTime && time <= track.stopTime)) {
        const float delta = track.stopTime - track.startTime;
        switch (track.extrapolation) {
            case 0: {
                if (delta <= 0.0f) time = track.startTime;
                else {
                    const float x = (time - track.startTime) / delta;
                    time = track.startTime + (x - std::floor(x)) * delta;
                }
                break;
            }
            case 1: {
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
    const auto upper = std::upper_bound(
        track.keys.begin(), track.keys.end(), time,
        [](float t, const core::NifBoolKey& key) { return t < key.time; });
    if (upper == track.keys.begin()) return upper->value;
    // Fiesta emitter-active data uses step/discrete bool keys. At a key time the new key wins.
    return (upper - 1)->value;
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

Mat4 ParticleScreenRotation(float angle) {
    // NiPSFacingQuadGeneratorKernel: positive particle angles rotate clockwise
    // around the screen-facing normal. OpenGL's local +Z rotation is CCW, so negate.
    const float s = std::sin(-angle);
    const float co = std::cos(-angle);
    Mat4 m = Mat4::Identity();
    m.m[0] = co;
    m.m[1] = s;
    m.m[4] = -s;
    m.m[5] = co;
    return m;
}

Mat4 ParticleAxisAngleRotation(const core::NifVec3& axis, float angle) {
    const float length = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (!(length > 1.0e-8f) || !std::isfinite(angle)) return Mat4::Identity();
    const float x = axis.x / length, y = axis.y / length, z = axis.z / length;
    const float co = std::cos(angle), s = std::sin(angle), t = 1.0f - co;
    Mat4 m = Mat4::Identity();
    m.m[0] = t*x*x + co;   m.m[4] = t*x*y - s*z; m.m[8]  = t*x*z + s*y;
    m.m[1] = t*x*y + s*z; m.m[5] = t*y*y + co;   m.m[9]  = t*y*z - s*x;
    m.m[2] = t*x*z - s*y; m.m[6] = t*y*z + s*x; m.m[10] = t*z*z + co;
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

Mat4 NifTransformToEditorMat4(const core::NifTransform& t) {
    // NIF rotations/translations are stored in legacy (x,y,z); editor space is (x,z,y).
    // Re = P*R*P where P swaps legacy Y/Z, then convert row-major Re to GL column-major.
    const auto& r = t.rotation;
    const std::array<float, 9> e{
        r[0], r[2], r[1],
        r[6], r[8], r[7],
        r[3], r[5], r[4],
    };
    Mat4 m = Mat4::Identity();
    m.m[0] = e[0] * t.scale; m.m[1] = e[3] * t.scale; m.m[2] = e[6] * t.scale;
    m.m[4] = e[1] * t.scale; m.m[5] = e[4] * t.scale; m.m[6] = e[7] * t.scale;
    m.m[8] = e[2] * t.scale; m.m[9] = e[5] * t.scale; m.m[10] = e[8] * t.scale;
    m.m[12] = t.translation.x;
    m.m[13] = t.translation.z;
    m.m[14] = t.translation.y;
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
    if (const auto hit = RaycastObjectDetailed(set, objectIndex, camera, rayOrigin, rayDirection)) return hit->distance;
    return std::nullopt;
}

std::optional<NifMeshRenderer::RayHit> NifMeshRenderer::RaycastObjectDetailed(
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
    std::array<std::array<float,3>,3> bestTriangle{};

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
            if(RayTriangle(rayOrigin,rayDirection,a,b,c,t)&&t<best) { best=t; bestTriangle={a,b,c}; }
        }
    }
    if(!std::isfinite(best)) return std::nullopt;
    RayHit hit;
    hit.distance=best;
    for(int axis=0;axis<3;++axis) hit.point[static_cast<std::size_t>(axis)]=rayOrigin[static_cast<std::size_t>(axis)]+rayDirection[static_cast<std::size_t>(axis)]*best;
    // Nächstgelegene Ecke des getroffenen Dreiecks (Vertex-Snap wie in Unreal).
    float bestD=std::numeric_limits<float>::infinity();
    for(const auto& v:bestTriangle) {
        const float dx=v[0]-hit.point[0], dy=v[1]-hit.point[1], dz=v[2]-hit.point[2];
        const float d=dx*dx+dy*dy+dz*dz;
        if(d<bestD) { bestD=d; hit.nearestVertex=v; }
    }
    return hit;
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
    const GLboolean prevStencilTest = glIsEnabled(GL_STENCIL_TEST);
    GLboolean prevDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
    GLint prevProgram = 0, prevVao = 0, prevActiveTexture = 0;
    std::array<GLint, 10 + kMaxEnvironmentSphereEffects> prevTextures{};
    std::array<GLint, 10 + kMaxEnvironmentSphereEffects> prevCubeTextures{};
    GLint prevCullFace = GL_BACK, prevFrontFace = GL_CCW, prevDepthFunc = GL_LESS;
    GLint prevBlendSrcRgb = GL_ONE, prevBlendDstRgb = GL_ZERO;
    GLint prevBlendSrcAlpha = GL_ONE, prevBlendDstAlpha = GL_ZERO;
    GLint prevStencilFunc = GL_ALWAYS, prevStencilRef = 0, prevStencilValueMask = -1;
    GLint prevStencilWriteMask = -1, prevStencilFail = GL_KEEP;
    GLint prevStencilZFail = GL_KEEP, prevStencilZPass = GL_KEEP;
    GLint prevStencilBackFunc = GL_ALWAYS, prevStencilBackRef = 0, prevStencilBackValueMask = -1;
    GLint prevStencilBackWriteMask = -1, prevStencilBackFail = GL_KEEP;
    GLint prevStencilBackZFail = GL_KEEP, prevStencilBackZPass = GL_KEEP;
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
    glGetIntegerv(GL_STENCIL_FUNC, &prevStencilFunc);
    glGetIntegerv(GL_STENCIL_REF, &prevStencilRef);
    glGetIntegerv(GL_STENCIL_VALUE_MASK, &prevStencilValueMask);
    glGetIntegerv(GL_STENCIL_WRITEMASK, &prevStencilWriteMask);
    glGetIntegerv(GL_STENCIL_FAIL, &prevStencilFail);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &prevStencilZFail);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &prevStencilZPass);
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &prevStencilBackFunc);
    glGetIntegerv(GL_STENCIL_BACK_REF, &prevStencilBackRef);
    glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &prevStencilBackValueMask);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &prevStencilBackWriteMask);
    glGetIntegerv(GL_STENCIL_BACK_FAIL, &prevStencilBackFail);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &prevStencilBackZFail);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &prevStencilBackZPass);
    for (std::size_t unit = 0; unit < prevTextures.size(); ++unit) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTextures[unit]);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &prevCubeTextures[unit]);
    }

    glUseProgram(shaderProgram_);
    const Mat4 view = camera.ViewMatrix();
    const Mat4 proj = camera.ProjectionMatrix(static_cast<float>(width) / static_cast<float>(height));
    const Mat4 viewProj = proj * view;
    static const auto animationEpoch = std::chrono::steady_clock::now();
    const float wallClockAnimationTime =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - animationEpoch).count();
    const float animationTime =
        animationTimeOverride_.value_or(wallClockAnimationTime);

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
    const auto& locAlphaTextureBlender11 = uniforms_.locAlphaTextureBlender11;
    const auto& locAlphaTextureBlender = uniforms_.locAlphaTextureBlender;
    const auto& locPgTerrain = uniforms_.locPgTerrain;
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
    const auto& locParticleMode = uniforms_.locParticleMode;
    const auto& locParticleColor = uniforms_.locParticleColor;
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
    glUniform3fv(locLightDir, 1, lighting_.lightDir);
    glUniform1i(uniforms_.locViewMode, viewMode_);
    glUniform1i(uniforms_.locSceneLightFromMap, lighting_.fromMapData ? 1 : 0);
    glUniform3fv(uniforms_.locSceneAmbient, 1, lighting_.ambient);
    glUniform3fv(uniforms_.locSunColor, 1, lighting_.sun);
    glUniform1i(uniforms_.locFogEnabled, lighting_.fogEnabled ? 1 : 0);
    glUniform3fv(uniforms_.locFogColor, 1, lighting_.fogColor);
    glUniform1f(uniforms_.locFogStart, lighting_.fogStart);
    glUniform1f(uniforms_.locFogEnd, lighting_.fogEnd);
    glUniform3f(locCameraPos, camera.EyeX(), camera.EyeY(), -camera.EyeZ());
    glEnable(GL_DEPTH_TEST);

    auto& opaqueItems = opaqueItems_;
    auto& blendedItems = blendedItems_;
    opaqueItems.clear();
    blendedItems.clear();

    std::size_t estimatedItems = 0;
    for (std::size_t objectIndex = 0; objectIndex < perObjectModel_.size(); ++objectIndex) {
        const auto* model = perObjectModel_[objectIndex];
        if (model == nullptr) continue;
        estimatedItems += model->subMeshes.size();
        if (objectIndex < perObjectParticleRuntime_.size()) {
            const auto& runtimeSystems = perObjectParticleRuntime_[objectIndex];
            for (std::size_t systemIndex = 0;
                 systemIndex < model->particleSystems.size() && systemIndex < runtimeSystems.size();
                 ++systemIndex) {
                if (!model->particleSystems[systemIndex].meshParticles)
                    estimatedItems += runtimeSystems[systemIndex].activeCount;
            }
        }
    }
    opaqueItems.reserve(estimatedItems);
    blendedItems.reserve(estimatedItems / 4 + 1);

    for (std::size_t i = 0; i < perObjectModel_.size() && i < set.Count(); ++i) {
        const LoadedModel* model = perObjectModel_[i];
        if (model == nullptr) continue;

        auto* runtimeSystems = i < perObjectParticleRuntime_.size()
            ? &perObjectParticleRuntime_[i] : nullptr;
        if (runtimeSystems != nullptr) {
            for (std::size_t systemIndex = 0;
                 systemIndex < model->particleSystems.size() && systemIndex < runtimeSystems->size();
                 ++systemIndex) {
                auto& runtime = (*runtimeSystems)[systemIndex];
                const auto& system = model->particleSystems[systemIndex];
                if (!runtime.initialized) {
                    runtime.lastSimulationTime = animationTime;
                    runtime.initialized = true;
                } else {
                    const float deltaTime = animationTime - runtime.lastSimulationTime;
                    if (deltaTime > 0.0f) {
                        const float sampleTime = runtime.lastSimulationTime + deltaTime * 0.5f;

                        // NiPSysModifierActiveCtlr targets a modifier by name. Apply it to a
                        // per-frame copy so cached authored data remains immutable and each
                        // placed object evaluates its own controller time independently.
                        auto effectiveModifiers = system.modifiers;
                        for (const auto& controller : system.controllers) {
                            if (controller.type != "NiPSysModifierActiveCtlr" ||
                                !controller.hasBoolTrack || controller.modifierName.empty()) {
                                continue;
                            }
                            // Gamebryo NiBlendInterpolator::LoadBinary deliberately streams no
                            // InterpArrayItems for manager-controlled blends. ControllerManager
                            // only adds them when a sequence is explicitly activated; loading the
                            // NIF merely registers SequenceData. Do not invent an activation.
                            if (controller.boolBlend && controller.boolBlend->managerControlled)
                                continue;
                            const auto activeValue = EvaluateBoolTrack(controller.boolTrack, sampleTime);
                            if (!activeValue) continue;
                            const auto modifier = std::find_if(
                                effectiveModifiers.begin(), effectiveModifiers.end(),
                                [&](const core::NifParticleModifierInfo& candidate) {
                                    return candidate.name == controller.modifierName;
                                });
                            if (modifier != effectiveModifiers.end()) modifier->active = *activeValue;
                        }

                        core::AdvanceNifParticleState(
                            runtime.particles, runtime.activeCount, effectiveModifiers, deltaTime,
                            &runtime.forceRandomState);

                        for (std::size_t modifierIndex = 0;
                             modifierIndex < effectiveModifiers.size() &&
                             modifierIndex < runtime.emitterAccumulators.size() &&
                             modifierIndex < runtime.emitterRandomStates.size();
                             ++modifierIndex) {
                            const auto& emitter = effectiveModifiers[modifierIndex];
                            if (!emitter.emitter || !emitter.active) continue;

                            const auto controller = std::find_if(
                                system.controllers.begin(), system.controllers.end(),
                                [&](const core::NifParticleControllerInfo& candidate) {
                                    return candidate.type == "NiPSysEmitterCtlr" &&
                                           candidate.modifierName == emitter.name;
                                });
                            if (controller == system.controllers.end() || !controller->hasFloatTrack) continue;

                            // Manager-controlled NiBlendFloatInterpolator has no standalone
                            // runtime value before ControllerManager activates a sequence.
                            // Gamebryo's blend Update returns false while interpCount == 0.
                            if (controller->floatBlend && controller->floatBlend->managerControlled)
                                continue;

                            const auto rateValue = EvaluateFloatTrack(controller->floatTrack, sampleTime);
                            bool emitterActive = true;
                            if (controller->hasVisibilityTrack &&
                                !(controller->visibilityBlend &&
                                  controller->visibilityBlend->managerControlled)) {
                                const auto activeValue =
                                    EvaluateBoolTrack(controller->visibilityTrack, sampleTime);
                                emitterActive = activeValue.value_or(false);
                            }

                            const float rate = rateValue.value_or(0.0f);
                            auto& accumulator = runtime.emitterAccumulators[modifierIndex];
                            if (!emitterActive || !(rate > 0.0f) || !std::isfinite(rate)) {
                                accumulator = 0.0f;
                                continue;
                            }

                            // NiPSEmitParticlesCtlr works in controller-scaled time. With a
                            // continuous active segment this fractional-event accumulator is
                            // equivalent to floor(rate*t) differencing while preserving the
                            // exact sub-frame ages used by NiPSEmitter::EmitParticles.
                            const float scaledDelta =
                                deltaTime * std::max(0.0f, std::fabs(controller->frequency));
                            const float previousFraction = accumulator;
                            const float totalBirths = previousFraction + rate * scaledDelta;
                            const auto requestedCount = static_cast<std::uint32_t>(
                                std::max(0.0f, std::floor(totalBirths)));
                            accumulator = totalBirths - static_cast<float>(requestedCount);
                            if (requestedCount == 0u) continue;

                            const std::uint32_t room = system.capacity > runtime.activeCount
                                ? system.capacity - runtime.activeCount : 0u;
                            const std::uint32_t count = std::min(requestedCount, room);
                            if (count == 0u) continue;

                            std::vector<float> ages;
                            ages.reserve(count);
                            const float firstEvent = (1.0f - previousFraction) / rate;
                            const float eventStep = 1.0f / rate;
                            for (std::uint32_t birth = 0; birth < count; ++birth) {
                                const float eventTime = firstEvent + eventStep * static_cast<float>(birth);
                                ages.push_back(std::max(0.0f, scaledDelta - eventTime));
                            }

                            (void)core::EmitNifParticles(
                                runtime.particles,
                                runtime.activeCount,
                                system.capacity,
                                emitter,
                                effectiveModifiers,
                                ages,
                                animationTime,
                                system.hasRotationAngles,
                                system.hasRotationAxes,
                                runtime.emitterRandomStates[modifierIndex]);
                        }
                    }
                    runtime.lastSimulationTime = animationTime;
                }
            }
        }

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
            if (sub.meshParticleTemplate) continue;
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

        if (runtimeSystems != nullptr) {
            for (std::size_t systemIndex = 0;
                 systemIndex < model->particleSystems.size() && systemIndex < runtimeSystems->size();
                 ++systemIndex) {
                const auto& system = model->particleSystems[systemIndex];
                const auto& runtime = (*runtimeSystems)[systemIndex];
                if (runtime.activeCount == 0) continue;
                // Gamebryo world-space particle systems render with their
                // world translation/rotation neutralized; only world scale
                // remains on the particle system itself. Positions emitted
                // into that space already contain the emitter's authored
                // world translation/rotation. Keep the map placement as the
                // editor instance transform, but do not apply the NIF particle
                // node translation/rotation a second time.
                const Mat4 particleSpace = system.worldSpace
                    ? (modelMat * UniformScaleMatrix(system.sceneTransform.scale))
                    : (modelMat * NifTransformToEditorMat4(system.sceneTransform));
                const float worldScale = std::sqrt(particleSpace.m[0] * particleSpace.m[0] +
                                                   particleSpace.m[1] * particleSpace.m[1] +
                                                   particleSpace.m[2] * particleSpace.m[2]);
                const std::size_t active =
                    std::min<std::size_t>(runtime.activeCount, runtime.particles.size());

                if (system.meshParticles) {
                    if (system.meshMasters.empty()) continue;
                    for (std::size_t p = 0; p < active; ++p) {
                        const auto& particle = runtime.particles[p];
                        std::size_t generation = particle.spawnGeneration;
                        if (generation >= system.meshMasters.size())
                            generation = system.meshMasters.size() - 1u;
                        const auto& master = system.meshMasters[generation];
                        if (master.subMeshIndices.empty()) continue;

                        // NiPSMeshParticleSystem::PostUpdate sets the cloned master root to
                        // particle position, axis-angle rotation and size*radius scale.
                        const Mat4 particleModel =
                            particleSpace *
                            TranslationMatrix(particle.position.x, particle.position.y, particle.position.z) *
                            ParticleAxisAngleRotation(particle.rotationAxis, particle.rotationAngle) *
                            UniformScaleMatrix(particle.radius * particle.size) *
                            NifTransformToEditorMat4(master.inverseSceneTransform);

                        for (const auto subMeshIndex : master.subMeshIndices) {
                            if (subMeshIndex >= model->subMeshes.size()) continue;
                            const auto& sub = model->subMeshes[subMeshIndex];

                            // Gamebryo clones the complete master AVObject subtree, starts its
                            // controllers at t=0 and updates that clone at particle.age. Billboard
                            // children therefore face the camera per clone, while Flip/Texture
                            // controllers sample particle age rather than the editor's global time.
                            Mat4 effectiveParticleModel = particleModel;
                            if (sub.billboard) {
                                const float cloneScale = std::sqrt(
                                    particleModel.m[0] * particleModel.m[0] +
                                    particleModel.m[1] * particleModel.m[1] +
                                    particleModel.m[2] * particleModel.m[2]);
                                effectiveParticleModel = ApplyBillboard(
                                    particleModel, cloneScale, sub.billboardPivot,
                                    sub.billboardMode, sub.billboardInverseRotation, camera);
                            }

                            const auto worldCenter =
                                TransformPoint(effectiveParticleModel, sub.localCenter);
                            const auto centerView = TransformPoint(view, worldCenter);
                            DrawItem item{&sub, effectiveParticleModel, -centerView[2]};
                            item.ageLocalControllers = true;
                            item.controllerTime = particle.age;
                            (sub.alphaBlend ? blendedItems : opaqueItems).push_back(item);
                        }
                    }
                    continue;
                }

                for (std::size_t p = 0; p < active; ++p) {
                    const auto& particle = runtime.particles[p];
                    const auto center = TransformPoint(particleSpace,
                        {particle.position.x, particle.position.y, particle.position.z});
                    const float radius = system.hasRadii ? particle.radius : 1.0f;
                    const float size = system.hasSizes ? particle.size : 1.0f;
                    const float halfSize = std::abs(radius * size * worldScale);
                    if (!std::isfinite(center[0]) || !std::isfinite(center[1]) || !std::isfinite(center[2]) ||
                        !std::isfinite(halfSize) || halfSize <= 1.0e-6f) continue;

                    const Mat4 face = BillboardFacingRotation(center, camera, 0u);
                    const Mat4 inPlaneRotation = system.hasRotationAngles
                        ? ParticleScreenRotation(particle.rotationAngle)
                        : Mat4::Identity();
                    const Mat4 particleModel = TranslationMatrix(center[0], center[1], center[2]) *
                                               face * inPlaneRotation * UniformScaleMatrix(halfSize);
                    const auto centerView = TransformPoint(view, center);
                    DrawItem item{&system.material, particleModel, -centerView[2]};
                    item.particle = true;
                    if (system.hasColors)
                        item.particleColor = {
                            particle.color.r, particle.color.g, particle.color.b, particle.color.a};
                    (system.material.alphaBlend ? blendedItems : opaqueItems).push_back(item);
                }
            }
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

    const auto stencilFunction = [](std::uint32_t f) -> GLenum {
        switch (f) {
            case 0: return GL_NEVER;
            case 1: return GL_LESS;
            case 2: return GL_EQUAL;
            case 3: return GL_LEQUAL;
            case 4: return GL_GREATER;
            case 5: return GL_NOTEQUAL;
            case 6: return GL_GEQUAL;
            case 7: return GL_ALWAYS;
            default: return GL_ALWAYS; // keep geometry visible; material audit reports the gap
        }
    };

    const auto stencilAction = [](std::uint32_t action) -> GLenum {
        switch (action) {
            case 0: return GL_KEEP;
            case 1: return GL_ZERO;
            case 2: return GL_REPLACE;
            case 3: return GL_INCR;
            case 4: return GL_DECR;
            case 5: return GL_INVERT;
            default: return GL_KEEP; // conservative runtime fallback; audit remains explicit
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
        applyFaceDrawMode(sub.faceDrawMode);
        if (sub.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(sub.depthWrite ? GL_TRUE : GL_FALSE);
        glDepthFunc(depthFunction(sub.depthFunction));
        if (sub.stencilEnabled) {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(stencilFunction(sub.stencilFunction),
                          static_cast<GLint>(sub.stencilReference),
                          static_cast<GLuint>(sub.stencilMask));
            // NiStencilProperty exposes one compare/value mask, not a separate write mask.
            // Gamebryo-compatible loaders therefore leave stencil writes fully enabled.
            glStencilMask(0xFFFFFFFFu);
            glStencilOp(stencilAction(sub.stencilFailAction),
                        stencilAction(sub.stencilZFailAction),
                        stencilAction(sub.stencilPassAction));
        } else {
            glDisable(GL_STENCIL_TEST);
        }
        if (blendedPass) glBlendFunc(blendFactor(sub.alphaSrcBlend), blendFactor(sub.alphaDstBlend));

        if (sub.glass) {
            glUseProgram(glassShaderProgram_);
            glUniformMatrix4fv(glassUniforms_.locViewProj, 1, GL_FALSE, viewProj.m);
            glUniformMatrix4fv(glassUniforms_.locModel, 1, GL_FALSE, item.model.m);
            glUniform3f(glassUniforms_.locCameraPos,
                        camera.EyeX(), camera.EyeY(), -camera.EyeZ());
            glUniform1i(glassUniforms_.locEnvironment, 0);
            glUniform1i(glassUniforms_.locRainbow, 1);
            glUniform4f(glassUniforms_.locBaseColor,
                        sub.glassParameters.baseColor.r,
                        sub.glassParameters.baseColor.g,
                        sub.glassParameters.baseColor.b,
                        sub.glassParameters.baseColor.a);
            glUniform1f(glassUniforms_.locRefractionScale,
                        sub.glassParameters.refractionScale);
            glUniform1f(glassUniforms_.locReflectionScale,
                        sub.glassParameters.reflectionScale);
            glUniform1f(glassUniforms_.locIorRatio,
                        sub.glassParameters.indexOfRefractionRatio);
            glUniform1f(glassUniforms_.locAmbient,
                        sub.glassParameters.ambient);
            glUniform1f(glassUniforms_.locRainbowSpread,
                        sub.glassParameters.rainbowSpread);
            glUniform1f(glassUniforms_.locRainbowScale,
                        sub.glassParameters.rainbowScale);
            glUniform1i(glassUniforms_.locAlphaTest, sub.alphaTest ? 1 : 0);
            glUniform1f(glassUniforms_.locAlphaCutoff, sub.alphaCutoff);
            glUniform1i(glassUniforms_.locAlphaTestFunc,
                        static_cast<int>(sub.alphaTestFunc));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, sub.glassEnvironmentCube);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, sub.textures[1].texture);
            if (sub.textures[1].texture != 0) {
                // Glass.NSF fixes both samplers to clamp + linear filtering.
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            }

            glBindVertexArray(sub.vao);
            glDrawElements(GL_TRIANGLES, static_cast<int>(sub.indexCount), GL_UNSIGNED_INT, nullptr);
            glUseProgram(shaderProgram_);
            return;
        }

        glUniformMatrix4fv(locModel, 1, GL_FALSE, item.model.m);
        {
            // LOD-Ansicht: grau = kein NiLODNode, grün = Detailstufe ab 0, gelb = mittlere, rot = ferne Stufe.
            float tint[3] = {0.55f, 0.55f, 0.55f};
            if (item.sub && item.sub->lodControlled) {
                if (item.sub->lodNear <= 0.0f) { tint[0] = 0.25f; tint[1] = 0.85f; tint[2] = 0.35f; }
                else if (item.sub->lodNear < 2000.0f) { tint[0] = 0.95f; tint[1] = 0.80f; tint[2] = 0.20f; }
                else { tint[0] = 0.95f; tint[1] = 0.30f; tint[2] = 0.25f; }
            }
            glUniform3fv(uniforms_.locLodTint, 1, tint);
        }
        glUniform1i(locParticleMode, item.particle ? 1 : 0);
        glUniform4f(locParticleColor, item.particleColor[0], item.particleColor[1],
                    item.particleColor[2], item.particleColor[3]);

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
        glUniform1i(locAlphaTextureBlender11, sub.alphaTextureBlender11 ? 1 : 0);
        glUniform1i(locAlphaTextureBlender, sub.alphaTextureBlender ? 1 : 0);
        glUniform1i(locPgTerrain, sub.pgTerrain ? 1 : 0);
        glUniform1i(locVertexColorMode, static_cast<int>(sub.vertexColorMode));
        glUniform1f(locBumpLumaScale, sub.bumpMapLumaScale);
        glUniform1f(locBumpLumaOffset, sub.bumpMapLumaOffset);
        glUniformMatrix2fv(locBumpMatrix, 1, GL_FALSE, sub.bumpMapMatrix.data());

        const float controllerTime =
            item.ageLocalControllers ? item.controllerTime : animationTime;
        std::array<TextureBinding, 10> animatedTextures = sub.textures;
        for (const auto& anim : sub.textureTransformAnimations) {
            if (anim.slot >= animatedTextures.size()) continue;
            const auto value = EvaluateFloatTrack(anim.track, controllerTime);
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
            const auto value = EvaluateFloatTrack(anim.track, controllerTime);
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
        glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(prevCubeTextures[unit]));
    }
    glActiveTexture(static_cast<GLenum>(prevActiveTexture));
    glUseProgram(static_cast<GLuint>(prevProgram));
    glDepthMask(prevDepthMask);
    glDepthFunc(static_cast<GLenum>(prevDepthFunc));
    glBlendFuncSeparate(static_cast<GLenum>(prevBlendSrcRgb), static_cast<GLenum>(prevBlendDstRgb),
                        static_cast<GLenum>(prevBlendSrcAlpha), static_cast<GLenum>(prevBlendDstAlpha));
    glStencilFuncSeparate(GL_FRONT, static_cast<GLenum>(prevStencilFunc), prevStencilRef,
                          static_cast<GLuint>(prevStencilValueMask));
    glStencilMaskSeparate(GL_FRONT, static_cast<GLuint>(prevStencilWriteMask));
    glStencilOpSeparate(GL_FRONT, static_cast<GLenum>(prevStencilFail),
                        static_cast<GLenum>(prevStencilZFail), static_cast<GLenum>(prevStencilZPass));
    glStencilFuncSeparate(GL_BACK, static_cast<GLenum>(prevStencilBackFunc), prevStencilBackRef,
                          static_cast<GLuint>(prevStencilBackValueMask));
    glStencilMaskSeparate(GL_BACK, static_cast<GLuint>(prevStencilBackWriteMask));
    glStencilOpSeparate(GL_BACK, static_cast<GLenum>(prevStencilBackFail),
                        static_cast<GLenum>(prevStencilBackZFail), static_cast<GLenum>(prevStencilBackZPass));
    glCullFace(static_cast<GLenum>(prevCullFace));
    glFrontFace(static_cast<GLenum>(prevFrontFace));
    if (prevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (prevBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (prevCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (prevStencilTest) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
}

} // namespace theseed::mapeditor::app
