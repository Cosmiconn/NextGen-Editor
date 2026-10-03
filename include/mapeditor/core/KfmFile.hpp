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
    // Runtime-only provenance. Never encoded into the KFM. LoadKfmFile sets this to the
    // actual working copy that supplied the bytes so relative NIF/KF references can honor
    // project copy-on-write without losing the separate read-only source identity.
    std::filesystem::path loadedPath;
};
const char* KfmVersionName(KfmVersion version);
std::expected<KfmFile, std::string> DecodeKfm(std::span<const std::uint8_t> bytes);
std::expected<std::vector<std::uint8_t>, std::string> EncodeKfm(const KfmFile& file);
std::expected<KfmFile, std::string> LoadKfmFile(const std::filesystem::path& path);
// Export a NEW file. Existing destinations are never truncated or replaced.
std::expected<void, std::string> SaveKfmFile(const KfmFile& file, const std::filesystem::path& path);

// ---- Bearbeitung der Animationsliste ----
// Kleinste Event-ID größer als alle vorhandenen (mindestens 1).
[[nodiscard]] std::int32_t KfmNextFreeEventCode(const KfmFile& file);
// Kopie der Animation `index` direkt dahinter einfügen: neue freie Event-ID, KF-Datei, Index und
// Übergänge unverändert; der Legacy-Name (nur 1.2.4b) erhält " Kopie". Liefert den neuen Index.
std::size_t KfmDuplicateAnimation(KfmFile& file, std::size_t index);
// Animation entfernen. Übergänge ANDERER Animationen, die auf ihre Event-ID zeigen, werden nicht
// still verändert, sondern gezählt (sie gelten danach als fehlende Ziele).
struct KfmRemoveReport { std::size_t danglingTransitions = 0, danglingIntermediates = 0; bool removed = false; };
KfmRemoveReport KfmRemoveAnimation(KfmFile& file, std::size_t index);

struct KfmReferences {
    std::optional<std::filesystem::path> nif;
    std::vector<std::optional<std::filesystem::path>> animations;
    std::size_t missingKfFiles = 0, duplicateEventCodes = 0;
    std::size_t missingTransitionTargets = 0, missingIntermediateTargets = 0;
};
// Explicit relative references. If file.loadedPath is set, project/working siblings are
// checked first and the source directory is the read-only fallback. No recursive basename
// guesses. Work is performed once per request, never per rendered row.
KfmReferences InspectKfmReferences(const KfmFile& file, const std::filesystem::path& source);
// Explicit copy-on-write variant for callers that already know both paths.
KfmReferences InspectKfmReferences(const KfmFile& file,
                                   const std::filesystem::path& source,
                                   const std::filesystem::path& workingSource);
} // namespace theseed::mapeditor::core
