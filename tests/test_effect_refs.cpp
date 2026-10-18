// test_effect_refs.cpp
// Verweise auf Effekt-Dateien (reseffect): Groß-/Kleinschreibung, leere Werte, Sonderwerte.

#include "mapeditor/core/legacy/EffectRefs.hpp"

#include <cstdio>
#include <cstdlib>

using namespace theseed::mapeditor::core::legacy;

static void Require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "[FEHLER] %s\n", what);
    std::exit(1);
}

int main() {
    const auto stems = EffectFileStems({"MagicBallSho.nif", "StaPoison.NIF", "readme"});
    Require(stems.contains("magicballsho") && stems.contains("stapoison") && stems.contains("readme"), "Dateinamen ohne Endung, klein");

    ShnFile f;
    ShnColumn inx; inx.name = "InxName"; inx.type = 9; inx.length = 32; inx.kind = ShnValueKind::String;
    ShnColumn sho = inx; sho.name = "ShoEffect";
    ShnColumn last = inx; last.name = "LastEffectA";
    f.columns = {inx, sho, last};
    const char* rows[][3] = {{"A", "magicBALLsho", "-"}, {"B", "MyWeapon", "Missing01"}, {"C", "", "0"}};
    for (const auto& r : rows) {
        ShnRow row;
        row.values = {std::string(r[0]), std::string(r[1]), std::string(r[2])};
        f.rows.push_back(std::move(row));
    }
    const auto missing = FindMissingEffects(f, {"ShoEffect", "LastEffectA", "GibtEsNicht"}, stems);
    Require(missing.size() == 2, "zwei Meldungen");
    Require(missing[0].row == 1 && missing[0].column == "ShoEffect" && missing[0].special, "MyWeapon als Sonderwert");
    Require(missing[1].row == 1 && missing[1].value == "Missing01" && !missing[1].special, "fehlende Datei gemeldet");
    std::printf("Effekt-Verweise: ok\n");
    return 0;
}
