#include "mapeditor/core/NifAnimationPose.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace theseed::mapeditor::core {
namespace {

NifTransform Multiply(const NifTransform& a, const NifTransform& b) {
    NifTransform out;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out.rotation[static_cast<std::size_t>(row * 3 + col)] =
                a.rotation[static_cast<std::size_t>(row * 3 + 0)] * b.rotation[static_cast<std::size_t>(0 * 3 + col)] +
                a.rotation[static_cast<std::size_t>(row * 3 + 1)] * b.rotation[static_cast<std::size_t>(1 * 3 + col)] +
                a.rotation[static_cast<std::size_t>(row * 3 + 2)] * b.rotation[static_cast<std::size_t>(2 * 3 + col)];
        }
    }

    const auto& t = b.translation;
    out.translation.x = a.translation.x + a.scale *
        (a.rotation[0] * t.x + a.rotation[1] * t.y + a.rotation[2] * t.z);
    out.translation.y = a.translation.y + a.scale *
        (a.rotation[3] * t.x + a.rotation[4] * t.y + a.rotation[5] * t.z);
    out.translation.z = a.translation.z + a.scale *
        (a.rotation[6] * t.x + a.rotation[7] * t.y + a.rotation[8] * t.z);
    out.scale = a.scale * b.scale;
    return out;
}

std::array<float, 9> QuatMatrix(KfQuat q) {
    const float len = std::sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    if (len > 1.0e-8f) {
        q.w /= len; q.x /= len; q.y /= len; q.z /= len;
    } else {
        q = {};
    }
    const float xx=q.x*q.x, yy=q.y*q.y, zz=q.z*q.z;
    const float xy=q.x*q.y, xz=q.x*q.z, yz=q.y*q.z;
    const float wx=q.w*q.x, wy=q.w*q.y, wz=q.w*q.z;
    return {
        1.0f-2.0f*(yy+zz), 2.0f*(xy-wz),       2.0f*(xz+wy),
        2.0f*(xy+wz),       1.0f-2.0f*(xx+zz), 2.0f*(yz-wx),
        2.0f*(xz-wy),       2.0f*(yz+wx),       1.0f-2.0f*(xx+yy)
    };
}

NifTransform FromNode(const NifNodeInfo& node) {
    NifTransform t;
    t.rotation = node.localRotation;
    t.translation = node.localTranslation;
    t.scale = node.localScale;
    return t;
}

NifTransform FromKf(const KfTransform& transform) {
    NifTransform t;
    t.rotation = QuatMatrix(transform.rotation);
    t.translation = {transform.translation.x, transform.translation.y, transform.translation.z};
    t.scale = transform.scale;
    return t;
}

NifVec3 TransformEditorPoint(const NifTransform& transform, const NifVec3& editorPoint) {
    // The preserved skin transforms are in the original Gamebryo frame while mesh arrays use
    // the editor's Y-up frame (x, legacyZ, legacyY). This is the exact conversion used by the
    // verified bind-pose implementation in NifModel.cpp.
    const float x = editorPoint.x;
    const float y = editorPoint.z;
    const float z = editorPoint.y;
    const float nx = (transform.rotation[0] * x + transform.rotation[1] * y + transform.rotation[2] * z) *
                         transform.scale + transform.translation.x;
    const float ny = (transform.rotation[3] * x + transform.rotation[4] * y + transform.rotation[5] * z) *
                         transform.scale + transform.translation.y;
    const float nz = (transform.rotation[6] * x + transform.rotation[7] * y + transform.rotation[8] * z) *
                         transform.scale + transform.translation.z;
    return {nx, nz, ny};
}

NifTransform RelativeToSkeletonRoot(const std::vector<NifAnimatedNode>& nodes,
                                    const std::vector<NifNodeInfo>& sourceNodes,
                                    int nodeIndex, int skeletonRoot) {
    NifTransform out;
    int node = nodeIndex;
    int guard = 0;
    while (node >= 0 && node != skeletonRoot && guard++ < 256) {
        if (static_cast<std::size_t>(node) >= nodes.size()) return {};
        out = Multiply(nodes[static_cast<std::size_t>(node)].local, out);
        node = sourceNodes[static_cast<std::size_t>(node)].parentIndex;
    }
    if (skeletonRoot >= 0 && node != skeletonRoot) return {};
    return out;
}

} // namespace

NifAnimationPose PoseNifModelWithKf(const NifModel& model, const KfAnimationFile& animation,
                                    float sequenceTime) {
    NifAnimationPose pose;
    pose.nodes.resize(model.nodes.size());

    for (std::size_t i = 0; i < model.nodes.size(); ++i)
        pose.nodes[i].local = FromNode(model.nodes[i]);

    std::unordered_map<std::string, std::vector<std::size_t>> nodeIndices;
    nodeIndices.reserve(model.nodes.size());
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        if (!model.nodes[i].name.empty()) nodeIndices[model.nodes[i].name].push_back(i);
    }

    std::unordered_map<std::string, std::vector<const KfControlledTrack*>> tracksByName;
    tracksByName.reserve(animation.sequence.transformTracks.size());
    for (const auto& track : animation.sequence.transformTracks) {
        if (!track.nodeName.empty()) tracksByName[track.nodeName].push_back(&track);
    }

    for (const auto& [name, tracks] : tracksByName) {
        const auto nodeIt = nodeIndices.find(name);
        if (nodeIt == nodeIndices.end()) {
            pose.unmatchedTracks += tracks.size();
            continue;
        }
        if (tracks.size() != 1 || nodeIt->second.size() != 1) {
            pose.ambiguousTracks += tracks.size();
            for (const auto nodeIndex : nodeIt->second)
                pose.nodes[nodeIndex].trackState = NifPoseTrackState::Ambiguous;
            continue;
        }

        const std::size_t nodeIndex = nodeIt->second.front();
        const auto sampled = SampleKfTransformTrack(animation, *tracks.front(), sequenceTime);
        if (!sampled) {
            pose.nodes[nodeIndex].trackState = NifPoseTrackState::Unsupported;
            ++pose.unsupportedTracks;
            continue;
        }

        pose.nodes[nodeIndex].local = FromKf(*sampled);
        pose.nodes[nodeIndex].trackState = NifPoseTrackState::Animated;
        ++pose.matchedTracks;
    }

    std::vector<std::uint8_t> visit(model.nodes.size(), 0);
    std::function<void(std::size_t)> buildWorld = [&](std::size_t index) {
        if (visit[index] == 2) return;
        if (visit[index] == 1) {
            // Corrupt/cyclic hierarchy: stay local rather than recursing forever. This is a
            // preview-only fallback and does not mutate source data.
            pose.nodes[index].world = pose.nodes[index].local;
            visit[index] = 2;
            return;
        }
        visit[index] = 1;
        const int parent = model.nodes[index].parentIndex;
        if (parent >= 0 && static_cast<std::size_t>(parent) < model.nodes.size()) {
            buildWorld(static_cast<std::size_t>(parent));
            pose.nodes[index].world =
                Multiply(pose.nodes[static_cast<std::size_t>(parent)].world, pose.nodes[index].local);
        } else {
            pose.nodes[index].world = pose.nodes[index].local;
        }
        visit[index] = 2;
    };
    for (std::size_t i = 0; i < model.nodes.size(); ++i) buildWorld(i);

    pose.parts.reserve(model.parts.size());
    for (std::size_t partIndex = 0; partIndex < model.parts.size(); ++partIndex) {
        const auto& part = model.parts[partIndex];
        NifAnimatedPart animatedPart;
        animatedPart.partIndex = partIndex;

        if (!part.skinBinding) {
            animatedPart.positions = part.positions;
            pose.parts.push_back(std::move(animatedPart));
            continue;
        }

        const auto& skin = *part.skinBinding;
        animatedPart.skinned = true;
        animatedPart.positions.resize(skin.sourcePositions.size());
        ++pose.skinnedParts;
        pose.skinnedVertices += skin.sourcePositions.size();

        for (std::size_t vertex = 0; vertex < skin.sourcePositions.size(); ++vertex) {
            NifVec3 skinned = skin.sourcePositions[vertex];
            NifVec3 accumulated{};
            float accumulatedWeight = 0.0f;

            if (vertex < skin.vertexInfluences.size()) {
                for (const auto& influence : skin.vertexInfluences[vertex]) {
                    if (influence.weight == 0.0f || influence.boneIndex >= skin.bones.size()) continue;
                    const auto& bone = skin.bones[influence.boneIndex];
                    if (bone.nodeIndex < 0 || static_cast<std::size_t>(bone.nodeIndex) >= pose.nodes.size())
                        continue;

                    const NifTransform relative =
                        RelativeToSkeletonRoot(pose.nodes, model.nodes, bone.nodeIndex,
                                               skin.skeletonRootNodeIndex);
                    NifTransform transform = Multiply(relative, bone.bindTransform);
                    if (!skin.partitionWeights)
                        transform = Multiply(skin.skinTransform, transform);

                    const NifVec3 p = TransformEditorPoint(transform, skin.sourcePositions[vertex]);
                    accumulated.x += p.x * influence.weight;
                    accumulated.y += p.y * influence.weight;
                    accumulated.z += p.z * influence.weight;
                    accumulatedWeight += influence.weight;
                }
            }

            if (accumulatedWeight > 1.0e-8f) skinned = accumulated;
            animatedPart.positions[vertex] =
                TransformEditorPoint(skin.meshToModelTransform, skinned);
        }

        pose.parts.push_back(std::move(animatedPart));
    }

    return pose;
}

} // namespace theseed::mapeditor::core
