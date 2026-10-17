// EditorCore.cpp
// Design-Tokens, Projektkonfiguration, Einstellungen, zuletzt geöffnet, Tastenkürzel, Toasts, Projektordner.

#include "EditorShared.hpp"

namespace nge {

// Approved package icon cache. Missing package assets are non-fatal and fall back to the
// existing DrawList glyph for that action until the migration of that icon is complete.
app::UiIconAssets gUiIcons;
void ApplyEditorTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 7.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.TabRounding = 5.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    style.FramePadding = ImVec2(9.0f, 7.0f);
    style.WindowPadding = ImVec2(10.0f, 9.0f);
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 12.0f;
    style.IndentSpacing = 18.0f;
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = ImVec2(16.0f, 6.0f);
    style.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                 = UiTheme::TextPrimary;
    c[ImGuiCol_TextDisabled]         = UiTheme::TextSecondary;
    c[ImGuiCol_WindowBg]             = UiTheme::Root;
    c[ImGuiCol_ChildBg]              = UiTheme::Panel;
    c[ImGuiCol_PopupBg]              = ImVec4(0.026f, 0.073f, 0.118f, 0.995f);
    c[ImGuiCol_Border]               = UiTheme::Border;
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = UiTheme::Input;
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.063f, 0.137f, 0.220f, 1.0f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.055f, 0.190f, 0.310f, 1.0f);
    c[ImGuiCol_TitleBg]              = UiTheme::PanelDeep;
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.035f, 0.122f, 0.200f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed]     = UiTheme::PanelDeep;
    c[ImGuiCol_MenuBarBg]            = UiTheme::PanelDeep;
    c[ImGuiCol_ScrollbarBg]          = UiTheme::Root;
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.125f, 0.231f, 0.333f, 0.90f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.075f, 0.360f, 0.560f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive]  = UiTheme::AccentBlue;
    c[ImGuiCol_CheckMark]            = UiTheme::AccentCyan;
    c[ImGuiCol_SliderGrab]           = UiTheme::AccentBlue;
    c[ImGuiCol_SliderGrabActive]     = UiTheme::AccentCyan;
    c[ImGuiCol_Button]               = ImVec4(0.047f, 0.122f, 0.192f, 1.0f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.055f, 0.255f, 0.425f, 1.0f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.035f, 0.360f, 0.720f, 1.0f);
    c[ImGuiCol_Header]               = ImVec4(0.047f, 0.220f, 0.365f, 0.78f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.055f, 0.350f, 0.570f, 0.92f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.075f, 0.430f, 0.720f, 1.0f);
    c[ImGuiCol_Separator]            = UiTheme::Border;
    c[ImGuiCol_SeparatorHovered]     = UiTheme::AccentBlue;
    c[ImGuiCol_SeparatorActive]      = UiTheme::AccentCyan;
    c[ImGuiCol_Tab]                  = ImVec4(0.031f, 0.082f, 0.132f, 1.0f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.047f, 0.275f, 0.465f, 1.0f);
    c[ImGuiCol_TabActive]            = ImVec4(0.035f, 0.333f, 0.690f, 0.94f);
    c[ImGuiCol_TabUnfocused]         = UiTheme::PanelDeep;
    c[ImGuiCol_TabUnfocusedActive]   = UiTheme::PanelAlt;
    c[ImGuiCol_DockingPreview]       = ImVec4(0.075f, 0.549f, 1.000f, 0.68f);
    c[ImGuiCol_DockingEmptyBg]       = UiTheme::Root;
    c[ImGuiCol_NavHighlight]         = UiTheme::AccentCyan;
    c[ImGuiCol_TableHeaderBg]        = UiTheme::PanelAlt;
    c[ImGuiCol_TableBorderStrong]    = UiTheme::Border;
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.090f, 0.165f, 0.235f, 1.0f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.035f, 0.078f, 0.122f, 0.62f);
}
#ifdef _WIN32
// Wird direkt nach glfwCreateWindow() in main() gesetzt - siehe BrowseForFolderWindows, das
// dieses Handle als Besitzerfenster für den Ordnerdialog braucht (IFileDialog::Show).
GLFWwindow* g_appWindowForDialogs = nullptr;
#endif
std::expected<void, std::string> ValidateProjectConfigOutputRoots(const ProjectConfig& cfg) {
    return core::ValidateProjectOutputRoots(
        std::filesystem::path(cfg.projectFolder),
        std::filesystem::path(cfg.clientFolder),
        std::filesystem::path(cfg.serverFolder));
}
std::expected<std::filesystem::path, std::string> ProjectPathForSource(
    const ProjectConfig& cfg,
    core::ProjectOutputSide side,
    const std::filesystem::path& sourcePath) {

    auto roots = ValidateProjectConfigOutputRoots(cfg);
    if (!roots) return std::unexpected(roots.error());

    const std::filesystem::path projectRoot(cfg.projectFolder);
    const std::filesystem::path sourceRoot(
        side == core::ProjectOutputSide::Client ? cfg.clientFolder : cfg.serverFolder);
    return core::ProjectOutputForSource(projectRoot, sourceRoot, sourcePath, side);
}
std::expected<std::filesystem::path, std::string> ProjectPathForRelative(
    const ProjectConfig& cfg,
    core::ProjectOutputSide side,
    const std::filesystem::path& relativePath) {

    auto roots = ValidateProjectConfigOutputRoots(cfg);
    if (!roots) return std::unexpected(roots.error());
    return core::ProjectOutputForRelative(
        std::filesystem::path(cfg.projectFolder), side, relativePath);
}
bool PrepareProjectOutput(
    const std::expected<std::filesystem::path, std::string>& target,
    std::filesystem::path& out,
    std::string* errorOut) {

    if (!target) {
        if (errorOut) *errorOut = target.error();
        return false;
    }
    auto parent = core::EnsureProjectOutputParent(*target);
    if (!parent) {
        if (errorOut) *errorOut = parent.error();
        return false;
    }
    out = *target;
    return true;
}
std::filesystem::path PreferProjectOverride(
    const ProjectConfig& cfg,
    core::ProjectOutputSide side,
    const std::filesystem::path& sourcePath) {

    auto target = ProjectPathForSource(cfg, side, sourcePath);
    std::error_code ec;
    if (target && std::filesystem::is_regular_file(*target, ec)) return *target;
    return sourcePath;
}
std::expected<std::filesystem::path, std::string> ProjectClientRessystemPath(
    const ProjectConfig& cfg, const std::filesystem::path& relative) {
    return ProjectPathForRelative(
        cfg, core::ProjectOutputSide::Client,
        std::filesystem::path("ressystem") / relative);
}
std::expected<std::filesystem::path, std::string> ProjectServerShinePath(
    const ProjectConfig& cfg, const std::filesystem::path& relative) {
    return ProjectPathForRelative(
        cfg, core::ProjectOutputSide::Server,
        std::filesystem::path("9Data") / "Shine" / relative);
}
std::expected<std::filesystem::path, std::string> SaveProjectShineOverride(
    core::legacy::ShineTextFile& file,
    const std::expected<std::filesystem::path, std::string>& target) {

    std::filesystem::path path;
    std::string error;
    if (!PrepareProjectOutput(target, path, &error)) return std::unexpected(error);
    auto saved = core::legacy::SaveShineTextFile(file, path);
    if (!saved) return std::unexpected(saved.error());
    file.path = path;
    return path;
}
bool IsProjectSidePath(
    const ProjectConfig& cfg,
    core::ProjectOutputSide side,
    const std::filesystem::path& path) {

    if (cfg.projectFolder[0] == '\0' || path.empty()) return false;
    const auto sideRoot = std::filesystem::path(cfg.projectFolder) /
        (side == core::ProjectOutputSide::Client ? "Client" : "Server");
    std::error_code ec;
    auto relative = std::filesystem::relative(path, sideRoot, ec);
    if (ec || relative.empty() || relative.is_absolute()) return false;
    for (const auto& part : relative) if (part == "..") return false;
    return true;
}
bool HasParentTraversal(const std::filesystem::path& path) {
    for (const auto& part : path) {
        if (part == "..") return true;
    }
    return false;
}
// A legacy/export UI may contain either the original Client source path or an already
// project-side destination. Both are accepted, but arbitrary third-party destinations and
// explicit ".." traversals are rejected: edited/generated Fiesta data belongs to the project.
std::expected<std::filesystem::path, std::string> ProjectClientOutputForRequestedPath(
    const ProjectConfig& cfg,
    const std::filesystem::path& requestedPath) {

    if (requestedPath.empty())
        return std::unexpected("Leerer Client-Ausgabepfad.");
    if (HasParentTraversal(requestedPath))
        return std::unexpected("Unsicherer Client-Ausgabepfad mit '..': " + requestedPath.string());

    auto roots = ValidateProjectConfigOutputRoots(cfg);
    if (!roots) return std::unexpected(roots.error());

    if (IsProjectSidePath(cfg, core::ProjectOutputSide::Client, requestedPath))
        return requestedPath.lexically_normal();

    auto mapped = ProjectPathForSource(
        cfg, core::ProjectOutputSide::Client, requestedPath);
    if (!mapped) {
        return std::unexpected(
            "Client-Ausgabe muss innerhalb der konfigurierten Client-Quelle oder unter "
            "<Projekt>/Client liegen: " + mapped.error());
    }
    return *mapped;
}
std::expected<std::filesystem::path, std::string> PrepareProjectClientRequestedOutput(
    const ProjectConfig& cfg,
    const std::filesystem::path& requestedPath) {

    auto target = ProjectClientOutputForRequestedPath(cfg, requestedPath);
    if (!target) return std::unexpected(target.error());
    auto parent = core::EnsureProjectOutputParent(*target);
    if (!parent) return std::unexpected(parent.error());
    return *target;
}
// Schreibt die Projekt-Konfiguration als einfache "schlüssel=wert"-Datei (bewusst kein
// JSON - im restlichen Code werden ausschließlich native/legacy Formate ohne
// JSON-Abhängigkeit verwendet, siehe docs/MAP_FORMAT.md) nach <projectFolder>/project.tsproj.
bool SaveProjectConfig(const ProjectConfig& cfg, std::string* errorOut) {
    if (cfg.projectFolder[0] == '\0') {
        if (errorOut) *errorOut = "Kein Projekt-Ordner angegeben.";
        return false;
    }
    auto roots = ValidateProjectConfigOutputRoots(cfg);
    if (!roots) {
        if (errorOut) *errorOut = roots.error();
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(cfg.projectFolder, ec);
    if (!ec) std::filesystem::create_directories(std::filesystem::path(cfg.projectFolder) / "Client", ec);
    if (!ec) std::filesystem::create_directories(std::filesystem::path(cfg.projectFolder) / "Server", ec);
    if (ec) {
        if (errorOut) *errorOut = "Projekt-Ausgabestruktur konnte nicht angelegt werden: " + ec.message();
        return false;
    }
    const std::filesystem::path path = std::filesystem::path(cfg.projectFolder) / "project.tsproj";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (errorOut) *errorOut = "Konnte nicht schreiben: " + path.string();
        return false;
    }
    out << "name=" << cfg.name << "\n";
    out << "client_folder=" << cfg.clientFolder << "\n";
    out << "server_folder=" << cfg.serverFolder << "\n";
    return true;
}
// Lädt eine zuvor gespeicherte Projekt-Konfiguration aus <projectFolder>/project.tsproj,
// falls vorhanden - wird beim Anlegen/Wählen eines Projekt-Ordners versucht, damit ein
// bereits bestehendes Projekt seine Client-/Server-Ordner-Angaben wiederfindet.
void TryLoadProjectConfig(ProjectConfig& cfg) {
    const std::filesystem::path path = std::filesystem::path(cfg.projectFolder) / "project.tsproj";
    std::ifstream in(path, std::ios::binary);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "name") std::snprintf(cfg.name, sizeof(cfg.name), "%s", value.c_str());
        else if (key == "client_folder") std::snprintf(cfg.clientFolder, sizeof(cfg.clientFolder), "%s", value.c_str());
        else if (key == "server_folder") std::snprintf(cfg.serverFolder, sizeof(cfg.serverFolder), "%s", value.c_str());
    }
}
std::filesystem::path NextGenUserSettingsDir() {
    std::filesystem::path base;
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData && *appData) base = appData;
    else if (const char* profile = std::getenv("USERPROFILE"); profile && *profile) base = profile;
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) base = xdg;
    else if (const char* home = std::getenv("HOME"); home && *home) base = std::filesystem::path(home) / ".config";
#endif
    if (base.empty()) {
        std::error_code ec;
        base = std::filesystem::temp_directory_path(ec);
        if (ec) base = ".";
    }
    return base / "NextGen-Editor";
}
bool MapSupportsTerrainEditing(const EditorState& state) {
    return state.mapHasHeightmap &&
           state.heightmap.Width() > 0 && state.heightmap.Height() > 0;
}
bool MapSupportsObjectGeometryEditing(const EditorState& state) {
    return !state.standaloneNifMap;
}
bool RequireWritableMapGeometry(EditorState& state) {
    if (MapSupportsObjectGeometryEditing(state)) return true;
    state.statusMessage = L(
        "Standalone-NIF-Karte: Geometrie ist read-only, solange kein verifizierter NIF-Writer existiert.",
        "Standalone NIF map: geometry is read-only until a verified NIF writer exists.");
    return false;
}
const char* MapWorkspacePresetName(int preset) {
    switch (preset) {
        case 1: return "3D-Fokus";
        case 2: return "Terrain / 2D";
        case 3: return "Daten / Szene";
        default: return "Standard";
    }
}
void SaveWorkspaceSettings(const EditorState& state) {
    const auto dir = NextGenUserSettingsDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return;
    std::ofstream out(dir / "workspace.txt", std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << "map_preset=" << std::clamp(state.mapWorkspacePreset,0,3) << "\n";
    out << "autosave_enabled=" << (state.autosaveEnabled ? 1 : 0) << "\n";
    out << "autosave_minutes=" << std::clamp(state.autosaveMinutes,1,60) << "\n";
}
void LoadWorkspaceSettings(EditorState& state) {
    if (state.workspacePresetLoaded) return;
    state.workspacePresetLoaded = true;
    std::ifstream in(NextGenUserSettingsDir() / "workspace.txt", std::ios::binary);
    if (!in) return;
    std::string line;
    while (std::getline(in,line)) {
        constexpr std::string_view prefix="map_preset=";
        if (line.rfind("autosave_enabled=",0) == 0) { state.autosaveEnabled = std::atoi(line.c_str()+17) != 0; continue; }
        if (line.rfind("autosave_minutes=",0) == 0) { state.autosaveMinutes = std::clamp(std::atoi(line.c_str()+17),1,60); continue; }
        if (line.rfind(prefix,0) != 0) continue;
        state.mapWorkspacePreset=std::clamp(std::atoi(line.c_str()+static_cast<std::ptrdiff_t>(prefix.size())),0,3);
    }
}
void RequestMapWorkspacePreset(EditorState& state,int preset) {
    state.pendingMapWorkspacePreset=std::clamp(preset,0,3);
    state.statusMessage=std::string("Workspace-Preset wird angewendet: ")+
                        MapWorkspacePresetName(state.pendingMapWorkspacePreset);
}
std::string NormalizedRecentPath(const std::string& input) {
    if (input.empty()) return {};
    std::error_code ec;
    auto path = std::filesystem::path(input);
    auto absolute = std::filesystem::absolute(path, ec);
    if (!ec) path = absolute;
    return path.lexically_normal().string();
}
void SaveRecentEntries(const EditorState& state) {
    const auto dir = NextGenUserSettingsDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return;
    std::ofstream out(dir / "recent.txt", std::ios::binary | std::ios::trunc);
    if (!out) return;
    for (const auto& path : state.recentProjects) out << "project=" << path << "\n";
    for (const auto& path : state.recentMaps) out << "map=" << path << "\n";
}
void LoadRecentEntries(EditorState& state) {
    if (state.recentEntriesLoaded) return;
    state.recentEntriesLoaded = true;
    std::ifstream in(NextGenUserSettingsDir() / "recent.txt", std::ios::binary);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string kind = line.substr(0, eq);
        const std::string path = NormalizedRecentPath(line.substr(eq + 1));
        if (path.empty()) continue;
        auto& list = kind == "project" ? state.recentProjects : state.recentMaps;
        if (kind != "project" && kind != "map") continue;
        if (std::find(list.begin(), list.end(), path) == list.end()) list.push_back(path);
        if (list.size() >= 8) continue;
    }
    if (state.recentProjects.size() > 8) state.recentProjects.resize(8);
    if (state.recentMaps.size() > 8) state.recentMaps.resize(8);
}
void TouchRecentPath(EditorState& state, std::vector<std::string>& list, const std::string& path) {
    const std::string normalized = NormalizedRecentPath(path);
    if (normalized.empty()) return;
    list.erase(std::remove(list.begin(), list.end(), normalized), list.end());
    list.insert(list.begin(), normalized);
    if (list.size() > 8) list.resize(8);
    SaveRecentEntries(state);
}
void TouchRecentProject(EditorState& state, const std::string& path) {
    TouchRecentPath(state, state.recentProjects, path);
}
void TouchRecentMap(EditorState& state, const std::string& path) {
    TouchRecentPath(state, state.recentMaps, path);
}
std::size_t DirtyShnDocumentCount(const EditorState& state) {
    return static_cast<std::size_t>(std::count_if(state.shnFiles.begin(), state.shnFiles.end(),
        [](const auto& doc) { return doc.dirty; }));
}
bool LoadProjectFolderIntoState(EditorState& state, const std::string& folder) {
    if (folder.empty()) return false;
    const auto projectFile = std::filesystem::path(folder) / "project.tsproj";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(projectFile, ec)) {
        state.statusMessage = "Kein NextGen-Projekt gefunden: " + projectFile.string();
        return false;
    }
    state.project = ProjectConfig{};
    std::snprintf(state.project.projectFolder, sizeof(state.project.projectFolder), "%s", folder.c_str());
    TryLoadProjectConfig(state.project);
    state.project.hasProject = true;
    TouchRecentProject(state, folder);
    state.statusMessage = "Projekt geladen: " + NormalizedRecentPath(folder);
    state.screen = AppScreen::ProjectHub;
    return true;
}
const std::vector<ShortcutKeyOption>& ShortcutKeyOptions() {
    static const std::vector<ShortcutKeyOption> keys = {
        {ImGuiKey_A,"A"},{ImGuiKey_B,"B"},{ImGuiKey_C,"C"},{ImGuiKey_D,"D"},
        {ImGuiKey_E,"E"},{ImGuiKey_F,"F"},{ImGuiKey_G,"G"},{ImGuiKey_H,"H"},
        {ImGuiKey_I,"I"},{ImGuiKey_J,"J"},{ImGuiKey_K,"K"},{ImGuiKey_L,"L"},
        {ImGuiKey_M,"M"},{ImGuiKey_N,"N"},{ImGuiKey_O,"O"},{ImGuiKey_P,"P"},
        {ImGuiKey_Q,"Q"},{ImGuiKey_R,"R"},{ImGuiKey_S,"S"},{ImGuiKey_T,"T"},
        {ImGuiKey_U,"U"},{ImGuiKey_V,"V"},{ImGuiKey_W,"W"},{ImGuiKey_X,"X"},
        {ImGuiKey_Y,"Y"},{ImGuiKey_Z,"Z"},
        {ImGuiKey_0,"0"},{ImGuiKey_1,"1"},{ImGuiKey_2,"2"},{ImGuiKey_3,"3"},
        {ImGuiKey_4,"4"},{ImGuiKey_5,"5"},{ImGuiKey_6,"6"},{ImGuiKey_7,"7"},
        {ImGuiKey_8,"8"},{ImGuiKey_9,"9"},
        {ImGuiKey_F2,"F2"},{ImGuiKey_F3,"F3"},{ImGuiKey_F4,"F4"},{ImGuiKey_F5,"F5"},
        {ImGuiKey_F6,"F6"},{ImGuiKey_F7,"F7"},{ImGuiKey_F8,"F8"},{ImGuiKey_F9,"F9"},
        {ImGuiKey_F10,"F10"},{ImGuiKey_F11,"F11"},{ImGuiKey_F12,"F12"},
        {ImGuiKey_Home,"Home"},{ImGuiKey_End,"Ende"},{ImGuiKey_Insert,"Einfg"},
        {ImGuiKey_Delete,"Entf"},{ImGuiKey_Space,"Leertaste"},{ImGuiKey_Enter,"Enter"}
    };
    return keys;
}
const char* ShortcutKeyName(ImGuiKey key) {
    // Sondertasten werden lokalisiert, damit dynamische Shortcut-Hinweise nicht in
    // englischer UI weiterhin deutsche Key-Namen wie "Ende"/"Entf" anzeigen.
    switch (key) {
        case ImGuiKey_End: return L("Ende","End");
        case ImGuiKey_Insert: return L("Einfg","Insert");
        case ImGuiKey_Delete: return L("Entf","Del");
        case ImGuiKey_Space: return L("Leertaste","Space");
        default: break;
    }
    for (const auto& option : ShortcutKeyOptions())
        if (option.key == key) return option.name;
    return "-";
}
std::string ShortcutLabel(const EditorState::ShortcutBinding& binding) {
    std::string out;
    if (binding.ctrl) out += L("Strg+","Ctrl+");
    if (binding.shift) out += "Shift+";
    if (binding.alt) out += "Alt+";
    out += ShortcutKeyName(binding.key);
    return out;
}
bool SameShortcut(const EditorState::ShortcutBinding& a, const EditorState::ShortcutBinding& b) {
    return a.key != ImGuiKey_None && a.key == b.key &&
           a.ctrl == b.ctrl && a.shift == b.shift && a.alt == b.alt;
}
bool ShortcutPressed(const EditorState::ShortcutBinding& binding) {
    if (binding.key == ImGuiKey_None) return false;
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl != binding.ctrl || io.KeyShift != binding.shift || io.KeyAlt != binding.alt) return false;
    return ImGui::IsKeyPressed(binding.key, false);
}
void ResetShortcutSettings(EditorState& state) {
    state.shortcutPalette = {ImGuiKey_P, true, false, false};
    state.shortcutSave = {ImGuiKey_S, true, false, false};
    state.shortcutGizmoMove = {ImGuiKey_1, false, false, false};
    state.shortcutGizmoRotate = {ImGuiKey_2, false, false, false};
    state.shortcutGizmoScale = {ImGuiKey_3, false, false, false};
    state.shortcutFocus = {ImGuiKey_F, false, false, false};
    state.shortcutGround = {ImGuiKey_End, false, false, false};
    state.shortcutDuplicate = {ImGuiKey_D, true, false, false};
    state.shortcutDelete = {ImGuiKey_Delete, false, false, false};
}
std::array<std::pair<const char*, EditorState::ShortcutBinding*>, 9>
ShortcutSettings(EditorState& state) {
    return {{
        {"palette",&state.shortcutPalette},
        {"save",&state.shortcutSave},
        {"gizmo_move",&state.shortcutGizmoMove},
        {"gizmo_rotate",&state.shortcutGizmoRotate},
        {"gizmo_scale",&state.shortcutGizmoScale},
        {"focus",&state.shortcutFocus},
        {"ground",&state.shortcutGround},
        {"duplicate",&state.shortcutDuplicate},
        {"delete",&state.shortcutDelete},
    }};
}
void SaveShortcutSettings(EditorState& state) {
    const auto dir = NextGenUserSettingsDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return;
    std::ofstream out(dir / "shortcuts.txt", std::ios::binary | std::ios::trunc);
    if (!out) return;
    for (const auto& [name, binding] : ShortcutSettings(state)) {
        out << name << "=" << static_cast<int>(binding->key) << ","
            << (binding->ctrl ? 1 : 0) << "," << (binding->shift ? 1 : 0) << ","
            << (binding->alt ? 1 : 0) << "\n";
    }
}
void LoadShortcutSettings(EditorState& state) {
    if (state.shortcutsLoaded) return;
    state.shortcutsLoaded = true;
    std::ifstream in(NextGenUserSettingsDir() / "shortcuts.txt", std::ios::binary);
    if (!in) return;
    std::unordered_map<std::string, EditorState::ShortcutBinding*> targets;
    for (const auto& [name, binding] : ShortcutSettings(state)) targets.emplace(name, binding);
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const auto it = targets.find(line.substr(0, eq));
        if (it == targets.end()) continue;
        int key = 0, ctrl = 0, shift = 0, alt = 0;
        if (std::sscanf(line.c_str() + static_cast<std::ptrdiff_t>(eq + 1),
                        "%d,%d,%d,%d", &key, &ctrl, &shift, &alt) == 4) {
            const bool supported = std::any_of(ShortcutKeyOptions().begin(), ShortcutKeyOptions().end(),
                [&](const auto& option) { return static_cast<int>(option.key) == key; });
            if (supported) *it->second = {static_cast<ImGuiKey>(key), ctrl != 0, shift != 0, alt != 0};
        }
    }
}
// ---- Viewport-Einstellungen (Unreal-Navigation, Overlays, Snap-Raster) -------------------------
void SaveViewportSettings(EditorState& state) {
    state.viewportSettingsDirty = false;
    const auto dir = NextGenUserSettingsDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return;
    std::ofstream out(dir / "viewport.txt", std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << "unreal_navigation=" << (state.unrealNavigation ? 1 : 0) << "\n"
        << "camera_speed=" << state.cameraSpeedSetting << "\n"
        << "surface_snap=" << (state.surfaceSnap ? 1 : 0) << "\n"
        << "pivot_active=" << (state.pivotAtActiveObject ? 1 : 0) << "\n"
        << "actor_snap=" << (state.actorSnap ? 1 : 0) << "\n"
        << "viewport_layout=" << state.viewportLayout << "\n"
        << "show_stats=" << (state.showViewportStats ? 1 : 0) << "\n"
        << "show_grid=" << (state.showViewportGrid ? 1 : 0) << "\n"
        << "show_axes=" << (state.showViewportAxes ? 1 : 0) << "\n"
        << "show_walk_collision=" << (state.showWalkCollision3D ? 1 : 0) << "\n"
        << "marquee_crossing=" << (state.marqueeMode == core::level::MarqueeMode::Crossing ? 1 : 0) << "\n"
        << "gizmo_snap=" << (state.objectGizmoSnap ? 1 : 0) << "\n"
        << "move_snap=" << state.objectMoveSnap << "\n"
        << "rotate_snap=" << state.objectRotateSnap << "\n"
        << "scale_snap=" << state.objectScaleSnap << "\n"
        << "playtest_speed=" << state.playtestConfig.runSpeed << "\n"
        << "view_mode=" << state.viewportViewMode << "\n"
        << "msaa=" << state.viewportMsaaSamples << "\n"
        << "glow=" << (state.viewportGlow ? 1 : 0) << "\n"
        << "object_animation=" << (state.viewportObjectAnimation ? 1 : 0) << "\n"
        << "vertex_colors=" << (state.viewportVertexColors ? 1 : 0) << "\n"
        << "map_lighting=" << (state.viewportMapLighting ? 1 : 0) << "\n"
        << "map_background=" << (state.viewportMapBackground ? 1 : 0) << "\n"
        << "fog=" << (state.viewportFog ? 1 : 0) << "\n";
}
void LoadViewportSettings(EditorState& state) {
    if (state.viewportSettingsLoaded) return;
    state.viewportSettingsLoaded = true;
    std::ifstream in(NextGenUserSettingsDir() / "viewport.txt", std::ios::binary);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const char* value = line.c_str() + static_cast<std::ptrdiff_t>(eq + 1);
        const float f = static_cast<float>(std::atof(value));
        const bool b = std::atoi(value) != 0;
        if (key == "unreal_navigation") state.unrealNavigation = b;
        else if (key == "camera_speed")
            state.cameraSpeedSetting = std::clamp(std::atoi(value), core::level::kMinCameraSpeedSetting,
                                                  core::level::kMaxCameraSpeedSetting);
        else if (key == "surface_snap") state.surfaceSnap = b;
        else if (key == "pivot_active") state.pivotAtActiveObject = b;
        else if (key == "actor_snap") state.actorSnap = b;
        else if (key == "viewport_layout") { const int l = std::atoi(value); state.viewportLayout = (l == 2 || l == 4) ? l : 1; }
        else if (key == "show_stats") state.showViewportStats = b;
        else if (key == "show_grid") state.showViewportGrid = b;
        else if (key == "show_axes") state.showViewportAxes = b;
        else if (key == "show_walk_collision") state.showWalkCollision3D = b;
        else if (key == "marquee_crossing")
            state.marqueeMode = b ? core::level::MarqueeMode::Crossing : core::level::MarqueeMode::Inside;
        else if (key == "gizmo_snap") state.objectGizmoSnap = b;
        else if (key == "move_snap" && f > 0.0f && std::isfinite(f)) state.objectMoveSnap = f;
        else if (key == "rotate_snap" && f > 0.0f && std::isfinite(f)) state.objectRotateSnap = f;
        else if (key == "scale_snap" && f > 0.0f && std::isfinite(f)) state.objectScaleSnap = f;
        else if (key == "playtest_speed" && f >= 10.0f && f <= 2000.0f) state.playtestConfig.runSpeed = f;
        else if (key == "view_mode") state.viewportViewMode = std::clamp(std::atoi(value), 0, 7);
        else if (key == "msaa") state.viewportMsaaSamples = std::clamp(std::atoi(value), 1, 8);
        else if (key == "glow") state.viewportGlow = b;
        else if (key == "object_animation") state.viewportObjectAnimation = b;
        else if (key == "vertex_colors") state.viewportVertexColors = b;
        else if (key == "map_lighting") state.viewportMapLighting = b;
        else if (key == "map_background") state.viewportMapBackground = b;
        else if (key == "fog") state.viewportFog = b;
    }
}
// ---- Kamera-Lesezeichen je Karte (Strg+0..9 setzen, 0..9 anspringen) ---------------------------
std::filesystem::path CameraBookmarkFile(const std::string& mapStem) {
    return NextGenUserSettingsDir() / "bookmarks" / (core::level::BookmarkFileStem(mapStem) + ".txt");
}
void EnsureCameraBookmarksLoaded(EditorState& state) {
    const std::string stem = state.legacySaveStem;
    if (state.cameraBookmarksForMap == stem) return;
    state.cameraBookmarksForMap = stem;
    state.cameraBookmarks = {};
    if (stem.empty()) return;
    std::ifstream in(CameraBookmarkFile(stem), std::ios::binary);
    if (!in) return;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    state.cameraBookmarks = core::level::CameraBookmarkSet::Parse(text);
}
void SaveCameraBookmarks(const EditorState& state) {
    if (state.cameraBookmarksForMap.empty()) return;
    const auto file = CameraBookmarkFile(state.cameraBookmarksForMap);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) return;
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (out) out << state.cameraBookmarks.Serialize();
}
std::string CompactToastText(const std::string& message) {
    std::string text = message;
    if (const auto nl = text.find('\n'); nl != std::string::npos) text.resize(nl);
    if (text.size() > 220) text.resize(217), text += "...";
    return text;
}
void PushToast(EditorState& state, const std::string& message, bool error) {
    const std::string text = CompactToastText(message);
    if (text.empty()) return;
    if (!state.toasts.empty() && state.toasts.back().text == text) {
        state.toasts.back().remaining = 4.0f;
        state.toasts.back().error = error;
        return;
    }
    state.toasts.push_back({text, 4.0f, error});
    while (state.toasts.size() > 3) state.toasts.pop_front();
}
void SyncStatusToast(EditorState& state) {
    if (state.statusMessage.empty() || state.statusMessage == state.lastToastedStatus) return;
    state.lastToastedStatus = state.statusMessage;
    std::string lower = state.statusMessage;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    const bool error = lower.find("fehler") != std::string::npos ||
                       lower.find("fehlgeschlagen") != std::string::npos ||
                       lower.find("nicht gefunden") != std::string::npos;
    PushToast(state, state.statusMessage, error);

    // Jede neue Statusmeldung landet zusätzlich dauerhaft im Ausgabeprotokoll (Output Log),
    // damit Meldungen nicht mit dem Toast verschwinden.
    const char* category = "Editor";
    switch (state.screen) {
        case AppScreen::MapEditorWorkspace: category = state.playtestActive ? "Spieltest" : "Karte"; break;
        case AppScreen::ShnEditor: category = "SHN"; break;
        case AppScreen::KfmBrowser: category = "KFM"; break;
        case AppScreen::ProjectHub:
        case AppScreen::NewProjectConfig:
        case AppScreen::MapEditorLauncher: category = "Projekt"; break;
        default: break;
    }
    const auto severity = error ? core::level::LogSeverity::Error
                                : core::level::ClassifyLogMessage(state.statusMessage);
    state.editorLog.Push(severity, category, state.statusMessage, ImGui::GetTime());
}
void DrawToasts(EditorState& state) {
    if (state.toasts.empty()) return;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 anchor(viewport->WorkPos.x + viewport->WorkSize.x - 18.0f,
                  viewport->WorkPos.y + viewport->WorkSize.y - 18.0f);
    constexpr float width = 390.0f;
    int index = static_cast<int>(state.toasts.size());
    for (auto it = state.toasts.rbegin(); it != state.toasts.rend(); ++it) {
        auto& toast = *it;
        toast.remaining -= ImGui::GetIO().DeltaTime;
        ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2(1.0f,1.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        ImGui::PushStyleColor(ImGuiCol_Border,
            toast.error ? ImVec4(0.85f,0.25f,0.22f,0.95f) : ImVec4(0.12f,0.52f,0.78f,0.95f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        const std::string id = "##toast" + std::to_string(--index);
        ImGui::Begin(id.c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
        ImGui::TextColored(toast.error ? ImVec4(1.0f,0.42f,0.36f,1.0f)
                                       : ImVec4(0.30f,0.78f,1.0f,1.0f),
                           "%s", toast.error ? L("FEHLER","ERROR") : "NEXTGEN");
        ImGui::Separator();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 28.0f);
        ImGui::TextWrapped("%s", toast.text.c_str());
        ImGui::PopTextWrapPos();
        const float h = ImGui::GetWindowHeight();
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        anchor.y -= h + 8.0f;
    }
    std::erase_if(state.toasts, [](const EditorState::Toast& t) { return t.remaining <= 0.0f; });
}
// Listet Dateien mit einer der angegebenen Endungen unter root (rekursiv, begrenzte Tiefe),
// relative Pfade zu root. Für Asset-Picker (Textur-/Modell-Auswahl) - siehe DrawAssetPickerPopup.
std::vector<std::string> ListFilesByExtension(const std::filesystem::path& root,
                                               const std::vector<std::string>& extensionsLower,
                                               int maxDepth) {
    std::vector<std::string> result;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return result;
    }
    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; it != end && !ec; it.increment(ec)) {
        if (it->is_directory(ec)) {
            if (maxDepth >= 0 && it.depth() >= maxDepth) it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (std::find(extensionsLower.begin(), extensionsLower.end(), ext) == extensionsLower.end()) continue;
        result.push_back(std::filesystem::relative(it->path(), root, ec).string());
    }
    std::sort(result.begin(), result.end());
    return result;
}
std::string ToLegacyResmapModelPath(std::string relativePath) {
    for (char& c : relativePath) {
        if (c == '/') c = '\\';
    }
    if (relativePath.empty()) return relativePath;
    std::string lower = relativePath;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower.rfind("resmap\\", 0) != 0) relativePath = "resmap\\" + relativePath;
    return relativePath;
}
// Findet einen Unterordner unter resmapRoot, dessen Name (Groß-/Kleinschreibung egal) einem
// der Kandidaten entspricht - z.B. "fieldTexture" für Texturen, "nif"/"nifs" für Objekt-Modelle
// (Nutzer hat "nif" angegeben, das Original-Testkorpus dieser Sitzung hatte "nifs" - beide
// werden akzeptiert, falls die tatsächliche Benennung abweicht).
std::optional<std::filesystem::path> FindNamedSubfolder(
    const std::filesystem::path& resmapRoot, const std::vector<std::string>& namesLower) {
    std::error_code ec;
    if (!std::filesystem::exists(resmapRoot, ec)) return std::nullopt;
    for (const auto& entry : std::filesystem::directory_iterator(
             resmapRoot, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec || !entry.is_directory(ec)) continue;
        std::string name = entry.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (std::find(namesLower.begin(), namesLower.end(), name) != namesLower.end()) {
            return entry.path();
        }
    }
    return std::nullopt;
}
// Breitensuche: sucht unterhalb von `root` (root selbst = Ebene 0, bis maxDepth Ebenen tief)
// einen Ordner namens `nameLower` (case-insensitiv), der `accept` erfuellt. Der flachste Treffer
// gewinnt. Wird fuer die automatische Ordnerfindung aus den Projekt-Ordnern genutzt.
std::optional<std::filesystem::path> FindDirBreadthFirst(
    const std::filesystem::path& root, const std::string& nameLower, int maxDepth,
    const std::function<bool(const std::filesystem::path&)>& accept) {
    std::error_code ec;
    if (root.empty() || !std::filesystem::is_directory(root, ec)) return std::nullopt;
    std::deque<std::pair<std::filesystem::path, int>> queue;
    queue.emplace_back(root, 0);
    while (!queue.empty()) {
        auto [dir, depth] = queue.front();
        queue.pop_front();
        std::string name = dir.filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == nameLower && (!accept || accept(dir))) return dir;
        if (depth >= maxDepth) continue;
        for (const auto& entry : std::filesystem::directory_iterator(
                 dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (ec) { ec.clear(); continue; }
            std::error_code ec2;
            if (entry.is_directory(ec2) && !ec2) queue.emplace_back(entry.path(), depth + 1);
        }
    }
    return std::nullopt;
}
// Client-Ordner -> ressystem (enthaelt TownPortal.shn, MapInfo.shn, NpcDialogData.shn, ...).
std::optional<std::filesystem::path> FindClientRessystem(const std::filesystem::path& clientFolder) {
    return FindDirBreadthFirst(clientFolder, "ressystem", 3, nullptr);
}
// Server-Ordner -> 9Data/Shine (enthaelt QuestData.shn, World/NPC.txt, MobRegen/, ...). Der
// Nutzer kann beim Anlegen des Projekts den Server-Hauptordner, 9Data oder Shine selbst angeben.
std::optional<std::filesystem::path> FindServerShineRoot(const std::filesystem::path& serverFolder) {
    auto looksLikeShine = [](const std::filesystem::path& p) {
        std::error_code ec;
        return std::filesystem::exists(p / "World", ec) || std::filesystem::exists(p / "MobRegen", ec) ||
               std::filesystem::exists(p / "QuestData.shn", ec) || std::filesystem::exists(p / "MapInfo.shn", ec);
    };
    if (auto found = FindDirBreadthFirst(serverFolder, "shine", 4, looksLikeShine)) return found;
    if (!serverFolder.empty() && looksLikeShine(serverFolder)) return serverFolder;
    return std::nullopt;
}
// Leitet aus den beim Anlegen des Projekts angegebenen Client-/Server-Ordnern alle weiteren
// Ordner ab, die die Editoren brauchen (ressystem, Server-Shine-Ordner) - Nutzerwunsch: "beim
// Anlegen des Projektes werden bereits Client und Serverpfad angegeben, der Rest sollte
// automatisch funktionieren". Laeuft jeden Frame, tut aber nur etwas, wenn sich die
// Projekt-Ordner geaendert haben. Manuell gesetzte Ordner werden bei einem Wechsel der
// Projekt-Ordner ueberschrieben, sonst nicht angefasst.
void SyncProjectRoots(EditorState& state) {
    SyncTownPortalMarks(state);
    const std::string key = std::string(state.project.clientFolder) + "|" + state.project.serverFolder;
    if (key == state.projectRootsKey) return;
    state.projectRootsKey = key;
    if (state.project.clientFolder[0] != '\0') {
        if (auto found = FindClientRessystem(state.project.clientFolder)) {
            state.npcDialogRessystemRoot = found->string();
            state.shnClientRoot = found->string();
        }
    }
    state.charRoot.clear();
    state.itemRoot.clear();
    state.interfaceRoot.clear();
    state.itemViewBuilt = false;
    state.avatarKey.clear();
    if (state.project.clientFolder[0] != '\0') {
        if (auto found = FindDirBreadthFirst(state.project.clientFolder, "reschar", 3, nullptr)) state.charRoot = found->string();
        if (auto found = FindDirBreadthFirst(state.project.clientFolder, "resitem", 3, nullptr)) state.itemRoot = found->string();
        if (auto found = FindDirBreadthFirst(state.project.clientFolder, "resmenu", 3, nullptr)) state.interfaceRoot = found->string();
    }
    if (state.project.serverFolder[0] != '\0') {
        if (auto found = FindServerShineRoot(state.project.serverFolder)) {
            state.shineTextRoot = found->string();
            state.shnServerRoot = found->string();
        }
    }
    // Alles, was aus diesen Ordnern geladen wurde, neu laden.
    state.npcDialogLoaded = false;
    state.npcDialogDoc = state.mobViewInfoDoc = state.questDialogDoc = -1;
    state.itemInfoDoc = state.townPortalDoc = state.mapInfoDoc = -1;
    state.npcTextLoaded = false;
    state.mobRegenTextLoaded = false;
    state.shopTextLoaded = false;
    state.questDataLoaded = false;
    state.questDialogLoaded = false;
    state.questDirty = false;
    state.townPortalDirty = false;
    state.recallCoordDirty = false;
    state.selectedQuestIdx = -1;
    state.questTextMapBuilt = false;
    state.questTextMap.clear();
    state.questListKey.clear();
    state.itemInfoLoaded = false;
    state.itemLookupTried = false;
    state.itemEntries.clear();
    state.itemByInx.clear();
    state.itemInxById.clear();
    state.itemIconByInx.clear();
    state.itemIconsTried = false;
    state.iconAtlasSizes.clear();
    state.mobViewInfoLoaded = false;
    state.townPortalLoaded = false;
    state.recallCoordLoaded = false;
    state.townPortalDirty = false;
    state.recallCoordDirty = false;
    state.mapInfoLoaded = false;
    state.portalDataSig.clear();
    state.shnAutoLoaded = false;
    state.interfaceAssetsScanned = false;
    state.interfaceAssets.clear();
    state.interfaceSelectedAsset = -1;
    state.interfaceAssetFilter[0] = '\0';
    state.interfaceOverridesOnly = false;
    state.interfaceChangedOverridesOnly = false;
    state.interfaceOverrideStateByAsset.clear();
    state.dropTableLoaded = false;
    state.dropTableDirty = false;
    state.dropTableFile = core::legacy::ShineTextFile{};
    state.dropTableLoadError.clear();
    state.dropTableSelectedRecord = -1;
    state.dropTableFilter[0] = '\0';
    state.dropTableProblemsOnly = false;
    state.dropGroups = core::legacy::DropGroupCatalog{};
    state.dropGroupsTried = false;
    state.dropGroupsStatus.clear();
}

} // namespace nge
