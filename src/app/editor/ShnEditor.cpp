// ShnEditor.cpp
// SHN-Editor: Dokumente, Abhängigkeiten, Zellbearbeitung, Raster, Multi-Profile.

#include "EditorShared.hpp"

namespace nge {

std::string ShnShortValue(const core::legacy::ShnValue& value) {
    std::string s = core::legacy::ShnValueToString(value);
    if (s.size() > 120) s.resize(117), s += "...";
    return s;
}
const char* ShnSourceName(EditorState::ShnSource source) {
    return source == EditorState::ShnSource::Client ? "CLIENT" : "SERVER";
}
ImVec4 ShnSourceColor(EditorState::ShnSource source) {
    return source == EditorState::ShnSource::Client
        ? UiTheme::AccentBlue
        : UiTheme::TextSecondary;
}
std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
EditorState::ShnSource InferShnSource(const std::filesystem::path& path, EditorState::ShnSource fallback) {
    std::string full = LowerAscii(path.generic_string());
    if (full.find("ressystem") != std::string::npos || full.find("client") != std::string::npos)
        return EditorState::ShnSource::Client;
    if (full.find("shine") != std::string::npos || full.find("server") != std::string::npos ||
        full.find("database") != std::string::npos || full.find("databases") != std::string::npos)
        return EditorState::ShnSource::Server;
    return fallback;
}
void SelectShnDocument(EditorState& state, int index) {
    state.shnSelectedFile = index;
    state.shnSelectedRow = -1;
    state.shnSelectedColumn = -1;
    state.shnInlineEditActive = false;
    state.shnInlineEditFocusPending = false;
    state.shnEditPopupOpen = false;
    state.shnSortColumn = -1;
    state.shnSortAscending = true;
    state.shnColumnFilterFile = -1;
    state.shnColumnFilters.clear();
    state.shnVisibleKey.clear();
}
std::filesystem::path ShnWorkingPath(
    const EditorState& state,
    const std::filesystem::path& sourcePath,
    EditorState::ShnSource source) {

    const auto side = source == EditorState::ShnSource::Client
        ? core::ProjectOutputSide::Client : core::ProjectOutputSide::Server;
    return PreferProjectOverride(state.project, side, sourcePath);
}
void OpenShnFile(EditorState& state, const std::filesystem::path& path,
                std::optional<EditorState::ShnSource> sourceOverride) {
    const auto source = sourceOverride.value_or(InferShnSource(path, EditorState::ShnSource::Client));
    const auto workingPath = ShnWorkingPath(state, path, source);
    auto result = core::legacy::LoadShnFile(workingPath);
    if (!result) {
        state.shnStatus = "SHN öffnen fehlgeschlagen: " + result.error();
        return;
    }
    auto existing = std::find_if(state.shnFiles.begin(), state.shnFiles.end(), [&](const auto& f) {
        return f.sourcePath == path && f.source == source;
    });
    if (existing != state.shnFiles.end()) {
        SelectShnDocument(state, static_cast<int>(std::distance(state.shnFiles.begin(), existing)));
    } else {
        EditorState::ShnDocument doc;
        doc.file = std::move(*result);
        doc.sourcePath = path;
        doc.source = source;
        state.shnFiles.push_back(std::move(doc));
        SelectShnDocument(state, static_cast<int>(state.shnFiles.size()) - 1);
    }
    state.shnStatus = std::string(ShnSourceName(source)) + " geladen: " + path.filename().string() +
        (workingPath != path ? " (Projekt-Override)" : "");
}
void ScanShnFolder(EditorState& state, const std::filesystem::path& root, EditorState::ShnSource source) {
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        state.shnStatus = "SHN-Ordner nicht gefunden: " + root.string();
        return;
    }
    std::size_t loaded = 0, failed = 0;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || ec) { ec.clear(); continue; }
        auto ext = LowerAscii(it->path().extension().string());
        if (ext != ".shn") continue;
        const auto sourcePath = it->path();
        const auto workingPath = ShnWorkingPath(state, sourcePath, source);
        auto result = core::legacy::LoadShnFile(workingPath);
        if (!result) { ++failed; continue; }
        auto existing = std::find_if(state.shnFiles.begin(), state.shnFiles.end(), [&](const auto& f) {
            return f.sourcePath == sourcePath && f.source == source;
        });
        if (existing == state.shnFiles.end()) {
            EditorState::ShnDocument doc;
            doc.file = std::move(*result);
            doc.sourcePath = sourcePath;
            doc.source = source;
            state.shnFiles.push_back(std::move(doc));
            ++loaded;
        }
    }
    if (source == EditorState::ShnSource::Client) state.shnClientRoot = root.string();
    else state.shnServerRoot = root.string();
    if (state.shnSelectedFile < 0 && !state.shnFiles.empty()) SelectShnDocument(state, 0);
    state.shnStatus = std::string(ShnSourceName(source)) + ": " + std::to_string(loaded) +
                      " SHN geladen" + (failed ? ", " + std::to_string(failed) + " übersprungen" : "") + ".";
}
std::vector<int> ShnIndicesForSource(const EditorState& state, EditorState::ShnSource source) {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(state.shnFiles.size()); ++i)
        if (state.shnFiles[static_cast<std::size_t>(i)].source == source) result.push_back(i);
    return result;
}
int FindShnCounterpart(const EditorState& state, int document) {
    if (document < 0 || document >= static_cast<int>(state.shnFiles.size())) return -1;
    const auto& current = state.shnFiles[static_cast<std::size_t>(document)];
    const auto otherSource = current.source == EditorState::ShnSource::Client
        ? EditorState::ShnSource::Server : EditorState::ShnSource::Client;
    const std::string filename = LowerAscii(current.file.FileName());
    for (int i = 0; i < static_cast<int>(state.shnFiles.size()); ++i) {
        if (i == document) continue;
        const auto& candidate = state.shnFiles[static_cast<std::size_t>(i)];
        if (candidate.source == otherSource && LowerAscii(candidate.file.FileName()) == filename)
            return i;
    }
    return -1;
}
std::vector<int> MatchShnColumnsByName(const core::legacy::ShnFile& source,
                                       const core::legacy::ShnFile& counterpart) {
    std::vector<int> result(source.columns.size(), -1);
    for (std::size_t i = 0; i < source.columns.size(); ++i) {
        const std::string name = LowerAscii(source.columns[i].name);
        for (std::size_t j = 0; j < counterpart.columns.size(); ++j) {
            if (LowerAscii(counterpart.columns[j].name) == name) {
                result[i] = static_cast<int>(j);
                break;
            }
        }
    }
    return result;
}
const core::DataDependencyProfile& ShnDependencyProfile(int profile) {
    const auto profiles = core::DataDependencyProfiles();
    if (profiles.empty()) throw std::logic_error("No data dependency profiles");
    const auto index = static_cast<std::size_t>(std::clamp(profile, 0, static_cast<int>(profiles.size()) - 1));
    return profiles[index];
}
bool ShnDependencyRuleMatchesDocument(const core::DataDependencyRule& rule,
                                      const EditorState::ShnDocument& doc) {
    const bool sideMatches =
        (rule.tree == core::DataTree::Client && doc.source == EditorState::ShnSource::Client) ||
        (rule.tree == core::DataTree::Server && doc.source == EditorState::ShnSource::Server);
    if (!sideMatches) return false;

    const std::filesystem::path configuredPath(std::string(rule.relativePath));
    if (LowerAscii(configuredPath.extension().string()) != ".shn") return false;
    return LowerAscii(configuredPath.filename().string()) == LowerAscii(doc.file.FileName());
}
bool ShnProfileMatch(const EditorState::ShnDocument& doc, int profile) {
    const auto& selected = ShnDependencyProfile(profile);
    return std::any_of(selected.rules.begin(), selected.rules.end(),
                       [&](const core::DataDependencyRule& rule) {
                           return ShnDependencyRuleMatchesDocument(rule, doc);
                       });
}
const char* ShnProfileName(int profile) {
    return ShnDependencyProfile(profile).labelDe.data();
}
// Extrahiert einen Integer-Wert aus einer ShnValue, falls die Spalte ein ganzzahliger Typ ist -
// für die Dropdown-Werteliste bei kategorischen Feldern, siehe DrawShnCellEditor.
bool ShnValueAsInt(const core::legacy::ShnValue& v, long long& out) {
    return std::visit([&out](auto&& x) -> bool {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> ||
                      std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::int8_t> ||
                      std::is_same_v<T, std::int16_t> || std::is_same_v<T, std::int32_t>) {
            out = static_cast<long long>(x);
            return true;
        }
        return false;
    }, v);
}
const KnownShnIdFamily* KnownShnFamilyFor(const std::string& fileName) {
    static const std::array<KnownShnIdFamily,3> families = {{
        {"Item",        {"ItemInfo.shn", "ItemInfoServer.shn", "ItemViewInfo.shn"}},
        {"Mob",         {"MobInfo.shn", "MobInfoServer.shn", "MobViewInfo.shn"}},
        {"ActiveSkill", {"ActiveSkill.shn", "ActiveSkillInfoServer.shn", "ActiveSkillView.shn"}},
    }};
    const std::string needle = LowerAscii(fileName);
    for (const auto& family : families) {
        for (const char* member : family.files) {
            if (needle == LowerAscii(member)) return &family;
        }
    }
    return nullptr;
}
int ExactShnColumnIndex(const core::legacy::ShnFile& file, const char* name) {
    for (std::size_t i = 0; i < file.columns.size(); ++i)
        if (file.columns[i].name == name) return static_cast<int>(i);
    return -1;
}
int BestLoadedShnDocument(const EditorState& state, const char* fileName,
                                 EditorState::ShnSource preferredSource, int excludeDocument) {
    int fallback = -1;
    const std::string needle = LowerAscii(fileName);
    for (int i = 0; i < static_cast<int>(state.shnFiles.size()); ++i) {
        if (i == excludeDocument) continue;
        const auto& doc = state.shnFiles[static_cast<std::size_t>(i)];
        if (LowerAscii(doc.file.FileName()) != needle) continue;
        if (doc.source == preferredSource) return i;
        if (fallback < 0) fallback = i;
    }
    return fallback;
}
std::vector<LoadedShnFamilyPeer> BuildKnownShnFamilyPeers(const EditorState& state, int currentDocument) {
    std::vector<LoadedShnFamilyPeer> result;
    if (currentDocument < 0 || currentDocument >= static_cast<int>(state.shnFiles.size())) return result;
    const auto& current = state.shnFiles[static_cast<std::size_t>(currentDocument)];
    const auto* family = KnownShnFamilyFor(current.file.FileName());
    if (!family) return result;

    for (const char* member : family->files) {
        if (LowerAscii(member) == LowerAscii(current.file.FileName())) continue;
        LoadedShnFamilyPeer peer;
        peer.fileName = member;
        peer.document = BestLoadedShnDocument(state, member, current.source, currentDocument);
        if (peer.document >= 0) {
            const auto& peerFile = state.shnFiles[static_cast<std::size_t>(peer.document)].file;
            peer.idColumn = ExactShnColumnIndex(peerFile, "ID");
            if (peer.idColumn >= 0) {
                peer.rowById.reserve(peerFile.rows.size());
                for (std::size_t r = 0; r < peerFile.rows.size(); ++r) {
                    if (static_cast<std::size_t>(peer.idColumn) >= peerFile.rows[r].values.size()) continue;
                    long long id = 0;
                    if (ShnValueAsInt(peerFile.rows[r].values[static_cast<std::size_t>(peer.idColumn)], id)) {
                        const auto [it, inserted] = peer.rowById.emplace(id, static_cast<int>(r));
                        if (!inserted) peer.duplicateIds.insert(id);
                    }
                }
            }
        }
        result.push_back(std::move(peer));
    }
    return result;
}
std::unordered_set<long long> DuplicateIdsInColumn(const core::legacy::ShnFile& file, int idColumn) {
    std::unordered_set<long long> seen;
    std::unordered_set<long long> duplicates;
    if (idColumn < 0) return duplicates;
    seen.reserve(file.rows.size());
    for (const auto& row : file.rows) {
        if (static_cast<std::size_t>(idColumn) >= row.values.size()) continue;
        long long id = 0;
        if (!ShnValueAsInt(row.values[static_cast<std::size_t>(idColumn)], id)) continue;
        if (!seen.insert(id).second) duplicates.insert(id);
    }
    return duplicates;
}
ShnReferenceState KnownShnReferenceState(const std::vector<LoadedShnFamilyPeer>& peers,
                                                long long id, bool currentIdDuplicate) {
    if (currentIdDuplicate) return ShnReferenceState::Ambiguous;
    bool checked = false;
    for (const auto& peer : peers) {
        if (peer.document < 0 || peer.idColumn < 0) continue; // not loaded != broken reference
        checked = true;
        if (!peer.rowById.contains(id)) return ShnReferenceState::Missing;
        if (peer.duplicateIds.contains(id)) return ShnReferenceState::Ambiguous;
    }
    return checked ? ShnReferenceState::Valid : ShnReferenceState::None;
}
std::string KnownShnReferenceTooltip(const std::vector<LoadedShnFamilyPeer>& peers,
                                            long long id) {
    std::string out = L("Verifizierte Familien-ID #","Verified family ID #") + std::to_string(id);
    for (const auto& peer : peers) {
        out += "\n· " + peer.fileName + ": ";
        if (peer.document < 0 || peer.idColumn < 0) out += L("nicht geladen","not loaded");
        else if (!peer.rowById.contains(id)) out += L("FEHLT","MISSING");
        else if (peer.duplicateIds.contains(id)) out += L("mehrdeutig (ID mehrfach)","ambiguous (duplicate ID)");
        else out += L("gefunden","found");
    }
    return out;
}
// Returns true when navigation changed the selected document. Caller must stop drawing the
// current grid in that frame because SelectShnDocument resets grid/filter state.
bool DrawKnownShnFamilyReferenceStrip(EditorState& state, int currentDocument,
                                             const std::vector<LoadedShnFamilyPeer>& peers) {
    if (!state.shnHighlightKnownReferences || currentDocument < 0 ||
        currentDocument >= static_cast<int>(state.shnFiles.size()) ||
        state.shnSelectedRow < 0) return false;

    const auto& current = state.shnFiles[static_cast<std::size_t>(currentDocument)];
    const auto* family = KnownShnFamilyFor(current.file.FileName());
    const int idColumn = ExactShnColumnIndex(current.file, "ID");
    if (!family || idColumn < 0 ||
        state.shnSelectedRow >= static_cast<int>(current.file.rows.size()) ||
        static_cast<std::size_t>(idColumn) >= current.file.rows[static_cast<std::size_t>(state.shnSelectedRow)].values.size())
        return false;

    long long id = 0;
    if (!ShnValueAsInt(current.file.rows[static_cast<std::size_t>(state.shnSelectedRow)]
                           .values[static_cast<std::size_t>(idColumn)], id))
        return false;

    const auto currentDuplicates = DuplicateIdsInColumn(current.file,idColumn);
    const auto refState = KnownShnReferenceState(peers,id,currentDuplicates.contains(id));
    const ImVec4 color = refState == ShnReferenceState::Missing ? UiTheme::Error
                       : refState == ShnReferenceState::Ambiguous ? UiTheme::Warning
                       : refState == ShnReferenceState::Valid ? UiTheme::Success
                                                             : UiTheme::TextSecondary;
    ImGui::TextColored(color, "%s · ID #%lld", family->label, id);
    ImGui::SameLine();
    ImGui::TextDisabled("%s",L("Referenzen:","References:"));

    for (std::size_t i = 0; i < peers.size(); ++i) {
        const auto& peer = peers[i];
        ImGui::SameLine();
        if (peer.document < 0 || peer.idColumn < 0) {
            ImGui::TextDisabled("%s %s", peer.fileName.c_str(), L("(nicht geladen)","(not loaded)"));
            continue;
        }
        const auto found = peer.rowById.find(id);
        if (found == peer.rowById.end()) {
            ImGui::TextColored(UiTheme::Error, "× %s", peer.fileName.c_str());
            continue;
        }
        if (peer.duplicateIds.contains(id) || currentDuplicates.contains(id)) {
            ImGui::TextColored(UiTheme::Warning, "! %s %s", peer.fileName.c_str(),
                               L("(ID mehrfach)","(duplicate ID)"));
            continue;
        }
        ImGui::PushID(static_cast<int>(i));
        if (UI::SmallButton((std::string("» ") + peer.fileName).c_str())) {
            const int row = found->second;
            SelectShnDocument(state, peer.document);
            state.shnSelectedRow = row;
            state.shnSelectedColumn = peer.idColumn;
            state.shnStatus = peer.fileName + L(": Familien-ID #", ": family ID #") +
                              std::to_string(id) + L(" geöffnet."," opened.");
            ImGui::PopID();
            return true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s",L("Passenden Datensatz in dieser verifizierten SHN-Familie öffnen",
                                     "Open matching record in this verified SHN family"));
        ImGui::PopID();
    }
    return false;
}
bool ShnValueAsNumber(const core::legacy::ShnValue& value, long double& out) {
    return std::visit([&](auto&& x) -> bool {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_arithmetic_v<T>) {
            out = static_cast<long double>(x);
            return true;
        }
        return false;
    }, value);
}
int CompareShnValues(const core::legacy::ShnValue& a, const core::legacy::ShnValue& b) {
    long double na = 0.0L, nb = 0.0L;
    if (ShnValueAsNumber(a, na) && ShnValueAsNumber(b, nb)) {
        if (na < nb) return -1;
        if (na > nb) return 1;
        return 0;
    }
    const std::string sa = LowerAscii(core::legacy::ShnValueToString(a));
    const std::string sb = LowerAscii(core::legacy::ShnValueToString(b));
    if (sa < sb) return -1;
    if (sa > sb) return 1;
    return 0;
}
void ClearShnDirtyCells(EditorState::ShnDocument& doc) {
    for (auto& row : doc.cellDirty) std::fill(row.begin(), row.end(), 0);
}
std::expected<void, std::string> ValidateShnPatch(EditorState::ShnDocument& doc) {
    EnsureCellStatusSize(doc);
    if (doc.file.path.empty())
        return std::unexpected("SHN hat keinen Arbeits-/Baseline-Pfad.");

    auto baseline = core::legacy::LoadShnFile(doc.file.path);
    if (!baseline) return std::unexpected("SHN-Baseline konnte nicht geladen werden: " + baseline.error());
    // Zell-Patches, angehängte und gelöschte Zeilen; die Zuordnung zur Platte über rowOrigin.
    return core::legacy::CheckShnPatch(*baseline, doc.file, doc.cellDirty, doc.rowOrigin);
}
std::vector<char> ReadFileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::expected<std::filesystem::path, std::string> ShnProjectTarget(
    const EditorState& state, const EditorState::ShnDocument& doc) {
    const auto side = doc.source == EditorState::ShnSource::Client
        ? core::ProjectOutputSide::Client : core::ProjectOutputSide::Server;
    const auto sourcePath = doc.sourcePath.empty() ? doc.file.path : doc.sourcePath;
    return ProjectPathForSource(state.project, side, sourcePath);
}
bool SaveShnDocument(EditorState& state,int document) {
    if (document < 0 || document >= static_cast<int>(state.shnFiles.size())) return false;
    auto& doc=state.shnFiles[static_cast<std::size_t>(document)];

    auto patch = ValidateShnPatch(doc);
    if (!patch) {
        state.shnStatus = "Speichern blockiert: " + patch.error();
        return false;
    }

    std::filesystem::path target;
    std::string pathError;
    if (!PrepareProjectOutput(ShnProjectTarget(state, doc), target, &pathError)) {
        state.shnStatus = "Speichern blockiert: " + pathError;
        return false;
    }

    // Gekoppelte Gegenseite: dieselbe Tabelle auf der anderen Seite (Client <-> Server). In NA2016
    // sind 73 von 74 doppelt vorhandenen SHN bytegleich (Ausnahme ColorInfo.shn). Sie wird nur
    // mitgeschrieben, wenn ihr aktueller Stand bytegleich zum bisherigen Stand dieser Datei ist
    // und sie keine eigenen ungespeicherten Änderungen hat - sonst nur ein Hinweis.
    int mirror = -1;
    std::string mirrorNote;
    for (int i = 0; i < static_cast<int>(state.shnFiles.size()); ++i) {
        const auto& other = state.shnFiles[static_cast<std::size_t>(i)];
        if (i == document || other.source == doc.source ||
            LowerAscii(other.file.FileName()) != LowerAscii(doc.file.FileName())) continue;
        const std::string sideName = ShnSourceName(other.source);
        if (other.dirty) { mirrorNote = " · " + sideName + "-Kopie hat eigene Änderungen, nicht mitgespeichert"; break; }
        if (ReadFileBytes(other.file.path) == ReadFileBytes(doc.file.path)) mirror = i;
        else mirrorNote = " · " + sideName + "-Kopie weicht ab, nicht mitgespeichert";
        break;
    }

    auto result=core::legacy::SaveShnFile(doc.file,target);
    if (result) {
        doc.file.path=target;
        doc.dirty=false;
        doc.rowOrigin.clear(); // gespeicherter Stand ist die neue Grundlage
        ClearShnDirtyCells(doc);
        ++state.shnEditCounter;
        state.shnStatus=std::string(ShnSourceName(doc.source))+" Projekt-Override gespeichert: "+target.string();
        if (mirror >= 0) {
            auto& other = state.shnFiles[static_cast<std::size_t>(mirror)];
            std::filesystem::path otherTarget;
            std::string otherError;
            if (PrepareProjectOutput(ShnProjectTarget(state, other), otherTarget, &otherError)) {
                if (auto mirrored = core::legacy::SaveShnFile(doc.file, otherTarget)) {
                    other.file = doc.file;
                    other.file.path = otherTarget;
                    other.rowOrigin.clear();
                    other.cellStatus.clear();
                    other.cellDirty.clear();
                    EnsureCellStatusSize(other);
                    state.shnStatus += " · " + std::string(ShnSourceName(other.source)) + "-Kopie gekoppelt mitgespeichert";
                } else {
                    state.shnStatus += " · " + std::string(ShnSourceName(other.source)) + "-Kopie FEHLER: " + mirrored.error();
                }
            } else {
                state.shnStatus += " · " + std::string(ShnSourceName(other.source)) + "-Kopie blockiert: " + otherError;
            }
        }
        state.shnStatus += mirrorNote;
        return true;
    }
    state.shnStatus="Speichern fehlgeschlagen: "+result.error();
    return false;
}
std::pair<std::size_t,std::size_t> SaveAllDirtyShnDocuments(EditorState& state) {
    std::size_t saved = 0, failed = 0;
    for (int i = 0; i < static_cast<int>(state.shnFiles.size()); ++i) {
        if (!state.shnFiles[static_cast<std::size_t>(i)].dirty) continue;
        if (SaveShnDocument(state, i)) ++saved;
        else ++failed;
    }
    return {saved,failed};
}
bool ReloadShnDocument(EditorState& state,int document) {
    if (document < 0 || document >= static_cast<int>(state.shnFiles.size())) return false;
    auto& doc=state.shnFiles[static_cast<std::size_t>(document)];

    // Reload follows the same copy-on-write read rule as initial loading. sourcePath remains
    // the immutable Client/Server identity even after doc.file.path switches to a project
    // working copy. This also picks up an override that was created after the document was
    // first opened instead of blindly re-reading the original installation.
    const auto side = doc.source == EditorState::ShnSource::Client
        ? core::ProjectOutputSide::Client : core::ProjectOutputSide::Server;
    const auto sourcePath = doc.sourcePath.empty() ? doc.file.path : doc.sourcePath;
    const auto path = PreferProjectOverride(state.project, side, sourcePath);
    auto loaded=core::legacy::LoadShnFile(path);
    if (!loaded) {
        state.shnStatus="Neu laden fehlgeschlagen: "+loaded.error();
        return false;
    }
    doc.file=std::move(*loaded);
    doc.dirty=false;
    doc.cellStatus.clear();
    doc.cellDirty.clear();
    doc.rowOrigin.clear();
    EnsureCellStatusSize(doc);
    // Der Verlauf enthält Zeilen-/Zellindizes dieses Dokuments, die nach dem Neuladen nicht mehr gelten.
    state.dataHistory.Clear();
    state.shnSelectedRow=-1;
    state.shnSelectedColumn=-1;
    state.shnInlineEditActive=false;
    state.shnVisibleKey.clear();
    ++state.shnEditCounter;
    state.shnStatus=std::string(ShnSourceName(doc.source))+" neu geladen: "+doc.file.FileName();
    return true;
}
bool ApplyShnCellText(EditorState& state, int document, int row, int column,
                      const std::string& text, bool recordUndo) {
    if (document < 0 || document >= static_cast<int>(state.shnFiles.size())) return false;
    auto& doc = state.shnFiles[static_cast<std::size_t>(document)];
    auto& file = doc.file;
    if (row < 0 || row >= static_cast<int>(file.rows.size()) ||
        column < 0 || column >= static_cast<int>(file.columns.size()) ||
        column >= static_cast<int>(file.rows[static_cast<std::size_t>(row)].values.size())) return false;

    auto parsed = core::legacy::ParseShnValue(file.columns[static_cast<std::size_t>(column)], text);
    if (!parsed) {
        state.shnStatus = parsed.error();
        return false;
    }

    auto& value = file.rows[static_cast<std::size_t>(row)].values[static_cast<std::size_t>(column)];
    if (value == *parsed) return true;

    core::legacy::ShnValue before = value;
    value = std::move(*parsed);
    if (recordUndo) RecordShnCellChange(state, document, row, column, std::move(before), value);
    doc.dirty = true;
    EnsureCellStatusSize(doc);
    doc.cellStatus[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = kShnCellNormal;
    doc.cellDirty[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = 1;
    ++state.shnEditCounter;
    state.shnVisibleKey.clear();
    state.shnStatus = std::string(ShnSourceName(doc.source)) + ": Zelle geändert (noch nicht gespeichert).";
    return true;
}
void UndoShnCellEdit(EditorState& state) { DataUndo(state); }
void RedoShnCellEdit(EditorState& state) { DataRedo(state); }
void CopySelectedShnCell(EditorState& state) {
    if (state.shnSelectedFile < 0 || state.shnSelectedFile >= static_cast<int>(state.shnFiles.size())) return;
    const auto& file = state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)].file;
    if (state.shnSelectedRow < 0 || state.shnSelectedRow >= static_cast<int>(file.rows.size()) ||
        state.shnSelectedColumn < 0 || state.shnSelectedColumn >= static_cast<int>(file.columns.size())) return;
    const auto& value = file.rows[static_cast<std::size_t>(state.shnSelectedRow)]
                            .values[static_cast<std::size_t>(state.shnSelectedColumn)];
    const std::string text = core::legacy::ShnValueToString(value);
    ImGui::SetClipboardText(text.c_str());
    state.shnStatus = "SHN-Zelle kopiert.";
}
void PasteSelectedShnCell(EditorState& state) {
    const char* text = ImGui::GetClipboardText();
    if (!text) return;
    ApplyShnCellText(state, state.shnSelectedFile, state.shnSelectedRow, state.shnSelectedColumn, text);
}
void StartShnInlineEdit(EditorState& state, int row, int column) {
    if (state.shnSelectedFile < 0 || state.shnSelectedFile >= static_cast<int>(state.shnFiles.size())) return;
    auto& file = state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)].file;
    if (row < 0 || row >= static_cast<int>(file.rows.size()) ||
        column < 0 || column >= static_cast<int>(file.columns.size())) return;
    state.shnSelectedRow = row;
    state.shnSelectedColumn = column;
    state.shnEditBuffer = core::legacy::ShnValueToString(
        file.rows[static_cast<std::size_t>(row)].values[static_cast<std::size_t>(column)]);
    state.shnInlineEditActive = true;
    state.shnInlineEditFocusPending = true;
    state.shnEditPopupOpen = false;
}
void DrawShnCellEditor(EditorState& state) {
    if (!state.shnEditPopupOpen || state.shnSelectedFile < 0 || state.shnSelectedFile >= static_cast<int>(state.shnFiles.size())) return;
    auto& doc = state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)];
    auto& file = doc.file;
    if (state.shnSelectedRow < 0 || state.shnSelectedRow >= static_cast<int>(file.rows.size()) ||
        state.shnSelectedColumn < 0 || state.shnSelectedColumn >= static_cast<int>(file.columns.size())) return;
    const auto& column = file.columns[static_cast<std::size_t>(state.shnSelectedColumn)];
    ImGui::OpenPopup("SHN-Zelle bearbeiten");
    if (ImGui::BeginPopupModal("SHN-Zelle bearbeiten", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", column.name.c_str());
        ImGui::TextDisabled("%s | Zeile: %d", file.TypeName(column).c_str(), state.shnSelectedRow);

        // Dropdown für kategorische Felder (Mockup-Wunsch "dropdown Menü ... was es sein soll"):
        // wenn die Spalte über das GESAMTE Dokument hinweg nur wenige unterschiedliche Werte
        // annimmt, ist sie sehr wahrscheinlich ein Enum/eine Kategorie (z.B. ItemInfo.Class/
        // UseClass/Type/Equip) statt eines freien Zahlenfelds - dann eine Auswahl der
        // TATSÄCHLICH beobachteten Werte anbieten statt Freitext-Zahleneingabe. Rein aus den
        // Daten abgeleitet, KEINE erfundene Klartext-Bedeutung der Zahlencodes (welche Zahl
        // z.B. "Schwert" bedeutet, ist nicht bekannt/dokumentiert).
        std::vector<long long> distinctValues;
        if (column.kind != core::legacy::ShnValueKind::String && column.kind != core::legacy::ShnValueKind::Float &&
            column.kind != core::legacy::ShnValueKind::Raw && column.kind != core::legacy::ShnValueKind::PairUInt32) {
            std::set<long long> seen;
            for (auto& row : file.rows) {
                if (state.shnSelectedColumn >= static_cast<int>(row.values.size())) continue;
                long long v;
                if (ShnValueAsInt(row.values[static_cast<std::size_t>(state.shnSelectedColumn)], v)) seen.insert(v);
                if (seen.size() > 24) break; // eindeutig kein Enum mehr - Freitext bleibt die richtige Wahl
            }
            if (seen.size() >= 2 && seen.size() <= 24) distinctValues.assign(seen.begin(), seen.end());
        }

        ImGui::SetNextItemWidth(520.0f);
        // WICHTIG: der Puffer muss GROSS genug sein - ImGui laesst nie mehr Zeichen zu als die
        // Puffergroesse. Frueher war er genau "aktueller Text + 1" gross: bei dem Wert "0" liess
        // sich nur EINE Ziffer eingeben, bei jedem Wert nie ein laengerer als vorher (CHANGELOG
        // [0.44.28]). Das war auch die eigentliche Ursache von "SHN-Zellen nicht bearbeitbar".
        auto apply = [&]() {
            if (ApplyShnCellText(state, state.shnSelectedFile, state.shnSelectedRow,
                                 state.shnSelectedColumn, state.shnEditBuffer)) {
                state.shnEditPopupOpen = false;
                ImGui::CloseCurrentPopup();
            }
        };

        if (!distinctValues.empty()) {
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::BeginCombo("##shnEnumDropdown", "Beobachteter Wert wählen...")) {
                for (long long v : distinctValues) {
                    std::string vs = std::to_string(v);
                    if (UI::Selectable(vs.c_str(), state.shnEditBuffer == vs)) state.shnEditBuffer = vs;
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu Werte im Dokument beobachtet)", distinctValues.size());
        }

        // Puffer erst HIER bauen (nach dem Dropdown, das shnEditBuffer setzen kann) und den Text nach
        // JEDEM Aufruf zurueck in den Zustand schreiben. Frueher wurde er nur bei Enter uebernommen -
        // "Uebernehmen" wendete dann den ALTEN Text an (CHANGELOG [0.44.29]).
        std::vector<char> buf(state.shnEditBuffer.begin(), state.shnEditBuffer.end());
        buf.resize(std::max<std::size_t>(buf.size() + 1, 1024), '\0');
        const bool enterPressed = UI::InputText("##shncell", buf.data(), buf.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        state.shnEditBuffer.assign(buf.data());
        if (enterPressed) apply();
        ImGui::SameLine(); if (UI::Button("Übernehmen")) { apply(); }
        ImGui::SameLine(); if (UI::Button("Abbrechen")) { state.shnEditPopupOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}
// Zellstatus-Konstanten (siehe oben, vor DrawShnCellEditor definiert) - hier nur die
// zugehörigen Hilfsfunktionen.
// Vergrößert cellStatus bei Bedarf auf die aktuelle Zeilen-/Spaltenzahl (neue Felder = normal).
void EnsureCellStatusSize(EditorState::ShnDocument& doc) {
    doc.cellStatus.resize(doc.file.rows.size());
    doc.cellDirty.resize(doc.file.rows.size());
    for (std::size_t i = 0; i < doc.file.rows.size(); ++i) {
        doc.cellStatus[i].resize(doc.file.columns.size(), kShnCellNormal);
        doc.cellDirty[i].resize(doc.file.columns.size(), 0);
    }
    // Außerhalb des Verlaufs angehängte Zeilen (z. B. Familien-Propagation) sind neu.
    if (!doc.rowOrigin.empty() && doc.rowOrigin.size() < doc.file.rows.size())
        doc.rowOrigin.resize(doc.file.rows.size(), -1);
}
// Liefert einen sinnvollen Default-Text für ParseShnValue, passend zum Spaltentyp - für neu
// angelegte Zeilen (siehe AddRowWithPropagation). Raw braucht exakt column.length Hex-Bytes.
std::string DefaultShnValueText(const core::legacy::ShnColumn& column) {
    using core::legacy::ShnValueKind;
    switch (column.kind) {
        case ShnValueKind::Float: return "0.0";
        case ShnValueKind::String: return "";
        case ShnValueKind::PairUInt32: return "0:0";
        case ShnValueKind::Raw: {
            std::string s;
            for (std::uint32_t i = 0; i < column.length; ++i) { if (i) s += ' '; s += "00"; }
            return s;
        }
        default: return "0";
    }
}
void DrawShnGrid(EditorState& state) {
    if (state.shnSelectedFile < 0 || state.shnSelectedFile >= static_cast<int>(state.shnFiles.size())) {
        ImGui::TextDisabled("Keine SHN-Datei geöffnet.");
        return;
    }

    auto& doc = state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)];
    auto& file = doc.file;
    EnsureCellStatusSize(doc);

    if (state.shnColumnFilterFile != state.shnSelectedFile ||
        state.shnColumnFilters.size() != file.columns.size()) {
        state.shnColumnFilterFile = state.shnSelectedFile;
        state.shnColumnFilters.assign(file.columns.size(), {});
        state.shnVisibleKey.clear();
    }

    const int counterpartIndex = FindShnCounterpart(state, state.shnSelectedFile);
    const core::legacy::ShnFile* counterpart =
        counterpartIndex >= 0 ? &state.shnFiles[static_cast<std::size_t>(counterpartIndex)].file : nullptr;
    const std::vector<int> counterpartColumns =
        counterpart ? MatchShnColumnsByName(file, *counterpart) : std::vector<int>(file.columns.size(), -1);

    const auto* knownFamily = KnownShnFamilyFor(file.FileName());
    const int familyIdColumn = knownFamily ? ExactShnColumnIndex(file, "ID") : -1;
    const auto familyPeers = knownFamily ? BuildKnownShnFamilyPeers(state, state.shnSelectedFile)
                                         : std::vector<LoadedShnFamilyPeer>{};
    const auto familyDuplicateIds = familyIdColumn >= 0 ? DuplicateIdsInColumn(file, familyIdColumn)
                                                        : std::unordered_set<long long>{};

    ImGui::TextColored(ShnSourceColor(doc.source), "%s", ShnSourceName(doc.source));
    ImGui::SameLine();
    ImGui::Text("%s", file.FileName().c_str());
    if (doc.dirty) {
        std::size_t dirtyCells=0;
        for (const auto& row:doc.cellDirty)
            dirtyCells+=static_cast<std::size_t>(std::count_if(row.begin(),row.end(),[](std::uint8_t v){return v!=0;}));
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f),
                           L("* geändert · %zu Zellen","* modified · %zu cells"),dirtyCells);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu Zeilen · %zu Spalten · V%u · %s",
                        file.rows.size(), file.columns.size(), file.version,
                        file.encrypted ? "verschlüsselt" : "raw");

    ImGui::BeginDisabled(!doc.dirty);
    if (UI::Button(L("Speichern##shnGrid","Save##shnGrid"))) SaveShnDocument(state,state.shnSelectedFile);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (UI::Button("+ Neue Zeile")) AddRowWithPropagation(state, state.shnSelectedFile);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Legt eine neue Zeile an. Automatische Propagation erfolgt ausschließlich in verifizierte Item/Mob/ActiveSkill-ID-Familien; heuristische Abhängigkeiten werden nur angezeigt.");
    ImGui::SameLine();
    ImGui::BeginDisabled(state.shnSelectedRow < 0);
    if (UI::Button(L("Zeile löschen","Delete row"))) RequestShnRowDelete(state, state.shnSelectedFile, state.shnSelectedRow);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", L("Löscht die gewählte Zeile (Entf). Bei Item/Mob/ActiveSkill dieselbe ID in allen geladenen "
                                  "Familien-Tabellen beider Seiten. Zeigt vorher Verweise aus geladenen Tabellen; Strg+Z macht es rückgängig.",
                                  "Deletes the selected row (Del). For Item/Mob/ActiveSkill also the same ID in every loaded "
                                  "family table on both sides. Shows references from loaded tables first; Ctrl+Z undoes it."));
    ImGui::SameLine();
    ImGui::BeginDisabled(state.shnSelectedRow < 0 || state.shnSelectedColumn < 0);
    if (UI::Button("Kopieren")) CopySelectedShnCell(state);
    ImGui::SameLine();
    if (UI::Button("Einfügen")) PasteSelectedShnCell(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.dataHistory.CanUndo());
    if (UI::Button("Undo")) UndoShnCellEdit(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.dataHistory.CanRedo());
    if (UI::Button("Redo")) RedoShnCellEdit(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool anyColumnFilter = std::any_of(state.shnColumnFilters.begin(),state.shnColumnFilters.end(),
        [](const auto& f){ return f[0] != '\0'; });
    ImGui::BeginDisabled(state.shnSearch[0] == '\0' && !anyColumnFilter);
    if (UI::SmallButton(L("Filter löschen##shn","Clear filters##shn"))) {
        state.shnSearch[0] = '\0';
        state.shnFilterActive = false;
        for (auto& filter : state.shnColumnFilters) filter[0] = '\0';
        state.shnVisibleKey.clear();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Doppelklick/F2: Inline · Rechtsklick: Optionen");
    if (counterpart) {
        ImGui::SameLine();
        UI::Checkbox(L("Client/Server Unterschiede##shnDiff","Client/server differences##shnDiff"),
                     &state.shnHighlightClientServerDiff);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s",L("Orange Schrift = gleicher Dateiname auf der Gegenseite, aber anderer Zellwert.",
                                     "Orange text = same file on the other side, but a different cell value."));
    }
    if (knownFamily && familyIdColumn >= 0) {
        ImGui::SameLine();
        UI::Checkbox(L("Familien-Referenzen##shnRefs","Family references##shnRefs"),
                     &state.shnHighlightKnownReferences);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s",L("Grün = ID in allen geladenen, verifizierten Familienmitgliedern vorhanden. Rot = ID fehlt. Gelb = ID ist mehrfach und daher als Cross-Link mehrdeutig.",
                                     "Green = ID exists in every loaded, verified family member. Red = ID is missing. Amber = duplicate ID, so the cross-link is ambiguous."));
    }

    if (DrawKnownShnFamilyReferenceStrip(state, state.shnSelectedFile, familyPeers))
        return;

    ImGui::Separator();
    const std::string needle = LowerAscii(state.shnSearch);
    ImGui::BeginChild("##shnGrid", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ShortcutPressed(state.shortcutSave) && doc.dirty) SaveShnDocument(state,state.shnSelectedFile);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) CopySelectedShnCell(state);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) PasteSelectedShnCell(state);
        // Strg+Z/Strg+Y übernimmt HandleGlobalShortcuts (gemeinsamer Daten-Verlauf).
        if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && state.shnSelectedRow >= 0)
            RequestShnRowDelete(state, state.shnSelectedFile, state.shnSelectedRow);
        if ((ImGui::IsKeyPressed(ImGuiKey_F2, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false)) &&
            state.shnSelectedRow >= 0 && state.shnSelectedColumn >= 0) {
            const auto kind = file.columns[static_cast<std::size_t>(state.shnSelectedColumn)].kind;
            if (kind == core::legacy::ShnValueKind::Raw || kind == core::legacy::ShnValueKind::PairUInt32) {
                state.shnEditBuffer = core::legacy::ShnValueToString(
                    file.rows[static_cast<std::size_t>(state.shnSelectedRow)]
                        .values[static_cast<std::size_t>(state.shnSelectedColumn)]);
                state.shnEditPopupOpen = true;
            } else {
                StartShnInlineEdit(state, state.shnSelectedRow, state.shnSelectedColumn);
            }
        }
    }

    const int visibleCols = static_cast<int>(file.columns.size());
    const ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti;

    if (visibleCols > 0 && ImGui::BeginTable("##shnTable", visibleCols + 1, flags, ImVec2(0,0))) {
        ImGui::TableSetupScrollFreeze(1, 2);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 55.0f);
        for (const auto& c : file.columns) {
            ImGui::TableSetupColumn(c.name.c_str(),
                                    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortAscending,
                                    std::max(100.0f, std::min(260.0f, 14.0f * static_cast<float>(c.name.size()+2))));
        }
        ImGui::TableHeadersRow();

        // Eine kompakte Filterzeile direkt unter den Headern: jeder nicht-leere Filter wird
        // UND-verknüpft. Damit lassen sich große Tabellen ohne zusätzliche Dialoge eingrenzen.
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Filter");
        for (std::size_t ci = 0; ci < file.columns.size(); ++ci) {
            ImGui::TableSetColumnIndex(static_cast<int>(ci + 1));
            ImGui::PushID(static_cast<int>(ci));
            ImGui::SetNextItemWidth(-1.0f);
            if (UI::InputTextWithHint("##columnFilter", "...",
                                      state.shnColumnFilters[ci].data(),
                                      state.shnColumnFilters[ci].size())) {
                state.shnVisibleKey.clear();
            }
            ImGui::PopID();
        }

        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0) {
            const auto& spec = specs->Specs[0];
            const int column = spec.ColumnIndex - 1;
            if (column >= 0 && column < static_cast<int>(file.columns.size())) {
                state.shnSortColumn = column;
                state.shnSortAscending = spec.SortDirection != ImGuiSortDirection_Descending;
            } else {
                state.shnSortColumn = -1;
            }
            specs->SpecsDirty = false;
        }

        std::string columnFilterKey;
        for (std::size_t ci = 0; ci < state.shnColumnFilters.size(); ++ci) {
            if (state.shnColumnFilters[ci][0] == '\0') continue;
            columnFilterKey += "|" + std::to_string(ci) + "=" + LowerAscii(state.shnColumnFilters[ci].data());
        }

        const std::string visibleKey =
            std::to_string(state.shnSelectedFile) + "|" + needle + "|" +
            (state.shnSearchColumns ? "c" : "-") + (state.shnSearchValues ? "v" : "-") +
            (state.shnFilterActive ? "f" : "-") + "|" + std::to_string(file.rows.size()) + "|" +
            std::to_string(state.shnEditCounter) + "|sort=" + std::to_string(state.shnSortColumn) +
            (state.shnSortAscending ? "a" : "d") + columnFilterKey;

        if (visibleKey != state.shnVisibleKey) {
            state.shnVisibleKey = visibleKey;
            state.shnVisibleRows.clear();
            for (std::size_t ri = 0; ri < file.rows.size(); ++ri) {
                bool matches = needle.empty();
                if (!matches) {
                    const auto& row = file.rows[ri];
                    for (std::size_t ci = 0; ci < row.values.size(); ++ci) {
                        if (state.shnSearchColumns && LowerAscii(file.columns[ci].name).find(needle) != std::string::npos) {
                            matches = true;
                            break;
                        }
                        if (state.shnSearchValues &&
                            LowerAscii(core::legacy::ShnValueToString(row.values[ci])).find(needle) != std::string::npos) {
                            matches = true;
                            break;
                        }
                    }
                }
                if (state.shnFilterActive && !matches) continue;

                bool columnFiltersMatch = true;
                for (std::size_t ci = 0; ci < state.shnColumnFilters.size(); ++ci) {
                    if (state.shnColumnFilters[ci][0] == '\0') continue;
                    if (ci >= file.rows[ri].values.size()) { columnFiltersMatch = false; break; }
                    const std::string cell = LowerAscii(core::legacy::ShnValueToString(file.rows[ri].values[ci]));
                    const std::string filter = LowerAscii(state.shnColumnFilters[ci].data());
                    if (cell.find(filter) == std::string::npos) { columnFiltersMatch = false; break; }
                }
                if (!columnFiltersMatch) continue;
                state.shnVisibleRows.push_back(ri);
            }

            if (state.shnSortColumn >= 0 &&
                state.shnSortColumn < static_cast<int>(file.columns.size())) {
                const std::size_t sortColumn = static_cast<std::size_t>(state.shnSortColumn);
                std::stable_sort(state.shnVisibleRows.begin(), state.shnVisibleRows.end(),
                    [&](std::size_t a, std::size_t b) {
                        if (sortColumn >= file.rows[a].values.size() || sortColumn >= file.rows[b].values.size())
                            return a < b;
                        const int cmp = CompareShnValues(file.rows[a].values[sortColumn],
                                                         file.rows[b].values[sortColumn]);
                        if (cmp == 0) return a < b;
                        return state.shnSortAscending ? cmp < 0 : cmp > 0;
                    });
            }
        }

        if (state.shnFilterActive || !columnFilterKey.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(UiTheme::AccentCyan,"%zu/%zu",
                               state.shnVisibleRows.size(),file.rows.size());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s",L("gefilterte Zeilen","filtered rows"));
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(state.shnVisibleRows.size()));
        while (clipper.Step()) {
            for (int vi = clipper.DisplayStart; vi < clipper.DisplayEnd; ++vi) {
                const std::size_t ri = state.shnVisibleRows[static_cast<std::size_t>(vi)];
                auto& row = file.rows[ri];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", ri);

                for (std::size_t ci = 0; ci < row.values.size(); ++ci) {
                    ImGui::TableSetColumnIndex(static_cast<int>(ci + 1));

                    const std::uint8_t status = ri < doc.cellStatus.size() && ci < doc.cellStatus[ri].size()
                        ? doc.cellStatus[ri][ci] : kShnCellNormal;
                    const bool dirtyCell = ri < doc.cellDirty.size() && ci < doc.cellDirty[ri].size() &&
                                           doc.cellDirty[ri][ci] != 0;
                    ShnReferenceState referenceState = ShnReferenceState::None;
                    long long referenceId = 0;
                    if (state.shnHighlightKnownReferences && static_cast<int>(ci) == familyIdColumn &&
                        ShnValueAsInt(row.values[ci], referenceId)) {
                        referenceState = KnownShnReferenceState(
                            familyPeers, referenceId, familyDuplicateIds.contains(referenceId));
                    }
                    if (status == kShnCellAutoFilled)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(30, 90, 40, 160));
                    else if (status == kShnCellNeedsInput)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(110, 30, 30, 160));
                    else if (dirtyCell)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(20, 92, 140, 125));
                    else if (referenceState == ShnReferenceState::Missing)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(120, 32, 42, 175));
                    else if (referenceState == ShnReferenceState::Valid)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(24, 86, 55, 125));
                    else if (referenceState == ShnReferenceState::Ambiguous)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(115, 78, 24, 145));

                    const bool selected =
                        state.shnSelectedRow == static_cast<int>(ri) &&
                        state.shnSelectedColumn == static_cast<int>(ci);
                    const bool inlineEditing = selected && state.shnInlineEditActive;

                    ImGui::PushID(static_cast<int>(ri));
                    ImGui::PushID(static_cast<int>(ci));

                    bool counterpartDiff = false;
                    std::string counterpartText;
                    if (state.shnHighlightClientServerDiff && counterpart &&
                        ci < counterpartColumns.size() && counterpartColumns[ci] >= 0 &&
                        ri < counterpart->rows.size()) {
                        const std::size_t peerColumn = static_cast<std::size_t>(counterpartColumns[ci]);
                        if (peerColumn < counterpart->rows[ri].values.size()) {
                            const auto& peerValue = counterpart->rows[ri].values[peerColumn];
                            counterpartDiff = row.values[ci] != peerValue;
                            if (counterpartDiff) counterpartText = core::legacy::ShnValueToString(peerValue);
                        }
                    }
                    if (counterpartDiff)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f,0.70f,0.25f,1.0f));

                    if (inlineEditing) {
                        std::vector<char> buf(state.shnEditBuffer.begin(), state.shnEditBuffer.end());
                        buf.resize(std::max<std::size_t>(buf.size() + 1, 1024), '\0');
                        if (state.shnInlineEditFocusPending) {
                            ImGui::SetKeyboardFocusHere();
                            state.shnInlineEditFocusPending = false;
                        }
                        ImGui::SetNextItemWidth(-1.0f);
                        const bool enter = UI::InputText("##inlineShnCell", buf.data(), buf.size(),
                                                         ImGuiInputTextFlags_EnterReturnsTrue |
                                                         ImGuiInputTextFlags_AutoSelectAll);
                        state.shnEditBuffer.assign(buf.data());

                        const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
                        const bool commit = enter || ImGui::IsItemDeactivatedAfterEdit();
                        if (cancel) {
                            state.shnInlineEditActive = false;
                            state.shnInlineEditFocusPending = false;
                        } else if (commit) {
                            if (ApplyShnCellText(state, state.shnSelectedFile,
                                                 static_cast<int>(ri), static_cast<int>(ci),
                                                 state.shnEditBuffer)) {
                                state.shnInlineEditActive = false;
                                state.shnInlineEditFocusPending = false;
                            }
                        }
                    } else {
                        const std::string label = ShnShortValue(row.values[ci]);
                        if (UI::Selectable((label + "##value").c_str(), selected,
                                           ImGuiSelectableFlags_AllowDoubleClick)) {
                            state.shnSelectedRow = static_cast<int>(ri);
                            state.shnSelectedColumn = static_cast<int>(ci);
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                const auto kind = file.columns[ci].kind;
                                if (kind == core::legacy::ShnValueKind::Raw ||
                                    kind == core::legacy::ShnValueKind::PairUInt32) {
                                    state.shnEditBuffer = core::legacy::ShnValueToString(row.values[ci]);
                                    state.shnEditPopupOpen = true;
                                } else {
                                    StartShnInlineEdit(state, static_cast<int>(ri), static_cast<int>(ci));
                                }
                            }
                        }

                        if (ImGui::BeginPopupContextItem("##shnCellContext")) {
                            if (!selected) {
                                state.shnSelectedRow = static_cast<int>(ri);
                                state.shnSelectedColumn = static_cast<int>(ci);
                            }
                            const std::string shnCellMeta =
                                file.columns[ci].name + " · " +
                                std::string(L("Zeile ","Row ")) + std::to_string(ri);
                            DrawContextMenuHeader("shnCellContextHeader",L("SHN-ZELLE","SHN CELL"),
                                                  DrawIconTable,"module.shn.single",shnCellMeta.c_str());
                            if (UI::MenuItem(L("Inline bearbeiten","Inline edit"), "F2")) {
                                StartShnInlineEdit(state, static_cast<int>(ri), static_cast<int>(ci));
                            }
                            if (UI::MenuItem(L("Erweitert bearbeiten...","Advanced edit..."))) {
                                state.shnEditBuffer = core::legacy::ShnValueToString(row.values[ci]);
                                state.shnEditPopupOpen = true;
                            }
                            ImGui::Separator();
                            if (UI::MenuItem(L("Kopieren","Copy"), L("Strg+C","Ctrl+C"))) CopySelectedShnCell(state);
                            if (UI::MenuItem(L("Einfügen","Paste"), L("Strg+V","Ctrl+V"))) PasteSelectedShnCell(state);
                            ImGui::EndPopup();
                        }

                        if (ImGui::IsItemHovered()) {
                            std::string tooltip = core::legacy::ShnValueToString(row.values[ci]) +
                                                  "\n" + L("Typ: ","Type: ") + file.TypeName(file.columns[ci]);
                            if (counterpartDiff)
                                tooltip += "\n" + std::string(L("Gegenstück: ","Counterpart: ")) + counterpartText;
                            if (referenceState != ShnReferenceState::None)
                                tooltip += "\n" + KnownShnReferenceTooltip(familyPeers, referenceId);
                            tooltip += "\n" + std::string(L("Doppelklick/F2 = Inline bearbeiten",
                                                             "Double-click/F2 = inline edit"));
                            ImGui::SetTooltip("%s", tooltip.c_str());
                        }
                    }

                    if (counterpartDiff) ImGui::PopStyleColor();
                    ImGui::PopID();
                    ImGui::PopID();
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
    DrawShnCellEditor(state);
}
// Trennt bekannte Namens-Suffixe ab, um Datei-Familien zu erkennen (z.B. "ItemInfo",
// "ItemInfoServer", "ItemViewInfo" -> alle Stamm "Item") - siehe FindDependencyPeers unten.
// Reihenfolge wichtig: längste Suffixe zuerst prüfen (sonst würde "InfoServer" nie erkannt,
// da schon "Info" vorher passt).
std::string ShnNameStem(const std::string& fileNameNoExt) {
    static const char* kSuffixes[] = {
        "InfoServer", "ViewInfo", "Info", "View", "Server", "Desc", "Group", "Data",
        "List", "Rate", "Count", "Type", "State"
    };
    for (const char* suf : kSuffixes) {
        std::size_t sufLen = std::strlen(suf);
        if (fileNameNoExt.size() > sufLen &&
            fileNameNoExt.compare(fileNameNoExt.size() - sufLen, sufLen, suf) == 0) {
            return fileNameNoExt.substr(0, fileNameNoExt.size() - sufLen);
        }
    }
    return fileNameNoExt;
}
// Liefert die Indizes ALLER anderen geladenen SHN-Dokumente mit exakt derselben Zeilenanzahl
// wie das Dokument bei docIndex - ein einfaches, aber empirisch bestätigtes Signal für
// "Datei-Paare", die im Gleichschritt gepflegt werden müssen (siehe CHANGELOG [0.44.13]:
// gegen die echten NA2016-Client/Server-SHN-Dateien geprüft - ItemInfo.shn, ItemInfoServer.shn
// UND ItemViewInfo.shn haben z.B. alle exakt 14999 Zeilen; ein neues Item ohne passende Zeile
// in allen dreien wäre inkonsistent). Schwelle >20 Zeilen, um zufällige Übereinstimmungen
// kleiner Lookup-Tabellen (oft nur wenige Zeilen) nicht fälschlich als Abhängigkeit zu werten.
// Bewusst NUR Zeilenanzahl als Signal (kein Werte-Abgleich) - ein Test mit Werte-Überlappung
// gegen ItemInfo.ID ergab bei ~15000 dichten IDs zu viele Zufallstreffer (>500 falsche
// Positive über verschiedenste Stat-Spalten), um verlässlich zu sein.
std::vector<int> FindRowCountPeers(const EditorState& state, int docIndex) {
    std::vector<int> peers;
    if (docIndex < 0 || docIndex >= static_cast<int>(state.shnFiles.size())) return peers;
    const auto& doc = state.shnFiles[static_cast<std::size_t>(docIndex)];
    if (doc.file.rows.size() <= 20) return peers;
    for (std::size_t i = 0; i < state.shnFiles.size(); ++i) {
        if (static_cast<int>(i) == docIndex) continue;
        if (state.shnFiles[i].file.rows.size() == doc.file.rows.size()) {
            peers.push_back(static_cast<int>(i));
        }
    }
    return peers;
}
// Liefert die Indizes anderer geladener Dokumente mit demselben Namens-STAMM (nach Abtrennen
// von Info/InfoServer/View/ViewInfo/Server/Desc/... - siehe ShnNameStem) - deutlich
// zuverlässiger als reiner Zeilenanzahl-Abgleich, siehe CHANGELOG [0.44.14]: gegen alle 350
// echten NA2016-SHN-Dateien geprüft, findet z.B. auch Teilmengen-Beziehungen wie
// ItemShop.shn (3930 Zeilen) / ItemShopView.shn (3559 Zeilen), wo die Zeilenanzahl bewusst
// NICHT übereinstimmt (nicht jeder Shop-Eintrag hat einen View-Eintrag). docs/
// SHN_DEPENDENCIES.md enthält die vollständige, gegen die echten Daten verifizierte Liste
// aller so gefundenen Familien.
std::vector<int> FindNameStemPeers(const EditorState& state, int docIndex) {
    std::vector<int> peers;
    if (docIndex < 0 || docIndex >= static_cast<int>(state.shnFiles.size())) return peers;
    auto stemOf = [](const std::string& fileName) {
        std::string base = fileName;
        if (base.size() > 4 && base.compare(base.size() - 4, 4, ".shn") == 0) base.resize(base.size() - 4);
        return ShnNameStem(base);
    };
    const std::string myStem = stemOf(state.shnFiles[static_cast<std::size_t>(docIndex)].file.FileName());
    if (myStem.size() < 3) return peers; // zu kurzer Stamm - zu unspezifisch, zu viele Zufallstreffer
    for (std::size_t i = 0; i < state.shnFiles.size(); ++i) {
        if (static_cast<int>(i) == docIndex) continue;
        if (stemOf(state.shnFiles[i].file.FileName()) == myStem) {
            peers.push_back(static_cast<int>(i));
        }
    }
    return peers;
}
// Vereinigt FindRowCountPeers und FindNameStemPeers (dedupliziert) - die eigentliche, im
// Datei-Browser genutzte Abhängigkeits-Erkennung, siehe DrawShnSourceList.
std::vector<int> FindDependencyPeers(const EditorState& state, int docIndex) {
    auto peers = FindRowCountPeers(state, docIndex);
    for (int p : FindNameStemPeers(state, docIndex)) {
        if (std::find(peers.begin(), peers.end(), p) == peers.end()) peers.push_back(p);
    }
    return peers;
}
// Spaltenindex per Name (-1, wenn nicht vorhanden).
int ShnColumnIndexByName(const core::legacy::ShnFile& f, const std::string& name) {
    for (std::size_t i = 0; i < f.columns.size(); ++i) if (f.columns[i].name == name) return static_cast<int>(i);
    return -1;
}
// Sucht die Spalte, die eine Zeile identifiziert: "ID" bzw. "Index" (Ganzzahl). -1, wenn keine.
int FindShnIdColumn(const core::legacy::ShnFile& f) {
    for (const char* wanted : {"ID", "Index"}) {
        for (std::size_t i = 0; i < f.columns.size(); ++i) {
            const auto& c = f.columns[i];
            if (c.name != wanted) continue;
            using core::legacy::ShnValueKind;
            if (c.kind == ShnValueKind::String || c.kind == ShnValueKind::Float || c.kind == ShnValueKind::Raw ||
                c.kind == ShnValueKind::PairUInt32) continue;
            return static_cast<int>(i);
        }
    }
    return -1;
}
// Groesster vorhandener Wert einer Ganzzahlspalte (-1 bei leerer Datei).
long long ShnMaxInt(const core::legacy::ShnFile& f, int col) {
    long long best = -1;
    for (const auto& row : f.rows) {
        long long v = 0;
        if (col >= 0 && static_cast<std::size_t>(col) < row.values.size() && ShnValueAsInt(row.values[static_cast<std::size_t>(col)], v)) best = std::max(best, v);
    }
    return best;
}
// Belegte Ganzzahl-Werte einer Spalte (alle Zeilen).
void CollectShnInts(const core::legacy::ShnFile& f, int col, std::set<long long>& out) {
    if (col < 0) return;
    for (const auto& row : f.rows) {
        long long v = 0;
        if (static_cast<std::size_t>(col) < row.values.size() && ShnValueAsInt(row.values[static_cast<std::size_t>(col)], v)) out.insert(v);
    }
}
// Freie, "passende" ID: die ERSTE ID im GROESSTEN zusammenhaengenden freien Block des Typbereichs.
// Reale Dateien haben verstreute ID-Bloecke (ItemInfo: 0-24999 dicht, dann Cash-Shop-Bloecke bis
// 65504; MobInfo: 0-15035 dicht, 50000-50007 Sonderwerte) - "hoechster Wert + 1" landete dort in
// Sonderbereichen oder am Typende (ItemInfo: 65505, nur 30 Plaetze uebrig). Ein grosser freier
// Block ist unbenutztes Gelaende; bei jeder weiteren neuen Zeile rueckt die ID um 1 vor.
// `used` = belegte IDs (Quelldatei + Familien-Dateien). Gibt -1 zurueck, wenn nichts gefunden wurde.
long long SuggestFreeShnId(const core::legacy::ShnColumn& column, const std::set<long long>& used) {
    // Groesster im Spaltentyp darstellbarer Wert (per ParseShnValue-Probe, unabhaengig von Typcodes).
    long long typeMax = 0;
    for (long long probe : {4294967295LL, 2147483647LL, 65535LL, 32767LL, 255LL, 127LL}) {
        if (core::legacy::ParseShnValue(column, std::to_string(probe))) { typeMax = probe; break; }
    }
    if (typeMax <= 0) return -1;
    typeMax = std::min<long long>(typeMax, 2000000000LL);
    long long bestStart = -1, bestLen = 0;
    long long cursor = 1; // 0 gilt in diesen Dateien als "kein Eintrag"/Platzhalter-ID, nicht vergeben
    auto consider = [&](long long start, long long endInclusive) {
        const long long len = endInclusive - start + 1;
        if (len > bestLen) { bestLen = len; bestStart = start; }
    };
    for (long long v : used) {
        if (v < cursor) continue;
        if (v > cursor) consider(cursor, std::min(v - 1, typeMax));
        cursor = v + 1;
        if (cursor > typeMax) break;
    }
    if (cursor <= typeMax) consider(cursor, typeMax);
    if (bestStart < 0) return -1;
    // Sehr kleine Bloecke (< 20 Plaetze) taugen nicht als "eigener Bereich" - dann lieber das
    // niedrigste freie Element, damit wenigstens etwas Eindeutiges vergeben wird.
    if (bestLen < 20) {
        for (long long v = 1; v <= typeMax; ++v) if (!used.count(v)) return v;
        return -1;
    }
    return bestStart;
}
void AddRowWithPropagation(EditorState& state, int docIndex) {
    if (docIndex < 0 || docIndex >= static_cast<int>(state.shnFiles.size())) return;
    auto& srcDoc = state.shnFiles[static_cast<std::size_t>(docIndex)];
    core::legacy::ShnRow newRow;
    for (auto& col : srcDoc.file.columns) {
        auto v = core::legacy::ParseShnValue(col, DefaultShnValueText(col));
        newRow.values.push_back(v ? std::move(*v) : core::legacy::ShnValue(std::uint32_t{0}));
    }
    std::vector<int> peers;
    if (const auto* verifiedFamily = KnownShnFamilyFor(srcDoc.file.FileName())) {
        for (std::size_t i = 0; i < state.shnFiles.size(); ++i) {
            if (static_cast<int>(i) == docIndex) continue;
            const auto* candidateFamily = KnownShnFamilyFor(state.shnFiles[i].file.FileName());
            if (candidateFamily && std::string_view(candidateFamily->label) == verifiedFamily->label)
                peers.push_back(static_cast<int>(i));
        }
    }

    // Automatische freie ID wird ausschließlich über die verifizierte Familien-ID bestimmt.
    // Andere Spaltenbeziehungen sind NICHT belegt und dürfen nicht automatisch übertragen werden.
    const int idCol = ExactShnColumnIndex(srcDoc.file, "ID");
    long long newId = -1;
    if (idCol >= 0) {
        const auto& idColumn = srcDoc.file.columns[static_cast<std::size_t>(idCol)];
        std::set<long long> used;
        CollectShnInts(srcDoc.file, idCol, used);
        for (int p : peers) {
            const auto& pf = state.shnFiles[static_cast<std::size_t>(p)].file;
            CollectShnInts(pf, ShnColumnIndexByName(pf, idColumn.name), used);
        }
        newId = SuggestFreeShnId(idColumn, used);
        if (newId >= 0) {
            if (auto parsed = core::legacy::ParseShnValue(idColumn, std::to_string(newId))) {
                newRow.values[static_cast<std::size_t>(idCol)] = std::move(*parsed);
            }
        }
    }
    // Namensspalte (InxName/IndexName), die eindeutig sein soll: Platzhalter "Custom<Datei><ID>".
    if (newId >= 0) {
        for (std::size_t c = 0; c < srcDoc.file.columns.size(); ++c) {
            const auto& col = srcDoc.file.columns[c];
            if (col.kind != core::legacy::ShnValueKind::String || (col.name != "InxName" && col.name != "IndexName")) continue;
            std::string stem = srcDoc.file.FileName();
            if (const auto dot = stem.rfind('.'); dot != std::string::npos) stem.resize(dot);
            std::string name = "Custom" + stem + std::to_string(newId);
            if (col.length > 1 && name.size() >= col.length) name.resize(col.length - 1);
            if (auto parsed = core::legacy::ParseShnValue(col, name)) newRow.values[c] = std::move(*parsed);
            break;
        }
    }
    srcDoc.file.rows.push_back(newRow);
    srcDoc.dirty = true;
    EnsureCellStatusSize(srcDoc);
    const std::size_t newRowIdx = srcDoc.file.rows.size() - 1;
    std::fill(srcDoc.cellDirty.back().begin(),srcDoc.cellDirty.back().end(),1);
    if (idCol >= 0 && newId >= 0) srcDoc.cellStatus.back()[static_cast<std::size_t>(idCol)] = kShnCellAutoFilled;
    int propagated = 0;
    std::vector<std::pair<int, int>> insertedRows = {{docIndex, static_cast<int>(newRowIdx)}};
    for (int p : peers) {
        auto& peerDoc = state.shnFiles[static_cast<std::size_t>(p)];
        core::legacy::ShnRow peerRow;
        std::vector<std::uint8_t> peerRowStatus;
        const int peerIdCol = ExactShnColumnIndex(peerDoc.file, "ID");
        for (std::size_t pc = 0; pc < peerDoc.file.columns.size(); ++pc) {
            const auto& peerCol = peerDoc.file.columns[pc];

            // Die NA2016-Evidenz verifiziert die gemeinsame ID-Menge, nicht die Semantik
            // gleichnamiger sonstiger Spalten. Deshalb wird NUR ID automatisch übertragen.
            if (static_cast<int>(pc) == peerIdCol && newId >= 0) {
                if (auto parsed = core::legacy::ParseShnValue(peerCol, std::to_string(newId))) {
                    peerRow.values.push_back(std::move(*parsed));
                    peerRowStatus.push_back(kShnCellAutoFilled);
                    continue;
                }
            }

            auto v = core::legacy::ParseShnValue(peerCol, DefaultShnValueText(peerCol));
            peerRow.values.push_back(v ? std::move(*v) : core::legacy::ShnValue(std::uint32_t{0}));
            peerRowStatus.push_back(kShnCellNeedsInput);
        }
        peerDoc.file.rows.push_back(peerRow);
        peerDoc.dirty = true;
        EnsureCellStatusSize(peerDoc);
        peerDoc.cellStatus.back() = peerRowStatus;
        std::fill(peerDoc.cellDirty.back().begin(),peerDoc.cellDirty.back().end(),1);
        insertedRows.emplace_back(p, static_cast<int>(peerDoc.file.rows.size()) - 1);
        ++propagated;
    }
    RecordRowsInserted(state, "Neue Zeile: " + srcDoc.file.FileName() +
                       (newId >= 0 ? " #" + std::to_string(newId) : std::string()), insertedRows);

    ++state.shnEditCounter;
    state.shnSelectedFile = docIndex;
    state.shnSelectedRow = static_cast<int>(newRowIdx);
    state.shnSelectedColumn = 0;
    state.shnStatus = "Neue Zeile angelegt" + (newId >= 0 ? " (freie ID " + std::to_string(newId) + " automatisch vergeben)" : std::string()) + (propagated > 0
        ? (" und die ID in " + std::to_string(propagated) + " verifizierte(n) Familienkopie(n) propagiert (nur ID grün; übrige Peer-Zellen rot=manuell prüfen).")
        : std::string(" (keine geladene verifizierte Mutationsfamilie gefunden; Heuristiken wurden nicht verändert)."));
}
void DrawShnSourceList(EditorState& state, EditorState::ShnSource source, const char* id) {
    ImGui::TextColored(ShnSourceColor(source), "%s", ShnSourceName(source));
    ImGui::SameLine();
    std::size_t count = 0, visibleCount = 0, dirtyCount = 0;
    const std::string fileNeedle = LowerAscii(state.shnFileFilter);
    for (const auto& d : state.shnFiles) {
        if (d.source != source) continue;
        ++count;
        if (d.dirty) ++dirtyCount;
        if (fileNeedle.empty() || LowerAscii(d.file.FileName()).find(fileNeedle) != std::string::npos) ++visibleCount;
    }
    if (fileNeedle.empty()) ImGui::TextDisabled(L("(%zu Dateien)","(%zu files)"),count);
    else ImGui::TextDisabled("%zu / %zu",visibleCount,count);
    if (dirtyCount > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f),"*%zu",dirtyCount);
    }
    ImGui::BeginChild(id, ImVec2(0, 170), true);
    for (int i : ShnIndicesForSource(state, source)) {
        auto& doc = state.shnFiles[static_cast<std::size_t>(i)];
        if (!fileNeedle.empty() && LowerAscii(doc.file.FileName()).find(fileNeedle) == std::string::npos) continue;
        const bool selected = state.shnSelectedFile == i;
        const auto peers = FindDependencyPeers(state, i);
        if (!peers.empty()) {
            // Gelbes Warndreieck: möglicherweise abhängige Datei(en) gefunden (siehe oben).
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "!");
            ImGui::SameLine(0.0f, 3.0f);
        }
        std::string label = doc.file.FileName() + (doc.dirty ? " *" : "");
        if (UI::Selectable((label + "##" + std::to_string(i)).c_str(), selected)) SelectShnDocument(state, i);
        if (ImGui::IsItemHovered()) {
            if (!peers.empty()) {
                std::string tip = doc.file.path.string() + "\n\n! Gleiche Zeilenanzahl (" +
                                   std::to_string(doc.file.rows.size()) + ") wie:\n";
                for (int p : peers) tip += "  - " + state.shnFiles[static_cast<std::size_t>(p)].file.FileName() + "\n";
                tip += "\nMögliche Abhängigkeit (Zeilenanzahl-Gleichschritt, siehe SHN_EDITOR.md) -\n"
                       "neue/gelöschte Zeilen hier sollten dort geprüft werden.";
                ImGui::SetTooltip("%s", tip.c_str());
            } else {
                ImGui::SetTooltip("%s", doc.file.path.string().c_str());
            }
        }
    }
    ImGui::EndChild();
}
void DrawShnMultiProfiles(EditorState& state) {
    DrawPanelHeader("multiShnHeader", "MULTI SHN", DrawIconLayers,
                    "module.shn.multi", L("Client / Server vergleichen","Compare client / server"));
    ImGui::TextWrapped("%s", L(
        "Die Aufgabe verwendet die verifizierte Daten-Abhängigkeitsmatrix. PFLICHT/BEDINGT/"
        "REFERENZ ersetzt die frühere Dateinamen-Keyword-Heuristik. Nur belegte ID-Familien "
        "dürfen automatisch mutiert werden.",
        "This task uses the verified data dependency matrix. REQUIRED/CONDITIONAL/REFERENCE "
        "replaces the former filename-keyword heuristic. Only evidenced ID families may be "
        "mutated automatically."));

    const auto profiles = core::DataDependencyProfiles();
    state.shnMultiProfile = std::clamp(
        state.shnMultiProfile, 0, std::max(0, static_cast<int>(profiles.size()) - 1));
    std::vector<const char*> profileLabels;
    profileLabels.reserve(profiles.size());
    for (const auto& profile : profiles)
        profileLabels.push_back((app::CurrentLanguage() == app::Language::English
            ? profile.labelEn : profile.labelDe).data());

    ImGui::SetNextItemWidth(280.0f);
    UI::Combo(L("Aufgabe","Task"), &state.shnMultiProfile, profileLabels.data(),
              static_cast<int>(profileLabels.size()));
    ImGui::SameLine();
    ImGui::TextDisabled("%s %zu", L("Geladene Dateien:","Loaded files:"), state.shnFiles.size());
    ImGui::Separator();

    const auto& selectedProfile = ShnDependencyProfile(state.shnMultiProfile);
    ImGui::BeginChild("##dependencyMatrix", ImVec2(0, 185.0f), true);
    ImGui::TextColored(UiTheme::AccentCyan, "%s", L("BETROFFENE DATEIEN","AFFECTED FILES"));
    ImGui::SameLine();
    ImGui::TextDisabled("· %s", L(
        "nur konkrete Funktion aktivieren = BEDINGT ändern",
        "change CONDITIONAL only when that feature is actually used"));

    for (const auto& rule : selectedProfile.rules) {
        const bool isRequired = rule.dependencyClass == core::DataDependencyClass::Required;
        const bool isConditional = rule.dependencyClass == core::DataDependencyClass::Conditional;
        const bool isReference = rule.dependencyClass == core::DataDependencyClass::Reference;
        const ImVec4 roleColor = isRequired ? ImVec4(1.0f,0.48f,0.34f,1.0f)
                               : isConditional ? ImVec4(1.0f,0.78f,0.28f,1.0f)
                               : isReference ? ImVec4(0.45f,0.75f,1.0f,1.0f)
                               : UiTheme::TextSecondary;

        bool shnLoaded = false;
        const std::filesystem::path configuredPath(std::string(rule.relativePath));
        const bool isShn = LowerAscii(configuredPath.extension().string()) == ".shn";
        if (isShn) {
            shnLoaded = std::any_of(state.shnFiles.begin(), state.shnFiles.end(),
                [&](const EditorState::ShnDocument& doc) {
                    return ShnDependencyRuleMatchesDocument(rule, doc);
                });
        }

        ImGui::TextColored(roleColor, "%s",
            (app::CurrentLanguage() == app::Language::English
                ? core::DataDependencyClassNameEn(rule.dependencyClass)
                : core::DataDependencyClassNameDe(rule.dependencyClass)).data());
        ImGui::SameLine(92.0f);
        ImGui::TextDisabled("%s", core::DataTreeName(rule.tree).data());
        ImGui::SameLine(160.0f);
        if (isShn) {
            ImGui::TextColored(shnLoaded ? UiTheme::Success : UiTheme::TextSecondary,
                               "%s %s", shnLoaded ? "OK" : "·",
                               std::string(rule.relativePath).c_str());
        } else {
            ImGui::TextUnformatted(std::string(rule.relativePath).c_str());
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", std::string(rule.reason).c_str());
    }
    ImGui::EndChild();
    ImGui::Separator();

    struct Pair {
        std::string fileName;
        int client = -1;
        int server = -1;
    };
    std::map<std::string, Pair> pairs;
    for (std::size_t i = 0; i < state.shnFiles.size(); ++i) {
        auto& doc = state.shnFiles[i];
        if (!ShnProfileMatch(doc, state.shnMultiProfile)) continue;
        const std::string key = LowerAscii(doc.file.FileName());
        auto& pair = pairs[key];
        pair.fileName = doc.file.FileName();
        if (doc.source == EditorState::ShnSource::Client) pair.client = static_cast<int>(i);
        else pair.server = static_cast<int>(i);
    }

    std::size_t paired = 0, clientOnly = 0, serverOnly = 0, dirtyFiles = 0;
    for (const auto& [key, p] : pairs) {
        (void)key;
        if (p.client >= 0 && p.server >= 0) ++paired;
        else if (p.client >= 0) ++clientOnly;
        else ++serverOnly;
        if ((p.client >= 0 && state.shnFiles[static_cast<std::size_t>(p.client)].dirty) ||
            (p.server >= 0 && state.shnFiles[static_cast<std::size_t>(p.server)].dirty))
            ++dirtyFiles;
    }

    ImGui::BeginChild("##multiSummary", ImVec2(0, 54.0f), true);
    ImGui::Text("Paare: %zu", paired);
    ImGui::SameLine(); ImGui::TextDisabled("| Client-only: %zu", clientOnly);
    ImGui::SameLine(); ImGui::TextDisabled("| Server-only: %zu", serverOnly);
    ImGui::SameLine(); ImGui::TextDisabled("| Geändert: %zu", dirtyFiles);
    ImGui::TextDisabled("Klick auf Client/Server öffnet die Datei direkt im Single-SHN-Editor.");
    ImGui::EndChild();

    const ImGuiTableFlags flags =
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##multiCompare", 7, flags, ImVec2(0,0))) {
        ImGui::TableSetupScrollFreeze(0,1);
        ImGui::TableSetupColumn("Datei", ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn("Client", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("Server", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("Schema", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn("Zeilen", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Diff", ImGuiTableColumnFlags_WidthFixed, 125.0f);
        ImGui::TableSetupColumn("Aktion", ImGuiTableColumnFlags_WidthFixed, 125.0f);
        ImGui::TableHeadersRow();

        for (const auto& [key, p] : pairs) {
            (void)key;
            const core::legacy::ShnFile* clientFile =
                p.client >= 0 ? &state.shnFiles[static_cast<std::size_t>(p.client)].file : nullptr;
            const core::legacy::ShnFile* serverFile =
                p.server >= 0 ? &state.shnFiles[static_cast<std::size_t>(p.server)].file : nullptr;

            // Vergleich nur nach Änderungen neu berechnen (ItemInfo: ~900k Zellen).
            struct PairDiff { std::size_t revision = 0; bool schemaEqual = false, rowCountEqual = false,
                              diffTruncated = false, comparisonById = false;
                              std::size_t diffCells = 0, unmatchedRows = 0; };
            static std::unordered_map<std::string, PairDiff> diffCache;
            const std::size_t revision = state.shnEditCounter + 1;
            auto& cached = diffCache[key + "|" + std::to_string(p.client) + "|" + std::to_string(p.server)];
            if (cached.revision != revision) {
                bool schemaEqual = false;
                bool rowCountEqual = false;
                std::size_t diffCells = 0;
                std::size_t unmatchedRows = 0;
                bool diffTruncated = false;
                bool comparisonById = false;

                if (clientFile && serverFile) {
                    rowCountEqual = clientFile->rows.size() == serverFile->rows.size();
                    schemaEqual = clientFile->columns.size() == serverFile->columns.size();
                    if (schemaEqual) {
                        for (std::size_t ci = 0; ci < clientFile->columns.size(); ++ci) {
                            if (clientFile->columns[ci].name != serverFile->columns[ci].name ||
                                clientFile->columns[ci].kind != serverFile->columns[ci].kind) {
                                schemaEqual = false;
                                break;
                            }
                        }
                    }
                    if (schemaEqual) {
                        constexpr std::size_t kDiffScanLimit = 2000000;
                        std::size_t scanned = 0;
                        const int clientIdCol=FindShnIdColumn(*clientFile);
                        const int serverIdCol=FindShnIdColumn(*serverFile);
                        std::unordered_map<long long,std::size_t> clientById,serverById;
                        bool uniqueIds=clientIdCol>=0 && serverIdCol>=0;
                        if (uniqueIds) {
                            for (std::size_t ri=0;ri<clientFile->rows.size();++ri) {
                                long long id=0;
                                if (static_cast<std::size_t>(clientIdCol)>=clientFile->rows[ri].values.size() ||
                                    !ShnValueAsInt(clientFile->rows[ri].values[static_cast<std::size_t>(clientIdCol)],id) ||
                                    !clientById.emplace(id,ri).second) { uniqueIds=false; break; }
                            }
                        }
                        if (uniqueIds) {
                            for (std::size_t ri=0;ri<serverFile->rows.size();++ri) {
                                long long id=0;
                                if (static_cast<std::size_t>(serverIdCol)>=serverFile->rows[ri].values.size() ||
                                    !ShnValueAsInt(serverFile->rows[ri].values[static_cast<std::size_t>(serverIdCol)],id) ||
                                    !serverById.emplace(id,ri).second) { uniqueIds=false; break; }
                            }
                        }

                        comparisonById=uniqueIds && (!clientById.empty() || !serverById.empty());
                        if (comparisonById) {
                            for (const auto& [id,clientRow] : clientById) {
                                const auto sit=serverById.find(id);
                                if (sit==serverById.end()) { ++unmatchedRows; continue; }
                                if (scanned>=kDiffScanLimit) { diffTruncated=true; continue; }
                                const auto& cr=clientFile->rows[clientRow];
                                const auto& sr=serverFile->rows[sit->second];
                                const std::size_t colLimit=std::min(cr.values.size(),sr.values.size());
                                for (std::size_t ci=0;ci<colLimit && scanned<kDiffScanLimit;++ci,++scanned) {
                                    if (core::legacy::ShnValueToString(cr.values[ci]) !=
                                        core::legacy::ShnValueToString(sr.values[ci])) ++diffCells;
                                }
                                if (scanned>=kDiffScanLimit) diffTruncated=true;
                            }
                            for (const auto& [id,serverRow] : serverById) {
                                (void)serverRow;
                                if (!clientById.contains(id)) ++unmatchedRows;
                            }
                        } else {
                            const std::size_t rowLimit=std::min(clientFile->rows.size(),serverFile->rows.size());
                            unmatchedRows=clientFile->rows.size()>serverFile->rows.size()
                                ? clientFile->rows.size()-serverFile->rows.size()
                                : serverFile->rows.size()-clientFile->rows.size();
                            for (std::size_t ri=0;ri<rowLimit && scanned<kDiffScanLimit;++ri) {
                                const auto& cr=clientFile->rows[ri];
                                const auto& sr=serverFile->rows[ri];
                                const std::size_t colLimit=std::min(cr.values.size(),sr.values.size());
                                for (std::size_t ci=0;ci<colLimit && scanned<kDiffScanLimit;++ci,++scanned) {
                                    if (core::legacy::ShnValueToString(cr.values[ci]) !=
                                        core::legacy::ShnValueToString(sr.values[ci])) ++diffCells;
                                }
                            }
                            diffTruncated=scanned>=kDiffScanLimit;
                        }
                    }
                }
                cached = {revision, schemaEqual, rowCountEqual, diffTruncated, comparisonById, diffCells, unmatchedRows};
            }
            const bool schemaEqual = cached.schemaEqual;
            const bool rowCountEqual = cached.rowCountEqual;
            const bool diffTruncated = cached.diffTruncated;
            const bool comparisonById = cached.comparisonById;
            const std::size_t diffCells = cached.diffCells;
            const std::size_t unmatchedRows = cached.unmatchedRows;

            ImGui::PushID(p.fileName.c_str());
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(p.fileName.c_str());

            auto drawSide = [&](int docIndex, const char* label) {
                if (docIndex < 0) {
                    ImGui::TextDisabled("-");
                    return;
                }
                auto& doc = state.shnFiles[static_cast<std::size_t>(docIndex)];
                const std::string button = std::string(label) + (doc.dirty ? " *" : "");
                if (UI::SmallButton(button.c_str())) {
                    SelectShnDocument(state, docIndex);
                    state.shnSubTab = 0;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\n%zu Zeilen · %zu Spalten%s",
                                      doc.file.path.string().c_str(),
                                      doc.file.rows.size(), doc.file.columns.size(),
                                      doc.dirty ? "\nNicht gespeicherte Änderungen" : "");
            };

            ImGui::TableSetColumnIndex(1); drawSide(p.client, "CLIENT");
            ImGui::TableSetColumnIndex(2); drawSide(p.server, "SERVER");

            ImGui::TableSetColumnIndex(3);
            if (!clientFile || !serverFile) {
                ImGui::TextDisabled("kein Paar");
            } else if (schemaEqual) {
                ImGui::TextColored(ImVec4(0.45f,0.85f,0.60f,1.0f), "gleich");
            } else {
                ImGui::TextColored(ImVec4(1.0f,0.55f,0.30f,1.0f), "abweichend");
            }

            ImGui::TableSetColumnIndex(4);
            if (clientFile && serverFile) {
                if (rowCountEqual)
                    ImGui::Text("%zu", clientFile->rows.size());
                else
                    ImGui::TextColored(ImVec4(1.0f,0.55f,0.30f,1.0f), "%zu / %zu",
                                       clientFile->rows.size(), serverFile->rows.size());
            } else {
                const auto* only = clientFile ? clientFile : serverFile;
                ImGui::TextDisabled("%zu", only ? only->rows.size() : 0u);
            }

            ImGui::TableSetColumnIndex(5);
            if (!clientFile || !serverFile) {
                ImGui::TextDisabled("-");
            } else if (!schemaEqual) {
                ImGui::TextDisabled("n/a");
            } else if (diffCells == 0 && unmatchedRows == 0 && !diffTruncated) {
                ImGui::TextColored(ImVec4(0.45f,0.85f,0.60f,1.0f), "0");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s",comparisonById
                        ? L("Zeilen wurden über ihre ID zugeordnet.","Rows were matched by ID.")
                        : L("Keine eindeutige ID-Spalte: Vergleich nach Zeilenposition.","No unique ID column: compared by row position."));
            } else {
                ImGui::TextColored(ImVec4(1.0f,0.72f,0.30f,1.0f), "Z:%zu%s R:%zu",
                                   diffCells,diffTruncated?"+":"",unmatchedRows);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s\n%s",comparisonById
                        ? L("Zeilen wurden über ihre ID zugeordnet; Sortierreihenfolge erzeugt keine falschen Diffs.",
                            "Rows were matched by ID; sort order does not create false diffs.")
                        : L("Keine eindeutige ID-Spalte: Vergleich nach Zeilenposition.",
                            "No unique ID column: compared by row position."),
                        diffTruncated
                            ? L("Maximal 2 Mio. Zellen wurden inhaltlich verglichen.","At most 2 million cells were compared.")
                            : L("Z = abweichende Zellen, R = nur auf einer Seite vorhandene Zeilen.",
                                "Z = differing cells, R = rows present on only one side."));
                }
            }

            ImGui::TableSetColumnIndex(6);
            if (clientFile && serverFile) {
                if (UI::SmallButton("Client öffnen")) {
                    SelectShnDocument(state, p.client);
                    state.shnSubTab = 0;
                    state.shnHighlightClientServerDiff = true;
                }
            } else {
                ImGui::TextDisabled("prüfen");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (pairs.empty())
        ImGui::TextDisabled("Für diese Aufgabe wurden in den geladenen Client-/Server-SHN keine Kandidaten gefunden.");
}
// Sucht ein bereits geladenes SHN-Dokument nach Dateiname (ohne Pfad) + Quelle; falls nicht
// geladen, aber der jeweilige Ordner (shnServerRoot/shnClientRoot) schon bekannt ist, wird
// dort rekursiv danach gesucht und automatisch geladen - für XP-/Preis-Editor, die feste,
// bekannte Dateien brauchen (MobInfoServer.shn bzw. ItemInfo.shn, siehe docs/
// SHN_DEPENDENCIES.md). Gibt -1 zurück, wenn weder geladen noch auffindbar.
int FindOrLoadShnDoc(EditorState& state, const std::string& fileName, EditorState::ShnSource source) {
    for (std::size_t i = 0; i < state.shnFiles.size(); ++i) {
        if (state.shnFiles[i].source == source &&
            LowerAscii(state.shnFiles[i].file.FileName()) == LowerAscii(fileName)) {
            return static_cast<int>(i);
        }
    }
    const std::string& root = source == EditorState::ShnSource::Client ? state.shnClientRoot : state.shnServerRoot;
    if (root.empty() || !std::filesystem::exists(root)) return -1;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || ec) { ec.clear(); continue; }
        if (LowerAscii(it->path().filename().string()) != LowerAscii(fileName)) continue;
        const auto sourcePath = it->path();
        const auto workingPath = ShnWorkingPath(state, sourcePath, source);
        auto result = core::legacy::LoadShnFile(workingPath);
        if (!result) return -1;
        EditorState::ShnDocument doc;
        doc.file = std::move(*result);
        doc.sourcePath = sourcePath;
        doc.source = source;
        state.shnFiles.push_back(std::move(doc));
        return static_cast<int>(state.shnFiles.size()) - 1;
    }
    return -1;
}
namespace {
bool PathIsUnder(const std::filesystem::path& path, const std::string& root) {
    if (root.empty()) return true;
    const std::string p = LowerAscii(path.lexically_normal().generic_string());
    std::string r = LowerAscii(std::filesystem::path(root).lexically_normal().generic_string());
    if (!r.empty() && r.back() != '/') r += '/';
    return p.rfind(r, 0) == 0;
}
} // namespace
int SharedShnDoc(EditorState& state, const std::string& fileName, EditorState::ShnSource source,
                 const std::filesystem::path& fallbackDir) {
    const std::string& root = source == EditorState::ShnSource::Client ? state.shnClientRoot : state.shnServerRoot;
    const std::string fallbackRoot = fallbackDir.empty() ? std::string() : fallbackDir.string();
    // Bereits geladen: nur Dokumente aus dem aktuellen Projekt (Wurzel- oder Rückfallordner), damit
    // nach einem Projektwechsel keine alte Kopie weiterverwendet wird.
    for (std::size_t i = 0; i < state.shnFiles.size(); ++i) {
        const auto& d = state.shnFiles[i];
        if (d.source != source || LowerAscii(d.file.FileName()) != LowerAscii(fileName)) continue;
        const auto& src = d.sourcePath.empty() ? d.file.path : d.sourcePath;
        if ((!root.empty() && PathIsUnder(src, root)) || (!fallbackRoot.empty() && PathIsUnder(src, fallbackRoot)) ||
            (root.empty() && fallbackRoot.empty()))
            return static_cast<int>(i);
    }
    if (!root.empty() && std::filesystem::exists(root)) {
        const int found = FindOrLoadShnDoc(state, fileName, source);
        if (found >= 0) return found;
    }
    if (fallbackDir.empty()) return -1;
    std::error_code ec;
    std::filesystem::path sourcePath = fallbackDir / fileName;
    if (!std::filesystem::is_regular_file(sourcePath, ec)) {
        // Groß-/Kleinschreibung der Datei im Ordner tolerieren.
        sourcePath.clear();
        for (std::filesystem::directory_iterator it(fallbackDir, ec), end; !ec && it != end; it.increment(ec))
            if (LowerAscii(it->path().filename().string()) == LowerAscii(fileName)) { sourcePath = it->path(); break; }
        if (sourcePath.empty()) return -1;
    }
    auto result = core::legacy::LoadShnFile(ShnWorkingPath(state, sourcePath, source));
    if (!result) return -1;
    EditorState::ShnDocument doc;
    doc.file = std::move(*result);
    doc.sourcePath = sourcePath;
    doc.source = source;
    state.shnFiles.push_back(std::move(doc));
    return static_cast<int>(state.shnFiles.size()) - 1;
}
core::legacy::ShnFile& ShnDocFile(EditorState& state, int document) {
    return state.shnFiles.at(static_cast<std::size_t>(document)).file;
}
void MarkShnChangesSince(EditorState::ShnDocument& doc, const core::legacy::ShnFile& baseline) {
    EnsureCellStatusSize(doc);
    bool any = false;
    for (std::size_t r = 0; r < doc.file.rows.size(); ++r) {
        const auto& row = doc.file.rows[r];
        for (std::size_t c = 0; c < row.values.size() && c < doc.cellDirty[r].size(); ++c) {
            const bool changed = r >= baseline.rows.size() || c >= baseline.rows[r].values.size() ||
                                 !(baseline.rows[r].values[c] == row.values[c]);
            if (changed) { doc.cellDirty[r][c] = 1; any = true; }
        }
    }
    if (any) doc.dirty = true;
}
bool OpenShnRecordById(EditorState& state, const std::vector<std::string>& fileNames,
                       EditorState::ShnSource preferredSource, long long id) {
    const std::array<EditorState::ShnSource,2> sources = {
        preferredSource,
        preferredSource == EditorState::ShnSource::Client
            ? EditorState::ShnSource::Server : EditorState::ShnSource::Client
    };
    for (const auto source : sources) {
        for (const auto& fileName : fileNames) {
            const int docIndex = FindOrLoadShnDoc(state, fileName, source);
            if (docIndex < 0) continue;
            auto& file = state.shnFiles[static_cast<std::size_t>(docIndex)].file;
            int idColumn = ShnColumnIndexByName(file, "ID");
            if (idColumn < 0 && !file.columns.empty()) idColumn = 0;
            if (idColumn < 0) continue;
            for (std::size_t row = 0; row < file.rows.size(); ++row) {
                if (static_cast<std::size_t>(idColumn) >= file.rows[row].values.size()) continue;
                long long candidate = 0;
                if (!ShnValueAsInt(file.rows[row].values[static_cast<std::size_t>(idColumn)], candidate) ||
                    candidate != id) continue;
                state.shnSubTab = 0;
                SelectShnDocument(state, docIndex);
                state.shnSelectedRow = static_cast<int>(row);
                state.shnSelectedColumn = idColumn;
                state.shnStatus = file.FileName() + ": Datensatz #" + std::to_string(id) + " geöffnet.";
                return true;
            }
        }
    }
    state.shnStatus = "Referenz #" + std::to_string(id) + " konnte in den passenden SHN-Dateien nicht gefunden werden.";
    return false;
}
// Gemeinsame Tabelle für XP-Rate- und Preis-Editor: zeigt ID+Name+die relevanten numerischen
// Spalten kompakt, mit Skalier-Aktion (z.B. "alle EXP-Werte um 10% erhöhen") - genau der
// Zweck eines "Rate"-Editors, statt nur ein Verweis auf die generische Tabelle. Zelle
// doppelklicken öffnet denselben Zell-Editor wie der Single-SHN-Editor (DrawShnCellEditor).
void DrawScalableFieldEditor(EditorState& state, int docIndex, const std::vector<std::string>& scaleCols,
                              const char* title) {
    if (docIndex < 0) {
        ImGui::TextWrapped("%s nicht gefunden - bitte zuerst den passenden SHN-Ordner einlesen (siehe Single SHN Editor).", title);
        return;
    }
    auto& doc = state.shnFiles[static_cast<std::size_t>(docIndex)];
    auto& file = doc.file;
    ImGui::Text("%s", title);
    ImGui::SameLine(); ImGui::TextDisabled("(%s, %zu Zeilen)%s", file.FileName().c_str(), file.rows.size(), doc.dirty ? " | GEÄNDERT" : "");

    // Skalier-Aktion: Prozentwert auf alle Zeilen einer Spalte anwenden.
    static float scalePercent = 10.0f;
    ImGui::SetNextItemWidth(120.0f);
    UI::InputFloat("% Änderung", &scalePercent, 1.0f, 10.0f, "%.1f");
    for (const auto& colName : scaleCols) {
        int colIdx = -1;
        for (std::size_t c = 0; c < file.columns.size(); ++c) if (file.columns[c].name == colName) { colIdx = static_cast<int>(c); break; }
        if (colIdx < 0) continue;
        ImGui::SameLine();
        if (UI::Button(("Auf '" + colName + "' anwenden##scale" + colName).c_str())) {
            const double factor = 1.0 + static_cast<double>(scalePercent) / 100.0;
            EnsureCellStatusSize(doc);
            std::size_t changedCells = 0;
            for (std::size_t ri = 0; ri < file.rows.size(); ++ri) {
                auto& row = file.rows[ri];
                if (static_cast<std::size_t>(colIdx) >= row.values.size()) continue;
                long long v;
                if (!ShnValueAsInt(row.values[static_cast<std::size_t>(colIdx)], v)) continue;
                auto scaled = std::llround(static_cast<double>(v) * factor);
                auto parsed = core::legacy::ParseShnValue(
                    file.columns[static_cast<std::size_t>(colIdx)], std::to_string(scaled));
                if (!parsed || row.values[static_cast<std::size_t>(colIdx)] == *parsed) continue;
                row.values[static_cast<std::size_t>(colIdx)] = std::move(*parsed);
                doc.cellDirty[ri][static_cast<std::size_t>(colIdx)] = 1;
                ++changedCells;
            }
            if (changedCells > 0) {
                doc.dirty = true;
                ++state.shnEditCounter;
            }
            state.shnStatus = title + std::string(": '") + colName + "' um " +
                std::to_string(scalePercent) + "% skaliert · " +
                std::to_string(changedCells) + " Zellen geändert (noch nicht gespeichert).";
        }
    }
    ImGui::Separator();

    // Kompakte Tabelle: ID/InxName (erste zwei erkennbaren "Schlüssel"-Spalten) + die
    // angegebenen Skalier-Spalten - nicht die vollen 40+ Spalten wie im generischen Editor.
    std::vector<std::size_t> shownCols;
    for (std::size_t c = 0; c < file.columns.size() && shownCols.size() < 2; ++c) {
        const std::string& n = file.columns[c].name;
        if (n == "ID" || n == "InxName" || n == "Name" || n == "goodsNo") shownCols.push_back(c);
    }
    for (const auto& colName : scaleCols) {
        for (std::size_t c = 0; c < file.columns.size(); ++c) {
            if (file.columns[c].name == colName) { shownCols.push_back(c); break; }
        }
    }
    if (shownCols.empty()) { ImGui::TextDisabled("Keine der erwarteten Spalten gefunden."); return; }

    ImGui::BeginChild("##scalableTable", ImVec2(0, 0), true);
    if (ImGui::BeginTable("##scalableTableInner", static_cast<int>(shownCols.size()) + 1,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        for (auto c : shownCols) ImGui::TableSetupColumn(file.columns[c].name.c_str());
        ImGui::TableHeadersRow();
        for (std::size_t ri = 0; ri < file.rows.size(); ++ri) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", ri);
            for (std::size_t ci = 0; ci < shownCols.size(); ++ci) {
                ImGui::TableSetColumnIndex(static_cast<int>(ci + 1));
                std::size_t col = shownCols[ci];
                if (col >= file.rows[ri].values.size()) continue;
                std::string label = ShnShortValue(file.rows[ri].values[col]);
                const bool selected = state.shnSelectedFile == docIndex && state.shnSelectedRow == static_cast<int>(ri) && state.shnSelectedColumn == static_cast<int>(col);
                if (UI::Selectable((label + "##scf" + std::to_string(ri) + "_" + std::to_string(col)).c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    state.shnSelectedFile = docIndex; state.shnSelectedRow = static_cast<int>(ri); state.shnSelectedColumn = static_cast<int>(col);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        state.shnEditBuffer = core::legacy::ShnValueToString(file.rows[ri].values[col]);
                        state.shnEditPopupOpen = true;
                    }
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    DrawShnCellEditor(state);
}
void DrawShnEditor(EditorState& state) {
    // Beim ersten Oeffnen die aus den Projekt-Ordnern abgeleiteten Client-/Server-SHN-Ordner
    // automatisch einlesen (statt jeden Ordner von Hand zu waehlen).
    if (!state.shnAutoLoaded) {
        state.shnAutoLoaded = true;
        if (state.shnFiles.empty()) {
            if (!state.shnClientRoot.empty()) ScanShnFolder(state, state.shnClientRoot, EditorState::ShnSource::Client);
            if (!state.shnServerRoot.empty()) ScanShnFolder(state, state.shnServerRoot, EditorState::ShnSource::Server);
        }
    }
    DrawTopNav(state, L("Datei Editor","File Editor"));
    DrawShnDeleteConfirm(state);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
    ImGui::BeginChild("##shnEditor", ImVec2(0,0), false);
    DrawPanelHeader("gameDataWorkspaceHeader", L("SPIELDATEN","GAME DATA"), DrawIconTable,
                    "module.shn.single",
                    "SHN · Quest · Portale/Portals · Custom NPC/Mob · Skills · AI · Interface · Drops");
    const std::size_t dataDirtyShn = DirtyShnDocumentCount(state);
    if (dataDirtyShn > 0 || state.questDirty || state.townPortalDirty || state.recallCoordDirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f), "%s",
                           L("* ungespeichert","* unsaved"));
    }
    if (dataDirtyShn > 0) {
        ImGui::SameLine();
        if (UI::SmallButton(L("Alle SHN speichern","Save all SHN"))) {
            const auto [saved,failed] = SaveAllDirtyShnDocuments(state);
            state.shnStatus = std::string(L("Gespeichert: ","Saved: ")) + std::to_string(saved) +
                              (failed ? std::string(", ") + L("Fehler: ","errors: ") + std::to_string(failed) : std::string());
        }
    }
    ImGui::Separator();
    struct DataTool {
        const char* id;
        const char* label;
        IconDrawFn icon;
        const char* semanticIcon;
        int subTab;
        int creatureMode; // -1 = n/a, 0 = mob, 1 = NPC
    };
    const DataTool dataTools[] = {
        {"single","Single SHN",DrawIconTable,"module.shn.single",0,-1},
        {"multi","Multi SHN",DrawIconLayers,"module.shn.multi",1,-1},
        {"xp","XP Rate",DrawIconBolt,"module.xp",2,-1},
        {"prices",L("Preise","Prices"),DrawIconTable,"module.prices",3,-1},
        {"quest","Quest",DrawIconBook,"module.quest",4,-1},
        // Das Final-Paket besitzt weiterhin kein eigenes Portal-Icon; hier bleibt der
        // funktionale Legacy-Fallback, statt ein semantisch falsches Paketicon umzudeuten.
        {"portals",L("Portale","Portals"),DrawIconPortal,nullptr,5,-1},
        {"customNpc","Custom NPC",DrawIconPerson,"module.custom_npc",6,1},
        {"customMob","Custom Mob",DrawIconSpawn,"module.custom_mob",6,0},
        {"skills","Skills",DrawIconBolt,"module.skill",7,-1},
        {"ai","AI Scripts",DrawIconCode,"module.ai",8,-1},
        {"interface","Interface",DrawIconMonitorEye,"module.interface",9,-1},
        {"drops","Drops",DrawIconSpawn,"module.droptable",10,-1},
        {"items","Items",DrawIconTable,"module.prices",11,-1},
        {"mobs","Mobs",DrawIconSpawn,"module.custom_mob",12,-1},
        {"passive",L("Passiv","Passive"),DrawIconBolt,"module.skill",13,-1},
        {"states",L("Zustände","States"),DrawIconBolt,"module.skill",14,-1},
        {"texts",L("Texte","Texts"),DrawIconBook,"module.shn.single",15,-1},
        {"itemSystems",L("Item-Systeme","Item systems"),DrawIconGem,"module.prices",16,-1},
        {"attackSeq",L("Angriffsfolgen","Attack seq."),DrawIconSword,"module.ai",17,-1},
    };
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 5.0f));
    for (int i = 0; i < static_cast<int>(std::size(dataTools)); ++i) {
        const auto& tool = dataTools[i];
        bool active = state.shnSubTab == tool.subTab;
        if (tool.creatureMode >= 0)
            active = active && (state.wiz.isNpc == (tool.creatureMode == 1));

        if (DrawIconButton(tool.id, tool.label, tool.icon, active,
                           ImVec2(96.0f, 58.0f), true, tool.semanticIcon)) {
            state.shnSubTab = tool.subTab;
            if (tool.creatureMode >= 0) {
                state.wiz.isNpc = tool.creatureMode == 1;
                if (!state.wiz.isNpc && state.wiz.lookMode == 2) state.wiz.lookMode = 0;
            }
        }
        // Nach einem Element steht der Cursor schon in der nächsten Zeile - deshalb an der rechten
        // Kante des letzten Knopfs messen, ob der nächste noch daneben passt.
        const float nextRight = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + 96.0f;
        if (i + 1 < static_cast<int>(std::size(dataTools)) &&
            nextRight <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x) {
            ImGui::SameLine();
        }
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    const float leftW = 300.0f;
    const float gap = 8.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const bool rawShnWorkspace = state.shnSubTab <= 3;

    if (rawShnWorkspace) {
        ImGui::BeginChild("##shnLeft", ImVec2(leftW, avail.y), true);
        DrawPanelHeader("rawShnWorkspaceHeader",
                        state.shnSubTab==0 ? "SINGLE SHN EDITOR" : L("SHN DATEIEN","SHN FILES"),
                        DrawIconTable,
                        state.shnSubTab==0 ? "module.shn.single" : "module.shn.multi");
#ifdef _WIN32
        if (UI::Button("CLIENT: SHN-Ordner einlesen", ImVec2(-1,0)))
            if(auto p=BrowseForFolderWindows("CLIENT SHN Ordner wählen"))
                ScanShnFolder(state,*p,EditorState::ShnSource::Client);
        if (UI::Button("SERVER: SHN-Ordner einlesen", ImVec2(-1,0)))
            if(auto p=BrowseForFolderWindows("SERVER SHN Ordner wählen"))
                ScanShnFolder(state,*p,EditorState::ShnSource::Server);
        if (UI::Button("Einzelne SHN öffnen...", ImVec2(-1,0)))
            if(auto p=BrowseForShnFileWindows("Fiesta SHN Datei öffnen")) {
                std::snprintf(state.shnPath,sizeof(state.shnPath),"%s",p->c_str());
                OpenShnFile(state,*p);
            }
#endif
        UI::InputText("Datei", state.shnPath, sizeof(state.shnPath));
        if (UI::Button("Pfad öffnen", ImVec2(-1,0))) OpenShnFile(state, state.shnPath);
        if (!state.shnClientRoot.empty()) ImGui::TextDisabled("Client: %s", state.shnClientRoot.c_str());
        if (!state.shnServerRoot.empty()) ImGui::TextDisabled("Server: %s", state.shnServerRoot.c_str());
        ImGui::Separator();
        DrawSearchInput("shnFileFilter",L("SHN-Datei suchen...","Search SHN file..."),
                        state.shnFileFilter,sizeof(state.shnFileFilter));
        DrawShnSourceList(state, EditorState::ShnSource::Client, "##shnClientFiles");
        DrawShnSourceList(state, EditorState::ShnSource::Server, "##shnServerFiles");
        if (state.shnSelectedFile>=0 && state.shnSelectedFile<static_cast<int>(state.shnFiles.size())) {
            auto& doc=state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)];
            auto& f=doc.file;
            ImGui::BeginDisabled(!doc.dirty);
            if(UI::Button(doc.dirty?L("Speichern *","Save *"):L("Speichern","Save"),ImVec2(-1,0)))
                SaveShnDocument(state,state.shnSelectedFile);
            if(UI::Button(L("Neu laden / Änderungen verwerfen","Reload / discard changes"),ImVec2(-1,0)))
                ReloadShnDocument(state,state.shnSelectedFile);
            ImGui::EndDisabled();
            ImGui::Separator();
            if (DrawSearchInput("shnSearch",L("Zeilen durchsuchen...","Search rows..."),
                                state.shnSearch,sizeof(state.shnSearch))) {
                state.shnVisibleKey.clear();
            }
            state.shnFilterActive = state.shnSearch[0] != '\0';
            UI::Checkbox(L("Spaltennamen","Column names"),&state.shnSearchColumns);
            ImGui::SameLine();
            UI::Checkbox(L("Werte","Values"),&state.shnSearchValues);
            ImGui::TextDisabled("%s",state.shnFilterActive
                ? L("Suche filtert die Zeilen sofort.","Search filters rows immediately.")
                : L("Zusätzliche Filter stehen direkt unter den Spaltenüberschriften.","Additional filters are below the column headers."));
            ImGui::Separator();
            if(state.shnSelectedRow>=0 && state.shnSelectedColumn>=0 &&
               state.shnSelectedColumn<static_cast<int>(f.columns.size())) {
                ImGui::Text("Auswahl: Zeile %d",state.shnSelectedRow);
                ImGui::TextWrapped("%s",f.columns[static_cast<std::size_t>(state.shnSelectedColumn)].name.c_str());
                ImGui::TextDisabled("%s",f.TypeName(f.columns[static_cast<std::size_t>(state.shnSelectedColumn)]).c_str());
                if (state.shnSelectedRow < static_cast<int>(f.rows.size()) &&
                    state.shnSelectedColumn < static_cast<int>(f.rows[static_cast<std::size_t>(state.shnSelectedRow)].values.size())) {
                    if (UI::Button("Inline bearbeiten", ImVec2(-1,0))) {
                        StartShnInlineEdit(state, state.shnSelectedRow, state.shnSelectedColumn);
                    }
                    ImGui::TextDisabled("Doppelklick/F2 = Inline · Rechtsklick = erweiterte Optionen");
                }
            }
        }
        if(!state.shnStatus.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s",state.shnStatus.c_str());
        }
        ImGui::EndChild();
        ImGui::SameLine(0,gap);
    } else {
        ImGui::TextDisabled("Client: %s", state.shnClientRoot.empty() ? "(nicht gefunden)" : state.shnClientRoot.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| Server: %s", state.shnServerRoot.empty() ? "(nicht gefunden)" : state.shnServerRoot.c_str());
    }

    ImGui::BeginChild("##shnMain",
                      ImVec2(rawShnWorkspace ? avail.x-leftW-gap : avail.x,
                             rawShnWorkspace ? avail.y : std::max(120.0f, avail.y - ImGui::GetTextLineHeightWithSpacing())),
                      true);
    if(state.shnSubTab==0) DrawShnGrid(state);
    else if(state.shnSubTab==1) DrawShnMultiProfiles(state);
    else if(state.shnSubTab==2) {
        int idx = FindOrLoadShnDoc(state, "MobInfoServer.shn", EditorState::ShnSource::Server);
        DrawScalableFieldEditor(state, idx, {"MonEXP", "EXPRange"}, "XP Rate Editor (MobInfoServer)");
    }
    else if(state.shnSubTab==3) {
        int idx = FindOrLoadShnDoc(state, "ItemInfo.shn", EditorState::ShnSource::Server);
        if (idx < 0) idx = FindOrLoadShnDoc(state, "ItemInfo.shn", EditorState::ShnSource::Client);
        DrawScalableFieldEditor(state, idx, {"BuyPrice", "SellPrice"}, "Buy & Sell Editor (ItemInfo)");
    }
    else if(state.shnSubTab==4) { DrawQuestEditor(state); }
    else if(state.shnSubTab==5) { DrawPortalEditor(state); }
    else if(state.shnSubTab==6) { DrawCustomCreatureEditor(state); }
    else if(state.shnSubTab==7) { DrawSkillEditor(state); }
    else if(state.shnSubTab==8) { DrawAiWorkspace(state); }
    else if(state.shnSubTab==9) { DrawInterfaceWorkspace(state); }
    else if(state.shnSubTab==10) { DrawDropTableEditor(state); }
    else if(state.shnSubTab==11) { DrawItemEditor(state); }
    else if(state.shnSubTab==12) { DrawMobEditor(state); }
    else if(state.shnSubTab==13) { DrawPassiveSkillEditor(state); }
    else if(state.shnSubTab==14) { DrawAbStateEditor(state); }
    else if(state.shnSubTab==15) { DrawTextEditor(state); }
    else if(state.shnSubTab==16) { DrawItemSystemsEditor(state); }
    else { DrawAttackSequenceEditor(state); }
    ImGui::EndChild(); ImGui::EndChild(); ImGui::PopStyleColor();
}

} // namespace nge
