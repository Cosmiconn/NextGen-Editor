#include "mapeditor/core/legacy/LegacyIdmAid.hpp"

#include <cstring>
#include <fstream>
#include <vector>

namespace theseed::mapeditor::core::legacy {

namespace {

template <typename T>
void WriteRaw(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadRaw(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

std::size_t PortableStrnlen(const char* buf, std::size_t maxLen) {
    const void* found = std::memchr(buf, '\0', maxLen);
    return found != nullptr ? static_cast<const char*>(found) - buf : maxLen;
}

} // namespace

std::expected<ObjectSpatialIndex, std::string> ParseLegacyIdm(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected("Konnte .idm nicht öffnen: " + file.string());

    ObjectSpatialIndex index;
    index.hash.resize(32);
    in.read(index.hash.data(), 32);
    if (!in) return std::unexpected("Unerwartetes Dateiende im Hash-Header: " + file.string());

    char newline = 0;
    if (!ReadRaw(in, newline) || newline != '\n') {
        return std::unexpected("Erwarteter Zeilenumbruch nach Hash fehlt: " + file.string());
    }
    if (!ReadRaw(in, index.headerValue)) {
        return std::unexpected("Unerwartetes Dateiende beim führenden Wert: " + file.string());
    }

    while (true) {
        std::int32_t count = 0;
        if (!ReadRaw(in, count)) break;
        if (count < 0) {
            return std::unexpected("Negativer Gruppen-Count in .idm - Datei korrupt oder Format falsch verstanden: " + file.string());
        }
        SpatialIndexGroup group;
        group.indices.resize(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i) {
            if (!ReadRaw(in, group.indices[static_cast<std::size_t>(i)])) {
                return std::unexpected("Unerwartetes Dateiende innerhalb einer Gruppe: " + file.string());
            }
        }
        index.groups.push_back(std::move(group));
    }
    return index;
}

std::expected<void, std::string> SerializeLegacyIdm(const ObjectSpatialIndex& index, const std::filesystem::path& file) {
    if (index.hash.size() != 32) {
        return std::unexpected("SerializeLegacyIdm: hash muss exakt 32 Zeichen lang sein");
    }
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return std::unexpected("Konnte .idm nicht zum Schreiben öffnen: " + file.string());

    out.write(index.hash.data(), 32);
    out.put('\n');
    WriteRaw(out, index.headerValue);
    for (const auto& group : index.groups) {
        WriteRaw(out, static_cast<std::int32_t>(group.indices.size()));
        for (const auto idx : group.indices) WriteRaw(out, idx);
    }
    if (!out) return std::unexpected("Fehler beim Schreiben der .idm: " + file.string());
    return {};
}

std::expected<ZoneMetadata, std::string> ParseLegacyAid(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected("Konnte .aid nicht öffnen: " + file.string());

    ZoneMetadata zone;
    if (!ReadRaw(in, zone.recordType)) {
        return std::unexpected("Unerwartetes Dateiende bei recordType: " + file.string());
    }
    char nameBuf[32]{};
    in.read(nameBuf, sizeof(nameBuf));
    if (!in) return std::unexpected("Unerwartetes Dateiende im Namensfeld: " + file.string());

    std::memcpy(zone.rawNameBuffer, nameBuf, sizeof(nameBuf));
    zone.hasRawNameBuffer = true;
    const std::size_t len = PortableStrnlen(nameBuf, sizeof(nameBuf));
    zone.name.assign(nameBuf, len);

    if (!ReadRaw(in, zone.flag)) {
        return std::unexpected("Unerwartetes Dateiende bei flag: " + file.string());
    }
    for (float& b : zone.bounds) {
        if (!ReadRaw(in, b)) return std::unexpected("Unerwartetes Dateiende in bounds: " + file.string());
    }
    return zone;
}

std::expected<void, std::string> SerializeLegacyAid(const ZoneMetadata& zone, const std::filesystem::path& file) {
    if (zone.name.size() > 32) {
        return std::unexpected("SerializeLegacyAid: Zonenname länger als 32 Byte (Legacy-Feldgröße)");
    }
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return std::unexpected("Konnte .aid nicht zum Schreiben öffnen: " + file.string());

    WriteRaw(out, zone.recordType);
    char nameBuf[32]{};
    bool useRaw = false;
    if (zone.hasRawNameBuffer) {
        const std::size_t rawLen = PortableStrnlen(reinterpret_cast<const char*>(zone.rawNameBuffer), sizeof(zone.rawNameBuffer));
        useRaw = (zone.name.size() == rawLen && std::memcmp(zone.name.data(), zone.rawNameBuffer, rawLen) == 0);
    }
    if (useRaw) std::memcpy(nameBuf, zone.rawNameBuffer, sizeof(nameBuf));
    else std::memcpy(nameBuf, zone.name.data(), zone.name.size());
    out.write(nameBuf, sizeof(nameBuf));

    WriteRaw(out, zone.flag);
    for (const float b : zone.bounds) WriteRaw(out, b);
    if (!out) return std::unexpected("Fehler beim Schreiben der .aid: " + file.string());
    return {};
}

} // namespace theseed::mapeditor::core::legacy
