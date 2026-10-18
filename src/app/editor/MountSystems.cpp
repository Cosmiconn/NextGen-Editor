// MountSystems.cpp
// Reittiere (Mover) und Haustiere (Pup) im Datei Editor (Unterreiter 18) - eine Kategorie des
// Gruppen-Editors (GroupSystems.hpp). Mover-Tabellen sind über MoverIDX verbunden, die Pup-Tabellen
// über PupIDX; MoverMain.MoverID und PupMain.PupID bekommen beim Kopieren eine freie Nummer.
// Spaltenbedeutungen aus NA2016 gelernt; Unsicheres ist im Handbuch als "vermutet" markiert.

#include "GroupSystems.hpp"

namespace nge::groups {

const Category& MountCategory() {
    static const std::vector<System> systems = {
        {"mover", "Mover", "Mover",
         "Ein Mover (Reittier/Fahrzeug) besteht aus MoverMain (Client und Server: Nummer, Zauberzeit, Abklingzeit, Lauf-/Gehtempo, "
         "Dauer in Stunden, Sitzplätze), MoverItem (Item, das ihn ruft), MoverHG (Hunger: Futter-Item, Menge, Maximum, Verbrauch), "
         "MoverView (Client: Name, Modellordner, Bilder, Typ) und MoverSlotView (Sitzplätze, Charakter sichtbar). MoverAbility und "
         "MoverUseSkill sind in NA2016 leer. Das Bild kommt aus resmenu/game/MobPortrait/<Normalmage>.",
         "A mover (mount/vehicle) consists of MoverMain (client and server: number, cast time, cooldown, run/walk speed, duration in "
         "hours, seats), MoverItem (item that summons it), MoverHG (hunger: feed item, amount, maximum, consumption), MoverView (client: "
         "name, model folder, images, type) and MoverSlotView (seats, character visible). MoverAbility and MoverUseSkill are empty in "
         "NA2016. The image comes from resmenu/game/MobPortrait/<Normalmage>.",
         {{"MoverMain.shn", true, true, "MoverIDX", "Grundwerte (MoverMain)", "Base values (MoverMain)", {}, "MoverID"},
          {"MoverItem.shn", true, true, "MoverIDX", "Rufendes Item (MoverItem)", "Summoning item (MoverItem)", {ItemName("ItemIDX")}, nullptr, "ItemIDX"},
          {"MoverHG.shn", true, true, "MoverIDX", "Hunger (MoverHG)", "Hunger (MoverHG)",
           {{"FeedType", "ItemInfo.shn", Source::Client, "InxName", "Name", false}}},
          {"MoverView.shn", true, false, "MoverIDX", "Anzeige (MoverView)", "Display (MoverView)",
           {{"MoverTypeIDX", "MoverSlotCharAni.shn", Source::Client, "MoverTypeIDX", "MoverTypeIDX", true}}},
          {"MoverSlotView.shn", true, false, "MoverIDX", "Sitzplätze (MoverSlotView)", "Seats (MoverSlotView)", {}},
          {"MoverAbility.shn", true, true, "MoverIDX", "Kampfwerte (MoverAbility, in NA2016 leer)", "Combat values (MoverAbility, empty in NA2016)", {}},
          {"MoverUseSkill.shn", true, true, "MoverIDX", "Skills (MoverUseSkill, in NA2016 leer)", "Skills (MoverUseSkill, empty in NA2016)",
           {{"ActiveSkillIDX", "ActiveSkill.shn", Source::Client, "InxName", "Name", true}}}},
         {},
         {"MoverView.shn", Source::Client, "MoverIDX", "Name"},
         {{"MoverView.shn", Source::Client, "MoverIDX", "Normalmage"}, "game/MobPortrait"}},
        {"movertype", "Mover-Typen", "Mover types",
         "MoverSlotCharAni (Client): Reit-Animationen je Mover-Typ und Sitzplatz - welche Charakter-Animation (CharAni) welche "
         "Reit-Animation bekommt (RideLegECode/RideAniECode, vermutet). Gilt für alle Mover mit diesem MoverView.MoverTypeIDX.",
         "MoverSlotCharAni (client): ride animations per mover type and seat - which character animation (CharAni) gets which ride "
         "animation (RideLegECode/RideAniECode, assumed). Applies to every mover with this MoverView.MoverTypeIDX.",
         {{"MoverSlotCharAni.shn", true, false, "MoverTypeIDX", "Animationen je Sitzplatz (MoverSlotCharAni)", "Animations per seat (MoverSlotCharAni)", {}}},
         {"MoverView.shn", Source::Client, "MoverTypeIDX", "Mover dieses Typs (MoverView)", "Movers of this type (MoverView)", "MoverIDX", "Name", "mover"}},
        {"moverupgrade", "Mover-Aufwertung", "Mover upgrade",
         "MoverUpgradeEffect (Client und Server): Boni je Aufwertungsstufe eines Movers (Zeile 1 = Stufe 0, vermutet) - Tempo, "
         "HP/SP-Erholung, Zauber- und Abklingzeit, Effekt und Zustand (AbStateIDX mit Stärke).",
         "MoverUpgradeEffect (client and server): bonuses per upgrade level of a mover (row 1 = level 0, assumed) - speed, HP/SP "
         "recovery, cast and cooldown time, effect and state (AbStateIDX with strength).",
         {{"MoverUpgradeEffect.shn", true, true, nullptr, "Stufen (MoverUpgradeEffect)", "Levels (MoverUpgradeEffect)",
           {{"AbStateIDX", "AbState.shn", Source::Server, "InxName", "InxName", true}}}},
         {}},
        {"pup", "Pups", "Pups",
         "Ein Pup (Haustier) besteht aus PupMain (Client und Server: Nummer, Item, Tempo), PupView (Client: Name, Modell, Größe), "
         "PupServer (Server: Spanne für Laune und Stress) und PupCaseDesc (Server: Reaktionen mit Meldung aus ScriptMsg, Effekt und "
         "Sound). Zeilen mit PupIDX '-' gelten für alle Pups (vermutet).",
         "A pup (pet) consists of PupMain (client and server: number, item, speed), PupView (client: name, model, size), PupServer "
         "(server: range for mood and stress) and PupCaseDesc (server: reactions with a message from ScriptMsg, effect and sound). "
         "Rows with PupIDX '-' apply to all pups (assumed).",
         {{"PupMain.shn", true, true, "PupIDX", "Grundwerte (PupMain)", "Base values (PupMain)", {ItemName("ItemIDX")}, "PupID", "ItemIDX"},
          {"PupView.shn", true, false, "PupIDX", "Anzeige (PupView)", "Display (PupView)", {}},
          {"PupServer.shn", false, true, "PupIDX", "Laune und Stress (PupServer)", "Mood and stress (PupServer)", {}},
          {"PupCaseDesc.shn", false, true, "PupIDX", "Reaktionen (PupCaseDesc)", "Reactions (PupCaseDesc)",
           {{"SM_Inx", "ScriptMsg.shn", Source::Client, "SM_Inx", "SM_Msg", true}}}},
         {},
         {"PupView.shn", Source::Client, "PupIDX", "Name"}},
        {"pupai", "Pup-Verhalten", "Pup behavior",
         "Allgemeine Pup-Regeln (Server): PupCase (Fall je Laune und Stress), PupMind (Laune-Stufen), PupPriority (Vorrang der "
         "Reaktionen) und PupFactorCondition (was Laune/Stress ändert). Die genaue Wirkung ist nicht aus dem Server belegt (vermutet).",
         "General pup rules (server): PupCase (case per mood and stress), PupMind (mood levels), PupPriority (priority of reactions) "
         "and PupFactorCondition (what changes mood/stress). The exact effect is not confirmed from the server (assumed).",
         {{"PupCase.shn", false, true, nullptr, "Fälle (PupCase)", "Cases (PupCase)", {}},
          {"PupMind.shn", false, true, nullptr, "Laune-Stufen (PupMind)", "Mood levels (PupMind)", {}},
          {"PupPriority.shn", false, true, nullptr, "Vorrang (PupPriority)", "Priority (PupPriority)", {}},
          {"PupFactorCondition.shn", false, true, nullptr, "Einflüsse (PupFactorCondition)", "Factors (PupFactorCondition)", {}}},
         {}},
    };
    static const Category category{"mounts", "REITTIERE & HAUSTIERE", "MOUNTS & PETS",
                                   "Mover (Reittiere) und Pups (Haustiere)", "Movers (mounts) and pups (pets)",
                                   DrawIconSpawn, "module.custom_mob", 18, systems, &EditorState::mountSystems};
    return category;
}

} // namespace nge::groups

namespace nge {

void DrawMountSystemsEditor(EditorState& state) { groups::DrawGroupSystemsEditor(state, groups::MountCategory()); }

} // namespace nge
