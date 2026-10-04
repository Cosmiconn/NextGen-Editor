#include "mapeditor/core/legacy/MapRenderSettings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace theseed::mapeditor::core::legacy {

namespace {

template <class T>
bool ReadLe(const std::vector<std::uint8_t>& d, std::size_t off, T& out) {
    if (off + sizeof(T) > d.size()) return false;
    std::memcpy(&out, d.data() + off, sizeof(T));
    return true;
}

std::string Trim(std::string_view s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return std::string(s.substr(a, b - a));
}

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool ParseBool(const std::string& v, bool& out) {
    const std::string l = Lower(v);
    if (l == "true" || l == "1" || l == "yes" || l == "on") { out = true; return true; }
    if (l == "false" || l == "0" || l == "no" || l == "off") { out = false; return true; }
    return false;
}

bool ParseNumber(const std::string& v, float& out) {
    if (v.empty()) return false;
    char* end = nullptr;
    const float f = std::strtof(v.c_str(), &end);
    if (end == v.c_str() || !std::isfinite(f)) return false;
    while (*end != '\0' && (std::isspace(static_cast<unsigned char>(*end)) || *end == 'f' || *end == 'F')) ++end;
    if (*end != '\0') return false;
    out = f;
    return true;
}

} // namespace

std::expected<RgbImage, std::string> ReadBmpRgb(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected("BMP nicht lesbar: " + file.string());
    const std::vector<std::uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') return std::unexpected("Keine BMP-Datei: " + file.string());
    std::uint32_t dataOffset = 0, compression = 0;
    std::int32_t width = 0, height = 0;
    std::uint16_t bitCount = 0;
    if (!ReadLe(d, 10, dataOffset) || !ReadLe(d, 18, width) || !ReadLe(d, 22, height) ||
        !ReadLe(d, 28, bitCount) || !ReadLe(d, 30, compression))
        return std::unexpected("BMP-Header unvollständig: " + file.string());
    if (compression != 0 || bitCount != 24)
        return std::unexpected("Nur unkomprimierte 24-bit-BMPs werden unterstützt: " + file.string());
    if (width <= 0 || height == 0 || width > 16384 || std::abs(height) > 16384)
        return std::unexpected("Ungültige BMP-Dimensionen: " + file.string());
    RgbImage img;
    img.width = static_cast<std::uint32_t>(width);
    img.height = static_cast<std::uint32_t>(std::abs(height));
    const std::size_t rowSize = (static_cast<std::size_t>(img.width) * 3u + 3u) & ~std::size_t{3};
    if (dataOffset + rowSize * img.height > d.size())
        return std::unexpected("BMP-Pixeldaten unvollständig: " + file.string());
    img.rgb.resize(static_cast<std::size_t>(img.width) * img.height * 3u);
    for (std::uint32_t y = 0; y < img.height; ++y) {
        // Gleiche Konvention wie ReadBlendMapBmp: Dateizeile y == Gitterzeile z (kein Flip).
        const std::uint8_t* src = d.data() + dataOffset + rowSize * y;
        std::uint8_t* dst = img.rgb.data() + static_cast<std::size_t>(y) * img.width * 3u;
        for (std::uint32_t x = 0; x < img.width; ++x) {
            dst[x * 3 + 0] = src[x * 3 + 2]; // R
            dst[x * 3 + 1] = src[x * 3 + 1]; // G
            dst[x * 3 + 2] = src[x * 3 + 0]; // B
        }
    }
    return img;
}

MapRenderConfig ParseMapRenderConfig(std::string_view text) {
    MapRenderConfig cfg;
    cfg.loaded = true;
    std::string section;
    std::size_t pos = 0;
    int lineNo = 0;
    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        std::string line = Trim(text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos));
        pos = nl == std::string_view::npos ? text.size() + 1 : nl + 1;
        ++lineNo;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[') {
            const auto close = line.find(']');
            section = Trim(std::string_view(line).substr(1, close == std::string::npos ? std::string::npos : close - 1));
            cfg.sections[section];
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            cfg.warnings.push_back("Zeile " + std::to_string(lineNo) + ": kein '=' gefunden");
            continue;
        }
        const std::string key = Trim(std::string_view(line).substr(0, eq));
        const std::string value = Trim(std::string_view(line).substr(eq + 1));
        cfg.sections[section][key] = value;

        const std::string s = Lower(section), k = Lower(key);
        auto number = [&](float& target) {
            if (!ParseNumber(value, target))
                cfg.warnings.push_back("[" + section + "] " + key + ": ungültige Zahl '" + value + "'");
        };
        if (s == "worldsetting" && k == "ground_dl_enable") {
            if (ParseBool(value, cfg.groundDirectionalLight)) cfg.groundDirectionalLightPresent = true;
            else cfg.warnings.push_back("[WorldSetting] Ground_DL_Enable: ungültiger Wahrheitswert '" + value + "'");
        } else if (s == "glowscreeneffect") {
            cfg.glow.present = true;
            if (k == "glowness") number(cfg.glow.glowness);
            else if (k == "blendfactor") number(cfg.glow.blendFactor);
            else if (k == "gaussfactor") number(cfg.glow.gaussFactor);
            else if (k == "downscaling") number(cfg.glow.downScaling);
            else if (k == "numblurring") {
                float n = 0.0f;
                if (ParseNumber(value, n)) cfg.glow.numBlurring = std::clamp(static_cast<int>(std::lround(n)), 0, 32);
                else cfg.warnings.push_back("[GlowScreenEffect] NumBlurring: ungültige Zahl '" + value + "'");
            }
        }
    }
    return cfg;
}

std::expected<MapRenderConfig, std::string> LoadMapRenderConfig(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected("Konfiguration nicht lesbar: " + file.string());
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return ParseMapRenderConfig(text);
}

} // namespace theseed::mapeditor::core::legacy
