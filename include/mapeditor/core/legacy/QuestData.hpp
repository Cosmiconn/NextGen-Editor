#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <array>
#include <expected>

namespace theseed::mapeditor::core::legacy {

// Parser für QuestData.shn - EIN EIGENES, von den binären .shn-Container-Dateien (siehe
// ShnFile.hpp) UND vom Shine-Textformat (siehe ShineText.hpp) komplett verschiedenes Format:
// kein 32-Byte-Verschlüsselungskopf, sondern uint16 Header(=0x0006) + uint16 QuestCount, dann
// variabel lange Records mit eigenem uint32 DataLen-Präfix. Struktur 1:1 aus einem vom Nutzer
// bereitgestellten, gegen die echten NA2016-Dateien voll verifizierten Python-Referenzparser
// übernommen (siehe CHANGELOG [0.44.19]) - byte-exakt gegen Client- UND Server-QuestData.shn
// geprüft: je 2304 Quests, kein einziger Ausrichtungsfehler.
//
// Mehrere Feldgruppen sind bewusst NICHT weiter aufgeschlüsselt (roher Hex-Blob, siehe die
// "*Raw"/"unkN"-Felder) - ihre genaue Bedeutung ist laut Referenzparser noch nicht geklärt,
// allen voran die 144 Byte "rewardsRaw" (12 Belohnungs-Slots, Struktur unbekannt). Ein Editor
// darauf sollte diese Blobs beim Speichern unverändert durchreichen, nicht raten und
// überschreiben.

struct QuestMobObjective {
    std::uint8_t active = 0;
    std::uint8_t isMob = 0;
    std::uint16_t id = 0;
    std::uint8_t hasToBeKilled = 0;
    std::uint8_t amount = 0;
    std::uint8_t unk1 = 0;
    std::uint8_t unk2 = 0;
};

struct QuestItemObjective {
    std::uint8_t active = 0;
    std::uint8_t type = 0;
    std::uint16_t id = 0;
    std::uint16_t amount = 0;
};

struct QuestDrop {
    std::uint32_t active = 0;
    std::uint32_t mobId = 0;
    std::uint32_t amount = 0;
    std::uint32_t itemId = 0;
    std::uint32_t rate = 0;
    std::uint32_t unk1 = 0;
    std::uint32_t unk2 = 0;
};

struct QuestScript {
    std::string text;                 // NUL-Terminator nicht enthalten
    std::uint16_t storedLength = 0;   // aus dem Kopf (Reihenfolge Start/Finish/Action) - siehe unten
};

struct QuestRecord {
    std::uint32_t dataLen = 0; // Gesamtlänge dieses Records inkl. der 4 dataLen-Bytes selbst

    std::uint16_t id = 0;
    std::uint16_t title = 0;        // Index in eine Zeichenketten-Tabelle (siehe docs)
    std::uint16_t description = 0;  // dito
    std::uint8_t unk1 = 0;
    std::uint8_t questGrade = 0;
    std::uint8_t multiQuest = 0;
    std::uint8_t dailyQuest = 0;
    std::uint16_t unk2 = 0;
    std::uint8_t enableQuest = 0;
    std::uint8_t instAcc = 0;
    std::uint8_t needLevel = 0;
    std::uint8_t minLevel = 0;
    std::uint8_t maxLevel = 0;
    std::uint8_t needNpc = 0;
    std::uint16_t startingNpc = 0;   // Index in MobViewInfo.shn (Spalte "ID"), 0 = kein fester NPC
    std::uint8_t needItem = 0;
    std::uint8_t unk3 = 0;
    std::uint16_t itemId = 0;
    std::uint8_t itemVanish = 0;
    std::array<std::uint8_t, 19> unk4{};
    std::uint8_t needPred = 0;
    std::uint8_t unk5 = 0;
    std::uint16_t predecessor = 0;
    std::uint16_t unk6 = 0;
    std::uint8_t needClass = 0;
    std::uint8_t classType = 0;
    std::array<std::uint8_t, 24> unk7{};
    std::uint8_t instHand = 0;
    std::array<std::uint8_t, 3> unk8{};

    std::array<QuestMobObjective, 5> mobs{};
    std::array<QuestItemObjective, 10> items{};
    std::vector<QuestDrop> drops; // Anzahl = dropCount, siehe unten
    std::vector<std::uint8_t> itemDropPadding; // 28*(11-dropCount)+12 Byte, unveränderlich reichen

    std::vector<std::uint8_t> rewardsRaw;   // 144 Byte, Struktur (noch) unbekannt - siehe oben
    std::array<std::uint8_t, 8> extra8{};
    std::array<std::uint8_t, 14> rewardData{}; // liegt zwischen den Skriptlängen und den Skripten selbst

    // Skript-Reihenfolge: die LÄNGEN stehen in der Reihenfolge Start/Finish/Action im Kopf,
    // aber die tatsächlichen Skript-STRINGS folgen physisch in der Reihenfolge Start/Action/
    // Finish - siehe Referenzparser-Kommentar, byte-exakt bestätigt.
    QuestScript start;
    QuestScript action;
    QuestScript finish;
};

struct QuestDataFile {
    std::uint16_t header = 0; // immer 0x0006 bei den bisher geprüften Dateien
    std::vector<QuestRecord> records;
};

std::expected<QuestDataFile, std::string> LoadQuestData(const std::filesystem::path& path);
std::expected<void, std::string> SaveQuestData(const QuestDataFile& file, const std::filesystem::path& path);

} // namespace theseed::mapeditor::core::legacy
