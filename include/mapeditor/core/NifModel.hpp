#pragma once
// NifModel.hpp
// GUI-freier Parser für das Gamebryo/NetImmerse-Format (NIF), Version 20.0.0.4 - verwendet von
// allen .nif-Objektdateien in den echten Kartensets (bestätigt per Header-Signatur an
// mehreren hundert echten Dateien).
//
// STATUS (siehe docs/MAP_FORMAT.md für die vollständige Herleitung):
//   - Datei-Header (Block-Typen-Liste, Block-Type-Index): VERIFIZIERT
//   - NiNode-Szenengraph (Name, Transform, Kinder, Properties): VERIFIZIERT
//   - NiMaterialProperty: VERIFIZIERT
//   - NiTriStripsData (Vertices/Normalen/Farben/UVs/Dreiecksstreifen): VERIFIZIERT BYTE-EXAKT
//   - NiTexturingProperty/NiSourceTexture/NiPixelData: VERIFIZIERT; Use External=0 wird
//     direkt über die referenzierten eingebetteten NiPixelData-Texel dekodiert. Use External=1
//     verwendet weiterhin die externe DDS-Auflösung. Mehrfache Textur-Slots (Dark/Detail/Gloss/...)
//     werden erkannt, aber nur die Base-Textur (Slot 0) wird aktuell als Diffuse-Textur übernommen.
//
// Für nicht unterstützte/nicht parsbare Objekte liefert LoadNifMesh einen Fehler zurück - der
// Aufrufer sollte in diesem Fall auf den Platzhalter-Marker zurückfallen (siehe
// ObjectMarkerRenderer), nicht abstürzen.

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct NifVec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct NifVec2 {
    float u = 0.0f, v = 0.0f;
};

struct NifMaterial {
    std::array<float, 3> ambient{1.0f, 1.0f, 1.0f};
    std::array<float, 3> diffuse{1.0f, 1.0f, 1.0f};
    std::array<float, 3> specular{1.0f, 1.0f, 1.0f};
    std::array<float, 3> emissive{0.0f, 0.0f, 0.0f};
    float glossiness = 10.0f;
    float alpha = 1.0f;
};

// Ein einzelnes NiTriStrips/NiTriStripsData-Paar, in Dreiecke aufgelöst (aus den
// Dreiecksstreifen mit Degenerate-Triangle-Entfernung - siehe ExpandTriangleStrip).
struct NifEmbeddedTexture {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Top-Mip als RGBA8; vor dem Upload wird die gleiche V-Ausrichtung wie bei DDS hergestellt.
    std::vector<std::uint8_t> rgba;
};

struct NifMeshPart {
    std::string name;
    std::vector<NifVec3> positions;
    std::vector<NifVec3> normals;      // leer, falls keine Normalen vorhanden
    std::vector<NifVec2> uvs;          // leer, falls kein UV-Set vorhanden (erstes UV-Set)
    std::vector<std::uint32_t> triangleIndices; // 3 Indizes pro Dreieck, in `positions` indiziert
    NifMaterial material;
    std::string diffuseTexture; // Name aus NiSourceTexture (auch bei eingebetteten Texturen vorhanden)
    std::shared_ptr<const NifEmbeddedTexture> embeddedDiffuseTexture; // gesetzt bei Use External=0
};

struct NifModel {
    std::string rootName;
    std::vector<NifMeshPart> parts;
};

std::expected<NifModel, std::string> LoadNifMesh(const std::filesystem::path& file);

} // namespace theseed::mapeditor::core
