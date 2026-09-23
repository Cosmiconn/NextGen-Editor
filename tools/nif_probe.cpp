#include "mapeditor/core/NifModel.hpp"
#include <iostream>
#include <string>

// One process per input: the parent harness owns the hard OS timeout.
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const auto result = theseed::mapeditor::core::LoadNifMesh(
            std::filesystem::u8path(argv[2]), std::string(argv[1]) == "--recovery");
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
        return 0;
    } catch (const std::exception& e) {
        std::cout << "ERROR\t0\t0\t0\t0\tException: " << e.what() << '\n';
        return 1;
    }
}
