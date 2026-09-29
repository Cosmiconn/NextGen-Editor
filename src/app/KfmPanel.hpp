#pragma once
#include "mapeditor/core/KfmFile.hpp"
#include "mapeditor/core/KfAnimation.hpp"
#include "mapeditor/core/NifModel.hpp"
#include "mapeditor/core/NifAnimationPose.hpp"
#include <functional>

namespace theseed::mapeditor::app {
class KfmPanel {
public:
    void Draw(const std::function<std::optional<std::string>()>& browse);
    bool Open(const std::filesystem::path& path);
private:
    void Filter();
    void LoadSelectedKfPreview();
    void DrawSkeletonPreview();
    void MarkEdited(bool referencesChanged = true);
    char path_[4096]{}, exportPath_[4096]{}, filter_[256]{};
    std::filesystem::path source_;
    std::optional<core::KfmFile> file_;
    std::optional<core::KfmReferences> references_;
    std::vector<std::size_t> visible_;
    std::size_t selected_ = 0, transitionCount_ = 0;
    bool dirty_ = false;

    std::optional<core::KfAnimationFile> previewKf_;
    std::filesystem::path previewKfPath_;
    std::optional<core::NifModel> previewNif_;
    std::filesystem::path previewNifPath_;
    std::string previewNifMessage_;
    std::size_t previewAnimationIndex_ = static_cast<std::size_t>(-1);
    float previewTime_ = 0.0f;
    float previewSpeed_ = 1.0f;
    bool previewPlaying_ = false;
    bool previewLoop_ = true;
    float previewSkeletonYaw_ = 0.35f;
    float previewSkeletonPitch_ = -0.20f;
    float previewSkeletonZoom_ = 1.0f;
    bool previewShowMesh_ = true;
    std::string previewMessage_;

    std::string message_;
};
}
