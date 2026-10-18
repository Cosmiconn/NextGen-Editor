// EffectCheck.cpp
// Prüfung "fehlende Effekt-Dateien": die Effekt-Spalten von ActiveSkillView und AbStateView gegen die
// Dateien im Client-Ordner reseffect (core::legacy::FindMissingEffects). Liste im Skill-Editor mit
// Sprung zur Zeile, Problemzeilen im Zustands-Editor.

#include "EditorShared.hpp"

#include "mapeditor/core/legacy/EffectRefs.hpp"

namespace nge {
namespace {

namespace legacy = core::legacy;
using Source = EditorState::ShnSource;

const std::vector<std::string> kSkillEffectColumns = {"ShoEffect", "LastEffectA", "LastAreaEf", "DOTRageEft", "DOTRageEftLoop"};
const std::vector<std::string> kStateEffectColumns = {"effName", "LoopEffect", "LastEffect", "DOTEffect"};

struct EffectCache {
    std::string key;
    bool haveFolder = false;
    std::size_t fileCount = 0;
    std::vector<EffectIssue> issues;
};
EffectCache& Cache() { static EffectCache c; return c; }

void Rebuild(EditorState& state) {
    auto& c = Cache();
    const std::string key = std::string(state.project.clientFolder) + "|" + state.shnClientRoot + "|" + std::to_string(state.shnEditCounter);
    if (c.key == key) return;
    c.key = key;
    c.issues.clear();
    c.haveFolder = false;
    c.fileCount = 0;
    if (state.project.clientFolder[0] == '\0' || state.shnClientRoot.empty()) return;
    const auto dir = FindChildCaseInsensitive(std::filesystem::path(state.project.clientFolder), "reseffect");
    if (!dir) return;
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(*dir, ec))
        if (e.is_regular_file(ec)) names.push_back(e.path().filename().string());
    c.haveFolder = true;
    c.fileCount = names.size();
    const auto stems = legacy::EffectFileStems(names);
    const auto scan = [&](const char* file, const char* inxColumn, const std::vector<std::string>& columns, bool stateView) {
        const int doc = SharedShnDoc(state, file, Source::Client, state.shnClientRoot);
        if (doc < 0) return;
        const auto& f = ShnDocFile(state, doc);
        for (const auto& m : legacy::FindMissingEffects(f, columns, stems))
            c.issues.push_back({stateView, std::atoll(ShnCellText(f, m.row, "ID").c_str()), TrimAscii(ShnCellText(f, m.row, inxColumn)),
                                m.column, m.value, m.special});
    };
    scan("ActiveSkillView.shn", "InxName", kSkillEffectColumns, false);
    scan("AbStateView.shn", "inxName", kStateEffectColumns, true);
}

} // namespace

const std::vector<EffectIssue>& EffectIssues(EditorState& state) {
    Rebuild(state);
    return Cache().issues;
}

std::vector<std::string> MissingEffectsFor(EditorState& state, bool stateView, long long id) {
    std::vector<std::string> out;
    for (const auto& i : EffectIssues(state))
        if (i.stateView == stateView && i.id == id && !i.special)
            out.push_back(i.column + ": " + i.value + L(" - Datei fehlt in reseffect", " - file missing in reseffect"));
    return out;
}

void DrawEffectCheckSection(EditorState& state) {
    const auto& issues = EffectIssues(state);
    const auto& c = Cache();
    const std::size_t real = static_cast<std::size_t>(std::count_if(issues.begin(), issues.end(), [](const EffectIssue& i) { return !i.special; }));
    const std::string header = std::string(L("Effekt-Dateien prüfen", "Check effect files")) + " (" +
                               (c.haveFolder ? std::to_string(real) + L(" fehlen", " missing") : std::string(L("kein reseffect-Ordner", "no reseffect folder"))) +
                               ")##effectCheck";
    if (!UI::CollapsingHeader(header.c_str())) return;
    if (!c.haveFolder) {
        ImGui::TextDisabled("%s", L("Im Client-Ordner wurde kein reseffect gefunden.", "No reseffect folder was found in the client folder."));
        return;
    }
    ImGui::TextDisabled(L("%zu Dateien in reseffect. Geprüft: ActiveSkillView (ShoEffect, LastEffectA, LastAreaEf, DOTRageEft, DOTRageEftLoop) und AbStateView (effName, LoopEffect, LastEffect, DOTEffect).",
                          "%zu files in reseffect. Checked: ActiveSkillView (ShoEffect, LastEffectA, LastAreaEf, DOTRageEft, DOTRageEftLoop) and AbStateView (effName, LoopEffect, LastEffect, DOTEffect)."),
                        c.fileCount);
    if (issues.empty()) ImGui::TextDisabled("%s", L("Alle Effekt-Verweise haben eine Datei.", "Every effect reference has a file."));
    ImGui::BeginChild("##effectCheckList", ImVec2(0, std::min(220.0f, 24.0f + 20.0f * static_cast<float>(issues.size()))), true);
    for (std::size_t n = 0; n < issues.size(); ++n) {
        const auto& i = issues[n];
        const std::string label = std::string(i.stateView ? L("Zustand ", "State ") : "Skill ") + std::to_string(i.id) + "  " + i.inx + "  ·  " +
                                  i.column + " = " + i.value +
                                  (i.special ? std::string(L("  (Sonderwert, vermutlich keine Datei)", "  (special value, probably no file)")) : std::string()) +
                                  "##effect" + std::to_string(n);
        if (!i.special) ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::Warning);
        const bool clicked = ImGui::Selectable(label.c_str());
        if (!i.special) ImGui::PopStyleColor();
        if (clicked) {
            if (i.stateView) { state.stateForm.selectedId = i.id; state.shnSubTab = 14; }
            else { state.skill.selectedId = i.id; state.shnSubTab = 7; }
        }
    }
    ImGui::EndChild();
}

} // namespace nge
