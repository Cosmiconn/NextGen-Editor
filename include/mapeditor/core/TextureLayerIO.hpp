#pragma once
// TextureLayerIO.hpp
// Persistenz für das Texturing-Modul: natives ".tstex"-Format sowie ein Konverter, der aus den
// per LegacyMapIni geparsten Layer-Metadaten einen TextureLayerStack aufbaut (Gewichte leer,
// da die referenzierten .BMP-Blend-Dateien nicht Teil der bereitgestellten Referenzdaten waren -
// echter Bild-Import ist ein separater, noch offener Schritt, siehe docs/MAP_FORMAT.md).

#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/legacy/LegacyMapIni.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core {

// -----------------------------------------------------------------------------------------
// Natives Format ".tstex" — Little-Endian.
//   Offset 0   char[4]   Magic = "TSTX"
//          4   uint32    Version (aktuell 1)
//          8   uint32    Width
//          12  uint32    Height
//          16  uint32    LayerCount
//   Danach pro Layer:
//          uint32        NameLength,      char[NameLength]       Name (UTF-8)
//          uint32        DiffuseLength,   char[DiffuseLength]    DiffuseFileName (UTF-8)
//          float32       UVScaleDiffuse
//          uint8[Width*Height]            Blend-Gewichte (0..255, entspricht 0.0..1.0)
// -----------------------------------------------------------------------------------------

std::expected<TextureLayerStack, std::string> LoadTsTex(const std::filesystem::path& file);
std::expected<void, std::string> SaveTsTex(const TextureLayerStack& stack, const std::filesystem::path& file);

// Baut einen TextureLayerStack aus den geparsten Legacy-Layer-Metadaten auf. OHNE echte BMP-Daten
// ist die tatsächliche Blend-Auflösung unbekannt (sie ist NICHT an die Heightmap-Auflösung
// gekoppelt, siehe docs/MAP_FORMAT.md) - Platzhalter-Auflösung 512x512. Gewichte starten bei 0
// (Basis-Layer bei 1.0). Für eine aus echten Dateien hergeleitete, belastbare Auflösung siehe
// legacy::ImportLegacyTextureSet.
TextureLayerStack BuildTextureLayerStackFromLegacyIni(const legacy::LegacyMapIni& ini);

} // namespace theseed::mapeditor::core
