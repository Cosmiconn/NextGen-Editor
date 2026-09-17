#pragma once
// NifMeshRenderer.hpp
// Rendert ECHTE .nif-Meshes für platzierte Objekte; nicht ladbare Modelle bleiben beim Platzhalterrenderer.

#include "Camera.hpp"
#include "mapeditor/core/NifModel.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace theseed::mapeditor::app {

class NifMeshRenderer {
public:
    ~NifMeshRenderer();
    void Init();
    void Shutdown();
    void LoadModelsForSet(const core::ObjectPlacementSet& set, const std::filesystem::path& mapDir);
    [[nodiscard]] bool HasRealMesh(std::size_t objectIndex) const;
    [[nodiscard]] std::size_t RealMeshCount() const;
    void Draw(const core::ObjectPlacementSet& set, const OrbitCamera& camera, int width, int height);

private:
    struct SubMesh {
        std::uint32_t vao = 0;
        std::uint32_t vbo = 0;
        std::uint32_t ebo = 0;
        std::uint32_t indexCount = 0;
        std::array<float, 3> diffuseColor{1.0f, 1.0f, 1.0f};
        std::uint32_t diffuseTex = 0;
    };
    struct LoadedModel { std::vector<SubMesh> subMeshes; };

    void ReleaseModel(LoadedModel& model);
    std::uint32_t GetOrLoadTexture(const std::filesystem::path& resolvedPath);
    std::uint32_t GetOrLoadEmbeddedTexture(const core::NifEmbeddedTexture& image, const std::string& cacheKey);

    std::unordered_map<std::string, LoadedModel> modelCache_;
    std::unordered_map<std::string, std::uint32_t> textureCache_;
    std::vector<const LoadedModel*> perObjectModel_;
    std::uint32_t shaderProgram_ = 0;
};

} // namespace theseed::mapeditor::app
