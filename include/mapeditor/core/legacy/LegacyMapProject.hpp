#pragma once
// LegacyMapProject.hpp
// Vereinheitlichtes Öffnen/Speichern einer kompletten Legacy-Karte.

#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/HeightmapIO.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"
#include "mapeditor/core/ObjectSpatialIndex.hpp"
#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/WalkGrid.hpp"
#include "mapeditor/core/WalkGridIO.hpp"
#include "mapeditor/core/legacy/LegacyIdmAid.hpp"
#include "mapeditor/core/legacy/LegacyMapIni.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct LegacyMapProject {
    LegacyMapIni ini;
    core::Heightmap heightmap;
    bool hasHeightmap = false;
    core::LegacyHtdHeader htdHeader{};
    std::vector<std::uint8_t> htdTrailingBytes;
    core::TextureLayerStack textureStack;
    core::WalkGrid walkGrid;
    bool hasWalkGrid = false;
    core::LegacyShbdHeader shbdHeader{};
    core::ObjectPlacementSet objects;
    bool hasObjects = false;
    ObjectSpatialIndex spatialIndex;
    bool hasSpatialIndex = false;
    ZoneMetadata zone;
    bool hasZone = false;
};

struct LegacyMapOpenReport {
    std::vector<std::string> issues;
};

std::expected<LegacyMapProject, std::string> OpenLegacyMap(
    const std::filesystem::path& iniPath, LegacyMapOpenReport* report = nullptr);

std::expected<void, std::string> SaveLegacyMap(
    LegacyMapProject& project, const std::filesystem::path& outDir, const std::string& mapStem);

} // namespace theseed::mapeditor::core::legacy
