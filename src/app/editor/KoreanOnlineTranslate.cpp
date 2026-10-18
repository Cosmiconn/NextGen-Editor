// KoreanOnlineTranslate.cpp
// Optionale Online-Übersetzung koreanischer Texte ohne Wörterbuch-Eintrag über die Claude-API.
// - API-Schlüssel: Umgebungsvariable ANTHROPIC_API_KEY, sonst Benutzer-Einstellung außerhalb von
//   Repo und Projekt (<Einstellungen>/anthropic_api_key.bin; unter Windows per DPAPI verschlüsselt).
// - Transport: Windows WinHTTP (Systembibliothek), sonst curl als Prozess (Schlüssel über eine
//   Konfigurationsdatei mit Rechten 0600, nie auf der Kommandozeile).
// - Blöcke zu kKoreanOnlineBatchSize Texten im Hintergrund-Thread; der Hauptthread speichert fertige
//   Blöcke ins Projekt-Wörterbuch (AddProjectKoreanTranslations). Abbrechen wirkt zwischen den Blöcken.
// NEXTGEN_KOREAN_API_URL ersetzt den Endpunkt (nur für Tests mit einem lokalen Server).

#include "EditorShared.hpp"

#include <atomic>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <wincrypt.h>
#include <winhttp.h>
#endif

namespace nge {
namespace {

namespace legacy = core::legacy;

struct HttpResponse {
    int status = 0;
    std::string body;
};

std::string EndpointUrl() {
    if (const char* url = std::getenv("NEXTGEN_KOREAN_API_URL"); url && *url) return url;
    return std::string("https://") + legacy::kKoreanOnlineHost + legacy::kKoreanOnlinePath;
}

std::filesystem::path KeyPath() { return NextGenUserSettingsDir() / "anthropic_api_key.bin"; }

#ifdef _WIN32
std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string WinError(const char* what) { return std::string(what) + " (Fehler " + std::to_string(GetLastError()) + ")"; }

std::expected<HttpResponse, std::string> HttpPostJson(const std::string& body, const std::string& key) {
    const std::wstring url = Widen(EndpointUrl());
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {}, path[1024] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 255;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 1023;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) return std::unexpected(WinError("Adresse ungültig"));

#ifdef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
    const DWORD access = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY; // System-Proxy (Windows 8.1+)
#else
    const DWORD access = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
#endif
    HINTERNET session = WinHttpOpen(L"NextGen-Editor", access, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) session = WinHttpOpen(L"NextGen-Editor", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return std::unexpected(WinError("WinHttpOpen"));
    WinHttpSetTimeouts(session, 15000, 15000, 60000, 600000);
    HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                     parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                                : nullptr;
    const auto close = [&] {
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
    };
    if (!request) { const auto e = WinError("Verbindung"); close(); return std::unexpected(e); }
    const std::wstring headers = Widen(std::string("content-type: application/json\r\nx-api-key: ") + key +
                                       "\r\nanthropic-version: " + legacy::kKoreanOnlineApiVersion +
                                       "\r\nanthropic-beta: " + legacy::kKoreanOnlineBeta + "\r\n");
    if (!WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1), const_cast<char*>(body.data()),
                            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        const auto e = WinError("Anfrage fehlgeschlagen (Netz/Proxy?)");
        close();
        return std::unexpected(e);
    }
    HttpResponse response;
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                        WINHTTP_NO_HEADER_INDEX);
    response.status = static_cast<int>(status);
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
        response.body.append(chunk.data(), read);
    }
    close();
    return response;
}

std::string LoadStoredKey() {
    std::ifstream in(KeyPath(), std::ios::binary);
    const std::string blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (blob.empty()) return {};
    DATA_BLOB input{static_cast<DWORD>(blob.size()), reinterpret_cast<BYTE*>(const_cast<char*>(blob.data()))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) return {};
    std::string key(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return key;
}

bool StoreKey(const std::string& key) {
    DATA_BLOB input{static_cast<DWORD>(key.size()), reinterpret_cast<BYTE*>(const_cast<char*>(key.data()))};
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"NextGen-Editor API key", nullptr, nullptr, nullptr, 0, &output)) return false;
    std::ofstream out(KeyPath(), std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return static_cast<bool>(out);
}
#else
std::string ShellQuote(const std::string& s) {
    std::string out = "'";
    for (const char c : s) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}

std::string CurlConfigValue(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

std::expected<HttpResponse, std::string> HttpPostJson(const std::string& body, const std::string& key) {
    std::error_code ec;
    const auto dir = std::filesystem::temp_directory_path(ec) /
                     ("nextgen-ko-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir, ec);
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all, ec);
    const auto bodyPath = dir / "body.json", outPath = dir / "out.json", cfgPath = dir / "curl.cfg";
    { std::ofstream(bodyPath, std::ios::binary) << body; }
    {
        std::ofstream cfg(cfgPath, std::ios::binary);
        cfg << "url = " << CurlConfigValue(EndpointUrl()) << "\n"
            << "header = " << CurlConfigValue("x-api-key: " + key) << "\n"
            << "header = " << CurlConfigValue(std::string("anthropic-version: ") + legacy::kKoreanOnlineApiVersion) << "\n"
            << "header = " << CurlConfigValue(std::string("anthropic-beta: ") + legacy::kKoreanOnlineBeta) << "\n"
            << "header = \"content-type: application/json\"\n"
            << "data-binary = " << CurlConfigValue("@" + bodyPath.string()) << "\n"
            << "output = " << CurlConfigValue(outPath.string()) << "\n"
            << "write-out = \"%{http_code}\"\nsilent\nshow-error\nmax-time = 600\n";
    }
    std::filesystem::permissions(cfgPath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, ec);
    const std::string command = "curl -K " + ShellQuote(cfgPath.string()) + " 2>&1";
    std::string printed;
    if (FILE* pipe = popen(command.c_str(), "r")) {
        char buf[512];
        while (std::fgets(buf, sizeof(buf), pipe)) printed += buf;
        pclose(pipe);
    }
    HttpResponse response;
    {
        std::ifstream in(outPath, std::ios::binary);
        response.body.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    std::filesystem::remove_all(dir, ec);
    const std::string code = printed.size() >= 3 ? printed.substr(printed.size() - 3) : printed;
    response.status = std::atoi(code.c_str());
    if (response.status == 0) return std::unexpected("curl: " + TrimAscii(printed.empty() ? std::string("nicht gefunden") : printed));
    return response;
}

std::string LoadStoredKey() {
    std::ifstream in(KeyPath(), std::ios::binary);
    std::string key((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return TrimAscii(key);
}

bool StoreKey(const std::string& key) {
    std::ofstream out(KeyPath(), std::ios::binary | std::ios::trunc);
    out << key;
    out.close();
    std::error_code ec;
    std::filesystem::permissions(KeyPath(), std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, ec);
    return static_cast<bool>(out);
}
#endif

struct OnlineJob {
    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<bool> cancel{false};
    std::atomic<int> batchesDone{0};
    int batchesTotal = 0;
    std::size_t texts = 0;
    std::string projectFolder;
    std::mutex mutex;
    std::vector<std::pair<std::string, legacy::KoreanTranslation>> pending; // vom Hauptthread zu speichern
    std::vector<std::string> log;
    std::size_t saved = 0;
    // Beim Beenden des Editors nicht auf eine laufende Anfrage warten.
    ~OnlineJob() {
        cancel = true;
        if (worker.joinable()) worker.detach();
    }
};

OnlineJob& Job() { static OnlineJob job; return job; }

void AddLog(OnlineJob& job, std::string line) {
    std::lock_guard lock(job.mutex);
    job.log.push_back(std::move(line));
}

void RunJob(std::vector<legacy::KoreanOnlineItem> items, legacy::KoreanDictionary glossary, std::string key) {
    auto& job = Job();
    for (std::size_t start = 0; start < items.size() && !job.cancel; start += legacy::kKoreanOnlineBatchSize) {
        const std::vector<legacy::KoreanOnlineItem> batch(items.begin() + static_cast<std::ptrdiff_t>(start),
                                                          items.begin() + static_cast<std::ptrdiff_t>(std::min(items.size(), start + legacy::kKoreanOnlineBatchSize)));
        const auto response = HttpPostJson(legacy::BuildKoreanTranslationRequest(batch, glossary), key);
        const std::string where = L("Block ", "Batch ") + std::to_string(start / legacy::kKoreanOnlineBatchSize + 1) + ": ";
        if (!response) {
            AddLog(job, where + response.error());
            break; // Netz/Proxy: weitere Blöcke würden ebenso scheitern
        }
        const auto parsed = legacy::ParseKoreanTranslationResponse(response->status, response->body, batch);
        if (!parsed) {
            AddLog(job, where + parsed.error());
            if (response->status == 401 || response->status == 403 || response->status == 429 || response->status >= 500) break;
        } else {
            std::lock_guard lock(job.mutex);
            job.pending.insert(job.pending.end(), parsed->translations.begin(), parsed->translations.end());
            for (const auto& p : parsed->problems) job.log.push_back(where + p);
        }
        ++job.batchesDone;
    }
    if (job.cancel) AddLog(job, L("Abgebrochen.", "Cancelled."));
    job.running = false;
}

} // namespace

std::string KoreanApiKey() {
    if (const char* env = std::getenv("ANTHROPIC_API_KEY"); env && *env) return TrimAscii(env);
    return LoadStoredKey();
}

bool KoreanApiKeyFromEnvironment() {
    const char* env = std::getenv("ANTHROPIC_API_KEY");
    return env && *env;
}

bool SaveKoreanApiKey(const std::string& key) {
    std::error_code ec;
    std::filesystem::create_directories(NextGenUserSettingsDir(), ec);
    if (TrimAscii(key).empty()) return std::filesystem::remove(KeyPath(), ec) || !std::filesystem::exists(KeyPath(), ec);
    return StoreKey(TrimAscii(key));
}

// Koreanische Texte ohne Wörterbuch-Eintrag (nur Automatik) aus Dokument und Gegenstücken; Byte-Grenze =
// kleinste feste Spaltenlänge minus Abschluss-0 unter allen Vorkommen.
std::vector<legacy::KoreanOnlineItem> UnknownKoreanTexts(EditorState& state, int document, const std::vector<int>& peers) {
    std::map<std::string, std::size_t> limits;
    std::vector<int> docs{document};
    docs.insert(docs.end(), peers.begin(), peers.end());
    for (const int d : docs) {
        if (d < 0 || d >= static_cast<int>(state.shnFiles.size())) continue;
        const auto& f = ShnDocFile(state, d);
        for (const auto& row : f.rows)
            for (std::size_t c = 0; c < f.columns.size() && c < row.values.size(); ++c) {
                const auto& col = f.columns[c];
                if (col.kind != legacy::ShnValueKind::String) continue;
                const KoreanResult tr = TranslateKorean(legacy::ShnValueToString(row.values[c]), true);
                if (!tr.korean || tr.source != KoreanSource::Automatic || tr.utf8.empty()) continue;
                const std::size_t limit = col.type == 26 || col.length == 0 ? 0 : col.length - 1;
                auto [it, inserted] = limits.emplace(tr.utf8, limit);
                if (!inserted && limit > 0 && (it->second == 0 || limit < it->second)) it->second = limit;
            }
    }
    std::vector<legacy::KoreanOnlineItem> items;
    for (const auto& [ko, limit] : limits) items.push_back({ko, limit});
    return items;
}

bool StartKoreanOnlineTranslation(EditorState& state, std::vector<legacy::KoreanOnlineItem> items, std::string& message) {
    auto& job = Job();
    if (job.running) { message = L("Eine Online-Übersetzung läuft bereits.", "An online translation is already running."); return false; }
    {
        std::lock_guard lock(job.mutex);
        job.log.clear();
    }
    if (state.project.projectFolder[0] == '\0') {
        message = L("Kein Projektordner - Ergebnisse könnten nicht gespeichert werden.", "No project folder - results could not be saved.");
        return false;
    }
    const std::string key = KoreanApiKey();
    if (key.empty()) {
        message = L("Kein API-Schlüssel: in den Einstellungen eintragen oder ANTHROPIC_API_KEY setzen.",
                    "No API key: enter it in the settings or set ANTHROPIC_API_KEY.");
        return false;
    }
    if (items.empty()) { message = L("Keine unbekannten koreanischen Texte.", "No unknown Korean texts."); return false; }
    if (job.worker.joinable()) job.worker.join();
    {
        std::lock_guard lock(job.mutex);
        job.pending.clear();
    }
    job.saved = 0;
    job.cancel = false;
    job.batchesDone = 0;
    job.texts = items.size();
    job.batchesTotal = static_cast<int>((items.size() + legacy::kKoreanOnlineBatchSize - 1) / legacy::kKoreanOnlineBatchSize);
    job.projectFolder = state.project.projectFolder;
    job.running = true;
    job.worker = std::thread(RunJob, std::move(items), KoreanGlossary(), key);
    message = std::to_string(job.texts) + L(" Texte werden online übersetzt ...", " texts are being translated online ...");
    return true;
}

bool KoreanOnlineRunning() { return Job().running; }

void PollKoreanOnlineTranslation(EditorState& state) {
    auto& job = Job();
    std::vector<std::pair<std::string, legacy::KoreanTranslation>> ready;
    {
        std::lock_guard lock(job.mutex);
        ready.swap(job.pending);
    }
    if (!ready.empty()) {
        std::string message;
        if (job.projectFolder != state.project.projectFolder) {
            AddLog(job, L("Projekt gewechselt - Ergebnisse verworfen.", "Project changed - results discarded."));
        } else if (AddProjectKoreanTranslations(state, ready, message)) {
            job.saved += ready.size();
        } else {
            AddLog(job, message);
        }
    }
    if (!job.running && job.worker.joinable()) {
        job.worker.join();
        AddLog(job, std::to_string(job.saved) + L(" von ", " of ") + std::to_string(job.texts) +
                        L(" Texten übersetzt und im Projekt-Wörterbuch gespeichert.", " texts translated and saved to the project dictionary."));
    }
}

std::vector<std::string> KoreanOnlineLog() {
    auto& job = Job();
    std::lock_guard lock(job.mutex);
    return job.log;
}

// Knopf "Unbekannte koreanische Texte online übersetzen (N)" mit Bestätigung, Fortschritt und Abbrechen.
void DrawKoreanOnlineButton(EditorState& state, int document, const std::vector<int>& peers, const char* id) {
    if (document < 0 || document >= static_cast<int>(state.shnFiles.size())) return;
    auto& job = Job();
    static std::unordered_map<std::string, std::pair<std::string, std::vector<legacy::KoreanOnlineItem>>> cache;
    const std::string key = std::to_string(document) + "|" + std::to_string(state.shnEditCounter) + "|" +
                            std::to_string(ShnDocFile(state, document).rows.size()) + "|" + std::to_string(job.saved) + "|" +
                            state.project.projectFolder;
    auto& [cacheKey, items] = cache[id];
    if (cacheKey != key) { cacheKey = key; items = UnknownKoreanTexts(state, document, peers); }
    const std::string popup = std::string("##koreanOnlineConfirm") + id;
    if (job.running) {
        const float progress = job.batchesTotal > 0 ? static_cast<float>(job.batchesDone) / static_cast<float>(job.batchesTotal) : 0.0f;
        const std::string overlay = std::to_string(job.batchesDone) + " / " + std::to_string(job.batchesTotal) + L(" Blöcke", " batches");
        ImGui::ProgressBar(progress, ImVec2(-90.0f, 0), overlay.c_str());
        ImGui::SameLine();
        if (UI::Button(L("Abbrechen##koreanOnline", "Cancel##koreanOnline"), ImVec2(-1, 0))) job.cancel = true;
    } else if (!items.empty()) {
        const std::string label = std::string(L("Unbekannte koreanische Texte online übersetzen", "Translate unknown Korean texts online")) +
                                  " (" + std::to_string(items.size()) + ")##koreanOnline" + id;
        if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) ImGui::OpenPopup(popup.c_str());
        UI::Tip("Button", (std::string(L("Unbekannte koreanische Texte online übersetzen", "Translate unknown Korean texts online")) + "##koreanOnline").c_str());
    }
    if (ImGui::BeginPopup(popup.c_str())) {
        ImGui::TextWrapped(L("%zu koreanische Texte ohne Wörterbuch-Eintrag werden an die Claude-API (Anthropic) gesendet und mit "
                             "Ihrem API-Schlüssel abgerechnet. Die Ergebnisse landen in <Projekt>/Texte/ko_user.tsv; die Daten "
                             "selbst ändern sich erst über \"Koreanische Texte übersetzen\".",
                             "%zu Korean texts without dictionary entry are sent to the Claude API (Anthropic) and billed to your "
                             "API key. Results go to <project>/Texte/ko_user.tsv; the data itself only changes via \"Translate Korean texts\"."),
                           items.size());
        if (KoreanApiKey().empty())
            ImGui::TextColored(UiTheme::Warning, "%s", L("Kein API-Schlüssel - bitte in den Einstellungen eintragen.", "No API key - please enter it in the settings."));
        if (UI::Button(L("Senden##koreanOnline", "Send##koreanOnline"))) {
            std::string message;
            if (!StartKoreanOnlineTranslation(state, items, message) && KoreanApiKey().empty()) state.settingsOpen = true;
            AddLog(job, message);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (UI::Button(L("Abbrechen##koreanOnlineConfirm", "Cancel##koreanOnlineConfirm"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    const auto log = KoreanOnlineLog();
    if (!log.empty() && (job.running || !items.empty() || job.saved > 0)) {
        const std::size_t first = log.size() > 6 ? log.size() - 6 : 0;
        for (std::size_t i = first; i < log.size(); ++i) ImGui::TextDisabled("%s", log[i].c_str());
    }
}

void DrawKoreanApiKeySettings(EditorState& state) {
    (void)state;
    static char buffer[256] = "";
    ImGui::SeparatorText(L("Koreanische Texte: Online-Übersetzung (optional)", "Korean texts: online translation (optional)"));
    ImGui::TextWrapped("%s", L("Unbekannte koreanische Texte können über die Claude-API übersetzt werden. Dafür ist ein eigener "
                               "API-Schlüssel nötig (console.anthropic.com); die Texte werden dabei an Anthropic gesendet.",
                               "Unknown Korean texts can be translated via the Claude API. This needs your own API key "
                               "(console.anthropic.com); the texts are sent to Anthropic."));
    if (KoreanApiKeyFromEnvironment()) {
        ImGui::TextColored(UiTheme::Success, "%s", L("Schlüssel aus ANTHROPIC_API_KEY wird verwendet.", "Using the key from ANTHROPIC_API_KEY."));
        return;
    }
    const std::string stored = LoadStoredKey();
    if (!stored.empty())
        ImGui::Text(L("Gespeichert: ...%s", "Stored: ...%s"), stored.size() > 4 ? stored.substr(stored.size() - 4).c_str() : "");
    ImGui::SetNextItemWidth(360.0f);
    UI::InputTextWithHint("##koreanApiKey", "sk-ant-...", buffer, sizeof(buffer), ImGuiInputTextFlags_Password);
    ImGui::SameLine();
    if (UI::Button(L("Speichern##koreanApiKey", "Save##koreanApiKey")) && buffer[0] != '\0') {
        state.statusMessage = SaveKoreanApiKey(buffer) ? L("API-Schlüssel gespeichert.", "API key saved.")
                                                       : L("API-Schlüssel konnte nicht gespeichert werden.", "API key could not be saved.");
        std::memset(buffer, 0, sizeof(buffer));
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(stored.empty());
    if (UI::Button(L("Löschen##koreanApiKey", "Delete##koreanApiKey"))) {
        SaveKoreanApiKey("");
        state.statusMessage = L("API-Schlüssel gelöscht.", "API key deleted.");
    }
    ImGui::EndDisabled();
#ifdef _WIN32
    ImGui::TextDisabled("%s", L("Abgelegt verschlüsselt (Windows-Benutzerkonto) in den Editor-Einstellungen, nie im Projekt.",
                                "Stored encrypted (Windows user account) in the editor settings, never in the project."));
#else
    ImGui::TextDisabled("%s", L("Abgelegt in den Editor-Einstellungen (nur für den Benutzer lesbar), nie im Projekt.",
                                "Stored in the editor settings (readable by the user only), never in the project."));
#endif
}

} // namespace nge
