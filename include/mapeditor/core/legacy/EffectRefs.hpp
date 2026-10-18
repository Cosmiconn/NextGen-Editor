#pragma once
// EffectRefs.hpp
// Verweise auf Effekt-Dateien: Spalten wie ActiveSkillView.ShoEffect oder AbStateView.LoopEffect
// nennen eine Datei reseffect/<Name>.nif (Groß-/Kleinschreibung egal). FindMissingEffects meldet
// Werte, zu denen es keine Datei gibt. "-", "0" und leere Werte bedeuten "kein Effekt"; Sonderwerte
// (z. B. "MyWeapon", vermutet: die eigene Waffe) werden getrennt gemeldet.

#include "mapeditor/core/legacy/ShnFile.hpp"

#include <set>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct MissingEffect {
    std::size_t row = 0;
    std::string column;
    std::string value;
    bool special = false; // Wert aus der Sonderwert-Liste - vermutlich keine Datei
};

// Dateinamen eines Effekt-Ordners ohne Endung, klein geschrieben.
[[nodiscard]] std::set<std::string> EffectFileStems(const std::vector<std::string>& fileNames);

[[nodiscard]] std::vector<MissingEffect> FindMissingEffects(const ShnFile& file, const std::vector<std::string>& columns,
                                                            const std::set<std::string>& lowercaseStems,
                                                            const std::set<std::string>& specialValues = {"MyWeapon"});

} // namespace theseed::mapeditor::core::legacy
