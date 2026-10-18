// KoreanDisplay.cpp
// Koreanische Texte (CP949) überall übersetzt anzeigen und auf Wunsch englisch in die Daten schreiben.
// Reihenfolge der Quellen: Projekt-Wörterbuch (<Projekt>/Texte/ko_user.tsv: Online-Ergebnisse und
// eigene Korrekturen) vor dem mitgelieferten Wörterbuch (assets/i18n/ko_dictionary.tsv) vor der
// Offline-Automatik (Glossar assets/i18n/ko_glossary.tsv + Umschrift, markiert mit "≈").

#include "EditorShared.hpp"

#include "UiIconAssets.hpp"

namespace nge {
namespace {

namespace legacy = core::legacy;

struct Translator {
    bool loaded = false;
    std::string projectFolder;
    legacy::KoreanDictionary shipped;
    legacy::KoreanDictionary glossary;
    legacy::KoreanDictionary user;
    std::uint64_t revision = 0; // erhöht bei jeder Änderung -> Cache ungültig
    struct Cached { std::uint64_t revision = ~0ull; KoreanResult result; };
    std::unordered_map<std::string, Cached> cache[2]; // [0] deutsch, [1] englisch
};

Translator& T9() { static Translator t; return t; }

std::filesystem::path I18nRoot() {
    const auto icons = app::UiIconAssets::FindAssetRoot(); // .../assets/ui/icons
    if (!icons.empty()) return icons.parent_path().parent_path() / "i18n";
    std::error_code ec;
    return std::filesystem::current_path(ec) / "assets" / "i18n";
}

std::filesystem::path UserDictionaryPath(const EditorState& state) {
    if (state.project.projectFolder[0] == '\0') return {};
    return std::filesystem::path(state.project.projectFolder) / "Texte" / "ko_user.tsv";
}

void EnsureLoaded() {
    auto& t = T9();
    if (t.loaded) return;
    t.loaded = true;
    const auto root = I18nRoot();
    t.shipped.LoadTsv(root / "ko_dictionary.tsv");
    t.glossary.LoadTsv(root / "ko_glossary.tsv");
    ++t.revision;
}

std::string TrimSpaces(const std::string& s) {
    const auto a = s.find_first_not_of(' ');
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(' ') - a + 1);
}

} // namespace

void UpdateKoreanProject(const EditorState& state) {
    EnsureLoaded();
    auto& t = T9();
    if (t.projectFolder == state.project.projectFolder) return;
    t.projectFolder = state.project.projectFolder;
    t.user = {};
    if (const auto path = UserDictionaryPath(state); !path.empty()) t.user.LoadTsv(path);
    ++t.revision;
}

KoreanResult TranslateKorean(const std::string& raw, bool english) {
    EnsureLoaded();
    auto& t = T9();
    auto& slot = t.cache[english ? 1 : 0][raw];
    if (slot.revision == t.revision) return slot.result;
    KoreanResult r;
    if (legacy::IsKoreanText(raw)) {
        r.korean = true;
        const std::string utf8 = TrimSpaces(legacy::DecodeCp949(raw).value_or(std::string()));
        r.utf8 = utf8;
        r.romanized = legacy::RomanizeHangul(utf8);
        if (const auto* u = t.user.Find(utf8)) { r.text = english ? u->english : u->german; r.source = KoreanSource::Project; }
        else if (const auto* d = t.shipped.Find(utf8)) { r.text = english ? d->english : d->german; r.source = KoreanSource::Dictionary; }
        else { r.text = legacy::GlossaryTranslate(utf8, t.glossary, english); r.source = KoreanSource::Automatic; }
    } else {
        r.text = raw;
    }
    slot = {t.revision, r};
    return r;
}

std::string DisplayText(const std::string& raw) {
    if (raw.empty() || std::none_of(raw.begin(), raw.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; })) return raw;
    const KoreanResult r = TranslateKorean(raw, app::CurrentLanguage() != app::Language::German);
    if (!r.korean) return raw;
    return r.source == KoreanSource::Automatic ? "\xE2\x89\x88 " + r.text : r.text; // "≈ "
}

std::string KoreanSearchText(const std::string& raw) {
    if (std::none_of(raw.begin(), raw.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; })) return {};
    const KoreanResult en = TranslateKorean(raw, true);
    if (!en.korean) return {};
    return en.text + " " + TranslateKorean(raw, false).text + " " + en.romanized;
}

std::string KoreanTooltip(const std::string& raw) {
    if (std::none_of(raw.begin(), raw.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; })) return {};
    const KoreanResult r = TranslateKorean(raw, app::CurrentLanguage() != app::Language::German);
    if (!r.korean) return {};
    const char* source = r.source == KoreanSource::Project ? L("Projekt-Wörterbuch", "project dictionary")
                       : r.source == KoreanSource::Dictionary ? L("mitgeliefertes Wörterbuch", "shipped dictionary")
                                                              : L("automatisch (Glossar + Umschrift)", "automatic (glossary + romanization)");
    return std::string(L("Original koreanisch (umgeschrieben): ", "Original Korean (romanized): ")) + r.romanized + "\n" +
           L("Übersetzung: ", "Translation: ") + source;
}

const legacy::KoreanDictionary& KoreanGlossary() {
    EnsureLoaded();
    return T9().glossary;
}

std::size_t KoreanDictionarySize() {
    EnsureLoaded();
    return T9().shipped.Size();
}

bool AddProjectKoreanTranslations(EditorState& state, const std::vector<std::pair<std::string, legacy::KoreanTranslation>>& entries,
                                  std::string& message) {
    UpdateKoreanProject(state);
    const auto path = UserDictionaryPath(state);
    if (path.empty()) { message = L("Kein Projektordner - Übersetzungen können nicht gespeichert werden.", "No project folder - translations cannot be saved."); return false; }
    auto& t = T9();
    for (const auto& [ko, tr] : entries) t.user.Set(ko, tr);
    ++t.revision;
    std::string err;
    if (!t.user.SaveTsv(path, &err)) { message = err; return false; }
    message = std::to_string(entries.size()) + L(" Übersetzungen gespeichert in ", " translations saved to ") + path.string();
    return true;
}

// Ersetzt alle koreanischen Zellen eines Dokuments durch die englische Übersetzung aus dem Wörterbuch
// (Projekt oder mitgeliefert; nie die Automatik). Ein Undo-Schritt; Server-/Client-Gegenstück mit.
KoreanApplyReport ApplyKoreanTranslation(EditorState& state, int document, const std::vector<int>& peers) {
    KoreanApplyReport rep;
    if (document < 0) return rep;
    DataHistoryBegin(state, std::string(L("Koreanisch übersetzt: ", "Korean translated: ")) + ShnDocFile(state, document).FileName());
    std::vector<int> docs{document};
    docs.insert(docs.end(), peers.begin(), peers.end());
    for (const int d : docs) {
        if (d < 0) continue;
        const auto& f = ShnDocFile(state, d);
        for (std::size_t r = 0; r < f.rows.size(); ++r)
            for (std::size_t c = 0; c < f.columns.size() && c < f.rows[r].values.size(); ++c) {
                const auto& col = f.columns[c];
                if (col.kind != legacy::ShnValueKind::String) continue;
                const std::string raw = legacy::ShnValueToString(f.rows[r].values[c]);
                const KoreanResult tr = TranslateKorean(raw, true);
                if (!tr.korean) continue;
                if (d == document) ++rep.korean;
                if (legacy::IsKoreanKeyColumn(col.name)) {
                    if (d == document) rep.keys.push_back(col.name + " #" + std::to_string(r) + ": " + tr.text);
                    continue;
                }
                if (tr.source == KoreanSource::Automatic) {
                    if (d == document) rep.missing.push_back(col.name + " #" + std::to_string(r) + ": " + tr.romanized);
                    continue;
                }
                if (col.type != 26 && col.length > 0 && tr.text.size() > col.length - 1) {
                    if (d == document)
                        rep.tooLong.push_back(col.name + " #" + std::to_string(r) + ": \"" + tr.text + "\" (" + std::to_string(tr.text.size()) +
                                              " > " + std::to_string(col.length - 1) + " Byte)");
                    continue;
                }
                if (ApplyShnCellText(state, d, static_cast<int>(r), static_cast<int>(c), tr.text) && d == document) ++rep.applied;
            }
    }
    DataHistoryCommit(state);
    return rep;
}

// Knopf "Koreanische Texte übersetzen (N)" mit Bericht; zählt die koreanischen Zellen gecacht.
void DrawKoreanApplyButton(EditorState& state, int document, const std::vector<int>& peers, const char* id) {
    if (document < 0 || document >= static_cast<int>(state.shnFiles.size())) return;
    static std::unordered_map<std::string, std::pair<std::string, std::size_t>> counts; // id -> (Schlüssel, Anzahl)
    static std::unordered_map<std::string, KoreanApplyReport> reports;
    const auto& f = ShnDocFile(state, document);
    const std::string key = std::to_string(document) + "|" + std::to_string(state.shnEditCounter) + "|" + std::to_string(f.rows.size());
    auto& [countKey, count] = counts[id];
    if (countKey != key) {
        countKey = key;
        count = 0;
        for (const auto& row : f.rows)
            for (std::size_t c = 0; c < f.columns.size() && c < row.values.size(); ++c)
                if (f.columns[c].kind == legacy::ShnValueKind::String && legacy::IsKoreanText(legacy::ShnValueToString(row.values[c]))) ++count;
    }
    if (count == 0) return;
    const std::string label = std::string(L("Koreanische Texte übersetzen", "Translate Korean texts")) + " (" + std::to_string(count) + ")##koreanApply" + id;
    if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
        reports[id] = ApplyKoreanTranslation(state, document, peers);
        ImGui::OpenPopup((std::string("##koreanReport") + id).c_str());
    }
    UI::Tip("Button", (std::string(L("Koreanische Texte übersetzen", "Translate Korean texts")) + "##koreanApply").c_str());
    if (ImGui::BeginPopup((std::string("##koreanReport") + id).c_str())) {
        const auto& r = reports[id];
        ImGui::Text(L("%zu von %zu koreanischen Zellen durch Englisch ersetzt (Strg+Z nimmt alles zurück).",
                      "%zu of %zu Korean cells replaced by English (Ctrl+Z reverts all)."), r.applied, r.korean);
        if (!r.tooLong.empty()) {
            ImGui::TextColored(UiTheme::Warning, L("%zu zu lang für die Spalte (übersprungen, bitte kürzen):", "%zu too long for the column (skipped, please shorten):"), r.tooLong.size());
            for (std::size_t i = 0; i < r.tooLong.size() && i < 15; ++i) ImGui::BulletText("%s", r.tooLong[i].c_str());
        }
        if (!r.keys.empty()) {
            ImGui::TextColored(UiTheme::Warning, L("%zu in Schlüsselspalten (Verweis/Befehl, nie übernommen):", "%zu in key columns (reference/command, never applied):"), r.keys.size());
            for (std::size_t i = 0; i < r.keys.size() && i < 15; ++i) ImGui::BulletText("%s", r.keys[i].c_str());
        }
        if (!r.missing.empty()) {
            ImGui::TextColored(UiTheme::Warning, L("%zu ohne Wörterbuch-Eintrag (übersprungen, nur automatisch):", "%zu without dictionary entry (skipped, automatic only):"), r.missing.size());
            for (std::size_t i = 0; i < r.missing.size() && i < 15; ++i) ImGui::BulletText("%s", r.missing[i].c_str());
        }
        ImGui::EndPopup();
    }
}

} // namespace nge
