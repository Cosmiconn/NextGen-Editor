#pragma once
// LegacyPathResolve.hpp
// Gemeinsame Hilfsfunktionen zum Auflösen von Legacy-Pfadangaben gegen das echte Dateisystem.
// Notwendig, weil echte Referenzkarten zwei Eigenschaften zeigen, die eine naive
// Pfadkonstruktion scheitern lassen (siehe docs/MAP_FORMAT.md):
//   1) Pfade sind teils relativ zu einer GETEILTEN Asset-Wurzel (z.B. "fieldTexture/" als
//      Geschwister von "field/<Karte>/"), nicht zwingend zum Kartenordner selbst.
//   2) Pfade sind case-sensitiv falsch (Windows-authored, auf case-sensitivem Dateisystem
//      gelesen) - z.B. ".\resmap\field\bera\moss.bmp" vs. echte Datei "field/Bera/Moss.BMP".

#include <cctype>
#include <filesystem>
#include <optional>
#include <string>

namespace theseed::mapeditor::core::legacy {

// ".\resmap\field\Rou\block.Bmp" -> "resmap/field/Rou/block.Bmp" (native Trennzeichen).
std::filesystem::path LegacyPathToNative(const std::string& legacyPath);

bool EqualsCaseInsensitive(const std::string& a, const std::string& b);

// "resmap" ist in den echten Referenzkarten KEIN realer Ordner, sondern ein rein logisches
// Präfix des Original-Tools - wird entfernt, bevor gegen das tatsächliche Dateisystem
// aufgelöst wird.
std::filesystem::path StripResmapPrefix(const std::filesystem::path& p);

// Löst einen relativen Pfad Komponente für Komponente auf, mit case-insensitivem Fallback pro
// Ebene.
std::optional<std::filesystem::path> ResolveCaseInsensitivePath(
    const std::filesystem::path& root, const std::filesystem::path& relative);

// Probiert mehrere plausible Wurzeln der Reihe nach: (1) mapDir selbst, (2) zwei Ebenen über
// mapDir (gemeinsame Asset-Wurzel bei Layout "<AssetRoot>/field/<Karte>/karte.ini").
std::optional<std::filesystem::path> ResolveLegacyAssetPath(
    const std::filesystem::path& mapDir, const std::string& legacyPath);

// Sucht im Verzeichnis `dir` case-insensitiv nach einer Datei mit Namen `stem` + `extension`
// (z.B. stem="Bera", extension=".shbd" findet auch "bera.SHBD"). Liefert nullopt, wenn keine
// oder mehrere mehrdeutige Treffer existieren (kein Raten bei Mehrdeutigkeit).
std::optional<std::filesystem::path> FindSiblingFileByStem(
    const std::filesystem::path& dir, const std::string& stem, const std::string& extension);

} // namespace theseed::mapeditor::core::legacy
