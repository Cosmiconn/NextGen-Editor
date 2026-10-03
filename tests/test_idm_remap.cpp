// test_idm_remap.cpp
// IDM-Pflege beim Export gegen die echten NA2016-Dateien Rou.idm + Rou.shmd:
//  - Beleg der Indexsemantik: IDM-Indizes 0..1077 = Präfix der SHMD-Dateireihenfolge
//  - unveränderte Karte -> byte-gleiches IDM
//  - Löschen / Duplizieren (Duplikat landet mitten im Modellblock) -> jede erhaltene Zuordnung
//    zeigt danach auf dasselbe Objekt wie vorher (geprüft über Position + Modell)

#include "mapeditor/core/ObjectPlacementIO.hpp"
#include "mapeditor/core/legacy/LegacyIdmAid.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

using namespace theseed::mapeditor::core;
using namespace theseed::mapeditor::core::legacy;

namespace {
int g_failures = 0;
void Check(bool ok, const char* what) {
    if (ok) std::printf("[ok]     %s\n", what);
    else { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; }
}
std::vector<char> ReadAll(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::vector<std::int32_t> WrittenSources(const ObjectPlacementSet& set) {
    std::vector<std::int32_t> out;
    for (const std::size_t i : ShmdWrittenOrder(set)) out.push_back(set.At(i).sourceIndex);
    return out;
}
// Jede Gruppe des Ergebnisses muss dieselben Objekte (Modell + Position) enthalten wie das Original,
// abzüglich gelöschter Objekte.
bool SameObjects(const ObjectSpatialIndex& before, const ObjectPlacementSet& original,
                 const ObjectSpatialIndex& after, const ObjectPlacementSet& edited, std::int32_t deletedSource) {
    const auto order = ShmdWrittenOrder(edited);
    if (before.groups.size() != after.groups.size()) return false;
    for (std::size_t g = 0; g < before.groups.size(); ++g) {
        std::vector<std::int32_t> expected;
        for (const auto i : before.groups[g].indices) if (i != deletedSource) expected.push_back(i);
        if (expected.size() != after.groups[g].indices.size()) return false;
        for (const auto newIdx : after.groups[g].indices) {
            const auto& obj = edited.At(order[static_cast<std::size_t>(newIdx)]);
            if (obj.sourceIndex < 0) return false;
            const auto& orig = original.At(static_cast<std::size_t>(obj.sourceIndex));
            if (orig.modelPath != obj.modelPath || orig.posX != obj.posX || orig.posZ != obj.posZ) return false;
            bool found = false;
            for (const auto e : expected) found = found || e == obj.sourceIndex;
            if (!found) return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path dir = argc > 1 ? argv[1] : "tests/fixtures";
    const auto idm = ParseLegacyIdm(dir / "Rou.idm");
    const auto shmd = ParseLegacyShmd(dir / "Rou.shmd");
    Check(idm.has_value() && shmd.has_value(), "Rou.idm + Rou.shmd geladen");
    if (!idm || !shmd) return 1;

    // --- Beleg der Indexsemantik ---
    std::int32_t maxIdx = -1;
    for (const auto& g : idm->groups) for (const auto i : g.indices) maxIdx = std::max(maxIdx, i);
    Check(maxIdx + 1 == idm->headerValue && idm->headerValue == 1078, "IDM: headerValue 1078 = höchster Index + 1");
    Check(shmd->Count() == 1580, "SHMD: 1580 Objekte (502 nicht im IDM)");
    std::size_t blockStart = 1077;
    while (blockStart > 0 && shmd->At(blockStart - 1).modelPath == shmd->At(1077).modelPath) --blockStart;
    Check(shmd->At(1077).modelPath == shmd->At(1078).modelPath && 1078 - blockStart == 221,
          "Grenze 1078 liegt 221 Objekte tief im letzten alten Block (jun_grass01) -> IDM = Präfix der Dateireihenfolge");
    const auto order0 = ShmdWrittenOrder(*shmd);
    bool fileOrder = true;
    for (std::size_t k = 0; k < order0.size(); ++k) fileOrder = fileOrder && order0[k] == k;
    Check(fileOrder, "Schreibreihenfolge einer unveränderten SHMD = Dateireihenfolge");

    // --- unverändert -> byte-gleich ---
    SpatialIndexRemapReport rep;
    const auto same = RemapSpatialIndex(*idm, WrittenSources(*shmd), &rep);
    const auto tmp = std::filesystem::temp_directory_path() / "nextgen_idm_remap_identity.idm";
    Check(SerializeLegacyIdm(same, tmp).has_value(), "Identität serialisiert");
    Check(rep.identity && rep.uncoveredObjects == 502 && ReadAll(tmp) == ReadAll(dir / "Rou.idm"),
          "unveränderte Karte: IDM byte-gleich, 502 Objekte wie im Original ohne IDM-Zuordnung");

    // --- Löschen eines indizierten Objekts ---
    {
        auto edited = *shmd;
        const std::size_t victim = 191; // in mehreren Gruppen enthalten (u. a. 18 und 19)
        edited.RemoveObject(victim);
        const auto out = RemapSpatialIndex(*idm, WrittenSources(edited), &rep);
        Check(!rep.identity && rep.removedObjects == 1 && rep.droppedReferences > 0, "Löschen: Objekt entfernt, Gruppeneinträge gestrichen");
        Check(out.headerValue == 1077, "Löschen: headerValue = 1077 (Invariante max+1)");
        Check(SameObjects(*idm, *shmd, out, edited, static_cast<std::int32_t>(victim)),
              "Löschen: alle übrigen Zuordnungen zeigen weiter auf dieselben Objekte");
    }
    // --- Duplikat eines frühen Modells: landet beim Schreiben mitten im Block ---
    {
        auto edited = *shmd;
        PlacedObject copy = edited.At(0); // GuildHall, erster Block
        copy.posX += 50.0f;
        copy.sourceIndex = -1;
        edited.AddObject(copy);
        const auto order = ShmdWrittenOrder(edited);
        std::size_t copyPos = 0;
        for (std::size_t k = 0; k < order.size(); ++k) if (order[k] == edited.Count() - 1) copyPos = k;
        Check(copyPos < 10, "Duplikat wird im GuildHall-Block geschrieben (verschiebt alle folgenden Objekte)");
        const auto out = RemapSpatialIndex(*idm, WrittenSources(edited), &rep);
        Check(rep.movedObjects > 1000 && rep.removedObjects == 0 && rep.droppedReferences == 0,
              "Duplikat: >1000 Objekte verschoben, nichts verloren");
        Check(out.headerValue == 1079, "Duplikat: headerValue 1079");
        Check(SameObjects(*idm, *shmd, out, edited, -1), "Duplikat: alle Zuordnungen zeigen weiter auf dieselben Objekte");
    }

    // --- Uruga-Fall: IDM verweist auf mehr Objekte als die SHMD enthält (Urg: 4828 vs 2243) ---
    {
        ObjectSpatialIndex stale;
        stale.hash = "stale";
        stale.headerValue = 10;
        stale.groups = {{{0, 3, 7}}, {{1, 2, 9}}, {{5, 6, 8}}, {{4}}};
        const std::vector<std::int32_t> identity = {0, 1, 2, 3, 4};
        SpatialIndexRemapReport sr;
        const auto same = RemapSpatialIndex(stale, identity, &sr, 5);
        bool equal = same.headerValue == stale.headerValue && same.groups.size() == stale.groups.size();
        for (std::size_t g = 0; equal && g < stale.groups.size(); ++g) equal = same.groups[g].indices == stale.groups[g].indices;
        Check(equal && sr.identity && sr.unattributedIndices == 5,
              "Uruga-Fall unverändert: Indizes ohne SHMD-Objekt (5..9) bleiben, Ergebnis identisch");
        const auto noCount = RemapSpatialIndex(stale, identity, &sr);
        Check(!sr.identity && sr.droppedReferences == 5, "ohne Objektanzahl gingen diese 5 Verweise verloren (alter Fehler)");
        (void)noCount;
        // Objekt 1 gelöscht: 2,3,4 rücken auf 1,2,3; 5..9 bleiben.
        const auto removed = RemapSpatialIndex(stale, {0, 2, 3, 4}, &sr, 5);
        Check(removed.groups[0].indices == std::vector<std::int32_t>{0, 2, 7} &&
              removed.groups[1].indices == std::vector<std::int32_t>{1, 9} &&
              removed.groups[2].indices == std::vector<std::int32_t>{5, 6, 8} &&
              removed.groups[3].indices == std::vector<std::int32_t>{3} && sr.droppedReferences == 1,
              "Uruga-Fall mit gelöschtem Objekt: SHMD-Indizes verschoben, übrige unverändert");
    }

    if (g_failures) { std::fprintf(stderr, "%d Fehler\n", g_failures); return 1; }
    std::printf("Alle IDM-Remap-Tests bestanden.\n");
    return 0;
}
