#pragma once
// LegacyMapProject.hpp
// Vereinheitlichtes Öffnen/Speichern EINER kompletten Legacy-Karte über alle vier Module hinweg
// (Heightmap, Texturing, Block&Walk, Objekt-Placement) plus Zusatzdaten (räumlicher Index,
// Zonen-Metadaten). Zweck: "Karte öffnen, bearbeiten, wieder speichern" als EINE Aktion statt
// sieben einzelner manueller Imports mit Pfad-/Maßangaben.
//
// Datei-Auffindung: alle Begleitdateien liegen in den echten Referenzkarten im selben
// Verzeichnis wie die .ini und teilen sich (mit EINER beobachteten Ausnahme: RouVal01s .HTD
// heißt "darkVally.HTD" statt "RouVal01.HTD") denselben Dateinamen-Stamm. Die Heightmap-Datei
// wird daher primär über das #HeightFileName-Feld der .ini aufgelöst (das genau diesen Fall
// abdeckt), alle anderen Begleitdateien über den Stamm der .ini selbst - case-insensitiv, siehe
// LegacyPathResolve.hpp.

#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/HeightmapIO.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"
#include "mapeditor/core/ObjectSpatialIndex.hpp"
#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/WalkGrid.hpp"
#include "mapeditor/core/WalkGridIO.hpp"
#include "mapeditor/core/legacy/LegacyIdmAid.hpp"
#include "mapeditor/core/legacy/LegacyMapIni.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct LegacyMapProject {
    LegacyMapIni ini;

    core::Heightmap heightmap;
    bool hasHeightmap = false;
    core::LegacyHtdHeader htdHeader{};
    std::vector<std::uint8_t> htdTrailingBytes; // siehe HeightmapIO.hpp - manche echten Dateien haben Daten nach dem Höhenraster

    core::TextureLayerStack textureStack;

    core::WalkGrid walkGrid;
    bool hasWalkGrid = false;
    core::LegacyShbdHeader shbdHeader{};

    core::ObjectPlacementSet objects;
    bool hasObjects = false;

    ObjectSpatialIndex spatialIndex;
    bool hasSpatialIndex = false;

    ZoneMetadata zone;
    bool hasZone = false;
};

struct LegacyMapOpenReport {
    std::vector<std::string> issues; // nicht-fatale Probleme (fehlende/nicht ladbare Teile)
};

// Öffnet ALLE Teile einer Legacy-Karte anhand ihrer .ini. Nur die .ini selbst ist zwingend
// erforderlich - alle anderen Teile sind optional und werden übersprungen (mit Eintrag in
// report), wenn ihre Datei fehlt oder nicht lesbar ist, statt den gesamten Open-Vorgang
// abzubrechen (z.B. "Eld" hat keine .HTD).
std::expected<LegacyMapProject, std::string> OpenLegacyMap(
    const std::filesystem::path& iniPath, LegacyMapOpenReport* report = nullptr);

// Speichert ALLE vorhandenen Teile (hasXxx-Flags) zurück nach outDir, unter dem Namen mapStem
// (z.B. "Bera" -> Bera.ini, Bera.HTD, Bera.shbd, Bera.shmd, Bera.idm, Bera.aid + Blend-BMPs).
// Gleicht project.ini.layers automatisch an project.textureStack an (Layer können in der GUI
// hinzugefügt/entfernt worden sein, seit die ini zuletzt geparst wurde).
std::expected<void, std::string> SaveLegacyMap(
    LegacyMapProject& project, const std::filesystem::path& outDir, const std::string& mapStem);

} // namespace theseed::mapeditor::core::legacy
