#pragma once
#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct NifVec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct NifVec2 { float u = 0.0f, v = 0.0f; };
struct NifMaterial {
    std::array<float, 3> ambient{1.0f,1.0f,1.0f};
    std::array<float, 3> diffuse{1.0f,1.0f,1.0f};
    std::array<float, 3> specular{1.0f,1.0f,1.0f};
    std::array<float, 3> emissive{0.0f,0.0f,0.0f};
    float glossiness = 10.0f;
    float alpha = 1.0f;
};
struct NifEmbeddedTexture {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
};
struct NifMeshPart {
    std::string name;
    std::vector<NifVec3> positions;
    std::vector<NifVec3> normals;
    std::vector<NifVec2> uvs;
    std::vector<std::uint32_t> triangleIndices;
    NifMaterial material;
    std::string diffuseTexture;
    std::shared_ptr<const NifEmbeddedTexture> embeddedDiffuseTexture;
};
struct NifModel { std::string rootName; std::vector<NifMeshPart> parts; };

std::expected<NifModel, std::string> LoadNifMesh(const std::filesystem::path& file);

} // namespace theseed::mapeditor::core
