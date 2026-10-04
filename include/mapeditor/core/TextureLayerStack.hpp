#pragma once
// TextureLayerStack.hpp
// Geordnete Liste von Textur-Layern (Diffuse-Textur-Referenz + Blend-Gewichtsgitter je Layer),
// analog zum #Layer-Konzept aus dem Legacy-.ini-Format. Alle Layer teilen sich eine gemeinsame
// Gitterauflösung (i.d.R. identisch zur Heightmap-Auflösung).

#include "mapeditor/core/BlendMap.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct TextureLayer {
    std::string name;
    std::string diffuseFileName;
    float uvScaleDiffuse = 1.0f;
    // Region, die die Blend-Map dieses Layers auf der Karte abdeckt - in Heightmap-Vertex-Einheiten
    // (Legacy-ini #StartPos_X/#StartPos_Y/#Width/#Height). regionWidth/regionHeight = 0 bedeutet
    // "ganze Karte". Grosse Karten wie Adl (951x476) bestehen aus mehreren Layern, die jeweils nur
    // eine 476x476-Region abdecken - ohne diese Angabe wurden sie ueber die ganze Karte gestreckt.
    float regionStartX = 0.0f;
    float regionStartY = 0.0f;
    float regionWidth = 0.0f;
    float regionHeight = 0.0f;
    BlendMap blend;
    // Originale Blend-Map in ihrer eigenen Auflösung, falls sie beim Import auf die gemeinsame
    // Stack-Auflösung resampelt werden musste (z.B. Adl: 476x476 neben 512x512). Der Export schreibt
    // einen unveränderten Layer dann bytegleich in Originalauflösung bzw. resampelt einen bearbeiteten
    // Layer zurück auf diese Auflösung, statt die Datei stillschweigend umzuskalieren.
    std::optional<BlendMap> sourceBlend;
    // Original-Dateibytes der Blend-BMP (beim Import gelesen). Der Export schreibt einen
    // unveränderten Layer damit bytegleich und patcht einen bearbeiteten nur an geänderten Pixeln.
    std::vector<std::uint8_t> sourceBmpBytes;
    // Die Blend-BMP war beim Import nicht auffindbar/lesbar. sourceBlend hält dann den
    // Anfangszustand; solange der Layer nicht bemalt wird, legt der Export keine Datei an.
    bool blendMissingAtImport = false;
    // Pfad der geladenen Blend-BMP auf der Platte; der Export übernimmt daraus Ordner- und
    // Dateinamen-Schreibweise (".\resmap\field\bera\Moss.BMP" liegt als Bera/Moss.BMP vor).
    std::string sourceBlendPath;
};

class TextureLayerStack {
public:
    TextureLayerStack() = default;
    TextureLayerStack(std::uint32_t width, std::uint32_t height) : width_(width), height_(height) {}

    // Setzt die gemeinsame Gitterauflösung; vorhandene Layer-Gewichte werden zurückgesetzt.
    void Resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }

    // Neuer Layer, Gewicht überall 0 - Ausnahme: der allererste Layer wird mit 1.0 vorbelegt
    // (Basis-Layer, sonst wäre die Normalisierung beim ersten Malen uneindeutig).
    std::size_t AddLayer(std::string name, std::string diffuseFileName, float uvScaleDiffuse = 1.0f);
    void RemoveLayer(std::size_t index);
    // History-friendly structural primitives: move a layer out without copying its BlendMap
    // and reinsert the exact layer at its original index.
    [[nodiscard]] std::optional<TextureLayer> TakeLayer(std::size_t index);
    void InsertLayer(std::size_t index, TextureLayer layer);
    void MoveLayer(std::size_t fromIndex, std::size_t toIndex);

    [[nodiscard]] std::size_t LayerCount() const noexcept { return layers_.size(); }
    [[nodiscard]] const TextureLayer& Layer(std::size_t index) const { return layers_.at(index); }
    [[nodiscard]] TextureLayer& Layer(std::size_t index) { return layers_.at(index); }

    // Summe der Gewichte aller Layer an Zelle (x,z) - sollte nach jeder Malaktion ~1.0 sein
    // (Invariante, siehe TexturePaintOps); nützlich für Tests/Debug-Anzeige.
    [[nodiscard]] float WeightSumAt(std::uint32_t x, std::uint32_t z) const;

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<TextureLayer> layers_;
};

} // namespace theseed::mapeditor::core
