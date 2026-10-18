// HouseSystems.cpp
// MiniHouse (Mini-Häuser und Möbel) und Sammelkarten im Datei Editor (Unterreiter 19) - eine Kategorie
// des Gruppen-Editors (GroupSystems.hpp). MiniHouse-Tabellen sind über ItemID (Item-InxName) verbunden,
// die laufende Nummer Handle bekommt beim Kopieren einen freien Wert; MiniHouseObjAni verweist über die
// numerische Item-ID. Sammelkarten hängen an CC_CardID, Mob-Gruppen an CC_CardMobGroup.
// Spaltenbedeutungen aus NA2016 gelernt; Unsicheres ist im Handbuch als "vermutet" markiert.

#include "GroupSystems.hpp"

namespace nge::groups {
namespace {
const NameSource kItemName{"ItemInfo.shn", Source::Client, "InxName", "Name"};
const NameSource kItemNameById{"ItemInfo.shn", Source::Client, "ID", "Name"};
} // namespace

const Category& HouseCategory() {
    static const std::vector<System> systems = {
        {"houses", "Mini-Häuser", "Mini houses",
         "MiniHouse (Client und Server): je Haus-Item eine Zeile - Handle (laufende Nummer), DummyType (Modell), Backimage "
         "(Hintergrund aus resmenu/game/MiniHouse), Dauer KeepTime_Hour (0 = dauerhaft), HP/SP-Erholung je Tick (ms), Zauberzeit "
         "Casting und Möbelplätze Slot.",
         "MiniHouse (client and server): one row per house item - Handle (running number), DummyType (model), Backimage (background "
         "from resmenu/game/MiniHouse), duration KeepTime_Hour (0 = permanent), HP/SP recovery per tick (ms), cast time Casting and "
         "furniture slots Slot.",
         {{"MiniHouse.shn", true, true, "ItemID", "Haus (MiniHouse)", "House (MiniHouse)", {}, "Handle"}},
         {},
         kItemName,
         {{"MiniHouse.shn", Source::Client, "ItemID", "Backimage"}, "game/MiniHouse"}},
        {"furniture", "Möbel", "Furniture",
         "MiniHouseFurniture (Client und Server): Möbel-Item mit Art (FurnitureType, InvenType, GameType), Aufstellort WALL/BOTTOM/"
         "CEILING, Animation, Gewicht, Dauer und Haltbarkeit, MaxSlot und Emotion. MiniHouseFurnitureObjEffect: Wirkung eines Möbels "
         "(z. B. Teleport-Rolle), benötigtes Item und Geld, Effekt und Sound. Die Möbel-Animationen stehen unter 'Möbel-Animationen'.",
         "MiniHouseFurniture (client and server): furniture item with kind (FurnitureType, InvenType, GameType), placement WALL/BOTTOM/"
         "CEILING, animation, weight, duration and durability, MaxSlot and emotion. MiniHouseFurnitureObjEffect: effect of a piece "
         "(e.g. teleport scroll), required item and money, effect and sound. The furniture animations are under 'Furniture animations'.",
         {{"MiniHouseFurniture.shn", true, true, "ItemID", "Möbel (MiniHouseFurniture)", "Furniture (MiniHouseFurniture)", {}, "Handle"},
          {"MiniHouseFurnitureObjEffect.shn", true, true, "ItemID", "Wirkung (MiniHouseFurnitureObjEffect)", "Effect (MiniHouseFurnitureObjEffect)",
           {{"NeedItem", "ItemInfo.shn", Source::Client, "InxName", "Name", true}}, "Handle"}},
         {},
         kItemName},
        {"furnitureani", "Möbel-Animationen", "Furniture animations",
         "MiniHouseObjAni (Client und Server): Animationsgruppen eines Möbels, über die numerische Item-ID (ItemInfo.ID) verbunden - "
         "Gruppe, Ereignis-Code, nächste Animation (NextAniHandle) und beteiligte Figuren (Actor/Actee, vermutet).",
         "MiniHouseObjAni (client and server): animation groups of a furniture piece, linked via the numeric item ID (ItemInfo.ID) - "
         "group, event code, next animation (NextAniHandle) and participating figures (actor/actee, assumed).",
         {{"MiniHouseObjAni.shn", true, true, "ItemID", "Animationen (MiniHouseObjAni)", "Animations (MiniHouseObjAni)", {}, "Handle"}},
         {},
         kItemNameById},
        {"housesettings", "Haus-Einstellungen", "House settings",
         "Allgemeine Tabellen ohne Schlüssel: MiniHouseEndure (Client und Server: Haltbarkeitsstufen, vermutet) und MiniHouseDummy "
         "(Server: Standard-Haus mit Symbol, Werten und Haus-Typen A-C).",
         "General tables without key: MiniHouseEndure (client and server: durability levels, assumed) and MiniHouseDummy (server: "
         "default house with icon, values and house types A-C).",
         {{"MiniHouseEndure.shn", true, true, nullptr, "Haltbarkeit (MiniHouseEndure)", "Durability (MiniHouseEndure)", {}, "Handle"},
          {"MiniHouseDummy.shn", false, true, nullptr, "Standard-Haus (MiniHouseDummy)", "Default house (MiniHouseDummy)", {}}},
         {}},
        {"cards", "Sammelkarten", "Collect cards",
         "Eine Sammelkarte (Schlüssel CC_CardID) besteht aus CollectCard (Client und Server: Karten-Item, Seltenheit CC_CardGradeType, "
         "Mob-Gruppe), CollectCardView (Client: Bild aus resmenu/Collect, Name, Beschreibung, Mob, Saison) und CollectCardDropRate "
         "(Server: Drop-Chance, vermutet in Promille).",
         "A collect card (key CC_CardID) consists of CollectCard (client and server: card item, rarity CC_CardGradeType, mob group), "
         "CollectCardView (client: image from resmenu/Collect, name, description, mob, season) and CollectCardDropRate (server: drop "
         "chance, assumed per mille).",
         {{"CollectCard.shn", true, true, "CC_CardID", "Karte (CollectCard)", "Card (CollectCard)",
           {ItemName("CC_ItemInx"), {"CC_CardMobGroup", "CollectCardGroupDesc.shn", Source::Client, "CC_CardMobGroup", "CC_MobGroupName", true}},
           nullptr, "CC_ItemInx"},
          {"CollectCardView.shn", true, false, "CC_CardID", "Anzeige (CollectCardView)", "Display (CollectCardView)",
           {{"CC_MobInx", "MobInfo.shn", Source::Client, "InxName", "Name", false}}},
          {"CollectCardDropRate.shn", false, true, "CC_CardID", "Drop-Chance (CollectCardDropRate)", "Drop chance (CollectCardDropRate)", {}}},
         {},
         {"CollectCardView.shn", Source::Client, "CC_CardID", "CC_CardName"},
         {{"CollectCardView.shn", Source::Client, "CC_CardID", "CC_CardFileName"}, "Collect/Max"}},
        {"cardgroups", "Karten-Mobgruppen", "Card mob groups",
         "Mob-Gruppen der Sammelkarten (Schlüssel CC_CardMobGroup): CollectCardGroupDesc (Client und Server: Name) und "
         "CollectCardMobGroup (Server: welche Mobs zur Gruppe gehören, also die Karten dieser Gruppe droppen können - vermutet).",
         "Mob groups of the collect cards (key CC_CardMobGroup): CollectCardGroupDesc (client and server: name) and CollectCardMobGroup "
         "(server: which mobs belong to the group, i.e. may drop the cards of this group - assumed).",
         {{"CollectCardGroupDesc.shn", true, true, "CC_CardMobGroup", "Name (CollectCardGroupDesc)", "Name (CollectCardGroupDesc)", {}},
          {"CollectCardMobGroup.shn", false, true, "CC_CardMobGroup", "Mobs der Gruppe (CollectCardMobGroup)", "Mobs of the group (CollectCardMobGroup)",
           {{"CC_MobInx", "MobInfo.shn", Source::Client, "InxName", "Name", true}}}},
         {"CollectCard.shn", Source::Client, "CC_CardMobGroup", "Karten dieser Gruppe (CollectCard)", "Cards of this group (CollectCard)",
          "CC_CardID", "CC_ItemInx", "cards"},
         {"CollectCardGroupDesc.shn", Source::Client, "CC_CardMobGroup", "CC_MobGroupName"}},
        {"cardrewards", "Karten-Belohnungen", "Card rewards",
         "Tabellen ohne Schlüssel: CollectCardReward (Client und Server: Belohnung ab CC_CardLot gesammelten Karten, Item und Menge), "
         "CollectCardTitle (Client und Server: Titel je Karten-Item) und CollectCardStarRate (Server: Sterne-Chancen je Seltenheit, "
         "vermutet).",
         "Tables without key: CollectCardReward (client and server: reward from CC_CardLot collected cards, item and amount), "
         "CollectCardTitle (client and server: title per card item) and CollectCardStarRate (server: star chances per rarity, assumed).",
         {{"CollectCardReward.shn", true, true, nullptr, "Belohnungen (CollectCardReward)", "Rewards (CollectCardReward)",
           {ItemName("CC_RewardItemInx")}, "CC_RewardID"},
          {"CollectCardTitle.shn", true, true, nullptr, "Titel (CollectCardTitle)", "Titles (CollectCardTitle)", {ItemName("CC_ItemInx")}},
          {"CollectCardStarRate.shn", false, true, nullptr, "Sterne (CollectCardStarRate)", "Stars (CollectCardStarRate)", {}}},
         {}},
    };
    static const Category category{"housing", "MINIHAUS & SAMMELKARTEN", "MINI HOUSE & COLLECT CARDS",
                                   "Mini-Häuser, Möbel und Sammelkarten", "Mini houses, furniture and collect cards",
                                   DrawIconPortal, "module.interface", 19, systems, &EditorState::houseSystems};
    return category;
}

} // namespace nge::groups

namespace nge {

void DrawHouseSystemsEditor(EditorState& state) { groups::DrawGroupSystemsEditor(state, groups::HouseCategory()); }

} // namespace nge
