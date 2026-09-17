#include "mapeditor/core/legacy/LegacyMapIni.hpp"

#include <cctype>
#include <fstream>

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

} // namespace

std::expected<LegacyMapIni, std::string> ParseLegacyMapIni(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
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

    return result;
}

std::expected<void, std::string> SerializeLegacyMapIni(const LegacyMapIni& ini, const std::filesystem::path& file) {
    std::ofstream out(file, std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte .ini nicht zum Schreiben \u00f6ffnen: " + file.string());
    }

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

    if (!out) {
        return std::unexpected("Fehler beim Schreiben der .ini: " + file.string());
    }
    return {};
}

} // namespace theseed::mapeditor::core::legacy
