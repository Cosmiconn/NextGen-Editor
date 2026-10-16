// main.cpp
// Programmeinstieg des NextGen-Editors: Fenster, ImGui und die Hauptschleife.
// Die Editor-Funktionen liegen thematisch in src/app/editor/*.cpp (SHN-Editor, Quest-Editor,
// Portale, Kartenprüfung, 2D-/3D-Ansicht, Automatisierung ...). EditorShared.hpp enthält die
// gemeinsamen Typen (EditorState) und die Deklarationen aller Editor-Funktionen.

#include "editor/EditorShared.hpp"

using namespace nge;

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit fehlgeschlagen\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1600, 900, "NextGen-Editor", nullptr, nullptr);
    g_automationWindow = window;
    if (window == nullptr) {
        std::fprintf(stderr, "glfwCreateWindow fehlgeschlagen\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
#ifdef _WIN32
    g_appWindowForDialogs = window;

    // Die RC-Ressource stellt das EXE-Icon bereit; zusätzlich setzen wir sie explizit am
    // nativen GLFW-Fenster. Dadurch verwenden Titelleiste, Alt-Tab und Taskleiste zuverlässig
    // dieselbe NG-Marke, statt je nach Windows/GLFW-Version auf ein Standardicon zurückzufallen.
    if (HWND hwnd = glfwGetWin32Window(window)) {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        HICON bigIcon = static_cast<HICON>(LoadImageW(
            instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
        HICON smallIcon = static_cast<HICON>(LoadImageW(
            instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_SHARED));
        if (bigIcon) SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
        if (smallIcon) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }
#endif

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "gladLoadGLLoader fehlgeschlagen\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    static std::string imguiIniPath;
    {
        std::error_code ec;
        const auto settingsDir = NextGenUserSettingsDir();
        std::filesystem::create_directories(settingsDir, ec);
        imguiIniPath = (settingsDir / "layout.ini").string();
        io.IniFilename = imguiIniPath.c_str();
    }
    ImGui::StyleColorsDark();
    ApplyEditorTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load the approved icon raster exports once the OpenGL context is ready. If the
    // packaged assets are missing, every button keeps its existing vector fallback.
    gUiIcons.Init();

    EditorState state;
    state.renderer.Init();
    state.objectMarkerRenderer.Init();
    state.portalMarkerRenderer.Init();
    state.nifMeshRenderer.Init();
    state.shmdCategoryMeshRenderer.Init();
    state.npcMeshRenderer.Init();
    UpdatePreviewTexture(state);
    state.selectedLayer = static_cast<int>(state.textureStack.AddLayer("Base", "base.dds", 1.0f));

    AutomationScript automation = LoadAutomationScriptFromEnvironment();

    while (!glfwWindowShouldClose(window) && !automation.quitRequested) {
        glfwPollEvents();
        RunAutomationStep(state, automation);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // ImGuizmo keeps its own per-frame hover/active state. Without BeginFrame() the
        // handles can be drawn while never entering a usable interaction state.
        ImGuizmo::BeginFrame();

        // Die neue Oberfläche (siehe Mockups "NextGen-Editor") ersetzt die bisherige frei
        // andockbare Fenster-Landschaft durch einen einzigen Vollbild-Host mit fest
        // layouteten Bereichen je Bildschirm (Projekt-Hub / Projekt-Konfiguration /
        // Map-Editor-Start / Arbeitsbereich) - siehe state.screen.
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
        ImGui::Begin("##MainHost", nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        SyncProjectRoots(state);
        switch (state.screen) {
            case AppScreen::ProjectHub: DrawProjectHub(state); break;
            case AppScreen::NewProjectConfig: DrawNewProjectConfig(state); break;
            case AppScreen::MapEditorLauncher: DrawMapEditorLauncher(state); break;
            case AppScreen::MapEditorWorkspace: DrawMapEditorWorkspace(state); break;
            case AppScreen::ShnEditor: DrawShnEditor(state); break;
            case AppScreen::KfmBrowser: {
                DrawTopNav(state, L("Animationen","Animations"));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, UiTheme::Panel);
                ImGui::BeginChild("##kfmWorkspace", ImVec2(0,0), false);
                DrawPanelHeader("kfmWorkspaceHeader", "Animationen / KFM",
                                DrawIconClapper, "module.kfm",
                                "Katalog · Übergänge · Dateiverweise · Kopie-Export");
                const auto resolveKfmRead = [&](const std::filesystem::path& sourcePath) {
                    return PreferProjectOverride(
                        state.project, core::ProjectOutputSide::Client, sourcePath);
                };
                const auto resolveKfmWrite = [&](const std::filesystem::path& requestedPath) {
                    return PrepareProjectClientRequestedOutput(state.project, requestedPath);
                };
#ifdef _WIN32
                state.kfmPanel.Draw(
                    [] { return BrowseForShnFileWindows("Fiesta KFM", true); },
                    resolveKfmRead, resolveKfmWrite);
#else
                state.kfmPanel.Draw({}, resolveKfmRead, resolveKfmWrite);
#endif
                ImGui::EndChild();
                ImGui::PopStyleColor();
                break;
            }
            case AppScreen::ComingSoon: DrawComingSoon(state); break;
        }

        ImGui::End();
        ImGui::PopStyleVar();

        HandleGlobalShortcuts(state);
        DrawCommandPalette(state);
        DrawSettingsWindow(state);
        DrawMapCheckWindow(state);
        TickAutosave(state);
        FinishScatterStrokeIfReleased(state);
        {
            // Projektkarten verweisen auf Texturen/Modelle im Client-Ordner (siehe SetLegacyAssetFallbackRoots).
            static std::string lastRoots;
            const std::string roots = std::string(state.project.clientFolder) + "|" + state.resmapRootPath;
            if (roots != lastRoots) {
                lastRoots = roots;
                core::legacy::SetLegacyAssetFallbackRoots({std::filesystem::path(state.project.clientFolder),
                                                           std::filesystem::path(state.resmapRootPath)});
            }
        }
        DrawAutosaveRecoveryPopup(state);
        DrawManualWindow(state);
        SyncStatusToast(state);
        DrawToasts(state);

        ImGui::Render();
        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(UiTheme::Root.x, UiTheme::Root.y, UiTheme::Root.z, UiTheme::Root.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!automation.pendingScreenshot.empty()) {
            WriteFramebufferPpm(automation.pendingScreenshot, displayW, displayH);
            automation.pendingScreenshot.clear();
        }

        glfwSwapBuffers(window);
    }

    for (auto& [path, thumb] : state.assetThumbnails) {
        if (thumb.tex) glDeleteTextures(1, &thumb.tex);
    }
    state.assetThumbnails.clear();
    gUiIcons.Shutdown();
    state.renderer.Shutdown();
    state.objectMarkerRenderer.Shutdown();
    state.portalMarkerRenderer.Shutdown();
    state.nifMeshRenderer.Shutdown();
    state.shmdCategoryMeshRenderer.Shutdown();
    state.npcMeshRenderer.Shutdown();
    if (io.IniFilename && *io.IniFilename) ImGui::SaveIniSettingsToDisk(io.IniFilename);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
