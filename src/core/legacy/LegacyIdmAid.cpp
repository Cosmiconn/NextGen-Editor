#include "mapeditor/core/legacy/LegacyIdmAid.hpp"

#include <set>

#include <algorithm>
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

// Portable Alternative zu strnlen (reine C++-Standardbibliothek, keine POSIX-Abhängigkeit -
// TheSeed muss auch unter Windows/MSVC bauen).
std::size_t PortableStrnlen(const char* buf, std::size_t maxLen) {
    const void* found = std::memchr(buf, '\0', maxLen);
    return found != nullptr ? static_cast<const char*>(found) - buf : maxLen;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// .idm
// ---------------------------------------------------------------------------------------------

std::expected<ObjectSpatialIndex, std::string> ParseLegacyIdm(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte .idm nicht \u00f6ffnen: " + file.string());
    }

    ObjectSpatialIndex index;
    index.hash.resize(32);
    in.read(index.hash.data(), 32);
    if (!in) {
        return std::unexpected("Unerwartetes Dateiende im Hash-Header: " + file.string());
    }

    char newline = 0;
    if (!ReadRaw(in, newline) || newline != '\n') {
        return std::unexpected("Erwarteter Zeilenumbruch nach Hash fehlt: " + file.string());
    }

    if (!ReadRaw(in, index.headerValue)) {
        return std::unexpected("Unerwartetes Dateiende beim f\u00fchrenden Wert: " + file.string());
    }

    const auto payloadStart = in.tellg();
    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    in.seekg(payloadStart);

    while (true) {
        std::int32_t count = 0;
        if (!ReadRaw(in, count)) {
            if (in.gcount() != 0 || !in.eof()) return std::unexpected("Abgeschnittener IDM-Gruppenzaehler");
            break;
        }
        if (count < 0 || static_cast<std::uint64_t>(count) * 4 > static_cast<std::uint64_t>(end - in.tellg())) {
            return std::unexpected("Negativer Gruppen-Count in .idm - Datei korrupt oder Format falsch verstanden: " + file.string());
        }
        SpatialIndexGroup group;
        group.indices.resize(static_cast<std::size_t>(count));
        in.read(reinterpret_cast<char*>(group.indices.data()), static_cast<std::streamsize>(group.indices.size() * 4));
        if (!in) return std::unexpected("Abgeschnittene IDM-Gruppe");
        index.groups.push_back(std::move(group));
    }

    return index;
}

std::expected<void, std::string> SerializeLegacyIdm(const ObjectSpatialIndex& index, const std::filesystem::path& file) {
    if (index.hash.size() != 32) {
        return std::unexpected("SerializeLegacyIdm: hash muss exakt 32 Zeichen lang sein");
    }

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte .idm nicht zum Schreiben \u00f6ffnen: " + file.string());
    }

    out.write(index.hash.data(), 32);
    out.put('\n');
    WriteRaw(out, index.headerValue);

    for (const auto& group : index.groups) {
        WriteRaw(out, static_cast<std::int32_t>(group.indices.size()));
        for (const auto idx : group.indices) {
            WriteRaw(out, idx);
        }
    }

    if (!out) {
        return std::unexpected("Fehler beim Schreiben der .idm: " + file.string());
    }
    return {};
}

// ---------------------------------------------------------------------------------------------
// .aid
// ---------------------------------------------------------------------------------------------

std::expected<ZoneMetadata, std::string> ParseLegacyAid(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) return std::unexpected("AID konnte nicht geoeffnet werden: " + file.string());
    const auto bytes = in.tellg();
    if (bytes < 4 || bytes > 64 * 1024 * 1024) return std::unexpected("Ungueltige AID-Dateigroesse");
    in.seekg(0);
    ZoneMetadata zones;
    std::uint32_t count = 0;
    if (!ReadRaw(in, count) || count > 100000 || std::uint64_t(count) * 48 > static_cast<std::uint64_t>(bytes) - 4)
        return std::unexpected("AID-Zonenanzahl passt nicht zur Dateigroesse");
    zones.recordType = static_cast<std::int32_t>(count);
    if (count) zones.additionalAreas.resize(count - 1);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto& area = zones.Area(i);
        in.read(reinterpret_cast<char*>(area.rawNameBuffer), sizeof(area.rawNameBuffer));
        if (!in || !ReadRaw(in, area.flag)) return std::unexpected("Abgeschnittener AID-Zonenkopf");
        area.hasRawNameBuffer = true;
        area.name.assign(reinterpret_cast<const char*>(area.rawNameBuffer),
            PortableStrnlen(reinterpret_cast<const char*>(area.rawNameBuffer), sizeof(area.rawNameBuffer)));
        if (area.flag != 0 && area.flag != 1) return std::unexpected("Unbekannter AID-Zonentyp: " + std::to_string(area.flag));
        const int values = area.flag == 0 ? 3 : 5;
        for (int v = 0; v < values; ++v)
            if (!ReadRaw(in, area.bounds[v])) return std::unexpected("Abgeschnittene AID-Zonendaten");
    }
    if (in.tellg() != bytes) return std::unexpected("AID enthaelt Daten hinter den deklarierten Zonen");
    return zones;
}

std::expected<void, std::string> SerializeLegacyAid(const ZoneMetadata& zones, const std::filesystem::path& file) {
    const auto count = zones.AreaCount();
    if (count > 100000) return std::unexpected("Zu viele AID-Zonen");
    // Validate all areas before opening/truncating an existing output file.
    for (std::size_t i = 0; i < count; ++i) {
        const auto& area = zones.Area(i);
        if (area.name.size() > 32 || (area.flag != 0 && area.flag != 1))
            return std::unexpected("Ungueltiger AID-Zonenname oder Zonentyp");
    }
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return std::unexpected("AID konnte nicht geschrieben werden: " + file.string());
    WriteRaw(out, static_cast<std::uint32_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const auto& area = zones.Area(i);
        char name[32]{};
        const auto* raw = reinterpret_cast<const char*>(area.rawNameBuffer);
        const auto rawLength = PortableStrnlen(raw, sizeof(area.rawNameBuffer));
        if (area.hasRawNameBuffer && area.name.size() == rawLength && std::memcmp(area.name.data(), raw, rawLength) == 0)
            std::memcpy(name, raw, sizeof(name));
        else std::memcpy(name, area.name.data(), area.name.size());
        out.write(name, sizeof(name));
        WriteRaw(out, area.flag);
        for (int v = 0; v < (area.flag == 0 ? 3 : 5); ++v) WriteRaw(out, area.bounds[v]);
    }
    if (!out) return std::unexpected("AID-Schreibfehler");
    return {};
}

ObjectSpatialIndex RemapSpatialIndex(const ObjectSpatialIndex& original,
                                     const std::vector<std::int32_t>& writtenSourceIndex,
                                     SpatialIndexRemapReport* report,
                                     std::int32_t sourceObjectCount) {
    SpatialIndexRemapReport r;
    // Altindex -> neue Schreibposition (nur für erhaltene Original-Objekte). Indizes ab
    // sourceObjectCount gehören zu keinem Objekt der geladenen SHMD (Uruga) -> unverändert.
    std::int32_t maxOld = -1;
    std::set<std::int32_t> unattributed;
    for (const auto& g : original.groups)
        for (const std::int32_t i : g.indices) {
            if (sourceObjectCount >= 0 && i >= sourceObjectCount) { unattributed.insert(i); continue; }
            maxOld = std::max(maxOld, i);
        }
    r.unattributedIndices = unattributed.size();
    std::vector<std::int32_t> newPos(static_cast<std::size_t>(std::max<std::int32_t>(maxOld + 1, 0)), -1);
    for (std::size_t k = 0; k < writtenSourceIndex.size(); ++k) {
        const std::int32_t src = writtenSourceIndex[k];
        if (src < 0) { ++r.uncoveredObjects; continue; }
        if (static_cast<std::size_t>(src) >= newPos.size()) { ++r.uncoveredObjects; continue; } // nie im IDM
        if (newPos[static_cast<std::size_t>(src)] >= 0) { ++r.uncoveredObjects; continue; }     // doppelte Herkunft
        newPos[static_cast<std::size_t>(src)] = static_cast<std::int32_t>(k);
        if (static_cast<std::int32_t>(k) != src) ++r.movedObjects;
    }
    for (const std::int32_t p : newPos) if (p < 0) ++r.removedObjects;

    ObjectSpatialIndex out;
    out.hash = original.hash; // Bedeutung ungeklärt -> unverändert übernehmen
    std::int32_t maxNew = -1;
    out.groups.reserve(original.groups.size());
    for (const auto& g : original.groups) {
        SpatialIndexGroup ng;
        ng.indices.reserve(g.indices.size());
        for (const std::int32_t i : g.indices) {
            if (sourceObjectCount >= 0 && i >= sourceObjectCount) {
                ng.indices.push_back(i);
                maxNew = std::max(maxNew, i);
                continue;
            }
            const std::int32_t p = (i >= 0 && static_cast<std::size_t>(i) < newPos.size()) ? newPos[static_cast<std::size_t>(i)] : -1;
            if (p < 0) { ++r.droppedReferences; continue; }
            ng.indices.push_back(p);
            maxNew = std::max(maxNew, p);
        }
        // Im Original sind alle Gruppen aufsteigend sortiert - nach dem Abbilden wieder herstellen.
        std::sort(ng.indices.begin(), ng.indices.end());
        out.groups.push_back(std::move(ng));
    }
    r.identity = r.removedObjects == 0 && r.movedObjects == 0 && r.droppedReferences == 0;
    // Invariante des Originals: alle Indizes < headerValue (Rou: max 1077, header 1078).
    out.headerValue = r.identity ? original.headerValue : std::max(maxNew + 1, 0);
    if (report) *report = r;
    return out;
}

} // namespace theseed::mapeditor::core::legacy
