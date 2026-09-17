#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

#include <algorithm>

namespace theseed::mapeditor::core::legacy {

std::filesystem::path LegacyPathToNative(const std::string& legacyPath) {
    std::string p = legacyPath;
    if (p.size() >= 2 && p[0] == '.' && (p[1] == '\\' || p[1] == '/')) {
        p = p.substr(2);
    }
    for (char& c : p) {
        if (c == '\\') c = '/';
    }
    return std::filesystem::path(p);
}

bool EqualsCaseInsensitive(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
        return std::tolower(x) == std::tolower(y);
    });
}

namespace {
std::string RemoveAllWhitespace(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) out.push_back(c);
    }
    return out;
}

bool EqualsCaseInsensitiveTrimmed(const std::string& a, const std::string& b) {
    return EqualsCaseInsensitive(RemoveAllWhitespace(a), RemoveAllWhitespace(b));
}
} // namespace

std::filesystem::path StripResmapPrefix(const std::filesystem::path& p) {
    auto it = p.begin();
    if (it != p.end() && EqualsCaseInsensitive(it->string(), "resmap")) {
        std::filesystem::path rest;
        for (auto rest_it = std::next(it); rest_it != p.end(); ++rest_it) {
            rest /= *rest_it;
        }
        return rest;
    }
    return p;
}

std::optional<std::filesystem::path> ResolveCaseInsensitivePath(
    const std::filesystem::path& root, const std::filesystem::path& relative) {
    std::filesystem::path current = root;
    for (const auto& part : relative) {
        if (part.empty() || part == ".") continue;

        std::error_code existsEc;
        if (std::filesystem::exists(current / part, existsEc)) {
            current = current / part;
            continue;
        }

        bool found = false;
        std::optional<std::filesystem::path> trimmedFallback;
        std::error_code iterEc;
        std::filesystem::directory_iterator it(current, iterEc);
        if (!iterEc) {
            for (const auto& entry : it) {
                const std::string entryName = entry.path().filename().string();
                if (EqualsCaseInsensitive(entryName, part.string())) {
                    current = entry.path();
                    found = true;
                    break;
                }
                if (!trimmedFallback && EqualsCaseInsensitiveTrimmed(entryName, part.string())) {
                    trimmedFallback = entry.path();
                }
            }
        }
        if (!found && trimmedFallback) {
            current = *trimmedFallback;
            found = true;
        }
        if (!found) return std::nullopt;
    }
    return current;
}

namespace {
std::optional<std::filesystem::path> FindFileRecursiveBounded(
    const std::filesystem::path& root, const std::string& filename, int maxDepth) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return std::nullopt;
    }
    std::optional<std::filesystem::path> match;
    bool ambiguous = false;
    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; it != end && !ec; it.increment(ec)) {
        if (it.depth() >= maxDepth) {
            it.disable_recursion_pending();
        }
        if (!it->is_regular_file(ec)) continue;
        if (EqualsCaseInsensitiveTrimmed(it->path().filename().string(), filename)) {
            if (match.has_value()) {
                ambiguous = true;
                break;
            }
            match = it->path();
        }
    }
    return ambiguous ? std::nullopt : match;
}
} // namespace

std::optional<std::filesystem::path> ResolveLegacyAssetPath(
    const std::filesystem::path& mapDir, const std::string& legacyPath) {
    const std::filesystem::path stripped = StripResmapPrefix(LegacyPathToNative(legacyPath));

    if (auto r = ResolveCaseInsensitivePath(mapDir, stripped)) {
        return r;
    }
    const std::filesystem::path assetRoot = mapDir.parent_path().parent_path();
    if (!assetRoot.empty()) {
        if (auto r = ResolveCaseInsensitivePath(assetRoot, stripped)) {
            return r;
        }

        if (std::distance(stripped.begin(), stripped.end()) == 1) {
            const std::string bareName = stripped.filename().string();
            for (const char* sharedFolder : {"fieldTexture", "FieldTexture", "fieldtexture"}) {
                std::error_code ec;
                const std::filesystem::path candidateDir = assetRoot / sharedFolder;
                if (!std::filesystem::exists(candidateDir, ec)) continue;
                if (auto direct = ResolveCaseInsensitivePath(candidateDir, bareName)) {
                    return direct;
                }
                if (auto nested = FindFileRecursiveBounded(candidateDir, bareName, 4)) {
                    return nested;
                }
                break;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> FindSiblingFileByStem(
    const std::filesystem::path& dir, const std::string& stem, const std::string& extension) {
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) return std::nullopt;

    std::optional<std::filesystem::path> match;
    for (const auto& entry : it) {
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();
        if (EqualsCaseInsensitive(p.stem().string(), stem) && EqualsCaseInsensitive(p.extension().string(), extension)) {
            if (match.has_value()) {
                return std::nullopt;
            }
            match = p;
        }
    }
    return match;
}

} // namespace theseed::mapeditor::core::legacy
