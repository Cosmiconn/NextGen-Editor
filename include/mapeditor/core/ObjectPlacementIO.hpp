#pragma once
// ObjectPlacementIO.hpp
// Persistenz für das Objekt-Placement-Modul.

#include "mapeditor/core/ObjectPlacement.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace theseed::mapeditor::core {

// Natives Format ".tsobj": einfaches Text-Format (eigene Zeilen, keine Legacy-Kompatibilität
// nötig, da es sich um ein neues, selbstbeschreibendes Format handelt).
std::expected<ObjectPlacementSet, std::string> LoadTsObj(const std::filesystem::path& file);
std::expected<void, std::string> SaveTsObj(const ObjectPlacementSet& set, const std::filesystem::path& file);

} // namespace theseed::mapeditor::core

namespace theseed::mapeditor::core::legacy {

// -----------------------------------------------------------------------------------------
// Legacy-Import/-Export ("Rou.shmd") - Format vollständig aus der echten Referenzdatei
// hergeleitet (Parser konsumiert alle Tokens ohne Rest, siehe docs/MAP_FORMAT.md):
//
//   <FormatVersion>                              z.B. "shmd0_5"
//   (<KategorieName> <Anzahl> <Anzahl x Modellpfad>)*   bis Token "GlobalLight"
//   GlobalLight <r> <g> <b>
//   Fog <r> <g> <b> <a>
//   BackGroundColor <r> <g> <b>
//   Frustum <weite>
//   (<Modellpfad> <InstanzAnzahl> <InstanzAnzahl x (posX posY posZ rotX rotY rotZ rotW scale)>)*
//     bis Token "DataObjectLoadingEnd"
//   DirectionLightAmbient <r> <g> <b>
//   DirectionLightDiffuse <r> <g> <b>
//
// Reines Whitespace-Format (Token-basiert geparst, nicht zeilenbasiert) - robust gegenüber
// CRLF/LF. Export reproduziert Zeilenumbrüche (CRLF) und Fließkomma-Formatierung (6
// Nachkommastellen, exakt wie im Original) - VERIFIZIERT BYTE-FÜR-BYTE IDENTISCH zur echten
// Rou.shmd (tests/test_object_placement.cpp).
//
// ACHSEN-KONVENTION: Legacy speichert Z-up (X/Y = horizontale Ebene, Z = Höhe) - verifiziert
// durch Abgleich realer Objekt-Z-Werte mit Heightmap.SampleWorld(X,Y) an derselben Position
// (Abweichung nahe 0 für bodenstehende Objekte). Intern wird durchgängig Y-up verwendet (wie
// das Heightmap-Modul: X/Z = horizontale Ebene, Y = Höhe) - Parse/Serialize tauschen Y und Z
// (Position UND Rotations-Quaternion) an der Legacy-Grenze. Reines Vertauschen ohne Berechnung,
// daher exakt umkehrbar und byte-exakt roundtrip-fähig. EINSCHRÄNKUNG: korrekt verifiziert nur
// für reine Rotation um die Hochachse (in allen Referenzdaten sind nur die "Yaw"-Komponenten
// der Quaternion belegt) - bei zusammengesetzten Rotationen (Pitch+Roll+Yaw) würde ein reiner
// Komponenten-Swap die Händigkeit des Koordinatensystems umkehren und wäre nicht mehr korrekt.
// -----------------------------------------------------------------------------------------

std::expected<ObjectPlacementSet, std::string> ParseLegacyShmd(const std::filesystem::path& file);
std::expected<void, std::string> SerializeLegacyShmd(const ObjectPlacementSet& set, const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
