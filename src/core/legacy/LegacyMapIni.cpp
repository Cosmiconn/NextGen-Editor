#include "mapeditor/core/legacy/LegacyMapIni.hpp"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <algorithm>

namespace theseed::mapeditor::core::legacy {

namespace {

std::string Trim(const std::string& s) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    auto begin = s.begin();
    while (begin != s.end() && isSpace(static_cast<unsigned char>(*begin))) ++begin;
    auto end = s.end();
    while (end != begin && isSpace(static_cast<unsigned char>(*(end - 1)))) --end;
    return std::string(begin, end);
}

float ParseFloatSafe(const std::string& s) {
    try {
        return std::stof(s);
    } catch (...) {
        return 0.0f;
    }
}

std::uint32_t ParseUIntSafe(const std::string& s) {
    try {
        return static_cast<std::uint32_t>(std::stoul(s));
    } catch (...) {
        return 0;
    }
}

std::string CanonicalText(const LegacyMapIni& ini);

} // namespace

std::expected<LegacyMapIni, std::string> ParseLegacyMapIni(const std::filesystem::path& file) {
    std::ifstream source(file, std::ios::binary);
    std::string original((std::istreambuf_iterator<char>(source)), {});
    std::istringstream in(original);
    if (!source) {
        return std::unexpected("Konnte .ini nicht \u00f6ffnen: " + file.string());
    }

    LegacyMapIni result;
    bool pendingLayer = false;
    bool inLayer = false;
    LegacyLayerDef currentLayer;

    std::string rawLine;
    while (std::getline(in, rawLine)) {
        const auto commentPos = rawLine.find("//");
        std::string line = (commentPos != std::string::npos) ? rawLine.substr(0, commentPos) : rawLine;
        line = Trim(line);
        if (line.empty()) continue;
        if (line == "#END_FILE") break;

        if (line == "{") {
            if (pendingLayer) {
                inLayer = true;
                currentLayer = LegacyLayerDef{};
                pendingLayer = false;
            }
            continue;
        }
        if (line == "}") {
            if (inLayer) {
                result.layers.push_back(currentLayer);
                inLayer = false;
            }
            continue;
        }
        if (line == "#Layer") {
            pendingLayer = true;
            continue;
        }
        if (line.front() != '#') {
            continue; // unerwartetes Format - defensiv \u00fcberspringen statt abzubrechen
        }

        const auto colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }
        const std::string key = Trim(line.substr(1, colonPos - 1));
        std::string value = Trim(line.substr(colonPos + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (inLayer) {
            if (key == "Name") currentLayer.name = value;
            else if (key == "DiffuseFileName") currentLayer.diffuseFileName = value;
            else if (key == "BlendFileName") currentLayer.blendFileName = value;
            else if (key == "StartPos_X") currentLayer.startX = ParseFloatSafe(value);
            else if (key == "StartPos_Y") currentLayer.startY = ParseFloatSafe(value);
            else if (key == "Width") currentLayer.width = ParseFloatSafe(value);
            else if (key == "Height") currentLayer.height = ParseFloatSafe(value);
            else if (key == "UVScaleDiffuse") currentLayer.uvScaleDiffuse = ParseFloatSafe(value);
            else if (key == "UVScaleBlend") currentLayer.uvScaleBlend = ParseFloatSafe(value);
        } else {
            if (key == "HeightFileName") result.heightFileName = value;
            else if (key == "VerTexColorTexture") result.vertexColorTexture = value;
            else if (key == "HEIGHTMAP_WIDTH") result.heightmapWidth = ParseUIntSafe(value);
            else if (key == "HEIGHTMAP_HEIGHT") result.heightmapHeight = ParseUIntSafe(value);
            else if (key == "OneBlockWidth") result.oneBlockWidth = ParseFloatSafe(value);
            else if (key == "OneBlockHeight") result.oneBlockHeight = ParseFloatSafe(value);
            else if (key == "QuadsWide") result.quadsWide = ParseUIntSafe(value);
            else if (key == "QuadsHigh") result.quadsHigh = ParseUIntSafe(value);
            // PGFILE / FILE_VER bewusst ignoriert (f\u00fcr keines der vier Module ben\u00f6tigt).
        }
    }

    if (result.heightmapWidth == 0 || result.heightmapHeight == 0) {
        return std::unexpected("HEIGHTMAP_WIDTH/HEIGHT fehlt oder ist 0 in: " + file.string());
    }

    // Absicherung: OneBlockWidth/-Height fehlt in manchen .ini oder ist nicht als Zahl lesbar ->
    // ParseFloatSafe liefert dann 0.0. Eine Blockgroesse von 0 fuehrt spaeter in
    // Heightmap::SampleWorld zu einer Division durch 0 (0/0 = NaN bei Weltkoordinate 0, also
    // genau am haeufig angesteuerten Kartenrand) - das Casten von NaN nach uint32_t ist
    // undefiniertes Verhalten und loeste den Heightmap::At-Assert aus ("ENABLE ASSERT am
    // Mauszeiger", CHANGELOG [0.44.34]). Bei fehlendem/ungueltigem Wert auf den in
    // LegacyMapIni.hpp dokumentierten Standard (50.0) zurueckfallen statt 0 durchzureichen.
    if (!(result.oneBlockWidth > 0.0f) || !std::isfinite(result.oneBlockWidth)) result.oneBlockWidth = 50.0f;
    if (!(result.oneBlockHeight > 0.0f) || !std::isfinite(result.oneBlockHeight)) result.oneBlockHeight = 50.0f;

    result.originalText = std::move(original);
    result.originalCanonical = CanonicalText(result);
    return result;
}

namespace {
std::string CanonicalText(const LegacyMapIni& ini) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "#PGFILE : HeightMap\n";
    out << "#FILE_VER : 0.01\n\n";
    out << "#HeightFileName : \"" << ini.heightFileName << "\"\n";
    out << "#VerTexColorTexture : \"" << ini.vertexColorTexture << "\"\n\n";
    out << "#HEIGHTMAP_WIDTH : " << ini.heightmapWidth << "\n";
    out << "#HEIGHTMAP_HEIGHT : " << ini.heightmapHeight << "\n\n";
    out << "#OneBlockWidth : " << ini.oneBlockWidth << "f\n";
    out << "#OneBlockHeight : " << ini.oneBlockHeight << "f\n\n";
    out << "#QuadsWide : " << ini.quadsWide << "\n";
    out << "#QuadsHigh : " << ini.quadsHigh << "\n\n";

    for (const auto& layer : ini.layers) {
        out << "#Layer\n{\n";
        out << "\t#Name : " << layer.name << "\n";
        out << "\t#DiffuseFileName : \"" << layer.diffuseFileName << "\"\n";
        out << "\t#BlendFileName : \"" << layer.blendFileName << "\"\n";
        out << "\t#StartPos_X : " << layer.startX << "f\n";
        out << "\t#StartPos_Y : " << layer.startY << "f\n";
        out << "\t#Width : " << layer.width << "f\n";
        out << "\t#Height : " << layer.height << "f\n";
        out << "\t#UVScaleDiffuse : " << layer.uvScaleDiffuse << "f\n";
        out << "\t#UVScaleBlend : " << layer.uvScaleBlend << "f\n";
        out << "}\n";
    }

    out << "\n#END_FILE\n";


    return out.str();
}
struct TextBlocks {
    std::string global;
    std::vector<std::string> layers;
    std::string tail;
};
TextBlocks SplitBlocks(const std::string& text) {
    TextBlocks result;
    bool layer = false, tail = false;
    std::istringstream in(text); std::string line;
    while (std::getline(in, line)) {
        const auto token = Trim(line.substr(0, line.find("//")));
        const std::string bytes = line + (in.eof() ? "" : "\n");
        if (token == "#END_FILE") tail = true;
        if (tail) { result.tail += bytes; continue; }
        if (token == "#Layer") { layer = true; result.layers.emplace_back(); }
        (layer ? result.layers.back() : result.global) += bytes;
        if (token == "}") layer = false;
    }
    return result;
}
using Values = std::map<std::string, std::string>;
std::pair<std::string, std::string> KeyValue(const std::string& line) {
    const auto token = Trim(line.substr(0, line.find("//")));
    const auto colon = token.find(':');
    if (token.empty() || token.front() != '#' || colon == std::string::npos) return {};
    return {Trim(token.substr(1, colon - 1)), Trim(token.substr(colon + 1))};
}
Values ReadValues(const std::string& text) {
    Values values; std::istringstream in(text); std::string line;
    while (std::getline(in, line)) {
        auto [key, value] = KeyValue(line);
        if (!key.empty()) values[key] = value;
    }
    return values;
}
std::string PatchBlock(const std::string& source, const std::string& baseline, const std::string& current) {
    const auto old = ReadValues(baseline), now = ReadValues(current);
    Values changes;
    for (const auto& [key, value] : now) {
        const auto it = old.find(key);
        if (it == old.end() || it->second != value) changes[key] = value;
    }
    if (changes.empty()) return source;
    const std::string newline = source.find("\r\n") == std::string::npos ? "\n" : "\r\n";
    std::string result; std::set<std::string> written;
    const auto appendMissing = [&] {
        for (const auto& [key, value] : changes) if (!written.contains(key)) {
            if (!result.empty() && result.back() != '\n') result += newline;
            result += "#" + key + " : " + value + newline;
            written.insert(key);
        }
    };
    std::istringstream in(source); std::string line;
    while (std::getline(in, line)) {
        if (Trim(line) == "}") appendMissing();
        const auto [key, value] = KeyValue(line);
        if (const auto changed = changes.find(key); changed != changes.end()) {
            const auto colon = line.find(':');
            const auto comment = line.find("//", colon);
            std::string suffix = comment == std::string::npos ? (line.ends_with('\r') ? "\r" : "") : " " + line.substr(comment);
            line = line.substr(0, colon + 1) + " " + changed->second + suffix;
            written.insert(key);
        }
        result += line;
        if (!in.eof()) result += '\n';
    }
    appendMissing();
    return result;
}
} // namespace

std::expected<void, std::string> SerializeLegacyMapIni(const LegacyMapIni& ini, const std::filesystem::path& file) {
    const auto canonical = CanonicalText(ini);
    std::string text = canonical;
    if (!ini.originalText.empty()) {
        if (canonical == ini.originalCanonical) text = ini.originalText;
        else {
            const auto source = SplitBlocks(ini.originalText);
            const auto baseline = SplitBlocks(ini.originalCanonical);
            const auto current = SplitBlocks(canonical);
            text = PatchBlock(source.global, baseline.global, current.global);
            for (std::size_t i = 0; i < current.layers.size(); ++i) {
                if (!text.empty() && text.back() != '\n') text += '\n';
                text += i < source.layers.size() && i < baseline.layers.size()
                    ? PatchBlock(source.layers[i], baseline.layers[i], current.layers[i]) : current.layers[i];
            }
            text += source.tail;
        }
    }
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) return std::unexpected("Fehler beim Schreiben der .ini: " + file.string());
    return {};
}

} // namespace theseed::mapeditor::core::legacy
