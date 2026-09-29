#pragma once
// BmpBlendMap.hpp
// Liest/schreibt ein BlendMap-Gewichtsgitter als Windows-BMP.
//
// FORMAT KORRIGIERT (v0.4.0) anhand echter Referenz-Blend-Bitmaps (Adl/Bera/RouVal01-Kartensets):
// die realen Dateien sind durchgehend unkomprimiertes 24-bit RGB (R=G=B, also grau, aber OHNE
// Palette - dataOffset=54, kein 8-bit-indiziertes Format wie ursprünglich angenommen). Reader
// unterstützt daher sowohl 8-bit-indiziert (für Rückwärtskompatibilität mit v0.3.0-Dateien) als
// auch 24-bit RGB; der Writer erzeugt jetzt 24-bit RGB, um dem realen Format zu entsprechen.
//
// WICHTIGER BEFUND: Die Blend-Bitmap-Auflösung ist in den echten Referenzkarten NICHT an die
// Heightmap-Auflösung gekoppelt (z.B. Bera: Heightmap 257x257, Blend-BMPs 512x512 - vermutlich
// eine feste, GPU-freundliche Textur-Auflösung unabhängig von der Vertex-Dichte). Das aktuelle
// TextureLayerStack-Modul geht von gemeinsamer Auflösung mit der Heightmap aus - das ist ein
// bekannter Diskrepanzpunkt zur Realität, siehe docs/MAP_FORMAT.md, noch nicht behoben.

#include "mapeditor/core/BlendMap.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core::legacy {

std::expected<BlendMap, std::string> ReadBlendMapBmp(const std::filesystem::path& file);
std::expected<void, std::string> WriteBlendMapBmp(const BlendMap& blend, const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
