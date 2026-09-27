#include "mapeditor/core/NifModel.hpp"
#include "mapeditor/core/DdsImage.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

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

std::string TextureLookupKey(std::string value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isspace(ch)) continue;
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

bool IsTextureExtension(const fs::path& path) {
    std::string ext = path.extension().string();
    for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return ext == ".dds" || ext == ".tga" || ext == ".bmp" ||
           ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

struct ExternalTextureResolution {
    std::optional<fs::path> path;
    bool ambiguous = false;
};

class ExternalTextureIndex {
public:
    void Build(const std::vector<fs::path>& roots) {
        if (built_) return;
        built_ = true;
        for (const auto& root : roots) {
            if (!fs::is_directory(root)) continue;
            std::error_code ec;
            fs::recursive_directory_iterator it(
                root, fs::directory_options::skip_permission_denied, ec);
            const fs::recursive_directory_iterator end;
            for (; it != end; it.increment(ec)) {
                if (ec) { ec.clear(); continue; }
                if (!it->is_regular_file(ec) || ec || !IsTextureExtension(it->path())) {
                    ec.clear();
                    continue;
                }
                const std::string key = TextureLookupKey(it->path().filename().string());
                if (key.empty() || ambiguous_.contains(key)) continue;
                const auto [found, inserted] = unique_.emplace(key, it->path());
                if (!inserted && found->second != it->path()) {
                    unique_.erase(found);
                    ambiguous_.insert(key);
                }
            }
        }
    }

    ExternalTextureResolution Resolve(
        const fs::path& nifPath,
        const std::string& legacyPath,
        const std::vector<fs::path>& roots) {
        if (legacyPath.empty()) return {};
        const auto native = core::legacy::LegacyPathToNative(legacyPath);
        const auto stripped = core::legacy::StripResmapPrefix(native);
        const auto modelDir = nifPath.parent_path();

        if (!modelDir.empty()) {
            if (auto local = core::legacy::ResolveCaseInsensitivePath(modelDir, native))
                return {*local, false};
            if (stripped != native) {
                if (auto local = core::legacy::ResolveCaseInsensitivePath(modelDir, stripped))
                    return {*local, false};
            }
        }
        for (const auto& root : roots) {
            if (auto rooted = core::legacy::ResolveCaseInsensitivePath(root, native))
                return {*rooted, false};
            if (stripped != native) {
                if (auto rooted = core::legacy::ResolveCaseInsensitivePath(root, stripped))
                    return {*rooted, false};
            }
        }

        Build(roots);
        std::string filename = legacyPath;
        if (const auto slash = filename.find_last_of("\\/"); slash != std::string::npos)
            filename = filename.substr(slash + 1);
        const std::string key = TextureLookupKey(filename);
        if (key.empty()) return {};
        if (ambiguous_.contains(key)) return {std::nullopt, true};
        const auto found = unique_.find(key);
        if (found == unique_.end()) return {};
        return {found->second, false};
    }

private:
    bool built_ = false;
    std::map<std::string, fs::path> unique_;
    std::set<std::string> ambiguous_;
};

const char* ApplyModeName(std::uint32_t mode) {
    switch (mode) {
        case 0: return "APPLY_REPLACE";
        case 1: return "APPLY_DECAL";
        case 2: return "APPLY_MODULATE";
        case 3: return "APPLY_DEPRECATED";
        case 4: return "APPLY_DEPRECATED2";
        default: return "UNKNOWN";
    }
}

} // namespace

int main(int argc, char** argv) {
    bool strictRenderer = false;
    std::vector<fs::path> roots;
    std::vector<fs::path> assetRoots;
    for (int arg = 1; arg < argc; ++arg) {
        const std::string value = argv[arg];
        if (value == "--strict-renderer") {
            strictRenderer = true;
        } else if (value == "--asset-root") {
            if (arg + 1 >= argc) {
                std::cerr << "--asset-root requires a directory argument.\n";
                return 2;
            }
            assetRoots.emplace_back(argv[++arg]);
        } else {
            roots.emplace_back(value);
        }
    }
    if (roots.empty()) {
        std::cerr << "Usage: nif_material_inventory [--strict-renderer] "
                     "[--asset-root <dir> ...] <root> [root ...]\n";
        return 2;
    }
    for (const auto& assetRoot : assetRoots) {
        if (!fs::is_directory(assetRoot)) {
            std::cerr << "Asset root is not a directory: " << assetRoot << '\n';
            return 2;
        }
    }
    const bool verifyExternalTextures = !assetRoots.empty();

    std::size_t files = 0;
    std::size_t loaded = 0;
    std::size_t failed = 0;
    std::size_t parts = 0;
    std::size_t decodedEmbedded = 0;
    std::size_t undecodedEmbedded = 0;
    std::size_t unsupportedEffects = 0;
    std::size_t inheritedProperties = 0;
    std::size_t particleSystems = 0;
    std::map<std::string, std::size_t> particleFiles;
    std::map<std::string, std::size_t> particleModifierTypes;
    std::map<std::string, std::size_t> particleControllerTypes;
    std::size_t resolvedParticleControllers = 0;
    std::size_t particleSystemsWithoutData = 0;
    std::size_t unsupportedParticleModifierBindings = 0;
    std::size_t unsupportedParticleControllerBindings = 0;
    std::size_t meshParticleMasterDynamicParts = 0;
    std::size_t emitterControllersWithoutRate = 0;
    std::size_t emitterControllersWithoutVisibility = 0;
    std::size_t activeControllersWithoutBoolTrack = 0;
    std::size_t blendParticleControllerTracks = 0;
    std::size_t managerControlledParticleBlendTracks = 0;
    std::size_t dormantManagerControlledParticleBlendTracks = 0;
    std::size_t weightedParticleBlendTracks = 0;
    std::size_t colorModifiersWithoutTrack = 0;
    std::size_t volumeEmittersWithoutTransform = 0;
    std::size_t meshEmittersWithoutGeometry = 0;
    std::size_t skinnedMeshEmitterBindings = 0;
    std::size_t forceModifiersWithoutTransform = 0;
    std::map<std::uint32_t, std::size_t> particleColorInterpolations;
    std::size_t invalidParticleModifierRefs = 0;
    std::size_t particleCapacity = 0;
    std::size_t activeParticles = 0;
    std::size_t texturedParticleSystems = 0;
    std::size_t meshParticleSystems = 0;
    std::size_t meshParticleSystemsWithoutMasters = 0;
    std::size_t meshParticleMasterGenerations = 0;
    std::size_t meshParticleMasterParts = 0;
    std::size_t worldSpaceParticleSystems = 0;
    std::size_t particleRotationSpeedSystems = 0;
    std::size_t spawnOnDeathSystems = 0;
    std::size_t spawnOnDeathWithoutSpawner = 0;
    std::size_t particleTextureBindings = 0;
    std::size_t particleShaderDescriptors = 0;
    std::size_t particleUnresolvedEmbedded = 0;
    std::size_t particleTextureTransformTracks = 0;
    std::size_t particleTextureFlipTracks = 0;
    std::vector<std::string> particleSystemDetails;
    std::vector<std::string> meshParticleDynamicDetails;
    std::size_t recoveredModels = 0;
    std::size_t partialModels = 0;

    // External texture verification is deliberately opt-in because the compact fixture corpus
    // does not ship a complete client asset tree. The final ResMap gate passes --asset-root.
    std::size_t externalTextureBindings = 0;
    std::size_t resolvedExternalTextureBindings = 0;
    std::size_t decodedExternalTextureBindings = 0;
    std::size_t missingExternalTextureBindings = 0;
    std::size_t ambiguousExternalTextureBindings = 0;
    std::size_t failedExternalTextureDecodeBindings = 0;
    std::size_t unsupportedExternalTextureDecodeBindings = 0;
    std::set<std::string> resolvedExternalTextureFiles;
    std::set<std::string> decodedExternalTextureFiles;
    std::vector<std::string> externalTextureGapDetails;
    ExternalTextureIndex externalTextureIndex;
    std::map<std::string, std::optional<std::string>> externalDecodeCache;

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
    std::vector<std::string> uvGapDetails;
    std::vector<std::string> applyModeDetails;
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
    std::size_t stencilPropertyParts = 0;
    std::size_t stencilEnabledParts = 0;
    std::size_t unsupportedStencilParts = 0;
    std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>, std::size_t> stencilStates;
    std::size_t unsupportedClampBindings = 0;
    std::size_t unsupportedFilterBindings = 0;
    std::size_t unsupportedTextureTransforms = 0;
    std::size_t unsupportedTransformAnimations = 0;
    std::size_t unsupportedFlipAnimations = 0;
    std::size_t unsupportedBlendParts = 0;
    std::size_t unsupportedAlphaTestParts = 0;
    std::size_t unsupportedDepthParts = 0;
    std::size_t unsupportedFaceDrawParts = 0;
    std::size_t unsupportedVertexColorParts = 0;
    std::size_t billboardParts = 0;
    std::map<std::uint16_t, std::size_t> billboardModes;
    std::size_t lodParts = 0;
    std::size_t skinnedParts = 0;
    std::size_t textureTransformTracks = 0;
    std::size_t textureFlipTracks = 0;
    std::vector<std::pair<std::string, std::string>> failures;

    for (const fs::path& root : roots) {
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
            if (model->recovered) {
                ++recoveredModels;
                rendererGapFiles.insert(entry.path().string());
            }
            if (model->partial) {
                ++partialModels;
                rendererGapFiles.insert(entry.path().string());
            }
            decodedEmbedded += model->decodedEmbeddedTextures;
            undecodedEmbedded += model->undecodedEmbeddedTextures;

            const auto auditExternalTexture = [&](const std::string& textureName,
                                                  const char* bindingKind) {
                if (textureName.empty()) return;
                ++externalTextureBindings;
                if (!verifyExternalTextures) return;

                const auto resolution =
                    externalTextureIndex.Resolve(entry.path(), textureName, assetRoots);
                if (!resolution.path) {
                    if (resolution.ambiguous) ++ambiguousExternalTextureBindings;
                    else ++missingExternalTextureBindings;
                    rendererGapFiles.insert(entry.path().string());
                    std::ostringstream detail;
                    detail << "TEXTUREGAP"
                           << "\tpath=" << Clean(entry.path().string())
                           << "\tbinding=" << bindingKind
                           << "\tsource=" << Clean(textureName)
                           << "\treason=" << (resolution.ambiguous ? "ambiguous" : "missing");
                    externalTextureGapDetails.push_back(detail.str());
                    return;
                }

                ++resolvedExternalTextureBindings;
                resolvedExternalTextureFiles.insert(resolution.path->string());
                const std::string decodeKey = resolution.path->string();
                auto cache = externalDecodeCache.find(decodeKey);
                if (cache == externalDecodeCache.end()) {
                    std::string ext = resolution.path->extension().string();
                    for (char& ch : ext)
                        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    std::optional<std::string> error;
                    if (ext == ".dds") {
                        const auto decoded = core::LoadDdsImage(*resolution.path);
                        if (!decoded) error = decoded.error();
                    } else if (ext == ".tga") {
                        const auto decoded = core::LoadTgaImage(*resolution.path);
                        if (!decoded) error = decoded.error();
                    } else {
                        error = "unsupported-core-decoder:" + ext;
                    }
                    cache = externalDecodeCache.emplace(decodeKey, std::move(error)).first;
                }

                if (!cache->second) {
                    ++decodedExternalTextureBindings;
                    decodedExternalTextureFiles.insert(decodeKey);
                    return;
                }

                rendererGapFiles.insert(entry.path().string());
                if (cache->second->rfind("unsupported-core-decoder:", 0) == 0)
                    ++unsupportedExternalTextureDecodeBindings;
                else
                    ++failedExternalTextureDecodeBindings;
                std::ostringstream detail;
                detail << "TEXTUREGAP"
                       << "\tpath=" << Clean(entry.path().string())
                       << "\tbinding=" << bindingKind
                       << "\tsource=" << Clean(textureName)
                       << "\tresolved=" << Clean(decodeKey)
                       << "\treason=" << Clean(*cache->second);
                externalTextureGapDetails.push_back(detail.str());
            };
            unsupportedEffects += model->textureEffectUnsupportedBlocks;
            inheritedProperties += model->inheritedPropertyBindings;
            for (const auto& controllerType : model->particleControllerTypes)
                ++particleControllerTypes[controllerType];
            if (model->particleSystemBlocks > 0u) {
                particleSystems += model->particleSystemBlocks;
                particleFiles[entry.path().string()] += model->particleSystemBlocks;

                // Gamebryo 2.6 conversion semantics for the old Fiesta particle stack:
                // - PositionModifier is absorbed by the simulator position/final step.
                // - BoundUpdateModifier becomes culling/bounds maintenance only.
                // - UpdateCtlr is deliberately discarded by NiPSConverter because the new
                //   particle system updates itself.
                // Everything else listed here has an explicit renderer/runtime implementation.
                static const std::set<std::string> kMaterializedParticleModifiers{
                    "NiPSysAgeDeathModifier",
                    "NiPSysBoundUpdateModifier",
                    "NiPSysBoxEmitter",
                    "NiPSysColorModifier",
                    "NiPSysCylinderEmitter",
                    "NiPSysDragModifier",
                    "NiPSysGravityModifier",
                    "NiPSysGrowFadeModifier",
                    "NiPSysMeshEmitter",
                    "NiPSysMeshUpdateModifier",
                    "NiPSysPositionModifier",
                    "NiPSysRotationModifier",
                    "NiPSysSpawnModifier",
                    "NiPSysSphereEmitter",
                };
                static const std::set<std::string> kMaterializedParticleControllers{
                    "NiPSysEmitterCtlr",
                    "NiPSysModifierActiveCtlr",
                    "NiPSysUpdateCtlr",
                };

                for (std::size_t systemIndex = 0; systemIndex < model->particleSystems.size(); ++systemIndex) {
                    const auto& system = model->particleSystems[systemIndex];
                    if (!system.hasParticleData) {
                        ++particleSystemsWithoutData;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    for (const auto& modifierType : system.modifierTypes) {
                        ++particleModifierTypes[modifierType];
                        if (modifierType == "<invalid>") {
                            ++invalidParticleModifierRefs;
                            ++unsupportedParticleModifierBindings;
                            rendererGapFiles.insert(entry.path().string());
                        } else if (!kMaterializedParticleModifiers.contains(modifierType)) {
                            ++unsupportedParticleModifierBindings;
                            rendererGapFiles.insert(entry.path().string());
                        }
                    }
                    for (const auto& modifier : system.modifiers) {
                        if (modifier.type == "NiPSysAgeDeathModifier" && modifier.spawnOnDeath) {
                            ++spawnOnDeathSystems;
                            const auto spawner = std::find_if(
                                system.modifiers.begin(), system.modifiers.end(),
                                [&](const auto& candidate) {
                                    return candidate.type == "NiPSysSpawnModifier" &&
                                           candidate.blockRef == modifier.spawnModifierRef;
                                });
                            if (spawner == system.modifiers.end()) {
                                ++spawnOnDeathWithoutSpawner;
                                rendererGapFiles.insert(entry.path().string());
                            }
                        }
                        if (modifier.type == "NiPSysColorModifier") {
                            if (modifier.colorDataRef >= 0 && !modifier.hasColorTrack) {
                                ++colorModifiersWithoutTrack;
                                rendererGapFiles.insert(entry.path().string());
                            }
                            if (modifier.hasColorTrack)
                                ++particleColorInterpolations[modifier.colorTrack.interpolation];
                        }
                        if (modifier.type == "NiPSysBoxEmitter" ||
                            modifier.type == "NiPSysCylinderEmitter" ||
                            modifier.type == "NiPSysSphereEmitter") {
                            if (modifier.emitterObjectRef >= 0 &&
                                !modifier.hasEmitterToParticleSystemTransform) {
                                ++volumeEmittersWithoutTransform;
                                rendererGapFiles.insert(entry.path().string());
                            }
                        }
                        if ((modifier.type == "NiPSysGravityModifier" ||
                             modifier.type == "NiPSysDragModifier") &&
                            modifier.forceObjectRef >= 0 &&
                            !modifier.hasForceToParticleSystemTransform) {
                            ++forceModifiersWithoutTransform;
                            rendererGapFiles.insert(entry.path().string());
                        }
                        if (modifier.type == "NiPSysMeshEmitter") {
                            if (!modifier.emitterMeshRefs.empty() && modifier.emitterMeshes.empty()) {
                                ++meshEmittersWithoutGeometry;
                                rendererGapFiles.insert(entry.path().string());
                            }
                            for (const auto& mesh : modifier.emitterMeshes) {
                                if (mesh.skinned) {
                                    ++skinnedMeshEmitterBindings;
                                    rendererGapFiles.insert(entry.path().string());
                                }
                            }
                        }
                    }
                    resolvedParticleControllers += system.controllers.size();
                    for (const auto& controller : system.controllers) {
                        if (!kMaterializedParticleControllers.contains(controller.type)) {
                            ++unsupportedParticleControllerBindings;
                            rendererGapFiles.insert(entry.path().string());
                        }
                        const auto auditBlend = [&](const auto& blend) {
                            if (!blend) return;
                            ++blendParticleControllerTracks;
                            if (blend->managerControlled) {
                                ++managerControlledParticleBlendTracks;
                                // Gamebryo 2.6: manager-controlled blend arrays are populated
                                // only by explicit NiControllerManager sequence activation.
                                // A streamed NIF registers its sequence data but does not
                                // activate it, so zero serialized items is a fully-defined
                                // dormant state for standalone map rendering.
                                if (blend->items.empty())
                                    ++dormantManagerControlledParticleBlendTracks;
                                else
                                    rendererGapFiles.insert(entry.path().string());
                            }
                            if (!blend->items.empty()) {
                                ++weightedParticleBlendTracks;
                                rendererGapFiles.insert(entry.path().string());
                            }
                        };
                        auditBlend(controller.floatBlend);
                        auditBlend(controller.boolBlend);
                        auditBlend(controller.visibilityBlend);
                        if (controller.type == "NiPSysEmitterCtlr") {
                            if (controller.interpolatorRef >= 0 && !controller.hasFloatTrack) {
                                ++emitterControllersWithoutRate;
                                rendererGapFiles.insert(entry.path().string());
                            }
                            if (controller.visibilityInterpolatorRef >= 0 && !controller.hasVisibilityTrack) {
                                ++emitterControllersWithoutVisibility;
                                rendererGapFiles.insert(entry.path().string());
                            }
                        } else if (controller.type == "NiPSysModifierActiveCtlr" && !controller.hasBoolTrack) {
                            ++activeControllersWithoutBoolTrack;
                            rendererGapFiles.insert(entry.path().string());
                        }
                    }
                    meshParticleSystems += system.meshParticles ? 1u : 0u;
                    if (system.meshParticles) {
                        if (system.meshParticleMasters.empty()) {
                            ++meshParticleSystemsWithoutMasters;
                            rendererGapFiles.insert(entry.path().string());
                        }
                        meshParticleMasterGenerations += system.meshParticleMasters.size();
                        for (const auto& master : system.meshParticleMasters) {
                            meshParticleMasterParts += master.partIndices.size();
                            if (master.partIndices.empty()) {
                                rendererGapFiles.insert(entry.path().string());
                                continue;
                            }
                            for (const auto partIndex : master.partIndices) {
                                if (partIndex >= model->parts.size()) {
                                    ++meshParticleMasterDynamicParts;
                                    rendererGapFiles.insert(entry.path().string());
                                    continue;
                                }
                                const auto& masterPart = model->parts[partIndex];
                                // Mesh-particle masters are cloned and updated at particle age.
                                // Billboard, TextureTransform and FlipController are now handled
                                // per clone by the renderer. Skinning and LOD still need their own
                                // age-local clone semantics and remain explicit fidelity gaps.
                                if (masterPart.skinned || masterPart.lodControlled) {
                                    ++meshParticleMasterDynamicParts;
                                    rendererGapFiles.insert(entry.path().string());
                                    std::ostringstream dynamicDetail;
                                    dynamicDetail << "MESHPARTICLEDYNAMIC"
                                                  << "\tpath=" << Clean(entry.path().string())
                                                  << "\tsystem=" << systemIndex
                                                  << "\tmasterBlock=" << master.blockRef
                                                  << "\tpart=" << partIndex
                                                  << "\tname=" << Clean(masterPart.name)
                                                  << "\tskinned=" << (masterPart.skinned ? 1 : 0)
                                                  << "\tbillboard=" << (masterPart.billboard ? 1 : 0)
                                                  << "\tlod=" << (masterPart.lodControlled ? 1 : 0)
                                                  << "\ttexTransformTracks="
                                                  << masterPart.textureTransformAnimations.size()
                                                  << "\tflipTracks="
                                                  << masterPart.textureFlipAnimations.size();
                                    meshParticleDynamicDetails.push_back(dynamicDetail.str());
                                }
                            }
                        }
                    }
                    worldSpaceParticleSystems += system.worldSpace ? 1u : 0u;
                    if (system.hasParticleData) {
                        particleCapacity += system.particleData.capacity;
                        activeParticles += system.particleData.activeCount;
                        if (system.particleData.hasRotationSpeeds) ++particleRotationSpeedSystems;
                    }
                    const bool textured = system.textureSlots[0].present &&
                        (!system.textureSlots[0].texture.empty() || system.textureSlots[0].embeddedTexture);
                    texturedParticleSystems += textured ? 1u : 0u;
                    for (const auto& texture : system.textureSlots) {
                        if (!texture.present) continue;
                        ++particleTextureBindings;
                        if (texture.sourceUsesEmbeddedPixelData && !texture.embeddedTexture) {
                            ++particleUnresolvedEmbedded;
                            rendererGapFiles.insert(entry.path().string());
                        } else if (!texture.sourceUsesEmbeddedPixelData && !texture.texture.empty()) {
                            auditExternalTexture(texture.texture, "particle-slot");
                        }
                        if (texture.clampMode > 3u) { ++unsupportedClampBindings; rendererGapFiles.insert(entry.path().string()); }
                        if (texture.filterMode > 6u) { ++unsupportedFilterBindings; rendererGapFiles.insert(entry.path().string()); }
                        if (texture.hasTransform && texture.transformType > core::kNifTextureTransformMaya) {
                            ++unsupportedTextureTransforms;
                            rendererGapFiles.insert(entry.path().string());
                        }
                    }
                    for (const auto& shaderSlot : system.shaderTextureSlots) {
                        ++particleShaderDescriptors;
                        if (system.shaderName != "VCAlphaTextureBlender" || shaderSlot.mapId > 2u) {
                            ++unmaterializedShaderDescriptors;
                            rendererGapFiles.insert(entry.path().string());
                        }
                        if (shaderSlot.texture.sourceUsesEmbeddedPixelData && !shaderSlot.texture.embeddedTexture) {
                            ++particleUnresolvedEmbedded;
                            rendererGapFiles.insert(entry.path().string());
                        } else if (!shaderSlot.texture.sourceUsesEmbeddedPixelData &&
                                   !shaderSlot.texture.texture.empty()) {
                            auditExternalTexture(shaderSlot.texture.texture, "particle-shader-slot");
                        }
                    }
                    particleTextureTransformTracks += system.textureTransformAnimations.size();
                    particleTextureFlipTracks += system.textureFlipAnimations.size();
                    for (const auto& animation : system.textureTransformAnimations) {
                        if (animation.slot >= system.textureSlots.size() || animation.operation > 4u) {
                            ++unsupportedTransformAnimations;
                            rendererGapFiles.insert(entry.path().string());
                        }
                    }
                    for (const auto& animation : system.textureFlipAnimations) {
                        if (animation.slot >= system.textureSlots.size() || animation.frames.empty()) {
                            ++unsupportedFlipAnimations;
                            rendererGapFiles.insert(entry.path().string());
                        }
                        for (const auto& frame : animation.frames) {
                            if (!frame.sourceUsesEmbeddedPixelData && !frame.texture.empty())
                                auditExternalTexture(frame.texture, "particle-flip-frame");
                        }
                    }
                    if (system.textureApplyMode > 2u) {
                        ++unmaterializedApplyModeParts;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (system.alphaBlend && (system.alphaSrcBlend > 10u || system.alphaDstBlend > 10u)) {
                        ++unsupportedBlendParts; rendererGapFiles.insert(entry.path().string());
                    }
                    if (system.alphaTest && system.alphaTestFunc > 7u) {
                        ++unsupportedAlphaTestParts; rendererGapFiles.insert(entry.path().string());
                    }
                    if (system.depthTest && system.depthFunction > 7u) {
                        ++unsupportedDepthParts; rendererGapFiles.insert(entry.path().string());
                    }
                    if (system.stencilEnabled &&
                        (system.stencilFunction > 7u || system.stencilFailAction > 5u ||
                         system.stencilZFailAction > 5u || system.stencilPassAction > 5u)) {
                        ++unsupportedStencilParts; rendererGapFiles.insert(entry.path().string());
                    }
                    if (system.hasVertexColorProperty &&
                        (system.vertexColorMode > 2u || system.vertexLightingMode > 1u)) {
                        ++unsupportedVertexColorParts; rendererGapFiles.insert(entry.path().string());
                    }
                    if (system.faceDrawMode > 3u) {
                        ++unsupportedFaceDrawParts; rendererGapFiles.insert(entry.path().string());
                    }
                    std::ostringstream detail;
                    detail << "PARTICLESYSTEM"
                           << "\tpath=" << Clean(entry.path().string())
                           << "\tindex=" << systemIndex
                           << "\tname=" << Clean(system.name)
                           << "\tmesh=" << (system.meshParticles ? 1 : 0)
                           << "\tworldSpace=" << (system.worldSpace ? 1 : 0)
                           << "\tdata=" << (system.hasParticleData ? 1 : 0)
                           << "\tcapacity=" << (system.hasParticleData ? system.particleData.capacity : 0u)
                           << "\tactive=" << (system.hasParticleData ? system.particleData.activeCount : 0u)
                           << "\ttextured=" << (textured ? 1 : 0)
                           << "\tcontrollers=";
                    for (std::size_t ci = 0; ci < system.controllers.size(); ++ci) {
                        if (ci != 0) detail << ',';
                        const auto& controller = system.controllers[ci];
                        detail << Clean(controller.type);
                        if (controller.type == "NiPSysEmitterCtlr") {
                            detail << "[rate=" << (controller.hasFloatTrack ? 1 : 0)
                                   << ";rateType=" << Clean(controller.interpolatorType)
                                   << ";rateBlend=" << (controller.floatBlend ? 1 : 0)
                                   << ";vis=" << (controller.hasVisibilityTrack ? 1 : 0)
                                   << ";visType=" << Clean(controller.visibilityInterpolatorType)
                                   << ";visBlend=" << (controller.visibilityBlend ? 1 : 0) << ']';
                        }
                    }
                    detail << "\tmodifiers=";
                    for (std::size_t mi = 0; mi < system.modifierTypes.size(); ++mi) {
                        if (mi != 0) detail << ',';
                        detail << Clean(system.modifierTypes[mi]);
                    }
                    particleSystemDetails.push_back(detail.str());
                }
            }

            for (std::size_t partIndex = 0; partIndex < model->parts.size(); ++partIndex) {
                const auto& part = model->parts[partIndex];
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
                if (part.textureApplyMode > 2u) {
                    ++unmaterializedApplyModeParts;
                    rendererGapFiles.insert(entry.path().string());
                    std::ostringstream detail;
                    detail << "APPLYMODEDETAIL"
                           << "\tpath=" << Clean(entry.path().string())
                           << "\tpart=" << partIndex
                           << "\tmode=" << part.textureApplyMode
                           << "\tname=" << ApplyModeName(part.textureApplyMode)
                           << "\tshader=" << Clean(shader)
                           << "\tvertices=" << part.positions.size()
                           << "\ttriangles=" << (part.triangleIndices.size() / 3u)
                           << "\tuvSets=" << part.uvSets.size()
                           << "\talphaBlend=" << (part.alphaBlend ? 1 : 0)
                           << "\talphaTest=" << (part.alphaTest ? 1 : 0)
                           << "\tspecular=" << (part.specularEnabled ? 1 : 0)
                           << "\tslots=";
                    bool firstSlot = true;
                    for (std::size_t slot = 0; slot < part.textureSlots.size(); ++slot) {
                        const auto& tex = part.textureSlots[slot];
                        if (!tex.present) continue;
                        if (!firstSlot) detail << ',';
                        detail << slot
                               << "{src=" << Clean(tex.texture)
                               << ";uv=" << tex.uvSet
                               << ";clamp=" << tex.clampMode
                               << ";filter=" << tex.filterMode
                               << ";embedded=" << (tex.embeddedTexture ? 1 : 0)
                               << '}';
                        firstSlot = false;
                    }
                    applyModeDetails.push_back(detail.str());
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
                if (part.hasVertexColorProperty && (part.vertexColorMode > 2u || part.vertexLightingMode > 1u)) {
                    ++unsupportedVertexColorParts;
                    rendererGapFiles.insert(entry.path().string());
                }
                if (part.faceDrawMode > 3u) {
                    ++unsupportedFaceDrawParts;
                    rendererGapFiles.insert(entry.path().string());
                }
                if (part.alphaBlend && (part.alphaSrcBlend > 10u || part.alphaDstBlend > 10u)) {
                    ++unsupportedBlendParts;
                    rendererGapFiles.insert(entry.path().string());
                }
                if (part.alphaTest && part.alphaTestFunc > 7u) {
                    ++unsupportedAlphaTestParts;
                    rendererGapFiles.insert(entry.path().string());
                }
                if (part.depthTest && part.depthFunction > 7u) {
                    ++unsupportedDepthParts;
                    rendererGapFiles.insert(entry.path().string());
                }
                if (part.billboard) {
                    ++billboardParts;
                    ++billboardModes[part.billboardMode];
                }
                lodParts += part.lodControlled ? 1u : 0u;
                skinnedParts += part.skinned ? 1u : 0u;
                if (part.hasStencilProperty) {
                    ++stencilPropertyParts;
                    stencilEnabledParts += part.stencilEnabled ? 1u : 0u;
                    ++stencilStates[{part.stencilFunction, part.stencilFailAction,
                                     part.stencilZFailAction, part.stencilPassAction}];
                    if (part.stencilEnabled &&
                        (part.stencilFunction > 7u || part.stencilFailAction > 5u ||
                         part.stencilZFailAction > 5u || part.stencilPassAction > 5u)) {
                        ++unsupportedStencilParts;
                        rendererGapFiles.insert(entry.path().string());
                    }
                }
                textureTransformTracks += part.textureTransformAnimations.size();
                textureFlipTracks += part.textureFlipAnimations.size();
                for (const auto& anim : part.textureTransformAnimations) {
                    if (anim.slot >= part.textureSlots.size() || anim.operation > 4u) {
                        ++unsupportedTransformAnimations;
                        rendererGapFiles.insert(entry.path().string());
                    }
                }
                for (const auto& anim : part.textureFlipAnimations) {
                    if (anim.slot >= part.textureSlots.size()) {
                        ++unsupportedFlipAnimations;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    for (const auto& frame : anim.frames) {
                        if (!frame.sourceUsesEmbeddedPixelData && !frame.texture.empty())
                            auditExternalTexture(frame.texture, "flip-frame");
                    }
                }

                const auto appendUvDiagnostic = [&](std::ostringstream& out, std::uint32_t uvSet) {
                    if (uvSet >= part.uvSetDiagnostics.size()) return;
                    const auto& diagnostic = part.uvSetDiagnostics[uvSet];
                    out << "\trawUvCount=" << diagnostic.originalCount
                        << "\tuvDiscarded=" << (diagnostic.discarded ? 1 : 0)
                        << "\textremeCount=" << diagnostic.extremeCount
                        << "\tminU=" << diagnostic.minFiniteU
                        << "\tmaxU=" << diagnostic.maxFiniteU
                        << "\tminV=" << diagnostic.minFiniteV
                        << "\tmaxV=" << diagnostic.maxFiniteV
                        << "\tmaxFiniteAbs=" << diagnostic.maxFiniteAbs;
                    if (diagnostic.discarded) {
                        out << "\tfirstBadIndex=" << diagnostic.firstBadIndex
                            << "\tfirstBadU=" << diagnostic.firstBadValue.u
                            << "\tfirstBadV=" << diagnostic.firstBadValue.v
                            << "\tnonFinite=" << (diagnostic.nonFinite ? 1 : 0);
                    }
                };

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
                        auditExternalTexture(texture.texture, "classic-slot");
                    }
                    ++classicClampModes[texture.clampMode];
                    ++classicFilterModes[texture.filterMode];
                    if (texture.clampMode > 3u) {
                        ++unsupportedClampBindings;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (texture.filterMode > 6u) {
                        ++unsupportedFilterBindings;
                        unusualFilterFiles[texture.filterMode].insert(entry.path().string());
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (texture.hasTransform && texture.transformType > core::kNifTextureTransformMaya) {
                        ++unsupportedTextureTransforms;
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
                        std::ostringstream detail;
                        detail << "UVGAP"
                               << "\tpath=" << Clean(entry.path().string())
                               << "\tpart=" << partIndex
                               << "\tslot=" << slot
                               << "\trequestedUv=" << texture.uvSet
                               << "\tbaseUv=" << part.baseUvSet
                               << "\tuvSets=" << part.uvSets.size()
                               << "\tvertices=" << part.positions.size()
                               << "\ttriangles=" << (part.triangleIndices.size() / 3u)
                               << "\tfallback=" << (hasBaseFallbackUvs ? "uv0" : "none")
                               << "\tclampMode=" << texture.clampMode
                               << "\tfilterMode=" << texture.filterMode
                               << "\tshader=" << Clean(shader)
                               << "\tsource=" << Clean(texture.texture);
                        appendUvDiagnostic(detail, texture.uvSet);
                        uvGapDetails.push_back(detail.str());
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
                    if (shaderSlot.texture.clampMode > 3u) {
                        ++unsupportedClampBindings;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (shaderSlot.texture.filterMode > 6u) {
                        ++unsupportedFilterBindings;
                        unusualFilterFiles[shaderSlot.texture.filterMode].insert(entry.path().string());
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (shaderSlot.texture.hasTransform &&
                        shaderSlot.texture.transformType > core::kNifTextureTransformMaya) {
                        ++unsupportedTextureTransforms;
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
                            std::ostringstream detail;
                            detail << "UVGAP"
                                   << "\tpath=" << Clean(entry.path().string())
                                   << "\tpart=" << partIndex
                                   << "\tshaderMap=" << shaderSlot.mapId
                                   << "\trequestedUv=" << shaderSlot.texture.uvSet
                                   << "\tbaseUv=" << part.baseUvSet
                                   << "\tuvSets=" << part.uvSets.size()
                                   << "\tvertices=" << part.positions.size()
                                   << "\ttriangles=" << (part.triangleIndices.size() / 3u)
                                   << "\tfallback=" << (hasBaseFallbackUvs ? "uv0" : "none")
                                   << "\tclampMode=" << shaderSlot.texture.clampMode
                                   << "\tfilterMode=" << shaderSlot.texture.filterMode
                                   << "\tshader=" << Clean(shader)
                                   << "\tsource=" << Clean(shaderSlot.texture.texture);
                            appendUvDiagnostic(detail, shaderSlot.texture.uvSet);
                            uvGapDetails.push_back(detail.str());
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
                        auditExternalTexture(texture.texture, "shader-slot");
                    }
                }

                for (const auto& effect : part.textureEffects) {
                    auto& stat = effects[{effect.textureType, effect.coordGenType, effect.enabled}];
                    ++stat.bindings;
                    stat.files.insert(entry.path().string());
                    stat.clampModes.insert(effect.clampMode);
                    stat.filterModes.insert(effect.filterMode);
                    if (effect.clampMode > 3u) {
                        ++unsupportedClampBindings;
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (effect.filterMode > 6u) {
                        ++unsupportedFilterBindings;
                        unusualFilterFiles[effect.filterMode].insert(entry.path().string());
                        rendererGapFiles.insert(entry.path().string());
                    }
                    if (effect.sourceUsesEmbeddedPixelData) {
                        ++stat.embedded;
                        if (!effect.embeddedTexture) ++stat.unresolvedEmbedded;
                    } else if (!effect.texture.empty()) {
                        ++stat.external;
                        auditExternalTexture(effect.texture, "texture-effect");
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
              << "\tparticleSystems=" << particleSystems
              << "\tparticleFiles=" << particleFiles.size()
              << "\tinvalidParticleModifierRefs=" << invalidParticleModifierRefs
              << "\tparticleCapacity=" << particleCapacity
              << "\tactiveParticles=" << activeParticles
              << "\ttexturedParticleSystems=" << texturedParticleSystems
              << "\tmeshParticleSystems=" << meshParticleSystems
              << "\tmeshParticleSystemsWithoutMasters=" << meshParticleSystemsWithoutMasters
              << "\tmeshParticleMasterGenerations=" << meshParticleMasterGenerations
              << "\tmeshParticleMasterParts=" << meshParticleMasterParts
              << "\tworldSpaceParticleSystems=" << worldSpaceParticleSystems
              << "\tresolvedParticleControllers=" << resolvedParticleControllers
              << "\tparticleSystemsWithoutData=" << particleSystemsWithoutData
              << "\tunsupportedParticleModifierBindings=" << unsupportedParticleModifierBindings
              << "\tunsupportedParticleControllerBindings=" << unsupportedParticleControllerBindings
              << "\tmeshParticleMasterDynamicParts=" << meshParticleMasterDynamicParts
              << "\temitterControllersWithoutRate=" << emitterControllersWithoutRate
              << "\temitterControllersWithoutVisibility=" << emitterControllersWithoutVisibility
              << "\tactiveControllersWithoutBoolTrack=" << activeControllersWithoutBoolTrack
              << "\tblendParticleControllerTracks=" << blendParticleControllerTracks
              << "\tmanagerControlledParticleBlendTracks=" << managerControlledParticleBlendTracks
              << "\tdormantManagerControlledParticleBlendTracks="
              << dormantManagerControlledParticleBlendTracks
              << "\tweightedParticleBlendTracks=" << weightedParticleBlendTracks
              << "\tcolorModifiersWithoutTrack=" << colorModifiersWithoutTrack
              << "\tvolumeEmittersWithoutTransform=" << volumeEmittersWithoutTransform
              << "\tmeshEmittersWithoutGeometry=" << meshEmittersWithoutGeometry
              << "\tskinnedMeshEmitterBindings=" << skinnedMeshEmitterBindings
              << "\tforceModifiersWithoutTransform=" << forceModifiersWithoutTransform
              << "\tparticleRotationSpeedSystems=" << particleRotationSpeedSystems
              << "\tspawnOnDeathSystems=" << spawnOnDeathSystems
              << "\tspawnOnDeathWithoutSpawner=" << spawnOnDeathWithoutSpawner
              << "\tparticleTextureBindings=" << particleTextureBindings
              << "\tparticleShaderDescriptors=" << particleShaderDescriptors
              << "\tparticleUnresolvedEmbedded=" << particleUnresolvedEmbedded
              << "\tparticleTextureTransformTracks=" << particleTextureTransformTracks
              << "\tparticleTextureFlipTracks=" << particleTextureFlipTracks
              << "\trecovered=" << recoveredModels
              << "\tpartial=" << partialModels
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
              << "\tunsupportedClampBindings=" << unsupportedClampBindings
              << "\tunsupportedFilterBindings=" << unsupportedFilterBindings
              << "\tunsupportedTextureTransforms=" << unsupportedTextureTransforms
              << "\tunsupportedTransformAnimations=" << unsupportedTransformAnimations
              << "\tunsupportedFlipAnimations=" << unsupportedFlipAnimations
              << "\tunsupportedBlendParts=" << unsupportedBlendParts
              << "\tunsupportedAlphaTestParts=" << unsupportedAlphaTestParts
              << "\tunsupportedDepthParts=" << unsupportedDepthParts
              << "\tunsupportedFaceDrawParts=" << unsupportedFaceDrawParts
              << "\tunsupportedVertexColorParts=" << unsupportedVertexColorParts
              << "\texternalTextureBindings=" << externalTextureBindings
              << "\tresolvedExternalTextureBindings=" << resolvedExternalTextureBindings
              << "\tdecodedExternalTextureBindings=" << decodedExternalTextureBindings
              << "\tresolvedExternalTextureFiles=" << resolvedExternalTextureFiles.size()
              << "\tdecodedExternalTextureFiles=" << decodedExternalTextureFiles.size()
              << "\tmissingExternalTextureBindings=" << missingExternalTextureBindings
              << "\tambiguousExternalTextureBindings=" << ambiguousExternalTextureBindings
              << "\tfailedExternalTextureDecodeBindings=" << failedExternalTextureDecodeBindings
              << "\tunsupportedExternalTextureDecodeBindings="
              << unsupportedExternalTextureDecodeBindings
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
        if (mode > 2u) std::cout << "\trenderer=modulate-fallback";
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
    for (const auto& detail : uvGapDetails)
        std::cout << detail << '\n';
    for (const auto& [method, partCount] : transformMethods)
        std::cout << "TEXTRANSFORM\tmethod=" << method << "\tparts=" << partCount << '\n';
    for (const auto& [mode, bindingCount] : classicClampModes)
        std::cout << "CLAMPMODE\tmode=" << mode << "\tbindings=" << bindingCount << '\n';
    for (const auto& [mode, bindingCount] : classicFilterModes) {
        std::cout << "FILTERMODE\tmode=" << mode << "\tbindings=" << bindingCount;
        if (mode > 6u) std::cout << "\trenderer=trilinear-fallback";
        std::cout << '\n';
        if (mode > 6u)
            for (const auto& path : unusualFilterFiles[mode])
                std::cout << "FILTERMODEFILE\tmode=" << mode
                          << "\tpath=" << Clean(path) << '\n';
    }
    for (const auto& [mode, partCount] : applyModes) {
        std::cout << "APPLYMODE\tmode=" << mode
                  << "\tname=" << ApplyModeName(mode)
                  << "\tparts=" << partCount;
        if (mode > 2u) std::cout << "\trenderer=modulate-fallback";
        std::cout << '\n';
        if (mode != 2u) {
            for (const auto& path : applyModeFiles[mode])
                std::cout << "APPLYMODEFILE\tmode=" << mode
                          << "\tname=" << ApplyModeName(mode)
                          << "\tpath=" << Clean(path) << '\n';
        }
    }
    for (const auto& detail : applyModeDetails)
        std::cout << detail << '\n';
    for (const auto& [type, count] : particleModifierTypes)
        std::cout << "PARTICLEMODIFIER\ttype=" << Clean(type)
                  << "\tbindings=" << count << '\n';
    for (const auto& [type, count] : particleControllerTypes)
        std::cout << "PARTICLECTLR\ttype=" << Clean(type)
                  << "\tbindings=" << count << '\n';
    for (const auto& [mode, count] : particleColorInterpolations)
        std::cout << "PARTICLECOLOR\tinterpolation=" << mode
                  << "\ttracks=" << count << '\n';
    for (const auto& [path, count] : particleFiles) {
        (void)count;
    }
    for (const auto& detail : particleSystemDetails)
        std::cout << detail << '\n';
    for (const auto& detail : meshParticleDynamicDetails)
        std::cout << detail << '\n';
    for (const auto& [path, count] : particleFiles)
        std::cout << "PARTICLEFILE\tsystems=" << count
                  << "\trenderer="
                  << (rendererGapFiles.contains(path) ? "gap" : "materialized")
                  << "\tpath=" << Clean(path) << '\n';
    for (const auto& [mode, partCount] : vertexColorModes)
        std::cout << "VERTEXCOLOR\tmode=" << mode << "\tparts=" << partCount << '\n';
    for (const auto& [mode, partCount] : faceDrawModes)
        std::cout << "FACEDRAW\tmode=" << mode << "\tparts=" << partCount
                  << "\trenderer=" << (mode <= 3u ? "materialized" : "fallback") << '\n';
    for (const auto& [mode, partCount] : billboardModes)
        std::cout << "BILLBOARD\tmode=" << mode << "\tparts=" << partCount << '\n';
    for (const auto& [key, partCount] : stencilStates) {
        const auto [function, failAction, zFailAction, passAction] = key;
        std::cout << "STENCIL\tfunction=" << function
                  << "\tfail=" << failAction
                  << "\tzfail=" << zFailAction
                  << "\tpass=" << passAction
                  << "\tparts=" << partCount
                  << "\trenderer="
                  << ((function <= 7u && failAction <= 5u && zFailAction <= 5u && passAction <= 5u)
                          ? "materialized" : "fallback")
                  << '\n';
    }

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

    for (const auto& detail : externalTextureGapDetails)
        std::cout << detail << '\n';
    for (const auto& path : rendererGapFiles)
        std::cout << "RENDERGAPFILE\tpath=" << Clean(path) << '\n';

    std::cout << "STATE\talphaBlendParts=" << alphaBlendParts
              << "\talphaTestParts=" << alphaTestParts
              << "\tdepthTestDisabledParts=" << depthTestDisabledParts
              << "\tdepthWriteDisabledParts=" << depthWriteDisabledParts
              << "\tstencilPropertyParts=" << stencilPropertyParts
              << "\tstencilEnabledParts=" << stencilEnabledParts
              << "\tunsupportedStencilParts=" << unsupportedStencilParts
              << "\tbillboardParts=" << billboardParts
              << "\tlodParts=" << lodParts
              << "\tskinnedParts=" << skinnedParts
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
    if (strictRenderer && !rendererGapFiles.empty()) {
        std::cerr << "Strict renderer audit failed: " << rendererGapFiles.size()
                  << " NIF file(s) still contain renderer-fidelity gaps.\n";
        return 1;
    }
    return 0;
}
