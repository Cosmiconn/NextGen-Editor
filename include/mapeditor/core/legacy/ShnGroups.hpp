#pragma once
// ShnGroups.hpp
// Gruppierte SHN-Tabellen: mehrere Zeilen mit demselben Schlüssel bilden eine Gruppe (SetItem nach
// Index, RandomOption nach DropItemIndex, ItemUpgrade nach ID ...). Die Item-System-Editoren
// bearbeiten eine Gruppe als Ganzes; die n-te Zeile einer Gruppe auf der Client-Seite entspricht der
// n-ten Zeile derselben Gruppe auf der Server-Seite.

#include "mapeditor/core/legacy/ShnFile.hpp"

#include <optional>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct ShnGroup {
    std::string key;              // Wert der Schlüsselspalte (ohne führende/folgende Leerzeichen)
    std::vector<std::size_t> rows; // Zeilenindizes in Dateireihenfolge
};

// Gruppen in Reihenfolge des ersten Auftretens. keyColumn < 0 oder ungültig: keine Gruppen.
[[nodiscard]] std::vector<ShnGroup> GroupShnRows(const ShnFile& file, int keyColumn);

// Zeilen einer Gruppe (leer, wenn es sie nicht gibt).
[[nodiscard]] std::vector<std::size_t> ShnGroupRows(const ShnFile& file, int keyColumn, const std::string& key);

// Einfügeposition für eine neue Zeile der Gruppe: direkt hinter ihrer letzten Zeile, bei einer neuen
// Gruppe am Dateiende.
[[nodiscard]] std::size_t ShnGroupInsertIndex(const ShnFile& file, int keyColumn, const std::string& key);

} // namespace theseed::mapeditor::core::legacy
