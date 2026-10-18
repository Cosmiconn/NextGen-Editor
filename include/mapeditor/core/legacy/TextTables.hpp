#pragma once
// TextTables.hpp
// Text- und Lokalisierungstabellen (TextData*, ScriptMsg, AnnounceData, ...): welche Spalten Text
// tragen, wie eine Zeile heißt (Schlüssel) und der CSV-Austausch für Übersetzungen.
//
// CSV-Format (RFC 4180, Komma, Felder bei Bedarf in Anführungszeichen, "" für ein Anführungszeichen,
// Zeilenumbrüche innerhalb eines Feldes erlaubt): Kopf "table,key,column,text", danach je Textzelle
// eine Zeile. Der Text wird bytegenau wie in der SHN übernommen (keine Umkodierung).

#include "mapeditor/core/legacy/ShnFile.hpp"

#include <expected>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

// Spalten, die Anzeigetext enthalten: Zeichenketten variabler Länge (Typ 26) oder feste mit
// mindestens 64 Byte. Kurze feste Zeichenketten sind Schlüssel/Dateinamen (acVoice, SM_Inx ...).
[[nodiscard]] std::vector<int> TextColumns(const ShnFile& file);

// Schlüssel aller Zeilen: der Wert der ersten Spalte, wenn sie keine Textspalte ist, sonst "#<Zeile>".
// Kommt ein Wert mehrfach vor (QuestDialog hat doppelte IDs), heißt das zweite Vorkommen "<Wert>#2",
// das dritte "<Wert>#3" usw. - so bleibt jede Zeile eindeutig adressierbar.
[[nodiscard]] std::vector<std::string> TextRowKeys(const ShnFile& file);

struct TextCsvRow {
    std::string table;  // Dateiname ohne Pfad, z.B. "ScriptMsg.shn"
    std::string key;
    std::string column;
    std::string text;
};

[[nodiscard]] std::string WriteTextCsv(const std::vector<TextCsvRow>& rows);
[[nodiscard]] std::expected<std::vector<TextCsvRow>, std::string> ParseTextCsv(const std::string& csv);

// Alle Textzellen einer Tabelle als CSV-Zeilen.
[[nodiscard]] std::vector<TextCsvRow> TextCsvRowsFor(const ShnFile& file, const std::string& tableName);

struct TextCellChange {
    std::size_t row = 0;
    int column = -1;
    std::string text;
};
struct TextCsvDiff {
    std::vector<TextCellChange> changes; // nur Zellen, deren Text sich unterscheidet
    std::vector<std::string> problems;   // unbekannter Schlüssel / unbekannte Spalte / zu lang
    std::size_t unchanged = 0;
};
// Gleicht die CSV-Zeilen dieser Tabelle (table == tableName) mit der Datei ab.
[[nodiscard]] TextCsvDiff DiffTextCsv(const ShnFile& file, const std::string& tableName, const std::vector<TextCsvRow>& rows);

} // namespace theseed::mapeditor::core::legacy
