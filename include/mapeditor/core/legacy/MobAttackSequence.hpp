#pragma once
// MobAttackSequence.hpp
// Angriffsfolgen der Mobs (Server/9Data/Shine/MobAttackSequence/<Mob>.txt, Shine-Text):
//   #Table AttSeq       Order (WORD), Attack (STRING[33])   - Skill je Schritt, "-" = leerer Schritt,
//                                                           "*RepeatStart" = Sondermarke (vermutet:
//                                                           ab hier wiederholen)
//   #Table SkillChange  Condition, Value, From, To          - Regeln (HPLow_ChangeOrder, OutOfRange,
//                                                           HPLow, NextSkill), nur in einem Teil
//                                                           der Dateien
// Belegt an allen 297 NA2016-Dateien: Order läuft 0..n-1 in Dateireihenfolge (eine Ausnahme), jeder
// Attack-Wert ist eine Server-ActiveSkill.InxName.
//
// Bearbeitet wird ein Array von Schritten: ApplyAttackSequence schreibt Schritt i in den i-ten
// Record (Order = i), hängt fehlende Records an und entfernt überzählige - Dateireihenfolge und
// Order bleiben so immer gleich. Alle übrigen Zeilen bleiben bytegleich (SaveShineTextFile).

#include "mapeditor/core/legacy/ShineText.hpp"

#include <expected>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct AttackRule {
    std::string condition;
    std::string value;
    std::string from;
    std::string to;
    friend bool operator==(const AttackRule&, const AttackRule&) = default;
};

struct AttackSequence {
    std::vector<std::string> attacks; // in Dateireihenfolge
    std::vector<AttackRule> rules;
    bool hasRuleTable = false;
    bool ordersSequential = true;     // false: Order in der Datei war nicht 0..n-1 (wird beim Schreiben korrigiert)
};

inline constexpr const char* kAttackRepeatMarker = "*RepeatStart";
inline constexpr const char* kAttackEmpty = "-";

[[nodiscard]] std::expected<AttackSequence, std::string> ReadAttackSequence(const ShineTextFile& file);

// Überträgt die Folge in die Datei (AttSeq muss existieren; SkillChange wird bei Bedarf angelegt).
std::expected<void, std::string> ApplyAttackSequence(ShineTextFile& file, const AttackSequence& sequence);

} // namespace theseed::mapeditor::core::legacy
