#pragma once
// Minimap.hpp
// Fiesta-Minimap (Client: resmenu\minimap\<MapFolderName>.dds, 512x512 DXT1/DXT3, meist ohne Mips).
// MapViewInfo.StartX/StartY/EndX/EndY beschreiben einen quadratischen Ausschnitt der Karte in
// normierten Kartenkoordinaten 0..511: Ursprung oben links (Nordwesten), X nach Osten, Y nach Süden.
// Das Bild zeigt genau diesen Ausschnitt, Norden oben. Belegt an den NA2016-Minimaps von Bera
// (0,0-511,511), Roumen (79,86-474,482) und RouTemDn01 (0,106-405,511) gegen die SHBD-Begehbarkeit
// (siehe docs/MINIMAP_FORMAT.md).
//
// Die Originale sind handgemalt (Pergament-Rahmen, dunkle Konturen). StylizeMinimap erzeugt aus einer
// Draufsicht und der Begehbarkeit ein Bild in diesem Stil.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct MinimapView {
    int startX = 0, startY = 0, endX = 511, endY = 511;
};

// Ausschnitt als Anteile der ganzen Karte (u nach Osten, v nach Süden, 0..1).
struct MinimapCrop {
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
};

[[nodiscard]] MinimapCrop MinimapCropOf(const MinimapView& view);

// Kartenanteil (u, v; v = 0 Norden) -> Bildanteil (0..1, oben links) und zurück.
void MapToMinimapImage(const MinimapCrop& crop, float u, float v, float& outX, float& outY);
void MinimapImageToMap(const MinimapCrop& crop, float x, float y, float& outU, float& outV);

// Schneidet aus einem Ganzkarten-Bild (RGBA, Zeile 0 = Norden) den Ausschnitt aus und skaliert ihn
// bilinear auf outW x outH.
[[nodiscard]] std::vector<std::uint8_t> CropResampleRgba(std::span<const std::uint8_t> rgba, std::uint32_t width,
                                                        std::uint32_t height, const MinimapCrop& crop,
                                                        std::uint32_t outW, std::uint32_t outH);

struct MinimapStyle {
    bool outlines = true;          // dunkle Kontur an der Grenze begehbar/blockiert
    bool parchmentFrame = true;    // ausgefranster dunkler Rand wie bei den Originalen
    float blockedDarken = 0.78f;   // Helligkeit blockierter Flächen
    float saturation = 1.25f;
};

// rgba: size x size, Zeile 0 = Norden. walkable: size x size, 1 = begehbar (leer = alles begehbar).
[[nodiscard]] std::vector<std::uint8_t> StylizeMinimap(std::span<const std::uint8_t> rgba,
                                                      std::span<const std::uint8_t> walkable, std::uint32_t size,
                                                      const MinimapStyle& style = {});

// Relativ zu resmenu: "minimap/<name>.dds".
[[nodiscard]] std::string MinimapRelativePath(const std::string& mapFolderName);

} // namespace theseed::mapeditor::core::legacy
