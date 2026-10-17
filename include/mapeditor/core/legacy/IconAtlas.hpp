#pragma once
// IconAtlas.hpp
// Item-/Skill-Icons des Clients: resmenu\Icon\<IconFile>.dds (Fiesta.bin "%sIcon\%s.dds"). Ein Atlas
// ist ein Raster aus 8x8 Icons, zeilenweise nummeriert (IconIndex 0 = oben links). Belegt an
// ItemEqu000.dds (256x256, 32-px-Zellen): 0 LeatherHelmet, 1 LeatherBoots, 2 LeatherPants, 3 LeatherShirt,
// 4 ChainHelmet ... 8 LitanyHelmet in der zweiten Zeile. Die Zustands-Atlanten AbState00-04 sind
// 128x128 und nutzen Index bis 63 - also ebenfalls 8x8, mit 16-px-Zellen. Alle 133 NA2016-Atlanten
// (120x 256, 12x 128, 1x 64) passen zu 8 Spalten; cellSize 0 = Atlasbreite / 8.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace theseed::mapeditor::core::legacy {

struct IconCell {
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f; // Bildanteile, v = 0 oben
    bool valid = false;
};

// Zelle eines Icons im Atlas (Größe in Pixeln); ungültig, wenn der Index außerhalb liegt.
[[nodiscard]] IconCell IconAtlasCell(std::uint32_t atlasWidth, std::uint32_t atlasHeight, int index,
                                     std::uint32_t cellSize = 0);
[[nodiscard]] int IconAtlasCapacity(std::uint32_t atlasWidth, std::uint32_t atlasHeight, std::uint32_t cellSize = 0);

// Relativ zu resmenu: "Icon/<datei>.dds". Leer für leere Namen und den Platzhalter "-".
[[nodiscard]] std::string IconAtlasRelativePath(const std::string& iconFile);

// Breite/Höhe aus dem DDS-Kopf, ohne das Bild zu dekodieren.
[[nodiscard]] std::optional<std::pair<std::uint32_t, std::uint32_t>> ReadDdsSize(const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
