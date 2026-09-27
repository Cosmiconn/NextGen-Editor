#include "mapeditor/core/NifModel.hpp"
#include <filesystem>
#include <iostream>
#include <string>

// One process per input: the parent harness owns the hard OS timeout.
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const auto result = theseed::mapeditor::core::LoadNifMesh(
            std::filesystem::path(argv[2]), std::string(argv[1]) == "--recovery");
        if (!result) {
            std::string error = result.error();
            for (char& c : error) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
            std::cout << "ERROR\t0\t0\t0\t0\t" << error << '\n';
            return 1;
        }
        std::size_t parts = 0, vertices = 0, triangles = 0, textures = 0;
        for (const auto& part : result->parts) {
            if (!part.positions.empty() && part.triangleIndices.size() >= 3) ++parts;
            vertices += part.positions.size();
            triangles += part.triangleIndices.size() / 3;
            if (part.embeddedDiffuseTexture) ++textures;
        }
        std::cout << (result->partial ? "RECOVERY_PARTIAL" : parts ? "OK_GEOMETRY" : "OK_NO_GEOMETRY") << '\t'
                  << parts << '\t' << vertices << '\t' << triangles << '\t' << textures << "\t\n";

        if (std::string(argv[1]) == "--inspect") {
            for (std::size_t partIndex = 0; partIndex < result->parts.size(); ++partIndex) {
                const auto& part = result->parts[partIndex];
                std::cout << "PART\tindex=" << partIndex
                          << "\tshader=" << part.shaderName
                          << "\tvertices=" << part.positions.size()
                          << "\ttriangles=" << (part.triangleIndices.size() / 3u)
                          << "\tuvSets=" << part.uvSets.size()
                          << "\tuvDiagnostics=" << part.uvSetDiagnostics.size()
                          << "\tvertexColors=" << part.vertexColors.size()
                          << "\tapplyMode=" << part.textureApplyMode
                          << "\tmaterialAlpha=" << part.material.alpha
                          << "\talphaBlend=" << part.alphaBlend
                          << "\talphaTest=" << part.alphaTest
                          << "\tdepthTest=" << part.depthTest
                          << "\tdepthWrite=" << part.depthWrite << '\n';
                if (!part.vertexColors.empty()) {
                    float minA = 1.0f, maxA = 0.0f, sumA = 0.0f;
                    for (const auto& color : part.vertexColors) {
                        minA = std::min(minA, color.a);
                        maxA = std::max(maxA, color.a);
                        sumA += color.a;
                    }
                    std::cout << "VCSTAT\tpart=" << partIndex
                              << "\taMin=" << minA
                              << "\taMax=" << maxA
                              << "\taMean=" << (sumA / part.vertexColors.size()) << '\n';
                }
                for (std::size_t uvIndex = 0; uvIndex < part.uvSetDiagnostics.size(); ++uvIndex) {
                    const auto& diag = part.uvSetDiagnostics[uvIndex];
                    std::cout << "UVDIAG\tpart=" << partIndex
                              << "\tset=" << uvIndex
                              << "\trawCount=" << diag.originalCount
                              << "\tdiscarded=" << diag.discarded
                              << "\tnonFinite=" << diag.nonFinite
                              << "\textreme=" << diag.extremeCount << '\n';
                }
                for (std::size_t slot = 0; slot < part.textureSlots.size(); ++slot) {
                    const auto& tex = part.textureSlots[slot];
                    if (!tex.present) continue;
                    std::cout << "TEX\tpart=" << partIndex
                              << "\tslot=" << slot
                              << "\tuv=" << tex.uvSet
                              << "\texternal=" << (!tex.sourceUsesEmbeddedPixelData)
                              << "\tsource=" << tex.texture << '\n';
                    if (tex.embeddedTexture && !tex.embeddedTexture->rgba.empty()) {
                        const auto& image = *tex.embeddedTexture;
                        std::uint64_t sums[4]{0,0,0,0};
                        std::uint32_t mins[4]{255,255,255,255};
                        std::uint32_t maxs[4]{0,0,0,0};
                        const std::size_t pixels = image.rgba.size() / 4u;
                        for (std::size_t px = 0; px < pixels; ++px) {
                            for (std::size_t ch = 0; ch < 4; ++ch) {
                                const auto value = static_cast<std::uint32_t>(image.rgba[px*4u+ch]);
                                sums[ch] += value;
                                if (value < mins[ch]) mins[ch] = value;
                                if (value > maxs[ch]) maxs[ch] = value;
                            }
                        }
                        std::cout << "EMBEDSTAT\tpart=" << partIndex
                                  << "\tkind=classic\tid=" << slot
                                  << "\twidth=" << image.width
                                  << "\theight=" << image.height
                                  << "\trMean=" << (pixels ? static_cast<double>(sums[0])/pixels : 0.0)
                                  << "\tgMean=" << (pixels ? static_cast<double>(sums[1])/pixels : 0.0)
                                  << "\tbMean=" << (pixels ? static_cast<double>(sums[2])/pixels : 0.0)
                                  << "\taMean=" << (pixels ? static_cast<double>(sums[3])/pixels : 0.0)
                                  << "\taMin=" << mins[3] << "\taMax=" << maxs[3] << '\n';
                    }
                }
                for (const auto& tex : part.shaderTextureSlots) {
                    std::cout << "SHADERTEX\tpart=" << partIndex
                              << "\tmapId=" << tex.mapId
                              << "\tuv=" << tex.texture.uvSet
                              << "\texternal=" << (!tex.texture.sourceUsesEmbeddedPixelData)
                              << "\tsource=" << tex.texture.texture << '\n';
                    if (tex.texture.embeddedTexture && !tex.texture.embeddedTexture->rgba.empty()) {
                        const auto& image = *tex.texture.embeddedTexture;
                        std::uint64_t sums[4]{0,0,0,0};
                        std::uint32_t mins[4]{255,255,255,255};
                        std::uint32_t maxs[4]{0,0,0,0};
                        const std::size_t pixels = image.rgba.size() / 4u;
                        for (std::size_t px = 0; px < pixels; ++px) {
                            for (std::size_t ch = 0; ch < 4; ++ch) {
                                const auto value = static_cast<std::uint32_t>(image.rgba[px*4u+ch]);
                                sums[ch] += value;
                                if (value < mins[ch]) mins[ch] = value;
                                if (value > maxs[ch]) maxs[ch] = value;
                            }
                        }
                        std::cout << "EMBEDSTAT\tpart=" << partIndex
                                  << "\tkind=shader\tid=" << tex.mapId
                                  << "\twidth=" << image.width
                                  << "\theight=" << image.height
                                  << "\trMean=" << (pixels ? static_cast<double>(sums[0])/pixels : 0.0)
                                  << "\tgMean=" << (pixels ? static_cast<double>(sums[1])/pixels : 0.0)
                                  << "\tbMean=" << (pixels ? static_cast<double>(sums[2])/pixels : 0.0)
                                  << "\taMean=" << (pixels ? static_cast<double>(sums[3])/pixels : 0.0)
                                  << "\taMin=" << mins[3] << "\taMax=" << maxs[3] << '\n';
                    }
                }
            }
            for (std::size_t systemIndex = 0; systemIndex < result->particleSystems.size(); ++systemIndex) {
                const auto& system = result->particleSystems[systemIndex];
                std::cout << "PSYS\tindex=" << systemIndex
                          << "\tname=" << system.name
                          << "\tshader=" << system.shaderName
                          << "\tworldSpace=" << system.worldSpace
                          << "\tmodifiers=" << system.modifiers.size() << '\n';
                for (const auto& modifier : system.modifiers) {
                    std::cout << "MOD\tsystem=" << systemIndex
                              << "\tblock=" << modifier.blockRef
                              << "\ttype=" << modifier.type
                              << "\tname=" << modifier.name
                              << "\tactive=" << modifier.active << '\n';
                    for (const auto& collider : modifier.colliders) {
                        std::cout << "COLLIDER\tsystem=" << systemIndex
                                  << "\tblock=" << collider.blockRef
                                  << "\ttype=" << collider.type
                                  << "\tbounce=" << collider.bounce
                                  << "\tspawn=" << collider.spawnOnCollide
                                  << "\tdie=" << collider.dieOnCollide
                                  << "\tspawnModifier=" << collider.spawnModifierRef
                                  << "\tobject=" << collider.colliderObjectRef
                                  << "\ttransform=" << collider.hasColliderToParticleSystemTransform
                                  << "\twidth=" << collider.width
                                  << "\theight=" << collider.height
                                  << "\tradius=" << collider.radius << '\n';
                    }
                }
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cout << "ERROR\t0\t0\t0\t0\tException: " << e.what() << '\n';
        return 1;
    }
}
