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
// Entfernt JEDES Leerzeichen im String (nicht nur am Rand) - manche in .nif-Dateien
// eingebetteten Dateinamen enthalten ein zusätzliches Leerzeichen MITTEN im Namen, direkt vor
// der Endung (z.B. "road01_lamp .dds" statt "road01_lamp.dds" - byte-exakt im Original-Asset
// bestätigt, kein Lesefehler). Ein einfaches Rand-Trimmen reicht hier nicht, da das
// Leerzeichen nicht am Anfang/Ende steht.
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
        std::optional<std::filesystem::path> trimmedFallback; // siehe EqualsCaseInsensitiveTrimmed
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
// Bounded rekursive Suche nach EINER Datei mit passendem Namen (case-insensitiv, mit
// Leerzeichen-Toleranz, siehe RemoveAllWhitespace) unter root - für Fälle, in denen ein
// referenzierter Dateiname KEINE Verzeichnisangabe enthält (siehe ResolveLegacyAssetPath:
// Objekt-Texturen in .nif-Dateien sind oft nur der nackte Dateiname, z.B. "grass.dds", ohne
// "fieldTexture\"-Präfix - die Datei kann dann in einem beliebigen Unterordner der geteilten
// Textur-Ablage liegen). Mehrdeutigkeit (mehrere Treffer) wird NICHT aufgelöst - lieber
// nichts finden als eine falsche Textur laden.
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
    // Zwei Ebenen über dem Kartenordner: bei Layout "<AssetRoot>/field/<Karte>/karte.ini" landet
    // man hier genau bei <AssetRoot>, wo z.B. "fieldTexture/" als Geschwister von "field/" liegt.
    const std::filesystem::path assetRoot = mapDir.parent_path().parent_path();
    if (!assetRoot.empty()) {
        if (auto r = ResolveCaseInsensitivePath(assetRoot, stripped)) {
            return r;
        }

        // KORRIGIERT: Objekt-Texturen aus .nif-Dateien sind oft NUR ein nackter Dateiname
        // ohne jede Verzeichnisangabe (z.B. "ELDERIN_wg.DDS", "lightYellow3.dds" - byte-exakt
        // aus echten Testdateien bestätigt) - anders als Heightmap-/Textur-Set-Pfade aus der
        // .ini, die immer den vollen "resmap\field\<Karte>\..."-Pfad enthalten. Für solche
        // Ein-Komponenten-Pfade zusätzlich gezielt in "fieldTexture" suchen (bekannter
        // Geschwisterordner von "field", siehe oben), und falls dort nicht direkt vorhanden,
        // begrenzt rekursiv darin suchen (Texturen können in Unterordnern liegen). NICHT
        // verifiziert gegen echte Textur-Dateien (in dieser Sandbox nicht vorhanden) - falls
        // Objekte danach weiterhin untexturiert bleiben, bitte den tatsächlichen Ablageort
        // der .dds-Dateien relativ zum Client-Ordner mitteilen.
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
                break; // Ordner existiert (Groß-/Kleinschreibung getroffen), weitere Varianten unnötig
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
                return std::nullopt; // mehrdeutig - lieber nichts finden als falsch raten
            }
            match = p;
        }
    }
    return match;
}

} // namespace theseed::mapeditor::core::legacy
