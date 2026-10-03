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
                theseed::mapeditor::core::NifVec3 boundsMin{}, boundsMax{};
                if (!part.positions.empty()) {
                    boundsMin = boundsMax = part.positions.front();
                    for (const auto& p : part.positions) {
                        boundsMin.x = std::min(boundsMin.x, p.x);
                        boundsMin.y = std::min(boundsMin.y, p.y);
                        boundsMin.z = std::min(boundsMin.z, p.z);
                        boundsMax.x = std::max(boundsMax.x, p.x);
                        boundsMax.y = std::max(boundsMax.y, p.y);
                        boundsMax.z = std::max(boundsMax.z, p.z);
                    }
                }
                std::cout << "PART\tindex=" << partIndex
                          << "\tname=" << part.name
                          << "\tshader=" << part.shaderName
                          << "\tshaderExtraData=" << part.shaderExtraData
                          << "\tvertices=" << part.positions.size()
                          << "\ttriangles=" << (part.triangleIndices.size() / 3u)
                          << "\tboundsMin=" << boundsMin.x << "," << boundsMin.y << "," << boundsMin.z
                          << "\tboundsMax=" << boundsMax.x << "," << boundsMax.y << "," << boundsMax.z
                          << "\tuvSets=" << part.uvSets.size()
                          << "\tuvDiagnostics=" << part.uvSetDiagnostics.size()
                          << "\tvertexColors=" << part.vertexColors.size()
                          << "\tapplyMode=" << part.textureApplyMode
                          << "\tmaterialAmbient=" << part.material.ambient[0] << "," << part.material.ambient[1] << "," << part.material.ambient[2]
                          << "\tmaterialDiffuse=" << part.material.diffuse[0] << "," << part.material.diffuse[1] << "," << part.material.diffuse[2]
                          << "\tmaterialSpecular=" << part.material.specular[0] << "," << part.material.specular[1] << "," << part.material.specular[2]
                          << "\tmaterialEmissive=" << part.material.emissive[0] << "," << part.material.emissive[1] << "," << part.material.emissive[2]
                          << "\tglossiness=" << part.material.glossiness
                          << "\tmaterialAlpha=" << part.material.alpha
                          << "\tbumpLumaScale=" << part.bumpMapLumaScale
                          << "\tbumpLumaOffset=" << part.bumpMapLumaOffset
                          << "\tbumpMatrix=" << part.bumpMapMatrix[0] << "," << part.bumpMapMatrix[1] << ","
                          << part.bumpMapMatrix[2] << "," << part.bumpMapMatrix[3]
                          << "\talphaBlend=" << part.alphaBlend
                          << "\talphaSrc=" << static_cast<unsigned>(part.alphaSrcBlend)
                          << "\talphaDst=" << static_cast<unsigned>(part.alphaDstBlend)
                          << "\talphaTest=" << part.alphaTest
                          << "\talphaTestFunc=" << static_cast<unsigned>(part.alphaTestFunc)
                          << "\talphaThreshold=" << static_cast<unsigned>(part.alphaThreshold)
                          << "\tdepthTest=" << part.depthTest
                          << "\tdepthWrite=" << part.depthWrite
                          << "\tdepthFunction=" << part.depthFunction
                          << "\tfaceDrawMode=" << part.faceDrawMode << '\n';
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
                if (part.glassShader) {
                    const auto& glass = *part.glassShader;
                    std::cout << "GLASS\tpart=" << partIndex
                              << "\tauthored=" << glass.authoredOverride
                              << "\tbaseColor=" << glass.baseColor.r << "," << glass.baseColor.g << ","
                              << glass.baseColor.b << "," << glass.baseColor.a
                              << "\trefractionScale=" << glass.refractionScale
                              << "\treflectionScale=" << glass.reflectionScale
                              << "\tiorRatio=" << glass.indexOfRefractionRatio
                              << "\tambient=" << glass.ambient
                              << "\trainbowSpread=" << glass.rainbowSpread
                              << "\trainbowScale=" << glass.rainbowScale << '\n';
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
                              << "\tclamp=" << tex.clampMode
                              << "\tfilter=" << tex.filterMode
                              << "\thasTransform=" << tex.hasTransform
                              << "\ttranslation=" << tex.translation.u << "," << tex.translation.v
                              << "\tscale=" << tex.scale.u << "," << tex.scale.v
                              << "\trotation=" << tex.rotation
                              << "\ttransformType=" << tex.transformType
                              << "\tcenter=" << tex.center.u << "," << tex.center.v
                              << "\texternal=" << (!tex.sourceUsesEmbeddedPixelData)
                              << "\tcube=" << tex.sourceIsCubeMap
                              << "\tembeddedFaces=" << (tex.embeddedTexture ? tex.embeddedTexture->faces : 0u)
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
                                  << "\tpixelFormat=" << image.pixelFormat
                                  << "\tbitsPerPixel=" << image.bitsPerPixel
                                  << "\tbytesPerPixel=" << image.bytesPerPixel
                                  << "\trMean=" << (pixels ? static_cast<double>(sums[0])/pixels : 0.0)
                                  << "\tgMean=" << (pixels ? static_cast<double>(sums[1])/pixels : 0.0)
                                  << "\tbMean=" << (pixels ? static_cast<double>(sums[2])/pixels : 0.0)
                                  << "\taMean=" << (pixels ? static_cast<double>(sums[3])/pixels : 0.0)
                                  << "\taMin=" << mins[3] << "\taMax=" << maxs[3] << '\n';
                        for (std::size_t channelIndex = 0; channelIndex < image.channels.size(); ++channelIndex) {
                            const auto& channel = image.channels[channelIndex];
                            std::cout << "EMBEDCHANNEL\tpart=" << partIndex
                                      << "\tkind=classic\tid=" << slot
                                      << "\tindex=" << channelIndex
                                      << "\tcomponent=" << channel.component
                                      << "\trepresentation=" << channel.representation
                                      << "\tbits=" << static_cast<unsigned>(channel.bits)
                                      << "\tsigned=" << channel.isSigned << '\n';
                        }
                    }
                }
                for (std::size_t animationIndex = 0;
                     animationIndex < part.textureTransformAnimations.size();
                     ++animationIndex) {
                    const auto& animation = part.textureTransformAnimations[animationIndex];
                    const auto& track = animation.track;
                    std::cout << "TEXANIM\tpart=" << partIndex
                              << "\tindex=" << animationIndex
                              << "\tslot=" << animation.slot
                              << "\toperation=" << animation.operation
                              << "\tactive=" << track.active
                              << "\textrapolation=" << static_cast<unsigned>(track.extrapolation)
                              << "\tfrequency=" << track.frequency
                              << "\tphase=" << track.phase
                              << "\tstart=" << track.startTime
                              << "\tstop=" << track.stopTime
                              << "\tcurrent=" << track.currentValue
                              << "\tinterpolation=" << track.interpolation
                              << "\tkeys=" << track.keys.size() << '\n';
                    for (std::size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex) {
                        const auto& key = track.keys[keyIndex];
                        std::cout << "TEXANIMKEY\tpart=" << partIndex
                                  << "\tanimation=" << animationIndex
                                  << "\tindex=" << keyIndex
                                  << "\ttime=" << key.time
                                  << "\tvalue=" << key.value
                                  << "\tforward=" << key.forwardTangent
                                  << "\tbackward=" << key.backwardTangent << '\n';
                    }
                }
                for (const auto& tex : part.shaderTextureSlots) {
                    std::cout << "SHADERTEX\tpart=" << partIndex
                              << "\tmapId=" << tex.mapId
                              << "\tuv=" << tex.texture.uvSet
                              << "\texternal=" << (!tex.texture.sourceUsesEmbeddedPixelData)
                              << "\tcube=" << tex.texture.sourceIsCubeMap
                              << "\tembeddedFaces=" << (tex.texture.embeddedTexture ? tex.texture.embeddedTexture->faces : 0u)
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
                                  << "\tpixelFormat=" << image.pixelFormat
                                  << "\tbitsPerPixel=" << image.bitsPerPixel
                                  << "\tbytesPerPixel=" << image.bytesPerPixel
                                  << "\trMean=" << (pixels ? static_cast<double>(sums[0])/pixels : 0.0)
                                  << "\tgMean=" << (pixels ? static_cast<double>(sums[1])/pixels : 0.0)
                                  << "\tbMean=" << (pixels ? static_cast<double>(sums[2])/pixels : 0.0)
                                  << "\taMean=" << (pixels ? static_cast<double>(sums[3])/pixels : 0.0)
                                  << "\taMin=" << mins[3] << "\taMax=" << maxs[3] << '\n';
                        for (std::size_t channelIndex = 0; channelIndex < image.channels.size(); ++channelIndex) {
                            const auto& channel = image.channels[channelIndex];
                            std::cout << "EMBEDCHANNEL\tpart=" << partIndex
                                      << "\tkind=shader\tid=" << tex.mapId
                                      << "\tindex=" << channelIndex
                                      << "\tcomponent=" << channel.component
                                      << "\trepresentation=" << channel.representation
                                      << "\tbits=" << static_cast<unsigned>(channel.bits)
                                      << "\tsigned=" << channel.isSigned << '\n';
                        }
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
