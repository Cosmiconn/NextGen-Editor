#include "mapeditor/core/WalkGridIO.hpp"

#include <cstring>
#include <fstream>

namespace theseed::mapeditor::core {

namespace {

template <typename T>
bool ReadRaw(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

} // namespace

std::expected<LegacyShbdHeaderInfo, std::string> PeekLegacyShbdHeader(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte Legacy-Datei nicht \u00f6ffnen: " + file.string());
    }
    LegacyShbdHeaderInfo info;
    if (!ReadRaw(in, info.quadCount) || !ReadRaw(in, info.height)) {
        return std::unexpected("Unerwartetes Dateiende im Legacy-Header: " + file.string());
    }
    return info;
}

std::expected<WalkGrid, std::string> ImportLegacyShbd(
    const std::filesystem::path& file,
    std::uint32_t width,
    std::uint32_t height,
    LegacyShbdHeader* outHeader) {

    if (width == 0 || height == 0) {
        return std::unexpected("ImportLegacyShbd: width/height m\u00fcssen > 0 sein");
    }

    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte Legacy-Datei nicht \u00f6ffnen: " + file.string());
    }

    LegacyShbdHeader header{};
    in.read(reinterpret_cast<char*>(header.raw), sizeof(header.raw));
    if (!in) {
        return std::unexpected("Unerwartetes Dateiende im Legacy-Header: " + file.string());
    }
    if (outHeader != nullptr) {
        *outHeader = header;
    }

    WalkGrid grid(width, height);
    auto data = grid.MutableData();
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size_bytes()));
    if (!in) {
        return std::unexpected(
            "Unerwartetes Dateiende in den Legacy-Gitterdaten - passen width/height (" +
            std::to_string(width) + "x" + std::to_string(height) + ") zur Datei? " + file.string());
    }

    in.peek();
    if (!in.eof()) {
        return std::unexpected(
            "Datei enth\u00e4lt mehr Daten als erwartet - width/height stimmen vermutlich nicht: " + file.string());
    }

    return grid;
}

std::expected<void, std::string> ExportLegacyShbd(
    const WalkGrid& grid,
    const std::filesystem::path& file,
    LegacyShbdHeader header) {

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte Legacy-Datei nicht zum Schreiben \u00f6ffnen: " + file.string());
    }

    out.write(reinterpret_cast<const char*>(header.raw), sizeof(header.raw));

    const auto data = grid.Data();
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size_bytes()));

    if (!out) {
        return std::unexpected("Fehler beim Schreiben der Legacy-Gitterdaten: " + file.string());
    }
    return {};
}

} // namespace theseed::mapeditor::core
