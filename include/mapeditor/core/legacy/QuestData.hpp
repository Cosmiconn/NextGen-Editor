#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <array>
#include <expected>
#include <optional>

namespace theseed::mapeditor::core::legacy {

// Parser für QuestData.shn - EIN EIGENES, von den binären .shn-Container-Dateien (siehe
// ShnFile.hpp) UND vom Shine-Textformat (siehe ShineText.hpp) komplett verschiedenes Format:
// kein 32-Byte-Verschlüsselungskopf, sondern uint16 Header(=0x0006) + uint16 QuestCount, dann
// variabel lange Records mit eigenem uint32 DataLen-Präfix.
//
// Feldreihenfolge und -namen stammen aus einem vom Nutzer bereitgestellten Python-Referenzparser
// (CHANGELOG [0.44.19]). Dessen Byte-Lage war ab dem Titel um 8 Byte verschoben (er las Titel
// und Beschreibung als uint16 und kannte zwei Füllfelder nicht; die fehlenden 8 Byte glich ein
// "Padding"/"extra8" am Ende aus, deshalb war der Roundtrip trotzdem bytegenau). Die hier
// verwendete Lage ist an allen 2304 Quests der NA2016-QuestData.shn belegt (test_questdata):
//   - Beschreibung = Titel + 1 bei 2295 Quests (beide uint32, Werte > 65535 kommen vor),
//   - alle 1390 Vorgänger (needPred=1) sind vorhandene Quest-IDs, alle 2099 Levelbereiche
//     (needLevel=1) haben min <= max,
//   - 941 von 960 Drops nennen ein Item, das auch als Item-Ziel der Quest eingetragen ist,
//   - alle Füllfelder, freien Drop-Slots und freien Belohnungseinträge sind 0.
// Felder mit "unk" im Namen sind weiterhin ungeklärt und werden unverändert durchgereicht.
//
// Record-Lage (Offsets ab dem Byte nach dataLen):
//   0 id u16 · 2 Füllfeld u16 · 4 title u32 · 8 description u32 · 12 unk1, grade, multi, daily
//   16 unk2 u16 · 18 Füllfeld u16 · 20 enable, instAcc, needLevel, min, max, needNpc · 26 NPC u16
//   28 needItem, unk3 · 30 itemId u16 · 32 itemVanish · 33 unk4[19] · 52 needPred, unk5
//   54 predecessor u16 · 56 unk6 u16 · 58 needClass, classType · 60 unk7[24] · 84 instHand, unk8[3]
//   88 5 × Monster-/NPC-Ziel (8 Byte) · 128 10 × Item-Ziel (6 Byte) · 188 dropCount u32
//   192 10 × Drop (32 Byte) · 512 12 × Belohnung (12 Byte) · 656 Skriptlängen, rewardData, Skripte

struct QuestMobObjective {
    std::uint8_t active = 0;
    std::uint8_t isMob = 0;          // in NA2016 immer 0
    std::uint16_t id = 0;            // MobInfo-ID (NPC oder Monster)
    std::uint8_t hasToBeKilled = 0;  // 0: NPC aufsuchen (Anzahl 0), 1: Monster besiegen
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

inline constexpr std::size_t kQuestDropSlots = 10;
inline constexpr std::size_t kQuestDropBytes = 32;

struct QuestDrop {
    std::uint32_t active = 0;  // 1, in 18 Fällen 2 (Bedeutung offen)
    std::uint32_t mobId = 0;
    std::uint32_t amount = 0;  // in NA2016 immer 1
    std::uint32_t itemId = 0;
    std::uint32_t rate = 0;    // bis 1000000 (vermutlich Anteil von 1 Mio.)
    std::uint32_t unk1 = 0;    // meist 1
    std::uint32_t unk2 = 0;    // meist 1, sonst 2..20 (unk1 <= unk2: vermutlich min./max. Anzahl)
    std::uint32_t unk3 = 0;    // 0 oder 2
};

// Belohnungen: 12 Einträge à 12 Byte. In allen Daten: pad 0, unbelegte Einträge komplett 0,
// use ∈ {1, 2}, type ∈ {0, 1, 2, 4}; bei type 2 enthält value die Item-ID (Bits 0-15) und die
// Anzahl (Bits 16-31), die oberen 4 Byte sind bei allen Einträgen 0.
// Gesichert ist nur die Struktur. Die Bedeutungen sind aus den Wertebereichen abgeleitet und
// nicht aus Client-/Servercode belegt: type 0 = EXP (bis 572 Mio.), type 1 = Geld (bis 4,8 Mio.),
// type 2 = Item, type 4 unbekannt (Werte 100/700); use 2 tritt in Gruppen von 2-5 bzw. 10
// Einträgen auf (vermutlich Auswahl- bzw. klassenabhängige Belohnung).
inline constexpr std::size_t kQuestRewardSlots = 12;

struct QuestRewardEntry {
    std::uint8_t use = 0;    // 0 = unbelegt, 1 = fest (vermutet), 2 = Auswahl/Gruppe (vermutet)
    std::uint8_t type = 0;   // 0 EXP, 1 Geld, 2 Item, 4 unbekannt (Deutung s. o.)
    std::uint16_t pad = 0;   // in allen Daten 0, wird unverändert geschrieben
    std::uint64_t value = 0; // little endian, bei Items: ID | Anzahl << 16

    [[nodiscard]] bool Empty() const { return use == 0 && type == 0 && pad == 0 && value == 0; }
    [[nodiscard]] std::uint16_t ItemId() const { return static_cast<std::uint16_t>(value & 0xFFFF); }
    [[nodiscard]] std::uint16_t ItemCount() const { return static_cast<std::uint16_t>((value >> 16) & 0xFFFF); }
    void SetItem(std::uint16_t id, std::uint16_t count) {
        value = (value & ~std::uint64_t{0xFFFFFFFF}) | id | (static_cast<std::uint64_t>(count) << 16);
    }
};

struct QuestScript {
    std::string text;                 // NUL-Terminator nicht enthalten
    std::uint16_t storedLength = 0;   // aus dem Kopf (Reihenfolge Start/Finish/Action) - siehe unten
};

struct QuestRecord {
    std::uint32_t dataLen = 0; // Gesamtlänge dieses Records inkl. der 4 dataLen-Bytes selbst

    std::uint16_t id = 0;
    std::uint16_t idPad = 0;        // in NA2016 immer 0
    std::uint32_t title = 0;        // ID in QuestDialog.shn
    std::uint32_t description = 0;  // dito, meist title + 1
    std::uint8_t unk1 = 0;
    std::uint8_t questGrade = 0;
    std::uint8_t multiQuest = 0;
    std::uint8_t dailyQuest = 0;
    std::uint16_t unk2 = 0;
    std::uint16_t unk2Pad = 0;      // in NA2016 immer 0
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
    std::vector<QuestDrop> drops; // Anzahl = dropCount (höchstens 10)
    // Rohbytes der freien Drop-Slots, 32 × (10 - dropCount), in NA2016 immer 0. Beim Speichern
    // passt SaveQuestData die Länge an eine geänderte Drop-Anzahl an (vorne, s. QuestData.cpp).
    std::vector<std::uint8_t> unusedDropSlots;

    std::array<QuestRewardEntry, kQuestRewardSlots> rewards{};
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
