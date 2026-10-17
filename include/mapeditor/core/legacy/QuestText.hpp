#pragma once
// QuestText.hpp
// Text-Verweise einer Quest auf QuestDialog.shn (Spalten ID = UInt32, Dialog = Text): Titel,
// Beschreibung und die SAY-Befehle der Skripte. Belegt an den 2304 NA2016-Quests: SAY steht immer
// am Zeilenanfang in der Form "SAY <Text-ID> <NPC|ME>" (19924 Zeilen, 5 davon mit Komma nach der
// ID: "SAY 1502, NPC"), die Beschreibung ist in 2295 Fällen Titel + 1, und die Texte einer Quest
// liegen als zusammenhängender ID-Block in QuestDialog (z. B. Quest 1: 200-205).
//
// Beim Klonen einer Quest verweisen Titel/Beschreibung/SAY zunächst auf die Texte der Vorlage -
// eine Textänderung träfe dann beide Quests. AssignNewQuestTextIds vergibt auf ausdrücklichen
// Wunsch einen freien ID-Block hinter der höchsten belegten ID und ersetzt alle Verweise; das
// Kopieren der Textzeilen übernimmt der Aufrufer (Editor), weil es das SHN-Dokument betrifft.

#include "mapeditor/core/legacy/QuestData.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct SayReference {
    std::size_t offset = 0; // Position der Ziffern im Skripttext
    std::size_t length = 0; // Anzahl Ziffern
    std::uint32_t id = 0;
};

// Alle "SAY <id>"-Verweise eines Skripts in Textreihenfolge (nur am Zeilenanfang, Leerraum davor
// erlaubt; Kommentarzeilen mit ";" zählen nicht).
std::vector<SayReference> FindSayReferences(std::string_view script);

// Ersetzt die SAY-IDs laut remap (nicht enthaltene bleiben). Der übrige Text bleibt unverändert.
std::string RemapSayIds(std::string_view script, const std::map<std::uint32_t, std::uint32_t>& remap);

// Text-IDs einer Quest ohne Duplikate in der Reihenfolge Titel, Beschreibung, SAY (Start, Action,
// Finish); 0 bedeutet "kein Text" und wird ausgelassen.
std::vector<std::uint32_t> QuestTextIds(const QuestRecord& quest);

// Erster Start >= minStart, ab dem count aufeinanderfolgende IDs frei sind (höchstens bis maxId).
std::optional<std::uint32_t> FindFreeTextBlock(const std::set<std::uint32_t>& used, std::size_t count,
                                               std::uint32_t minStart, std::uint32_t maxId = 0xFFFFFFFFu);

// Vergibt allen Text-IDs der Quest neue IDs als Block hinter der höchsten belegten ID (Reihenfolge
// wie QuestTextIds) und ersetzt Titel, Beschreibung und SAY-Verweise. Rückgabe: alt -> neu (leer,
// wenn die Quest keine Texte hat oder kein Block frei ist; die Quest bleibt dann unverändert).
std::map<std::uint32_t, std::uint32_t> AssignNewQuestTextIds(QuestRecord& quest, const std::set<std::uint32_t>& used);

} // namespace theseed::mapeditor::core::legacy
