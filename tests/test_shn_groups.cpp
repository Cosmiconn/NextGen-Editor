// test_shn_groups.cpp
// Gruppierte SHN-Tabellen (Item-Systeme): Gruppen in Reihenfolge des ersten Auftretens, Zeilen einer
// Gruppe, Einfügeposition hinter der letzten Gruppenzeile, Leerzeichen im Schlüssel.

#include "mapeditor/core/legacy/ShnGroups.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace theseed::mapeditor::core::legacy;

namespace {

void Require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "[FEHLER] %s\n", what);
    std::exit(1);
}

ShnFile MakeSetItem() {
    ShnFile f;
    f.encrypted = false;
    ShnColumn index; index.name = "Index"; index.type = 9; index.length = 32; index.kind = ShnValueKind::String;
    ShnColumn piece; piece.name = "Piece"; piece.type = 1; piece.length = 1; piece.kind = ShnValueKind::UInt8;
    f.columns = {index, piece};
    const char* keys[] = {"BoneSet", "WinSet", "BoneSet ", "WinSet", "Lonely"};
    for (int i = 0; i < 5; ++i) {
        ShnRow r;
        r.values = {std::string(keys[i]), static_cast<std::uint8_t>(i + 2)};
        f.rows.push_back(std::move(r));
    }
    return f;
}

} // namespace

int main() {
    const ShnFile f = MakeSetItem();
    const auto groups = GroupShnRows(f, 0);
    Require(groups.size() == 3, "drei Gruppen");
    Require(groups[0].key == "BoneSet" && groups[1].key == "WinSet" && groups[2].key == "Lonely", "Reihenfolge des ersten Auftretens");
    Require(groups[0].rows == std::vector<std::size_t>({0, 2}), "Leerzeichen am Ende gehören zum selben Schlüssel");
    Require(groups[1].rows == std::vector<std::size_t>({1, 3}), "Gruppe mit verteilten Zeilen");
    Require(ShnGroupRows(f, 0, "WinSet") == std::vector<std::size_t>({1, 3}), "Zeilen einer Gruppe");
    Require(ShnGroupRows(f, 0, "Fehlt").empty(), "unbekannte Gruppe ist leer");
    Require(ShnGroupInsertIndex(f, 0, "BoneSet") == 3, "Einfügen hinter der letzten Gruppenzeile");
    Require(ShnGroupInsertIndex(f, 0, "Neu") == f.rows.size(), "neue Gruppe am Dateiende");
    Require(GroupShnRows(f, 7).empty() && ShnGroupRows(f, -1, "BoneSet").empty(), "ungültige Spalte: keine Gruppen");
    std::printf("SHN-Gruppen: ok\n");
    return 0;
}
