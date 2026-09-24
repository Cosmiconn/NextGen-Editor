#pragma once
// WalkGridIO.hpp
// Persistenz für das Block&Walk-Modul.

#include "mapeditor/core/WalkGrid.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core {

// -----------------------------------------------------------------------------------------
// Legacy-Import/-Export ("Rou.shbd"): 8 rohe Header-Bytes (in der Referenzdatei
// 00 01 00 00 00 08 00 00 - Bedeutung nicht gesichert) + Width*Height int16, row-major.
// Dimensionen stehen NICHT in der Datei selbst (in der Referenzdatei 512x512, siehe
// docs/MAP_FORMAT.md für die Herleitung aus QuadsWide/QuadsHigh * 8).
// -----------------------------------------------------------------------------------------

struct LegacyShbdHeader {
    std::uint8_t raw[8]{};
};

// Liest NUR die 8 Header-Bytes (ohne width/height zu benötigen) - das zweite Feld enthält bei
// allen bisher geprüften echten Referenzkarten exakt die tatsächliche Gitterhöhe (verifiziert
// an 4 Karten: Rou/Bera 2048, Adl 7600, RouVal01/Eld 4096 - jeweils exakt Quads*8). Deutlich
// robuster als eine reine Formel-Herleitung aus der Heightmap, da direkt aus der Datei selbst
// gelesen. Erstes Feld enthält die ursprüngliche Quad-Anzahl (X-Achse).
struct LegacyShbdHeaderInfo {
    std::uint32_t quadCount = 0; // erstes Header-Feld
    std::uint32_t height = 0;    // zweites Header-Feld - entspricht der tatsächlichen Gitterhöhe
};
std::expected<LegacyShbdHeaderInfo, std::string> PeekLegacyShbdHeader(const std::filesystem::path& file);

std::expected<WalkGrid, std::string> ImportLegacyShbd(
    const std::filesystem::path& file,
    std::uint32_t width,
    std::uint32_t height,
    LegacyShbdHeader* outHeader = nullptr);

std::expected<void, std::string> ExportLegacyShbd(
    const WalkGrid& grid,
    const std::filesystem::path& file,
    LegacyShbdHeader header = LegacyShbdHeader{{0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00}});

} // namespace theseed::mapeditor::core
