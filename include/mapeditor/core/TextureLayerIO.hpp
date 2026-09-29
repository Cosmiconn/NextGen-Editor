#pragma once
// TextureLayerIO.hpp
// Fiesta-Persistenz für das Texturing-Modul: ein Konverter, der aus den
// per LegacyMapIni geparsten Layer-Metadaten einen TextureLayerStack aufbaut (Gewichte leer,
// da die referenzierten .BMP-Blend-Dateien nicht Teil der bereitgestellten Referenzdaten waren -
// echter Bild-Import ist ein separater, noch offener Schritt, siehe docs/MAP_FORMAT.md).

#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/legacy/LegacyMapIni.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core {

// Baut einen TextureLayerStack aus den geparsten Legacy-Layer-Metadaten auf. OHNE echte BMP-Daten
// ist die tatsächliche Blend-Auflösung unbekannt (sie ist NICHT an die Heightmap-Auflösung
// gekoppelt, siehe docs/MAP_FORMAT.md) - Platzhalter-Auflösung 512x512. Gewichte starten bei 0
// (Basis-Layer bei 1.0). Für eine aus echten Dateien hergeleitete, belastbare Auflösung siehe
// legacy::ImportLegacyTextureSet.
TextureLayerStack BuildTextureLayerStackFromLegacyIni(const legacy::LegacyMapIni& ini);

} // namespace theseed::mapeditor::core
