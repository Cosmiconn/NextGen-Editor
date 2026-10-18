// ItemMobEditors.cpp
// Item-Editor (ItemInfo, ItemInfoServer, ItemViewInfo) und Mob-Editor (MobInfo, MobInfoServer,
// MobViewInfo) auf Basis des allgemeinen Formular-Editors. Felder ohne eigenen Tooltip nutzen die
// Spaltenreferenz des Handbuchs; Bedeutungen, die nur aus Namen und Wertebereichen abgeleitet sind,
// sind als "(vermutet)" markiert.

#include "EditorShared.hpp"

namespace nge {

namespace {
using K = dataform::Kind;
using F = dataform::Field;
const ImVec4 kRefDim(0.62f, 0.66f, 0.72f, 1.0f);

F Fld(int table, const char* col, K kind, const char* de, const char* en, const char* tipDe = nullptr, const char* tipEn = nullptr) {
    F f;
    f.table = table; f.col = col; f.kind = kind;
    f.labelDe = de; f.labelEn = en; f.tipDe = tipDe; f.tipEn = tipEn;
    return f;
}

// Doppelte InxNames und fehlende Kopien - für Item und Mob gleich.
void FamilyProblems(EditorState& state, const dataform::Spec& spec, const dataform::Docs& docs, long long id,
                    std::vector<std::string>& out) {
    const int mainDoc = docs.Main();
    const long long row = FormRowIn(state, mainDoc, id);
    if (row < 0) return;
    const auto& mf = ShnDocFile(state, mainDoc);
    const std::string inx = TrimAscii(ShnCellText(mf, static_cast<std::size_t>(row), "InxName"));
    // Index InxName -> Anzahl, neu aufgebaut nur bei Änderungen.
    static std::unordered_map<std::string, int> counts;
    static std::string countsKey;
    const std::string key = std::to_string(mainDoc) + "|" + std::to_string(state.shnEditCounter) + "|" + std::to_string(mf.rows.size());
    if (key != countsKey) {
        countsKey = key;
        counts.clear();
        for (std::size_t r = 0; r < mf.rows.size(); ++r) ++counts[TrimAscii(ShnCellText(mf, r, "InxName"))];
    }
    if (counts[inx] > 1)
        out.push_back(std::string(L("InxName kommt mehrfach vor: ", "InxName occurs more than once: ")) + inx);
    for (std::size_t t = 0; t < docs.byTable.size(); ++t)
        for (const int d : docs.byTable[t])
            if (FormRowIn(state, d, id) < 0)
                out.push_back(std::string(L("Zeile fehlt in ", "Row missing in ")) + ShnSourceName(state.shnFiles[static_cast<std::size_t>(d)].source) +
                              " " + spec.tables[t].file);
}

// Modell-Vorschau (NIF-Thumbnail) für einen Pfad relativ zum Client-Ordner.
bool DrawModelThumbnail(EditorState& state, const std::string& relative, float size) {
    if (relative.empty()) return false;
    const auto path = std::filesystem::path(state.project.clientFolder) / relative;
    const auto thumb = GetOrLoadAssetThumbnail(state, path, true, state.resmapRootForThumbnails);
    if (thumb.tex == 0) return false;
    ImGui::Image(static_cast<ImTextureID>(thumb.tex), ImVec2(size * thumb.aspect, size));
    return true;
}
// Mob-Modelle: FindNpcModelPath durchsucht Ordner - Ergebnis je FileName merken.
std::string CachedNpcModelPath(EditorState& state, const std::string& fileName) {
    static std::unordered_map<std::string, std::string> cache;
    static std::string cacheRoot;
    if (cacheRoot != state.project.clientFolder) { cache.clear(); cacheRoot = state.project.clientFolder; }
    if (const auto it = cache.find(fileName); it != cache.end()) return it->second;
    return cache[fileName] = fileName.empty() || fileName == "-" ? std::string() : FindNpcModelPath(state, fileName);
}
// Ohne reschar-Ordner im Client lässt sich ein Mob-Modell nicht prüfen (keine Fehlmeldung).
bool HasRescharFolder(EditorState& state) {
    if (!state.rescharRoot.empty()) return true;
    static std::string checkedFor;
    static bool found = false;
    if (checkedFor != state.project.clientFolder) {
        checkedFor = state.project.clientFolder;
        found = !checkedFor.empty() && FindChildCaseInsensitive(std::filesystem::path(checkedFor), "reschar").has_value();
    }
    return found;
}
std::optional<std::filesystem::path> FindItemModel(EditorState& state, const std::string& linkFile) {
    if (linkFile.empty() || linkFile == "-") return std::nullopt;
    const auto resitem = FindChildCaseInsensitive(std::filesystem::path(state.project.clientFolder), "resitem");
    if (!resitem) return std::nullopt;
    return FindChildCaseInsensitive(*resitem, linkFile + ".nif");
}

void DrawItemHeader(EditorState& state, const dataform::Docs& docs, long long id) {
    auto& form = state.itemForm;
    const std::string iconFile = TrimAscii(FormCellRead(state, docs, 2, "IconFile", id));
    int iconIndex = std::atoi(FormCellRead(state, docs, 2, "IconIndex", id).c_str());
    const bool iconOk = DrawAtlasIcon(state, iconFile, iconIndex, 48.0f);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Icon: %s #%d", iconFile.c_str(), iconIndex);
    if (!iconOk && !iconFile.empty() && iconFile != "-")
        ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f), "%s", L("Icon nicht im Atlas gefunden (resmenu/Icon)", "Icon not found in the atlas (resmenu/Icon)"));
    if (UI::SmallButton(form.iconPickerOpen ? L("Icon-Auswahl schließen", "Close icon picker") : L("Icon wählen...", "Choose icon...")))
        form.iconPickerOpen = !form.iconPickerOpen;
    if (!iconFile.empty() && iconFile != "-") {
        ImGui::SameLine();
        if (UI::SmallButton(L("Im Atlas-Editor##item", "In atlas editor##item"))) OpenIconInAtlasEditor(state, iconFile, iconIndex);
    }
    ImGui::EndGroup();
    if (form.iconPickerOpen && !iconFile.empty() && DrawIconAtlasPicker(state, iconFile, iconIndex))
        FormCellWrite(state, docs, 2, "IconIndex", id, std::to_string(iconIndex), "Item #" + std::to_string(id) + " · IconIndex");
    // Modell einer Waffe/eines Schilds (resitem/<LinkFile>.nif).
    const std::string link = TrimAscii(FormCellRead(state, docs, 2, "LinkFile", id));
    if (const auto model = FindItemModel(state, link)) {
        std::error_code ec;
        const auto rel = std::filesystem::relative(*model, std::filesystem::path(state.project.clientFolder), ec);
        ImGui::SameLine();
        if (!DrawModelThumbnail(state, ec ? model->string() : rel.string(), 96.0f)) ImGui::TextDisabled("%s", link.c_str());
        else if (ImGui::IsItemHovered()) ImGui::SetTooltip("resitem/%s.nif", link.c_str());
    }
    // Verwendung (auf Knopfdruck, weil alle geladenen Tabellen durchsucht werden).
    const std::string usageKey = std::to_string(id) + "|" + std::to_string(state.shnEditCounter) + "|" + std::to_string(state.dataHistory.Revision());
    if (UI::SmallButton(L("Verwendung suchen##item", "Find usage##item"))) {
        form.usageKey = usageKey;
        form.usageLines.clear();
        const long long row = FormRowIn(state, docs.Main(), id);
        if (row >= 0) form.usageLines = FindShnRowReferences(state, docs.Main(), static_cast<int>(row), 40);
        EnsureDropGroupCatalog(state);
        if (const auto* t = state.dropGroupLoaded ? state.dropGroupFile.FindTable("ItemDropGroup") : nullptr) {
            const auto cols = core::legacy::FindDropGroupColumns(*t);
            const std::string inx = TrimAscii(FormCellRead(state, docs, 0, "InxName", id));
            const std::string ga = TrimAscii(FormCellRead(state, docs, 1, "DropGroupA", id));
            const std::string gb = TrimAscii(FormCellRead(state, docs, 1, "DropGroupB", id));
            for (const auto& r : t->records) {
                const std::string item = ShineRecordValue(r, cols.item);
                if (item == inx || (!core::legacy::DropGroupCatalog::IsEmptySlot(ga) && item == ga) ||
                    (!core::legacy::DropGroupCatalog::IsEmptySlot(gb) && item == gb))
                    form.usageLines.push_back("ItemDropGroup · " + ShineRecordValue(r, cols.group) + " (ItemID " + item + ")");
            }
        }
        if (form.usageLines.empty()) form.usageLines.push_back(L("Keine Verweise in den geladenen Tabellen.", "No references in the loaded tables."));
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", L("Sucht Verweise auf dieses Item in allen geladenen SHN-Tabellen und in den Drop-Gruppen.",
                                  "Searches references to this item in every loaded SHN table and in the drop groups."));
    if (form.usageKey == usageKey)
        for (const auto& line : form.usageLines) ImGui::TextColored(kRefDim, "  %s", line.c_str());
    ImGui::Separator();
}

std::vector<std::string> ItemProblems(EditorState& state, const dataform::Docs& docs, long long id) {
    std::vector<std::string> out;
    FamilyProblems(state, ItemFormSpec(), docs, id, out);
    const long long buy = std::atoll(FormCellRead(state, docs, 0, "BuyPrice", id).c_str());
    const long long sell = std::atoll(FormCellRead(state, docs, 0, "SellPrice", id).c_str());
    if (buy > 0 && sell > buy)
        out.push_back(L("Verkaufspreis ist höher als der Kaufpreis (Gewinn durch Kaufen und Verkaufen).",
                        "Sell price is higher than the buy price (profit by buying and selling)."));
    const std::string minWc = FormCellRead(state, docs, 0, "MinWC", id), maxWc = FormCellRead(state, docs, 0, "MaxWC", id);
    if (std::atoll(minWc.c_str()) > std::atoll(maxWc.c_str())) out.push_back("MinWC > MaxWC");
    const std::string minMa = FormCellRead(state, docs, 0, "MinMA", id), maxMa = FormCellRead(state, docs, 0, "MaxMA", id);
    if (std::atoll(minMa.c_str()) > std::atoll(maxMa.c_str())) out.push_back("MinMA > MaxMA");
    return out;
}

void DrawMobHeader(EditorState& state, const dataform::Docs& docs, long long id) {
    auto& form = state.mobForm;
    const std::string inx = TrimAscii(FormCellRead(state, docs, 0, "InxName", id));
    const std::string fileName = TrimAscii(FormCellRead(state, docs, 2, "FileName", id));
    const long long npcView = std::atoll(FormCellRead(state, docs, 2, "NpcViewIndex", id).c_str());
    const std::string model = CachedNpcModelPath(state, fileName);
    if (!DrawModelThumbnail(state, model, 128.0f)) {
        ImGui::Dummy(ImVec2(128.0f, 128.0f));
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(80, 92, 110, 160), 3.0f);
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (npcView != 0) ImGui::TextDisabled(L("Aussehen über NPCViewInfo #%lld (Spieler-Modell)", "Look via NPCViewInfo #%lld (player model)"), npcView);
    else if (model.empty() && !HasRescharFolder(state))
        ImGui::TextDisabled(L("Modell '%s' - kein reschar-Ordner im Client, Vorschau nicht möglich", "Model '%s' - no reschar folder in the client, no preview"), fileName.c_str());
    else if (model.empty()) ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f), L("Modell '%s' nicht gefunden", "Model '%s' not found"), fileName.c_str());
    else ImGui::TextDisabled("%s", model.c_str());

    // Drops: Zeilen der Mob-Drop-Tabelle für diesen Mob.
    EnsureDropTableLoaded(state);
    if (const auto* dt = state.dropTableLoaded ? state.dropTableFile.FindTable("ItemGroup") : nullptr) {
        const int cMob = FindShineColumn(*dt, "MobId");
        int shown = 0;
        for (std::size_t r = 0; r < dt->records.size(); ++r) {
            if (ShineRecordValue(dt->records[r], cMob) != inx) continue;
            ImGui::PushID(static_cast<int>(r));
            if (shown++ == 0) ImGui::TextUnformatted(L("Drops:", "Drops:"));
            ImGui::SameLine();
            if (UI::SmallButton((std::string(L("Drop-Zeile ", "Drop row ")) + std::to_string(r)).c_str())) {
                state.dropTableSelectedRecord = static_cast<int>(r);
                state.dropEditorTabRequest = 0;
                state.shnSubTab = 10;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", L("Diese Zeile im Drop-Editor öffnen.", "Open this row in the drop editor."));
            ImGui::PopID();
        }
        if (shown == 0) ImGui::TextDisabled("%s", L("Keine Zeile in ItemDropTable.txt (droppt nichts).", "No row in ItemDropTable.txt (drops nothing)."));
    }
    // Angriffsfolge (MobAttackSequence/<InxName>.txt).
    if (!inx.empty()) {
        if (AttackSequenceExists(state, inx)) {
            if (UI::SmallButton(L("Angriffsfolge öffnen##mobAttackSeq", "Open attack sequence##mobAttackSeq"))) OpenAttackSequence(state, inx);
        } else {
            ImGui::TextDisabled("%s", L("Keine Angriffsfolge (MobAttackSequence).", "No attack sequence (MobAttackSequence)."));
        }
    }
    // Platzierung (World/NPC.txt) und Gespräch (NpcDialogData).
    if (const auto* npc = FindQuestNpc(state, static_cast<int>(id)); npc && !npc->placements.empty())
        ImGui::TextDisabled(L("Platziert: %s", "Placed: %s"), QuestNpcPlacementText(*npc).c_str());
    if (FindDialogRowForNpc(state, inx) >= 0) {
        ImGui::TextDisabled("%s", L("Hat ein NPC-Gespräch.", "Has an NPC conversation."));
        ImGui::SameLine();
        if (UI::SmallButton(L("Gespräch öffnen", "Open conversation"))) OpenNpcDialogEditor(state, inx);
    }
    // Spawns: alle MobRegen-Dateien (auf Knopfdruck).
    const std::string usageKey = inx + "|" + state.shineTextRoot;
    if (UI::SmallButton(L("Spawns suchen", "Find spawns"))) {
        form.usageKey = usageKey;
        form.usageLines.clear();
        std::error_code ec;
        const auto dir = std::filesystem::path(state.shineTextRoot) / "MobRegen";
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
            if (e.path().extension() != ".txt") continue;
            const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, e.path());
            auto file = core::legacy::LoadShineTextFile(path);
            if (!file) continue;
            int count = 0;
            for (const auto& t : file->tables)
                for (const auto& rec : t.records)
                    count += static_cast<int>(std::count(rec.values.begin(), rec.values.end(), inx));
            if (count > 0) form.usageLines.push_back(e.path().stem().string() + ": " + std::to_string(count) + L(" Eintrag/Einträge", " entry/entries"));
        }
        if (form.usageLines.empty()) form.usageLines.push_back(L("In keiner MobRegen-Datei gefunden.", "Not found in any MobRegen file."));
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", L("Durchsucht alle MobRegen/<Karte>.txt nach diesem Mob (Spawns je Karte).",
                                  "Searches every MobRegen/<map>.txt for this mob (spawns per map)."));
    if (form.usageKey == usageKey)
        for (const auto& line : form.usageLines) ImGui::TextColored(kRefDim, "  %s", line.c_str());
    ImGui::EndGroup();
    ImGui::Separator();
}

std::vector<std::string> MobProblems(EditorState& state, const dataform::Docs& docs, long long id) {
    std::vector<std::string> out;
    FamilyProblems(state, MobFormSpec(), docs, id, out);
    const long long npcView = std::atoll(FormCellRead(state, docs, 2, "NpcViewIndex", id).c_str());
    const std::string fileName = TrimAscii(FormCellRead(state, docs, 2, "FileName", id));
    if (npcView == 0 && !fileName.empty() && fileName != "-" && HasRescharFolder(state) &&
        CachedNpcModelPath(state, fileName).empty())
        out.push_back(std::string(L("Modell nicht gefunden: ", "Model not found: ")) + fileName);
    return out;
}
} // namespace

dataform::Spec& ItemFormSpec() {
    static dataform::Spec spec = [] {
        dataform::Spec s;
        s.id = "Item";
        s.tables = {{"ItemInfo.shn", true, true}, {"ItemInfoServer.shn", false, true}, {"ItemViewInfo.shn", true, false}};
        s.sections = {
            {"Allgemein", "General", {
                Fld(0, "Name", K::Text, "Name", "Name"),
                Fld(0, "Type", K::Int, "Typ", "Type", "Item-Art (Aufzählung, 0-6; Wert aus ähnlichem Item übernehmen) (vermutet).", "Item kind (enum, 0-6; copy from a similar item) (assumed)."),
                Fld(0, "Class", K::Int, "Klasse", "Class", "Item-Klasse/Kategorie (Aufzählung; Wert aus ähnlichem Item übernehmen) (vermutet).", "Item class/category (enum; copy from a similar item) (assumed)."),
                Fld(0, "Equip", K::Int, "Ausrüstungs-Slot", "Equipment slot"),
                Fld(0, "ItemGradeType", K::Int, "Qualitätsstufe", "Grade type", "Seltenheit/Qualität (0-6) (vermutet).", "Rarity/quality (0-6) (assumed)."),
                Fld(0, "DemandLv", K::Int, "Benötigtes Level", "Required level"),
                Fld(0, "Grade", K::Int, "Grad", "Grade", "Stufe des Items innerhalb seiner Reihe (vermutet).", "Level of the item within its series (assumed)."),
                Fld(0, "MaxLot", K::Int, "Stapelgröße", "Stack size"),
                Fld(0, "UseClass", K::Int, "Klassen (Bitmaske)", "Classes (bitmask)"),
                Fld(0, "TwoHand", K::Int, "Zweihändig", "Two-handed"),
                Fld(0, "WeaponType", K::Int, "Waffenart", "Weapon type", "Waffenart (Aufzählung, 0 = keine Waffe) (vermutet).", "Weapon kind (enum, 0 = not a weapon) (assumed)."),
                Fld(0, "ArmorType", K::Int, "Rüstungsart", "Armor type", "Rüstungsart (Aufzählung, 0 = keine Rüstung) (vermutet).", "Armor kind (enum, 0 = not armor) (assumed)."),
                Fld(0, "ItemUseSkill", K::Values, "Benutzungs-Skill", "Use skill", "Skill, der beim Benutzen ausgelöst wird (vermutet).", "Skill triggered on use (assumed)."),
                Fld(0, "SetItemIndex", K::Int, "Set-Nummer", "Set index", "Zugehöriges Ausrüstungsset (vermutet).", "Equipment set it belongs to (assumed)."),
                Fld(0, "TitleName", K::Text, "Titel", "Title", "Titel, den das Item verleiht (vermutet).", "Title granted by the item (assumed)."),
            }, true},
            {"Handel", "Trade", {
                Fld(0, "BuyPrice", K::Int, "Kaufpreis", "Buy price"),
                Fld(0, "SellPrice", K::Int, "Verkaufspreis", "Sell price"),
                Fld(0, "BuyDemandLv", K::Int, "Kauf ab Level", "Buy from level", "Level, ab dem das Item gekauft werden kann (vermutet).", "Level from which the item can be bought (assumed)."),
                Fld(0, "BuyFame", K::Int, "Kauf: Ruhm", "Buy: fame", "Benötigter Ruhm beim Kauf (vermutet).", "Fame needed to buy (assumed)."),
                Fld(0, "BuyGToken", K::Int, "Kauf: Gilden-Token", "Buy: guild tokens", "Preis in Gilden-Token (vermutet).", "Price in guild tokens (assumed)."),
                Fld(0, "BuyGBCoin", K::Int, "Kauf: GB-Münzen", "Buy: GB coins", "Preis in Spielhallen-Münzen (vermutet).", "Price in game hall coins (assumed)."),
                Fld(1, "MarketIndex", K::Values, "Markt-Kategorie", "Market category", "Kategorie des Items: 'Skill' = Skillbuch, 'Weapon', 'Item', 'WeaponTitle'.", "Category of the item: 'Skill' = skill book, 'Weapon', 'Item', 'WeaponTitle'."),
                Fld(1, "ItemSort_Index", K::Values, "Sortier-Kategorie", "Sort category", "Kategorie zum Sortieren im Inventar (z. B. IS_QUEST, IS_SKILLSCROLL).", "Category for sorting in the inventory (e.g. IS_QUEST, IS_SKILLSCROLL)."),
                Fld(1, "City", K::Int, "Stadt-Item", "City item", "1 = Stadt-Item (vermutet).", "1 = city item (assumed)."),
            }, true},
            {"Kampfwerte", "Combat values", {
                Fld(0, "MinWC", K::Int, "Phys. Schaden min", "Phys. damage min"), Fld(0, "MaxWC", K::Int, "Phys. Schaden max", "Phys. damage max"),
                Fld(0, "MinMA", K::Int, "Mag. Schaden min", "Magic damage min"), Fld(0, "MaxMA", K::Int, "Mag. Schaden max", "Magic damage max"),
                Fld(0, "AC", K::Int, "Rüstung (AC)", "Armor (AC)"), Fld(0, "MR", K::Int, "Magieresistenz (MR)", "Magic resist (MR)"),
                Fld(0, "TH", K::Int, "Trefferwert (TH)", "Hit (TH)", "Trefferwert (vermutet).", "Hit value (assumed)."),
                Fld(0, "TB", K::Int, "Ausweichen (TB)", "Evade (TB)", "Ausweich-/Blockwert (vermutet).", "Evade/block value (assumed)."),
                Fld(0, "AtkSpeed", K::Int, "Angriffstempo", "Attack speed"),
                Fld(0, "CriRate", K::Int, "Krit. Chance", "Crit chance", "Chance auf kritischen Treffer (vermutet).", "Chance of a critical hit (assumed)."),
                Fld(0, "ShieldAC", K::Int, "Schild-AC", "Shield AC", "Rüstungswert des Schilds (vermutet).", "Armor value of the shield (assumed)."),
                Fld(0, "HitRatePlus", K::Int, "Treffer +", "Hit +", "Bonus auf die Trefferchance (vermutet).", "Bonus to hit chance (assumed)."),
                Fld(0, "EvaRatePlus", K::Int, "Ausweichen +", "Evade +", "Bonus auf Ausweichen (vermutet).", "Bonus to evasion (assumed)."),
            }, false},
            {"Verbesserung", "Enhancement", {
                Fld(0, "UpLimit", K::Int, "Max. Verbesserung", "Max enhancement", "Höchste Verbesserungsstufe (+N) (vermutet).", "Highest enhancement level (+N) (assumed)."),
                Fld(0, "BasicUpInx", K::Int, "Grund-Verbesserung", "Base enhancement", "Verweis auf die Verbesserungswerte (vermutet).", "Reference to the enhancement values (assumed)."),
                Fld(0, "UpSucRatio", K::Int, "Erfolgsrate", "Success ratio", "Erfolgschance beim Verbessern (vermutet).", "Success chance when enhancing (assumed)."),
                Fld(0, "UpLuckRatio", K::Int, "Glücksrate", "Luck ratio", "Glücksanteil beim Verbessern (vermutet).", "Luck share when enhancing (assumed)."),
                Fld(0, "UpResource", K::Int, "Verbesserungs-Material", "Enhancement material", "Material zum Verbessern (vermutet).", "Material used to enhance (assumed)."),
            }, false},
            {"Drops", "Drops", {
                Fld(1, "DropGroupA", K::DropGroupRef, "Drop-Gruppe A", "Drop group A", "Name einer Item-Menge: eine Zeile in ItemDropGroup.txt mit diesem Namen als ItemID droppt eines der Items mit diesem Namen.", "Name of an item set: a row in ItemDropGroup.txt with this name as ItemID drops one of the items with this name."),
                Fld(1, "DropGroupB", K::DropGroupRef, "Drop-Gruppe B", "Drop group B", "Zweite Item-Menge (wie Drop-Gruppe A).", "Second item set (like drop group A)."),
                Fld(1, "RandomOptionDropGroup", K::Values, "Zufallsoptionen", "Random options", "Gruppe der Zufallsoptionen beim Drop (vermutet).", "Group of random options on drop (assumed)."),
                Fld(1, "Vanish", K::Int, "Verschwindet nach (ms)", "Vanishes after (ms)", "Zeit, bis ein gedropptes Item am Boden verschwindet (60000/300000 in den Daten) (vermutet).", "Time until a dropped item vanishes from the ground (60000/300000 in the data) (assumed)."),
                Fld(1, "looting", K::Int, "Plünder-Schutz (ms)", "Loot protection (ms)", "Zeit, in der nur der Berechtigte aufheben darf (vermutet).", "Time in which only the owner may pick up (assumed)."),
                Fld(1, "DropRateKilledByMob", K::Int, "Droprate (von Mob getötet)", "Drop rate (killed by mob)", "Drop-Chance, wenn ein Mob den Träger tötet (vermutet).", "Drop chance when a mob kills the carrier (assumed)."),
                Fld(1, "DropRateKilledByPlayer", K::Int, "Droprate (von Spieler getötet)", "Drop rate (killed by player)", "Drop-Chance, wenn ein Spieler den Träger tötet (vermutet).", "Drop chance when a player kills the carrier (assumed)."),
            }, false},
            {"Darstellung", "Presentation", {
                Fld(2, "IconFile", K::Values, "Icon-Datei", "Icon file"), Fld(2, "IconIndex", K::Int, "Icon-Nummer", "Icon index"),
                Fld(2, "LinkFile", K::Values, "Modell (Waffe/Schild)", "Model (weapon/shield)"),
                Fld(2, "TextureFile", K::Values, "Rüstungstextur", "Armor texture"),
                Fld(2, "MSetNo", K::Int, "Rüstungsmodell (m)", "Armor model (m)"), Fld(2, "FSetNo", K::Int, "Rüstungsmodell (w)", "Armor model (f)"),
                Fld(2, "EquipType", K::Int, "Darstellungs-Art", "Display kind"),
                Fld(2, "R", K::Int, "Farbe R", "Colour R", "Rotanteil der Item-Farbe (0-255) (vermutet).", "Red part of the item colour (0-255) (assumed)."),
                Fld(2, "G", K::Int, "Farbe G", "Colour G", "Grünanteil (vermutet).", "Green part (assumed)."),
                Fld(2, "B", K::Int, "Farbe B", "Colour B", "Blauanteil (vermutet).", "Blue part (assumed)."),
                Fld(2, "UpEffect", K::Values, "Verbesserungs-Effekt", "Enhancement effect", "Effekt verbesserter Waffen (vermutet).", "Effect of enhanced weapons (assumed)."),
                Fld(2, "DropSnd", K::Values, "Sound: Fallen", "Sound: drop", "Sound beim Fallenlassen (vermutet).", "Sound when dropped (assumed)."),
                Fld(2, "EquSnd", K::Values, "Sound: Anlegen", "Sound: equip", "Sound beim Ausrüsten (vermutet).", "Sound when equipped (assumed)."),
                Fld(2, "Descript", K::Text, "Beschreibung", "Description"),
            }, false},
        };
        return s;
    }();
    return spec;
}

dataform::Spec& MobFormSpec() {
    static dataform::Spec spec = [] {
        dataform::Spec s;
        s.id = "Mob";
        s.tables = {{"MobInfo.shn", true, true}, {"MobInfoServer.shn", false, true}, {"MobViewInfo.shn", true, false}};
        F isNpc = Fld(0, "IsNPC", K::Enum, "Art", "Kind");
        isNpc.options = {{0, "Monster", "Monster"}, {1, "NPC", "NPC"}};
        s.sections = {
            {"Allgemein", "General", {
                Fld(0, "Name", K::Text, "Name", "Name"), isNpc,
                Fld(0, "Level", K::Int, "Level", "Level"), Fld(0, "MaxHP", K::Int, "Max. HP", "Max HP"),
                Fld(0, "Type", K::Int, "Typ", "Type", "Mob-Art (Aufzählung; Wert aus ähnlichem Mob übernehmen) (vermutet).", "Mob kind (enum; copy from a similar mob) (assumed)."),
                Fld(0, "GradeType", K::Int, "Rang-Art", "Grade type"),
                Fld(0, "Size", K::Int, "Größe", "Size"), Fld(0, "AbsoluteSize", K::Int, "Absolute Größe", "Absolute size"),
                Fld(0, "WalkSpeed", K::Int, "Gehtempo", "Walk speed"), Fld(0, "RunSpeed", K::Int, "Lauftempo", "Run speed"),
                Fld(0, "WeaponType", K::Int, "Waffenart", "Weapon type"), Fld(0, "ArmorType", K::Int, "Rüstungsart", "Armor type"),
                Fld(0, "IsPlayerSide", K::Int, "Spielerseite", "Player side"),
            }, true},
            {"Kampf (Server)", "Combat (server)", {
                Fld(1, "MonEXP", K::Int, "Erfahrung", "Experience"), Fld(1, "EXPRange", K::Int, "Erfahrungs-Streuung", "Experience range"),
                Fld(1, "AC", K::Int, "Rüstung (AC)", "Armor (AC)"), Fld(1, "MR", K::Int, "Magieresistenz (MR)", "Magic resist (MR)"),
                Fld(1, "TB", K::Int, "Ausweichen (TB)", "Evade (TB)"), Fld(1, "MB", K::Int, "Magieblock (MB)", "Magic block (MB)"),
                Fld(1, "Str", K::Int, "Stärke", "Strength"), Fld(1, "Dex", K::Int, "Geschick", "Dexterity"), Fld(1, "Con", K::Int, "Konstitution", "Constitution"),
                Fld(1, "Int", K::Int, "Intelligenz", "Intelligence"), Fld(1, "Men", K::Int, "Mentalität", "Mentality"),
                Fld(1, "MaxSP", K::Int, "Max. SP", "Max SP"), Fld(1, "Rank", K::Int, "Rang", "Rank"),
                Fld(1, "MobRaceType", K::Int, "Rasse", "Race", "Rassen-Art des Mobs (Aufzählung) (vermutet).", "Race kind of the mob (enum) (assumed)."),
                Fld(1, "MobKillInx", K::Values, "Kill-Zähler", "Kill counter"),
            }, true},
            {"Verhalten (Server)", "Behaviour (server)", {
                Fld(1, "Visible", K::Int, "Sichtbar", "Visible"), Fld(1, "EnemyDetectType", K::Int, "Gegnererkennung", "Enemy detection"),
                Fld(1, "DetectCha", K::Int, "Erkennungsweite", "Detection range"),
                Fld(1, "FollowCha", K::Int, "Verfolgungsweite", "Follow range", "Wie weit der Mob ein Ziel verfolgt (vermutet).", "How far the mob follows a target (assumed)."),
                Fld(1, "IsRoaming", K::Int, "Wandert", "Roams"), Fld(1, "RoamingNumber", K::Int, "Wanderziele", "Roaming targets"),
                Fld(1, "RoamingDistance", K::Int, "Wanderweite", "Roaming distance"), Fld(1, "RoamingRestTime", K::Int, "Wander-Pause", "Roaming rest"),
                Fld(1, "Return2Regen", K::Int, "Kehrt zum Spawn zurück", "Returns to spawn", "1 = läuft nach dem Kampf zum Spawnpunkt zurück (vermutet).", "1 = walks back to its spawn after combat (assumed)."),
                Fld(1, "TurnSpeed", K::Int, "Drehtempo", "Turn speed", "Drehgeschwindigkeit (vermutet).", "Turn speed (assumed)."),
                Fld(1, "RegenInterval", K::Int, "Respawn-Intervall", "Respawn interval", "Zeit bis zum Wiedererscheinen (vermutet).", "Time until it respawns (assumed)."),
            }, false},
            {"Erholung & Widerstände (Server)", "Recovery & resistances (server)", {
                Fld(1, "PceHPRcv", K::Int, "HP-Erholung (ruhig)", "HP recovery (idle)", "HP-Erholung außerhalb des Kampfes (vermutet).", "HP recovery out of combat (assumed)."),
                Fld(1, "PceHPRcvDly", K::Int, "Intervall (ruhig)", "Interval (idle)", "Intervall der HP-Erholung außerhalb des Kampfes (vermutet).", "Interval of HP recovery out of combat (assumed)."),
                Fld(1, "AtkHPRcv", K::Int, "HP-Erholung (Kampf)", "HP recovery (combat)", "HP-Erholung im Kampf (vermutet).", "HP recovery in combat (assumed)."),
                Fld(1, "AtkHPRcvDly", K::Int, "Intervall (Kampf)", "Interval (combat)", "Intervall der HP-Erholung im Kampf (vermutet).", "Interval of HP recovery in combat (assumed)."),
                Fld(1, "BloodingResi", K::Int, "Widerstand Blutung", "Bleeding resist", "Widerstand gegen Blutung (vermutet).", "Resistance to bleeding (assumed)."),
                Fld(1, "StunResi", K::Int, "Widerstand Betäubung", "Stun resist", "Widerstand gegen Betäubung (vermutet).", "Resistance to stun (assumed)."),
                Fld(1, "MoveSpeedResi", K::Int, "Widerstand Verlangsamung", "Slow resist", "Widerstand gegen Verlangsamung (vermutet).", "Resistance to slow (assumed)."),
                Fld(1, "FearResi", K::Int, "Widerstand Furcht", "Fear resist", "Widerstand gegen Furcht (vermutet).", "Resistance to fear (assumed)."),
            }, false},
            {"Darstellung", "Presentation", {
                Fld(2, "FileName", K::Values, "Modell", "Model"), Fld(2, "Texture", K::Values, "Textur", "Texture"),
                Fld(2, "NpcViewIndex", K::Int, "Spieler-Aussehen", "Player look"),
                Fld(2, "AttackType", K::Int, "Angriffsart", "Attack type"), Fld(2, "ShotEffect", K::Values, "Geschoss-Effekt", "Projectile effect"),
                Fld(2, "MobPortrait", K::Values, "Porträt", "Portrait"), Fld(2, "MiniMapIcon", K::Int, "Minikarten-Symbol", "Minimap icon"),
                Fld(2, "BoundingBox", K::Int, "Auswahlkörper", "Selection volume"),
                Fld(2, "ChrMarkSize", K::Int, "Markierungsgröße", "Marker size", "Größe der Auswahlmarkierung (50 = Standard) (vermutet).", "Size of the selection marker (50 = default) (assumed)."),
            }, false},
        };
        return s;
    }();
    return spec;
}

void DrawItemEditor(EditorState& state) {
    DrawDataFormEditor(state, ItemFormSpec(), state.itemForm, L("ITEM-EDITOR", "ITEM EDITOR"), DrawIconTable, "module.prices",
                       [&](const dataform::Docs& docs, long long id) { DrawItemHeader(state, docs, id); },
                       [&](const dataform::Docs& docs, long long id) { return ItemProblems(state, docs, id); },
                       [&](long long id, float size) { DrawItemIconById(state, id, size); });
}

void DrawMobEditor(EditorState& state) {
    DrawDataFormEditor(state, MobFormSpec(), state.mobForm, L("MOB-EDITOR", "MOB EDITOR"), DrawIconSpawn, "module.custom_mob",
                       [&](const dataform::Docs& docs, long long id) { DrawMobHeader(state, docs, id); },
                       [&](const dataform::Docs& docs, long long id) { return MobProblems(state, docs, id); });
}

} // namespace nge
