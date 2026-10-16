#pragma once
// NifNodeAnimation.hpp
// Im NIF eingebettete Knoten-Animationen (NiTransformController -> NiTransformInterpolator ->
// NiTransformData), z.B. Windmühlenflügel, schaukelnde Boote, Schilder, Leuchtturm-Licht.
// Die Mesh-Vertices liegen im NifModel bereits in der Bind-Pose (Szenengraph eingebrannt). Für die
// Wiedergabe genügt deshalb je Part eine starre Matrix: W_anim * inverse(W_bind) entlang der
// Kette Geometrie -> Wurzel.

#include "mapeditor/core/KfAnimation.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace theseed::mapeditor::core {

struct NifNodeTrack {
    KfTransformKeys keys;
    // Default-Werte des NiTransformInterpolator; ungültige Kanäle (-FLT_MAX) behalten den
    // Bind-Wert des Knotens.
    KfTransform pose{};
    bool poseTranslation = false, poseRotation = false, poseScale = false;
    // NiTimeController
    std::uint16_t flags = 0x0008; // Bit 3 aktiv, Bits 1-2 Zyklus (0 Loop, 1 Reverse, 2 Clamp)
    float frequency = 1.0f;
    float phase = 0.0f;
    float startTime = 0.0f;
    float stopTime = 0.0f;
};

struct NifAnimNode {
    std::int32_t parent = -1; // Index in NifNodeAnimation::nodes
    // Lokaler Bind-Transform im Legacy/Gamebryo-Rahmen (Zeilen-Matrix, v' = R*v*s + t).
    std::array<float, 9> rotation{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    float tx = 0.0f, ty = 0.0f, tz = 0.0f;
    float scale = 1.0f;
    std::int32_t track = -1; // Index in tracks, -1 = statisch
};

struct NifNodeAnimation {
    std::vector<NifAnimNode> nodes;   // nur Knoten auf Ketten animierter Parts (Eltern vor Kindern)
    std::vector<NifNodeTrack> tracks;
    [[nodiscard]] bool Empty() const { return tracks.empty() || nodes.empty(); }
    // Längste Laufzeit (stop-start)/frequency aller Spuren; 0 bei leeren Spuren.
    [[nodiscard]] float Duration() const;
};

// Controller-Zeit nach Gamebryo: t = sceneTime*frequency + phase, danach Loop/Reverse/Clamp in
// [start, stop].
[[nodiscard]] float NifControllerTime(const NifNodeTrack& track, float sceneTime);

// Lokaler Transform eines Knotens zur Controller-Zeit (Gamebryo-Rahmen). Unterstützt Linear,
// Konstant, Quadratisch (Hermite mit den gespeicherten Tangenten), TBC (Kochanek-Bartels) und
// XYZ-Rotation; Quaternion-TBC wird sphärisch linear interpoliert.
[[nodiscard]] NifAnimNode SampleNifAnimNode(const NifNodeAnimation& animation, std::size_t node, float sceneTime);

// Je Knoten eine 4x4-Matrix (spaltenweise, Editor-Rahmen x, y-hoch, z) = W_anim * inverse(W_bind).
// Bei Parts mit animNode >= 0 wird diese Matrix vor der Objektmatrix angewandt.
[[nodiscard]] std::vector<std::array<float, 16>> EvaluateNifNodeAnimation(const NifNodeAnimation& animation, float sceneTime);

} // namespace theseed::mapeditor::core
