#include "mapeditor/core/NifAnimationPose.hpp"

#include <cassert>
#include <cmath>

using namespace theseed::mapeditor::core;

namespace {
bool Near(float a, float b, float eps = 1.0e-4f) {
    return std::abs(a - b) <= eps;
}

NifModel MakeModel(bool partitionWeights) {
    NifModel model;

    NifNodeInfo root;
    root.name = "Root";
    root.parentIndex = -1;
    model.nodes.push_back(root);

    NifNodeInfo bone;
    bone.name = "Bone";
    bone.parentIndex = 0;
    bone.localTranslation = {1.0f, 0.0f, 0.0f};
    model.nodes.push_back(bone);

    NifMeshPart part;
    part.name = "Skinned";
    part.skinned = true;
    part.positions = {
        {1.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
    };
    part.triangleIndices = {0, 1, 2};

    NifSkinBinding skin;
    skin.skeletonRootNodeIndex = 0;
    skin.partitionWeights = partitionWeights;
    skin.sourcePositions = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    };
    skin.vertexInfluences.resize(3);
    for (auto& influences : skin.vertexInfluences)
        influences.push_back({0, 1.0f});

    NifSkinBoneBinding binding;
    binding.nodeIndex = 1;
    skin.bones.push_back(binding);

    if (!partitionWeights)
        skin.skinTransform.translation = {10.0f, 0.0f, 0.0f};

    part.skinBinding = std::move(skin);
    model.parts.push_back(std::move(part));
    return model;
}

KfAnimationFile MakeAnimation(float boneX) {
    KfAnimationFile animation;
    animation.sequence.startTime = 0.0f;
    animation.sequence.stopTime = 1.0f;

    KfControlledTrack track;
    track.nodeName = "Bone";
    track.pose.translation = {boneX, 0.0f, 0.0f};
    animation.sequence.transformTracks.push_back(track);
    return animation;
}
}

int main() {
    {
        const auto model = MakeModel(true);
        const auto animation = MakeAnimation(2.0f);
        const auto pose = PoseNifModelWithKf(model, animation, 0.5f);

        assert(pose.matchedTracks == 1);
        assert(pose.unsupportedTracks == 0);
        assert(pose.ambiguousTracks == 0);
        assert(pose.skinnedParts == 1);
        assert(pose.parts.size() == 1);
        assert(pose.parts[0].skinned);
        assert(Near(pose.nodes[1].world.translation.x, 2.0f));
        assert(Near(pose.parts[0].positions[0].x, 2.0f));
        assert(Near(pose.parts[0].positions[1].x, 3.0f));
    }

    {
        // Sparse NiSkinData weights use the preserved skinTransform in addition to the bone
        // relative transform; this mirrors the verified bind-pose path in NifModel.cpp.
        const auto model = MakeModel(false);
        const auto animation = MakeAnimation(2.0f);
        const auto pose = PoseNifModelWithKf(model, animation, 0.5f);
        assert(Near(pose.parts[0].positions[0].x, 12.0f));
    }

    {
        // Duplicate KF node names are not guessed. The affected NIF node remains in bind pose.
        auto model = MakeModel(true);
        auto animation = MakeAnimation(2.0f);
        animation.sequence.transformTracks.push_back(animation.sequence.transformTracks.front());
        animation.sequence.transformTracks.back().pose.translation.x = 5.0f;

        const auto pose = PoseNifModelWithKf(model, animation, 0.5f);
        assert(pose.matchedTracks == 0);
        assert(pose.ambiguousTracks == 2);
        assert(pose.nodes[1].trackState == NifPoseTrackState::Ambiguous);
        assert(Near(pose.nodes[1].world.translation.x, 1.0f));
        assert(Near(pose.parts[0].positions[0].x, 1.0f));
    }

    {
        // A track that has no matching NIF node is diagnostic-only and does not affect the mesh.
        auto model = MakeModel(true);
        auto animation = MakeAnimation(2.0f);
        animation.sequence.transformTracks.front().nodeName = "MissingBone";
        const auto pose = PoseNifModelWithKf(model, animation, 0.5f);
        assert(pose.unmatchedTracks == 1);
        assert(Near(pose.parts[0].positions[0].x, 1.0f));
    }

    return 0;
}
