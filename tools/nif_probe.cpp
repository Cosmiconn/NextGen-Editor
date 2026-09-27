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
                          << "\tvertexColors=" << part.vertexColors.size()
                          << "\tapplyMode=" << part.textureApplyMode << '\n';
                for (std::size_t slot = 0; slot < part.textureSlots.size(); ++slot) {
                    const auto& tex = part.textureSlots[slot];
                    if (!tex.present) continue;
                    std::cout << "TEX\tpart=" << partIndex
                              << "\tslot=" << slot
                              << "\tuv=" << tex.uvSet
                              << "\texternal=" << (!tex.sourceUsesEmbeddedPixelData)
                              << "\tsource=" << tex.texture << '\n';
                }
                for (const auto& tex : part.shaderTextureSlots) {
                    std::cout << "SHADERTEX\tpart=" << partIndex
                              << "\tmapId=" << tex.mapId
                              << "\tuv=" << tex.texture.uvSet
                              << "\texternal=" << (!tex.texture.sourceUsesEmbeddedPixelData)
                              << "\tsource=" << tex.texture.texture << '\n';
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
                }
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cout << "ERROR\t0\t0\t0\t0\tException: " << e.what() << '\n';
        return 1;
    }
}
