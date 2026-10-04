#pragma once
// MapCheck.hpp
// Kartenprüfung wie Unreals "Map Check": sammelt prüfbare Fehler einer geladenen Karte. Der Kern
// kennt nur Positionen, Gitter und Namen; die Oberfläche (main.cpp) liefert die Daten aus NPC.txt,
// MobRegen, LinkTable, SHMD und QuestData und formatiert die Meldungen zweisprachig.
//
// Koordinaten: x = Ost, z = Server-Y; identisch mit World/NPC.txt, MobRegen und SHBD
// (Zelle = WalkGrid::kCellSize = 6.25 Welteinheiten, Bit gesetzt = blockiert).

#include "mapeditor/core/WalkGrid.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

enum class MapCheckSeverity : std::uint8_t { Error, Warning, Info };

enum class MapCheckCode : std::uint8_t {
    NoWalkGrid,              // Karte ohne SHBD: Positionsprüfungen entfallen
    NpcOnBlockedCell,        // NPC steht auf blockierter Zelle
    NpcWithoutPosition,      // NPC mit Koordinate 0/0 (im Original: nicht aufgestellt)
    GateOnBlockedCell,       // Gate-NPC steht auf blockierter Zelle
    GateTargetBlocked,       // Gate-Ziel liegt auf blockierter Zelle der Zielkarte
    GateTargetOutside,       // Gate-Ziel liegt außerhalb des SHBD der Zielkarte
    GateTargetMapUnknown,    // Zielkarte nicht gefunden (kein SHBD lesbar)
    GateLinkMissing,         // Gate-NPC verweist auf einen LinkTable-Eintrag, den es nicht gibt
    GateArrivalBlocked,      // Gate einer anderen Karte kommt hier auf blockierter Zelle an
    TeleportTargetBlocked,   // TownPortal-/Rückruf-Ziel auf blockierter Zelle
    SpawnZoneCenterBlocked,  // Mittelpunkt einer Spawn-Zone blockiert
    SpawnZoneMostlyBlocked,  // Spawn-Zone fast vollständig blockiert
    SpawnZoneOutside,        // Spawn-Zone außerhalb der Karte
    SpawnZoneUnused,         // Spawn-Zone ohne aktiven MobRegen-Eintrag (wird nicht geprüft)
    SpawnMobUnknown,         // MobRegen verweist auf einen Mob, der nicht in MobInfo steht
    RespawnPointBlocked,     // MapInfo RegenX/RegenY auf blockierter Zelle
    ObjectOutsideMap,        // Objekt außerhalb der Kartenfläche
    ModelMissing,            // NIF nicht gefunden
    ModelLoadFailed,         // NIF nicht lesbar
    TextureMissing,          // Textur eines NIF nicht gefunden
    TerrainTextureMissing,   // Terrain-Layer-Textur nicht gefunden
    QuestStartNpcUnknown,    // Start-NPC nicht in MobInfo
    QuestStartNpcNotPlaced,  // Start-NPC steht auf keiner Karte
};

struct MapCheckIssue {
    MapCheckSeverity severity = MapCheckSeverity::Warning;
    MapCheckCode code = MapCheckCode::NoWalkGrid;
    std::string subject;      // z.B. NPC-Name, Modellpfad, Zonen-Index, Quest-Titel
    std::string detail;       // z.B. Texturname, Zielkarte
    bool hasPosition = false; // x/z gültig (Klick springt dorthin)
    float x = 0.0f;
    float z = 0.0f;
    double value = 0.0;       // z.B. blockierter Anteil (0..1) oder Abstand zur nächsten freien Zelle
    int ref = -1;             // Index in der Quelle (NPC-Record, Zone, Objekt, Quest)
};

struct MapCheckPoint {
    std::string label;
    float x = 0.0f;
    float z = 0.0f;
    int ref = -1;
};

struct MapCheckZone {
    std::string label;
    float x = 0.0f;
    float z = 0.0f;
    float radius = 0.0f;
    int ref = -1;
};

struct MapCheckThresholds {
    // Ab diesem blockierten Anteil gilt eine Spawn-Zone als "fast vollständig in Wänden". An den
    // NA2016-Karten kalibriert (siehe CHANGELOG): Originalzonen liegen deutlich darunter.
    double zoneMostlyBlocked = 0.9;
};

// Anteil blockierter Zellen (0..1) in einem Kreis; Zellen außerhalb des Gitters zählen als
// blockiert. Radius < eine Zelle: nur die Zelle unter dem Mittelpunkt.
[[nodiscard]] double BlockedFractionInCircle(const WalkGrid& grid, float x, float z, float radius);

// Schwere nach Abstand zur nächsten begehbaren Zelle: bis infoDistance Hinweis (im Original häufig,
// z.B. Händler hinter einer Theke), bis warnDistance Warnung, darüber (oder keine freie Zelle im
// Suchradius) Fehler.
struct WalkableDistanceSeverity {
    float infoDistance = 4.0f * WalkGrid::kCellSize;
    float warnDistance = 16.0f * WalkGrid::kCellSize;
};

// Punkte (NPCs, Gates, Wiederbelebungspunkt) auf blockierten Zellen. value = Abstand zur nächsten
// begehbaren Zelle in Welteinheiten (-1, wenn im Suchradius keine gefunden wurde).
void CheckPointsWalkable(const WalkGrid& grid, std::span<const MapCheckPoint> points, MapCheckCode code,
                         const WalkableDistanceSeverity& severity, std::vector<MapCheckIssue>& out);

// Spawn-Zonen: Mittelpunkt blockiert (Warnung), fast vollständig blockiert (Fehler), außerhalb.
void CheckSpawnZones(const WalkGrid& grid, std::span<const MapCheckZone> zones, const MapCheckThresholds& thresholds,
                     std::vector<MapCheckIssue>& out);

// Objekte außerhalb der Kartenfläche [0, sizeX] x [0, sizeZ] (mit Toleranz margin).
void CheckPointsInsideMap(std::span<const MapCheckPoint> points, float sizeX, float sizeZ, float margin,
                          MapCheckCode code, MapCheckSeverity severity, std::vector<MapCheckIssue>& out);

// Sortiert: Fehler vor Warnungen vor Hinweisen, innerhalb nach Code und Betreff (stabil).
void SortMapCheckIssues(std::vector<MapCheckIssue>& issues);

// SHBD mit Abmessungen aus dem Dateikopf lesen (zweites Feld = Gitterhöhe, Breite aus der
// Dateigröße), wie beim Kartenladen. Für Gate-Ziele auf anderen Karten.
[[nodiscard]] std::expected<WalkGrid, std::string> ImportLegacyShbdAutoSize(const std::filesystem::path& file);

} // namespace theseed::mapeditor::core
