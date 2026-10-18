// SkillStateEditors.cpp
// Passiv-Skill-Editor (PassiveSkill + PassiveSkillView) und Zustands-Editor (AbState + AbStateView
// mit den Stärkestufen aus SubAbState) auf Basis des Formular-Editors. Gemessen an NA2016:
// PassiveSkill.DemandSk nennt in 470 von 503 Zeilen die Vorstufe (sonst "-"); AbState.SubAbState
// nennt in 775 von 777 Zeilen einen SubAbState-InxName, dessen Zeilen die Stufen 1..n sind;
// ActiveSkill.StaNameA-D nennt 1087-mal einen AbState. AbStateView schreibt die Spalte "inxName".

#include "EditorShared.hpp"

namespace nge {

namespace {
using K = dataform::Kind;
using F = dataform::Field;
const ImVec4 kDim(0.62f, 0.66f, 0.72f, 1.0f);
const ImVec4 kWarn(1.0f, 0.68f, 0.25f, 1.0f);

F Fld(int table, const char* col, K kind, const char* de, const char* en, const char* tipDe = nullptr, const char* tipEn = nullptr) {
    F f;
    f.table = table; f.col = col; f.kind = kind;
    f.labelDe = de; f.labelEn = en; f.tipDe = tipDe; f.tipEn = tipEn;
    return f;
}
F Ref(int table, const char* col, const char* file, bool server, const char* de, const char* en, const char* tipDe, const char* tipEn) {
    F f = Fld(table, col, K::InxRef, de, en, tipDe, tipEn);
    f.refFile = file;
    f.refServer = server;
    return f;
}

// Doppelte InxNames in der Haupttabelle und fehlende Zeilen in einer Kopie.
void BasicProblems(EditorState& state, const dataform::Spec& spec, const dataform::Docs& docs, long long id, std::vector<std::string>& out) {
    const int mainDoc = docs.Main();
    const long long row = FormRowIn(state, mainDoc, id);
    if (row < 0) return;
    const auto& mf = ShnDocFile(state, mainDoc);
    const std::string inx = TrimAscii(ShnCellText(mf, static_cast<std::size_t>(row), "InxName"));
    int same = 0;
    for (std::size_t r = 0; r < mf.rows.size(); ++r) same += TrimAscii(ShnCellText(mf, r, "InxName")) == inx;
    if (same > 1) out.push_back(std::string(L("InxName kommt mehrfach vor: ", "InxName occurs more than once: ")) + inx);
    for (std::size_t t = 0; t < docs.byTable.size(); ++t)
        for (const int d : docs.byTable[t])
            if (FormRowIn(state, d, id) < 0)
                out.push_back(std::string(L("Zeile fehlt in ", "Row missing in ")) + ShnSourceName(state.shnFiles[static_cast<std::size_t>(d)].source) +
                              " " + spec.tables[t].file);
}
bool InxExists(EditorState& state, const char* file, EditorState::ShnSource side, const std::string& inx) {
    const std::string& root = side == EditorState::ShnSource::Client ? state.shnClientRoot : state.shnServerRoot;
    const int doc = SharedShnDoc(state, file, side, root);
    if (doc < 0) return true; // nicht prüfbar
    const auto& f = ShnDocFile(state, doc);
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        if (TrimAscii(ShnCellText(f, r, "InxName")) == inx) return true;
    return false;
}

// SubAbState-Kopien (Client + Server) als Formular-Dokumente.
dataform::Docs SubAbStateDocs(EditorState& state) {
    dataform::Docs docs;
    std::vector<int> copies;
    if (!state.shnClientRoot.empty())
        if (const int d = SharedShnDoc(state, "SubAbState.shn", EditorState::ShnSource::Client, state.shnClientRoot); d >= 0) copies.push_back(d);
    if (!state.shnServerRoot.empty())
        if (const int d = SharedShnDoc(state, "SubAbState.shn", EditorState::ShnSource::Server, state.shnServerRoot); d >= 0) copies.push_back(d);
    docs.byTable.push_back(std::move(copies));
    return docs;
}

// Stärkestufen eines Zustands: alle SubAbState-Zeilen mit dem InxName, bearbeitbar in beiden Kopien.
void DrawSubAbStateLevels(EditorState& state, const std::string& sub) {
    const dataform::Docs docs = SubAbStateDocs(state);
    if (docs.Main() < 0) { ImGui::TextDisabled("%s", L("SubAbState.shn nicht geladen.", "SubAbState.shn not loaded.")); return; }
    const auto& f = ShnDocFile(state, docs.Main());
    std::vector<long long> ids;
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        if (TrimAscii(ShnCellText(f, r, "InxName")) == sub) ids.push_back(std::atoll(ShnCellText(f, r, "ID").c_str()));
    ImGui::SeparatorText((std::string(L("Stufen (SubAbState ", "Levels (SubAbState ")) + sub + ")").c_str());
    if (ids.empty()) {
        ImGui::TextColored(kWarn, "%s", L("Keine SubAbState-Zeile mit diesem Namen.", "No SubAbState row with this name."));
        return;
    }
    static const char* const kCols[] = {"Strength", "Type", "SubType", "KeepTime", "ActionIndexA", "ActionArgA", "ActionIndexB", "ActionArgB",
                                        "ActionIndexC", "ActionArgC", "ActionIndexD", "ActionArgD"};
    long long deleteId = -1;
    if (ImGui::BeginTable("##subLevels", static_cast<int>(std::size(kCols)) + 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("ID");
        for (const char* c : kCols) ImGui::TableSetupColumn(c);
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (const long long id : ids) {
            ImGui::PushID(static_cast<int>(id));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%lld", id);
            for (std::size_t c = 0; c < std::size(kCols); ++c) {
                ImGui::TableSetColumnIndex(static_cast<int>(c) + 1);
                std::string edited;
                const std::string current = FormCellRead(state, docs, 0, kCols[c], id);
                std::string details;
                const bool differ = FormCopiesDiffer(state, docs, 0, kCols[c], id, &details);
                if (differ) ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
                if (DeferredInputText((std::string("##") + kCols[c]).c_str(), current, edited, c == 3 ? 92.0f : 62.0f))
                    FormCellWrite(state, docs, 0, kCols[c], id, TrimAscii(edited), "SubAbState #" + std::to_string(id) + " · " + kCols[c]);
                if (differ) ImGui::PopStyleColor();
                if (differ && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", details.c_str());
            }
            ImGui::TableSetColumnIndex(static_cast<int>(std::size(kCols)) + 1);
            if (ImGui::SmallButton("x")) deleteId = id;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", L("Stufe löschen (Client und Server, Strg+Z macht es rückgängig).", "Delete level (client and server, Ctrl+Z undoes it)."));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("%s", L("KeepTime in ms (4000000000 = dauerhaft); Type/SubType/ActionIndex sind Aufzählungen - Werte aus ähnlichen Zuständen übernehmen (vermutet).",
                                "KeepTime in ms (4000000000 = permanent); Type/SubType/ActionIndex are enums - copy values from similar states (assumed)."));
    if (deleteId >= 0) {
        DataHistoryBegin(state, "SubAbState #" + std::to_string(deleteId) + L(" gelöscht", " deleted"));
        for (const int d : docs.byTable[0])
            if (const long long row = FormRowIn(state, d, deleteId); row >= 0) DeleteShnRowWithHistory(state, d, static_cast<int>(row));
        DataHistoryCommit(state);
    }
    if (UI::SmallButton(L("+ Stufe", "+ Level"))) {
        // Kopie der letzten Stufe mit freier ID, Strength + 1 - in beiden Kopien, ein Schritt.
        std::set<long long> used;
        const core::legacy::ShnColumn* idColumn = nullptr;
        for (const int d : docs.byTable[0]) {
            const auto& df = ShnDocFile(state, d);
            const int c = ShnColumnIndexByName(df, "ID");
            CollectShnInts(df, c, used);
            if (!idColumn && c >= 0) idColumn = &df.columns[static_cast<std::size_t>(c)];
        }
        const long long newId = idColumn ? SuggestFreeShnId(*idColumn, used) : -1;
        if (newId >= 0) {
            const long long last = ids.back();
            const long long strength = std::atoll(FormCellRead(state, docs, 0, "Strength", last).c_str());
            std::vector<std::string> report;
            DataHistoryBegin(state, "SubAbState " + sub + L(": Stufe angelegt", ": level added"));
            for (const int d : docs.byTable[0]) {
                const auto& df = ShnDocFile(state, d);
                const long long row = FormRowIn(state, d, last);
                if (row < 0) continue;
                CloneRowsById(state, "SubAbState.shn", state.shnFiles[static_cast<std::size_t>(d)].source, "InxName", last,
                              ShnCellText(df, static_cast<std::size_t>(row), "InxName"), newId,
                              ShnCellText(df, static_cast<std::size_t>(row), "InxName"), report);
            }
            FormCellWrite(state, docs, 0, "Strength", newId, std::to_string(strength + 1), "Strength");
            DataHistoryCommit(state);
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", L("Neue Stufe als Kopie der letzten (freie ID, Stärke + 1) in Client und Server.",
                                  "New level as a copy of the last one (free ID, strength + 1) in client and server."));
}

void DrawAbStateHeader(EditorState& state, const dataform::Docs& docs, long long id) {
    const std::string inx = TrimAscii(FormCellRead(state, docs, 0, "InxName", id));
    const std::string iconFile = TrimAscii(FormCellRead(state, docs, 1, "iconFile", id));
    const int icon = std::atoi(FormCellRead(state, docs, 1, "icon", id).c_str());
    DrawAtlasIcon(state, iconFile, icon, 40.0f);
    ImGui::SameLine();
    ImGui::BeginGroup();
    const std::string sort = TrimAscii(FormCellRead(state, docs, 1, "IconSort", id));
    ImGui::Text("%s  %s #%d", sort.c_str(), iconFile.c_str(), icon);
    if (!iconFile.empty() && iconFile != "-") {
        ImGui::SameLine();
        if (UI::SmallButton(L("Im Atlas-Editor##state", "In atlas editor##state"))) OpenIconInAtlasEditor(state, iconFile, icon);
    }
    const std::string desc = FormCellRead(state, docs, 1, "Descript", id);
    if (!desc.empty()) ImGui::TextColored(kDim, "%s", desc.c_str());
    // Verwendet von: aktive Skills (StaNameA-D).
    const int skills = SharedShnDoc(state, "ActiveSkill.shn", EditorState::ShnSource::Client, state.shnClientRoot);
    if (skills >= 0) {
        const auto& sf = ShnDocFile(state, skills);
        int shown = 0, total = 0;
        for (std::size_t r = 0; r < sf.rows.size(); ++r) {
            bool uses = false;
            for (const char* c : {"StaNameA", "StaNameB", "StaNameC", "StaNameD"}) uses = uses || TrimAscii(ShnCellText(sf, r, c)) == inx;
            if (!uses) continue;
            ++total;
            if (shown >= 12) continue;
            const std::string skillInx = ShnCellText(sf, r, "InxName");
            if (shown++ == 0) ImGui::TextUnformatted(L("Skills:", "Skills:"));
            // Nur in derselben Zeile, wenn der Knopf noch passt.
            const float width = ImGui::CalcTextSize(skillInx.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
                ImGui::SameLine();
            ImGui::PushID(static_cast<int>(r));
            if (UI::SmallButton(skillInx.c_str())) {
                state.skill.selectedId = std::atoll(ShnCellText(sf, r, "ID").c_str());
                state.shnSubTab = 7;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", L("Diesen Skill im Skill-Editor öffnen.", "Open this skill in the skill editor."));
            ImGui::PopID();
        }
        if (total > shown) { ImGui::SameLine(); ImGui::TextDisabled(L("+%d weitere", "+%d more"), total - shown); }
        if (total == 0) ImGui::TextDisabled("%s", L("Kein aktiver Skill löst diesen Zustand aus.", "No active skill applies this state."));
    }
    ImGui::EndGroup();
    const std::string sub = TrimAscii(FormCellRead(state, docs, 0, "SubAbState", id));
    if (!sub.empty() && sub != "-") DrawSubAbStateLevels(state, sub);
    ImGui::Separator();
}

std::vector<std::string> AbStateProblems(EditorState& state, const dataform::Docs& docs, long long id) {
    std::vector<std::string> out;
    BasicProblems(state, AbStateFormSpec(), docs, id, out);
    const std::string inx = TrimAscii(FormCellRead(state, docs, 0, "InxName", id));
    const std::string viewInx = TrimAscii(FormCellRead(state, docs, 1, "inxName", id));
    if (!viewInx.empty() && viewInx != inx)
        out.push_back(std::string(L("AbStateView nennt einen anderen InxName: ", "AbStateView names another InxName: ")) + viewInx);
    const std::string sub = TrimAscii(FormCellRead(state, docs, 0, "SubAbState", id));
    if (!sub.empty() && sub != "-" && !InxExists(state, "SubAbState.shn", EditorState::ShnSource::Server, sub))
        out.push_back(std::string(L("SubAbState nicht gefunden: ", "SubAbState not found: ")) + sub);
    for (auto& missing : MissingEffectsFor(state, true, id)) out.push_back(std::move(missing));
    return out;
}

void DrawPassiveHeader(EditorState& state, const dataform::Docs& docs, long long id) {
    const std::string iconFile = TrimAscii(FormCellRead(state, docs, 1, "IconFile", id));
    const int icon = std::atoi(FormCellRead(state, docs, 1, "Icon", id).c_str());
    DrawAtlasIcon(state, iconFile, icon, 40.0f);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("%s #%d", iconFile.c_str(), icon);
    if (!iconFile.empty() && iconFile != "-") {
        ImGui::SameLine();
        if (UI::SmallButton(L("Im Atlas-Editor##passive", "In atlas editor##passive"))) OpenIconInAtlasEditor(state, iconFile, icon);
    }
    const std::string desc = FormCellRead(state, docs, 1, "Descript", id);
    if (!desc.empty()) ImGui::TextColored(kDim, "%s", desc.c_str());
    const std::string demand = TrimAscii(FormCellRead(state, docs, 0, "DemandSk", id));
    if (!demand.empty() && demand != "-") ImGui::TextDisabled(L("Voraussetzung: %s", "Requires: %s"), demand.c_str());
    ImGui::EndGroup();
    ImGui::Separator();
}

std::vector<std::string> PassiveProblems(EditorState& state, const dataform::Docs& docs, long long id) {
    std::vector<std::string> out;
    BasicProblems(state, PassiveFormSpec(), docs, id, out);
    const std::string demand = TrimAscii(FormCellRead(state, docs, 0, "DemandSk", id));
    if (!demand.empty() && demand != "-" && !InxExists(state, "PassiveSkill.shn", EditorState::ShnSource::Client, demand))
        out.push_back(std::string(L("Voraussetzung nicht gefunden: ", "Requirement not found: ")) + demand);
    return out;
}
} // namespace

long long AbStateIdByInx(EditorState& state, const std::string& inx) {
    const int doc = SharedShnDoc(state, "AbState.shn", EditorState::ShnSource::Server, state.shnServerRoot);
    if (doc < 0 || inx.empty()) return -1;
    const auto& f = ShnDocFile(state, doc);
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        if (TrimAscii(ShnCellText(f, r, "InxName")) == inx) return std::atoll(ShnCellText(f, r, "ID").c_str());
    return -1;
}

dataform::Spec& PassiveFormSpec() {
    static dataform::Spec spec = [] {
        dataform::Spec s;
        s.id = "Passive";
        s.tables = {{"PassiveSkill.shn", true, true}, {"PassiveSkillView.shn", true, false}};
        std::vector<F> mastery;
        static const char* const kWeapons[] = {"Sword1", "Hammer1", "Sword2", "Axe2", "Mace1", "Bow2", "CrossBow2", "Wand2", "Staff2", "Claw", "DSword"};
        static std::vector<std::string> cols; // Spaltennamen müssen so lange leben wie die Spec
        for (const char* w : kWeapons) { cols.push_back(std::string("MstRt") + w); cols.push_back(std::string("MstPl") + w); }
        for (const auto& c : cols)
            mastery.push_back(Fld(0, c.c_str(), K::Int, nullptr, nullptr,
                                  c.compare(0, 5, "MstRt") == 0 ? "Schadensbonus in % für diese Waffenart (MstRt = Rate) (vermutet)." : "Fester Schadensbonus für diese Waffenart (MstPl = Plus) (vermutet).",
                                  c.compare(0, 5, "MstRt") == 0 ? "Damage bonus in % for this weapon kind (MstRt = rate) (assumed)." : "Flat damage bonus for this weapon kind (MstPl = plus) (assumed)."));
        s.sections = {
            {"Allgemein", "General", {
                Fld(0, "Name", K::Text, "Name", "Name"),
                Ref(0, "DemandSk", "PassiveSkill.shn", false, "Voraussetzung", "Requirement",
                    "Passiv-Skill (InxName), der vorher gelernt sein muss - meist die Vorstufe; '-' = keiner.",
                    "Passive skill (InxName) that must be learned first - usually the previous step; '-' = none."),
                Fld(0, "WeaponMastery", K::Int, "Waffenbeherrschung", "Weapon mastery", "1 = Waffenbeherrschung (Boni je Waffenart unten) (vermutet).", "1 = weapon mastery (bonuses per weapon kind below) (assumed)."),
                Fld(1, "DemandLv", K::Int, "Benötigtes Level", "Required level", "Level, ab dem der Skill lernbar ist (Anzeige).", "Level from which the skill can be learned (display)."),
                Fld(1, "UseClass", K::Int, "Klassen", "Classes", "Klassen, die den Skill lernen (Aufzählung/Bitmaske) (vermutet).", "Classes that learn the skill (enum/bitmask) (assumed)."),
            }, true},
            {"Waffenbeherrschung", "Weapon mastery", mastery, false},
            {"Werte", "Values", {
                Fld(0, "SPRecover", K::Int, "SP-Erholung", "SP recovery", "Bonus auf SP-Erholung (vermutet).", "Bonus to SP recovery (assumed)."),
                Fld(0, "TB", K::Int, "Ausweichen (TB)", "Evade (TB)", "Bonus auf Ausweichen (vermutet).", "Bonus to evasion (assumed)."),
                Fld(0, "MaxSP", K::Int, "Max. SP", "Max SP", "Bonus auf maximale SP (vermutet).", "Bonus to maximum SP (assumed)."),
                Fld(0, "MaxLP", K::Int, "Max. LP", "Max LP", "Bonus auf maximale LP (vermutet).", "Bonus to maximum LP (assumed)."),
                Fld(0, "Intel", K::Int, "Intelligenz", "Intelligence", "Bonus auf Intelligenz (vermutet).", "Bonus to intelligence (assumed)."),
                Fld(0, "CastingTime", K::Int, "Zauberzeit", "Casting time", "Änderung der Zauberzeit (vermutet).", "Change of casting time (assumed)."),
                Fld(0, "MACriRate", K::Int, "Mag. Krit-Rate", "Magic crit rate", "Bonus auf magische kritische Treffer (vermutet).", "Bonus to magic critical hits (assumed)."),
                Fld(0, "WCRateUp", K::Int, "Phys. Schaden +%", "Phys. damage +%", "Prozentualer Bonus auf physischen Schaden (vermutet).", "Percentage bonus to physical damage (assumed)."),
                Fld(0, "MARateUp", K::Int, "Mag. Schaden +%", "Magic damage +%", "Prozentualer Bonus auf magischen Schaden (vermutet).", "Percentage bonus to magic damage (assumed)."),
                Fld(0, "HealUPRate", K::Int, "Heilung +%", "Healing +%", "Bonus auf Heilung (vermutet).", "Bonus to healing (assumed)."),
                Fld(0, "KeepTimeBuffUPRate", K::Int, "Buff-Dauer +%", "Buff duration +%", "Verlängert die Dauer eigener Buffs (vermutet).", "Extends the duration of own buffs (assumed)."),
                Fld(0, "CriDmgUpRate", K::Int, "Krit-Schaden +%", "Crit damage +%", "Bonus auf kritischen Schaden (vermutet).", "Bonus to critical damage (assumed)."),
                Fld(0, "DMG_MinusRate", K::Int, "Schaden -%", "Damage -%", "Verringert erlittenen Schaden (vermutet).", "Reduces damage taken (assumed)."),
                Fld(0, "ActiveSkillGroupInx", K::Int, "Aktiv-Skill-Gruppe", "Active skill group", "Gruppe aktiver Skills, auf die der Bonus wirkt (vermutet).", "Group of active skills the bonus applies to (assumed)."),
            }, false},
            {"Darstellung", "Presentation", {
                Fld(1, "IconFile", K::Values, "Icon-Datei", "Icon file", "Icon-Atlas (resmenu/Icon), z. B. FighterSk00.", "Icon atlas (resmenu/Icon), e.g. FighterSk00."),
                Fld(1, "Icon", K::Int, "Icon-Nummer", "Icon index", "Position im Icon-Atlas.", "Position in the icon atlas."),
                Fld(1, "R", K::Int, "Farbe R", "Colour R"), Fld(1, "G", K::Int, "Farbe G", "Colour G"), Fld(1, "B", K::Int, "Farbe B", "Colour B"),
                Fld(1, "Descript", K::Text, "Beschreibung", "Description", "Text im Skillfenster.", "Text in the skill window."),
            }, true},
        };
        return s;
    }();
    return spec;
}

dataform::Spec& AbStateFormSpec() {
    static dataform::Spec spec = [] {
        dataform::Spec s;
        s.id = "AbState";
        dataform::Table view{"AbStateView.shn", true, false};
        view.inxColumn = "inxName";
        s.tables = {{"AbState.shn", true, true}, view};
        auto party = [](const char* col) {
            return Ref(0, col, "AbState.shn", true, col, col, "Zustand (InxName), der auch auf Gruppenmitglieder wirkt (vermutet).",
                       "State (InxName) that also applies to party members (assumed).");
        };
        s.sections = {
            {"Allgemein", "General", {
                Ref(0, "SubAbState", "SubAbState.shn", true, "Stufen (SubAbState)", "Levels (SubAbState)",
                    "InxName der SubAbState-Zeilen mit den Werten je Stärkestufe (Tabelle oben).",
                    "InxName of the SubAbState rows with the values per strength level (table above)."),
                Fld(0, "AbStataIndex", K::Int, "Zustands-Nummer", "State number", "Laufende Nummer des Zustands (fast eindeutig) (vermutet).", "Running number of the state (nearly unique) (assumed)."),
                Fld(0, "StateGrade", K::Int, "Grad", "Grade", "Stufe/Rang des Zustands (0-7) (vermutet).", "Level/rank of the state (0-7) (assumed)."),
                Fld(0, "KeepTimeRatio", K::Int, "Dauer-Faktor", "Duration factor", "Faktor für die Dauer (fast immer 0) (vermutet).", "Duration factor (almost always 0) (assumed)."),
                Fld(0, "KeepTimePower", K::Int, "Dauer nach Stärke", "Duration by strength", "1 = Dauer hängt von der Stärke ab (vermutet).", "1 = duration depends on strength (assumed)."),
                Fld(0, "AbStateSaveType", K::Int, "Speicherart", "Save type", "Ob der Zustand beim Ausloggen erhalten bleibt (0-3, siehe AbStateSaveTypeInfo) (vermutet).", "Whether the state survives logout (0-3, see AbStateSaveTypeInfo) (assumed)."),
                Ref(0, "MainStateInx", "AbState.shn", true, "Hauptzustand", "Main state", "Übergeordneter Zustand (InxName) (vermutet).", "Parent state (InxName) (assumed)."),
                Fld(0, "Duplicate", K::Int, "Stapelung", "Stacking", "Wie oft der Zustand gleichzeitig wirken kann (vermutet).", "How often the state can be active at once (assumed)."),
                Fld(0, "DispelIndex", K::Values, "Aufheben-Gruppe", "Dispel group", "Gruppe für das Aufheben durch Skills (vermutet).", "Group for removal by skills (assumed)."),
                Fld(0, "SubDispelIndex", K::Values, "Aufheben-Untergruppe", "Dispel subgroup", "Untergruppe für das Aufheben (vermutet).", "Subgroup for removal (assumed)."),
            }, true},
            {"Gruppe", "Party", {
                party("PartyState1"), party("PartyState2"), party("PartyState3"), party("PartyState4"), party("PartyState5"),
                Fld(0, "PartyRange", K::Int, "Gruppen-Reichweite", "Party range", "Reichweite der Gruppenwirkung (vermutet).", "Range of the party effect (assumed)."),
                Fld(0, "PartyEnchantNumber", K::Int, "Gruppen-Anzahl", "Party count", "Anzahl betroffener Gruppenmitglieder (vermutet).", "Number of affected party members (assumed)."),
            }, false},
            {"Darstellung", "Presentation", {
                Fld(1, "IconSort", K::Values, "Art", "Kind", "BUFF oder DEBUFF (427/349 in NA2016).", "BUFF or DEBUFF (427/349 in NA2016)."),
                Fld(1, "iconFile", K::Values, "Icon-Datei", "Icon file", "Icon-Atlas (AbState00-03).", "Icon atlas (AbState00-03)."),
                Fld(1, "icon", K::Int, "Icon-Nummer", "Icon index", "Position im Icon-Atlas.", "Position in the icon atlas."),
                Fld(1, "Descript", K::Text, "Beschreibung", "Description", "Text im Zustandsfenster.", "Text in the state window."),
                Fld(1, "View", K::Int, "Sichtbar", "Visible", "1 = als Symbol angezeigt (683 von 776) (vermutet).", "1 = shown as an icon (683 of 776) (assumed)."),
                Fld(1, "TypeSort", K::Int, "Sortierung", "Sort group", "Sortiergruppe der Symbole (vermutet).", "Sort group of the icons (assumed)."),
                Fld(1, "AniIndex", K::Values, "Animation", "Animation", "Animation während des Zustands ('-' = keine).", "Animation during the state ('-' = none)."),
                Fld(1, "effName", K::Values, "Effekt: Start", "Effect: start"), Fld(1, "LoopEffect", K::Values, "Effekt: Schleife", "Effect: loop"),
                Fld(1, "LastEffect", K::Values, "Effekt: Ende", "Effect: end"), Fld(1, "DOTEffect", K::Values, "Effekt: Dauerschaden", "Effect: DOT"),
                Fld(1, "R", K::Int, "Farbe R", "Colour R"), Fld(1, "G", K::Int, "Farbe G", "Colour G"), Fld(1, "B", K::Int, "Farbe B", "Colour B"),
            }, false},
        };
        return s;
    }();
    return spec;
}

void DrawPassiveSkillEditor(EditorState& state) {
    DrawDataFormEditor(state, PassiveFormSpec(), state.passiveForm, L("PASSIV-SKILLS", "PASSIVE SKILLS"), DrawIconBolt, "module.skill",
                       [&](const dataform::Docs& docs, long long id) { DrawPassiveHeader(state, docs, id); },
                       [&](const dataform::Docs& docs, long long id) { return PassiveProblems(state, docs, id); });
}

void DrawAbStateEditor(EditorState& state) {
    DrawDataFormEditor(state, AbStateFormSpec(), state.stateForm, L("ZUSTÄNDE (ABSTATE)", "STATES (ABSTATE)"), DrawIconBolt, "module.skill",
                       [&](const dataform::Docs& docs, long long id) { DrawAbStateHeader(state, docs, id); },
                       [&](const dataform::Docs& docs, long long id) { return AbStateProblems(state, docs, id); });
}

} // namespace nge
