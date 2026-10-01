#include "mapeditor/core/ProjectOutput.hpp"

#include <system_error>

namespace theseed::mapeditor::core {
namespace {

const char* SideName(ProjectOutputSide side) {
    return side == ProjectOutputSide::Client ? "Client" : "Server";
}

bool IsSafeRelative(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;
    for (const auto& component : path) {
        if (component == "..") return false;
    }
    return true;
}

std::filesystem::path StableAbsolute(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) return canonical.lexically_normal();
    ec.clear();
    auto absolute = std::filesystem::absolute(path, ec);
    return (ec ? path : absolute).lexically_normal();
}

} // namespace

std::expected<std::filesystem::path, std::string> ProjectOutputForRelative(
    const std::filesystem::path& projectRoot,
    ProjectOutputSide side,
    const std::filesystem::path& relativePath) {

    if (projectRoot.empty())
        return std::unexpected("Kein Projekt-Ordner konfiguriert.");
    const auto relative = relativePath.lexically_normal();
    if (!IsSafeRelative(relative))
        return std::unexpected("Unsicherer relativer Projektpfad: " + relativePath.string());

    return (projectRoot / SideName(side) / relative).lexically_normal();
}

std::expected<std::filesystem::path, std::string> ProjectOutputForSource(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& sourcePath,
    ProjectOutputSide side) {

    if (sourceRoot.empty())
        return std::unexpected("Kein Quellordner fuer " + std::string(SideName(side)) + " konfiguriert.");
    if (sourcePath.empty())
        return std::unexpected("Leerer Quelldateipfad.");

    const auto root = StableAbsolute(sourceRoot);
    const auto source = StableAbsolute(sourcePath);
    std::error_code ec;
    auto relative = std::filesystem::relative(source, root, ec);
    if (ec || !IsSafeRelative(relative)) {
        return std::unexpected(
            "Quelldatei liegt ausserhalb des konfigurierten " +
            std::string(SideName(side)) + "-Ordners: " + sourcePath.string());
    }
    return ProjectOutputForRelative(projectRoot, side, relative);
}

std::expected<void, std::string> EnsureProjectOutputParent(
    const std::filesystem::path& outputPath) {

    if (outputPath.empty())
        return std::unexpected("Leerer Projekt-Ausgabepfad.");
    std::error_code ec;
    const auto parent = outputPath.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec)
        return std::unexpected(
            "Projekt-Ausgabeverzeichnis konnte nicht angelegt werden: " +
            parent.string() + " (" + ec.message() + ")");
    return {};
}

} // namespace theseed::mapeditor::core
