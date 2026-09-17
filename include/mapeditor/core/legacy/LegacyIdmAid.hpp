#pragma once
// LegacyIdmAid.hpp
// Import/Export für "Rou.idm" (räumlicher Objekt-Index) und "Rou.aid" (Zonen-Metadaten).

#include "mapeditor/core/ObjectSpatialIndex.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core::legacy {

std::expected<ObjectSpatialIndex, std::string> ParseLegacyIdm(const std::filesystem::path& file);
std::expected<void, std::string> SerializeLegacyIdm(const ObjectSpatialIndex& index, const std::filesystem::path& file);

struct ZoneMetadata {
    std::int32_t recordType = 1;
    std::string name;
    std::int32_t flag = 1;
    float bounds[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    unsigned char rawNameBuffer[32] = {};
    bool hasRawNameBuffer = false;
};

std::expected<ZoneMetadata, std::string> ParseLegacyAid(const std::filesystem::path& file);
std::expected<void, std::string> SerializeLegacyAid(const ZoneMetadata& zone, const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
