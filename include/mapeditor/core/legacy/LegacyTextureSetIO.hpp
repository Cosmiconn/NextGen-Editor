#pragma once
// LegacyTextureSetIO.hpp
// Bündelt LegacyMapIni + BmpBlendMap zu einem vollständigen Texturing-Roundtrip: ini (Layer-
// Metadaten) UND die referenzierten Blend-Bitmaps in einem Aufwasch importieren/exportieren.

#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/legacy/LegacyMapIni.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct TextureSetImportReport {
    std::vector<std::string> missingBlendFiles;
};

std::expected<TextureLayerStack, std::string> ImportLegacyTextureSet(
    const std::filesystem::path& iniFile,
    TextureSetImportReport* report = nullptr);

std::expected<void, std::string> ExportLegacyTextureSet(
    const TextureLayerStack& stack,
    const LegacyMapIni& iniMeta,
    const std::filesystem::path& outDir,
    const std::string& iniFileName = "Rou.ini");

} // namespace theseed::mapeditor::core::legacy
