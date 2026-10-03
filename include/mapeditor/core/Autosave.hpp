#pragma once
// Autosave.hpp
// Automatische Sicherung offener Karten. Die Sicherung liegt bewusst NICHT in der Projektausgabe
// (<Projekt>/Client), sondern daneben unter <Projekt>/Autosave/Client/resmap/field/<Karte>/ - so
// wird sie weder als Projektkarte gefunden noch ausgeliefert. Neben den Kartendateien (geschrieben
// mit SaveLegacyMap, daher mit derselben resmap-relativen Ablage der Blend-BMPs) steht
// autosave.txt mit Quell-INI, Kartenname und Zeitpunkt.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace theseed::mapeditor::core {

struct AutosaveMeta {
    std::string sourceIni;        // INI, aus der die Karte geöffnet wurde (Identität der Karte)
    std::string mapStem;          // Kartenname, z.B. "Rou"
    std::int64_t savedAtUnix = 0; // Sekunden seit 1970 (UTC)
};

// <Projekt>/Autosave/Client/resmap/field/<Karte>
[[nodiscard]] std::filesystem::path AutosaveMapDir(const std::filesystem::path& projectFolder, const std::string& mapStem);

std::expected<void, std::string> WriteAutosaveMeta(const std::filesystem::path& mapDir, const AutosaveMeta& meta);
[[nodiscard]] std::optional<AutosaveMeta> ReadAutosaveMeta(const std::filesystem::path& mapDir);

// Entfernt den Sicherungsordner der Karte (nur innerhalb von <Projekt>/Autosave).
void RemoveAutosave(const std::filesystem::path& projectFolder, const std::string& mapStem);

// true, wenn gesichert werden soll: Änderungen vorhanden und seit der letzten Sicherung (bzw. der
// ersten Änderung) mindestens intervalSeconds vergangen.
[[nodiscard]] bool AutosaveDue(bool enabled, bool dirty, double nowSeconds, double lastSaveSeconds, double intervalSeconds);

} // namespace theseed::mapeditor::core
