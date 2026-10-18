// GuildSystems.cpp
// Gilde (Stufen, Akademie, Turnier), Kingdom Quest und Glücksspiel (Würfel, Spielhaus, Spielautomat) im
// Datei Editor (Unterreiter 20) - eine Kategorie des Gruppen-Editors (GroupSystems.hpp). Die meisten
// dieser Tabellen sind kleine Einstellungstabellen ohne Schlüssel; Kingdom Quests hängen an ihrer ID
// (KingdomQuest.ID = KQIsVote.ID = KQTeam.ID, RewardIndex -> KingdomQuestRew.ID).
// Spaltenbedeutungen aus NA2016 gelernt; Unsicheres ist im Handbuch als "vermutet" markiert.

#include "GroupSystems.hpp"

namespace nge::groups {
namespace {
Lookup StateName(const char* column) { return {column, "AbState.shn", Source::Server, "InxName", "InxName", true}; }
Lookup ItemNameById(const char* column) { return {column, "ItemInfo.shn", Source::Client, "ID", "Name", true}; }
} // namespace

const Category& GuildCategory() {
    static const std::vector<System> systems = {
        {"guildgrade", "Gilden-Stufen", "Guild grades",
         "Tabellen ohne Schlüssel (Server): GuildGradeData (je Gildenstufe benötigter Ruhm NeedFame, maximale Mitglieder, "
         "Mitglieder je Rang), GuildGradeScoreData und GuildLevelScoreData (Punkte für die Stufe, vermutet).",
         "Tables without key (server): GuildGradeData (per guild grade the required fame NeedFame, maximum members, members per "
         "rank), GuildGradeScoreData and GuildLevelScoreData (points for the grade, assumed).",
         {{"GuildGradeData.shn", false, true, nullptr, "Stufen (GuildGradeData)", "Grades (GuildGradeData)", {}},
          {"GuildGradeScoreData.shn", false, true, nullptr, "Stufen-Punkte (GuildGradeScoreData)", "Grade points (GuildGradeScoreData)", {}},
          {"GuildLevelScoreData.shn", false, true, nullptr, "Level-Punkte (GuildLevelScoreData)", "Level points (GuildLevelScoreData)", {}}},
         {}},
        {"academy", "Gilden-Akademie", "Guild academy",
         "Tabellen ohne Schlüssel (Server): GuildAcademy (allgemeiner Akademie-Buff, Mindest-Beitrittszeit, Auswertungszeit), "
         "GuildAcademyLevelUp (Punkte und Buff-Dauer je Level) und GuildAcademyRank (Ruhm und Buff je Rang). Buffs sind AbState-"
         "InxNames.",
         "Tables without key (server): GuildAcademy (general academy buff, minimum join time, ranking time), GuildAcademyLevelUp "
         "(points and buff duration per level) and GuildAcademyRank (fame and buff per rank). Buffs are AbState InxNames.",
         {{"GuildAcademy.shn", false, true, nullptr, "Akademie (GuildAcademy)", "Academy (GuildAcademy)", {StateName("BuffName")}},
          {"GuildAcademyLevelUp.shn", false, true, nullptr, "Level (GuildAcademyLevelUp)", "Levels (GuildAcademyLevelUp)", {}},
          {"GuildAcademyRank.shn", false, true, nullptr, "Ränge (GuildAcademyRank)", "Ranks (GuildAcademyRank)", {StateName("BuffName")}}},
         {}},
        {"tournament", "Gilden-Turnier", "Guild tournament",
         "Tabellen ohne Schlüssel: GuildTournament (Server: Termine, Dauer und Runden), GuildTournamentRequire (Client und Server: "
         "Mindest-Level, Mitglieder, Startgeld), GuildTournamentLvGap, GuildTournamentOccupy, GuildTournamentMasterBuff, "
         "GuildTournamentScore und GuildTournamentReward (Server: Belohnung je Rang; RewardType/Value, vermutet).",
         "Tables without key: GuildTournament (server: dates, duration and rounds), GuildTournamentRequire (client and server: "
         "minimum level, members, entry fee), GuildTournamentLvGap, GuildTournamentOccupy, GuildTournamentMasterBuff, "
         "GuildTournamentScore and GuildTournamentReward (server: reward per rank; RewardType/Value, assumed).",
         {{"GuildTournament.shn", false, true, nullptr, "Termine (GuildTournament)", "Schedule (GuildTournament)", {}},
          {"GuildTournamentRequire.shn", true, true, nullptr, "Voraussetzungen (GuildTournamentRequire)", "Requirements (GuildTournamentRequire)", {}},
          {"GuildTournamentLvGap.shn", false, true, nullptr, "Level-Abstand (GuildTournamentLvGap)", "Level gap (GuildTournamentLvGap)", {}},
          {"GuildTournamentOccupy.shn", false, true, nullptr, "Besetzen (GuildTournamentOccupy)", "Occupy (GuildTournamentOccupy)", {}},
          {"GuildTournamentMasterBuff.shn", false, true, nullptr, "Meister-Buff (GuildTournamentMasterBuff)", "Master buff (GuildTournamentMasterBuff)",
           {StateName("StateName")}},
          {"GuildTournamentScore.shn", false, true, nullptr, "Punkte (GuildTournamentScore)", "Score (GuildTournamentScore)", {}},
          {"GuildTournamentReward.shn", false, true, nullptr, "Belohnungen (GuildTournamentReward)", "Rewards (GuildTournamentReward)", {}}},
         {}},
        {"tournamentskills", "Turnier-Skills", "Tournament skills",
         "GuildTournamentSkill (Client und Server: Skill je Karte und Index - Kosten DeathPoint, Zustand StaName, Ziel, Abklingzeit) "
         "und GuildTournamentSkillDesc (Client: Symbol, Name und Beschreibung). Beide Tabellen ohne Schlüssel; Zeile n gehört zu "
         "Zeile n (vermutet über MAP_TYPE/Index).",
         "GuildTournamentSkill (client and server: skill per map and index - cost DeathPoint, state StaName, target, cooldown) and "
         "GuildTournamentSkillDesc (client: icon, name and description). Both tables without key; row n belongs to row n (assumed via "
         "MAP_TYPE/Index).",
         {{"GuildTournamentSkill.shn", true, true, nullptr, "Skills (GuildTournamentSkill)", "Skills (GuildTournamentSkill)", {StateName("StaName")}},
          {"GuildTournamentSkillDesc.shn", true, false, nullptr, "Beschreibung (GuildTournamentSkillDesc)", "Description (GuildTournamentSkillDesc)", {}}},
         {}},
        {"kq", "Kingdom Quests", "Kingdom quests",
         "Eine Kingdom Quest (Schlüssel ID): KingdomQuest (Server: Titel, Zeitlimit, Startzeit, Wiederholung, Level- und "
         "Spielerbereich, Wiederbelebungen, Belohnung RewardIndex, Karte MapLink, Skript), KQIsVote (Client und Server: Abstimmung "
         "erlaubt) und KQTeam (Client und Server: Team-Modus und Startpunkte Rot/Blau). Die Beschreibungstexte stehen im Text-Editor "
         "(KingdomQuestDesc).",
         "A kingdom quest (key ID): KingdomQuest (server: title, time limit, start time, repetition, level and player range, revivals, "
         "reward RewardIndex, map MapLink, script), KQIsVote (client and server: voting allowed) and KQTeam (client and server: team "
         "mode and start points red/blue). The description texts are in the text editor (KingdomQuestDesc).",
         {{"KingdomQuest.shn", false, true, "ID", "Quest (KingdomQuest)", "Quest (KingdomQuest)",
           {{"RewardIndex", "KingdomQuestRew.shn", Source::Server, "ID", "KQBoxItemIDX", true}}},
          {"KQIsVote.shn", true, true, "ID", "Abstimmung (KQIsVote)", "Voting (KQIsVote)", {}},
          {"KQTeam.shn", true, true, "ID", "Teams (KQTeam)", "Teams (KQTeam)", {}}},
         {},
         {"KingdomQuest.shn", Source::Server, "ID", "Title"}},
        {"kqrewards", "KQ-Belohnungen", "KQ rewards",
         "KingdomQuestRew (Server, Schlüssel ID): Belohnungskiste KQBoxItemIDX und die Belohnungs-Nummern mit ihren Raten "
         "(RewardRate ff., vermutet). Darunter die Kingdom Quests, die diese Belohnung nutzen.",
         "KingdomQuestRew (server, key ID): reward box KQBoxItemIDX and the reward numbers with their rates (RewardRate etc., "
         "assumed). Below are the kingdom quests using this reward.",
         {{"KingdomQuestRew.shn", false, true, "ID", "Belohnung (KingdomQuestRew)", "Reward (KingdomQuestRew)", {ItemName("KQBoxItemIDX")}}},
         {"KingdomQuest.shn", Source::Server, "RewardIndex", "Kingdom Quests mit dieser Belohnung", "Kingdom quests with this reward", "ID", "Title", "kq"},
         {"KingdomQuestRew.shn", Source::Server, "ID", "KQBoxItemIDX"}},
        {"kqsettings", "KQ-Einstellungen", "KQ settings",
         "Tabellen ohne Schlüssel: KingdomQuestMap (Server: Basiskarte und Instanzkarten; Zeile = KingdomQuest.MapLink, vermutet), "
         "KQItem (Server: Sonder-Items in Kingdom Quests), KQVoteDesc (Client: Abstimmungsgründe) und KQVoteMajorityRate (Client und "
         "Server: nötige Zustimmung).",
         "Tables without key: KingdomQuestMap (server: base map and instance maps; row = KingdomQuest.MapLink, assumed), KQItem "
         "(server: special items in kingdom quests), KQVoteDesc (client: voting reasons) and KQVoteMajorityRate (client and server: "
         "required agreement).",
         {{"KingdomQuestMap.shn", false, true, nullptr, "Karten (KingdomQuestMap)", "Maps (KingdomQuestMap)", {}},
          {"KQItem.shn", false, true, nullptr, "Sonder-Items (KQItem)", "Special items (KQItem)", {ItemName("ItemIndex")}},
          {"KQVoteDesc.shn", true, false, nullptr, "Abstimmungsgründe (KQVoteDesc)", "Voting reasons (KQVoteDesc)", {}},
          {"KQVoteMajorityRate.shn", true, true, nullptr, "Mehrheit (KQVoteMajorityRate)", "Majority (KQVoteMajorityRate)", {}}},
         {}},
        {"dice", "Würfelspiel", "Dice game",
         "Tabellen ohne Schlüssel: DiceGame (Server: Würfel-Item über die Item-ID, Mindest-Level, Gewinnanteile, Einsätze, Zeiten und "
         "Ereignis-Codes), DiceDividind (Client und Server: Auszahlungsfaktoren je Ergebnis), DiceRate (Server: 216 Würfelergebnisse, "
         "vermutet Gewichtung) und BelongDice (Server: Zeitlimit).",
         "Tables without key: DiceGame (server: dice item via the item ID, minimum level, profit shares, bets, times and event codes), "
         "DiceDividind (client and server: payout factors per result), DiceRate (server: 216 dice results, assumed weighting) and "
         "BelongDice (server: time limit).",
         {{"DiceGame.shn", false, true, nullptr, "Spiel (DiceGame)", "Game (DiceGame)", {ItemNameById("ItemID")}},
          {"DiceDividind.shn", true, true, nullptr, "Auszahlung (DiceDividind)", "Payout (DiceDividind)", {}},
          {"DiceRate.shn", false, true, nullptr, "Ergebnisse (DiceRate)", "Results (DiceRate)", {}},
          {"BelongDice.shn", false, true, nullptr, "Zeitlimit (BelongDice)", "Time limit (BelongDice)", {}}},
         {}},
        {"gamblehouse", "Spielhaus", "Gamble house",
         "Tabellen ohne Schlüssel für das Spielhaus (GB = Gamble): GBHouse (Client und Server: Spielgeld, Tauschsteuer, Rücksetzzeit), "
         "GBDiceGame/GBDiceDividind/GBDiceRate (Würfeltisch), GBBanTime, GBEventCode, GBExchangeMaxCoin (Münzen je Level), "
         "GBJoinGameMember, GBReward (Belohnungs-Item) und GBTaxRate (Steuer und Jackpot-Anteil).",
         "Tables without key for the gamble house (GB = gamble): GBHouse (client and server: game money, exchange tax, reset time), "
         "GBDiceGame/GBDiceDividind/GBDiceRate (dice table), GBBanTime, GBEventCode, GBExchangeMaxCoin (coins per level), "
         "GBJoinGameMember, GBReward (reward item) and GBTaxRate (tax and jackpot share).",
         {{"GBHouse.shn", true, true, nullptr, "Spielhaus (GBHouse)", "Gamble house (GBHouse)", {}},
          {"GBDiceGame.shn", false, true, nullptr, "Würfeltisch (GBDiceGame)", "Dice table (GBDiceGame)", {}},
          {"GBDiceDividind.shn", true, true, nullptr, "Würfel-Auszahlung (GBDiceDividind)", "Dice payout (GBDiceDividind)", {}},
          {"GBDiceRate.shn", false, true, nullptr, "Würfel-Ergebnisse (GBDiceRate)", "Dice results (GBDiceRate)", {}},
          {"GBBanTime.shn", false, true, nullptr, "Sperrzeit (GBBanTime)", "Ban time (GBBanTime)", {}},
          {"GBEventCode.shn", false, true, nullptr, "Ereignis-Codes (GBEventCode)", "Event codes (GBEventCode)", {}},
          {"GBExchangeMaxCoin.shn", false, true, nullptr, "Münzen je Level (GBExchangeMaxCoin)", "Coins per level (GBExchangeMaxCoin)", {}},
          {"GBJoinGameMember.shn", false, true, nullptr, "Spielerzahl (GBJoinGameMember)", "Player count (GBJoinGameMember)", {}},
          {"GBReward.shn", false, true, nullptr, "Belohnung (GBReward)", "Reward (GBReward)", {ItemName("Item_INX")}},
          {"GBTaxRate.shn", false, true, nullptr, "Steuer (GBTaxRate)", "Tax (GBTaxRate)", {}}},
         {}},
        {"slotmachine", "Spielautomat", "Slot machine",
         "Tabellen ohne Schlüssel für den Spielautomaten (GBSM): GBSMNPC (Automaten-NPCs und ihr Einsatz), GBSMBetCoin (Einsatz -> "
         "Münzen), GBSMGroup/GBSMCardRate (Symbole und ihre Raten), GBSMAll/GBSMLine/GBSMCenter (Gewinnfaktoren) und GBSMJPRate "
         "(Jackpot). Die genaue Rechnung ist nicht aus dem Server belegt (vermutet).",
         "Tables without key for the slot machine (GBSM): GBSMNPC (machine NPCs and their bet), GBSMBetCoin (bet -> coins), "
         "GBSMGroup/GBSMCardRate (symbols and their rates), GBSMAll/GBSMLine/GBSMCenter (win factors) and GBSMJPRate (jackpot). The "
         "exact calculation is not confirmed from the server (assumed).",
         {{"GBSMNPC.shn", false, true, nullptr, "Automaten (GBSMNPC)", "Machines (GBSMNPC)",
           {{"GBSM_MobInx", "MobInfo.shn", Source::Client, "InxName", "Name", true}}},
          {"GBSMBetCoin.shn", false, true, nullptr, "Einsatz (GBSMBetCoin)", "Bet (GBSMBetCoin)", {}},
          {"GBSMGroup.shn", false, true, nullptr, "Symbol-Gruppen (GBSMGroup)", "Symbol groups (GBSMGroup)", {}},
          {"GBSMCardRate.shn", false, true, nullptr, "Symbol-Raten (GBSMCardRate)", "Symbol rates (GBSMCardRate)", {}},
          {"GBSMAll.shn", false, true, nullptr, "Alle gleich (GBSMAll)", "All equal (GBSMAll)", {}},
          {"GBSMLine.shn", false, true, nullptr, "Linie (GBSMLine)", "Line (GBSMLine)", {}},
          {"GBSMCenter.shn", false, true, nullptr, "Mitte (GBSMCenter)", "Center (GBSMCenter)", {}},
          {"GBSMJPRate.shn", false, true, nullptr, "Jackpot (GBSMJPRate)", "Jackpot (GBSMJPRate)", {}}},
         {}},
    };
    static const Category category{"guild", "GILDE, KQ & GLÜCKSSPIEL", "GUILD, KQ & GAMBLING",
                                   "Gilden, Kingdom Quests, Würfel, Spielhaus und Automat", "Guilds, kingdom quests, dice, gamble house and slots",
                                   DrawIconBook, "module.quest", 20, systems, &EditorState::guildSystems};
    return category;
}

} // namespace nge::groups

namespace nge {

void DrawGuildSystemsEditor(EditorState& state) { groups::DrawGroupSystemsEditor(state, groups::GuildCategory()); }

} // namespace nge
