#include "mapeditor/core/TextureLayerIO.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace theseed::mapeditor::core {

TextureLayerStack BuildTextureLayerStackFromLegacyIni(const legacy::LegacyMapIni& ini) {
    // WICHTIG: Auflösung NICHT von der Heightmap übernehmen - echte Referenzkarten zeigen
    // Blend-Bitmaps in fester, von der Heightmap-Auflösung unabhängiger Größe (z.B. Bera:
    // 257x257-Heightmap, aber 512x512-Blend-BMPs), siehe docs/MAP_FORMAT.md. Ohne echte BMP-Daten
    // (reiner ini-Metadaten-Import) ist die tatsächliche Auflösung unbekannt - 512x512 ist der in
    // allen bisher gesichteten Kartensets beobachtete Wert, aber eine reine Verlegenheitslösung,
    // kein aus der ini hergeleiteter Wert. Für eine belastbare Auflösung siehe
    // legacy::ImportLegacyTextureSet, das die echten BMP-Dateien liest.
    constexpr std::uint32_t kPlaceholderResolution = 512;
    TextureLayerStack stack(kPlaceholderResolution, kPlaceholderResolution);
    for (const auto& layerDef : ini.layers) {
        stack.AddLayer(layerDef.name, layerDef.diffuseFileName, layerDef.uvScaleDiffuse);
    }
    return stack;
}

} // namespace theseed::mapeditor::core
