#pragma once
// HeightmapIO.hpp
// Import und Export der Fiesta-Heightmaps HTD/HTDG mit erhaltenem Header und Trailer.

#include "mapeditor/core/Heightmap.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

// -----------------------------------------------------------------------------------------
// Legacy-Import (z.B. "Rou.HTD" / "Rou.HTDG"):
//   Offset 0..3   4 rohe Header-Bytes (in den Referenzdateien durchgehend 01 02 01 00) —
//                 Bedeutung nicht gesichert (vermutlich Format-/Versionskennung),
//                 werden nur durchgereicht und beim Import verworfen.
//   Offset 4..    Width * Height float32, row-major.
//
// Die Dimensionen stehen bei diesem Format NICHT in der Datei selbst, sondern in der
// begleitenden .ini (HEIGHTMAP_WIDTH / HEIGHTMAP_HEIGHT) — deshalb hier als Parameter.
// -----------------------------------------------------------------------------------------

struct LegacyHtdHeader {
    std::uint8_t raw[4]{};
};

std::expected<Heightmap, std::string> ImportLegacyHtd(
    const std::filesystem::path& file,
    std::uint32_t width,
    std::uint32_t height,
    float blockWidth = 50.0f,
    float blockHeight = 50.0f,
    LegacyHtdHeader* outHeader = nullptr,
    std::vector<std::uint8_t>* outTrailingBytes = nullptr);

// Schreibt eine Heightmap wieder im Legacy-Layout (4 Header-Bytes + width*height float32,
// row-major). header wird unverändert übernommen (z.B. das beim Import gelesene Original),
// per Default die in den Referenzdateien beobachteten Bytes 01 02 01 00. trailingBytes wird
// (falls angegeben) unverändert ans Dateiende angehängt - manche echten Legacy-Dateien haben
// zusätzliche Daten nach dem reinen Höhenraster (beobachtet bei UrgDark01/UrgSwa01, Bedeutung
// nicht gesichert), die für einen byte-exakten Re-Export erhalten bleiben müssen.
std::expected<void, std::string> ExportLegacyHtd(
    const Heightmap& heightmap,
    const std::filesystem::path& file,
    LegacyHtdHeader header = LegacyHtdHeader{{0x01, 0x02, 0x01, 0x00}},
    const std::vector<std::uint8_t>& trailingBytes = {});

} // namespace theseed::mapeditor::core
