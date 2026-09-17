// main.cpp
// Standalone-Anwendung des TheSeed Map-Editors, Phase 1+2: Heightmap- + Texturing-Modul.
//
// UI-Aufteilung bewusst zweigeteilt:
//   - "Editor (2D)": EIN Graustufenbild, dessen Inhalt und Pinselziel vom aktiven
//     Bearbeitungsmodus abhängt (Heightmap sculpten ODER Textur-Layer-Gewicht malen) -
//     einfachste robuste Interaktion, kein Ray-Mesh-Picking nötig.
//   - "3D-Vorschau": nicht-interaktives Ergebnis-Rendering (Orbit-Kamera) zur Kontrolle
//     (zeigt aktuell nur die Heightmap-Geometrie, noch ohne Textur-Layer-Darstellung).
//
// Block&Walk / Objekt-Placement folgen als eigene Module in den nächsten Ausbaustufen
// (siehe docs/MAP_FORMAT.md und CHANGELOG.md).

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
// Für den nativen "Ordner wählen"-Dialog (SHBrowseForFolder) - Teil des Windows SDK, keine
// zusätzliche Abhängigkeit. windows.h vor glad/GLFW einzubinden wäre problematisch (Makro-
// Kollisionen), daher erst hier nach den GL-Includes.
// NOMINMAX: windows.h definiert sonst min/max als Makros, die std::max(...)/std::min(...) im
// Code weiter unten kaputt machen (C2589 "ungültiges Token rechts von ::").
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>  // IFileOpenDialog - moderner Ordnerdialog statt des veralteten
                        // SHBrowseForFolder (siehe BrowseForFolderWindows weiter unten:
                        // SHBrowseForFolder zeigte auf manchen Systemen den Ordnerinhalt
                        // nicht zuverlässig an bzw. ließ sich schlecht navigieren).
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>  // glfwGetWin32Window - damit der Ordnerdialog als echtes
                                // Kind-Fenster des Hauptfensters erscheint (sonst könnte er
                                // dahinter landen und wie "nichts passiert" wirken).
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "mapeditor/core/EditOps.hpp"
#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/HeightmapIO.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"
#include "mapeditor/core/ObjectPlacementIO.hpp"
#include "mapeditor/core/TextureLayerIO.hpp"
#include "mapeditor/core/TextureLayerStack.hpp"
#include "mapeditor/core/TexturePaintOps.hpp"
#include "mapeditor/core/WalkEditOps.hpp"
#include "mapeditor/core/WalkGrid.hpp"
#include "mapeditor/core/WalkGridIO.hpp"
#include "mapeditor/core/legacy/LegacyIdmAid.hpp"
#include "mapeditor/core/legacy/LegacyMapIni.hpp"
#include "mapeditor/core/legacy/LegacyMapProject.hpp"
#include "mapeditor/core/legacy/LegacyTextureSetIO.hpp"
#include "mapeditor/core/legacy/ShnFile.hpp"
#include "mapeditor/app/Localization.hpp"

#include "Camera.hpp"
#include "ObjectMarkerRenderer.hpp"
#include "NifMeshRenderer.hpp"
#include "Renderer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <sstream>
#include <vector>

using namespace theseed::mapeditor;
using theseed::mapeditor::app::T;

namespace {

// Einheitliches UI-Farbschema: Schwarz / Grau / Blau / Weiß.
// Alle normalen ImGui-Controls verwenden diese Palette; einzelne Bereiche dürfen
// darüber gezielt nur noch die Blau-Abstufungen aus der Palette verwenden.
void ApplyEditorTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.FramePadding = ImVec2(9.0f, 6.0f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                 = ImVec4(0.92f, 0.95f, 0.98f, 1.0f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.48f, 0.53f, 0.60f, 1.0f);
    c[ImGuiCol_WindowBg]             = ImVec4(0.035f, 0.045f, 0.060f, 1.0f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.065f, 0.080f, 0.105f, 1.0f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.075f, 0.090f, 0.115f, 0.98f);
    c[ImGuiCol_Border]               = ImVec4(0.20f, 0.25f, 0.32f, 1.0f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.125f, 0.16f, 1.0f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.14f, 0.20f, 0.28f, 1.0f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.16f, 0.24f, 0.34f, 1.0f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.025f, 0.035f, 0.05f, 1.0f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.05f, 0.10f, 0.16f, 1.0f);
    c[ImGuiCol_MenuBarBg]             = ImVec4(0.04f, 0.055f, 0.075f, 1.0f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.025f, 0.03f, 0.04f, 1.0f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.25f, 0.32f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.35f, 0.46f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.30f, 0.45f, 0.60f, 1.0f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.30f, 0.58f, 0.88f, 1.0f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.40f, 0.70f, 1.0f, 1.0f);
    c[ImGuiCol_Button]               = ImVec4(0.13f, 0.25f, 0.38f, 1.0f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.18f, 0.36f, 0.55f, 1.0f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.11f, 0.28f, 0.45f, 1.0f);
    c[ImGuiCol_Header]               = ImVec4(0.12f, 0.24f, 0.36f, 1.0f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.36f, 0.55f, 1.0f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.14f, 0.30f, 0.48f, 1.0f);
    c[ImGuiCol_Separator]            = ImVec4(0.20f, 0.27f, 0.35f, 1.0f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.30f, 0.52f, 0.72f, 1.0f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.35f, 0.62f, 0.85f, 1.0f);
    c[ImGuiCol_Tab]                  = ImVec4(0.08f, 0.16f, 0.24f, 1.0f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.18f, 0.36f, 0.55f, 1.0f);
    c[ImGuiCol_TabActive]            = ImVec4(0.13f, 0.29f, 0.46f, 1.0f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.06f, 0.11f, 0.17f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.10f, 0.21f, 0.33f, 1.0f);
}

#ifdef _WIN32
// Wird direkt nach glfwCreateWindow() in main() gesetzt - siehe BrowseForFolderWindows, das
// dieses Handle als Besitzerfenster für den Ordnerdialog braucht (IFileDialog::Show).
GLFWwindow* g_appWindowForDialogs = nullptr;
#endif

// EditMode deckt jetzt auch die vier laut Mockup vorgesehenen, aber noch nicht
// implementierten Bereiche ab (Npcs/NpcAi/Mobs/MobAi) - sie erscheinen als eigene
// Reiter im neuen Arbeitsbereich-Tabbalken (siehe DrawWorkspaceTabBar), zeigen im
// Tools-/2D-/3D-Bereich aber nur einen "noch nicht implementiert"-Platzhalter an,
// bis die jeweilige Funktion gebaut wird.
enum class EditMode { Heightmap, TexturePaint, BlockWalk, ObjectPlacement, Npcs, NpcAi, Mobs, MobAi };

// Oberste Navigationsebene der neuen, an den Mockups orientierten Oberfläche (siehe
// HANDOFF.md / MAP_FORMAT.md für den Kontext). ComingSoon deckt die vier Editor-Karten ab,
// die im Mockup als "Noch nicht entschieden" markiert sind bzw. noch nicht gebaut wurden
// (Quest/Interface/Drop Table/Skill+Action) sowie den SHN Editor.
enum class AppScreen { ProjectHub, NewProjectConfig, MapEditorLauncher, MapEditorWorkspace, ShnEditor, ComingSoon };

// Projekt-Ebene (NEU): getrennt von den Client-/Server-Ordnern, siehe die Erläuterung im
// Mockup ("Neues Projekt konfigurieren") - im Projekt-Ordner werden geänderte Dateien mit
// der korrekten Ordnerstruktur für Client/Server abgelegt, NIE direkt im Client- oder
// Server-Ordner. Die tatsächliche Ordnerstruktur-Spiegelung ist noch nicht implementiert
// (siehe Kommentar bei SaveProjectConfig) - vorerst wird nur die Konfiguration selbst
// gespeichert/geladen, die Client-Ordner-Angabe wird bereits als Suchwurzel für "Map
// Öffnen" verwendet.
struct ProjectConfig {
    char name[256] = "";
    char projectFolder[512] = "";
    char clientFolder[512] = "";
    char serverFolder[512] = "";
    bool hasProject = false;
};

// Schreibt die Projekt-Konfiguration als einfache "schlüssel=wert"-Datei (bewusst kein
// JSON - im restlichen Code werden ausschließlich native/legacy Formate ohne
// JSON-Abhängigkeit verwendet, siehe docs/MAP_FORMAT.md) nach <projectFolder>/project.tsproj.
bool SaveProjectConfig(const ProjectConfig& cfg, std::string* errorOut) {
    if (cfg.projectFolder[0] == '\0') {
        if (errorOut) *errorOut = "Kein Projekt-Ordner angegeben.";
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(cfg.projectFolder, ec);
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

// Eine über den Ordner-Scan gefundene Karte (Name = Verzeichnisname, iniPath = Pfad zur .ini).
struct DiscoveredMap {
    std::string name;
    std::string iniPath;
};

struct EditorState {
    core::Heightmap heightmap{257, 257, 50.0f, 50.0f};
    core::UndoStack undo;
    core::BrushMode brushMode = core::BrushMode::Raise;
    core::BrushSettings brush;

    core::TextureLayerStack textureStack{512, 512}; // unabhängig von Heightmap, siehe docs/MAP_FORMAT.md
    core::TexturePaintUndoStack textureUndo;
    int selectedLayer = -1;
    core::PaintMode paintMode = core::PaintMode::Increase;
    core::TexturePaintSettings paintSettings;

    EditMode editMode = EditMode::Heightmap;

    core::WalkGrid walkGrid{512, 512};
    core::WalkUndoStack walkUndo;
    core::WalkStampSettings walkSettings;
    GLuint walkPreviewTex = 0;
    bool walkPreviewDirty = true;
    char walkLegacyPath[512] = "";
    int walkLegacyWidth = 512;
    int walkLegacyHeight = 512;
    char tswalkPath[512] = "walk.tswalk";

    core::ObjectPlacementSet placementSet;
    int selectedObject = -1;
    int objectPlaceMode = 1; // 1 = Platzieren, 0 = Auswählen (ImGui::RadioButton braucht int*)
    char newObjectModelPath[512] = "resmap\\field\\Rou\\GuildHall.nif";
    float newObjectScale = 1.0f;
    float newObjectRotDeg = 0.0f;
    char tsobjPath[512] = "objects.tsobj";
    char legacyShmdPath[512] = "";
    char legacyIdmPath[512] = "";
    char legacyAidPath[512] = "";
    core::ObjectSpatialIndex legacySpatialIndex;
    bool hasLegacySpatialIndex = false;
    core::legacy::ZoneMetadata legacyZoneMetadata;
    bool hasLegacyZoneMetadata = false;
    char zoneNameBuf[256] = "";

    app::OrbitCamera camera;
    app::HeightmapRenderer renderer;
    app::ObjectMarkerRenderer objectMarkerRenderer;
    app::NifMeshRenderer nifMeshRenderer;

    GLuint previewTex = 0;      // Graustufen-Vorschau Heightmap
    GLuint layerPreviewTex = 0; // Graustufen-Vorschau ausgewählter Textur-Layer
    bool meshDirty = true;
    bool layerPreviewDirty = true;
    bool wireframe = false;

    // Import-Dialog-Zustand (Legacy-Format beschreibt seine Dimensionen NICHT selbst).
    char legacyPath[512] = "";
    int legacyWidth = 257;
    int legacyHeight = 257;
    float legacyBlockWidth = 50.0f;
    float legacyBlockHeight = 50.0f;

    char tshmPath[512] = "map.tshm";
    char tstexPath[512] = "layers.tstex";

    // Legacy-Texturing-Set (ini + Blend-BMPs) - für den Export werden die beim Import
    // gelesenen Nicht-Layer-Metadaten (Pfade, BlendFileName je Layer) wiederverwendet.
    char legacyIniPath[512] = "";
    char legacyExportDir[512] = "export";
    core::legacy::LegacyMapIni legacyIniMeta;
    bool hasLegacyIniMeta = false;

    // Header-Rohbytes aus dem zuletzt importierten .HTD/.shbd - werden beim Export
    // wiederverwendet, damit Re-Export byte-exakt bleibt (statt stumpf den Default zu nehmen).
    core::LegacyHtdHeader htdHeader{};
    std::vector<std::uint8_t> htdTrailingBytes; // siehe HeightmapIO.hpp
    core::LegacyShbdHeader shbdHeader{};

    // Vereinheitlichter "Karte öffnen/speichern"-Workflow (alle vier Module auf einmal).
    char legacyMapIniPath[512] = "";
    char legacySaveDir[512] = "";
    char legacySaveStem[128] = "";
    char resmapRootPath[512] = ""; // vom Nutzer per Ordnerdialog gewählte Asset-Wurzel
    std::vector<DiscoveredMap> discoveredMaps;
    int selectedMapIndex = -1;

    char newLayerName[128] = "";
    char newLayerDiffuse[512] = "";
    float newLayerUvScale = 1.0f;

    // --- SHN Editor ---------------------------------------------------------
    enum class ShnSource { Client, Server };
    struct ShnDocument {
        core::legacy::ShnFile file;
        ShnSource source = ShnSource::Client;
        bool dirty = false;
    };
    std::vector<ShnDocument> shnFiles;
    std::string shnClientRoot;
    std::string shnServerRoot;
    int shnSelectedFile = -1;
    int shnMultiProfile = 0;
    int shnSelectedRow = -1;
    int shnSelectedColumn = -1;
    int shnSubTab = 0; // 0=Single, 1=Multi, 2=XP, 3=Buy&Sell, 4=Quest
    char shnPath[1024] = "";
    char shnSearch[256] = "";
    bool shnSearchColumns = true;
    bool shnSearchValues = true;
    bool shnFilterActive = false;
    bool shnEditPopupOpen = false;
    std::string shnEditBuffer;
    std::string shnStatus;

    std::string statusMessage;

    // --- Neue Navigationsebene (Projekt-Hub / Projekt-Konfiguration / Map-Editor-Start) ---
    AppScreen screen = AppScreen::ProjectHub;
    ProjectConfig project;
    std::string comingSoonTitle; // Titel der Karte, über die der Platzhalter-Bildschirm erreicht wurde

    // "Create New Map"-Formular (siehe DrawMapEditorLauncher) - ersetzt das bisherige feste
    // "Neu (257x257)" durch nutzerdefinierte Maße, wie im Mockup vorgesehen.
    char newMapName[128] = "";
    int newMapWidth = 257;
    int newMapHeight = 257;
    char newMapTextureLayer[128] = "Base";

    // Steuert, welches der beiden Panels im Map-Editor-Start sichtbar ist (siehe die
    // "New Map"/"Map Öffnen"-Knöpfe in DrawMapEditorLauncher).
    enum class MapLauncherView { NewMap, Browse } mapLauncherView = MapLauncherView::Browse;
    std::string lastScannedMapRoot; // löst erneutes Scannen aus, wenn sich der Client-Ordner ändert
    int lastResmapCandidateCount = 0; // wie viele gleichnamige "resmap"-Ordner insgesamt gefunden wurden
    std::string lastResmapResolvedPath; // Anzeige-Text für "Suche in: ..." (gecacht, kein Scan pro Frame)
    bool lastResmapFound = false;

    // Asset-Picker (Textur-Auswahl für Layer, Modell-Auswahl für Objekt-Platzierung) - siehe
    // DrawAssetPickerPopup. Listen werden einmal beim Öffnen des Pickers gescannt und gecacht
    // (nicht pro Frame - bei tausenden .nif-Dateien spürbar teuer).
    std::vector<std::string> availableTextureFiles; // relativ zu "fieldTexture", z.B. "wall/stone01.dds"
    std::vector<std::string> availableNifFiles;      // relativ zu "nif"/"nifs", z.B. "field/Rou/GuildHall.nif"
    bool textureListScanned = false;
    bool nifListScanned = false;
    std::string assetPickerFilter;
};

// Listet Dateien mit einer der angegebenen Endungen unter root (rekursiv, begrenzte Tiefe),
// relative Pfade zu root. Für Asset-Picker (Textur-/Modell-Auswahl) - siehe DrawAssetPickerPopup.
std::vector<std::string> ListFilesByExtension(const std::filesystem::path& root,
                                               const std::vector<std::string>& extensionsLower,
                                               int maxDepth = 6) {
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
            if (it.depth() >= maxDepth) it.disable_recursion_pending();
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

// HINWEIS: Es gibt bewusst KEIN SyncTextureStackSize mehr (anders als SyncWalkGridSize unten) -
// die Textur-Layer-Auflösung ist NICHT an die Heightmap gekoppelt (echte Referenzkarten zeigen
// z.B. 512x512-Blend-Bitmaps bei 257x257-Heightmap), siehe docs/MAP_FORMAT.md. Der
// TextureLayerStack behält seine eigene Auflösung unabhängig von Heightmap-Änderungen bei.

// Baut aus dem aktuellen Editor-Zustand ein LegacyMapProject für SaveLegacyMap zusammen -
// Kehrseite dessen, was ApplyProjectToState (siehe unten) beim Öffnen verteilt.
core::legacy::LegacyMapProject BuildProjectFromState(const EditorState& state) {
    core::legacy::LegacyMapProject project;
    project.ini = state.legacyIniMeta;
    project.heightmap = state.heightmap;
    project.hasHeightmap = true;
    project.htdHeader = state.htdHeader;
    project.htdTrailingBytes = state.htdTrailingBytes;
    project.textureStack = state.textureStack;
    project.walkGrid = state.walkGrid;
    project.hasWalkGrid = true;
    project.shbdHeader = state.shbdHeader;
    project.objects = state.placementSet;
    project.hasObjects = true;
    project.spatialIndex = state.legacySpatialIndex;
    project.hasSpatialIndex = state.hasLegacySpatialIndex;
    project.zone = state.legacyZoneMetadata;
    project.hasZone = state.hasLegacyZoneMetadata;
    return project;
}

// Verteilt ein frisch geöffnetes LegacyMapProject auf die einzelnen Editor-Zustandsfelder -
// setzt außerdem alle Undo-Stacks/Auswahl/Dirty-Flags zurück (neue Karte, alte Historie ungültig).
void ApplyProjectToState(EditorState& state, core::legacy::LegacyMapProject&& project, const std::filesystem::path& mapDir) {
    state.heightmap = std::move(project.heightmap);
    state.htdHeader = project.htdHeader;
    state.htdTrailingBytes = project.htdTrailingBytes;
    state.undo.Clear();
    state.meshDirty = true;

    // Kamera auf die neue Karte zentrieren + Entfernung an ihre Größe anpassen - sonst startet
    // man bei sehr großen oder sehr kleinen Karten leicht außerhalb des sichtbaren Bereichs.
    {
        const float spanX = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth();
        const float spanZ = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight();
        const auto [lo, hi] = state.heightmap.MinMax();
        state.camera.SetTarget(spanX * 0.5f, (lo + hi) * 0.5f, spanZ * 0.5f);
        state.camera.Zoom(std::max(spanX, spanZ) * 0.9f - state.camera.Distance());
    }

    state.textureStack = std::move(project.textureStack);
    state.textureUndo.Clear();
    state.selectedLayer = state.textureStack.LayerCount() > 0 ? 0 : -1;
    state.layerPreviewDirty = true;
    state.renderer.LoadTerrainTextures(state.textureStack, mapDir);

    state.walkGrid = std::move(project.walkGrid);
    state.shbdHeader = project.shbdHeader;
    state.walkUndo.Clear();
    state.walkPreviewDirty = true;

    state.placementSet = std::move(project.objects);
    state.selectedObject = -1;
    // Versucht, für alle Objekte echte .nif-Meshes zu laden (aktuell nur untexturierte Meshes
    // erfolgreich, siehe docs/MAP_FORMAT.md) - für den Rest bleibt der Platzhalter-Marker.
    state.nifMeshRenderer.LoadModelsForSet(state.placementSet, mapDir);

    state.legacySpatialIndex = std::move(project.spatialIndex);
    state.hasLegacySpatialIndex = project.hasSpatialIndex;
    state.legacyZoneMetadata = std::move(project.zone);
    state.hasLegacyZoneMetadata = project.hasZone;
    std::snprintf(state.zoneNameBuf, sizeof(state.zoneNameBuf), "%s", state.legacyZoneMetadata.name.c_str());

    state.legacyIniMeta = std::move(project.ini);
    state.hasLegacyIniMeta = true;
}

// Leitet die Block&Walk-Gitterauflösung aus der Heightmap ab: Breite = QuadsBreite/2, Höhe =
// QuadsBreite*8 - KEIN Quadrat. Korrigiert anhand von vier echten Referenzkarten: die vorherige
// Annahme ("quadratisch, max(QuadsBreite,QuadsHöhe)*2") bestätigte nur die GESAMT-Byte-Anzahl,
// nicht die tatsächliche Breite/Höhe-Aufteilung - dadurch wurde z.B. bei RouVal01 (echte Daten:
// 256x4096) fälschlich als 1024x1024 interpretiert, was beim Anzeigen sichtbar als "4x
// dieselbe Silhouette nebeneinander" auffiel (vom Nutzer per Screenshot entdeckt). Die
// richtige Breite ergibt bei allen vier Karten ein einzelnes, nicht wiederholtes Bild; das
// zweite Header-Feld der echten .shbd-Dateien enthält die Höhe sogar direkt (siehe
// docs/MAP_FORMAT.md). Achtung: nutzt nur QuadsBreite (X-Achse), QuadsHöhe fließt nicht ein -
// bei nicht-quadratischen Karten (z.B. Adl) durch die echten Header-Werte bestätigt, aber die
// genaue X/Z-Achsenzuordnung bleibt ungeklärt (siehe Dokumentation).
void SyncWalkGridSize(EditorState& state) {
    const std::uint32_t quadsW = state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 0;
    const std::uint32_t gridWidth = quadsW / 2;
    const std::uint32_t gridHeight = quadsW * 8;
    state.walkGrid.Resize(gridWidth, gridHeight, -1);
    state.walkUndo.Clear();
    state.walkPreviewDirty = true;
}

// Erzeugt/aktualisiert die Graustufen-Vorschautextur aus den aktuellen Höhendaten.
void UpdatePreviewTexture(EditorState& state) {
    const auto& hm = state.heightmap;
    const auto [lo, hi] = hm.MinMax();
    const float range = (hi - lo) > 0.0001f ? (hi - lo) : 1.0f;

    std::vector<unsigned char> pixels(static_cast<std::size_t>(hm.Width()) * hm.Height());
    const auto data = hm.Data();
    for (std::size_t i = 0; i < data.size(); ++i) {
        const float t = std::clamp((data[i] - lo) / range, 0.0f, 1.0f);
        pixels[i] = static_cast<unsigned char>(t * 255.0f);
    }

    if (state.previewTex == 0) {
        glGenTextures(1, &state.previewTex);
    }
    glBindTexture(GL_TEXTURE_2D, state.previewTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Ohne Swizzle liest OpenGL bei GL_R8-Einkanaltexturen G/B als 0 -> erscheint rot statt grau.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(hm.Width()), static_cast<int>(hm.Height()),
                 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Erzeugt/aktualisiert die Graustufen-Vorschautextur für den aktuell ausgewählten Textur-Layer.
void UpdateLayerPreviewTexture(EditorState& state) {
    if (state.selectedLayer < 0 || static_cast<std::size_t>(state.selectedLayer) >= state.textureStack.LayerCount()) {
        return;
    }
    const auto& blend = state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer)).blend;
    if (blend.Width() == 0 || blend.Height() == 0) return;

    std::vector<unsigned char> pixels(static_cast<std::size_t>(blend.Width()) * blend.Height());
    const auto data = blend.Data();
    for (std::size_t i = 0; i < data.size(); ++i) {
        pixels[i] = static_cast<unsigned char>(std::clamp(data[i], 0.0f, 1.0f) * 255.0f);
    }

    if (state.layerPreviewTex == 0) {
        glGenTextures(1, &state.layerPreviewTex);
    }
    glBindTexture(GL_TEXTURE_2D, state.layerPreviewTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Ohne Swizzle liest OpenGL bei GL_R8-Einkanaltexturen G/B als 0 -> erscheint rot statt grau.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(blend.Width()), static_cast<int>(blend.Height()),
                 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Erzeugt/aktualisiert eine grobe Graustufen-Vorschau des Block&Walk-Gitters. Da die exakte
// Bit-Semantik der Rohwerte nicht gesichert ist (siehe WalkGrid.hpp), wird nur das High-Byte
// des vorzeichenlosen 16-bit-Werts abgebildet - reicht als visuelle Orientierung, ist aber
// keine "korrekte" Begehbarkeits-Darstellung.
void UpdateWalkPreviewTexture(EditorState& state) {
    const auto& grid = state.walkGrid;
    if (grid.Width() == 0 || grid.Height() == 0) return;

    std::vector<unsigned char> pixels(static_cast<std::size_t>(grid.Width()) * grid.Height());
    const auto data = grid.Data();
    for (std::size_t i = 0; i < data.size(); ++i) {
        const auto u = static_cast<std::uint16_t>(data[i]);
        pixels[i] = static_cast<unsigned char>(u >> 8);
    }

    if (state.walkPreviewTex == 0) {
        glGenTextures(1, &state.walkPreviewTex);
    }
    glBindTexture(GL_TEXTURE_2D, state.walkPreviewTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Ohne Swizzle liest OpenGL bei GL_R8-Einkanaltexturen G/B als 0 -> erscheint rot statt grau.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(grid.Width()), static_cast<int>(grid.Height()),
                 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

#ifdef _WIN32
// Nativer "Ordner wählen"-Dialog. Ältere, aber sehr stabile Win32-API (seit Windows 95
// verfügbar) - bewusst statt der moderneren IFileOpenDialog-COM-API gewählt, da die
// Ausdrucksweise hier deutlich weniger fehleranfällig ist (kein manuelles COM-Interface-
// Lifetime-Management nötig).
// Ordnerdialog über die moderne IFileOpenDialog-COM-API (seit Windows Vista) statt des
// alten SHBrowseForFolder - letzteres zeigte den Ordnerinhalt auf manchen Systemen nicht
// zuverlässig an und ließ sich schlecht navigieren (siehe Rückmeldung nach dem ersten
// echten Testlauf). IFileOpenDialog verhält sich wie ein normales Explorer-Fenster
// (Inhalt sichtbar, Adressleiste, Favoriten, Größe änderbar).
std::optional<std::string> BrowseForFolderWindows(const char* title) {
    // COM ggf. selbst initialisieren, aber nur wieder freigeben, wenn WIR es initialisiert
    // haben (S_OK) - war es schon initialisiert (S_FALSE) oder mit anderem Threading-Modell
    // aktiv (RPC_E_CHANGED_MODE, z.B. durch eine andere Bibliothek), NICHT per CoUninitialize
    // eingreifen, das würde fremden Zustand kaputt machen.
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool weOwnCom = comInit == S_OK;

    std::optional<std::string> result;
    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr) && dialog != nullptr) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

        wchar_t wtitle[256]{};
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, static_cast<int>(std::size(wtitle)));
        dialog->SetTitle(wtitle);

        const HWND owner = g_appWindowForDialogs != nullptr ? glfwGetWin32Window(g_appWindowForDialogs) : nullptr;
        hr = dialog->Show(owner);
        if (SUCCEEDED(hr)) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
                PWSTR pathW = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pathW)) && pathW != nullptr) {
                    char pathA[1024]{};
                    WideCharToMultiByte(CP_UTF8, 0, pathW, -1, pathA, static_cast<int>(std::size(pathA)), nullptr, nullptr);
                    result = std::string(pathA);
                    CoTaskMemFree(pathW);
                }
                item->Release();
            }
        }
        // hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) bei Abbruch durch den Nutzer - kein Fehler,
        // result bleibt einfach std::nullopt.
        dialog->Release();
    }

    if (weOwnCom) {
        CoUninitialize();
    }
    return result;
}
#endif

#ifdef _WIN32
std::optional<std::string> BrowseForShnFileWindows(const char* title) {
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool weOwnCom = comInit == S_OK;
    std::optional<std::string> result;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog))) && dialog) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
        COMDLG_FILTERSPEC filters[] = {{L"Fiesta SHN", L"*.shn"}, {L"Alle Dateien", L"*.*"}};
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        wchar_t wtitle[256]{};
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, static_cast<int>(std::size(wtitle)));
        dialog->SetTitle(wtitle);
        const HWND owner = g_appWindowForDialogs ? glfwGetWin32Window(g_appWindowForDialogs) : nullptr;
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR pathW = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pathW)) && pathW) {
                    char pathA[4096]{};
                    WideCharToMultiByte(CP_UTF8, 0, pathW, -1, pathA, static_cast<int>(std::size(pathA)), nullptr, nullptr);
                    result = std::string(pathA);
                    CoTaskMemFree(pathW);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (weOwnCom) CoUninitialize();
    return result;
}
#endif

// Sammelt ALLE Ordner namens "resmap" (Groß-/Kleinschreibung egal) unter root, bis zu einer
// begrenzten Tiefe - bewusst NICHT nur den ersten Treffer (siehe Rückmeldung: es gibt
// offenbar mehr als einen so benannten Ordner, z.B. einen leeren/falschen unter einem
// anderen Unterordner - der erste Treffer war nicht der richtige). Welcher Kandidat
// tatsächlich Kartendaten enthält, entscheidet ResolveMapSearchRootAndScan weiter unten
// (scannt jeden Kandidaten und nimmt den mit den meisten gefundenen Karten). Steigt bewusst
// NICHT in "ressystem" oder "fieldTexture"/"fieldtexture" ab - das sind andere,
// potenziell große Ressourcen-Ordner ohne Kartendaten.
void FindAllResmapCandidates(const std::filesystem::path& root, int depthRemaining,
                              std::vector<std::filesystem::path>& outCandidates) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return;
    }
    std::vector<std::filesystem::path> subdirs;
    for (const auto& entry : std::filesystem::directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec || !entry.is_directory(ec)) continue;
        std::string name = entry.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == "resmap") {
            outCandidates.push_back(entry.path());
            continue; // in ein gefundenes resmap selbst nicht nochmal hinabsteigen
        }
        if (name != "ressystem" && name != "fieldtexture") subdirs.push_back(entry.path());
    }
    if (depthRemaining <= 0) return;
    for (const auto& sub : subdirs) {
        FindAllResmapCandidates(sub, depthRemaining - 1, outCandidates);
    }
}

// Sucht rekursiv (begrenzte Tiefe) unter root nach Karten-.ini-Dateien - das sind die
// Karten-Einstiegspunkte für OpenLegacyMap. WICHTIG: akzeptiert nur .ini-Dateien, deren
// Dateiname (ohne Endung) mit dem Namen ihres eigenen Ordners übereinstimmt
// (<MAPORDNER>/<MAPORDNER>.ini - die übliche Konvention). Das schließt gezielt andere,
// nicht zu einer Karte gehörende .ini-Dateien aus, die sonst z.B. direkt unter einem
// Kategorie-Ordner (UNTERORDNER) liegen könnten und fälschlich als "Karte" auftauchten.
// Steigt bewusst NICHT in "fieldTexture"/"fieldtexture" ab (dort liegen nur geteilte
// Texturen, keine Karten-inis, aber potenziell sehr viele Dateien).
std::vector<DiscoveredMap> ScanForMaps(const std::filesystem::path& root) {
    std::vector<DiscoveredMap> found;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return found;
    }

    constexpr int kMaxDepth = 6;
    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; it != end && !ec; it.increment(ec)) {
        const auto& entry = *it;
        if (entry.is_directory(ec)) {
            const std::string dirName = entry.path().filename().string();
            const bool isTextureDir = dirName.size() == 12 &&
                (dirName == "fieldTexture" || dirName == "fieldtexture" || dirName == "FieldTexture");
            if (isTextureDir || it.depth() >= kMaxDepth) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!entry.is_regular_file(ec)) continue;

        const auto& path = entry.path();
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".ini") continue;

        std::string stem = path.stem().string();
        std::string folderName = path.parent_path().filename().string();
        std::string stemLower = stem, folderLower = folderName;
        std::transform(stemLower.begin(), stemLower.end(), stemLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(folderLower.begin(), folderLower.end(), folderLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (stemLower != folderLower) continue; // z.B. eine Kategorie-weite .ini, keine Karte

        found.push_back({folderName, path.string()});
    }

    std::sort(found.begin(), found.end(), [](const DiscoveredMap& a, const DiscoveredMap& b) { return a.name < b.name; });
    return found;
}

// Ergebnis der Kartensuche: welcher "resmap"-Kandidat letztlich verwendet wurde (falls
// überhaupt einer gefunden wurde), die darin gefundenen Karten, und wie viele Kandidaten
// insgesamt in Frage kamen (zur Anzeige, falls mehrere "resmap"-Ordner existieren).
struct ResmapResolution {
    std::optional<std::filesystem::path> root;
    std::vector<DiscoveredMap> maps;
    int candidateCount = 0;
};

// "Client Ordner" ist laut Rückmeldung der Client-WURZELordner (enthält "resmap" - direkt
// oder ein paar Ebenen tiefer -, nicht direkt die Karten). Sammelt ALLE Ordner namens
// "resmap" und scannt JEDEN davon - verwendet den mit den MEISTEN gefundenen Karten (nicht
// einfach den ersten Treffer, siehe Rückmeldung: es gab einen leeren/falschen "resmap" an
// anderer Stelle, der vorher fälschlich zuerst gefunden wurde). Liefert `root = nullopt`,
// wenn gar kein "resmap"-Ordner existiert - fällt NIE auf eine Suche im gesamten
// Client-Ordner zurück.
ResmapResolution ResolveMapSearchRootAndScan(const std::filesystem::path& clientFolder) {
    ResmapResolution result;

    std::string selfName = clientFolder.filename().string();
    std::transform(selfName.begin(), selfName.end(), selfName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::vector<std::filesystem::path> candidates;
    if (selfName == "resmap") {
        candidates.push_back(clientFolder); // Nutzer hat resmap selbst als Client Ordner gewählt
    } else {
        FindAllResmapCandidates(clientFolder, 3, candidates);
    }
    result.candidateCount = static_cast<int>(candidates.size());
    if (candidates.empty()) {
        return result;
    }

    std::size_t bestIdx = 0;
    std::vector<DiscoveredMap> bestMaps = ScanForMaps(candidates[0]);
    for (std::size_t i = 1; i < candidates.size(); ++i) {
        auto maps = ScanForMaps(candidates[i]);
        if (maps.size() > bestMaps.size()) {
            bestIdx = i;
            bestMaps = std::move(maps);
        }
    }
    result.root = candidates[bestIdx];
    result.maps = std::move(bestMaps);
    return result;
}

// Enthält die komplette, bereits bestehende "Karte öffnen/speichern"- und
// Einzelmodul-Import/Export-Logik (früher Inhalt des "Datei"-Menüs) - jetzt als
// eigenständiger Inhalt statt Menü-Dropdown, damit er sich in die neue "Datei"-Spalte
// des Arbeitsbereichs (siehe DrawMapEditorWorkspace) einbetten lässt. Funktional
// unverändert gegenüber der bisherigen Menüleiste.
void DrawAdvancedFileOps(EditorState& state) {
    ImGui::TextColored(ImVec4(0.90f, 0.94f, 1.0f, 1.0f), "Karte öffnen/speichern (komplett, alle Module)");

#ifdef _WIN32
        if (ImGui::Button("Asset-Ordner wählen...")) {
            if (auto picked = BrowseForFolderWindows("Client-Ordner mit den Karten w\u00e4hlen (enth\u00e4lt z.B. 'field\\')")) {
                std::snprintf(state.resmapRootPath, sizeof(state.resmapRootPath), "%s", picked->c_str());
                state.discoveredMaps = ScanForMaps(state.resmapRootPath);
                state.selectedMapIndex = -1;
                state.statusMessage = std::to_string(state.discoveredMaps.size()) + " Karte(n) gefunden in: " + std::string(state.resmapRootPath);
            }
        }
        ImGui::SameLine();
#endif
        ImGui::TextDisabled("%s", state.resmapRootPath[0] != '\0' ? state.resmapRootPath : "(kein Ordner gew\u00e4hlt)");

        if (!state.discoveredMaps.empty()) {
            ImGui::Text("Gefundene Karten (%zu):", state.discoveredMaps.size());
            ImGui::BeginChild("DiscoveredMaps", ImVec2(0, 100), true);
            for (int i = 0; i < static_cast<int>(state.discoveredMaps.size()); ++i) {
                const bool selected = state.selectedMapIndex == i;
                if (ImGui::Selectable(state.discoveredMaps[static_cast<std::size_t>(i)].name.c_str(), selected)) {
                    state.selectedMapIndex = i;
                    std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s",
                                  state.discoveredMaps[static_cast<std::size_t>(i)].iniPath.c_str());
                }
            }
            ImGui::EndChild();
        }

        ImGui::InputText("Karte-.ini##project", state.legacyMapIniPath, sizeof(state.legacyMapIniPath));
        if (ImGui::Button("Karte öffnen")) {
            core::legacy::LegacyMapOpenReport report;
            auto result = core::legacy::OpenLegacyMap(state.legacyMapIniPath, &report);
            if (result) {
                const std::filesystem::path iniPath(state.legacyMapIniPath);
                ApplyProjectToState(state, std::move(*result), iniPath.parent_path());
                std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", iniPath.parent_path().string().c_str());
                std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", iniPath.stem().string().c_str());
                state.statusMessage = "Karte ge\u00f6ffnet (" + std::to_string(report.issues.size()) + " Hinweis(e)) - " +
                                       std::to_string(state.placementSet.Count()) + " Objekte, " +
                                       std::to_string(state.textureStack.LayerCount()) + " Textur-Layer.";
                for (const auto& issue : report.issues) {
                    state.statusMessage += "\n- " + issue;
                }
            } else {
                state.statusMessage = "Karte \u00f6ffnen fehlgeschlagen: " + result.error();
            }
        }
        ImGui::InputText("Ausgabeverzeichnis##project", state.legacySaveDir, sizeof(state.legacySaveDir));
#ifdef _WIN32
        ImGui::SameLine();
        if (ImGui::Button("Wählen...##saveDir")) {
            if (auto picked = BrowseForFolderWindows("Ausgabeverzeichnis w\u00e4hlen")) {
                std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", picked->c_str());
            }
        }
#endif
        ImGui::InputText("Kartenname##project", state.legacySaveStem, sizeof(state.legacySaveStem));
        ImGui::BeginDisabled(!state.hasLegacyIniMeta);
        if (ImGui::Button("Karte speichern")) {
            auto project = BuildProjectFromState(state);
            auto result = core::legacy::SaveLegacyMap(project, state.legacySaveDir, state.legacySaveStem);
            if (result) {
                state.legacyIniMeta = project.ini; // ExportLegacyTextureSet aktualisiert z.B. Layer-Metadaten
                state.statusMessage = "Karte gespeichert nach: " + std::string(state.legacySaveDir);
            } else {
                state.statusMessage = "Karte speichern fehlgeschlagen: " + result.error();
            }
        }
        ImGui::EndDisabled();
        if (!state.hasLegacyIniMeta) {
            ImGui::TextDisabled("(Erst eine Karte \u00f6ffnen, bevor gespeichert werden kann.)");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Einzelne Module (fortgeschritten)");
        if (ImGui::MenuItem("Neu (257x257)")) {
            state.heightmap = core::Heightmap(257, 257, 50.0f, 50.0f);
            state.undo.Clear();
            state.meshDirty = true;
            SyncWalkGridSize(state);
            state.selectedLayer = -1;
            state.statusMessage = "Neue leere Heightmap erstellt.";
        }
        ImGui::Separator();

        ImGui::TextDisabled("Heightmap");
        ImGui::InputText("##tshmPath", state.tshmPath, sizeof(state.tshmPath));
        ImGui::SameLine();
        if (ImGui::Button("Laden (.tshm)")) {
            auto result = core::LoadTshm(state.tshmPath);
            if (result) {
                state.heightmap = std::move(*result);
                state.undo.Clear();
                state.meshDirty = true;
                SyncWalkGridSize(state);
                state.statusMessage = "Geladen: " + std::string(state.tshmPath);
            } else {
                state.statusMessage = "Fehler beim Laden: " + result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Speichern (.tshm)")) {
            auto result = core::SaveTshm(state.heightmap, state.tshmPath);
            state.statusMessage = result ? "Gespeichert: " + std::string(state.tshmPath)
                                          : "Fehler beim Speichern: " + result.error();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Heightmap-Import/Export (.HTD / .HTDG)");
        ImGui::InputText("Pfad##legacy", state.legacyPath, sizeof(state.legacyPath));
        ImGui::InputInt("Breite (aus .ini)", &state.legacyWidth);
        ImGui::InputInt("Höhe (aus .ini)", &state.legacyHeight);
        ImGui::InputFloat("Blockbreite", &state.legacyBlockWidth);
        ImGui::InputFloat("Blockhöhe", &state.legacyBlockHeight);
        if (ImGui::Button("Importieren##htd")) {
            if (state.legacyWidth > 0 && state.legacyHeight > 0) {
                auto result = core::ImportLegacyHtd(
                    state.legacyPath,
                    static_cast<std::uint32_t>(state.legacyWidth),
                    static_cast<std::uint32_t>(state.legacyHeight),
                    state.legacyBlockWidth, state.legacyBlockHeight, &state.htdHeader, &state.htdTrailingBytes);
                if (result) {
                    state.heightmap = std::move(*result);
                    state.undo.Clear();
                    state.meshDirty = true;
                    SyncWalkGridSize(state);
                    state.statusMessage = "Legacy-Datei importiert: " + std::string(state.legacyPath);
                } else {
                    state.statusMessage = "Import fehlgeschlagen: " + result.error();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Exportieren##htd")) {
            auto result = core::ExportLegacyHtd(state.heightmap, state.legacyPath, state.htdHeader, state.htdTrailingBytes);
            state.statusMessage = result ? "Legacy-HTD exportiert nach: " + std::string(state.legacyPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Texturing (natives Format)");
        ImGui::InputText("##tstexPath", state.tstexPath, sizeof(state.tstexPath));
        ImGui::SameLine();
        if (ImGui::Button("Laden (.tstex)")) {
            auto result = core::LoadTsTex(state.tstexPath);
            if (result) {
                state.textureStack = std::move(*result);
                state.textureUndo.Clear();
                state.selectedLayer = state.textureStack.LayerCount() > 0 ? 0 : -1;
                state.layerPreviewDirty = true;
                state.statusMessage = "Textur-Layer geladen: " + std::string(state.tstexPath);
            } else {
                state.statusMessage = "Fehler beim Laden: " + result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Speichern (.tstex)")) {
            auto result = core::SaveTsTex(state.textureStack, state.tstexPath);
            state.statusMessage = result ? "Gespeichert: " + std::string(state.tstexPath)
                                          : "Fehler beim Speichern: " + result.error();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Texturing-Set (ini + Blend-BMPs)");
        ImGui::InputText("ini-Pfad##legacyTex", state.legacyIniPath, sizeof(state.legacyIniPath));
        if (ImGui::Button("Legacy-Set importieren")) {
            core::legacy::TextureSetImportReport report;
            auto result = core::legacy::ImportLegacyTextureSet(state.legacyIniPath, &report);
            auto metaResult = core::legacy::ParseLegacyMapIni(state.legacyIniPath);
            if (result && metaResult) {
                state.textureStack = std::move(*result);
                state.textureUndo.Clear();
                state.selectedLayer = state.textureStack.LayerCount() > 0 ? 0 : -1;
                state.layerPreviewDirty = true;
                state.legacyIniMeta = std::move(*metaResult);
                state.hasLegacyIniMeta = true;
                state.statusMessage = "Legacy-Set importiert (" + std::to_string(state.textureStack.LayerCount()) +
                                       " Layer, " + std::to_string(report.missingBlendFiles.size()) + " Blend-BMP(s) fehlend/nicht ladbar).";
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + (result ? metaResult.error() : result.error());
            }
        }
        ImGui::InputText("Export-Verzeichnis##legacyTex", state.legacyExportDir, sizeof(state.legacyExportDir));
        ImGui::BeginDisabled(!state.hasLegacyIniMeta);
        if (ImGui::Button("Legacy-Set exportieren")) {
            auto result = core::legacy::ExportLegacyTextureSet(state.textureStack, state.legacyIniMeta, state.legacyExportDir, "Rou.ini");
            state.statusMessage = result ? "Legacy-Set exportiert nach: " + std::string(state.legacyExportDir)
                                          : "Export fehlgeschlagen: " + result.error();
        }
        ImGui::EndDisabled();
        if (!state.hasLegacyIniMeta) {
            ImGui::TextDisabled("(Erst \u00fcber 'Legacy-Set importieren' Metadaten laden.)");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Block&Walk (natives Format)");
        ImGui::InputText("##tswalkPath", state.tswalkPath, sizeof(state.tswalkPath));
        ImGui::SameLine();
        if (ImGui::Button("Laden (.tswalk)")) {
            auto result = core::LoadTsWalk(state.tswalkPath);
            if (result) {
                state.walkGrid = std::move(*result);
                state.walkUndo.Clear();
                state.walkPreviewDirty = true;
                state.statusMessage = "Block&Walk-Gitter geladen: " + std::string(state.tswalkPath);
            } else {
                state.statusMessage = "Fehler beim Laden: " + result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Speichern (.tswalk)")) {
            auto result = core::SaveTsWalk(state.walkGrid, state.tswalkPath);
            state.statusMessage = result ? "Gespeichert: " + std::string(state.tswalkPath)
                                          : "Fehler beim Speichern: " + result.error();
        }

        ImGui::TextDisabled("Legacy-Import/Export (.shbd)");
        ImGui::InputText("Pfad##walkLegacy", state.walkLegacyPath, sizeof(state.walkLegacyPath));
        ImGui::InputInt("Breite##walkLegacy", &state.walkLegacyWidth);
        ImGui::InputInt("Höhe##walkLegacy", &state.walkLegacyHeight);
        if (ImGui::Button("Importieren##shbd")) {
            if (state.walkLegacyWidth > 0 && state.walkLegacyHeight > 0) {
                auto result = core::ImportLegacyShbd(
                    state.walkLegacyPath,
                    static_cast<std::uint32_t>(state.walkLegacyWidth),
                    static_cast<std::uint32_t>(state.walkLegacyHeight), &state.shbdHeader);
                if (result) {
                    state.walkGrid = std::move(*result);
                    state.walkUndo.Clear();
                    state.walkPreviewDirty = true;
                    state.statusMessage = "Legacy-shbd importiert: " + std::string(state.walkLegacyPath);
                } else {
                    state.statusMessage = "Import fehlgeschlagen: " + result.error();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Exportieren##shbd")) {
            auto result = core::ExportLegacyShbd(state.walkGrid, state.walkLegacyPath, state.shbdHeader);
            state.statusMessage = result ? "Legacy-shbd exportiert nach: " + std::string(state.walkLegacyPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Objekt-Placement (natives Format)");
        ImGui::InputText("##tsobjPath", state.tsobjPath, sizeof(state.tsobjPath));
        ImGui::SameLine();
        if (ImGui::Button("Laden (.tsobj)")) {
            auto result = core::LoadTsObj(state.tsobjPath);
            if (result) {
                state.placementSet = std::move(*result);
                state.selectedObject = -1;
                state.nifMeshRenderer.LoadModelsForSet(state.placementSet, std::filesystem::path(state.tsobjPath).parent_path());
                state.statusMessage = "Objekte geladen: " + std::string(state.tsobjPath);
            } else {
                state.statusMessage = "Fehler beim Laden: " + result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Speichern (.tsobj)")) {
            auto result = core::SaveTsObj(state.placementSet, state.tsobjPath);
            state.statusMessage = result ? "Gespeichert: " + std::string(state.tsobjPath)
                                          : "Fehler beim Speichern: " + result.error();
        }

        ImGui::TextDisabled("Legacy-Objekt-Placement (.shmd)");
        ImGui::InputText("Pfad##shmd", state.legacyShmdPath, sizeof(state.legacyShmdPath));
        if (ImGui::Button("Importieren##shmd")) {
            auto result = core::legacy::ParseLegacyShmd(state.legacyShmdPath);
            if (result) {
                state.placementSet = std::move(*result);
                state.selectedObject = -1;
                state.nifMeshRenderer.LoadModelsForSet(state.placementSet, std::filesystem::path(state.legacyShmdPath).parent_path());
                state.statusMessage = "Legacy-shmd importiert (" + std::to_string(state.placementSet.Count()) + " Objekte): " + std::string(state.legacyShmdPath);
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Exportieren##shmd")) {
            auto result = core::legacy::SerializeLegacyShmd(state.placementSet, state.legacyShmdPath);
            state.statusMessage = result ? "Legacy-shmd exportiert nach: " + std::string(state.legacyShmdPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }

        ImGui::TextDisabled("Legacy räumlicher Index (.idm) - reiner Pass-Through, Semantik ungeklärt");
        ImGui::InputText("Pfad##idm", state.legacyIdmPath, sizeof(state.legacyIdmPath));
        if (ImGui::Button("Importieren##idm")) {
            auto result = core::legacy::ParseLegacyIdm(state.legacyIdmPath);
            if (result) {
                state.legacySpatialIndex = std::move(*result);
                state.hasLegacySpatialIndex = true;
                state.statusMessage = "Legacy-idm importiert (" + std::to_string(state.legacySpatialIndex.groups.size()) + " Gruppen, roh gehalten): " + std::string(state.legacyIdmPath);
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + result.error();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.hasLegacySpatialIndex);
        if (ImGui::Button("Exportieren##idm")) {
            auto result = core::legacy::SerializeLegacyIdm(state.legacySpatialIndex, state.legacyIdmPath);
            state.statusMessage = result ? "Legacy-idm exportiert nach: " + std::string(state.legacyIdmPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }
        ImGui::EndDisabled();

        ImGui::TextDisabled("Legacy-Zonen-Metadaten (.aid)");
        ImGui::InputText("Pfad##aid", state.legacyAidPath, sizeof(state.legacyAidPath));
        if (ImGui::Button("Importieren##aid")) {
            auto result = core::legacy::ParseLegacyAid(state.legacyAidPath);
            if (result) {
                state.legacyZoneMetadata = *result;
                state.hasLegacyZoneMetadata = true;
                std::snprintf(state.zoneNameBuf, sizeof(state.zoneNameBuf), "%s", state.legacyZoneMetadata.name.c_str());
                state.statusMessage = "Legacy-aid importiert (Zone '" + state.legacyZoneMetadata.name + "'): " + std::string(state.legacyAidPath);
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + result.error();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.hasLegacyZoneMetadata);
        if (ImGui::Button("Exportieren##aid")) {
            state.legacyZoneMetadata.name = state.zoneNameBuf;
            auto result = core::legacy::SerializeLegacyAid(state.legacyZoneMetadata, state.legacyAidPath);
            state.statusMessage = result ? "Legacy-aid exportiert nach: " + std::string(state.legacyAidPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }
        ImGui::EndDisabled();
        if (state.hasLegacyZoneMetadata) {
            ImGui::InputText("Zonenname", state.zoneNameBuf, sizeof(state.zoneNameBuf));
        }
}

// ===========================================================================================
// Neue Navigationsebene (Projekt-Hub / Projekt-Konfiguration / Map-Editor-Start /
// Arbeitsbereich) - orientiert an den vom Nutzer bereitgestellten Mockups ("NextGen-Editor").
// Beschriftungen laufen über Localization.hpp (T(...)), siehe dort für den aktuellen Umfang
// der Übersetzung (neue Navigationsebene vollständig, tiefe Funktions-Panels noch Deutsch).
// ===========================================================================================

// Einfache, rein vektorielle Karten-Icons (kein Bild-Asset nötig) - bewusst nur grobe,
// wiedererkennbare Annäherungen an die Mockup-Symbole, keine Pixel-genaue Nachbildung.
void DrawIconGlobe(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r, col, 32, 2.5f);
    dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, 2.0f);
    dl->AddEllipse(c, ImVec2(r * 0.42f, r), col, 0.0f, 24, 2.0f);
    dl->AddEllipse(c, ImVec2(r, r * 0.42f), col, 0.0f, 24, 2.0f);
}
void DrawIconPencilPaper(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 p0(c.x - r * 0.55f, c.y - r);
    const ImVec2 p1(c.x + r * 0.55f, c.y + r);
    dl->AddRect(p0, p1, col, 0.0f, 0, 2.5f);
    for (float t = -0.6f; t <= 0.7f; t += 0.35f) {
        dl->AddLine(ImVec2(p0.x + r * 0.15f, c.y + t * r), ImVec2(p1.x - r * 0.15f, c.y + t * r), col, 1.5f);
    }
    dl->AddLine(ImVec2(c.x - r * 0.1f, c.y - r * 1.3f), ImVec2(c.x + r * 1.1f, c.y + r * 0.6f), col, 3.0f);
    dl->AddTriangleFilled(ImVec2(c.x + r * 1.1f, c.y + r * 0.6f), ImVec2(c.x + r * 1.25f, c.y + r * 0.75f),
                           ImVec2(c.x + r * 0.95f, c.y + r * 0.85f), col);
}
void DrawIconBook(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 spine(c.x, c.y - r * 0.7f);
    const ImVec2 spineBottom(c.x, c.y + r * 0.9f);
    const ImVec2 leftPts[4] = {spine, ImVec2(c.x - r * 1.1f, c.y - r * 0.95f), ImVec2(c.x - r * 1.1f, c.y + r * 0.65f), spineBottom};
    const ImVec2 rightPts[4] = {spine, ImVec2(c.x + r * 1.1f, c.y - r * 0.95f), ImVec2(c.x + r * 1.1f, c.y + r * 0.65f), spineBottom};
    dl->AddPolyline(leftPts, 4, col, ImDrawFlags_None, 2.5f);
    dl->AddPolyline(rightPts, 4, col, ImDrawFlags_None, 2.5f);
    dl->AddLine(spine, spineBottom, col, 2.0f);
}
void DrawIconMonitorEye(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 p0(c.x - r, c.y - r * 0.75f);
    const ImVec2 p1(c.x + r, c.y + r * 0.45f);
    dl->AddRect(p0, p1, col, 4.0f, 0, 2.5f);
    dl->AddLine(ImVec2(c.x, p1.y), ImVec2(c.x, c.y + r), col, 2.5f);
    dl->AddLine(ImVec2(c.x - r * 0.5f, c.y + r), ImVec2(c.x + r * 0.5f, c.y + r), col, 2.5f);
    const float eyeCY = (p0.y + p1.y) * 0.5f;
    dl->AddEllipse(ImVec2(c.x, eyeCY), ImVec2(r * 0.55f, r * 0.3f), col, 0.0f, 24, 2.0f);
    dl->AddCircleFilled(ImVec2(c.x, eyeCY), r * 0.14f, col);
}
void DrawIconAtom(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    constexpr float kPi = 3.14159265358979323846f;
    dl->AddEllipse(c, ImVec2(r, r * 0.4f), col, 0.0f, 28, 2.0f);
    dl->AddEllipse(c, ImVec2(r, r * 0.4f), col, kPi / 3.0f, 28, 2.0f);
    dl->AddEllipse(c, ImVec2(r, r * 0.4f), col, -kPi / 3.0f, 28, 2.0f);
    dl->AddCircleFilled(c, r * 0.16f, col);
}
void DrawIconClapper(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 bodyP0(c.x - r, c.y - r * 0.1f);
    const ImVec2 bodyP1(c.x + r, c.y + r);
    dl->AddRect(bodyP0, bodyP1, col, 2.0f, 0, 2.5f);
    const ImVec2 topL(c.x - r * 1.05f, c.y - r * 0.15f);
    const ImVec2 topR(c.x + r * 1.05f, c.y - r * 0.15f);
    dl->AddLine(ImVec2(topL.x, topL.y - r * 0.35f), topR, col, 2.5f);
    dl->AddLine(topL, ImVec2(topR.x, topR.y - r * 0.35f), col, 2.5f);
    for (float x = -0.85f; x < 1.0f; x += 0.4f) {
        dl->AddLine(ImVec2(c.x + x * r, c.y - r * 0.2f), ImVec2(c.x + (x + 0.2f) * r, c.y - r * 0.45f), col, 2.0f);
    }
}

using IconDrawFn = void (*)(ImDrawList*, ImVec2, float, ImU32);

// Zeichnet eine einzelne Editor-Karte (siehe Mockup "Projekt"-Übersicht). Gibt true zurück,
// wenn der Start-Knopf in diesem Frame geklickt wurde. Die gelben Klebezettel aus dem Mockup
// waren Hinweise für die Umsetzung (z.B. "Noch nicht entschieden"), keine echten UI-Elemente -
// erscheinen daher hier bewusst NICHT in der gerenderten Karte.
bool DrawEditorCard(const char* id, ImVec2 size, ImU32 bodyColor, ImU32 headerColor, IconDrawFn icon,
                    const char* title, const std::vector<std::string>& features, bool startEnabled) {
    ImGui::PushID(id);
    ImGui::BeginGroup();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float headerH = 34.0f;

    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), bodyColor, 6.0f);
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + headerH), headerColor, 6.0f, ImDrawFlags_RoundCornersTop);

    const ImVec2 titleTextSize = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(origin.x + (size.x - titleTextSize.x) * 0.5f, origin.y + (headerH - titleTextSize.y) * 0.5f),
                IM_COL32(255, 255, 255, 255), title);

    const ImVec2 iconCenter(origin.x + size.x * 0.5f, origin.y + headerH + size.x * 0.32f);
    icon(dl, iconCenter, size.x * 0.22f, IM_COL32(255, 255, 255, 235));

    float textY = origin.y + headerH + size.x * 0.32f + size.x * 0.30f;
    for (const auto& feature : features) {
        dl->AddText(ImVec2(origin.x + 12.0f, textY), IM_COL32(235, 235, 225, 255), feature.c_str());
        textY += ImGui::GetTextLineHeight() + 2.0f;
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + 10.0f, origin.y + size.y - 44.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70, 145, 220, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 95, 155, 255));
    ImGui::BeginDisabled(!startEnabled);
    const bool clicked = ImGui::Button(T("card.start"), ImVec2(size.x - 20.0f, 34.0f));
    ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y + 8.0f));
    ImGui::Dummy(ImVec2(size.x, 1.0f));
    ImGui::EndGroup();
    ImGui::PopID();
    return clicked;
}

// Obere Navigationsleiste, auf allen Bildschirmen der neuen Oberfläche sichtbar - links die
// Tabs, rechts Credits/Donate/? und die Sprachumschaltung (DE/EN, siehe Localization.hpp).
// tabs==nullptr blendet die linken Tabs aus (Detail-Bildschirme zeigen stattdessen NUR den
// aktuellen Titel als "Breadcrumb", siehe Mockup rechtes/zweites Bild).
void DrawTopNav(EditorState& state, const char* breadcrumbTitle) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 8.0f));
    if (breadcrumbTitle != nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
        ImGui::Button(breadcrumbTitle);
        ImGui::PopStyleColor();
    } else {
        struct TabEntry { const char* key; AppScreen target; };
        const TabEntry tabs[] = {
            {"nav.project", AppScreen::ProjectHub},
            {"nav.new", AppScreen::NewProjectConfig},
            {"nav.open", AppScreen::ProjectHub},
            {"nav.edit", AppScreen::NewProjectConfig},
            {"nav.save", AppScreen::ProjectHub},
        };
        for (const auto& tab : tabs) {
            const bool active = (state.screen == tab.target) &&
                                 (std::strcmp(tab.key, "nav.project") == 0) == (state.screen == AppScreen::ProjectHub);
            ImGui::PushStyleColor(ImGuiCol_Button, active ? IM_COL32(55, 125, 195, 255) : IM_COL32(55, 125, 195, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(75, 150, 225, 255));
            if (ImGui::Button(T(tab.key))) {
                if (std::strcmp(tab.key, "nav.new") == 0) {
                    state.project = ProjectConfig{};
                    state.screen = AppScreen::NewProjectConfig;
                } else if (std::strcmp(tab.key, "nav.open") == 0) {
#ifdef _WIN32
                    if (auto picked = BrowseForFolderWindows("Projekt-Ordner w\u00e4hlen")) {
                        std::snprintf(state.project.projectFolder, sizeof(state.project.projectFolder), "%s", picked->c_str());
                        TryLoadProjectConfig(state.project);
                        state.project.hasProject = true;
                        state.statusMessage = "Projekt geladen: " + std::string(state.project.projectFolder);
                    }
#endif
                } else if (std::strcmp(tab.key, "nav.edit") == 0) {
                    if (state.project.hasProject) {
                        state.screen = AppScreen::NewProjectConfig;
                    } else {
                        state.statusMessage = "Kein Projekt aktiv - zuerst unter 'Neu' oder 'Öffnen' eines wählen.";
                    }
                } else if (std::strcmp(tab.key, "nav.save") == 0) {
                    if (state.project.hasProject) {
                        std::string err;
                        state.statusMessage = SaveProjectConfig(state.project, &err) ? T("newproject.saved")
                                                                                      : (T("newproject.savefailed") + err);
                    } else {
                        state.statusMessage = "Kein Projekt aktiv.";
                    }
                } else {
                    state.screen = tab.target;
                }
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
        }
    }

    // Rechtsbündig: Credits / Donate / ? / Sprache
    const float rightWidth = 340.0f;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - rightWidth);
    ImGui::TextDisabled("%s", T("nav.credits"));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", T("nav.donate"));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", T("nav.help"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    int langIdx = app::CurrentLanguage() == app::Language::German ? 0 : 1;
    const char* langItems[] = {"DE", "EN"};
    if (ImGui::Combo("##lang", &langIdx, langItems, 2)) {
        app::SetLanguage(langIdx == 0 ? app::Language::German : app::Language::English);
    }
    ImGui::PopStyleVar();
    ImGui::Separator();
}


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
        ? ImVec4(0.40f, 0.72f, 0.96f, 1.0f)
        : ImVec4(0.72f, 0.74f, 0.78f, 1.0f);
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
}

void OpenShnFile(EditorState& state, const std::filesystem::path& path,
                std::optional<EditorState::ShnSource> sourceOverride = std::nullopt) {
    auto result = core::legacy::LoadShnFile(path);
    if (!result) {
        state.shnStatus = "SHN öffnen fehlgeschlagen: " + result.error();
        return;
    }
    const auto source = sourceOverride.value_or(InferShnSource(path, EditorState::ShnSource::Client));
    auto existing = std::find_if(state.shnFiles.begin(), state.shnFiles.end(), [&](const auto& f) {
        return f.file.path == path && f.source == source;
    });
    if (existing != state.shnFiles.end()) {
        SelectShnDocument(state, static_cast<int>(std::distance(state.shnFiles.begin(), existing)));
    } else {
        EditorState::ShnDocument doc;
        doc.file = std::move(*result);
        doc.source = source;
        state.shnFiles.push_back(std::move(doc));
        SelectShnDocument(state, static_cast<int>(state.shnFiles.size()) - 1);
    }
    state.shnStatus = std::string(ShnSourceName(source)) + " geladen: " + path.filename().string();
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
        auto result = core::legacy::LoadShnFile(it->path());
        if (!result) { ++failed; continue; }
        auto existing = std::find_if(state.shnFiles.begin(), state.shnFiles.end(), [&](const auto& f) {
            return f.file.path == it->path() && f.source == source;
        });
        if (existing == state.shnFiles.end()) {
            EditorState::ShnDocument doc;
            doc.file = std::move(*result);
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

bool ShnProfileMatch(const std::string& filename, int profile) {
    const std::string n = LowerAscii(filename);
    static const std::array<std::vector<std::string>, 7> tokens = {{
        {"item", "money", "sell", "buy", "price", "icon"},
        {"npc", "shop", "dialog", "merchant"},
        {"mob", "monster", "drop"},
        {"skill", "ability"},
        {"shop", "money", "price", "buy", "sell"},
        {"quest", "dialog", "condition", "reward"},
        {"exp", "xp", "rate"}
    }};
    if (profile < 0 || profile >= static_cast<int>(tokens.size())) return false;
    return std::any_of(tokens[static_cast<std::size_t>(profile)].begin(), tokens[static_cast<std::size_t>(profile)].end(),
                       [&](const std::string& token) { return n.find(token) != std::string::npos; });
}

const char* ShnProfileName(int profile) {
    static const char* names[] = {"Neues Item", "Neuer NPC", "Neuer Mob", "Neuer Skill", "Shop / Preis", "Neue Quest", "XP / Rate"};
    return (profile >= 0 && profile < 7) ? names[profile] : names[0];
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
        ImGui::SetNextItemWidth(520.0f);
        std::vector<char> buf(state.shnEditBuffer.begin(), state.shnEditBuffer.end());
        buf.push_back('\0');
        auto apply = [&]() {
            auto parsed = core::legacy::ParseShnValue(column, state.shnEditBuffer);
            if (parsed) {
                file.rows[static_cast<std::size_t>(state.shnSelectedRow)].values[static_cast<std::size_t>(state.shnSelectedColumn)] = std::move(*parsed);
                doc.dirty = true;
                state.shnStatus = std::string(ShnSourceName(doc.source)) + ": Zelle geändert (noch nicht gespeichert).";
                state.shnEditPopupOpen = false;
                ImGui::CloseCurrentPopup();
            } else state.shnStatus = parsed.error();
        };
        if (ImGui::InputText("##shncell", buf.data(), buf.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            state.shnEditBuffer.assign(buf.data()); apply();
        }
        ImGui::SameLine(); if (ImGui::Button("Übernehmen")) { apply(); }
        ImGui::SameLine(); if (ImGui::Button("Abbrechen")) { state.shnEditPopupOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

void DrawShnGrid(EditorState& state) {
    if (state.shnSelectedFile < 0 || state.shnSelectedFile >= static_cast<int>(state.shnFiles.size())) {
        ImGui::TextDisabled("Keine SHN-Datei geöffnet."); return;
    }
    auto& doc = state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)];
    auto& file = doc.file;
    ImGui::TextColored(ShnSourceColor(doc.source), "%s", ShnSourceName(doc.source));
    ImGui::SameLine(); ImGui::Text("%s", file.FileName().c_str());
    ImGui::SameLine(); ImGui::TextDisabled("%zu Zeilen | %zu Spalten | V%u | %s%s", file.rows.size(), file.columns.size(), file.version,
        file.encrypted ? "verschlüsselt" : "raw", doc.dirty ? " | GEÄNDERT" : "");
    ImGui::Separator();
    const std::string needle = LowerAscii(state.shnSearch);
    ImGui::BeginChild("##shnGrid", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    const int visibleCols = static_cast<int>(file.columns.size());
    if (visibleCols > 0 && ImGui::BeginTable("##shnTable", visibleCols + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit, ImVec2(0,0))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        for (const auto& c : file.columns) ImGui::TableSetupColumn(c.name.c_str(), ImGuiTableColumnFlags_WidthFixed, std::max(100.0f, std::min(260.0f, 14.0f * static_cast<float>(c.name.size()+2))));
        ImGui::TableHeadersRow();
        for (std::size_t ri=0; ri<file.rows.size(); ++ri) {
            const auto& row=file.rows[ri]; bool matches = needle.empty();
            if (!matches) for (std::size_t ci=0; ci<row.values.size(); ++ci) {
                if (state.shnSearchColumns && LowerAscii(file.columns[ci].name).find(needle) != std::string::npos) { matches=true; break; }
                if (state.shnSearchValues && LowerAscii(core::legacy::ShnValueToString(row.values[ci])).find(needle) != std::string::npos) { matches=true; break; }
            }
            if (state.shnFilterActive && !matches) continue;
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", ri);
            for (std::size_t ci=0; ci<row.values.size(); ++ci) {
                ImGui::TableSetColumnIndex(static_cast<int>(ci+1));
                std::string label=ShnShortValue(row.values[ci]);
                const bool selected = state.shnSelectedRow == static_cast<int>(ri) && state.shnSelectedColumn == static_cast<int>(ci);
                if (ImGui::Selectable((label+"##shncell"+std::to_string(ri)+"_"+std::to_string(ci)).c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    state.shnSelectedRow=static_cast<int>(ri); state.shnSelectedColumn=static_cast<int>(ci);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { state.shnEditBuffer=core::legacy::ShnValueToString(row.values[ci]); state.shnEditPopupOpen=true; }
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Doppelklick zum Bearbeiten\nTyp: %s", file.TypeName(file.columns[ci]).c_str());
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild(); DrawShnCellEditor(state);
}

void DrawShnSourceList(EditorState& state, EditorState::ShnSource source, const char* id) {
    ImGui::TextColored(ShnSourceColor(source), "%s", ShnSourceName(source));
    ImGui::SameLine();
    std::size_t count = 0; for (const auto& d : state.shnFiles) if (d.source == source) ++count;
    ImGui::TextDisabled("(%zu Dateien)", count);
    ImGui::BeginChild(id, ImVec2(0, 170), true);
    for (int i : ShnIndicesForSource(state, source)) {
        auto& doc = state.shnFiles[static_cast<std::size_t>(i)];
        const bool selected = state.shnSelectedFile == i;
        std::string label = doc.file.FileName() + (doc.dirty ? " *" : "");
        if (ImGui::Selectable((label + "##" + std::to_string(i)).c_str(), selected)) SelectShnDocument(state, i);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", doc.file.path.string().c_str());
    }
    ImGui::EndChild();
}

void DrawShnMultiProfiles(EditorState& state) {
    ImGui::Text("Multi SHN Editor");
    ImGui::TextWrapped("Die Aufgabe markiert Kandidaten in CLIENT und SERVER getrennt. Die Markierung basiert auf Dateinamen/Tabellenkontext und ist bewusst eine Kandidatenliste, keine behauptete harte Abhängigkeit.");
    const char* profiles[] = {"Neues Item", "Neuer NPC", "Neuer Mob", "Neuer Skill", "Shop / Preis", "Neue Quest", "XP / Rate"};
    ImGui::SetNextItemWidth(260.0f); ImGui::Combo("Aufgabe", &state.shnMultiProfile, profiles, static_cast<int>(std::size(profiles)));
    ImGui::Separator();
    for (EditorState::ShnSource source : {EditorState::ShnSource::Client, EditorState::ShnSource::Server}) {
        ImGui::TextColored(ShnSourceColor(source), "%s", ShnSourceName(source));
        ImGui::BeginChild(source == EditorState::ShnSource::Client ? "##multiClient" : "##multiServer", ImVec2(0, 190), true);
        bool any = false;
        for (int i : ShnIndicesForSource(state, source)) {
            auto& doc = state.shnFiles[static_cast<std::size_t>(i)];
            const bool candidate = ShnProfileMatch(doc.file.FileName(), state.shnMultiProfile);
            if (!candidate) continue;
            any = true;
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 70, 255));
            if (ImGui::Selectable(("◆ " + doc.file.FileName() + (doc.dirty ? " *" : "") + "##multi" + std::to_string(i)).c_str(), state.shnSelectedFile == i)) SelectShnDocument(state, i);
            ImGui::PopStyleColor();
            ImGui::SameLine(); ImGui::TextDisabled("Kandidat");
        }
        if (!any) ImGui::TextDisabled("Keine Kandidaten in den geladenen %s-SHN gefunden.", ShnSourceName(source));
        ImGui::EndChild();
    }
    ImGui::TextDisabled("Hinweis: Gelb = prüfen/ggf. ändern. Die tatsächliche Abhängigkeit wird erst als gesichert markiert, wenn sie aus den vorhandenen SHN-/Datenbeziehungen belegt ist.");
}

void DrawShnEditor(EditorState& state) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 30, 38, 255));
    ImGui::BeginChild("##shnEditor", ImVec2(0,0), false);
    ImGui::TextColored(ImVec4(0.40f,0.72f,0.96f,1.0f), "SHN Editor"); ImGui::SameLine(); ImGui::TextDisabled("— Fiesta SHN Tabellen");
    if (ImGui::Button("← Zurück")) state.screen = AppScreen::ProjectHub;
    ImGui::SameLine(); ImGui::Separator();
    const char* tabs[] = {"Single SHN Editor", "Multi SHN Editor", "XP Rate Editor", "Buy & Sell Editor", "Quest Editor"};
    for (int i=0;i<5;++i) { if(i) ImGui::SameLine(); bool active=state.shnSubTab==i; ImGui::PushStyleColor(ImGuiCol_Button, active?IM_COL32(55,125,195,255):IM_COL32(48,56,68,255)); if(ImGui::Button(tabs[i])) state.shnSubTab=i; ImGui::PopStyleColor(); }
    ImGui::Separator();

    const float leftW=300.0f; const float gap=8.0f; const ImVec2 avail=ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##shnLeft", ImVec2(leftW, avail.y), true);
    ImGui::TextColored(ImVec4(0.55f,0.82f,1.0f,1.0f), state.shnSubTab==0?"Single SHN Editor":"SHN Dateien"); ImGui::Separator();
#ifdef _WIN32
    if (ImGui::Button("CLIENT: SHN-Ordner einlesen", ImVec2(-1,0))) if(auto p=BrowseForFolderWindows("CLIENT SHN Ordner wählen")) ScanShnFolder(state,*p,EditorState::ShnSource::Client);
    if (ImGui::Button("SERVER: SHN-Ordner einlesen", ImVec2(-1,0))) if(auto p=BrowseForFolderWindows("SERVER SHN Ordner wählen")) ScanShnFolder(state,*p,EditorState::ShnSource::Server);
    if (ImGui::Button("Einzelne SHN öffnen...", ImVec2(-1,0))) if(auto p=BrowseForShnFileWindows("Fiesta SHN Datei öffnen")) { std::snprintf(state.shnPath,sizeof(state.shnPath),"%s",p->c_str()); OpenShnFile(state,*p); }
#endif
    ImGui::InputText("Datei", state.shnPath, sizeof(state.shnPath));
    if (ImGui::Button("Pfad öffnen", ImVec2(-1,0))) OpenShnFile(state, state.shnPath);
    if (!state.shnClientRoot.empty()) ImGui::TextDisabled("Client: %s", state.shnClientRoot.c_str());
    if (!state.shnServerRoot.empty()) ImGui::TextDisabled("Server: %s", state.shnServerRoot.c_str());
    ImGui::Separator();
    DrawShnSourceList(state, EditorState::ShnSource::Client, "##shnClientFiles");
    DrawShnSourceList(state, EditorState::ShnSource::Server, "##shnServerFiles");
    if (state.shnSelectedFile>=0 && state.shnSelectedFile<static_cast<int>(state.shnFiles.size())) {
        auto& doc=state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)]; auto& f=doc.file;
        if(ImGui::Button("Speichern",ImVec2(-1,0))) { auto r=core::legacy::SaveShnFile(f,f.path); state.shnStatus=r?(std::string(ShnSourceName(doc.source))+" gespeichert: "+f.FileName()):"Speichern fehlgeschlagen: "+r.error(); if(r) doc.dirty=false; }
        ImGui::Separator();
        ImGui::InputText("Suche",state.shnSearch,sizeof(state.shnSearch));
        ImGui::Checkbox("Spalten durchsuchen",&state.shnSearchColumns); ImGui::Checkbox("Werte durchsuchen",&state.shnSearchValues);
        ImGui::TextDisabled("Filter wird %s angewendet.", state.shnFilterActive ? "automatisch" : "nicht");
        ImGui::Separator();
        if(state.shnSelectedRow>=0 && state.shnSelectedColumn>=0 && state.shnSelectedColumn<static_cast<int>(f.columns.size())) { ImGui::Text("Auswahl: Zeile %d",state.shnSelectedRow); ImGui::TextWrapped("%s",f.columns[static_cast<std::size_t>(state.shnSelectedColumn)].name.c_str()); ImGui::TextDisabled("%s",f.TypeName(f.columns[static_cast<std::size_t>(state.shnSelectedColumn)]).c_str()); }
    }
    if(!state.shnStatus.empty()) { ImGui::Separator(); ImGui::TextWrapped("%s",state.shnStatus.c_str()); }
    ImGui::EndChild();
    ImGui::SameLine(0,gap);
    ImGui::BeginChild("##shnMain", ImVec2(avail.x-leftW-gap, avail.y), true);
    if(state.shnSubTab==0) DrawShnGrid(state);
    else if(state.shnSubTab==1) DrawShnMultiProfiles(state);
    else if(state.shnSubTab==2) { ImGui::Text("XP Rate Editor"); ImGui::TextWrapped("Die geladenen CLIENT- und SERVER-SHN bleiben getrennt. Nutze Multi SHN → XP / Rate für die Kandidatenübersicht."); }
    else if(state.shnSubTab==3) { ImGui::Text("Buy & Sell Editor"); ImGui::TextWrapped("CLIENT und SERVER werden getrennt geführt. Nutze Multi SHN → Shop / Preis für die Kandidatenübersicht."); }
    else { ImGui::Text("Quest Editor"); ImGui::TextWrapped("CLIENT und SERVER werden getrennt geführt. Nutze Multi SHN → Neue Quest für die Kandidatenübersicht."); }
    ImGui::EndChild(); ImGui::EndChild(); ImGui::PopStyleColor();
}

void DrawProjectHub(EditorState& state) {
    DrawTopNav(state, nullptr);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float gap = 10.0f;
    const float cardW = (avail.x - gap * 5.0f) / 6.0f;
    const float cardH = avail.y - 20.0f;
    const ImVec2 cardSize(cardW, cardH);

    struct CardDef {
        const char* titleKey; std::vector<const char*> featureKeys;
        ImU32 body; ImU32 header; IconDrawFn icon; bool enabled;
    };
    const CardDef cards[] = {
        {"card.mapeditor.title",
         {"card.mapeditor.f1","card.mapeditor.f2","card.mapeditor.f3","card.mapeditor.f4",
          "card.mapeditor.f5","card.mapeditor.f6","card.mapeditor.f7","card.mapeditor.f8"},
         IM_COL32(38, 56, 30, 255), IM_COL32(28, 42, 22, 255), DrawIconGlobe, true},
        {"card.shn.title", {"card.shn.f1","card.shn.f2","card.shn.f3","card.shn.f4"},
         IM_COL32(70, 78, 26, 255), IM_COL32(54, 60, 20, 255), DrawIconPencilPaper, true},
        {"card.quest.title", {"card.quest.f1","card.quest.f2"},
         IM_COL32(78, 70, 26, 255), IM_COL32(60, 54, 20, 255), DrawIconBook, true},
        {"card.interface.title", {"card.interface.f1","card.interface.f2"},
         IM_COL32(70, 50, 24, 255), IM_COL32(54, 38, 18, 255), DrawIconMonitorEye, true},
        {"card.droptable.title", {"card.droptable.f1","card.droptable.f2"},
         IM_COL32(60, 34, 18, 255), IM_COL32(46, 26, 14, 255), DrawIconAtom, true},
        {"card.skill.title", {"card.skill.f1","card.skill.f2","card.skill.f3"},
         IM_COL32(76, 22, 18, 255), IM_COL32(58, 16, 14, 255), DrawIconClapper, true},
    };

    for (std::size_t i = 0; i < std::size(cards); ++i) {
        const auto& card = cards[i];
        std::vector<std::string> features;
        for (const char* fk : card.featureKeys) features.emplace_back(std::string("\u2022 ") + T(fk));
        const bool clicked = DrawEditorCard(card.titleKey, cardSize, card.body, card.header, card.icon,
                                             T(card.titleKey), features, card.enabled);
        if (clicked) {
            if (std::strcmp(card.titleKey, "card.mapeditor.title") == 0) {
                state.screen = AppScreen::MapEditorLauncher;
            } else if (std::strcmp(card.titleKey, "card.shn.title") == 0) {
                state.screen = AppScreen::ShnEditor;
            } else {
                state.comingSoonTitle = T(card.titleKey);
                state.screen = AppScreen::ComingSoon;
            }
        }
        if (i + 1 < std::size(cards)) ImGui::SameLine(0.0f, gap);
    }

    if (!state.statusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }
}

// (Hinweis: die gelben Klebezettel aus den Mockups waren Umsetzungs-Hinweise, keine
// echten UI-Elemente - daher gibt es hier bewusst keine "Sticky Note"-Komponente mehr.)

void DrawNewProjectConfig(EditorState& state) {
    DrawTopNav(state, T("newproject.title"));
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    ImGui::SetNextItemWidth(500.0f);
    ImGui::Text("%s", T("newproject.name")); ImGui::SameLine(180.0f);
    ImGui::SetNextItemWidth(500.0f);
    ImGui::InputText("##projname", state.project.name, sizeof(state.project.name));

    ImGui::Text("%s", T("newproject.projectfolder")); ImGui::SameLine(180.0f);
    ImGui::SetNextItemWidth(460.0f);
    ImGui::InputText("##projfolder", state.project.projectFolder, sizeof(state.project.projectFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (ImGui::Button(("...##pf"))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.projectfolder"))) {
            std::snprintf(state.project.projectFolder, sizeof(state.project.projectFolder), "%s", picked->c_str());
        }
    }
#endif

    ImGui::Text("%s", T("newproject.clientfolder")); ImGui::SameLine(180.0f);
    ImGui::SetNextItemWidth(460.0f);
    ImGui::InputText("##clientfolder", state.project.clientFolder, sizeof(state.project.clientFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (ImGui::Button(("...##cf"))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.clientfolder"))) {
            std::snprintf(state.project.clientFolder, sizeof(state.project.clientFolder), "%s", picked->c_str());
        }
    }
#endif

    ImGui::Text("%s", T("newproject.serverfolder")); ImGui::SameLine(180.0f);
    ImGui::SetNextItemWidth(460.0f);
    ImGui::InputText("##serverfolder", state.project.serverFolder, sizeof(state.project.serverFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (ImGui::Button(("...##sf"))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.serverfolder"))) {
            std::snprintf(state.project.serverFolder, sizeof(state.project.serverFolder), "%s", picked->c_str());
        }
    }
#endif
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70, 145, 220, 255));
    if (ImGui::Button(T("newproject.createsave"), ImVec2(620.0f, 44.0f))) {
        if (state.project.name[0] == '\0') {
            state.statusMessage = T("newproject.namemissing");
        } else if (state.project.projectFolder[0] == '\0') {
            state.statusMessage = T("newproject.folderMissing");
        } else {
            std::string err;
            if (SaveProjectConfig(state.project, &err)) {
                state.project.hasProject = true;
                state.statusMessage = T("newproject.saved");
                state.screen = AppScreen::ProjectHub;
            } else {
                state.statusMessage = std::string(T("newproject.savefailed")) + err;
            }
        }
    }
    ImGui::PopStyleColor(2);

    if (!state.statusMessage.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }
}

void DrawMapEditorLauncher(EditorState& state) {
    DrawTopNav(state, T("mapeditor.title"));
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // "New Map" / "Map Öffnen" wählen jetzt aktiv aus, welches der beiden Panels unten
    // sichtbar ist (vorher taten die Knöpfe nichts - beide Panels waren immer zugleich zu
    // sehen).
    const bool onNewMap = state.mapLauncherView == EditorState::MapLauncherView::NewMap;
    const bool onBrowse = state.mapLauncherView == EditorState::MapLauncherView::Browse;
    ImGui::PushStyleColor(ImGuiCol_Button, onNewMap ? IM_COL32(55, 125, 195, 255) : IM_COL32(55, 125, 195, 255));
    if (ImGui::Button(T("mapeditor.newmap"))) state.mapLauncherView = EditorState::MapLauncherView::NewMap;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, onBrowse ? IM_COL32(55, 125, 195, 255) : IM_COL32(55, 125, 195, 255));
    if (ImGui::Button(T("mapeditor.openmap"))) state.mapLauncherView = EditorState::MapLauncherView::Browse;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    if (ImGui::Button(T("nav.back"))) state.screen = AppScreen::ProjectHub;
    ImGui::PopStyleColor();
    ImGui::Separator();

    // Karten liegen laut Rückmeldung unter <Client Ordner>/resmap/... - es kann dabei
    // MEHRERE gleichnamige "resmap"-Ordner geben (z.B. einen leeren/falschen an anderer
    // Stelle) - ResolveMapSearchRootAndScan probiert JEDEN und nimmt den mit den meisten
    // echten Karten, statt blind den ersten Treffer zu verwenden (siehe CHANGELOG). Neu
    // gescannt wird nur, wenn sich der Client-Ordner ändert - NICHT bei jedem Frame.
    if (state.project.clientFolder[0] != '\0') {
        const std::string clientFolderStr = state.project.clientFolder;
        if (clientFolderStr != state.lastScannedMapRoot) {
            const auto resolution = ResolveMapSearchRootAndScan(state.project.clientFolder);
            state.discoveredMaps = resolution.maps;
            state.lastResmapCandidateCount = resolution.candidateCount;
            state.lastResmapFound = resolution.root.has_value();
            state.lastResmapResolvedPath = resolution.root
                ? (resolution.candidateCount > 1
                       ? (resolution.root->string() + "  (" + std::to_string(resolution.candidateCount) + " 'resmap'-Ordner gefunden, dieser hatte die meisten Karten)")
                       : resolution.root->string())
                : ("(kein 'resmap'-Ordner unter " + clientFolderStr + " gefunden)");
            state.lastScannedMapRoot = clientFolderStr;
            state.selectedMapIndex = -1;
        }
    }
    const std::string& resolvedRoot = state.lastResmapResolvedPath;
    const bool resmapFound = state.lastResmapFound;

    if (onNewMap) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(55, 125, 195, 255));
    ImGui::BeginChild("##createNewMap", ImVec2(420.0f, 420.0f), true);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::TextColored(ImVec4(0.90f, 0.94f, 1.0f, 1.0f), "%s", T("mapeditor.createnewmap"));
    ImGui::Separator();
    ImGui::Text("%s", T("mapeditor.name")); ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("##newmapname", state.newMapName, sizeof(state.newMapName));
    ImGui::Text("%s", T("mapeditor.xlength")); ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputInt("##newmapx", &state.newMapWidth);
    ImGui::Text("%s", T("mapeditor.ybreadth")); ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputInt("##newmapy", &state.newMapHeight);
    ImGui::Text("%s", T("mapeditor.texturelayer")); ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("##newmaplayer", state.newMapTextureLayer, sizeof(state.newMapTextureLayer));
    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    if (ImGui::Button(T("mapeditor.createmap"), ImVec2(180.0f, 32.0f))) {
        const int w = std::max(2, state.newMapWidth);
        const int h = std::max(2, state.newMapHeight);
        state.heightmap = core::Heightmap(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 50.0f, 50.0f);
        state.undo.Clear();
        state.meshDirty = true;
        SyncWalkGridSize(state);
        state.textureStack = core::TextureLayerStack(512, 512);
        state.selectedLayer = static_cast<int>(state.textureStack.AddLayer(
            state.newMapTextureLayer[0] != '\0' ? state.newMapTextureLayer : "Base", "base.dds", 1.0f));
        state.layerPreviewDirty = true;
        std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", state.newMapName);
        if (state.project.projectFolder[0] != '\0') {
            std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", state.project.projectFolder);
        }
        state.statusMessage = "Neue Karte '" + std::string(state.newMapName) + "' angelegt (" +
                               std::to_string(w) + "x" + std::to_string(h) + ").";
        state.screen = AppScreen::MapEditorWorkspace;
    }
    ImGui::SameLine();
    if (ImGui::Button(T("mapeditor.cancel"), ImVec2(120.0f, 32.0f))) {
        state.newMapName[0] = '\0';
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    } // onNewMap

    if (onBrowse) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(55, 125, 195, 255));
    ImGui::BeginChild("##browseMaps", ImVec2(420.0f, 420.0f), true);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::TextColored(ImVec4(0.90f, 0.94f, 1.0f, 1.0f), "%s", T("mapeditor.browsemaps"));
    ImGui::Separator();
    if (state.project.clientFolder[0] == '\0') {
        ImGui::PushTextWrapPos(400.0f);
        ImGui::TextWrapped("%s", T("mapeditor.noclientfolder"));
        ImGui::PopTextWrapPos();
    } else {
        // Transparenz statt stillem Leerbleiben: zeigt genau, WO gesucht wurde und wie viele
        // Karten gefunden wurden - damit sofort erkennbar ist, ob ein falscher Ordner gewählt
        // wurde, statt nur eine rätselhaft leere Liste zu sehen.
        ImGui::PushTextWrapPos(400.0f);
        ImGui::TextWrapped("Suche in: %s", resolvedRoot.c_str());
        ImGui::PopTextWrapPos();
        if (resmapFound) {
            ImGui::Text("%d Karte(n) gefunden.", static_cast<int>(state.discoveredMaps.size()));
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Neu durchsuchen")) {
            state.lastScannedMapRoot.clear(); // erzwingt erneutes vollständiges Scannen oben
        }
        ImGui::BeginChild("##mapList", ImVec2(0.0f, 230.0f), true);
        if (state.discoveredMaps.empty()) {
            ImGui::TextDisabled("%s", resmapFound
                ? "Keine .ini-Kartendateien im gefundenen 'resmap'-Ordner."
                : "'resmap' nicht gefunden - Client Ordner pruefen oder 'Neu durchsuchen' klicken.");
        }
        for (int i = 0; i < static_cast<int>(state.discoveredMaps.size()); ++i) {
            const bool selected = state.selectedMapIndex == i;
            if (ImGui::Selectable(state.discoveredMaps[static_cast<std::size_t>(i)].name.c_str(), selected)) {
                state.selectedMapIndex = i;
                std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s",
                              state.discoveredMaps[static_cast<std::size_t>(i)].iniPath.c_str());
            }
            // Vollen Pfad beim Überfahren anzeigen - direkte Kontrolle, WAS genau gefunden
            // wurde, ohne erst eine Karte öffnen zu müssen.
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", state.discoveredMaps[static_cast<std::size_t>(i)].iniPath.c_str());
            }
        }
        ImGui::EndChild();
    }
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    ImGui::BeginDisabled(state.selectedMapIndex < 0);
    if (ImGui::Button(T("mapeditor.open"), ImVec2(110.0f, 32.0f))) {
        core::legacy::LegacyMapOpenReport report;
        auto result = core::legacy::OpenLegacyMap(state.legacyMapIniPath, &report);
        if (result) {
            const std::filesystem::path iniPath(state.legacyMapIniPath);
            ApplyProjectToState(state, std::move(*result), iniPath.parent_path());
            if (state.project.projectFolder[0] != '\0') {
                std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", state.project.projectFolder);
            } else {
                std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", iniPath.parent_path().string().c_str());
            }
            std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", iniPath.stem().string().c_str());
            state.statusMessage = "Karte ge\u00f6ffnet (" + std::to_string(report.issues.size()) + " Hinweis(e)).";
            state.screen = AppScreen::MapEditorWorkspace;
        } else {
            state.statusMessage = "Karte \u00f6ffnen fehlgeschlagen: " + result.error();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(T("mapeditor.cancel"), ImVec2(110.0f, 32.0f))) {
        state.selectedMapIndex = -1;
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    } // onBrowse

    if (!state.statusMessage.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }
}

void DrawComingSoon(EditorState& state) {
    DrawTopNav(state, state.comingSoonTitle.c_str());
    ImGui::Dummy(ImVec2(0.0f, 40.0f));
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const char* msg = T("card.comingsoon");
    const ImVec2 textSize = ImGui::CalcTextSize(msg);
    ImGui::SetCursorPosX((avail.x - textSize.x) * 0.5f);
    ImGui::TextDisabled("%s", msg);
    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::SetCursorPosX((avail.x - 120.0f) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    if (ImGui::Button(T("nav.back"), ImVec2(120.0f, 34.0f))) {
        state.screen = AppScreen::ProjectHub;
    }
    ImGui::PopStyleColor();
}

// Findet den "resmap"-Ordner (wiederverwendet dieselbe mehrdeutigkeitstolerante Suche wie
// beim Karten-Scan, siehe ResolveMapSearchRootAndScan) - Basis für die Asset-Picker unten.
std::optional<std::filesystem::path> FindResmapRootForAssets(const std::string& clientFolder) {
    if (clientFolder.empty()) return std::nullopt;
    std::string selfName = std::filesystem::path(clientFolder).filename().string();
    std::transform(selfName.begin(), selfName.end(), selfName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (selfName == "resmap") return std::filesystem::path(clientFolder);
    std::vector<std::filesystem::path> candidates;
    FindAllResmapCandidates(clientFolder, 3, candidates);
    if (candidates.empty()) return std::nullopt;
    // Den mit den meisten Karten nehmen, wie beim Karten-Scan - derselbe "richtige" resmap-
    // Ordner ist auch hier die richtige Basis für fieldTexture/nif.
    std::size_t bestIdx = 0;
    std::size_t bestCount = ScanForMaps(candidates[0]).size();
    for (std::size_t i = 1; i < candidates.size(); ++i) {
        const std::size_t count = ScanForMaps(candidates[i]).size();
        if (count > bestCount) { bestIdx = i; bestCount = count; }
    }
    return candidates[bestIdx];
}

// Popup mit Textfilter zur Auswahl einer Datei aus files (relative Pfade). Gibt true zurück
// UND setzt outSelected, wenn der Nutzer in diesem Frame etwas ausgewählt hat. Muss von
// ImGui::OpenPopup(popupId) aus geöffnet werden - zeichnet nur, wenn das Popup offen ist.
bool DrawAssetPickerPopup(const char* popupId, const std::vector<std::string>& files,
                           std::string& filterBuf, std::string& outSelected) {
    bool picked = false;
    ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup(popupId)) {
        char filterCstr[128];
        std::snprintf(filterCstr, sizeof(filterCstr), "%s", filterBuf.c_str());
        if (ImGui::InputTextWithHint("##filter", "Filtern...", filterCstr, sizeof(filterCstr))) {
            filterBuf = filterCstr;
        }
        ImGui::Text("%zu Datei(en)", files.size());
        ImGui::Separator();
        ImGui::BeginChild("##assetList", ImVec2(0.0f, 340.0f), true);
        for (const auto& f : files) {
            if (!filterBuf.empty()) {
                std::string fLower = f, filterLower = filterBuf;
                std::transform(fLower.begin(), fLower.end(), fLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (fLower.find(filterLower) == std::string::npos) continue;
            }
            if (ImGui::Selectable(f.c_str())) {
                outSelected = f;
                picked = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("Abbrechen")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return picked;
}

// Werkzeug-Inhalt für den aktuell aktiven Modus (siehe EditMode) - der Moduswechsel selbst
// erfolgt jetzt über den Tab-Balken des Arbeitsbereichs (siehe DrawWorkspaceTabBar), nicht
// mehr über Radio-Buttons hier. Wird OHNE eigenes Fenster aufgerufen (eingebettet in die
// "Tools/etc"-Spalte, siehe DrawMapEditorWorkspace) - daher kein ImGui::Begin/End mehr.
void DrawToolsContent(EditorState& state) {
    if (state.editMode == EditMode::Heightmap) {
        ImGui::Text("Pinselmodus");
        ImGui::RadioButton("Anheben", reinterpret_cast<int*>(&state.brushMode), static_cast<int>(core::BrushMode::Raise));
        ImGui::RadioButton("Absenken", reinterpret_cast<int*>(&state.brushMode), static_cast<int>(core::BrushMode::Lower));
        ImGui::RadioButton("Glätten", reinterpret_cast<int*>(&state.brushMode), static_cast<int>(core::BrushMode::Smooth));
        ImGui::RadioButton("Einebnen", reinterpret_cast<int*>(&state.brushMode), static_cast<int>(core::BrushMode::Flatten));

        ImGui::Separator();
        ImGui::SliderFloat("Radius", &state.brush.radius, 10.0f, 2000.0f);
        ImGui::SliderFloat("Stärke", &state.brush.strength, 0.1f, 100.0f);
        if (state.brushMode == core::BrushMode::Flatten) {
            ImGui::InputFloat("Zielhöhe", &state.brush.flattenTarget);
        }

        ImGui::Separator();
        ImGui::BeginDisabled(!state.undo.CanUndo());
        if (ImGui::Button("Rückgängig (Strg+Z)")) {
            if (state.undo.Undo(state.heightmap)) state.meshDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.undo.CanRedo());
        if (ImGui::Button("Wiederholen (Strg+Y)")) {
            if (state.undo.Redo(state.heightmap)) state.meshDirty = true;
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        const auto [lo, hi] = state.heightmap.MinMax();
        ImGui::Text("Gitter: %u x %u", state.heightmap.Width(), state.heightmap.Height());
        ImGui::Text("Höhen-Range: [%.2f, %.2f]", lo, hi);
    } else if (state.editMode == EditMode::TexturePaint) {
        ImGui::Text("Layer (%zu)", state.textureStack.LayerCount());
        ImGui::BeginChild("LayerList", ImVec2(0, 120), true);
        for (std::size_t i = 0; i < state.textureStack.LayerCount(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool selected = state.selectedLayer == static_cast<int>(i);
            if (ImGui::Selectable(state.textureStack.Layer(i).name.c_str(), selected)) {
                state.selectedLayer = static_cast<int>(i);
                state.layerPreviewDirty = true;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::InputText("Name##newLayer", state.newLayerName, sizeof(state.newLayerName));
        ImGui::InputText("Diffuse##newLayer", state.newLayerDiffuse, sizeof(state.newLayerDiffuse));
        ImGui::SameLine();
        if (ImGui::Button("Durchsuchen...##tex")) {
            if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
                if (const auto texRoot = FindNamedSubfolder(*resmapRoot, {"fieldtexture"})) {
                    state.availableTextureFiles = ListFilesByExtension(*texRoot, {".dds"});
                    state.textureListScanned = true;
                    state.assetPickerFilter.clear();
                    ImGui::OpenPopup("##texPicker");
                } else {
                    state.statusMessage = "Kein 'fieldTexture'-Ordner unter " + resmapRoot->string() + " gefunden.";
                }
            } else {
                state.statusMessage = "Kein Client-Ordner/'resmap' aktiv - unter 'Neu' ein Projekt mit Client Ordner anlegen.";
            }
        }
        {
            std::string picked;
            if (DrawAssetPickerPopup("##texPicker", state.availableTextureFiles, state.assetPickerFilter, picked)) {
                std::snprintf(state.newLayerDiffuse, sizeof(state.newLayerDiffuse), "%s", picked.c_str());
            }
        }
        ImGui::InputFloat("UV-Scale##newLayer", &state.newLayerUvScale);
        if (ImGui::Button("Layer hinzufügen")) {
            if (state.textureStack.Width() == 0) {
                state.textureStack = core::TextureLayerStack(512, 512); // unabhängiger Default, siehe docs/MAP_FORMAT.md
            }
            const auto idx = state.textureStack.AddLayer(state.newLayerName, state.newLayerDiffuse, state.newLayerUvScale);
            state.selectedLayer = static_cast<int>(idx);
            state.layerPreviewDirty = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(state.selectedLayer < 0);
        if (ImGui::Button("Layer entfernen")) {
            state.textureStack.RemoveLayer(static_cast<std::size_t>(state.selectedLayer));
            state.selectedLayer = -1;
            state.layerPreviewDirty = true;
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::Text("Pinselmodus (Textur)");
        int paintModeInt = static_cast<int>(state.paintMode);
        ImGui::RadioButton("Erhöhen", &paintModeInt, static_cast<int>(core::PaintMode::Increase));
        ImGui::SameLine();
        ImGui::RadioButton("Senken", &paintModeInt, static_cast<int>(core::PaintMode::Decrease));
        state.paintMode = static_cast<core::PaintMode>(paintModeInt);
        ImGui::SliderFloat("Radius##tex", &state.paintSettings.radius, 10.0f, 2000.0f);
        ImGui::SliderFloat("Stärke##tex", &state.paintSettings.strength, 0.01f, 1.0f);

        ImGui::Separator();
        ImGui::BeginDisabled(!state.textureUndo.CanUndo());
        if (ImGui::Button("Rückgängig (Textur)")) {
            if (state.textureUndo.Undo(state.textureStack)) {
                state.layerPreviewDirty = true;
                state.renderer.UpdateBlendTextures(state.textureStack);
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.textureUndo.CanRedo());
        if (ImGui::Button("Wiederholen (Textur)")) {
            if (state.textureUndo.Redo(state.textureStack)) {
                state.layerPreviewDirty = true;
                state.renderer.UpdateBlendTextures(state.textureStack);
            }
        }
        ImGui::EndDisabled();

        if (state.selectedLayer >= 0) {
            ImGui::Separator();
            ImGui::Text("Gewichtssumme (Zelle 0,0): %.3f (sollte ~1.0 sein)", state.textureStack.WeightSumAt(0, 0));
        }
    } else if (state.editMode == EditMode::BlockWalk) {
        ImGui::TextWrapped("Rohwert-Stempel - Bit-Semantik der Legacy-Daten ist nicht vollständig "
                            "gesichert (siehe docs/MAP_FORMAT.md), daher kein 'Begehbar/Blockiert'-"
                            "Toggle, sondern direkter 16-bit-Wert.");
        int stampValue = state.walkSettings.value;
        ImGui::InputInt("Zielwert (int16)", &stampValue);
        state.walkSettings.value = static_cast<std::int16_t>(std::clamp(stampValue, -32768, 32767));
        ImGui::Text("= 0x%04X", static_cast<std::uint16_t>(state.walkSettings.value));

        if (ImGui::Button("Preset: Frei (-1)")) state.walkSettings.value = -1;
        ImGui::SameLine();
        if (ImGui::Button("Preset: Voll blockiert (0)")) state.walkSettings.value = 0;
        ImGui::SameLine();
        if (ImGui::Button("Preset: 0xFFFF")) state.walkSettings.value = static_cast<std::int16_t>(0xFFFF);

        // Radius-Bereich deutlich abgesenkt (vorher 10-1000, jetzt 1-1000) + logarithmische
        // Skalierung, damit sich kleine Werte für präzises Editieren feiner einstellen lassen
        // (bei linearer Skalierung nimmt ein 1000er-Bereich fast den ganzen Schieberegler für
        // grobe Werte ein). Zusätzlich ein "1 Zelle"-Knopf, der den Radius exakt auf die halbe
        // Diagonale einer Walk-Gitterzelle setzt - trifft dann garantiert nur die nächstgelegene
        // Zelle, unabhängig von der (nicht quadratischen) Zellgröße.
        ImGui::SliderFloat("Radius##walk", &state.walkSettings.radius, 1.0f, 1000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
        ImGui::SameLine();
        if (ImGui::Button("1 Zelle##walk")) {
            const float spanX = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth();
            const float spanZ = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight();
            if (state.walkGrid.Width() > 0 && state.walkGrid.Height() > 0) {
                const float cellW = spanX / static_cast<float>(state.walkGrid.Width());
                const float cellH = spanZ / static_cast<float>(state.walkGrid.Height());
                state.walkSettings.radius = std::max(0.5f, std::min(cellW, cellH) * 0.5f);
            }
        }

        ImGui::Separator();
        ImGui::BeginDisabled(!state.walkUndo.CanUndo());
        if (ImGui::Button("Rückgängig (Walk)")) {
            if (state.walkUndo.Undo(state.walkGrid)) state.walkPreviewDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.walkUndo.CanRedo());
        if (ImGui::Button("Wiederholen (Walk)")) {
            if (state.walkUndo.Redo(state.walkGrid)) state.walkPreviewDirty = true;
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::Text("Gitter: %u x %u", state.walkGrid.Width(), state.walkGrid.Height());
    } else if (state.editMode == EditMode::ObjectPlacement) {
        ImGui::Text("Klick im Editor (2D) unten:");
        ImGui::RadioButton("Platzieren", &state.objectPlaceMode, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Auswählen", &state.objectPlaceMode, 0);

        if (state.objectPlaceMode) {
            ImGui::InputText("Modellpfad", state.newObjectModelPath, sizeof(state.newObjectModelPath));
            ImGui::SameLine();
            if (ImGui::Button("Durchsuchen...##nif")) {
                if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
                    if (const auto nifRoot = FindNamedSubfolder(*resmapRoot, {"nif", "nifs"})) {
                        state.availableNifFiles = ListFilesByExtension(*nifRoot, {".nif"});
                        state.nifListScanned = true;
                        state.assetPickerFilter.clear();
                        ImGui::OpenPopup("##nifPicker");
                    } else {
                        state.statusMessage = "Kein 'nif'/'nifs'-Ordner unter " + resmapRoot->string() + " gefunden.";
                    }
                } else {
                    state.statusMessage = "Kein Client-Ordner/'resmap' aktiv - unter 'Neu' ein Projekt mit Client Ordner anlegen.";
                }
            }
            {
                std::string picked;
                if (DrawAssetPickerPopup("##nifPicker", state.availableNifFiles, state.assetPickerFilter, picked)) {
                    std::snprintf(state.newObjectModelPath, sizeof(state.newObjectModelPath), "%s", picked.c_str());
                }
            }
            ImGui::SliderFloat("Rotation um Hochachse (°)##new", &state.newObjectRotDeg, -180.0f, 180.0f);
            ImGui::SliderFloat("Skalierung##new", &state.newObjectScale, 0.1f, 5.0f);
        }

        ImGui::Separator();
        ImGui::Text("Objekte (%zu)", state.placementSet.Count());
        ImGui::TextDisabled("  davon %zu mit echtem Mesh (Rest: Platzhalter)", state.nifMeshRenderer.RealMeshCount());
        ImGui::BeginChild("ObjectList", ImVec2(0, 150), true);
        for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool selected = state.selectedObject == static_cast<int>(i);
            char label[600];
            std::snprintf(label, sizeof(label), "%s", state.placementSet.At(i).modelPath.c_str());
            if (ImGui::Selectable(label, selected)) {
                state.selectedObject = static_cast<int>(i);
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        if (state.selectedObject >= 0 && static_cast<std::size_t>(state.selectedObject) < state.placementSet.Count()) {
            auto& obj = state.placementSet.At(static_cast<std::size_t>(state.selectedObject));
            ImGui::Separator();
            ImGui::Text("Position: %.1f, %.1f, %.1f", obj.posX, obj.posY, obj.posZ);

            float rotDeg = std::atan2(obj.rotY, obj.rotW) * 2.0f * 180.0f / 3.14159265f;
            if (ImGui::SliderFloat("Rotation um Hochachse (°)", &rotDeg, -180.0f, 180.0f)) {
                const float rad = rotDeg * 3.14159265f / 180.0f / 2.0f;
                obj.rotX = 0.0f; obj.rotZ = 0.0f;
                obj.rotY = std::sin(rad);
                obj.rotW = std::cos(rad);
            }
            ImGui::SliderFloat("Skalierung", &obj.scale, 0.1f, 5.0f);

            if (ImGui::Button("Objekt löschen")) {
                state.placementSet.RemoveObject(static_cast<std::size_t>(state.selectedObject));
                state.selectedObject = -1;
            }
        }
    } else {
        // Npcs / NpcAi / Mobs / MobAi - laut Mockup vorgesehene, aber noch nicht gebaute
        // Bereiche des MapEditors (siehe Karten-Feature-Liste im Projekt-Hub).
        ImGui::TextDisabled("%s", T("workspace.notimplemented"));
    }

    ImGui::Separator();
    ImGui::Checkbox(T("workspace.wireframe"), &state.wireframe);
    if (ImGui::Button(T("workspace.centercamera"))) {
        const float spanX = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth();
        const float spanZ = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight();
        const auto [lo, hi] = state.heightmap.MinMax();
        state.camera.SetTarget(spanX * 0.5f, (lo + hi) * 0.5f, spanZ * 0.5f);
    }
}

void DrawEditor2DContent(EditorState& state) {
    const bool texMode = state.editMode == EditMode::TexturePaint;
    const bool walkMode = state.editMode == EditMode::BlockWalk;
    const bool objectMode = state.editMode == EditMode::ObjectPlacement;

    if (texMode && (state.selectedLayer < 0 || static_cast<std::size_t>(state.selectedLayer) >= state.textureStack.LayerCount())) {
        ImGui::TextDisabled("Kein Layer ausgewählt - links in den Werkzeugen einen wählen oder anlegen.");
        return;
    }

    if (texMode && state.layerPreviewDirty) {
        UpdateLayerPreviewTexture(state);
        state.layerPreviewDirty = false;
    }
    if (walkMode && state.walkPreviewDirty) {
        UpdateWalkPreviewTexture(state);
        state.walkPreviewDirty = false;
    }

    // Objekt-Placement/Heightmap/Texturing nutzen jetzt alle dieselbe echte, texturierte
    // Draufsicht als Hintergrund (statt reiner Graustufen) - siehe
    // HeightmapRenderer::BeginTopDownScene. Bei Block&Walk wird die Heatmap zusätzlich
    // halbtransparent rot darübergelegt, statt sie allein anzuzeigen.
    const std::uint32_t gridW = walkMode ? state.walkGrid.Width() : (texMode ? state.textureStack.Width() : state.heightmap.Width());
    const std::uint32_t gridH = walkMode ? state.walkGrid.Height() : (texMode ? state.textureStack.Height() : state.heightmap.Height());
    if (gridW == 0 || gridH == 0) {
        return;
    }

    // WICHTIG: Die angezeigte Fläche orientiert sich IMMER an der Form der Heightmap, NICHT an
    // der nativen Pixel-Form des jeweiligen Rohgitters. Block&Walk hat z.B. ein extrem
    // längliches Rohformat (Seitenverhältnis 1:16, siehe docs/MAP_FORMAT.md) - trotzdem soll der
    // Nutzer auf einer Fläche malen, die wie die Heightmap aussieht ("wo auf der Karte"), nicht
    // auf der krummen Rohform. Die Grafikkarte streckt/staucht die Textur beim Zeichnen
    // automatisch auf die Zielgröße (wie eine Textur auf ein andersförmiges Quad) - Speichern
    // konvertiert unsichtbar zurück ins Original-Rohformat (siehe SaveLegacyMap), der Nutzer
    // bekommt davon nie etwas mit.
    const float spanX = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth();
    const float spanZ = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight();

    const float availW = ImGui::GetContentRegionAvail().x;
    const float aspect = spanX > 0.0f ? (spanZ / spanX) : 1.0f;
    const ImVec2 imageSize(availW, availW * aspect);
    const ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();

    state.renderer.BeginTopDownScene(static_cast<int>(imageSize.x), static_cast<int>(imageSize.y));
    if (walkMode) {
        state.renderer.DrawTopDownOverlay(state.walkPreviewTex, 0.75f);
    }
    const GLuint tex = state.renderer.EndTopDownScene();

    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), imageSize,
                 ImVec2(0, 1), ImVec2(1, 0)); // FBO-Textur ist vertikal gespiegelt -> UVs tauschen

    if (objectMode) {
        // Marker für alle platzierten Objekte einzeichnen (gelb = ausgewählt, rot = übrige).
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
            const auto& obj = state.placementSet.At(i);
            const float ou = spanX > 0.0f ? obj.posX / spanX : 0.0f;
            const float ov = spanZ > 0.0f ? obj.posZ / spanZ : 0.0f;
            const ImVec2 screenPos(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
            const bool selected = state.selectedObject == static_cast<int>(i);
            drawList->AddCircleFilled(screenPos, selected ? 5.0f : 3.5f,
                                       selected ? IM_COL32(255, 220, 0, 255) : IM_COL32(255, 60, 60, 220));
        }
    }

    const bool hovered = ImGui::IsItemHovered();
    const bool clickTrigger = objectMode ? ImGui::IsMouseClicked(ImGuiMouseButton_Left) : ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (hovered && clickTrigger && imageSize.x > 0 && imageSize.y > 0) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const float u = (mouse.x - cursorScreenPos.x) / imageSize.x;
        const float v = (mouse.y - cursorScreenPos.y) / imageSize.y;

        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
            // Weltraum-Ausdehnung immer aus der Heightmap ableiten (alle Gitter decken dieselbe
            // physische Kartenfläche ab, nur mit unterschiedlicher Auflösung) - so bleibt die
            // Maus-Zuordnung über alle Bearbeitungsmodi hinweg konsistent.
            const float worldX = u * spanX;
            const float worldZ = v * spanZ;

            if (objectMode) {
                if (state.objectPlaceMode) {
                    core::PlacedObject obj;
                    obj.modelPath = state.newObjectModelPath;
                    obj.posX = worldX;
                    obj.posZ = worldZ;
                    obj.posY = state.heightmap.SampleWorld(worldX, worldZ);
                    const float rad = state.newObjectRotDeg * 3.14159265f / 180.0f / 2.0f;
                    obj.rotY = std::sin(rad);
                    obj.rotW = std::cos(rad);
                    obj.scale = state.newObjectScale;
                    state.selectedObject = static_cast<int>(state.placementSet.AddObject(std::move(obj)));
                } else {
                    // Nächstes Objekt innerhalb einer kleinen Toleranz suchen und auswählen.
                    const float tolerance = spanX * 0.02f;
                    float bestDist = tolerance;
                    int bestIdx = -1;
                    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
                        const auto& obj = state.placementSet.At(i);
                        const float dx = obj.posX - worldX;
                        const float dz = obj.posZ - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestIdx = static_cast<int>(i);
                        }
                    }
                    state.selectedObject = bestIdx;
                }
            } else if (!texMode && !walkMode) {
                core::UndoPatch patch = core::ApplyBrush(state.heightmap, state.brushMode, state.brush, worldX, worldZ);
                if (!patch.entries.empty()) {
                    state.undo.Push(std::move(patch));
                    state.meshDirty = true;
                }
            } else if (texMode) {
                // Eigene Zellgröße des Textur-Gitters (unabhängige Auflösung, siehe
                // docs/MAP_FORMAT.md) statt der Heightmap-Blockgröße - sonst wäre der Pinsel bei
                // z.B. 512x512-Textur-Layern auf einer 257x257-Heightmap deutlich daneben.
                const float texCellW = spanX / static_cast<float>(state.textureStack.Width());
                const float texCellH = spanZ / static_cast<float>(state.textureStack.Height());
                core::TexturePaintPatch patch = core::PaintLayerWeight(
                    state.textureStack, static_cast<std::size_t>(state.selectedLayer), state.paintMode,
                    state.paintSettings, worldX, worldZ, texCellW, texCellH);
                if (!patch.entries.empty()) {
                    state.textureUndo.Push(std::move(patch));
                    state.layerPreviewDirty = true;
                    state.renderer.UpdateBlendTextures(state.textureStack);
                }
            } else {
                // Eigene Zellgröße des Walk-Gitters (feinere Auflösung als die Heightmap) statt
                // der Heightmap-Blockgröße - sonst würde der Pinsel um Faktor ~2 daneben liegen.
                const float walkCellW = spanX / static_cast<float>(state.walkGrid.Width());
                const float walkCellH = spanZ / static_cast<float>(state.walkGrid.Height());
                core::WalkUndoPatch patch = core::ApplyWalkStamp(
                    state.walkGrid, state.walkSettings, worldX, worldZ, walkCellW, walkCellH);
                if (!patch.entries.empty()) {
                    state.walkUndo.Push(std::move(patch));
                    state.walkPreviewDirty = true;
                }
            }
        }
    }
}

void DrawPreview3DContent(EditorState& state) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int w = std::max(1, static_cast<int>(avail.x));
    const int h = std::max(1, static_cast<int>(avail.y));

    if (state.meshDirty) {
        state.renderer.RebuildMesh(state.heightmap);
        UpdatePreviewTexture(state);
        state.meshDirty = false;
    }

    const ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();
    state.renderer.BeginScene(state.camera, w, h, state.wireframe);
    // Immer neu aufbauen (statt Dirty-Tracking über alle Objekt-Mutationsstellen hinweg) -
    // bei ein paar Tausend Instanzen unproblematisch, aber garantiert nie veraltet (z.B. nach
    // Auswahl-Wechsel für den Highlight-Farbton). Platzhalter-Pyramide wird für Objekte
    // übersprungen, für die bereits ein echtes Mesh geladen werden konnte (siehe
    // NifMeshRenderer) - sonst doppelte Darstellung.
    state.objectMarkerRenderer.RebuildInstances(state.placementSet, state.selectedObject,
        [&state](std::size_t i) { return state.nifMeshRenderer.HasRealMesh(i); });
    state.objectMarkerRenderer.Draw(state.camera, w, h);
    state.nifMeshRenderer.Draw(state.placementSet, state.camera, w, h);
    const GLuint tex = state.renderer.EndScene();
    if (tex != 0) {
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), ImVec2(static_cast<float>(w), static_cast<float>(h)),
                     ImVec2(0, 1), ImVec2(1, 0)); // FBO-Textur ist vertikal gespiegelt -> UVs tauschen
    }

    if (ImGui::IsItemHovered()) {
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const ImVec2 delta = io.MouseDelta;
            state.camera.OrbitBy(delta.x * 0.01f, -delta.y * 0.01f);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            // Rechte Maustaste = Kamera verschieben (Pan) - macht die Kamera tatsächlich
            // beweglich statt nur um einen festen Punkt zu rotieren. Skaliert mit der aktuellen
            // Entfernung, damit das Schwenktempo bei jedem Zoom-Level stimmig wirkt.
            const ImVec2 delta = io.MouseDelta;
            const float panScale = state.camera.Distance() * 0.0015f;
            state.camera.PanBy(-delta.x * panScale, delta.y * panScale);
        }
        if (io.MouseWheel != 0.0f) {
            state.camera.Zoom(-io.MouseWheel * 200.0f);
        }
    }

    // Zoom +/- Knöpfe unten rechts über dem 3D-Bild (siehe Mockup) - zusätzlich zum
    // Mausrad, für Nutzer ohne Maus mit Rad bzw. als deutlicher sichtbarer Zugriffspunkt.
    const ImVec2 zoomBtnSize(26.0f, 26.0f);
    const ImVec2 zoomPos(imageScreenPos.x + avail.x - zoomBtnSize.x - 10.0f, imageScreenPos.y + avail.y - zoomBtnSize.y * 2.0f - 16.0f);
    ImGui::SetCursorScreenPos(zoomPos);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(28, 34, 42, 230));
    if (ImGui::Button("+##zoomIn", zoomBtnSize)) state.camera.Zoom(-150.0f);
    ImGui::SetCursorScreenPos(ImVec2(zoomPos.x, zoomPos.y + zoomBtnSize.y + 4.0f));
    if (ImGui::Button("-##zoomOut", zoomBtnSize)) state.camera.Zoom(150.0f);
    ImGui::PopStyleColor();
}

// Tab-Leiste des Arbeitsbereichs (Hightmap/Texturing/Block-Walk/Objects/NPCs/NPC AI/Mobs/
// Mob AI + Zurück, siehe Mockup) - steuert denselben EditMode, der früher über Radio-Buttons
// im Werkzeuge-Panel gewählt wurde.
void DrawWorkspaceTabBar(EditorState& state) {
    struct TabDef { const char* key; EditMode mode; };
    const TabDef tabs[] = {
        {"workspace.tab.heightmap", EditMode::Heightmap},
        {"workspace.tab.texturing", EditMode::TexturePaint},
        {"workspace.tab.blockwalk", EditMode::BlockWalk},
        {"workspace.tab.objects", EditMode::ObjectPlacement},
        {"workspace.tab.npcs", EditMode::Npcs},
        {"workspace.tab.npcai", EditMode::NpcAi},
        {"workspace.tab.mobs", EditMode::Mobs},
        {"workspace.tab.mobai", EditMode::MobAi},
    };
    for (const auto& tab : tabs) {
        const bool active = state.editMode == tab.mode;
        ImGui::PushStyleColor(ImGuiCol_Button, active ? IM_COL32(55, 125, 195, 255) : IM_COL32(48, 56, 68, 255));
        if (ImGui::Button(T(tab.key)) && !active) {
            state.editMode = tab.mode;
            state.layerPreviewDirty = true;
            state.walkPreviewDirty = true;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    // Lücke, damit "Zurück" wie im Mockup rechts absetzt statt direkt anzuschließen.
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 100.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    if (ImGui::Button(T("nav.back"), ImVec2(90.0f, 0.0f))) {
        state.screen = AppScreen::MapEditorLauncher;
    }
    ImGui::PopStyleColor();
}

// Der eigentliche Arbeitsbereich (siehe Mockup, zweites/rechtes Bild): Tab-Leiste oben,
// darunter drei Spalten - "Datei"+"Tools/etc" links, "2D View" Mitte, "3D View" rechts.
void DrawMapEditorWorkspace(EditorState& state) {
    DrawWorkspaceTabBar(state);
    ImGui::Separator();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float leftW = 300.0f;
    const float gap = 6.0f;
    const float rightAndMiddleW = avail.x - leftW - gap * 2.0f;
    const float viewW = rightAndMiddleW * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(22, 29, 38, 255));
    ImGui::BeginChild("##fileToolsCol", ImVec2(leftW, avail.y), true);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", T("workspace.file"));
    ImGui::Separator();
    ImGui::BeginDisabled(!state.hasLegacyIniMeta && state.legacySaveDir[0] == '\0');
    if (ImGui::Button(T("workspace.save"), ImVec2(-1.0f, 0.0f))) {
        auto project = BuildProjectFromState(state);
        auto result = core::legacy::SaveLegacyMap(project, state.legacySaveDir, state.legacySaveStem);
        if (result) {
            state.legacyIniMeta = project.ini;
            state.statusMessage = std::string(T("workspace.savedas")) + state.legacySaveDir;
        } else {
            state.statusMessage = "Fehler: " + result.error();
        }
    }
    ImGui::EndDisabled();
    if (ImGui::Button(T("workspace.saveas"), ImVec2(-1.0f, 0.0f))) {
        ImGui::OpenPopup("##saveAsPopup");
    }
    if (ImGui::BeginPopup("##saveAsPopup")) {
        ImGui::InputText("Ordner", state.legacySaveDir, sizeof(state.legacySaveDir));
        ImGui::InputText("Name", state.legacySaveStem, sizeof(state.legacySaveStem));
        if (ImGui::Button(T("workspace.save"))) {
            auto project = BuildProjectFromState(state);
            auto result = core::legacy::SaveLegacyMap(project, state.legacySaveDir, state.legacySaveStem);
            state.statusMessage = result ? std::string(T("workspace.savedas")) + state.legacySaveDir
                                          : "Fehler: " + result.error();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::BeginDisabled(!state.undo.CanUndo());
    if (ImGui::Button(T("workspace.undo"), ImVec2((leftW - 32.0f) * 0.5f, 0.0f))) {
        if (state.undo.Undo(state.heightmap)) state.meshDirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.undo.CanRedo());
    if (ImGui::Button(T("workspace.redo"), ImVec2((leftW - 32.0f) * 0.5f, 0.0f))) {
        if (state.undo.Redo(state.heightmap)) state.meshDirty = true;
    }
    ImGui::EndDisabled();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", T("workspace.tools"));
    ImGui::Separator();
    DrawToolsContent(state);

    if (ImGui::CollapsingHeader("Erweitert (natives Format / Legacy Import-Export)")) {
        DrawAdvancedFileOps(state);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, gap);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 48, 68, 255));
    ImGui::BeginChild("##view2d", ImVec2(viewW, avail.y), true);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", T("workspace.2dview"));
    ImGui::Separator();
    DrawEditor2DContent(state);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, gap);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(30, 48, 68, 255));
    ImGui::BeginChild("##view3d", ImVec2(viewW, avail.y), true);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", T("workspace.3dview"));
    ImGui::Separator();
    DrawPreview3DContent(state);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (!state.statusMessage.empty()) {
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }
}

} // namespace

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

    GLFWwindow* window = glfwCreateWindow(1600, 900, "TheSeed Map-Editor - Heightmap + Texturing", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "glfwCreateWindow fehlgeschlagen\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
#ifdef _WIN32
    g_appWindowForDialogs = window;
#endif

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "gladLoadGLLoader fehlgeschlagen\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ApplyEditorTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    EditorState state;
    state.renderer.Init();
    state.objectMarkerRenderer.Init();
    state.nifMeshRenderer.Init();
    UpdatePreviewTexture(state);
    state.selectedLayer = static_cast<int>(state.textureStack.AddLayer("Base", "base.dds", 1.0f));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

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

        switch (state.screen) {
            case AppScreen::ProjectHub: DrawProjectHub(state); break;
            case AppScreen::NewProjectConfig: DrawNewProjectConfig(state); break;
            case AppScreen::MapEditorLauncher: DrawMapEditorLauncher(state); break;
            case AppScreen::MapEditorWorkspace: DrawMapEditorWorkspace(state); break;
            case AppScreen::ShnEditor: DrawShnEditor(state); break;
            case AppScreen::ComingSoon: DrawComingSoon(state); break;
        }

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::Render();
        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.025f, 0.035f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    state.renderer.Shutdown();
    state.objectMarkerRenderer.Shutdown();
    state.nifMeshRenderer.Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
