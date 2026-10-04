#pragma once
// LegacyIdmAid.hpp
// Import/Export für "Rou.idm" (räumlicher Objekt-Index) und "Rou.aid" (Zonen-Metadaten).

#include "mapeditor/core/ObjectSpatialIndex.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

// .idm - verifiziert byte-für-byte-roundtrip-fähig (tests/test_object_placement.cpp).
std::expected<ObjectSpatialIndex, std::string> ParseLegacyIdm(const std::filesystem::path& file);
std::expected<void, std::string> SerializeLegacyIdm(const ObjectSpatialIndex& index, const std::filesystem::path& file);

// IDM-Pflege beim Export. Belegt an Rou (NA2016): die IDM-Indizes 0..1077 sind exakt die ersten
// 1078 Objekte der SHMD in Dateireihenfolge (die Grenze liegt 221 Objekte tief im letzten alten
// Block jun_grass01; alles danach - 420 weitere Gräser, Lightbugs, Steinhaufen, AuctionHouse01 -
// wurde später angehängt und ist im ausgelieferten Client NICHT im IDM enthalten). Die Bedeutung
// der Gruppen selbst (vermutlich Sichtbarkeitsmengen) ist nicht belegt; deshalb wird NICHT neu
// berechnet, sondern nur jede erhaltene Objekt-Zuordnung auf die neue Schreibposition abgebildet.
//
// Uruga (NA2016) widerlegt eine allgemeine Gültigkeit: Urg.idm verweist auf 4828 Objekte
// (0..4827), Urg.shmd enthält nur 2243 - der IDM wurde nach Änderungen an der SHMD offenbar nicht
// neu erzeugt. Indizes >= sourceObjectCount (Objektanzahl der geladenen SHMD) lassen sich keinem
// Objekt zuordnen und werden deshalb unverändert durchgereicht statt verworfen.
//
// writtenSourceIndex[k] = sourceIndex des Objekts an Schreibposition k (-1 = neu).
struct SpatialIndexRemapReport {
    std::size_t removedObjects = 0;    // Objekte des Originals, die nicht mehr existieren
    std::size_t droppedReferences = 0; // dadurch entfernte Gruppeneinträge
    std::size_t movedObjects = 0;      // erhaltene Objekte mit neuer Schreibposition
    std::size_t uncoveredObjects = 0;  // neue Objekte ohne IDM-Zuordnung (wie im Original-Client)
    std::size_t unattributedIndices = 0; // Indizes ohne SHMD-Objekt (>= sourceObjectCount), unverändert
    bool identity = true;              // unverändert -> Ergebnis byte-gleich zum Original
};
[[nodiscard]] ObjectSpatialIndex RemapSpatialIndex(const ObjectSpatialIndex& original,
                                                   const std::vector<std::int32_t>& writtenSourceIndex,
                                                   SpatialIndexRemapReport* report = nullptr,
                                                   std::int32_t sourceObjectCount = -1);

// -----------------------------------------------------------------------------------------
// .aid: uint32 areaCount, followed by all area records.
// Each record: char name[32], uint32 shape, float values[shape == 0 ? 3 : 5].
// Shape 0/1 is established across the supplied corpus; geometric interpretation
// beyond the record layout must not be inferred from roundtrip success alone.
// -----------------------------------------------------------------------------------------

struct ZoneArea {
    std::string name;
    std::int32_t flag = 1;
    float bounds[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Rohe 32 Byte des Namensfelds. WICHTIG: in der Referenzdatei ist dieses Feld NUR
    // nullterminiert, NICHT vollständig genullt - nach dem Namen folgen Speicherreste aus dem
    // Original-Tool (kein Zero-Padding). Für einen byte-exakten Export wird dieser Rohpuffer
    // unverändert durchgereicht statt aus `name` neu (mit Nullen) aufgebaut. Beim Neuanlegen
    // einer Zone (kein Rohpuffer vorhanden) wird stattdessen sauber mit Nullen aufgefüllt.
    unsigned char rawNameBuffer[32] = {};
    bool hasRawNameBuffer = false;
};

// Inherit the first area to retain the editor's existing single-area accessors.
// recordType was historically misnamed: it is the input count, not a record type.
struct ZoneMetadata : ZoneArea {
    std::int32_t recordType = 1;
    std::vector<ZoneArea> additionalAreas;
    [[nodiscard]] std::size_t AreaCount() const { return recordType == 0 ? 0 : 1 + additionalAreas.size(); }
    ZoneArea& Area(std::size_t index) { return index == 0 ? static_cast<ZoneArea&>(*this) : additionalAreas.at(index - 1); }
    const ZoneArea& Area(std::size_t index) const { return index == 0 ? static_cast<const ZoneArea&>(*this) : additionalAreas.at(index - 1); }
};

std::expected<ZoneMetadata, std::string> ParseLegacyAid(const std::filesystem::path& file);
std::expected<void, std::string> SerializeLegacyAid(const ZoneMetadata& zone, const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
