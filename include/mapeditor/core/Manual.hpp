#pragma once
// Handbuch, Tooltips und Spalten-Referenz des Editors (zweisprachig Deutsch/Englisch).
// Die Daten stehen in src/core/ManualData.cpp (generiert, siehe docs/MANUAL_MAINTENANCE.md); dieser Teil
// enthaelt nur die Abfragen (Suche, Tooltip-Suche) und ist frei von UI-Abhaengigkeiten (testbar).
//
// Ehrlichkeit im Inhalt: Beschreibungen, die nur aus Namen abgeleitet sind, tragen "(vermutet)" bzw. sind
// in ColumnDoc als certain=false markiert.

#include <cstddef>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::manual {

struct Chapter { const char* id; const char* titleDe; const char* titleEn; };
struct Section {
    const char* id; const char* chapterId;
    const char* titleDe; const char* titleEn;
    const char* bodyDe; const char* bodyEn;   // "# " = Zwischenueberschrift, "- " = Aufzaehlung, sonst Absatz
    const char* keywords;
};
struct ColumnDoc { const char* table; const char* column; bool certain; const char* de; const char* en; };

// Rohdaten (in ManualData.cpp)
const Chapter* ChapterData(std::size_t& count);
const Section* SectionData(std::size_t& count);
const ColumnDoc* ColumnData(std::size_t& count);
void ForEachTip(void (*fn)(const char* key, const char* de, const char* en, void* user), void* user);

const std::vector<Chapter>& Chapters();
const std::vector<Section>& Sections();
const std::vector<ColumnDoc>& Columns();

// Tooltip zu einer Widget-Beschriftung. Suchreihenfolge: "<Art>:<Beschriftung>", "<Beschriftung>" (jeweils mit
// und ohne "##Suffix"). nullptr, wenn keiner vorhanden.
const char* TooltipFor(const std::string& kind, const std::string& label, bool german);

// Beschreibung einer Spalte: erst exakte Tabelle, dann die allgemeinen Eintraege ("*").
const ColumnDoc* FindColumn(const std::string& table, const std::string& column);

// Volltextsuche (alle Woerter muessen vorkommen; Titeltreffer zuerst).
std::vector<const Section*> Search(const std::string& query, bool german);

} // namespace theseed::mapeditor::core::manual
