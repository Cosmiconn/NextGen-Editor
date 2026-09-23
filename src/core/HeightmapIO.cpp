#include "mapeditor/core/HeightmapIO.hpp"

#include <cstring>
#include <fstream>

namespace theseed::mapeditor::core {

namespace {

constexpr char kTshmMagic[4] = {'T', 'S', 'H', 'M'};
constexpr std::uint32_t kTshmVersion = 1;

template <typename T>
void WriteRaw(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadRaw(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

} // namespace

std::expected<Heightmap, std::string> LoadTshm(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte Datei nicht \u00f6ffnen: " + file.string());
    }

    char magic[4]{};
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, kTshmMagic, sizeof(magic)) != 0) {
        return std::unexpected("Ung\u00fcltige .tshm-Datei (Magic stimmt nicht): " + file.string());
    }

    std::uint32_t version = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float blockWidth = 0.0f;
    float blockHeight = 0.0f;

    if (!ReadRaw(in, version) || !ReadRaw(in, width) || !ReadRaw(in, height) ||
        !ReadRaw(in, blockWidth) || !ReadRaw(in, blockHeight)) {
        return std::unexpected("Unerwartetes Dateiende im Header: " + file.string());
    }
    if (version != kTshmVersion) {
        return std::unexpected("Nicht unterst\u00fctzte .tshm-Version: " + std::to_string(version));
    }
    if (width == 0 || height == 0) {
        return std::unexpected(".tshm-Datei mit ung\u00fcltigen Dimensionen 0x0: " + file.string());
    }

    Heightmap heightmap(width, height, blockWidth, blockHeight);
    auto data = heightmap.MutableData();
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size_bytes()));
    if (!in) {
        return std::unexpected("Unerwartetes Dateiende in den H\u00f6hendaten: " + file.string());
    }

    return heightmap;
}

std::expected<void, std::string> SaveTshm(const Heightmap& heightmap, const std::filesystem::path& file) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte Datei nicht zum Schreiben \u00f6ffnen: " + file.string());
    }

    out.write(kTshmMagic, sizeof(kTshmMagic));
    WriteRaw(out, kTshmVersion);
    WriteRaw(out, heightmap.Width());
    WriteRaw(out, heightmap.Height());
    WriteRaw(out, heightmap.BlockWidth());
    WriteRaw(out, heightmap.BlockHeight());

    const auto data = heightmap.Data();
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size_bytes()));

    if (!out) {
        return std::unexpected("Fehler beim Schreiben der H\u00f6hendaten: " + file.string());
    }
    return {};
}

std::expected<Heightmap, std::string> ImportLegacyHtd(
    const std::filesystem::path& file,
    std::uint32_t width,
    std::uint32_t height,
    float blockWidth,
    float blockHeight,
    LegacyHtdHeader* outHeader,
    std::vector<std::uint8_t>* outTrailingBytes) {

    if (width == 0 || height == 0) {
        return std::unexpected("ImportLegacyHtd: width/height m\u00fcssen > 0 sein (aus begleitender .ini lesen)");
    }

    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte Legacy-Datei nicht \u00f6ffnen: " + file.string());
    }

    LegacyHtdHeader header{};
    in.read(reinterpret_cast<char*>(header.raw), sizeof(header.raw));
    if (!in) {
        return std::unexpected("Unerwartetes Dateiende im Legacy-Header: " + file.string());
    }
    if (outHeader != nullptr) {
        *outHeader = header;
    }

    Heightmap heightmap(width, height, blockWidth, blockHeight);
    auto data = heightmap.MutableData();
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size_bytes()));
    if (!in) {
        return std::unexpected(
            "Unerwartetes Dateiende in den Legacy-H\u00f6hendaten - passen width/height (" +
            std::to_string(width) + "x" + std::to_string(height) + ") zur Datei? " + file.string());
    }

    // Manche echten Legacy-Dateien haben zusätzliche Daten NACH dem reinen Höhenraster
    // (beobachtet bei UrgDark01.HTD/UrgSwa01.HTD/BigCoast.HTD - Bedeutung nicht gesichert,
    // Länge variiert zwischen 4 und über 5000 Byte). Früher wurde das als Fehler behandelt
    // (verhinderte den Import dieser Karten komplett) - jetzt werden die Bytes unverändert
    // mitgeführt, damit ein Re-Export weiterhin byte-exakt bleibt, ohne den Import zu blockieren.
    if (outTrailingBytes != nullptr) {
        outTrailingBytes->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    return heightmap;
}

std::expected<void, std::string> ExportLegacyHtd(
    const Heightmap& heightmap,
    const std::filesystem::path& file,
    LegacyHtdHeader header,
    const std::vector<std::uint8_t>& trailingBytes) {

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte Legacy-Datei nicht zum Schreiben \u00f6ffnen: " + file.string());
    }

    out.write(reinterpret_cast<const char*>(header.raw), sizeof(header.raw));

    const auto data = heightmap.Data();
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size_bytes()));

    if (!trailingBytes.empty()) {
        out.write(reinterpret_cast<const char*>(trailingBytes.data()), static_cast<std::streamsize>(trailingBytes.size()));
    }

    if (!out) {
        return std::unexpected("Fehler beim Schreiben der Legacy-H\u00f6hendaten: " + file.string());
    }
    return {};
}

} // namespace theseed::mapeditor::core
