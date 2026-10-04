#include "mapeditor/core/DataDependency.hpp"

#include <array>
#include <stdexcept>

namespace theseed::mapeditor::core {
namespace {

using C = DataDependencyClass;
using T = DataTree;

constexpr std::array kNewMap = {
    DataDependencyRule{T::Client, C::Required,    "ressystem/MapInfo.shn",                "Map-Stammdaten"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/MapInfo.shn",             "Server-Kopie der Map-Stammdaten"},
    DataDependencyRule{T::Client, C::Required,    "ressystem/MapViewInfo.shn",            "Client-Anzeige der Map"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/View/MapViewInfo.shn",     "Server-View-Kopie"},
    DataDependencyRule{T::Client, C::Asset,       "resmap/<Map>/",                       "Eigentliche Karten- und Companion-Dateien"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/MapLinkPoint.shn",           "Nur bei Map-/Gate-Links"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MapLinkPoint.shn",        "Nur bei Map-/Gate-Links"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/MapWayPoint.shn",            "Nur bei Wegpunkt-/Navigationsdaten"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MapWayPoint.shn",         "Nur bei Wegpunkt-/Navigationsdaten"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/TownPortal.shn",             "Nur wenn per TownPortal erreichbar"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/TownPortal.shn",          "Nur wenn per TownPortal erreichbar"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/World/RecallCoord.txt",   "Nur bei Recall-Ziel"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/World/NPC.txt",           "Nur bei NPCs/Gates auf der Map"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MobRegen/<Map>.txt",      "Nur bei Mob-Spawns"}
};

constexpr std::array kNewItem = {
    DataDependencyRule{T::Client, C::Required,    "ressystem/ItemInfo.shn",                   "Verifizierte Item-ID-Familie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/ItemInfo.shn",                "Verifizierte Item-ID-Familie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/ItemInfoServer.shn",          "Server-Erweiterung derselben Item-ID"},
    DataDependencyRule{T::Client, C::Required,    "ressystem/ItemViewInfo.shn",               "Client-View derselben Item-ID"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/View/ItemViewInfo.shn",       "Server-View derselben Item-ID"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/GradeItemOption.shn",            "Nur bei entsprechender Grade/Option-Semantik"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/GradeItemOption.shn",         "Nur bei entsprechender Grade/Option-Semantik"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/ItemUpgrade.shn",             "Nur bei eigener Upgrade-Konfiguration"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/ItemServerEquipTypeInfo.shn", "Nur bei neuem/geändertem Equip-Type"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/ItemViewEquipTypeInfo.shn",      "Nur bei neuem/geändertem Equip-Type"},
    DataDependencyRule{T::Client, C::Conditional, "resitem/",                                "Nur bei neuen/ersetzten Item-Assets"},
    DataDependencyRule{T::Client, C::Conditional, "reschar/",                                "Nur wenn die Ausrüstung Character-Assets benötigt"}
};

constexpr std::array kNewEquipment = {
    DataDependencyRule{T::Client, C::Required,    "ressystem/ItemInfo.shn",                   "Item-Basisdatensatz"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/ItemInfo.shn",                "Item-Basisdatensatz"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/ItemInfoServer.shn",          "Server-Erweiterung"},
    DataDependencyRule{T::Client, C::Required,    "ressystem/ItemViewInfo.shn",               "Darstellung/Ausrüstung"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/View/ItemViewInfo.shn",       "Server-View-Kopie"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/WeaponAttrib.shn",               "Nur wenn ein neuer/geänderter Waffentyp nötig ist"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/WeaponAttrib.shn",            "Nur wenn ein neuer/geänderter Waffentyp nötig ist"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/ItemServerEquipTypeInfo.shn", "Nur bei neuem/geändertem Equip-Type"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/ItemViewEquipTypeInfo.shn",      "Nur bei neuem/geändertem Equip-Type"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/ItemUpgrade.shn",             "Nur bei eigener Upgrade-Konfiguration"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/SetItem.shn",                 "Nur als Set-Gegenstand"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/SetItemEffect.shn",           "Nur mit Set-Effekt"},
    DataDependencyRule{T::Client, C::Conditional, "resitem/",                                "Nur bei neuen/ersetzten Item-Assets"},
    DataDependencyRule{T::Client, C::Conditional, "reschar/",                                "Nur bei neuen/ersetzten Equip-/Character-Assets"}
};

constexpr std::array kPlaceNpc = {
    DataDependencyRule{T::Server, C::Required, "9Data/Shine/World/NPC.txt", "Platzierung einer bereits existierenden NPC-/Mob-Identität"},
    DataDependencyRule{T::Client, C::Reference, "ressystem/MobViewInfo.shn", "Modell-/View-Auflösung; nicht wegen Platzierung ändern"},
    DataDependencyRule{T::Client, C::Reference, "ressystem/NPCViewInfo.shn", "Avatar-Auflösung, falls verwendet"}
};

constexpr std::array kNewNpcIdentity = {
    DataDependencyRule{T::Client, C::Required,    "ressystem/MobInfo.shn",               "Verifizierte Mob/NPC-ID-Familie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/MobInfo.shn",            "Verifizierte Mob/NPC-ID-Familie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/MobInfoServer.shn",      "Server-Erweiterung derselben ID"},
    DataDependencyRule{T::Client, C::Required,    "ressystem/MobViewInfo.shn",           "Client-View derselben ID"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/View/MobViewInfo.shn",   "Server-View derselben ID"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/World/NPC.txt",          "Nur wenn der NPC auf einer Map platziert wird"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/NPCViewInfo.shn",           "Nur bei Avatar-NPC/NpcViewIndex"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/View/NPCViewInfo.shn",   "Nur bei Avatar-NPC/NpcViewIndex"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/NpcDialogData.shn",         "Nur bei NPC-Dialog"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/NPCItemList/<NPC>.txt",  "Nur bei Merchant/Shop"},
    DataDependencyRule{T::Client, C::Asset,       "reschar/",                           "Nur bei neuer Character-/Mob-Darstellung"}
};

constexpr std::array kNewMob = {
    DataDependencyRule{T::Client, C::Required,    "ressystem/MobInfo.shn",                  "Verifizierte Mob-ID-Familie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/MobInfo.shn",               "Verifizierte Mob-ID-Familie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/MobInfoServer.shn",         "Server-Erweiterung derselben ID"},
    DataDependencyRule{T::Client, C::Required,    "ressystem/MobViewInfo.shn",              "Client-View derselben ID"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/View/MobViewInfo.shn",      "Server-View derselben ID"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MobRegen/<Map>.txt",        "Nur wenn der Mob gespawnt wird"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MobSpecies.shn",            "Nur wenn die Mob-Konfiguration darauf verweist"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MobWeapon.shn",             "Nur bei eigener Waffen-/Attack-Konfiguration"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MobResist.shn",             "Nur bei eigener Resistenz-Konfiguration"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MobLifeTime.shn",           "Nur bei Lifetime-Semantik"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MobRoam/<Mob>.txt",         "Nur bei Patrouillenroute"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/LuaScript/AIScript/<Mob>.lua", "Nur wenn referenziert"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/MobBehaviorDescript/<...>.ps", "Nur wenn referenziert"},
    DataDependencyRule{T::Client, C::Asset,       "reschar/",                              "Nur bei neuem Modell/Animation/Texture"}
};

constexpr std::array kNewQuest = {
    DataDependencyRule{T::Client, C::Required,    "ressystem/QuestData.shn",             "Client-Kopie des Quest-Datensatzes; im NA2016-Bestand byte-identisch zur Server-Kopie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/QuestData.shn",          "Server-Kopie des Quest-Datensatzes; im NA2016-Bestand byte-identisch zur Client-Kopie"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/QuestDialog.shn",          "Nur für neue/geänderte Text-IDs"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/QuestDialog.shn",       "Nur wenn die Server-Kopie mitgeführt wird"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/QuestScript.shn",       "Nur wenn der Questpfad registrierte Scriptdaten benötigt"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/QuestSpecies.shn",      "Nur bei tatsächlich benötigter Species-Semantik"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/NpcDialogData.shn",        "Nur wenn der Questgeberdialog geändert wird"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/World/NPC.txt",         "Nur wenn ein neuer Questgeber platziert wird"},
    DataDependencyRule{T::Client, C::Reference,   "ressystem/ItemInfo.shn",             "Reward-/Requirement-Validierung"},
    DataDependencyRule{T::Client, C::Reference,   "ressystem/MobInfo.shn",              "Kill-/Mobziel-Validierung"}
};

constexpr std::array kNewSkill = {
    DataDependencyRule{T::Client, C::Required,    "ressystem/ActiveSkill.shn",                  "Verifizierte ActiveSkill-ID-Familie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/ActiveSkill.shn",               "Verifizierte ActiveSkill-ID-Familie"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/ActiveSkillInfoServer.shn",     "Server-Erweiterung derselben Skill-ID"},
    DataDependencyRule{T::Client, C::Required,    "ressystem/ActiveSkillView.shn",              "Client-View derselben Skill-ID"},
    DataDependencyRule{T::Server, C::Required,    "9Data/Shine/View/ActiveSkillView.shn",      "Server-View derselben Skill-ID"},
    DataDependencyRule{T::Client, C::Conditional, "ressystem/ItemInfo.shn",                     "Nur wenn ein Skillbuch-/Lern-Item erzeugt wird"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/ItemInfoServer.shn",            "Nur wenn ein Skillbuch-/Lern-Item erzeugt wird"}
};

constexpr std::array kShop = {
    DataDependencyRule{T::Server, C::Required,  "9Data/Shine/NPCItemList/<NPC>.txt", "Händler-Inventar"},
    DataDependencyRule{T::Client, C::Reference, "ressystem/ItemInfo.shn",            "Item-Auswahl/Validierung"},
    DataDependencyRule{T::Client, C::Reference, "ressystem/MobViewInfo.shn",         "NPC-/Merchant-Auflösung"}
};

constexpr std::array kDrop = {
    DataDependencyRule{T::Server, C::Required,  "9Data/Shine/World/ItemDropTable.txt", "Drop-Konfiguration"},
    DataDependencyRule{T::Client, C::Reference, "ressystem/ItemInfo.shn",              "Drop-Item-Validierung"},
    DataDependencyRule{T::Client, C::Reference, "ressystem/MobInfo.shn",               "Mob-Validierung"}
};

constexpr std::array kPortal = {
    DataDependencyRule{T::Client, C::Conditional, "ressystem/TownPortal.shn",           "TownPortal-Menü/Skill"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/TownPortal.shn",        "Synchronisierte vorhandene Server-Kopie"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/World/RecallCoord.txt", "Recall-Ziele"},
    DataDependencyRule{T::Server, C::Conditional, "9Data/Shine/World/NPC.txt",         "NPC-/Gate-basierte Portale"}
};

constexpr std::array kProfiles = {
    DataDependencyProfile{DataChangeKind::NewMap,         "Neue Karte",          "New map",          kNewMap},
    DataDependencyProfile{DataChangeKind::NewItem,        "Neues Item",          "New item",         kNewItem},
    DataDependencyProfile{DataChangeKind::NewEquipment,   "Waffe / Rüstung",     "Weapon / armor",   kNewEquipment},
    DataDependencyProfile{DataChangeKind::PlaceNpc,       "NPC platzieren",      "Place NPC",        kPlaceNpc},
    DataDependencyProfile{DataChangeKind::NewNpcIdentity, "Neue NPC-Identität",  "New NPC identity", kNewNpcIdentity},
    DataDependencyProfile{DataChangeKind::NewMob,         "Neuer Mob",           "New mob",          kNewMob},
    DataDependencyProfile{DataChangeKind::NewQuest,       "Neue Quest",          "New quest",        kNewQuest},
    DataDependencyProfile{DataChangeKind::NewActiveSkill, "Neuer Skill",         "New skill",        kNewSkill},
    DataDependencyProfile{DataChangeKind::Shop,           "Shop / Händler",      "Shop / merchant",  kShop},
    DataDependencyProfile{DataChangeKind::DropTable,      "Drop Table",          "Drop table",       kDrop},
    DataDependencyProfile{DataChangeKind::Portal,         "Portale / Recall",    "Portals / recall", kPortal}
};

} // namespace

std::span<const DataDependencyProfile> DataDependencyProfiles() {
    return kProfiles;
}

const DataDependencyProfile& DataDependencyProfileFor(DataChangeKind kind) {
    for (const auto& profile : kProfiles)
        if (profile.kind == kind) return profile;
    throw std::logic_error("Unknown DataChangeKind");
}

std::string_view DataTreeName(DataTree tree) {
    return tree == DataTree::Client ? "CLIENT" : "SERVER";
}

std::string_view DataDependencyClassNameDe(DataDependencyClass value) {
    switch (value) {
        case C::Required: return "PFLICHT";
        case C::Conditional: return "BEDINGT";
        case C::Reference: return "REFERENZ";
        case C::Asset: return "ASSET";
    }
    return "?";
}

std::string_view DataDependencyClassNameEn(DataDependencyClass value) {
    switch (value) {
        case C::Required: return "REQUIRED";
        case C::Conditional: return "CONDITIONAL";
        case C::Reference: return "REFERENCE";
        case C::Asset: return "ASSET";
    }
    return "?";
}

} // namespace theseed::mapeditor::core
