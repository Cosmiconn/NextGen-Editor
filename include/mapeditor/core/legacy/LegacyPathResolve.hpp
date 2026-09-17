#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace theseed::mapeditor::core::legacy {

std::filesystem::path LegacyPathToNative(const std::string& legacyPath);
bool EqualsCaseInsensitive(const std::string& a, const std::string& b);
std::filesystem::path StripResmapPrefix(const std::filesystem::path& p);
std::optional<std::filesystem::path> ResolveCaseInsensitivePath(
    const std::filesystem::path& root, const std::filesystem::path& relative);
std::optional<std::filesystem::path> ResolveLegacyAssetPath(
    const std::filesystem::path& mapDir, const std::string& legacyPath);
std::optional<std::filesystem::path> FindSiblingFileByStem(
    const std::filesystem::path& dir, const std::string& stem, const std::string& extension);

} // namespace theseed::mapeditor::core::legacy
