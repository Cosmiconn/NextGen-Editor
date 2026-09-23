#pragma once
// LegacyMapIni.hpp
// Parser für das Legacy-Karten-.ini-Format (siehe "Rou.ini"): Key:Value-Zeilen mit "#"-Präfix
// plus "#Layer { ... }"-Blöcke. Liefert sowohl die Heightmap-Metadaten (Dimensionen, Blockgröße)
// als auch die Textur-Layer-Definitionen - beide Module (Heightmap-Import, Texturing-Import)
// nutzen denselben Parser, statt das Format zweimal zu interpretieren.
//
// Bewusst NICHT Teil des nativen TheSeed-Formats - reiner Import-/Migrationspfad, siehe
// docs/MAP_FORMAT.md.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct LegacyLayerDef {
    std::string name;
    std::string diffuseFileName;
    std::string blendFileName;
    float startX = 0.0f;
    float startY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float uvScaleDiffuse = 1.0f;
    float uvScaleBlend = 1.0f;
};

struct LegacyMapIni {
    std::string heightFileName;
    std::string vertexColorTexture;
    std::uint32_t heightmapWidth = 0;
    std::uint32_t heightmapHeight = 0;
    float oneBlockWidth = 50.0f;
    float oneBlockHeight = 50.0f;
    std::uint32_t quadsWide = 0;
    std::uint32_t quadsHigh = 0;
    std::vector<LegacyLayerDef> layers;
};

std::expected<LegacyMapIni, std::string> ParseLegacyMapIni(const std::filesystem::path& file);

// Schreibt die Struktur wieder im Legacy-Text-Layout (Key:Value + #Layer{}-Blöcke). Inhaltlich
// vollständig (parse(serialize(x)) == x), aber NICHT byte-identisch zum Original: Kommentare
// und exaktes Whitespace/Tab-Layout des Originals werden nicht reproduziert, da sie beim Parsen
// bewusst verworfen werden (siehe ParseLegacyMapIni).
std::expected<void, std::string> SerializeLegacyMapIni(const LegacyMapIni& ini, const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
