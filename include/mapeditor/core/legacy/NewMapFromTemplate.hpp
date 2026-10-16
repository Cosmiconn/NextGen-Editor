#pragma once
// NewMapFromTemplate.hpp
// Neue Fiesta-Karte aus einer vorhandenen Vorlagenkarte. Übernommen werden nur Dinge, die im Client
// existieren und sich nicht auf die Geometrie der Vorlage beziehen: Blockgröße, Textur-Layer
// (Name, Diffuse-Textur, UV-Skalierung), HTD-Kopf, SHBD-Kopf-Konvention, SHMD-Szenenlicht und die
// Himmel-Kategorie, die .conf (Licht/Glow) und die .aid. Neu angelegt werden eine flache Heightmap,
// frische Masken (Layer 1 voll, übrige leer), ein freies Block&Walk-Gitter und eine leere Objektliste.
//
// Belegt an NA2016 (siehe CHANGELOG): Rou/Urg/Bera 257 Punkte, 50 Einheiten Blockgröße; SHBD-Zeilen =
// Blöcke * 8, quadratisch; SHBD-Kopf = (Blöcke, Zeilen); HTD-Kopf überall 01 02 01 00. Teva hat keine
// .idm, Bera keine .HTDG - beide sind also nicht zwingend.

#include "mapeditor/core/legacy/LegacyMapProject.hpp"

#include <expected>
#include <string>

namespace theseed::mapeditor::core::legacy {

struct NewMapParams {
    std::string name;                  // Ordner-, Datei- und Servername (z.B. "MyField")
    std::uint32_t heightmapPoints = 257; // Punkte je Seite (Blöcke + 1)
    float baseHeight = 0.0f;
};

[[nodiscard]] bool IsValidNewMapName(const std::string& name);

[[nodiscard]] std::expected<LegacyMapProject, std::string> CreateMapFromTemplate(const LegacyMapProject& templateMap,
                                                                                const NewMapParams& params);

} // namespace theseed::mapeditor::core::legacy
