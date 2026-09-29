#pragma once
// ObjectSpatialIndex.hpp
// Kern-Datencontainer für den räumlichen Index aus "Rou.idm". Struktur vollständig aus der
// echten Datei hergeleitet und verifiziert (Parser konsumiert die komplette Payload ohne Rest,
// siehe docs/MAP_FORMAT.md): 32-Zeichen-ASCII-Hex-Hash + Zeilenumbruch, dann ein führender
// int32-Wert, dann eine Folge variabler-Länge-Gruppen [count, idx_1..idx_count] bis Dateiende.
//
// BEWUSST kein Zellen-Grid-Objekt mit x/y-Koordinaten: eine 2D-Zuordnung der 1178 Gruppen auf
// Rasterzellen ist NICHT verifiziert (kein sauberer Bezug zu Heightmap/shbd-Auflösung
// gefunden). Die Gruppen werden daher als flache, geordnete Liste behandelt - Bedeutung des
// führenden Werts und der genauen Zellzuordnung bleibt offene Hypothese.

#include <cstdint>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct SpatialIndexGroup {
    std::vector<std::int32_t> indices; // vermutlich Indizes in die flache Objekt-Placement-Liste
};

struct ObjectSpatialIndex {
    std::string hash;              // 32-Zeichen-ASCII-Hex (vermutlich Sync-Check gegen .shmd)
    std::int32_t headerValue = 0;  // Bedeutung nicht gesichert (in der Referenzdatei: 1078)
    std::vector<SpatialIndexGroup> groups;
};

} // namespace theseed::mapeditor::core
