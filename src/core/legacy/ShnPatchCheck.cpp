#include "mapeditor/core/legacy/ShnPatchCheck.hpp"

#include <algorithm>

namespace theseed::mapeditor::core::legacy {

std::expected<void, std::string> CheckShnPatch(const ShnFile& baseline, const ShnFile& current,
                                               const std::vector<std::vector<std::uint8_t>>& cellDirty,
                                               const std::vector<int>& rowOrigin) {
    if (baseline.version != current.version || baseline.defaultRecordLength != current.defaultRecordLength ||
        baseline.encrypted != current.encrypted || baseline.cryptoHeader != current.cryptoHeader ||
        baseline.columns.size() != current.columns.size())
        return std::unexpected("SHN-Struktur wurde ausserhalb einer erlaubten Zell-/Zeilenaenderung veraendert.");
    for (std::size_t ci = 0; ci < baseline.columns.size(); ++ci) {
        const auto& a = baseline.columns[ci];
        const auto& b = current.columns[ci];
        if (a.name != b.name || a.type != b.type || a.length != b.length || a.kind != b.kind ||
            a.synthesizedName != b.synthesizedName)
            return std::unexpected("SHN-Spaltenschema wurde veraendert: " + b.name);
    }

    const bool explicitOrigin = !rowOrigin.empty();
    if (explicitOrigin && rowOrigin.size() != current.rows.size())
        return std::unexpected("SHN-Zeilenzuordnung passt nicht zur Zeilenzahl (interner Fehler).");
    if (!explicitOrigin && current.rows.size() < baseline.rows.size())
        return std::unexpected("SHN-Zeilen wurden ohne Zuordnung geloescht/umgeordnet.");

    std::vector<char> used(baseline.rows.size(), 0);
    const auto allMarked = [&](std::size_t ri) {
        return ri < cellDirty.size() && cellDirty[ri].size() == current.columns.size() &&
               std::all_of(cellDirty[ri].begin(), cellDirty[ri].end(), [](std::uint8_t v) { return v != 0; });
    };
    for (std::size_t ri = 0; ri < current.rows.size(); ++ri) {
        const long long origin = explicitOrigin ? rowOrigin[ri] : static_cast<long long>(ri);
        if (origin < 0 || origin >= static_cast<long long>(baseline.rows.size())) {
            if (!allMarked(ri))
                return std::unexpected("Neue SHN-Zeile " + std::to_string(ri) +
                                       " ist nicht vollstaendig als explizite Aenderung markiert.");
            continue;
        }
        const auto o = static_cast<std::size_t>(origin);
        if (used[o]) return std::unexpected("SHN-Zeile " + std::to_string(o) + " ist doppelt zugeordnet.");
        used[o] = 1;
        const auto& base = baseline.rows[o];
        const auto& row = current.rows[ri];
        if (row.values.size() != base.values.size())
            return std::unexpected("SHN-Zellenstruktur wurde in Zeile " + std::to_string(ri) + " veraendert.");
        for (std::size_t ci = 0; ci < base.values.size(); ++ci) {
            if (row.values[ci] == base.values[ci]) continue;
            const bool marked = ri < cellDirty.size() && ci < cellDirty[ri].size() && cellDirty[ri][ci] != 0;
            if (!marked)
                return std::unexpected("SHN-Sicherheitscheck: nicht markierte Zelle wurde veraendert (Zeile " +
                                       std::to_string(ri) + ", Spalte " + std::to_string(ci) + ").");
        }
    }
    return {};
}

} // namespace theseed::mapeditor::core::legacy
