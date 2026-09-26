#include "mapeditor/core/NifModel.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>

namespace fs = std::filesystem;
namespace core = theseed::mapeditor::core;

namespace {

struct SlotStats {
    std::size_t parts = 0;
    std::size_t embedded = 0;
    std::size_t external = 0;
    std::size_t unresolvedEmbedded = 0;
};

struct ShaderSlotStats {
    std::size_t descriptors = 0;
    std::size_t embedded = 0;
    std::size_t external = 0;
    std::size_t unresolvedEmbedded = 0;
    std::set<std::string> files;
    std::set<std::uint32_t> uvSets;
    std::set<std::uint32_t> transformMethods;
};

struct EffectStats {
    std::size_t bindings = 0;
    std::size_t embedded = 0;
    std::size_t external = 0;
    std::size_t unresolvedEmbedded = 0;
    std::size_t clippingPlane = 0;
    std::size_t nonIdentityProjection = 0;
    std::set<std::string> files;
    std::set<std::uint32_t> clampModes;
    std::set<std::uint32_t> filterModes;
};

bool ProjectionIsIdentity(const core::NifTextureEffectBinding& effect) {
    static constexpr std::array<float, 9> kIdentity{
        1.0f,0.0f,0.0f,
        0.0f,1.0f,0.0f,
        0.0f,0.0f,1.0f
    };
    constexpr float kEpsilon = 1.0e-5f;
    for (std::size_t i = 0; i < kIdentity.size(); ++i)
        if (std::abs(effect.projectionRotation[i] - kIdentity[i]) > kEpsilon) return false;
    return std::abs(effect.projectionPosition.x) <= kEpsilon &&
           std::abs(effect.projectionPosition.y) <= kEpsilon &&
           std::abs(effect.projectionPosition.z) <= kEpsilon;
}

std::string Clean(std::string value) {
    for (char& ch : value) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    }
    return value;
}

const char* ApplyModeName(std::uint32_t mode) {
    switch (mode) {
        case 0: return "APPLY_REPLACE";
        case 1: return "APPLY_DECAL";
        case 2: return "APPLY_MODULATE";
        case 3: return "APPLY_HILIGHT";
        case 4: return "APPLY_HILIGHT2";
        default: return "UNKNOWN";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: nif_material_inventory <root> [root ...]\n";
        return 2;
    }

    std::size_t files = 0;
    std::size_t loaded = 0;
    std::size_t failed = 0;
    std::size_t parts = 0;
    std::size_t decodedEmbedded = 0;
    std::size_t undecodedEmbedded = 0;
    std::size_t unsupportedEffects = 0;
    std::size_t inheritedProperties = 0;

    std::map<std::string, std::size_t> shaderParts;
    std::map<std::string, std::set<std::string>> shaderFiles;
    std::map<std::pair<std::string, std::uint32_t>, ShaderSlotStats> shaderSlots;
    std::map<std::pair<std::string, std::size_t>, SlotStats> shaderClassicSlots;
    std::map<std::pair<std::string, std::uint32_t>, std::size_t> shaderApplyModes;
    std::array<SlotStats, 10> slots{};
    std::map<std::size_t, std::size_t> uvSetCounts;
    std::map<std::uint32_t, std::size_t> transformMethods;
    std::map<std::uint32_t, std::size_t> classicClampModes;
    std::map<std::uint32_t, std::size_t> classicFilterModes;
    std::map<std::uint32_t, std::set<std::string>> unusualFilterFiles;
    std::map<std::uint32_t, std::size_t> applyModes;
    std::map<std::uint32_t, std::set<std::string>> applyModeFiles;
    std::map<std::uint32_t, std::size_t> vertexColorModes;
    std::map<std::uint32_t, std::size_t> faceDrawModes;
    std::map<std::tuple<std::uint32_t, std::uint32_t, bool>, EffectStats> effects;
    std::size_t uvRendererOverflowParts = 0;
    std::size_t uvRendererOverflowBindings = 0;
    std::size_t missingRequestedUvBindings = 0;
    std::size_t uv0FallbackBindings = 0;
    std::size_t noUsableUvBindings = 0;
    std::map<std::string, std::size_t> uvFallbackFiles;
    std::map<std::string, std::size_t> noUvFiles;
    std::size_t unmaterializedShaderDescriptors = 0;
    std::size_t unmaterializedApplyModeParts = 0;
    std::size_t unsupportedEffectBindings = 0;
    std::size_t clippingEffectBindings = 0;
    std::size_t projectedEffectBindings = 0;
    std::set<std::string> rendererGapFiles;
    std::size_t alphaBlendParts = 0;
    std::size_t alphaTestParts = 0;
    std::size_t depthTestDisabledParts = 0;
    std::size_t depthWriteDisabledParts = 0;
    std::size_t textureTransformTracks = 0;
    std::size_t textureFlipTracks = 0;
    std::vector<std::pair<std::string, std::string>> failures;

    for (int arg = 1; arg < argc; ++arg) {
        const fs::path root(argv[arg]);
        if (!fs::is_directory(root)) {
            std::cerr << "Not a directory: " << root << '\n';
            return 2;
        }

        for (const auto& entry : fs::recursive_directory_iterator(
                 root, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".nif") continue;
            ++files;

            const auto model = core::LoadNifMesh(entry.path(), false);
            if (!model) {
                ++failed;
                failures.emplace_back(entry.path().string(), Clean(model.error()));
                continue;
            }

            ++loaded;
            decodedEmbedded += model->decodedEmbeddedTextures;
            undecodedEmbedded += model->undecodedEmbeddedTextures;
            unsupportedEffects += model->textureEffectUnsupportedBlocks;
            inheritedProperties += model->inheritedPropertyBindings;

            for (const auto& part : model->parts) {
                ++parts;
                const std::string shader = part.shaderName.empty() ? "<fixed-function>" : part.shaderName;
                ++shaderParts[shader];
                shaderFiles[shader].insert(entry.path().string());
                ++uvSetCounts[part.uvSets.size()];
                if (part.uvSets.size() > 8u) {
                    ++uvRendererOverflowParts;
                    rendererGapFiles.insert(entry.path().string());
                }
                ++applyModes[part.textureApplyMode];
                if (part.textureApplyMode == 3u || part.textureApplyMode == 4u) {
                    ++unmaterializedApplyModeParts;
                    rendererGapFiles.insert(entry.path().string());
                }
                applyModeFiles[part.textureApplyMode].insert(entry.path().string());
                if (shader != "<fixed-function>")
                    ++shaderApplyModes[{shader, part.textureApplyMode}];
                ++vertexColorModes[part.hasVertexColorProperty ? part.vertexColorMode : 0xffffffffu];
                ++faceDrawModes[part.faceDrawMode];
                alphaBlendParts += part.alphaBlend ? 1u : 0u;
                alphaTestParts += part.alphaTest ? 1u : 0u;
                depthTestDisabledParts += part.depthTest ? 0u : 1u;
                depthWriteDisabledParts += part.depthWrite ? 0u : 1u;
                textureTransformTracks += part.textureTransformAnimations.size();
                textureFlipTracks += part.textureFlipAnimations.size();

                for (std::size_t slot = 0; slot < part.textureSlots.size(); ++slot) {
                    const auto& texture = part.textureSlots[slot];
                    if (!texture.present) continue;
                    auto& stat = slots[slot];
                    ++stat.parts;
                    if (texture.sourceUsesEmbeddedPixelData) {
                        ++stat.embedded;
                        if (!texture.embeddedTexture) ++stat.unresolvedEmbedded;
                    } else if (!texture.texture.empty()) {
                        ++stat.external;
                    }
                    ++classicClampModes[texture.clampMode];
                    ++classicFilterModes[texture.filterMode];
                    if (texture.filterMode >= 6u) {
                        unusualFilterFiles[texture.filterMode].insert(entry.path().string());
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (texture.uvSet > 7u) {
                        ++uvRendererOverflowBindings;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    const bool hasRequestedUvs =
                        texture.uvSet < part.uvSets.size() &&
                        part.uvSets[texture.uvSet].size() == part.positions.size();
                    if (!hasRequestedUvs) {
                        ++missingRequestedUvBindings;
                        const bool hasBaseFallbackUvs =
                            part.uvs.size() == part.positions.size();
                        if (hasBaseFallbackUvs) {
                            ++uv0FallbackBindings;
                            ++uvFallbackFiles[entry.path().string()];
                        } else {
                            ++noUsableUvBindings;
                            ++noUvFiles[entry.path().string()];
                        }
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (shader != "<fixed-function>") {
                        auto& shaderStat = shaderClassicSlots[{shader, slot}];
                        ++shaderStat.parts;
                        if (texture.sourceUsesEmbeddedPixelData) {
                            ++shaderStat.embedded;
                            if (!texture.embeddedTexture) ++shaderStat.unresolvedEmbedded;
                        } else if (!texture.texture.empty()) {
                            ++shaderStat.external;
                        }
                    }
                    if (texture.hasTransform) ++transformMethods[texture.transformType];
                }

                for (const auto& shaderSlot : part.shaderTextureSlots) {
                    auto& stat = shaderSlots[{shader, shaderSlot.mapId}];
                    if (shader != "VCAlphaTextureBlender" || shaderSlot.mapId > 2u) {
                        ++unmaterializedShaderDescriptors;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (shaderSlot.texture.uvSet > 7u) {
                        ++uvRendererOverflowBindings;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (shaderSlot.texture.present) {
                        const bool hasRequestedUvs =
                            shaderSlot.texture.uvSet < part.uvSets.size() &&
                            part.uvSets[shaderSlot.texture.uvSet].size() == part.positions.size();
                        if (!hasRequestedUvs) {
                            ++missingRequestedUvBindings;
                            const bool hasBaseFallbackUvs =
                                part.uvs.size() == part.positions.size();
                            if (hasBaseFallbackUvs) {
                                ++uv0FallbackBindings;
                                ++uvFallbackFiles[entry.path().string()];
                            } else {
                                ++noUsableUvBindings;
                                ++noUvFiles[entry.path().string()];
                            }
                            rendererGapFiles.insert(entry.path().string());
                        }
                    }
                    ++stat.descriptors;
                    stat.files.insert(entry.path().string());
                    const auto& texture = shaderSlot.texture;
                    if (!texture.present) continue;
                    stat.uvSets.insert(texture.uvSet);
                    if (texture.hasTransform) stat.transformMethods.insert(texture.transformType);
                    if (texture.sourceUsesEmbeddedPixelData) {
                        ++stat.embedded;
                        if (!texture.embeddedTexture) ++stat.unresolvedEmbedded;
                    } else if (!texture.texture.empty()) {
                        ++stat.external;
                    }
                }

                for (const auto& effect : part.textureEffects) {
                    auto& stat = effects[{effect.textureType, effect.coordGenType, effect.enabled}];
                    ++stat.bindings;
                    stat.files.insert(entry.path().string());
                    stat.clampModes.insert(effect.clampMode);
                    stat.filterModes.insert(effect.filterMode);
                    if (effect.sourceUsesEmbeddedPixelData) {
                        ++stat.embedded;
                        if (!effect.embeddedTexture) ++stat.unresolvedEmbedded;
                    } else if (!effect.texture.empty()) {
                        ++stat.external;
                    }
                    if (effect.clippingPlaneEnabled) {
                        ++stat.clippingPlane;
                        ++clippingEffectBindings;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (!ProjectionIsIdentity(effect)) {
                        ++stat.nonIdentityProjection;
                        ++projectedEffectBindings;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    const bool rendererSupported =
                        effect.enabled && effect.textureType == 2u && effect.coordGenType == 2u &&
                        !effect.clippingPlaneEnabled && ProjectionIsIdentity(effect);
                    if (effect.enabled && !rendererSupported) {
                        ++unsupportedEffectBindings;
                        rendererGapFiles.insert(entry.path().string());
                    }
                }
            }
        }
    }

    std::cout << "SUMMARY\tfiles=" << files
              << "\tloaded=" << loaded
              << "\tfailed=" << failed
              << "\tparts=" << parts
              << "\tdecodedEmbedded=" << decodedEmbedded
              << "\tundecodedEmbedded=" << undecodedEmbedded
              << "\tunsupportedEffects=" << unsupportedEffects
              << "\tinheritedProperties=" << inheritedProperties
              << "\tuvOverflowParts=" << uvRendererOverflowParts
              << "\tuvOverflowBindings=" << uvRendererOverflowBindings
              << "\tmissingRequestedUvBindings=" << missingRequestedUvBindings
              << "\tuv0FallbackBindings=" << uv0FallbackBindings
              << "\tnoUsableUvBindings=" << noUsableUvBindings
              << "\tunmaterializedShaderDescriptors=" << unmaterializedShaderDescriptors
              << "\tunmaterializedApplyModes=" << unmaterializedApplyModeParts
              << "\tunsupportedEffectBindings=" << unsupportedEffectBindings
              << "\tclippingEffects=" << clippingEffectBindings
              << "\tprojectedEffects=" << projectedEffectBindings
              << "\trendererGapFiles=" << rendererGapFiles.size() << '\n';

    for (const auto& [name, count] : shaderParts) {
        std::cout << "SHADER\tparts=" << count
                  << "\tfiles=" << shaderFiles[name].size()
                  << "\tname=" << Clean(name) << '\n';
        if (name != "<fixed-function>") {
            for (const auto& path : shaderFiles[name])
                std::cout << "SHADERFILE\tname=" << Clean(name)
                          << "\tpath=" << Clean(path) << '\n';
        }
    }

    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        const auto& stat = slots[slot];
        if (!stat.parts) continue;
        std::cout << "SLOT\tindex=" << slot
                  << "\tparts=" << stat.parts
                  << "\tembedded=" << stat.embedded
                  << "\texternal=" << stat.external
                  << "\tunresolvedEmbedded=" << stat.unresolvedEmbedded << '\n';
    }

    for (const auto& [key, count] : shaderApplyModes) {
        const auto& [shader, mode] = key;
        std::cout << "SHADERAPPLY\tshader=" << Clean(shader)
                  << "\tmode=" << mode
                  << "\tname=" << ApplyModeName(mode)
                  << "\tparts=" << count;
        if (mode == 3u || mode == 4u) std::cout << "\trenderer=modulate-fallback";
        std::cout << '\n';
    }

    for (const auto& [key, stat] : shaderClassicSlots) {
        const auto& [shader, slot] = key;
        std::cout << "SHADERCLASSICSLOT\tshader=" << Clean(shader)
                  << "\tindex=" << slot
                  << "\tparts=" << stat.parts
                  << "\tembedded=" << stat.embedded
                  << "\texternal=" << stat.external
                  << "\tunresolvedEmbedded=" << stat.unresolvedEmbedded << '\n';
    }

    for (const auto& [key, stat] : shaderSlots) {
        const auto& [shader, mapId] = key;
        std::cout << "SHADERSLOT\tshader=" << Clean(shader)
                  << "\tmapId=" << mapId
                  << "\tdescriptors=" << stat.descriptors
                  << "\tfiles=" << stat.files.size()
                  << "\tembedded=" << stat.embedded
                  << "\texternal=" << stat.external
                  << "\tunresolvedEmbedded=" << stat.unresolvedEmbedded
                  << "\tuvSets=";
        bool first = true;
        for (const auto uv : stat.uvSets) {
            if (!first) std::cout << ',';
            std::cout << uv;
            first = false;
        }
        std::cout << "\ttransformMethods=";
        first = true;
        for (const auto method : stat.transformMethods) {
            if (!first) std::cout << ',';
            std::cout << method;
            first = false;
        }
        std::cout << '\n';
        for (const auto& path : stat.files)
            std::cout << "SHADERSLOTFILE\tshader=" << Clean(shader)
                      << "\tmapId=" << mapId
                      << "\tpath=" << Clean(path) << '\n';
    }

    for (const auto& [count, partCount] : uvSetCounts)
        std::cout << "UVSETS\tcount=" << count << "\tparts=" << partCount << '\n';
    for (const auto& [path, count] : uvFallbackFiles)
        std::cout << "UVFALLBACKFILE\tbindings=" << count
                  << "\tpath=" << Clean(path) << '\n';
    for (const auto& [path, count] : noUvFiles)
        std::cout << "NOUVFILE\tbindings=" << count
                  << "\tpath=" << Clean(path) << '\n';
    for (const auto& [method, partCount] : transformMethods)
        std::cout << "TEXTRANSFORM\tmethod=" << method << "\tparts=" << partCount << '\n';
    for (const auto& [mode, bindingCount] : classicClampModes)
        std::cout << "CLAMPMODE\tmode=" << mode << "\tbindings=" << bindingCount << '\n';
    for (const auto& [mode, bindingCount] : classicFilterModes) {
        std::cout << "FILTERMODE\tmode=" << mode << "\tbindings=" << bindingCount;
        if (mode >= 6u) std::cout << "\trenderer=trilinear-fallback";
        std::cout << '\n';
        if (mode >= 6u)
            for (const auto& path : unusualFilterFiles[mode])
                std::cout << "FILTERMODEFILE\tmode=" << mode
                          << "\tpath=" << Clean(path) << '\n';
    }
    for (const auto& [mode, partCount] : applyModes) {
        std::cout << "APPLYMODE\tmode=" << mode
                  << "\tname=" << ApplyModeName(mode)
                  << "\tparts=" << partCount;
        if (mode == 3u || mode == 4u) std::cout << "\trenderer=modulate-fallback";
        std::cout << '\n';
        if (mode != 2u) {
            for (const auto& path : applyModeFiles[mode])
                std::cout << "APPLYMODEFILE\tmode=" << mode
                          << "\tname=" << ApplyModeName(mode)
                          << "\tpath=" << Clean(path) << '\n';
        }
    }
    for (const auto& [mode, partCount] : vertexColorModes)
        std::cout << "VERTEXCOLOR\tmode=" << mode << "\tparts=" << partCount << '\n';
    for (const auto& [mode, partCount] : faceDrawModes)
        std::cout << "FACEDRAW\tmode=" << mode << "\tparts=" << partCount << '\n';

    for (const auto& [key, stat] : effects) {
        const auto [textureType, coordGenType, enabled] = key;
        const bool rendererSupported = enabled && textureType == 2u && coordGenType == 2u &&
                                       stat.clippingPlane == 0u && stat.nonIdentityProjection == 0u;
        std::cout << "EFFECT\ttextureType=" << textureType
                  << "\tcoordGenType=" << coordGenType
                  << "\tenabled=" << (enabled ? 1 : 0)
                  << "\tbindings=" << stat.bindings
                  << "\tembedded=" << stat.embedded
                  << "\texternal=" << stat.external
                  << "\tunresolvedEmbedded=" << stat.unresolvedEmbedded
                  << "\tclipping=" << stat.clippingPlane
                  << "\tnonIdentityProjection=" << stat.nonIdentityProjection
                  << "\trenderer=" << (rendererSupported ? "environment-sphere" : "diagnostic")
                  << "\tclampModes=";
        bool first = true;
        for (const auto mode : stat.clampModes) {
            if (!first) std::cout << ',';
            std::cout << mode;
            first = false;
        }
        std::cout << "\tfilterModes=";
        first = true;
        for (const auto mode : stat.filterModes) {
            if (!first) std::cout << ',';
            std::cout << mode;
            first = false;
        }
        std::cout << '\n';
        if (!rendererSupported || stat.unresolvedEmbedded != 0u) {
            for (const auto& path : stat.files)
                std::cout << "EFFECTFILE\ttextureType=" << textureType
                          << "\tcoordGenType=" << coordGenType
                          << "\tenabled=" << (enabled ? 1 : 0)
                          << "\tpath=" << Clean(path) << '\n';
        }
    }

    for (const auto& path : rendererGapFiles)
        std::cout << "RENDERGAPFILE\tpath=" << Clean(path) << '\n';

    std::cout << "STATE\talphaBlendParts=" << alphaBlendParts
              << "\talphaTestParts=" << alphaTestParts
              << "\tdepthTestDisabledParts=" << depthTestDisabledParts
              << "\tdepthWriteDisabledParts=" << depthWriteDisabledParts
              << "\ttextureTransformTracks=" << textureTransformTracks
              << "\ttextureFlipTracks=" << textureFlipTracks << '\n';

    for (const auto& [path, error] : failures)
        std::cout << "FAIL\tpath=" << Clean(path) << "\terror=" << error << '\n';

    if (files == 0) {
        std::cerr << "No NIF files found.\n";
        return 1;
    }
    if (failed != 0) {
        std::cerr << "Fixture material audit failed: " << failed << " NIF file(s) did not load.\n";
        return 1;
    }
    if (undecodedEmbedded != 0) {
        std::cerr << "Fixture material audit failed: " << undecodedEmbedded
                  << " embedded texture(s) were not decoded.\n";
        return 1;
    }
    return 0;
}
