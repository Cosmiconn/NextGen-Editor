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
}

int main(int argc, char** argv) {
    assert(argc >= 3);

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

    const auto* plane = FindTrack(*effect, "Plane");
    assert(plane != nullptr);
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
