#pragma once
// LegacyIdmAid.hpp
// Import/Export für "Rou.idm" (räumlicher Objekt-Index) und "Rou.aid" (Zonen-Metadaten).

#include "mapeditor/core/ObjectSpatialIndex.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

// .idm - verifiziert byte-für-byte-roundtrip-fähig (tests/test_object_placement.cpp).
std::expected<ObjectSpatialIndex, std::string> ParseLegacyIdm(const std::filesystem::path& file);
std::expected<void, std::string> SerializeLegacyIdm(const ObjectSpatialIndex& index, const std::filesystem::path& file);

// -----------------------------------------------------------------------------------------
// .aid: uint32 areaCount, followed by all area records.
// Each record: char name[32], uint32 shape, float values[shape == 0 ? 3 : 5].
// Shape 0/1 is established across the supplied corpus; geometric interpretation
// beyond the record layout must not be inferred from roundtrip success alone.
// -----------------------------------------------------------------------------------------

struct ZoneArea {
    std::string name;
    std::int32_t flag = 1;
    float bounds[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Rohe 32 Byte des Namensfelds. WICHTIG: in der Referenzdatei ist dieses Feld NUR
    // nullterminiert, NICHT vollständig genullt - nach dem Namen folgen Speicherreste aus dem
    // Original-Tool (kein Zero-Padding). Für einen byte-exakten Export wird dieser Rohpuffer
    // unverändert durchgereicht statt aus `name` neu (mit Nullen) aufgebaut. Beim Neuanlegen
    // einer Zone (kein Rohpuffer vorhanden) wird stattdessen sauber mit Nullen aufgefüllt.
    unsigned char rawNameBuffer[32] = {};
    bool hasRawNameBuffer = false;
};

// Inherit the first area to retain the editor's existing single-area accessors.
// recordType was historically misnamed: it is the input count, not a record type.
struct ZoneMetadata : ZoneArea {
    std::int32_t recordType = 1;
    std::vector<ZoneArea> additionalAreas;
    [[nodiscard]] std::size_t AreaCount() const { return recordType == 0 ? 0 : 1 + additionalAreas.size(); }
    ZoneArea& Area(std::size_t index) { return index == 0 ? static_cast<ZoneArea&>(*this) : additionalAreas.at(index - 1); }
    const ZoneArea& Area(std::size_t index) const { return index == 0 ? static_cast<const ZoneArea&>(*this) : additionalAreas.at(index - 1); }
};

std::expected<ZoneMetadata, std::string> ParseLegacyAid(const std::filesystem::path& file);
std::expected<void, std::string> SerializeLegacyAid(const ZoneMetadata& zone, const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
