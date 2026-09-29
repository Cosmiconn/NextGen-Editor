#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct KfVec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct KfQuat {
    float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f;
};

struct KfTransform {
    KfVec3 translation{};
    KfQuat rotation{};
    float scale = 1.0f;
};

struct KfFloatKey {
    float time = 0.0f;
    float value = 0.0f;
    float forward = 0.0f;
    float backward = 0.0f;
    float tension = 0.0f, bias = 0.0f, continuity = 0.0f;
};

struct KfVec3Key {
    float time = 0.0f;
    KfVec3 value{};
    KfVec3 forward{};
    KfVec3 backward{};
    float tension = 0.0f, bias = 0.0f, continuity = 0.0f;
};

struct KfQuatKey {
    float time = 0.0f;
    KfQuat value{};
    float tension = 0.0f, bias = 0.0f, continuity = 0.0f;
};

struct KfKeyGroupFloat {
    std::uint32_t interpolation = 0;
    std::vector<KfFloatKey> keys;
};

struct KfKeyGroupVec3 {
    std::uint32_t interpolation = 0;
    std::vector<KfVec3Key> keys;
};

struct KfTransformKeys {
    std::uint32_t rotationType = 0;
    std::vector<KfQuatKey> quaternionRotation;
    std::array<KfKeyGroupFloat, 3> xyzRotation;
    KfKeyGroupVec3 translation;
    KfKeyGroupFloat scale;
};

struct KfSplineDataBlock {
    std::int32_t blockIndex = -1;
    std::vector<float> floatControlPoints;
    std::vector<std::int16_t> compactControlPoints;
};

struct KfSplineBasisBlock {
    std::int32_t blockIndex = -1;
    std::uint32_t controlPointCount = 0;
};

struct KfSplineTransform {
    float startTime = 0.0f;
    float stopTime = 0.0f;
    std::int32_t dataBlock = -1;
    std::int32_t basisBlock = -1;
    std::uint32_t translationHandle = 0xFFFFu;
    std::uint32_t rotationHandle = 0xFFFFu;
    std::uint32_t scaleHandle = 0xFFFFu;
    float translationOffset = 0.0f;
    float translationHalfRange = 0.0f;
    float rotationOffset = 0.0f;
    float rotationHalfRange = 0.0f;
    float scaleOffset = 0.0f;
    float scaleHalfRange = 0.0f;
};

struct KfControlledTrack {
    std::string nodeName;
    std::string propertyType;
    std::string controllerType;
    std::string controllerId;
    std::string interpolatorId;
    std::int32_t interpolatorBlock = -1;
    KfTransform pose{};
    bool compressedSpline = false;
    KfTransformKeys keys{};
    KfSplineTransform spline{};
};

struct KfTextKey {
    float time = 0.0f;
    std::string text;
};

struct KfSequence {
    std::string name;
    std::string accumulationRoot;
    float weight = 1.0f;
    std::uint32_t cycleType = 0;
    float frequency = 1.0f;
    float startTime = 0.0f;
    float stopTime = 0.0f;
    std::size_t controlledBlockCount = 0;
    std::vector<KfControlledTrack> transformTracks;
    std::vector<KfTextKey> textKeys;
};

struct KfAnimationFile {
    std::uint32_t version = 0;
    std::vector<std::string> blockTypes;
    KfSequence sequence;
    std::vector<KfSplineDataBlock> splineData;
    std::vector<KfSplineBasisBlock> splineBases;
};

// Fiesta-KF-Dateien sind Gamebryo-NIF-Container (20.0.0.4), deren Root ein
// NiControllerSequence ist. Der Decoder unterstützt zunächst genau die in den
// verifizierten Fiesta-Fixtures vorkommenden Animationstypen. Unbekannte Blocktypen
// führen zu einem Fehler statt zu geraten oder Bytepositionen zu verlieren.
std::expected<KfAnimationFile, std::string> DecodeKfAnimation(std::span<const std::uint8_t> bytes);
std::expected<KfAnimationFile, std::string> LoadKfAnimation(const std::filesystem::path& path);

// Zeitliche Auswertung bereits dekodierter Transform-Tracks. Die API bleibt GUI-frei und
// arbeitet direkt in der KF-Sequenzzeit (StartTime..StopTime), damit Preview, Tests und spätere
// Export-/Diagnosewerkzeuge exakt denselben Sampler verwenden können.
//
// Unterstützt werden zunächst die für Fiesta bereits verifizierten Pose-, Linear-, Constant-
// und XYZ-Rotationsspuren. Für noch nicht verifizierte Quadratic-/TBC- oder komprimierte
// B-Spline-Spuren wird bewusst ein Fehler geliefert statt eine falsche Animation zu raten.
std::expected<KfTransform, std::string>
SampleKfTransformTrack(const KfAnimationFile& file, const KfControlledTrack& track,
                       float sequenceTime);

struct KfSampledTransform {
    std::string nodeName;
    KfTransform transform{};
};

std::expected<std::vector<KfSampledTransform>, std::string>
SampleKfSequence(const KfAnimationFile& file, float sequenceTime);

} // namespace theseed::mapeditor::core
