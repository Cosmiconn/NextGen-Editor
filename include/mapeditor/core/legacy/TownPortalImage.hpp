#pragma once
// TownPortalImage.hpp
// Bilder der Stadtportal-Auswahl (Client: resmenu\TownPortal\<MapName>.DDS und <MapName>_disable.DDS,
// dazu NoneMap.dds). Die NA2016-Originale sind perspektivische Aufnahmen des Ankunftsorts, 256x256
// DXT5 mit Mipmaps (einzelne 200x200 DXT3), mit abgerundeten Ecken im Alpha (Radius ~25 px bei 256: Übergang auf der Diagonalen bei 7.5 px).
// Die _disable-Variante ist dieselbe Aufnahme entsättigt und abgedunkelt; gemessen an sechs
// Originalpaaren (Rou, Bera, Eld, Urg, Teva, Adl): Sättigung im Mittel x0.45, Helligkeit x0.80.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

// Setzt Alpha außerhalb der abgerundeten Ecken auf 0 (mit 1 px weicher Kante). rgba: size x size.
void ApplyRoundedCorners(std::vector<std::uint8_t>& rgba, std::uint32_t size, float radius);

// Graue Variante für nicht freigeschaltete Ziele.
[[nodiscard]] std::vector<std::uint8_t> MakeTownPortalDisabled(std::span<const std::uint8_t> rgba,
                                                              float saturation = 0.45f, float brightness = 0.80f);

// Relativ zu resmenu: "TownPortal/<name>.dds" bzw. "TownPortal/<name>_disable.dds".
[[nodiscard]] std::string TownPortalImageRelativePath(const std::string& mapName, bool disabled);

} // namespace theseed::mapeditor::core::legacy
