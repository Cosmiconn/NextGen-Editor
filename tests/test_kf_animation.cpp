#include "mapeditor/core/KfAnimation.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

using namespace theseed::mapeditor::core;

namespace {
bool Near(float a, float b, float eps = 1.0e-4f) {
    return std::abs(a - b) <= eps;
}

const KfControlledTrack* FindTrack(const KfAnimationFile& file, const std::string& name) {
    for (const auto& track : file.sequence.transformTracks)
        if (track.nodeName == name) return &track;
    return nullptr;
}

void TestSampler() {
    KfAnimationFile file;
    file.sequence.startTime = 0.0f;
    file.sequence.stopTime = 2.0f;

    KfControlledTrack track;
    track.nodeName = "SamplerNode";
    track.pose.translation = {100.0f, 200.0f, 300.0f};
    track.pose.rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    track.pose.scale = 1.0f;

    track.keys.translation.interpolation = 1;
    track.keys.translation.keys = {
        {0.0f, {0.0f, 0.0f, 0.0f}},
        {2.0f, {10.0f, 20.0f, 30.0f}},
    };
    track.keys.scale.interpolation = 1;
    track.keys.scale.keys = {
        {0.0f, 1.0f},
        {2.0f, 3.0f},
    };
    track.keys.rotationType = 1;
    track.keys.quaternionRotation = {
        {0.0f, {1.0f, 0.0f, 0.0f, 0.0f}},
        {2.0f, {0.0f, 0.0f, 1.0f, 0.0f}},
    };
    file.sequence.transformTracks.push_back(track);

    auto mid = SampleKfTransformTrack(file, file.sequence.transformTracks.front(), 1.0f);
    assert(mid);
    assert(Near(mid->translation.x, 5.0f));
    assert(Near(mid->translation.y, 10.0f));
    assert(Near(mid->translation.z, 15.0f));
    assert(Near(mid->scale, 2.0f));
    const float rootHalf = std::sqrt(0.5f);
    assert(Near(std::abs(mid->rotation.w), rootHalf));
    assert(Near(std::abs(mid->rotation.y), rootHalf));

    // Sequence time is clamped, so sampling before/after the sequence yields endpoints.
    auto before = SampleKfTransformTrack(file, file.sequence.transformTracks.front(), -50.0f);
    auto after = SampleKfTransformTrack(file, file.sequence.transformTracks.front(), 50.0f);
    assert(before && after);
    assert(Near(before->translation.x, 0.0f));
    assert(Near(after->translation.x, 10.0f));
    assert(Near(after->scale, 3.0f));

    auto all = SampleKfSequence(file, 1.0f);
    assert(all && all->size() == 1);
    assert(all->front().nodeName == "SamplerNode");

    // Constant keys are true step functions.
    KfControlledTrack constantTrack;
    constantTrack.nodeName = "Constant";
    constantTrack.keys.scale.interpolation = 5;
    constantTrack.keys.scale.keys = {{0.0f, 2.0f}, {1.0f, 7.0f}};
    auto constant = SampleKfTransformTrack(file, constantTrack, 0.75f);
    assert(constant && Near(constant->scale, 2.0f));

    // XYZ rotation channels are supported independently.
    KfControlledTrack xyzTrack;
    xyzTrack.nodeName = "XYZ";
    xyzTrack.keys.rotationType = 4;
    xyzTrack.keys.xyzRotation[0].interpolation = 1;
    xyzTrack.keys.xyzRotation[0].keys = {
        {0.0f, 0.0f},
        {2.0f, 3.14159265358979323846f},
    };
    auto xyz = SampleKfTransformTrack(file, xyzTrack, 1.0f);
    assert(xyz);
    assert(Near(std::abs(xyz->rotation.w), rootHalf));
    assert(Near(std::abs(xyz->rotation.x), rootHalf));

    // Unknown interpolation semantics and compressed splines must fail explicitly.
    KfControlledTrack unsupported = track;
    unsupported.nodeName = "Quadratic";
    unsupported.keys.translation.interpolation = 2;
    assert(!SampleKfTransformTrack(file, unsupported, 1.0f));

    KfControlledTrack compressed;
    compressed.nodeName = "Compressed";
    compressed.compressedSpline = true;
    assert(!SampleKfTransformTrack(file, compressed, 1.0f));
}
}

int main(int argc, char** argv) {
    assert(argc >= 3);
    TestSampler();

    const auto effect = LoadKfAnimation(std::filesystem::path(argv[1]));
    if (!effect) {
        std::cerr << effect.error() << "\n";
        return 1;
    }

    assert(effect->version == 0x14000004u);
    assert(effect->blockTypes.size() == 53);
    assert(effect->sequence.name == "Stand");
    assert(effect->sequence.accumulationRoot == "Base");
    assert(effect->sequence.controlledBlockCount == 34);
    assert(effect->sequence.transformTracks.size() == 8);
    assert(Near(effect->sequence.startTime, 0.0f));
    assert(Near(effect->sequence.stopTime, 0.5f));
    assert(effect->sequence.textKeys.size() == 2);
    assert(effect->sequence.textKeys[0].text == "start");
    assert(effect->sequence.textKeys[1].text == "end");
    assert(Near(effect->sequence.textKeys[1].time, 0.5f));
    assert(effect->splineData.size() == 1);
    assert(effect->splineBases.size() == 1);

    // Die echte Fixture benennt die Plane-Knoten durchnummeriert (Plane13..Plane18).
    // Getestet werden soll hier die dekodierte XYZ-Rotationsstruktur, nicht ein erfundener
    // exakter Knotename. Suche deshalb den passenden Plane-Track anhand seiner Datenform.
    const KfControlledTrack* plane = nullptr;
    for (const auto& track : effect->sequence.transformTracks) {
        if (track.nodeName.rfind("Plane", 0) != 0 || track.compressedSpline ||
            track.keys.rotationType != 4) continue;
        if (track.keys.xyzRotation[0].keys.size() == 2 &&
            track.keys.xyzRotation[1].keys.size() == 2 &&
            track.keys.xyzRotation[2].keys.size() == 2) {
            plane = &track;
            break;
        }
    }
    if (plane == nullptr) {
        std::cerr << "No Plane* track with 2-key XYZ rotation found. Tracks:";
        for (const auto& track : effect->sequence.transformTracks)
            std::cerr << " [" << track.nodeName << "]";
        std::cerr << "\n";
        return 2;
    }
    assert(!plane->compressedSpline);
    assert(plane->keys.rotationType == 4);
    assert(plane->keys.xyzRotation[0].keys.size() == 2);
    assert(plane->keys.xyzRotation[1].keys.size() == 2);
    assert(plane->keys.xyzRotation[2].keys.size() == 2);

    const auto character = LoadKfAnimation(std::filesystem::path(argv[2]));
    if (!character) {
        std::cerr << character.error() << "\n";
        return 1;
    }

    assert(character->blockTypes.size() == 58);
    assert(character->sequence.name == "Emotion_ChargdDance69");
    assert(character->sequence.accumulationRoot == "Bip01");
    assert(character->sequence.controlledBlockCount == 53);
    assert(character->sequence.transformTracks.size() == 53);
    assert(Near(character->sequence.startTime, 0.0f));
    assert(Near(character->sequence.stopTime, 31.466667f, 1.0e-3f));
    assert(character->sequence.textKeys.size() == 2);
    assert(character->sequence.textKeys.front().text == "start");
    assert(character->sequence.textKeys.back().text == "end");

    assert(character->splineData.size() == 1);
    assert(character->splineBases.size() == 1);
    assert(character->splineData[0].floatControlPoints.empty());
    assert(character->splineData[0].compactControlPoints.size() == 245760);
    assert(character->splineBases[0].controlPointCount == 960);

    const auto* pelvis = FindTrack(*character, "Bip01 Pelvis");
    assert(pelvis != nullptr);
    assert(pelvis->compressedSpline);
    assert(pelvis->spline.dataBlock >= 0);
    assert(pelvis->spline.basisBlock >= 0);

    const auto* head = FindTrack(*character, "Bip01 Head");
    assert(head != nullptr);
    assert(head->compressedSpline);

    // Defensive behavior: truncated data must fail cleanly rather than reading past the buffer.
    const std::array<std::uint8_t, 8> bad = {0,1,2,3,4,5,6,7};
    assert(!DecodeKfAnimation(bad));

    std::cout << "KF animation parser tests passed\n";
    return 0;
}
