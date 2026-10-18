// AttackSequenceEditor.cpp
// Angriffsfolgen der Mobs (Datei Editor, Unterreiter 17): Server/9Data/Shine/MobAttackSequence/<Mob>.txt.
// Schritte (AttSeq) mit Skill-Auswahl aus der Server-ActiveSkill, Regeln (SkillChange), neue Folge als
// Kopie für einen anderen Mob, Speichern als Projektkopie. Bearbeitet wird über
// core::legacy::MobAttackSequence; jede Änderung ist ein Undo-Schritt (Schnappschuss der Datei).

#include "EditorShared.hpp"

#include "mapeditor/core/legacy/MobAttackSequence.hpp"

namespace nge {
namespace {

namespace legacy = core::legacy;
using Source = EditorState::ShnSource;

const char* const kConditions[] = {"HPLow_ChangeOrder", "OutOfRange", "HPLow", "NextSkill"};

std::filesystem::path ServerSequenceDir(const EditorState& state) {
    return state.shnServerRoot.empty() ? std::filesystem::path() : std::filesystem::path(state.shnServerRoot) / "MobAttackSequence";
}

void ScanSequenceFiles(EditorState& state) {
    auto& as = state.attackSeq;
    const std::string key = state.shnServerRoot + "|" + state.project.projectFolder;
    if (as.listRoot == key) return;
    as.listRoot = key;
    std::set<std::string> stems;
    std::error_code ec;
    const auto scan = [&](const std::filesystem::path& dir) {
        if (dir.empty() || !std::filesystem::is_directory(dir, ec)) return;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec))
            if (e.is_regular_file(ec) && LowerAscii(e.path().extension().string()) == ".txt") stems.insert(e.path().stem().string());
    };
    scan(ServerSequenceDir(state));
    if (const auto project = ProjectServerShinePath(state.project, "MobAttackSequence")) scan(*project);
    as.files.assign(stems.begin(), stems.end());
}

bool LoadSequence(EditorState& state, const std::string& stem) {
    auto& as = state.attackSeq;
    as.loaded = stem;
    as.dirty = false;
    as.loadError.clear();
    const std::filesystem::path source = ServerSequenceDir(state) / (stem + ".txt");
    std::filesystem::path path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, source);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        if (const auto project = ProjectServerShinePath(state.project, std::filesystem::path("MobAttackSequence") / (stem + ".txt")))
            path = *project;
    auto file = legacy::LoadShineTextFile(path);
    if (!file) { as.loadError = file.error(); as.loadedOk = false; return false; }
    as.doc = std::move(*file);
    as.loadedOk = true;
    return true;
}

const std::unordered_map<std::string, std::string>& NameMap(EditorState& state, const char* file, Source side) {
    struct Cached { std::uint64_t edit = ~0ull; std::string root; bool german = false; std::unordered_map<std::string, std::string> map; };
    static std::unordered_map<std::string, Cached> cache;
    const std::string& root = side == Source::Client ? state.shnClientRoot : state.shnServerRoot;
    auto& c = cache[file];
    const bool german = app::CurrentLanguage() == app::Language::German;
    if (c.edit == state.shnEditCounter && c.root == root && c.german == german) return c.map;
    c.edit = state.shnEditCounter;
    c.root = root;
    c.german = german;
    c.map.clear();
    if (root.empty()) return c.map;
    const int doc = SharedShnDoc(state, file, side, root);
    if (doc < 0) return c.map;
    const auto& f = ShnDocFile(state, doc);
    // Mob-Skills tragen in NA2016 koreanische Namen (CP949): übersetzt anzeigen (Sprache der Oberfläche).
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        c.map.emplace(TrimAscii(ShnCellText(f, r, "InxName")), DisplayText(ShnCellText(f, r, "Name")));
    return c.map;
}
const std::unordered_map<std::string, std::string>& SkillNames(EditorState& state) { return NameMap(state, "ActiveSkill.shn", Source::Server); }
const std::unordered_map<std::string, std::string>& MobNames(EditorState& state) { return NameMap(state, "MobInfo.shn", Source::Client); }

// Wendet eine geänderte Folge an und zeichnet sie als einen Undo-Schritt auf.
void CommitSequence(EditorState& state, const legacy::AttackSequence& seq, const std::string& label) {
    auto& as = state.attackSeq;
    legacy::ShineTextFile before = as.doc;
    legacy::ShineTextFile after = as.doc;
    if (auto ok = legacy::ApplyAttackSequence(after, seq); !ok) { as.report = {ok.error()}; return; }
    as.doc = after;
    as.dirty = true;
    EditorState* s = &state;
    const std::string stem = as.loaded;
    state.dataHistory.Record({
        [s, stem, after]() { s->attackSeq.loaded = stem; s->attackSeq.doc = after; s->attackSeq.loadedOk = true; s->attackSeq.dirty = true; },
        [s, stem, before]() { s->attackSeq.loaded = stem; s->attackSeq.doc = before; s->attackSeq.loadedOk = true; s->attackSeq.dirty = true; },
    }, std::string(L("Angriffsfolge ", "Attack sequence ")) + stem + ": " + label);
}

std::string SkillLabel(EditorState& state, const std::string& attack) {
    if (attack == legacy::kAttackEmpty) return L("(leerer Schritt)", "(empty step)");
    if (attack == legacy::kAttackRepeatMarker) return L("Wiederholung ab hier (vermutet)", "Repeat from here (assumed)");
    const auto& names = SkillNames(state);
    const auto it = names.find(attack);
    return it == names.end() ? std::string() : it->second;
}
bool KnownAttack(EditorState& state, const std::string& attack) {
    if (attack == legacy::kAttackEmpty || attack == legacy::kAttackRepeatMarker) return true;
    const auto& names = SkillNames(state);
    return names.empty() || names.contains(attack);
}

// Skill-Auswahl als Popup; true und result gesetzt, wenn gewählt wurde.
bool SkillPickerPopup(EditorState& state, const char* popupId, bool allowMarkers, std::string& result) {
    bool picked = false;
    if (ImGui::BeginPopup(popupId)) {
        auto& as = state.attackSeq;
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(320.0f);
        UI::InputTextWithHint("##attackSkillFilter", L("Skill suchen (InxName oder Name)", "Search skill (InxName or name)"),
                              as.skillFilter, sizeof(as.skillFilter));
        if (ImGui::Selectable(L("- (leerer Schritt)", "- (empty step)"))) { result = legacy::kAttackEmpty; picked = true; }
        if (allowMarkers && ImGui::Selectable(L("*RepeatStart (Wiederholung ab hier, vermutet)", "*RepeatStart (repeat from here, assumed)"))) {
            result = legacy::kAttackRepeatMarker;
            picked = true;
        }
        ImGui::Separator();
        const std::string needle = LowerAscii(as.skillFilter);
        ImGui::BeginChild("##attackSkillList", ImVec2(420.0f, 300.0f));
        int shown = 0;
        for (const auto& [inx, name] : SkillNames(state)) {
            if (!needle.empty() && LowerAscii(inx + " " + name).find(needle) == std::string::npos) continue;
            if (++shown > 400) { ImGui::TextDisabled("%s", L("... Suche verfeinern", "... refine the search")); break; }
            if (ImGui::Selectable((inx + "  ·  " + name).c_str())) { result = inx; picked = true; }
        }
        ImGui::EndChild();
        if (picked) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return picked;
}

bool SaveSequence(EditorState& state) {
    auto& as = state.attackSeq;
    const auto target = ProjectServerShinePath(state.project, std::filesystem::path("MobAttackSequence") / (as.loaded + ".txt"));
    const auto saved = SaveProjectShineOverride(as.doc, target);
    if (!saved) { as.report = {std::string(L("Speichern fehlgeschlagen: ", "Save failed: ")) + saved.error()}; return false; }
    const std::string stem = as.loaded;
    LoadSequence(state, stem); // neue Zeilen bekommen ihre Quellzeilen
    as.listRoot.clear();
    as.report = {std::string(L("Gespeichert: ", "Saved: ")) + saved->string()};
    return true;
}

} // namespace

void OpenAttackSequence(EditorState& state, const std::string& mobInx) {
    state.screen = AppScreen::ShnEditor;
    state.shnSubTab = 17;
    state.attackSeq.selected = mobInx;
    std::snprintf(state.attackSeq.newMob, sizeof(state.attackSeq.newMob), "%s", mobInx.c_str());
}

bool AttackSequenceExists(EditorState& state, const std::string& mobInx) {
    ScanSequenceFiles(state);
    return std::binary_search(state.attackSeq.files.begin(), state.attackSeq.files.end(), mobInx);
}

std::string AttackSequenceAutomation(EditorState& state, const std::string& cmd, std::istream& in) {
    auto& as = state.attackSeq;
    if (cmd == "attackseq") {
        std::string stem; in >> stem;
        OpenAttackSequence(state, stem);
        ScanSequenceFiles(state);
        if (!LoadSequence(state, stem)) return "load failed: " + as.loadError;
        const auto seq = legacy::ReadAttackSequence(as.doc);
        return seq ? std::to_string(seq->attacks.size()) + " steps, " + std::to_string(seq->rules.size()) + " rules" : seq.error();
    }
    if (!as.loadedOk) return "no sequence loaded";
    auto seq = legacy::ReadAttackSequence(as.doc);
    if (!seq) return seq.error();
    if (cmd == "attackseqset") {
        std::size_t index = 0; std::string skill; in >> index >> skill;
        if (index >= seq->attacks.size()) return "index out of range";
        seq->attacks[index] = skill;
        CommitSequence(state, *seq, "set");
    } else if (cmd == "attackseqinsert") {
        std::size_t index = 0; std::string skill; in >> index >> skill;
        seq->attacks.insert(seq->attacks.begin() + static_cast<std::ptrdiff_t>(std::min(index, seq->attacks.size())), skill);
        CommitSequence(state, *seq, "insert");
    } else if (cmd == "attackseqrule") {
        legacy::AttackRule rule; in >> rule.condition >> rule.value >> rule.from >> rule.to;
        seq->rules.push_back(rule);
        CommitSequence(state, *seq, "rule");
    } else if (cmd == "attackseqcopy") {
        std::string to; in >> to;
        as.doc.path = ServerSequenceDir(state) / (to + ".txt");
        as.loaded = to;
        as.dirty = true;
    } else if (cmd == "attackseqsave") {
        SaveSequence(state);
        return as.report.empty() ? std::string() : as.report.front();
    } else {
        return "unknown command";
    }
    return "ok";
}

void DrawAttackSequenceEditor(EditorState& state) {
    auto& as = state.attackSeq;
    ScanSequenceFiles(state);
    DrawPanelHeader("attackSeqHeader", L("ANGRIFFSFOLGEN", "ATTACK SEQUENCES"), DrawIconSword, "module.ai",
                    L("Skill-Reihenfolge und Regeln der Mobs (MobAttackSequence)", "Skill order and rules of mobs (MobAttackSequence)"));
    if (state.shnServerRoot.empty()) {
        ImGui::TextColored(UiTheme::Warning, "%s", L("Kein Server-Ordner im Projekt.", "No server folder in the project."));
        return;
    }
    if (!as.selected.empty() && as.selected != as.loaded && !as.dirty) LoadSequence(state, as.selected);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##attackSeqFiles", ImVec2(300.0f, avail.y), true);
    DrawSearchInput("attackSeqFilter", L("Mob suchen", "Search mob"), as.filter, sizeof(as.filter));
    const auto& mobs = MobNames(state);
    const std::string needle = LowerAscii(as.filter);
    ImGui::TextDisabled(L("%zu Dateien", "%zu files"), as.files.size());
    ImGui::BeginChild("##attackSeqFileList", ImVec2(0, -96.0f), false);
    for (const auto& stem : as.files) {
        const auto it = mobs.find(stem);
        const std::string name = it == mobs.end() ? std::string() : it->second;
        if (!needle.empty() && LowerAscii(stem + " " + name).find(needle) == std::string::npos) continue;
        const std::string label = stem + (name.empty() ? std::string(L("  (kein Mob)", "  (no mob)")) : "  ·  " + name);
        if (ImGui::Selectable(label.c_str(), as.loaded == stem)) {
            if (as.dirty && as.loaded != stem) as.report = {L("Erst speichern oder Strg+Z - ungespeicherte Änderungen.", "Save first or Ctrl+Z - unsaved changes.")};
            else as.selected = stem;
        }
    }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputTextWithHint("##attackSeqNewMob", L("Mob-InxName für neue Folge", "Mob InxName for a new sequence"), as.newMob, sizeof(as.newMob));
    ImGui::BeginDisabled(!as.loadedOk || as.newMob[0] == '\0');
    if (UI::Button(L("Als Kopie für diesen Mob anlegen##attackSeq", "Create as copy for this mob##attackSeq"), ImVec2(-1, 0))) {
        const std::string to = TrimAscii(as.newMob);
        if (std::binary_search(as.files.begin(), as.files.end(), to)) as.report = {to + L(" hat schon eine Angriffsfolge.", " already has an attack sequence.")};
        else {
            as.doc.path = ServerSequenceDir(state) / (to + ".txt");
            as.loaded = as.selected = to;
            as.dirty = true;
            as.report = {std::string(L("Kopie für ", "Copy for ")) + to + L(" - noch nicht gespeichert.", " - not saved yet.")};
        }
    }
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##attackSeqMain", ImVec2(0, avail.y), true);
    if (!as.loadedOk) {
        ImGui::TextDisabled("%s", as.loadError.empty() ? L("Links einen Mob wählen.", "Pick a mob on the left.") : as.loadError.c_str());
        ImGui::EndChild();
        return;
    }
    auto seq = legacy::ReadAttackSequence(as.doc);
    if (!seq) { ImGui::TextColored(UiTheme::Warning, "%s", seq.error().c_str()); ImGui::EndChild(); return; }
    {
        const auto it = mobs.find(as.loaded);
        ImGui::TextColored(UiTheme::AccentCyan, "%s%s%s", as.loaded.c_str(), as.dirty ? " *" : "",
                           it == mobs.end() ? L("  (kein Mob mit diesem InxName in MobInfo)", "  (no mob with this InxName in MobInfo)")
                                            : ("  ·  " + it->second).c_str());
    }
    if (UI::Button(L("Speichern##attackSeq", "Save##attackSeq")) || (ShortcutPressed(state.shortcutSave) && as.dirty)) SaveSequence(state);
    ImGui::SameLine();
    DrawDataHistoryButtons(state, "attackSeqHistory");
    if (!seq->ordersSequential)
        ImGui::TextColored(UiTheme::Warning, "%s", L("Order war in der Datei nicht 0..n-1 - die nächste Änderung nummeriert neu.",
                                                     "Order in the file was not 0..n-1 - the next change renumbers it."));

    ImGui::SeparatorText(L("Schritte (AttSeq)", "Steps (AttSeq)"));
    std::optional<legacy::AttackSequence> next;
    std::string action;
    if (ImGui::BeginTable("##attackSteps", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 34.0f);
        ImGui::TableSetupColumn(L("Skill", "Skill"));
        ImGui::TableSetupColumn(L("Name", "Name"));
        ImGui::TableSetupColumn("##stepActions", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < seq->attacks.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%zu", i);
            ImGui::TableNextColumn();
            const std::string& attack = seq->attacks[i];
            if (!KnownAttack(state, attack)) ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::Warning);
            if (ImGui::Button((attack + "##pick").c_str(), ImVec2(-1.0f, 0.0f))) { as.skillFilter[0] = '\0'; ImGui::OpenPopup("##attackPick"); }
            if (!KnownAttack(state, attack)) ImGui::PopStyleColor();
            std::string picked;
            if (SkillPickerPopup(state, "##attackPick", true, picked) && picked != attack) {
                next = *seq; (*next).attacks[i] = picked; action = L("Schritt geändert", "step changed");
            }
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", SkillLabel(state, attack).c_str());
            ImGui::TableNextColumn();
            if (UI::SmallButton("^##stepUp") && i > 0) { next = *seq; std::swap((*next).attacks[i], (*next).attacks[i - 1]); action = L("Schritt verschoben", "step moved"); }
            ImGui::SameLine();
            if (UI::SmallButton("v##stepDown") && i + 1 < seq->attacks.size()) { next = *seq; std::swap((*next).attacks[i], (*next).attacks[i + 1]); action = L("Schritt verschoben", "step moved"); }
            ImGui::SameLine();
            if (UI::SmallButton("+##stepInsert")) { next = *seq; (*next).attacks.insert((*next).attacks.begin() + static_cast<std::ptrdiff_t>(i) + 1, attack); action = L("Schritt eingefügt", "step inserted"); }
            ImGui::SameLine();
            if (UI::SmallButton("X##stepDelete") && seq->attacks.size() > 1) { next = *seq; (*next).attacks.erase((*next).attacks.begin() + static_cast<std::ptrdiff_t>(i)); action = L("Schritt gelöscht", "step deleted"); }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (UI::SmallButton(L("+ Schritt am Ende##attackSeq", "+ Step at the end##attackSeq"))) {
        next = *seq; (*next).attacks.push_back(seq->attacks.empty() ? std::string(legacy::kAttackEmpty) : seq->attacks.back());
        action = L("Schritt angehängt", "step appended");
    }

    ImGui::SeparatorText(L("Regeln (SkillChange)", "Rules (SkillChange)"));
    ImGui::TextDisabled("%s", L("Bedingung, Wert (z. B. HP-Grenze in Promille, vermutet), From = Schritt, To = neuer Skill.",
                                "Condition, value (e.g. HP limit in per mille, assumed), From = step, To = new skill."));
    if (!seq->rules.empty() &&
        ImGui::BeginTable("##attackRules", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(L("Bedingung", "Condition"));
        ImGui::TableSetupColumn(L("Wert", "Value"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("To");
        ImGui::TableSetupColumn("##ruleActions", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableHeadersRow();
        for (std::size_t r = 0; r < seq->rules.size(); ++r) {
            ImGui::PushID(static_cast<int>(1000 + r));
            const auto& rule = seq->rules[r];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##cond", rule.condition.c_str())) {
                for (const char* c : kConditions)
                    if (ImGui::Selectable(c, rule.condition == c) && rule.condition != c) { next = *seq; (*next).rules[r].condition = c; action = L("Regel geändert", "rule changed"); }
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            std::string edited;
            if (DeferredInputText("##value", rule.value, edited, -1.0f)) { next = *seq; (*next).rules[r].value = TrimAscii(edited); action = L("Regel geändert", "rule changed"); }
            ImGui::TableNextColumn();
            if (DeferredInputText("##from", rule.from, edited, -1.0f)) { next = *seq; (*next).rules[r].from = TrimAscii(edited); action = L("Regel geändert", "rule changed"); }
            ImGui::TableNextColumn();
            if (!KnownAttack(state, rule.to)) ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::Warning);
            if (ImGui::Button((rule.to + "##ruleTo").c_str(), ImVec2(-1.0f, 0.0f))) { as.skillFilter[0] = '\0'; ImGui::OpenPopup("##rulePick"); }
            if (!KnownAttack(state, rule.to)) ImGui::PopStyleColor();
            std::string picked;
            if (SkillPickerPopup(state, "##rulePick", false, picked) && picked != rule.to) { next = *seq; (*next).rules[r].to = picked; action = L("Regel geändert", "rule changed"); }
            ImGui::TableNextColumn();
            if (UI::SmallButton("X##ruleDelete")) { next = *seq; (*next).rules.erase((*next).rules.begin() + static_cast<std::ptrdiff_t>(r)); action = L("Regel gelöscht", "rule deleted"); }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (UI::SmallButton(L("+ Regel##attackSeq", "+ Rule##attackSeq"))) {
        next = *seq;
        (*next).rules.push_back({"HPLow_ChangeOrder", "500", "0", seq->attacks.empty() ? std::string(legacy::kAttackEmpty) : seq->attacks.front()});
        action = L("Regel angelegt", "rule added");
    }
    if (next) CommitSequence(state, *next, action);

    if (!as.report.empty()) {
        ImGui::Separator();
        for (const auto& line : as.report) ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::EndChild();
}

} // namespace nge
