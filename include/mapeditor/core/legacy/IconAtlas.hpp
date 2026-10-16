#pragma once
// IconAtlas.hpp
// Item-/Skill-Icons des Clients: resmenu\Icon\<IconFile>.dds (Fiesta.bin "%sIcon\%s.dds"). Ein Atlas
// enthält Icons zu 32x32 Pixeln, zeilenweise nummeriert (IconIndex 0 = oben links). Belegt an
// ItemEqu000.dds (256x256 = 8x8): 0 LeatherHelmet, 1 LeatherBoots, 2 LeatherPants, 3 LeatherShirt,
// 4 ChainHelmet ... 8 LitanyHelmet in der zweiten Zeile.

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
                                     std::uint32_t cellSize = 32);
[[nodiscard]] int IconAtlasCapacity(std::uint32_t atlasWidth, std::uint32_t atlasHeight, std::uint32_t cellSize = 32);

// Relativ zu resmenu: "Icon/<datei>.dds". Leer für leere Namen und den Platzhalter "-".
[[nodiscard]] std::string IconAtlasRelativePath(const std::string& iconFile);

// Breite/Höhe aus dem DDS-Kopf, ohne das Bild zu dekodieren.
[[nodiscard]] std::optional<std::pair<std::uint32_t, std::uint32_t>> ReadDdsSize(const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
