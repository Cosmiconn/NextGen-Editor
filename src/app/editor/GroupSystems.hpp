#pragma once
// GroupSystems.hpp
// Allgemeiner Gruppen-Editor (ItemSystemsEditor.cpp): Tabellen, in denen mehrere Zeilen mit demselben
// Schlüssel einen Eintrag bilden (core::legacy::ShnGroups). Eine Kategorie ist ein eigener Unterreiter
// mit mehreren Systemen (Item-Systeme, Reittiere & Haustiere ...); je System eine Liste der Schlüssel
// links und alle Zeilen des gewählten Eintrags in allen beteiligten Tabellen rechts. Client- und
// Server-Kopie werden gemeinsam geändert (n-te Zeile der Gruppe), alles mit Strg+Z.

#include "EditorShared.hpp"

namespace nge::groups {

using Source = EditorState::ShnSource;

// Spalte, deren Wert in einer anderen Tabelle nachgeschlagen wird (Anzeige neben der Zelle).
struct Lookup {
    const char* column;
    const char* file;
    Source side;
    const char* keyColumn;
    const char* valueColumn;
    bool required; // unbekannter Wert ist ein Problem (rot)
};

struct GroupTable {
    const char* file;
    bool client;
    bool server;
    const char* keyColumn;          // nullptr: Tabelle ohne Schlüssel (globale Einstellungen, eine Gruppe "alle Zeilen")
    const char* titleDe;
    const char* titleEn;
    std::vector<Lookup> lookups;
    const char* uniqueColumn = nullptr; // Zahl, die je Zeile eindeutig sein muss (bekommt beim Kopieren/+Zeile einen freien Wert)
    const char* distinctColumn = nullptr; // Wert, den kein anderer Eintrag haben darf (z. B. das rufende Item) - sonst Problem
};

// Zeilen anderer Tabellen, die auf den Eintrag verweisen (Spalte == Schlüssel). Ohne jumpSystem
// sind es Items (Klick öffnet den Item-Editor über die ID), sonst Einträge eines Systems derselben
// Kategorie (Klick wählt resultKey dort).
struct Link {
    const char* file = nullptr;
    Source side = Source::Client;
    const char* column = nullptr;
    const char* labelDe = nullptr;
    const char* labelEn = nullptr;
    const char* resultKey = "InxName";
    const char* resultName = nullptr; // Spalte für den Namen; nullptr = Item-Name über ItemInfo
    const char* jumpSystem = nullptr;
};

// Name neben dem Schlüssel in der Liste (z. B. MoverView.Name) und Porträt-Vorschau im Kopf.
struct NameSource {
    const char* file = nullptr;
    Source side = Source::Client;
    const char* keyColumn = nullptr;
    const char* valueColumn = nullptr;
};
struct PortraitSource {
    NameSource name;              // liefert den Bildnamen
    const char* folder = nullptr; // unter resmenu, z. B. "game/MobPortrait"
};

struct System {
    const char* id;
    const char* nameDe;
    const char* nameEn;
    const char* noteDe;
    const char* noteEn;
    std::vector<GroupTable> tables;
    Link link;
    NameSource display;
    PortraitSource portrait;
};

struct Category {
    const char* id;          // Automation: groupsystem <id> ...
    const char* titleDe;
    const char* titleEn;
    const char* subtitleDe;
    const char* subtitleEn;
    IconDrawFn icon;
    const char* semanticIcon;
    int subTab;
    std::vector<System> systems;
    EditorState::ItemSystemsState EditorState::* state;
};

// Spalte mit einem Item-InxName (Name aus ItemInfo, unbekanntes Item ist ein Problem).
inline Lookup ItemName(const char* column) { return {column, "ItemInfo.shn", Source::Client, "InxName", "Name", true}; }

const Category& ItemCategory();   // ItemSystemsEditor.cpp
const Category& MountCategory();  // MountSystems.cpp
const Category& HouseCategory();  // HouseSystems.cpp

void DrawGroupSystemsEditor(EditorState& state, const Category& category);
std::string GroupSystemsAutomation(EditorState& state, const Category& category, const std::string& cmd, std::istream& in);

} // namespace nge::groups
