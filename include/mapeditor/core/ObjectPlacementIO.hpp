#pragma once

#include "mapeditor/core/ObjectPlacement.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core {

std::expected<ObjectPlacementSet, std::string> LoadTsObj(const std::filesystem::path& file);
std::expected<void, std::string> SaveTsObj(const ObjectPlacementSet& set, const std::filesystem::path& file);

namespace legacy {
std::expected<ObjectPlacementSet, std::string> ParseLegacyShmd(const std::filesystem::path& file);
std::expected<void, std::string> SerializeLegacyShmd(const ObjectPlacementSet& set, const std::filesystem::path& file);
}

} // namespace theseed::mapeditor::core
