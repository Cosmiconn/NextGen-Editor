#pragma once
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {
enum class KfmVersion { V1_2_4b, V2_0_0_0b };
struct KfmTextKeyPair { std::string source, destination; };
struct KfmIntermediateAnimation { std::int32_t eventCode = 0; float value = -1.0f; };
struct KfmTransition {
    std::int32_t eventCode = 0, type = 5;
    // These three fields are absent for type 5. The intermediate float's runtime
    // meaning is not yet established; preserve it without interpreting it.
    float duration = 0;
    std::vector<KfmTextKeyPair> textKeys;
    std::vector<KfmIntermediateAnimation> intermediateAnimations;
};
struct KfmAnimation {
    std::int32_t eventCode = 0;
    std::string name; // Only present in 1.2.4b.
    std::string kfFileName;
    std::int32_t index = 0;
    std::vector<KfmTransition> transitions;
};
struct KfmFile {
    KfmVersion version = KfmVersion::V2_0_0_0b;
    bool crlf = false;
    std::uint8_t unknownByte = 1; // Only present in 2.0.0.0b; not assumed to be an endian flag.
    std::string nifFileName, master;
    std::int32_t unknownInt1 = 1, unknownInt2 = 0;
    float unknownFloat1 = 0.25f, unknownFloat2 = 0;
    std::vector<KfmAnimation> animations;
    std::int32_t unknownInt3 = 0;
};
const char* KfmVersionName(KfmVersion version);
std::expected<KfmFile, std::string> DecodeKfm(std::span<const std::uint8_t> bytes);
std::expected<std::vector<std::uint8_t>, std::string> EncodeKfm(const KfmFile& file);
std::expected<KfmFile, std::string> LoadKfmFile(const std::filesystem::path& path);
// Export a NEW file. Existing destinations are never truncated or replaced.
std::expected<void, std::string> SaveKfmFile(const KfmFile& file, const std::filesystem::path& path);

struct KfmReferences {
    std::optional<std::filesystem::path> nif;
    std::vector<std::optional<std::filesystem::path>> animations;
    std::size_t missingKfFiles = 0, duplicateEventCodes = 0;
    std::size_t missingTransitionTargets = 0, missingIntermediateTargets = 0;
};
// Explicit relative references, resolved against the KFM directory. No recursive
// basename guesses. Work is performed once per request, never per rendered row.
KfmReferences InspectKfmReferences(const KfmFile& file, const std::filesystem::path& source);
} // namespace theseed::mapeditor::core
