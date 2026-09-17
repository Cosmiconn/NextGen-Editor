#pragma once
// TextureLayerIO.hpp
// Persistenz für das Texturing-Modul: natives ".tstex"-Format sowie ein Konverter, der aus den
// per LegacyMapIni geparsten Layer-Metadaten einen TextureLayerStack aufbaut.

#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/legacy/LegacyMapIni.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core {

std::expected<TextureLayerStack, std::string> LoadTsTex(const std::filesystem::path& file);
std::expected<void, std::string> SaveTsTex(const TextureLayerStack& stack, const std::filesystem::path& file);
TextureLayerStack BuildTextureLayerStackFromLegacyIni(const legacy::LegacyMapIni& ini);

} // namespace theseed::mapeditor::core
