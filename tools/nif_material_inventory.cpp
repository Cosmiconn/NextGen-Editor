#include "mapeditor/core/NifModel.hpp"

#include <array>
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
    std::array<SlotStats, 10> slots{};
    std::map<std::size_t, std::size_t> uvSetCounts;
    std::map<std::uint32_t, std::size_t> transformMethods;
    std::map<std::uint32_t, std::size_t> applyModes;
    std::map<std::uint32_t, std::set<std::string>> applyModeFiles;
    std::map<std::uint32_t, std::size_t> vertexColorModes;
    std::map<std::uint32_t, std::size_t> faceDrawModes;
    std::map<std::tuple<std::uint32_t, std::uint32_t, bool>, std::size_t> effects;
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
                shaderFiles[shader].insert(entry.path().filename().string());
                ++uvSetCounts[part.uvSets.size()];
                ++applyModes[part.textureApplyMode];
                applyModeFiles[part.textureApplyMode].insert(entry.path().string());
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
                    if (texture.hasTransform) ++transformMethods[texture.transformType];
                }

                for (const auto& shaderSlot : part.shaderTextureSlots) {
                    auto& stat = shaderSlots[{shader, shaderSlot.mapId}];
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
                    ++effects[{effect.textureType, effect.coordGenType, effect.enabled}];
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
              << "\tinheritedProperties=" << inheritedProperties << '\n';

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
    for (const auto& [method, partCount] : transformMethods)
        std::cout << "TEXTRANSFORM\tmethod=" << method << "\tparts=" << partCount << '\n';
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

    for (const auto& [key, count] : effects) {
        const auto [textureType, coordGenType, enabled] = key;
        std::cout << "EFFECT\ttextureType=" << textureType
                  << "\tcoordGenType=" << coordGenType
                  << "\tenabled=" << (enabled ? 1 : 0)
                  << "\tbindings=" << count << '\n';
    }

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
