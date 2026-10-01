#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core {

enum class ProjectOutputSide {
    Client,
    Server
};

// Resolve a source file to its non-destructive project override location.
// Example:
//   sourceRoot = C:/Fiesta/Client
//   sourcePath = C:/Fiesta/Client/ressystem/ItemInfo.shn
//   -> <project>/Client/ressystem/ItemInfo.shn
//
// sourcePath must be inside sourceRoot. The returned path is lexical only; callers
// create parent directories immediately before writing.
std::expected<std::filesystem::path, std::string> ProjectOutputForSource(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& sourcePath,
    ProjectOutputSide side);

// Resolve a known runtime-relative path below <project>/Client or <project>/Server.
// Absolute paths and paths containing ".." are rejected.
std::expected<std::filesystem::path, std::string> ProjectOutputForRelative(
    const std::filesystem::path& projectRoot,
    ProjectOutputSide side,
    const std::filesystem::path& relativePath);

// Creates only the parent directory of an already validated project output path.
std::expected<void, std::string> EnsureProjectOutputParent(
    const std::filesystem::path& outputPath);

} // namespace theseed::mapeditor::core
