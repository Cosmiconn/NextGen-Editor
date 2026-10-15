#pragma once
// HeightmapExchange.hpp
// Höhendaten mit externen Terrain-Werkzeugen (World Machine, Gaea, Photoshop, ...) austauschen:
// 16-Bit-Graustufen-PNG und RAW R16 (little endian, quadratisch). Der Höhenbereich wird beim Export
// auf 0..65535 abgebildet und im PNG als tEXt-Chunk "NextGenHeightRange" ("min max") mitgeschrieben;
// beim Import gibt der Nutzer den Bereich an (Vorgabe: aus dem tEXt-Chunk bzw. der aktuellen Karte).
// Weicht die Bildgröße von der Heightmap ab, wird bilinear auf die Kartengröße umgerechnet.
//
// Ohne externe Bibliothek: PNG wird mit unkomprimierten Deflate-Blöcken geschrieben (gültiges PNG,
// nur größer) und mit einem eigenen Inflate gelesen (gespeicherte, feste und dynamische Huffman-
// Blöcke; 8/16 Bit, Grau/Grau+Alpha/RGB/RGBA - es zählt der erste Kanal; kein Interlacing).

#include "mapeditor/core/EditOps.hpp"
#include "mapeditor/core/Heightmap.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace theseed::mapeditor::core {

struct HeightImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint16_t> values; // Zeile 0 = Norden (größtes Z), wie eine Karte von oben
    std::optional<std::pair<float, float>> range; // aus dem PNG-tEXt-Chunk, falls vorhanden
};

[[nodiscard]] HeightImage HeightmapToImage(const Heightmap& heightmap, float minHeight, float maxHeight);

[[nodiscard]] std::vector<std::uint8_t> EncodePng16(const HeightImage& image);
[[nodiscard]] std::expected<HeightImage, std::string> DecodePng(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::vector<std::uint8_t> EncodeR16(const HeightImage& image);
// R16 hat keinen Kopf: Breite = Höhe = sqrt(Größe/2), muss ganzzahlig sein.
[[nodiscard]] std::expected<HeightImage, std::string> DecodeR16(std::span<const std::uint8_t> bytes);

// Schreibt .png oder .r16/.raw (nach Endung) mit dem Höhenbereich der Karte.
std::expected<void, std::string> ExportHeightmapImage(const Heightmap& heightmap, const std::filesystem::path& file);
[[nodiscard]] std::expected<HeightImage, std::string> ImportHeightImage(const std::filesystem::path& file);

// Überträgt das Bild (bilinear umgerechnet) mit dem Bereich [minHeight, maxHeight] auf die
// Heightmap; liefert den Undo-Patch.
UndoPatch ApplyHeightImage(Heightmap& heightmap, const HeightImage& image, float minHeight, float maxHeight);

} // namespace theseed::mapeditor::core
