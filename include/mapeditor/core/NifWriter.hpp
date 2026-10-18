#pragma once
// NifWriter.hpp
// NIF-Writer v1: setzt eine strikt geparste NIF-Datei Block für Block neu zusammen. Unveränderte Blöcke
// werden roh kopiert, nur der bearbeitete Block wird neu geschrieben. Das geht, weil Gamebryo
// 10.x/20.0.0.4 weder Blockgrößen- noch Offset-Tabellen kennt und alle Verweise Blocknummern sind -
// ein Block darf also seine Länge ändern (z. B. ein längerer Texturname).
//
// Bearbeitbar:
//   - NiSourceTexture: Dateiname (beliebige Länge, nur externe Texturen),
//   - NiAVObject (NiNode, NiTriShape ...): Translation, Rotation, Skalierung,
//   - NiMaterialProperty: Ambient/Diffuse/Specular/Emissive, Glanz, Alpha.
// Jede Ausgabe wird vor der Rückgabe strikt neu geparst und gegengeprüft: gleiche Blockzahl und
// -typen, Kopf, Fuß und alle anderen Blöcke bytegleich, der neue Wert kommt an. Schlägt etwas fehl,
// gibt es einen Fehler statt einer Datei.

#include "mapeditor/core/NifModel.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct NifTextureEntry {
    std::uint32_t block = 0;
    std::string file;
    bool external = true;
};

struct NifMaterialColors {
    std::array<float, 3> ambient{};
    std::array<float, 3> diffuse{};
    std::array<float, 3> specular{};
    std::array<float, 3> emissive{};
    float glossiness = 0.0f;
    float alpha = 1.0f;
};

// Alle NiSourceTexture-Blöcke der Datei (aus LoadNifByteLayout).
[[nodiscard]] std::vector<NifTextureEntry> NifTextureSources(const NifByteLayout& layout);
[[nodiscard]] std::expected<NifMaterialColors, std::string> ReadNifMaterial(const std::vector<std::uint8_t>& bytes,
                                                                           const NifByteLayout& layout, std::uint32_t block);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> RewriteNifTextureFile(const std::vector<std::uint8_t>& bytes,
                                                                                         std::uint32_t block, const std::string& file);
// NifTransform aus NifModel.hpp: Rotation zeilenweise, Translation, Skalierung (Dateirahmen).
[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> RewriteNifTransform(const std::vector<std::uint8_t>& bytes,
                                                                                       std::uint32_t block, const NifTransform& transform);
[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> RewriteNifMaterial(const std::vector<std::uint8_t>& bytes,
                                                                                      std::uint32_t block, const NifMaterialColors& colors);

} // namespace theseed::mapeditor::core
