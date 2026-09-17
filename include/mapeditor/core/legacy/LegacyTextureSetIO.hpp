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
    std::vector<std::string> missingBlendFiles; // Layer, deren .BMP nicht gefunden wurde (Gewicht bleibt 0)
};

// Liest die .ini und lädt für jeden Layer die zugehörige Blend-BMP (relativ zum Verzeichnis der
// .ini, Legacy-Pfade mit "\" werden dabei in native Pfade übersetzt). Fehlende Blend-Dateien
// sind kein Abbruchkriterium - der Layer existiert dann mit Gewicht 0 (bzw. 1.0 falls Layer 0),
// siehe report.missingBlendFiles.
std::expected<TextureLayerStack, std::string> ImportLegacyTextureSet(
    const std::filesystem::path& iniFile,
    TextureSetImportReport* report = nullptr);

// Schreibt .ini + alle Blend-BMPs nach outDir (Verzeichnisstruktur der Legacy-Pfade wird unter
// outDir nachgebildet, z.B. ".\resmap\field\Rou\block.Bmp" -> outDir/resmap/field/Rou/block.Bmp).
std::expected<void, std::string> ExportLegacyTextureSet(
    const TextureLayerStack& stack,
    const LegacyMapIni& iniMeta, // liefert die Nicht-Layer-Felder (HeightFileName etc.) + BlendFileName je Layer
    const std::filesystem::path& outDir,
    const std::string& iniFileName = "Rou.ini");

} // namespace theseed::mapeditor::core::legacy
