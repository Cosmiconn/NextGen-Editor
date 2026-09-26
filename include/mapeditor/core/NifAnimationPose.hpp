#pragma once

#include "mapeditor/core/KfAnimation.hpp"
#include "mapeditor/core/NifModel.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace theseed::mapeditor::core {

enum class NifPoseTrackState : std::uint8_t {
    BindPose = 0,
    Animated,
    Unsupported,
    Ambiguous,
};

struct NifAnimatedNode {
    NifTransform local{};
    NifTransform world{};
    NifPoseTrackState trackState = NifPoseTrackState::BindPose;
};

struct NifAnimatedPart {
    std::size_t partIndex = 0;
    bool skinned = false;
    std::vector<NifVec3> positions;
};

struct NifAnimationPose {
    std::vector<NifAnimatedNode> nodes;
    std::vector<NifAnimatedPart> parts;
    std::size_t matchedTracks = 0;
    std::size_t unsupportedTracks = 0;
    std::size_t ambiguousTracks = 0;
    std::size_t unmatchedTracks = 0;
    std::size_t skinnedParts = 0;
    std::size_t skinnedVertices = 0;
};

// Binds KF transform tracks to exact NIF node names, samples only interpolation modes already
// verified by SampleKfTransformTrack(), rebuilds the real NIF hierarchy, and then re-evaluates
// the preserved NiSkinInstance/NiSkinData weights with the same transform order used by the
// parser's bind-pose path.
//
// Unsupported or ambiguous tracks deliberately remain in NIF bind pose instead of being guessed.
// Static/non-skinned parts are returned unchanged so the preview can render the complete NIF.
NifAnimationPose PoseNifModelWithKf(const NifModel& model, const KfAnimationFile& animation,
                                    float sequenceTime);

} // namespace theseed::mapeditor::core
