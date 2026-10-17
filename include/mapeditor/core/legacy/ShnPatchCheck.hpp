#pragma once
// ShnPatchCheck.hpp
// Sicherheitsprüfung vor dem Speichern einer SHN-Tabelle: Das Dokument darf sich gegenüber dem
// Stand auf der Platte (baseline) nur durch ausdrücklich markierte Änderungen unterscheiden -
// geänderte Zellen (cellDirty), neue Zeilen (ganz markiert) und gelöschte Zeilen. Kopf und
// Spaltenschema müssen gleich bleiben.
//
// rowOrigin ordnet jeder aktuellen Zeile ihre Zeile in der baseline zu (-1 = neu). Leer bedeutet
// "unverändert in der Reihenfolge": Zeile i entspricht baseline-Zeile i, dahinter nur neue Zeilen;
// gelöschte Zeilen sind dann nicht erlaubt (sie wären nicht von einer Umordnung unterscheidbar).

#include "mapeditor/core/legacy/ShnFile.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace theseed::mapeditor::core::legacy {

std::expected<void, std::string> CheckShnPatch(const ShnFile& baseline, const ShnFile& current,
                                               const std::vector<std::vector<std::uint8_t>>& cellDirty,
                                               const std::vector<int>& rowOrigin);

} // namespace theseed::mapeditor::core::legacy
