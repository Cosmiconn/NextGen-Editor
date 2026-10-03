#include "mapeditor/core/ProjectOutput.hpp"

#include <algorithm>
#include <cctype>
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

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::filesystem::path TailFromComponent(
    const std::filesystem::path& source,
    std::initializer_list<const char*> componentNames) {

    std::filesystem::path tail;
    bool copying = false;
    for (const auto& component : source) {
        if (!copying) {
            const std::string lower = LowerAscii(component.string());
            for (const char* name : componentNames) {
                if (lower == LowerAscii(name)) {
                    copying = true;
                    break;
                }
            }
        }
        if (copying) tail /= component;
    }
    return tail;
}

bool IsSameOrDescendant(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root) {

    if (candidate.empty() || root.empty()) return false;
    const auto stableCandidate = StableAbsolute(candidate);
    const auto stableRoot = StableAbsolute(root);
    std::error_code ec;
    const auto relative = std::filesystem::relative(stableCandidate, stableRoot, ec);
    if (ec || relative.is_absolute()) return false;
    for (const auto& component : relative) {
        if (component == "..") return false;
    }
    return true;
}

std::filesystem::path CanonicalRuntimeRelative(
    const std::filesystem::path& source,
    const std::filesystem::path& sourceRelative,
    ProjectOutputSide side) {

    if (side == ProjectOutputSide::Client) {
        auto anchored = TailFromComponent(
            source, {"ressystem", "resmap", "resmenu", "resitem", "reschar"});
        if (!anchored.empty()) return anchored.lexically_normal();
        return sourceRelative;
    }

    // Standard Fiesta server project layout is always Server/9Data/Shine/...
    // even when the user selected 9Data or Shine itself as the read-only source root.
    auto from9Data = TailFromComponent(source, {"9data"});
    if (!from9Data.empty()) return from9Data.lexically_normal();

    auto fromShine = TailFromComponent(source, {"shine"});
    if (!fromShine.empty())
        return (std::filesystem::path("9Data") / fromShine).lexically_normal();

    return sourceRelative;
}

} // namespace

std::expected<void, std::string> ValidateProjectOutputRoots(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& clientSourceRoot,
    const std::filesystem::path& serverSourceRoot) {

    if (projectRoot.empty())
        return std::unexpected("Kein Projekt-Ordner konfiguriert.");

    for (const auto& [label, sourceRoot] : {
             std::pair<const char*, std::filesystem::path>{"Client", clientSourceRoot},
             std::pair<const char*, std::filesystem::path>{"Server", serverSourceRoot}}) {
        if (sourceRoot.empty()) continue;
        if (IsSameOrDescendant(projectRoot, sourceRoot) ||
            IsSameOrDescendant(sourceRoot, projectRoot)) {
            return std::unexpected(
                "Projektordner und read-only " + std::string(label) +
                "-Quelle ueberlappen physisch: " + projectRoot.string() +
                " <-> " + sourceRoot.string());
        }
    }
    return {};
}

std::expected<std::filesystem::path, std::string> ProjectOutputForRelative(
    const std::filesystem::path& projectRoot,
    ProjectOutputSide side,
    const std::filesystem::path& relativePath) {

    if (projectRoot.empty())
        return std::unexpected("Kein Projekt-Ordner konfiguriert.");
    const auto relative = relativePath.lexically_normal();
    if (!IsSafeRelative(relative))
        return std::unexpected("Unsicherer relativer Projektpfad: " + relativePath.string());

    const auto sideRoot = (projectRoot / SideName(side)).lexically_normal();
    // Resolve existing path components before accepting the target. This prevents a
    // project-side Client/Server directory (or a nested component) from being a symlink/
    // junction that physically points outside the project tree and back into a source install.
    if (!IsSameOrDescendant(sideRoot, projectRoot))
        return std::unexpected(
            "Projekt-" + std::string(SideName(side)) +
            "-Ordner verlaesst physisch den Projektordner: " + sideRoot.string());

    const auto target = (sideRoot / relative).lexically_normal();
    if (!IsSameOrDescendant(target, sideRoot))
        return std::unexpected(
            "Projekt-Ausgabepfad verlaesst physisch den " + std::string(SideName(side)) +
            "-Projektbaum: " + target.string());
    return target;
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

    const auto separated = ValidateProjectOutputRoots(
        projectRoot,
        side == ProjectOutputSide::Client ? sourceRoot : std::filesystem::path{},
        side == ProjectOutputSide::Server ? sourceRoot : std::filesystem::path{});
    if (!separated) return std::unexpected(separated.error());

    const auto root = StableAbsolute(sourceRoot);
    const auto source = StableAbsolute(sourcePath);
    std::error_code ec;
    auto relative = std::filesystem::relative(source, root, ec);
    if (ec || !IsSafeRelative(relative)) {
        return std::unexpected(
            "Quelldatei liegt ausserhalb des konfigurierten " +
            std::string(SideName(side)) + "-Ordners: " + sourcePath.string());
    }
    const auto canonicalRelative = CanonicalRuntimeRelative(source, relative, side);
    if (!IsSafeRelative(canonicalRelative))
        return std::unexpected("Unsicherer kanonischer Projektpfad fuer Quelle: " + sourcePath.string());
    return ProjectOutputForRelative(projectRoot, side, canonicalRelative);
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
