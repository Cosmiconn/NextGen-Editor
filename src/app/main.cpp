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

// Für DockBuilder* (programmatisches Default-Layout des Map-Editor-DockSpace, siehe
// DrawMapEditorWorkspace / CHANGELOG [0.44.12]) muss IMGUI_DEFINE_MATH_OPERATORS schon VOR
// imgui.h selbst gesetzt sein.
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
// imgui_internal.h ist kein Teil der öffentlichen imgui.h-API, aber in Docking-Branch-Apps
// der übliche, akzeptierte Weg für DockBuilder*.
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"

#include "mapeditor/core/AvatarPreview.hpp"
#include "mapeditor/core/DdsImage.hpp"
#include "mapeditor/core/Manual.hpp"
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
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"
#include "mapeditor/core/legacy/LegacyTextureSetIO.hpp"
#include "mapeditor/core/legacy/ShnFile.hpp"
#include "mapeditor/core/legacy/ShineText.hpp"
#include "mapeditor/core/legacy/QuestData.hpp"
#include "mapeditor/app/Localization.hpp"

#include "Camera.hpp"
#include "KfmPanel.hpp"
#include "ObjectMarkerRenderer.hpp"
#include "NifMeshRenderer.hpp"
#include "Renderer.hpp"

#include <algorithm>
#include <array>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <deque>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <set>
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
    style.WindowRounding = 5.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.FramePadding = ImVec2(9.0f, 6.0f);
    style.WindowPadding = ImVec2(10.0f, 9.0f);
    style.ScrollbarSize = 13.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                 = ImVec4(0.91f, 0.94f, 0.98f, 1.0f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.48f, 0.54f, 0.62f, 1.0f);
    c[ImGuiCol_WindowBg]             = ImVec4(0.025f, 0.035f, 0.050f, 1.0f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.040f, 0.055f, 0.075f, 1.0f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.045f, 0.060f, 0.082f, 0.99f);
    c[ImGuiCol_Border]               = ImVec4(0.12f, 0.18f, 0.25f, 1.0f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = ImVec4(0.070f, 0.095f, 0.130f, 1.0f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.090f, 0.150f, 0.215f, 1.0f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.105f, 0.195f, 0.285f, 1.0f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.018f, 0.028f, 0.042f, 1.0f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.030f, 0.070f, 0.110f, 1.0f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.026f, 0.038f, 0.055f, 1.0f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.020f, 0.027f, 0.038f, 1.0f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.15f, 0.21f, 0.29f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.21f, 0.32f, 0.43f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.20f, 0.45f, 0.68f, 1.0f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.18f, 0.68f, 1.00f, 1.0f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.18f, 0.60f, 0.92f, 1.0f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.25f, 0.76f, 1.00f, 1.0f);
    c[ImGuiCol_Button]               = ImVec4(0.060f, 0.095f, 0.135f, 1.0f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.080f, 0.190f, 0.300f, 1.0f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.070f, 0.280f, 0.455f, 1.0f);
    c[ImGuiCol_Header]               = ImVec4(0.060f, 0.140f, 0.220f, 1.0f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.080f, 0.220f, 0.350f, 1.0f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.075f, 0.300f, 0.490f, 1.0f);
    c[ImGuiCol_Separator]            = ImVec4(0.12f, 0.18f, 0.25f, 1.0f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.18f, 0.46f, 0.68f, 1.0f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.22f, 0.62f, 0.90f, 1.0f);
    c[ImGuiCol_Tab]                  = ImVec4(0.040f, 0.075f, 0.110f, 1.0f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.075f, 0.200f, 0.320f, 1.0f);
    c[ImGuiCol_TabActive]            = ImVec4(0.060f, 0.240f, 0.390f, 1.0f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.030f, 0.052f, 0.075f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.050f, 0.140f, 0.220f, 1.0f);
    c[ImGuiCol_DockingPreview]       = ImVec4(0.10f, 0.55f, 0.90f, 0.70f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.15f, 0.62f, 0.98f, 0.85f);
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

// ---------------------------------------------------------------------------
// UI::  - duenne Huelle um die ImGui-Eingabeelemente: rufen das Element auf und zeigen danach (nach kurzer
// Verzoegerung) den Tooltip aus dem Handbuch-Datenbestand (core::manual::TooltipFor) zur Beschriftung.
// Beschriftungen aus T("...") werden ueber die Uebersetzungstabelle auf ihren Schluessel zurueckgefuehrt
// ("T:<schluessel>"). Fehlt ein Tooltip, passiert nichts.
// ---------------------------------------------------------------------------
namespace UI {
inline void Tip(const char* kind, const char* label) {
    if (label == nullptr || !ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) return;
    const bool german = app::CurrentLanguage() == app::Language::German;
    const char* text = core::manual::TooltipFor(kind, label, german);
    if (text == nullptr) {
        static const std::unordered_map<std::string, std::string> reverse = [] {
            std::unordered_map<std::string, std::string> r;
            for (const auto& [key, pair] : app::detail::TranslationTable()) { r.emplace(pair.first, key); r.emplace(pair.second, key); }
            return r;
        }();
        if (const auto it = reverse.find(label); it != reverse.end()) text = core::manual::TooltipFor(kind, "T:" + it->second, german);
    }
    if (text != nullptr) ImGui::SetTooltip("%s", text);
}
template <class... A> bool Button(const char* l, A&&... a) { const bool r = ImGui::Button(l, std::forward<A>(a)...); Tip("Button", l); return r; }
template <class... A> bool SmallButton(const char* l, A&&... a) { const bool r = ImGui::SmallButton(l, std::forward<A>(a)...); Tip("SmallButton", l); return r; }
template <class... A> bool Checkbox(const char* l, A&&... a) { const bool r = ImGui::Checkbox(l, std::forward<A>(a)...); Tip("Checkbox", l); return r; }
template <class... A> bool RadioButton(const char* l, A&&... a) { const bool r = ImGui::RadioButton(l, std::forward<A>(a)...); Tip("RadioButton", l); return r; }
template <class... A> bool SliderFloat(const char* l, A&&... a) { const bool r = ImGui::SliderFloat(l, std::forward<A>(a)...); Tip("SliderFloat", l); return r; }
template <class... A> bool SliderInt(const char* l, A&&... a) { const bool r = ImGui::SliderInt(l, std::forward<A>(a)...); Tip("SliderInt", l); return r; }
template <class... A> bool InputInt(const char* l, A&&... a) { const bool r = ImGui::InputInt(l, std::forward<A>(a)...); Tip("InputInt", l); return r; }
template <class... A> bool InputFloat(const char* l, A&&... a) { const bool r = ImGui::InputFloat(l, std::forward<A>(a)...); Tip("InputFloat", l); return r; }
template <class... A> bool InputText(const char* l, A&&... a) { const bool r = ImGui::InputText(l, std::forward<A>(a)...); Tip("InputText", l); return r; }
template <class... A> bool InputTextWithHint(const char* l, A&&... a) { const bool r = ImGui::InputTextWithHint(l, std::forward<A>(a)...); Tip("InputText", l); return r; }
template <class... A> bool Combo(const char* l, A&&... a) { const bool r = ImGui::Combo(l, std::forward<A>(a)...); Tip("Combo", l); return r; }
template <class... A> bool CollapsingHeader(const char* l, A&&... a) { const bool r = ImGui::CollapsingHeader(l, std::forward<A>(a)...); Tip("CollapsingHeader", l); return r; }
template <class... A> bool TabItemButton(const char* l, A&&... a) { const bool r = ImGui::TabItemButton(l, std::forward<A>(a)...); Tip("TabItemButton", l); return r; }
template <class... A> bool Selectable(const char* l, A&&... a) { const bool r = ImGui::Selectable(l, std::forward<A>(a)...); Tip("Selectable", l); return r; }
template <class... A> bool MenuItem(const char* l, A&&... a) { const bool r = ImGui::MenuItem(l, std::forward<A>(a)...); Tip("MenuItem", l); return r; }
} // namespace UI

enum class EditMode { Heightmap, TexturePaint, BlockWalk, ObjectPlacement, Npcs, NpcAi, Mobs, MobAi, Portals };

// Oberste Navigationsebene der neuen, an den Mockups orientierten Oberfläche (siehe
// HANDOFF.md / MAP_FORMAT.md für den Kontext). ComingSoon deckt die vier Editor-Karten ab,
// die im Mockup als "Noch nicht entschieden" markiert sind bzw. noch nicht gebaut wurden
// (Quest/Interface/Drop Table/Skill+Action) sowie den SHN Editor.
enum class AppScreen { ProjectHub, NewProjectConfig, MapEditorLauncher, MapEditorWorkspace, ShnEditor, KfmBrowser, ComingSoon };

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
    app::KfmPanel kfmPanel;
    core::Heightmap heightmap{257, 257, 50.0f, 50.0f};
    core::UndoStack undo;
    core::BrushMode brushMode = core::BrushMode::Raise;
    core::BrushSettings brush;

    core::TextureLayerStack textureStack{1024, 1024}; // feinere Malauflösung; unabhängig von Heightmap
    core::TexturePaintUndoStack textureUndo;
    int selectedLayer = -1;
    int layerRenameIndex = -1;
    char layerRenameBuffer[128] = "";
    core::PaintMode paintMode = core::PaintMode::Increase;
    core::TexturePaintSettings paintSettings;
    int textureResolutionWidth = 2048;
    int textureResolutionHeight = 2048;

    EditMode editMode = EditMode::Heightmap;

    core::WalkGrid walkGrid{512, 512};
    core::WalkUndoStack walkUndo;
    core::WalkStampSettings walkSettings;
    GLuint walkPreviewTex = 0;
    std::uint32_t walkPreviewCols = 0, walkPreviewRows = 0; // Zell-Aufloesung der Vorschau-Textur
    bool walkBlockMode = true;   // Stempel sperrt (true) oder gibt frei (false)
    bool walkRectActive = false;
    ImVec2 walkRectStart{};
    bool walkPreviewDirty = true;
    char walkLegacyPath[512] = "";
    int walkLegacyWidth = 512;
    int walkLegacyHeight = 512;

    core::ObjectPlacementSet placementSet;
    // Auswahl-IDs: >=0 = normales Placement, -1 = keine Auswahl, <=-2 = SHMD-
    // Szenenmodell (Index = -2-id). So kann dieselbe Mehrfachauswahl beide Welten enthalten.
    int selectedObject = -1;
    std::vector<int> selectedObjects;
    bool objectDragActive = false;
    bool objectBoxSelectActive = false;
    ImVec2 objectBoxSelectStart{};
    char selectedObjectModelPath[512] = "";
    int selectedObjectModelPathFor = -1;
    bool selectedObjectModelPathDirty = false;
    int objectListRangeAnchor = -1; // Shift-Bereichsauswahl in der Objektliste
    int objectPlaceMode = 1; // 1 = Platzieren, 0 = Auswählen (ImGui::RadioButton braucht int*)

    // Level-Editor Transform-Werkzeuge (0=Move, 1=Rotate, 2=Scale).
    int objectGizmoOperation = 0;
    bool objectGizmoLocal = false;
    bool objectGizmoSnap = true;
    float objectMoveSnap = 50.0f;
    float objectRotateSnap = 15.0f;
    float objectScaleSnap = 0.10f;
    std::array<float, 16> objectGizmoMatrix{};
    bool objectGizmoMatrixValid = false;
    bool objectGizmoWasUsing = false;
    std::string objectGizmoSelectionKey;
    std::vector<core::PlacedObject> objectClipboard;
    std::vector<char> objectEditorHidden; // nur Editor-Sichtbarkeit, nicht SHMD-Export
    std::vector<char> objectEditorLocked; // nur Editor-Lock, nicht SHMD-Export
    std::unordered_set<std::string> shmdEditorHiddenKeys;
    std::unordered_set<std::string> shmdEditorLockedKeys;
    char newObjectModelPath[512] = "resmap\\field\\Rou\\GuildHall.nif";
    float newObjectScale = 1.0f;
    float newObjectRotDeg = 0.0f;
    char legacyShmdPath[512] = "";
    char legacyIdmPath[512] = "";
    char legacyAidPath[512] = "";
    core::ObjectSpatialIndex legacySpatialIndex;
    bool hasLegacySpatialIndex = false;
    core::legacy::ZoneMetadata legacyZoneMetadata;
    std::vector<core::legacy::PreservedMapFile> preservedMapFiles;
    bool hasLegacyZoneMetadata = false;
    int selectedZone = 0;
    char zoneNameBuf[256] = "";

    app::OrbitCamera camera;
    bool cameraLooking = false;       // rechte Maustaste im 3D-View gedrueckt (Umsehen + WASD)
    // 2D-Ansicht: Zoom (1 = ganze Karte) und Mittelpunkt des sichtbaren Ausschnitts in Kartenkoordinaten
    // (u = x/spanX, v = Bildzeile, Norden oben, siehe DrawEditor2DContent).
    float view2dZoom = 1.0f;
    float view2dCenterU = 0.5f, view2dCenterV = 0.5f;
    app::HeightmapRenderer renderer;
    app::ObjectMarkerRenderer objectMarkerRenderer;
    app::ObjectMarkerRenderer portalMarkerRenderer; // Portale-Tab: TownPortal-/Schriftrollen-Ziele im 3D-View
    app::NifMeshRenderer nifMeshRenderer;
    // SHMD enthält vor den normalen Placement-Instanzen eigenständige Modelllisten
    // (Sky/Water/GroundObject) OHNE Transform. Diese Modelle dürfen nicht in placementSet
    // eingefügt werden, weil sie sonst beim Export fälschlich als normale Instanzen
    // serialisiert würden. Darum eigener synthetischer Render-Set + Renderer.
    app::NifMeshRenderer shmdCategoryMeshRenderer;
    core::ObjectPlacementSet shmdCategoryRenderSet;
    std::vector<int> shmdCategoryRenderKind; // 0=Sky, 1=Water, 2=GroundObject, -1=sonstige Kategorie
    // Je Render-Eintrag: (Index in placementSet.categories, Index in modelPaths).
    // Wird gebraucht, um dieselben Einträge im Objekt-Editor direkt bearbeiten zu können.
    std::vector<std::pair<std::size_t, std::size_t>> shmdCategorySource;
    std::vector<char> shmdCategoryHidden;
    // Eigenständiger, zweiter NifMeshRenderer NUR für die 3D-Darstellung von NPCs (siehe
    // CHANGELOG [0.44.17]) - getrennt von nifMeshRenderer/placementSet, da LoadModelsForSet
    // pro Aufruf immer sein GESAMTES internes Modell-Set neu aufbaut (ein gemeinsamer
    // Renderer für Objekte UND NPCs würde sich bei jedem Neuladen gegenseitig überschreiben).
    app::NifMeshRenderer npcMeshRenderer;
    // NPC-Ausrichtung im 3D-View (Richtung aus NPC.txt -> Blickrichtung des Modells). Die Zuordnung ist NICHT
    // aus Daten belegt (kein Ground-Truth fuer "wohin schaut ein NPC") - deshalb einstellbar und mit
    // Blickpfeil in 2D/3D sichtbar: Winkel = Vorzeichen * Richtung + Versatz.
    // Default per Block&Walk-Analyse ermittelt (CHANGELOG [0.44.34]): 270 NPCs auf 28 Karten,
    // Blickrichtung mit "steht vorne offen/hinten an der Wand" abgeglichen - sign=-1/offset=180
    // (bisheriger Default) lag beim Vorzeichen falsch, sign=+1/offset=0 passt deutlich besser
    // (Feinsuche fand ein noch etwas besseres Optimum um -20 Grad, aber 0 ist die einfachere,
    // nicht ueberangepasste Wahl). Weiterhin NICHT am echten Spiel verifiziert - "Versatz schaetzen"
    // unten wiederholt dieselbe Analyse live fuer die gerade offene Karte.
    int npcDirSign = 1;
    int npcDirOffsetDeg = 0;
    std::string npcOrientEstimateStatus;
    std::vector<std::size_t> npcRenderRecordIdx; // je npcRenderSet-Objekt der Datensatz in NPC.txt
    bool showNpcLabels = true;
    int npcModelsMissing = 0;                 // NPCs der Karte ohne auffindbares Modell
    std::vector<std::string> npcModelsMissingNames;
    core::ObjectPlacementSet npcRenderSet; // synthetisch aus World/NPC.txt aufgebaut, nur zum Rendern
    std::string npcRenderSetForMap; // welche Karte gerade in npcRenderSet steckt (Cache-Invalidierung)

    GLuint previewTex = 0;      // Graustufen-Vorschau Heightmap
    GLuint layerPreviewTex = 0; // Graustufen-Vorschau ausgewählter Textur-Layer
    bool meshDirty = true;
    bool layerPreviewDirty = true;
    bool wireframe = false;
    // Sichtbarkeit (Map-Editor, Bereich "Sichtbarkeit"): alles einzeln ein-/ausblendbar.
    bool showTerrain = true;
    bool showObjectMeshes = true;    // echte Objekt-Modelle
    bool showObjectMarkers = true;   // Platzhalter-Pyramiden fuer Objekte ohne ladbares Modell
    bool showShmdSky = true;          // SHMD-Kategorie "Sky"
    bool showShmdWater = true;        // SHMD-Kategorie "Water"
    bool showShmdGroundObject = true; // SHMD-Kategorie "GroundObject"
    bool showNpcModels = true;
    bool showObjects2D = true;       // Objektpunkte/Grundflaechen im 2D-View
    bool categoryVisible[10] = {true, true, true, true, true, true, true, true, true, true};
    std::vector<char> layerHidden;   // je Terrain-Layer: 1 = ausgeblendet
    std::vector<int> objectCategory; // je Objekt (Index in placementSet)
    std::vector<char> objectHidden;  // je Objekt: 1 = wegen Kategorie ausgeblendet
    std::string objectVisKey;
    std::unordered_map<std::string, int> modelCategoryCache;

    // Import-Dialog-Zustand (Legacy-Format beschreibt seine Dimensionen NICHT selbst).
    char legacyPath[512] = "";
    int legacyWidth = 257;
    int legacyHeight = 257;
    float legacyBlockWidth = 50.0f;
    float legacyBlockHeight = 50.0f;


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

    // NPC-Platzierung/Mob-Spawn (World/NPC.txt, MobRegen/<Karte>.txt aus Server/9Data/Shine) -
    // siehe CHANGELOG [0.44.15]. Eigener Root, unabhängig vom SHN-Editor-Server-Ordner (dort
    // liegen nur die .shn-Dateien direkt, diese Text-Dateien in Unterordnern desselben Baums).
    std::string shineTextRoot;
    core::legacy::ShineTextFile npcTextFile;
    bool npcTextLoaded = false;
    int selectedNpcRecordIdx = -1; // Index in npcTextFile-Tabelle "ShineNPC" (nicht in der gefilterten Liste)

    core::legacy::ShineTextFile mobRegenTextFile;
    bool mobRegenTextLoaded = false;
    std::string mobRegenLoadedForMap; // welche Karte gerade geladen ist
    int selectedMobZoneIdx = -1; // Index in Tabelle "MobRegenGroup"
    char newMobSpawnName[64] = ""; // Eingabe fuer "+ Monster hinzufuegen"
    bool mobDeleteArmed = false;   // Sicherung: Loeschen von Zonen/Monstern erst nach Haken

    // Händler-Inventar (NPCItemList/<NPC>.txt) - nur geladen, wenn der Popup dafür offen ist.
    core::legacy::ShineTextFile shopTextFile;
    bool shopTextLoaded = false;
    std::string shopLoadedForNpc;
    bool shopEditorOpen = false;
    // Shop-Editor (NPCItemList/<NPC>.txt): Item-Auswahl, Auswahlzustand, Sicherung fuers Loeschen.
    struct ShopItemEntry { std::string inx; std::string name; long long id = 0; };
    std::vector<ShopItemEntry> itemEntries;                  // aus ItemInfo.shn, nach InxName sortiert
    std::unordered_map<std::string, std::size_t> itemByInx;  // InxName -> Index in itemEntries
    bool itemLookupTried = false;
    int shopPickTable = -1, shopPickRow = -1, shopPickCol = -1;
    bool shopPickRequested = false;
    char shopItemFilter[128] = "";
    bool shopDeleteArmed = false;
    bool shopFileIsNew = false;

    // Custom NPC / Mob Assistent (SHN-Editor, Tab "Custom NPC/Mob"), siehe DrawCustomCreatureEditor.
    // Avatar-Vorschau (Custom NPC, Modus "Spieler-Avatar"): Ordner reschar/resitem, gecachte Item-Daten.
    std::string charRoot, itemRoot;
    struct ItemViewData { std::string textureFile, linkFile; int mSet = 0, fSet = 0; };
    std::unordered_map<std::string, ItemViewData> itemViewByInx;
    bool itemViewBuilt = false;
    std::shared_ptr<const core::AvatarModel> avatarModel;
    std::string avatarKey;
    std::string avatarError;
    GLuint avatarTex = 0;
    float avatarYaw = 0.35f;
    float avatarRenderedYaw = 1e9f;
    // Handbuch (F1 / Knopf unten rechts), siehe DrawManualWindow.
    bool manualOpen = false;
    char manualQuery[128] = "";
    std::string manualSection = "start.overview";   // gewaehlter Abschnitt (oder "reference.columns")
    int manualColumnDoc = 0;                        // gewaehlte Tabelle in der Spalten-Referenz
    char manualColumnFilter[64] = "";
    // Skill-Editor (SHN-Editor, Tab "Skill Editor"), siehe DrawSkillEditor.
    struct SkillEditorState {
        long long selectedId = -1;
        char filter[64] = "";
        std::string listKey;
        std::vector<std::size_t> visible;          // Zeilen in ActiveSkill (Client)
        std::unordered_map<std::string, std::vector<std::string>> pickerOptions; // Spalte -> Werte (nach Haeufigkeit)
        std::unordered_map<std::string, std::vector<std::string>> pickerLabels;
        std::string pickerBuiltKey;
        char pickFilter[128] = "";
        char newInx[40] = "";
        char newName[64] = "";
        bool makeBook = true;
        bool rawOpen = false;
        int scalePercent = 100;
        bool scaleDamage = true, scaleCost = false, scaleCooldown = false, scaleCast = false;
        std::vector<std::string> report;
    } skill;
    struct CreatureWizard {
        bool isNpc = true;
        char templateFilter[64] = "";
        long long templateId = -1;
        std::string templateInx;
        char newInx[40] = "";
        char displayName[64] = "";
        bool autoId = true;
        int manualId = 0;
        int level = 1, maxHp = 100, walkSpeed = 50, runSpeed = 100, size = 1000;
        int lookMode = 0;            // 0 = wie Vorlage, 1 = anderes Modell, 2 = Spieler-Avatar mit Ausruestung (nur NPC)
        char modelFile[64] = "";
        char modelFilter[64] = "";
        int avClass = 0, avGender = 1, avFace = 0, avHairType = 1, avHairColor = 0;
        std::array<std::string, 19> equ;
        int pickSlot = -1;
        bool pickRequested = false;
        char itemFilter[128] = "";
        bool copyDialog = true;
        // Platzierung in NPC.txt (nur NPC)
        bool placeOnMap = false;
        int placeX = 0, placeY = 0, placeDir = 0;
        int roleIdx = 0;             // Index in kNpcRoles
        char roleArg[32] = "Quest";
        std::vector<std::string> report;
    } wiz;

    // NPC-Dialog (NpcDialogData.shn, Client-seitig unter ressystem/) - siehe CHANGELOG
    // [0.44.16]. Eigenes Feld für den Client-ressystem-Ordner, unabhängig vom SHN-Editor-
    // Client-Root (dort wird i.d.R. derselbe Ordner erneut gesucht, aber der Nutzer könnte
    // die beiden Editoren in unterschiedlicher Reihenfolge/Sitzung nutzen).
    std::string npcDialogRessystemRoot;
    // "reschar"-Ordner (Client, Geschwister von "resmap"/"ressystem") - enthält PRO NPC/
    // Charakter einen eigenen Unterordner mit .nif+.kf(m) (z.B. "reschar/AdlSmithAlexia/
    // AdlSmithAlexia.nif" + Animations-Dateien) - EIGENSTÄNDIGER Ordner, NICHT Teil von
    // resmap/nif(s) (siehe CHANGELOG [0.44.18], vom Nutzer per Screenshot bestätigt). Für die
    // NPC-3D-Auflösung genutzt statt state.availableNifFiles (das deckt nur resmap/nif(s) ab).
    std::string rescharRoot;
    core::legacy::ShnFile npcDialogShn;
    bool npcDialogLoaded = false;
    bool dialogEditorOpen = false;
    int dialogEditorRowIdx = -1; // Index in npcDialogShn.rows
    std::string dialogGreetingBuf;
    std::vector<std::pair<std::string, std::string>> dialogButtonsBuf; // Label, Aktion

    // MobViewInfo.shn (Client, ressystem/) - InxName->FileName-Auflösung für die 3D-Darstellung
    // von NPCs, siehe CHANGELOG [0.44.17]. Nutzt denselben ressystem-Root wie NpcDialogData.shn.
    core::legacy::ShnFile mobViewInfoShn;
    bool mobViewInfoLoaded = false;

    // Quest-Editor (QuestData.shn, Server-seitig + QuestDialog.shn zur Textauflösung,
    // Client-seitig) - siehe CHANGELOG [0.44.19]. Eigenes Format (kein ShnFile/ShineText),
    // portiert aus einem vom Nutzer bereitgestellten, gegen echte Daten verifizierten
    // Referenzparser.
    core::legacy::QuestDataFile questDataFile;
    bool questDataLoaded = false;
    core::legacy::ShnFile questDialogShn;
    bool questDialogLoaded = false;
    int selectedQuestIdx = -1;
    // Quest-Editor-Caches: Text-ID -> Text (statt Linearsuche pro Aufruf), Listen-Beschriftungen und
    // sichtbare Eintraege (Filter) - der alte Editor durchsuchte QuestDialog fuer JEDE Quest in JEDEM
    // Frame (CHANGELOG [0.44.29]).
    std::unordered_map<int, std::string> questTextMap;
    bool questTextMapBuilt = false;
    std::vector<std::string> questLabels;
    std::string questListKey;
    std::vector<std::size_t> questVisible;
    std::uint64_t questRevision = 0;
    char questSearch[128] = "";

    // Für Item-/Mob-Namensauflösung im Quest-Editor (dient zugleich als einfache Validierung -
    // leerer Name = ID existiert nicht, siehe CHANGELOG [0.44.21]).
    core::legacy::ShnFile itemInfoShn;
    bool itemInfoLoaded = false;

    // Portal-Editor (TownPortal.shn + RecallCoord.txt) - siehe CHANGELOG [0.44.20].
    core::legacy::ShnFile townPortalShn;
    bool townPortalLoaded = false;

    // Portale-Tab im Map-Editor (EditMode::Portals), siehe CHANGELOG [0.44.25]: MapInfo.shn nur
    // als Referenz (RegenX/RegenY), Auswahl-/Klick-Zustand fuer TownPortal- und Schriftrollen-Ziele.
    core::legacy::ShnFile mapInfoShn;
    bool mapInfoLoaded = false;
    std::string portalDataSig;       // Ordner-Kombination, fuer die zuletzt geladen wurde
    int selectedPortalKind = 0;      // 0 = nichts, 1 = TownPortal, 2 = Schriftrolle (RecallCoord)
    int selectedPortalIdx = -1;      // Zeilen-/Record-Index in der jeweiligen Tabelle
    bool portalPickMode = false;     // Klick im 2D-View setzt die Position des gewaehlten Ziels

    // Automatisch aus den beim Anlegen des Projekts angegebenen Client-/Server-Ordnern
    // abgeleitete Unterordner (SyncProjectRoots) - Nutzer soll sie nicht mehr einzeln waehlen.
    // SHN-Grid: Zeilen-Sichtbarkeits-Cache (Filter) und Aenderungszaehler, siehe DrawShnGrid.
    std::uint64_t shnEditCounter = 0;
    std::string shnVisibleKey;
    std::vector<std::size_t> shnVisibleRows;

    std::string projectRootsKey;     // "<client>|<server>", fuer die zuletzt abgeleitet wurde
    bool shnAutoLoaded = false;      // SHN-Editor hat die abgeleiteten Ordner schon eingelesen

    // KI-Skript-Editor (Lua unter LuaScript/AIScript/<Name>.lua, PineScript unter
    // MobBehaviorDescript/) - reiner Text-Editor, keine Syntaxprüfung/Interpretation, siehe
    // CHANGELOG [0.44.22]. Pfad wird bei Bedarf direkt unter state.shnServerRoot gesucht.
    bool aiScriptEditorOpen = false;
    std::string aiScriptEditorPath;
    std::string aiScriptEditorText;
    std::string aiScriptEditorName;
    char aiScriptLookupBuf[128] = "";

    // Patrouillenrouten-Editor (MobRoam/<Name>.txt, Server) - siehe CHANGELOG [0.44.23].
    bool patrolEditorOpen = false;
    std::string patrolEditorName;
    core::legacy::ShineTextFile patrolRouteFile;
    bool patrolRouteLoaded = false;

    // Grundfläche (Bounding-Box in X/Z, aus den echten NIF-Vertex-Positionen) für die
    // 2D-Anzeige - siehe CHANGELOG [0.44.24]. Schlüssel: PlacedObject::modelPath (innerhalb
    // einer Kartensitzung eindeutig genug, da Modelle relativ zum selben Kartenordner
    // aufgelöst werden).
    struct ObjectFootprint {
        float minX = 0.0f, maxX = 0.0f, minZ = 0.0f, maxZ = 0.0f;
        std::vector<std::pair<float, float>> hull; // konvexe Huelle der untersten Schicht (siehe ComputeFootprintHull)
        bool valid = false;
    };
    std::unordered_map<std::string, ObjectFootprint> footprintCache;
    core::legacy::ShineTextFile recallCoordFile;
    bool recallCoordLoaded = false;
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
        // Pro-Zelle Status für die Grün/Rot-Markierung (Mockup-Wunsch, siehe CHANGELOG
        // [0.44.14]): 0=normal, 1=automatisch aus einer anderen Datei übernommen (grün),
        // 2=braucht noch manuelle Eingabe (rot). NUR Editor-seitiger Zustand für die aktuelle
        // Sitzung - wird NICHT in die .shn-Datei selbst geschrieben (das Format hat dafür
        // keinen Platz) und geht beim Neuladen verloren. Zeilen-Index -> Spalten-Index -> Status.
        std::vector<std::vector<std::uint8_t>> cellStatus;
        // Manuell geänderte Zellen bekommen im Grid einen eigenen Dirty-Indikator.
        // Wie cellStatus ist das reiner Editor-Sitzungszustand und wird nicht ins SHN geschrieben.
        std::vector<std::vector<std::uint8_t>> cellDirty;
    };
    struct ShnCellEdit {
        int document = -1;
        int row = -1;
        int column = -1;
        core::legacy::ShnValue before;
        core::legacy::ShnValue after;
    };
    std::vector<ShnCellEdit> shnUndo;
    std::vector<ShnCellEdit> shnRedo;
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
    bool shnInlineEditActive = false;
    bool shnInlineEditFocusPending = false;
    std::string shnEditBuffer;
    int shnSortColumn = -1;
    bool shnSortAscending = true;
    int shnColumnFilterFile = -1;
    std::vector<std::array<char, 64>> shnColumnFilters;
    bool shnHighlightClientServerDiff = true;
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
    // Alle .nif unter resmap, nicht nur resmap/nif(s): echte SHMDs referenzieren auch
    // field/, IDField/, KDField/ usw. Pfade sind relativ zu resmap.
    std::vector<std::string> availableNifFiles;      // z.B. "field/Rou/GuildHall.nif"
    bool textureListScanned = false;
    bool nifListScanned = false;
    std::string assetPickerFilter;
    char workspaceAssetFilter[128] = "";
    char objectOutlinerFilter[128] = "";
    // Basis-Ordner der beiden Listen oben - zum Auflösen relativer Picker-Einträge zu echten
    // Pfaden für Vorschaubilder (siehe GetOrLoadAssetThumbnail). Werden beim Scannen (Klick auf
    // "Durchsuchen...") zusammen mit der jeweiligen Liste gesetzt.
    std::filesystem::path textureAssetRoot;
    std::filesystem::path nifAssetRoot;
    std::filesystem::path resmapRootForThumbnails; // für die externe-Textur-Auflösung bei NIF-Vorschaubildern

    // Vorschaubild-Cache für Asset-Picker-Popups (Textur-/Objekt-Auswahl) und die Textur-Layer-
    // Liste - siehe GetOrLoadAssetThumbnail. Schlüssel: aufgelöster absoluter Pfad. Wird NUR für
    // gerade sichtbare Zeilen befüllt (ImGuiListClipper), nicht die komplette Liste auf einen
    // Schlag - bei .nif-Dateien bedeutet ein Treffer einen vollen LoadNifMesh-Aufruf (~4ms im
    // Schnitt, siehe CHANGELOG), das wäre bei tausenden Dateien spürbar. Ein Eintrag mit tex==0
    // bedeutet "schon versucht, kein Vorschaubild verfügbar" (z.B. Mesh ohne Textur) - wird
    // NICHT erneut versucht, sonst würde ein dauerhaft sichtbarer Eintrag ohne Textur bei jedem
    // einzelnen Frame neu geparst.
    struct AssetThumbnail { std::uint32_t tex = 0; float aspect = 1.0f; };
    std::unordered_map<std::string, AssetThumbnail> assetThumbnails;

    // Einmalige Vorlade-Sequenz für ALLE .nif-Vorschaubilder der gesamten Asset-Bibliothek
    // (nicht nur der auf der aktuellen Karte platzierten Objekte) - läuft in DrawMapEditor-
    // Launcher, sobald ein "resmap"-Ordner gefunden wurde, siehe StartNifThumbnailPrecache.
    // Zweck: verhindert das kurze Ruckeln beim ERSTEN Sichtbarwerden neuer Einträge im
    // NIF-Asset-Picker (siehe CHANGELOG [0.44.9]) - stattdessen EIN kurzer, sichtbarer
    // Ladebalken direkt nach Auswahl des Map-Editors.
    bool nifPrecacheActive = false;
    // Ob das DockSpace-Default-Layout der Map-Editor-Arbeitsfläche (Werkzeuge/2D/3D) schon
    // einmal aufgebaut wurde - siehe DrawMapEditorWorkspace. Nur beim allerersten Betreten
    // dieses Bildschirms in der Sitzung true setzen, damit spätere Nutzer-Anpassungen
    // (Größe/Position) nicht bei jedem Frame zurückgesetzt werden.
    bool mapEditorDockspaceBuilt = false;
    std::string nifPrecacheDoneForRoot; // resmapRoot, für den die Vorladung zuletzt komplett durchlief
    std::vector<std::string> nifPrecacheQueue; // Kopie von availableNifFiles zum Abarbeiten
    std::size_t nifPrecacheCursor = 0;
};

// Listet Dateien mit einer der angegebenen Endungen unter root (rekursiv, begrenzte Tiefe),
// relative Pfade zu root. Für Asset-Picker (Textur-/Modell-Auswahl) - siehe DrawAssetPickerPopup.
std::vector<std::string> ListFilesByExtension(const std::filesystem::path& root,
                                               const std::vector<std::string>& extensionsLower,
                                               int maxDepth = -1) {
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
    state.itemViewBuilt = false;
    state.avatarKey.clear();
    if (state.project.clientFolder[0] != '\0') {
        if (auto found = FindDirBreadthFirst(state.project.clientFolder, "reschar", 3, nullptr)) state.charRoot = found->string();
        if (auto found = FindDirBreadthFirst(state.project.clientFolder, "resitem", 3, nullptr)) state.itemRoot = found->string();
    }
    if (state.project.serverFolder[0] != '\0') {
        if (auto found = FindServerShineRoot(state.project.serverFolder)) {
            state.shineTextRoot = found->string();
            state.shnServerRoot = found->string();
        }
    }
    // Alles, was aus diesen Ordnern geladen wurde, neu laden.
    state.npcDialogLoaded = false;
    state.npcTextLoaded = false;
    state.mobRegenTextLoaded = false;
    state.shopTextLoaded = false;
    state.questDataLoaded = false;
    state.questDialogLoaded = false;
    state.questTextMapBuilt = false;
    state.questTextMap.clear();
    state.questListKey.clear();
    state.itemInfoLoaded = false;
    state.itemLookupTried = false;
    state.itemEntries.clear();
    state.itemByInx.clear();
    state.mobViewInfoLoaded = false;
    state.townPortalLoaded = false;
    state.recallCoordLoaded = false;
    state.mapInfoLoaded = false;
    state.portalDataSig.clear();
    state.shnAutoLoaded = false;
}

// Die Textur-Layer-Auflösung bleibt unabhängig von der Heightmap. Neue Karten verwenden
// standardmäßig 1024x1024, während beim Laden die tatsächlich gespeicherte Auflösung erhalten bleibt.

// Baut aus dem aktuellen Editor-Zustand ein LegacyMapProject für SaveLegacyMap zusammen -
// Kehrseite dessen, was ApplyProjectToState (siehe unten) beim Öffnen verteilt.
core::legacy::LegacyMapProject BuildProjectFromState(const EditorState& state) {
    core::legacy::LegacyMapProject project;
    project.ini = state.legacyIniMeta;
    project.preservedFiles = state.preservedMapFiles;
    project.heightmap = state.heightmap;
    project.hasHeightmap = state.heightmap.Width() > 0 && state.heightmap.Height() > 0;
    project.htdHeader = state.htdHeader;
    project.htdTrailingBytes = state.htdTrailingBytes;
    project.textureStack = state.textureStack;
    project.walkGrid = state.walkGrid;
    project.hasWalkGrid = state.walkGrid.Width() > 0 && state.walkGrid.Height() > 0;
    project.shbdHeader = state.shbdHeader;
    project.objects = state.placementSet;
    project.hasObjects = true;
    project.spatialIndex = state.legacySpatialIndex;
    project.hasSpatialIndex = state.hasLegacySpatialIndex;
    project.zone = state.legacyZoneMetadata;
    project.hasZone = state.hasLegacyZoneMetadata;
    return project;
}

int ShmdCategoryKind(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower == "sky") return 0;
    if (lower == "water") return 1;
    if (lower == "groundobject") return 2;
    return -1;
}

void RebuildShmdCategoryRenderSet(EditorState& state, const std::filesystem::path& mapDir) {
    state.shmdCategoryRenderSet = core::ObjectPlacementSet{};
    state.shmdCategoryRenderKind.clear();
    state.shmdCategorySource.clear();

    // Kategorieeinträge haben absichtlich keinen Placement-Transform. Die zugehörigen NIFs
    // tragen ihre Szenengeometrie selbst; für die Vorschau werden sie deshalb einmal mit
    // Identity-Transform gerendert. Das Original-ObjectPlacementSet bleibt unverändert.
    for (std::size_t categoryIndex = 0; categoryIndex < state.placementSet.categories.size(); ++categoryIndex) {
        const auto& category = state.placementSet.categories[categoryIndex];
        const int kind = ShmdCategoryKind(category.name);
        for (std::size_t pathIndex = 0; pathIndex < category.modelPaths.size(); ++pathIndex) {
            const auto& modelPath = category.modelPaths[pathIndex];
            if (modelPath.empty()) continue;
            core::PlacedObject obj;
            obj.modelPath = modelPath;
            state.shmdCategoryRenderSet.AddObject(std::move(obj));
            state.shmdCategoryRenderKind.push_back(kind);
            state.shmdCategorySource.emplace_back(categoryIndex, pathIndex);
        }
    }

    state.shmdCategoryHidden.assign(state.shmdCategoryRenderSet.Count(), 0);
    state.shmdCategoryMeshRenderer.LoadModelsForSet(state.shmdCategoryRenderSet, mapDir);
}

constexpr int kNoObjectSelection = -1;

int ShmdSelectionId(std::size_t renderIndex) {
    return -2 - static_cast<int>(renderIndex);
}

bool IsShmdSelection(int selectionId) {
    return selectionId <= -2;
}

std::optional<std::size_t> ShmdRenderIndex(int selectionId) {
    if (!IsShmdSelection(selectionId)) return std::nullopt;
    return static_cast<std::size_t>(-2 - selectionId);
}

std::filesystem::path CurrentObjectAssetMapDir(const EditorState& state) {
    if (state.legacyMapIniPath[0] != '\0') return std::filesystem::path(state.legacyMapIniPath).parent_path();
    if (state.legacyShmdPath[0] != '\0') return std::filesystem::path(state.legacyShmdPath).parent_path();
    if (state.legacySaveDir[0] != '\0') return std::filesystem::path(state.legacySaveDir);
    return {};
}

core::PlacedObject* EditableObject(EditorState& state, int selectionId) {
    if (selectionId >= 0) {
        const auto index = static_cast<std::size_t>(selectionId);
        return index < state.placementSet.Count() ? &state.placementSet.At(index) : nullptr;
    }
    const auto renderIndex = ShmdRenderIndex(selectionId);
    if (!renderIndex || *renderIndex >= state.shmdCategoryRenderSet.Count()) return nullptr;
    return &state.shmdCategoryRenderSet.At(*renderIndex);
}

const core::PlacedObject* EditableObject(const EditorState& state, int selectionId) {
    if (selectionId >= 0) {
        const auto index = static_cast<std::size_t>(selectionId);
        return index < state.placementSet.Count() ? &state.placementSet.At(index) : nullptr;
    }
    const auto renderIndex = ShmdRenderIndex(selectionId);
    if (!renderIndex || *renderIndex >= state.shmdCategoryRenderSet.Count()) return nullptr;
    return &state.shmdCategoryRenderSet.At(*renderIndex);
}

std::string ShmdSelectionCategoryName(const EditorState& state, int selectionId) {
    const auto renderIndex = ShmdRenderIndex(selectionId);
    if (!renderIndex || *renderIndex >= state.shmdCategorySource.size()) return {};
    const auto [categoryIndex, pathIndex] = state.shmdCategorySource[*renderIndex];
    (void)pathIndex;
    if (categoryIndex >= state.placementSet.categories.size()) return {};
    return state.placementSet.categories[categoryIndex].name;
}

bool PromoteSelectedShmdObjectsToPlacements(EditorState& state,
                                               std::optional<int> onlySelection = std::nullopt);

void ReloadObjectRenderers(EditorState& state) {
    const auto mapDir = CurrentObjectAssetMapDir(state);
    state.nifMeshRenderer.LoadModelsForSet(state.placementSet, mapDir);
    RebuildShmdCategoryRenderSet(state, mapDir);
    state.objectVisKey.clear();
}


void SyncObjectEditorMetadata(EditorState& state) {
    state.objectEditorHidden.resize(state.placementSet.Count(), 0);
    state.objectEditorLocked.resize(state.placementSet.Count(), 0);
}

std::string ShmdEditorObjectKey(const EditorState& state, int id) {
    const auto renderIndex=ShmdRenderIndex(id);
    if(!renderIndex || *renderIndex>=state.shmdCategorySource.size() ||
       *renderIndex>=state.shmdCategoryRenderSet.Count()) return {};
    const auto [categoryIndex,pathIndex]=state.shmdCategorySource[*renderIndex];
    if(categoryIndex>=state.placementSet.categories.size()) return {};
    const auto& category=state.placementSet.categories[categoryIndex];
    if(pathIndex>=category.modelPaths.size()) return {};
    return category.name+"|"+category.modelPaths[pathIndex];
}

bool IsObjectEditorLocked(const EditorState& state, int id) {
    if(id>=0)
        return static_cast<std::size_t>(id)<state.objectEditorLocked.size() &&
               state.objectEditorLocked[static_cast<std::size_t>(id)]!=0;
    const std::string key=ShmdEditorObjectKey(state,id);
    return !key.empty() && state.shmdEditorLockedKeys.contains(key);
}

bool IsObjectEditorHidden(const EditorState& state, int id) {
    if(id>=0)
        return static_cast<std::size_t>(id)<state.objectEditorHidden.size() &&
               state.objectEditorHidden[static_cast<std::size_t>(id)]!=0;
    const std::string key=ShmdEditorObjectKey(state,id);
    return !key.empty() && state.shmdEditorHiddenKeys.contains(key);
}

struct EditVec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct EditQuat { float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f; };

EditQuat NormalizeEditQuat(EditQuat q) {
    const float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (len < 1.0e-8f) return {};
    return {q.x/len, q.y/len, q.z/len, q.w/len};
}

EditQuat MulEditQuatRaw(const EditQuat& a, const EditQuat& b) {
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };
}

EditQuat MulEditQuat(const EditQuat& a, const EditQuat& b) {
    return NormalizeEditQuat(MulEditQuatRaw(a,b));
}

EditQuat ConjugateEditQuat(const EditQuat& q) { return {-q.x,-q.y,-q.z,q.w}; }

EditVec3 RotateEditVec(const EditQuat& qn, const EditVec3& v) {
    const EditQuat q = NormalizeEditQuat(qn);
    const EditQuat p{v.x,v.y,v.z,0.0f};
    const EditQuat r = MulEditQuatRaw(MulEditQuatRaw(q,p), ConjugateEditQuat(q));
    return {r.x,r.y,r.z};
}

app::Mat4 ObjectEditMatrix(const EditVec3& position, const EditQuat& qn, float scale) {
    const EditQuat q = NormalizeEditQuat(qn);
    app::Mat4 m = app::Mat4::Identity();
    m.m[0] = (1.0f - 2.0f*(q.y*q.y + q.z*q.z)) * scale;
    m.m[1] = (2.0f*(q.x*q.y + q.w*q.z)) * scale;
    m.m[2] = (2.0f*(q.x*q.z - q.w*q.y)) * scale;
    m.m[4] = (2.0f*(q.x*q.y - q.w*q.z)) * scale;
    m.m[5] = (1.0f - 2.0f*(q.x*q.x + q.z*q.z)) * scale;
    m.m[6] = (2.0f*(q.y*q.z + q.w*q.x)) * scale;
    m.m[8] = (2.0f*(q.x*q.z + q.w*q.y)) * scale;
    m.m[9] = (2.0f*(q.y*q.z - q.w*q.x)) * scale;
    m.m[10] = (1.0f - 2.0f*(q.x*q.x + q.y*q.y)) * scale;
    m.m[12] = position.x; m.m[13] = position.y; m.m[14] = position.z;
    return m;
}

float MatrixUniformScale(const app::Mat4& m) {
    const auto len = [&](int c) {
        return std::sqrt(m.m[c*4+0]*m.m[c*4+0] + m.m[c*4+1]*m.m[c*4+1] + m.m[c*4+2]*m.m[c*4+2]);
    };
    return std::max(1.0e-6f, (len(0)+len(1)+len(2))/3.0f);
}

EditQuat MatrixRotationQuat(const app::Mat4& m) {
    const float s = MatrixUniformScale(m);
    const float r00=m.m[0]/s, r01=m.m[4]/s, r02=m.m[8]/s;
    const float r10=m.m[1]/s, r11=m.m[5]/s, r12=m.m[9]/s;
    const float r20=m.m[2]/s, r21=m.m[6]/s, r22=m.m[10]/s;
    EditQuat q;
    const float trace = r00+r11+r22;
    if (trace > 0.0f) {
        const float t = std::sqrt(trace+1.0f)*2.0f;
        q.w=0.25f*t; q.x=(r21-r12)/t; q.y=(r02-r20)/t; q.z=(r10-r01)/t;
    } else if (r00 > r11 && r00 > r22) {
        const float t = std::sqrt(1.0f+r00-r11-r22)*2.0f;
        q.w=(r21-r12)/t; q.x=0.25f*t; q.y=(r01+r10)/t; q.z=(r02+r20)/t;
    } else if (r11 > r22) {
        const float t = std::sqrt(1.0f+r11-r00-r22)*2.0f;
        q.w=(r02-r20)/t; q.x=(r01+r10)/t; q.y=0.25f*t; q.z=(r12+r21)/t;
    } else {
        const float t = std::sqrt(1.0f+r22-r00-r11)*2.0f;
        q.w=(r10-r01)/t; q.x=(r02+r20)/t; q.y=(r12+r21)/t; q.z=0.25f*t;
    }
    return NormalizeEditQuat(q);
}

struct ObjectSelectionPivot {
    bool valid = false;
    EditVec3 position{};
    EditQuat rotation{};
    float scale = 1.0f;
    std::size_t editableCount = 0;
};

ObjectSelectionPivot ComputeObjectSelectionPivot(const EditorState& state) {
    ObjectSelectionPivot p;
    const core::PlacedObject* orientationSource = nullptr;
    for (const int id : state.selectedObjects) {
        if (IsObjectEditorLocked(state,id)) continue;
        const auto* obj = EditableObject(state,id);
        if (!obj) continue;
        p.position.x += obj->posX; p.position.y += obj->posY; p.position.z += obj->posZ;
        ++p.editableCount;
        if (!orientationSource) orientationSource = obj;
    }
    if (p.editableCount == 0 || !orientationSource) return p;
    const float inv = 1.0f/static_cast<float>(p.editableCount);
    p.position.x*=inv; p.position.y*=inv; p.position.z*=inv;
    p.rotation = NormalizeEditQuat({orientationSource->rotX,orientationSource->rotY,
                                    orientationSource->rotZ,orientationSource->rotW});
    p.scale = p.editableCount == 1 ? std::max(0.001f,orientationSource->scale) : 1.0f;
    p.valid = true;
    return p;
}

void RotateSelectedObjectsAroundPivot(EditorState& state, const EditVec3& pivot, const EditQuat& delta) {
    PromoteSelectedShmdObjectsToPlacements(state);
    SyncObjectEditorMetadata(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& obj=state.placementSet.At(static_cast<std::size_t>(id));
        const EditVec3 rel{obj.posX-pivot.x,obj.posY-pivot.y,obj.posZ-pivot.z};
        const EditVec3 rr=RotateEditVec(delta,rel);
        obj.posX=pivot.x+rr.x; obj.posY=pivot.y+rr.y; obj.posZ=pivot.z+rr.z;
        const EditQuat oq{obj.rotX,obj.rotY,obj.rotZ,obj.rotW};
        const EditQuat nq=MulEditQuat(delta,oq);
        obj.rotX=nq.x; obj.rotY=nq.y; obj.rotZ=nq.z; obj.rotW=nq.w;
    }
}

void ScaleSelectedObjectsAroundPivot(EditorState& state, const EditVec3& pivot, float factor) {
    if (!std::isfinite(factor) || factor <= 0.0f) return;
    PromoteSelectedShmdObjectsToPlacements(state);
    SyncObjectEditorMetadata(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& obj=state.placementSet.At(static_cast<std::size_t>(id));
        obj.posX=pivot.x+(obj.posX-pivot.x)*factor;
        obj.posY=pivot.y+(obj.posY-pivot.y)*factor;
        obj.posZ=pivot.z+(obj.posZ-pivot.z)*factor;
        obj.scale=std::clamp(obj.scale*factor,0.01f,100.0f);
    }
}

void CopySelectedObjects(EditorState& state) {
    state.objectClipboard.clear();
    for (const int id : state.selectedObjects)
        if (const auto* obj=EditableObject(state,id)) state.objectClipboard.push_back(*obj);
    state.statusMessage=state.objectClipboard.empty()
        ? "Keine Objekte kopiert."
        : std::to_string(state.objectClipboard.size())+" Objekt(e) kopiert.";
}

void PasteObjectClipboard(EditorState& state) {
    if (state.objectClipboard.empty()) return;
    SyncObjectEditorMetadata(state);
    const float offset=state.objectGizmoSnap ? std::max(1.0f,state.objectMoveSnap) : 50.0f;
    std::vector<int> ids;
    for (auto obj : state.objectClipboard) {
        obj.posX+=offset; obj.posZ+=offset;
        const int id=static_cast<int>(state.placementSet.AddObject(std::move(obj)));
        ids.push_back(id);
        state.objectEditorHidden.push_back(0); state.objectEditorLocked.push_back(0);
    }
    state.selectedObjects=std::move(ids);
    state.selectedObject=state.selectedObjects.empty()?kNoObjectSelection:state.selectedObjects.back();
    ReloadObjectRenderers(state);
    state.statusMessage=std::to_string(state.selectedObjects.size())+" Objekt(e) eingefügt.";
}

void DuplicateSelectedObjects(EditorState& state) {
    if (state.selectedObjects.empty()) return;
    std::vector<core::PlacedObject> copies;
    for (const int id : state.selectedObjects)
        if (const auto* obj=EditableObject(state,id)) copies.push_back(*obj);
    if (copies.empty()) return;
    const float offset=state.objectGizmoSnap ? std::max(1.0f,state.objectMoveSnap) : 50.0f;
    SyncObjectEditorMetadata(state);
    std::vector<int> ids;
    for (auto obj : copies) {
        obj.posX+=offset; obj.posZ+=offset;
        const int id=static_cast<int>(state.placementSet.AddObject(std::move(obj)));
        ids.push_back(id);
        state.objectEditorHidden.push_back(0); state.objectEditorLocked.push_back(0);
    }
    state.selectedObjects=std::move(ids);
    state.selectedObject=state.selectedObjects.back();
    ReloadObjectRenderers(state);
    state.statusMessage=std::to_string(state.selectedObjects.size())+" Objekt(e) dupliziert.";
}

void GroundSelectedObjects(EditorState& state) {
    if (state.selectedObjects.empty()) return;
    PromoteSelectedShmdObjectsToPlacements(state);
    SyncObjectEditorMetadata(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& obj=state.placementSet.At(static_cast<std::size_t>(id));
        obj.posY=state.heightmap.SampleWorld(obj.posX,obj.posZ);
    }
    state.statusMessage="Auswahl auf Terrain gesetzt.";
}

void FocusSelectedObjects(EditorState& state) {
    const auto pivot=ComputeObjectSelectionPivot(state);
    if (!pivot.valid) return;
    float radius=150.0f;
    for (const int id : state.selectedObjects) {
        const auto* obj=EditableObject(state,id);
        if (!obj) continue;
        const float dx=obj->posX-pivot.position.x, dy=obj->posY-pivot.position.y, dz=obj->posZ-pivot.position.z;
        radius=std::max(radius,std::sqrt(dx*dx+dy*dy+dz*dz)+100.0f*std::max(1.0f,obj->scale));
    }
    state.camera.SetTarget(pivot.position.x,pivot.position.y,pivot.position.z);
    const float desired=std::clamp(radius*2.8f,120.0f,25000.0f);
    state.camera.Zoom(desired-state.camera.Distance());
}

void SelectObjectId(EditorState& state, int id, bool ctrl) {
    if (ctrl) {
        auto it = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), id);
        if (it == state.selectedObjects.end()) state.selectedObjects.push_back(id);
        else state.selectedObjects.erase(it);
    } else {
        state.selectedObjects = {id};
    }
    state.selectedObject = state.selectedObjects.empty() ? kNoObjectSelection : state.selectedObjects.back();
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
}

void ClearObjectSelection(EditorState& state) {
    state.selectedObjects.clear();
    state.selectedObject = kNoObjectSelection;
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
}

void SelectAllNormalObjects(EditorState& state) {
    state.selectedObjects.clear();
    state.selectedObjects.reserve(state.placementSet.Count());
    for (std::size_t i = 0; i < state.placementSet.Count(); ++i)
        state.selectedObjects.push_back(static_cast<int>(i));
    state.selectedObject = state.selectedObjects.empty() ? kNoObjectSelection : state.selectedObjects.back();
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
}

void SelectObjectFromList(EditorState& state, int id, int ordinal,
                          const std::vector<int>& orderedIds, bool ctrl, bool shift) {
    if (shift && state.objectListRangeAnchor >= 0 &&
        state.objectListRangeAnchor < static_cast<int>(orderedIds.size())) {
        if (!ctrl) state.selectedObjects.clear();
        const int lo = std::min(state.objectListRangeAnchor, ordinal);
        const int hi = std::max(state.objectListRangeAnchor, ordinal);
        for (int i = lo; i <= hi; ++i) {
            const int candidate = orderedIds[static_cast<std::size_t>(i)];
            if (std::find(state.selectedObjects.begin(), state.selectedObjects.end(), candidate) == state.selectedObjects.end())
                state.selectedObjects.push_back(candidate);
        }
        state.selectedObject = id;
        state.selectedObjectModelPathFor = kNoObjectSelection;
        state.selectedObjectModelPathDirty = false;
    } else {
        SelectObjectId(state, id, ctrl);
    }
    state.objectListRangeAnchor = ordinal;
}

// Auf der 2D-Karte darf ein normaler Klick auf EIN BEREITS ausgewähltes Objekt die bestehende
// Mehrfachauswahl nicht zerstören: genau dieser Klick ist der Start eines Gruppen-Drags.
void SelectObjectOnCanvas(EditorState& state, int id, bool ctrl) {
    const auto it = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), id);
    const bool alreadySelected = it != state.selectedObjects.end();
    if (!ctrl && alreadySelected && state.selectedObjects.size() > 1) {
        state.selectedObject = id;
        state.selectedObjectModelPathFor = kNoObjectSelection;
        state.selectedObjectModelPathDirty = false;
        return;
    }
    SelectObjectId(state, id, ctrl);
}

void EraseShmdCategorySources(EditorState& state,
                              std::vector<std::pair<std::size_t, std::size_t>> sources) {
    std::sort(sources.begin(), sources.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second > b.second;
    });
    sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
    for (const auto [categoryIndex, pathIndex] : sources) {
        if (categoryIndex >= state.placementSet.categories.size()) continue;
        auto& paths = state.placementSet.categories[categoryIndex].modelPaths;
        if (pathIndex < paths.size()) paths.erase(paths.begin() + static_cast<std::ptrdiff_t>(pathIndex));
    }
}

void DeleteSelectedObjects(EditorState& state) {
    std::vector<int> placementIndices;
    std::vector<std::pair<std::size_t, std::size_t>> categorySources;
    for (const int id : state.selectedObjects) {
        if (id >= 0) {
            if (static_cast<std::size_t>(id) < state.placementSet.Count() && !IsObjectEditorLocked(state,id))
                placementIndices.push_back(id);
            continue;
        }
        if (IsObjectEditorLocked(state,id)) continue;
        const auto renderIndex = ShmdRenderIndex(id);
        if (renderIndex && *renderIndex < state.shmdCategorySource.size()) {
            categorySources.push_back(state.shmdCategorySource[*renderIndex]);
        }
    }

    std::sort(placementIndices.begin(), placementIndices.end(), std::greater<int>());
    placementIndices.erase(std::unique(placementIndices.begin(), placementIndices.end()), placementIndices.end());
    for (const int index : placementIndices) {
        if (index >= 0 && static_cast<std::size_t>(index) < state.placementSet.Count()) {
            state.placementSet.RemoveObject(static_cast<std::size_t>(index));
            if (static_cast<std::size_t>(index) < state.objectEditorHidden.size())
                state.objectEditorHidden.erase(state.objectEditorHidden.begin() + index);
            if (static_cast<std::size_t>(index) < state.objectEditorLocked.size())
                state.objectEditorLocked.erase(state.objectEditorLocked.begin() + index);
        }
    }
    EraseShmdCategorySources(state, std::move(categorySources));

    ClearObjectSelection(state);
    ReloadObjectRenderers(state);
}

void DeleteAllNormalObjects(EditorState& state) {
    const std::size_t removed = state.placementSet.Count();
    state.placementSet.ClearObjects();
    state.objectEditorHidden.clear();
    state.objectEditorLocked.clear();
    ClearObjectSelection(state);
    ReloadObjectRenderers(state);
    state.statusMessage = std::to_string(removed) +
        " normale Placement-Objekte entfernt. Sky/Water/GroundObject bleiben unverändert.";
}

// SHMD-Kategorieeinträge besitzen im Dateiformat KEINEN Transform. Sobald der Benutzer einen
// solchen Eintrag verschiebt/dreht/skaliert, wird er deshalb automatisch aus der Kategorie-
// Pfadliste entfernt und als normales Placement mit exakt diesem Transform gespeichert.
bool PromoteSelectedShmdObjectsToPlacements(EditorState& state, std::optional<int> onlySelection) {
    struct Promotion {
        int oldId = kNoObjectSelection;
        core::PlacedObject object;
        std::pair<std::size_t, std::size_t> source{};
    };
    std::vector<Promotion> promotions;
    for (const int id : state.selectedObjects) {
        if (onlySelection && id != *onlySelection) continue;
        if (IsObjectEditorLocked(state,id)) continue;
        const auto renderIndex = ShmdRenderIndex(id);
        if (!renderIndex || *renderIndex >= state.shmdCategorySource.size() ||
            *renderIndex >= state.shmdCategoryRenderSet.Count()) continue;
        promotions.push_back({id, state.shmdCategoryRenderSet.At(*renderIndex),
                              state.shmdCategorySource[*renderIndex]});
    }
    if (promotions.empty()) return false;

    std::unordered_map<int, int> replacement;
    std::vector<std::pair<std::size_t, std::size_t>> sources;
    for (auto& promotion : promotions) {
        const int newId = static_cast<int>(state.placementSet.AddObject(std::move(promotion.object)));
        state.objectEditorHidden.push_back(0);
        state.objectEditorLocked.push_back(0);
        replacement[promotion.oldId] = newId;
        sources.push_back(promotion.source);
    }
    EraseShmdCategorySources(state, std::move(sources));

    for (int& id : state.selectedObjects) {
        if (const auto it = replacement.find(id); it != replacement.end()) id = it->second;
    }
    if (const auto it = replacement.find(state.selectedObject); it != replacement.end()) {
        state.selectedObject = it->second;
    }
    // Bei einer Einzel-Transformbearbeitung kann das Entfernen eines Kategoriepfads die
    // negativen Render-IDs anderer, weiterhin ausgewählter SHMD-Einträge verschieben.
    // Diese anderen Einträge wurden durch den Regler ohnehin nicht verändert; daher bleibt
    // danach nur das tatsächlich bearbeitete/konvertierte Objekt ausgewählt. Beim Gruppen-
    // Drag (onlySelection==nullopt) werden dagegen alle ausgewählten SHMD-Einträge gemeinsam
    // promoted und die gemischte Mehrfachauswahl bleibt vollständig erhalten.
    if (onlySelection) {
        if (const auto it = replacement.find(*onlySelection); it != replacement.end()) {
            state.selectedObjects = {it->second};
            state.selectedObject = it->second;
        }
    }
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
    ReloadObjectRenderers(state);
    state.statusMessage =
        "SHMD-Szenenmodell in normales Placement umgewandelt: Sky/Water/GroundObject speichern "
        "keine eigenen Transform-Daten; Position/Rotation/Skalierung bleiben so verlustfrei erhalten.";
    return true;
}

void MoveSelectedObjectsBy(EditorState& state, float dx, float dy, float dz) {
    if (state.selectedObjects.empty()) return;
    PromoteSelectedShmdObjectsToPlacements(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& object = state.placementSet.At(static_cast<std::size_t>(id));
        object.posX += dx;
        object.posY += dy;
        object.posZ += dz;
    }
}

void RotateSelectedObjectsYawBy(EditorState& state, float deltaRadians) {
    if (state.selectedObjects.empty() || std::abs(deltaRadians) < 1.0e-8f) return;
    PromoteSelectedShmdObjectsToPlacements(state);
    const float sy = std::sin(deltaRadians * 0.5f);
    const float cy = std::cos(deltaRadians * 0.5f);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& object = state.placementSet.At(static_cast<std::size_t>(id));
        const float x = object.rotX, y = object.rotY, z = object.rotZ, w = object.rotW;
        object.rotX = cy * x + sy * z;
        object.rotY = cy * y + sy * w;
        object.rotZ = cy * z - sy * x;
        object.rotW = cy * w - sy * y;
        const float length = std::sqrt(object.rotX * object.rotX + object.rotY * object.rotY +
                                       object.rotZ * object.rotZ + object.rotW * object.rotW);
        if (length > 1.0e-8f) {
            object.rotX /= length; object.rotY /= length; object.rotZ /= length; object.rotW /= length;
        }
    }
}

void ScaleSelectedObjectsBy(EditorState& state, float factor) {
    if (state.selectedObjects.empty() || !std::isfinite(factor) || factor <= 0.0f) return;
    PromoteSelectedShmdObjectsToPlacements(state);
    for (const int id : state.selectedObjects) {
        if (id < 0 || static_cast<std::size_t>(id) >= state.placementSet.Count() || IsObjectEditorLocked(state,id)) continue;
        auto& object = state.placementSet.At(static_cast<std::size_t>(id));
        object.scale = std::clamp(object.scale * factor, 0.01f, 100.0f);
    }
}

void SyncSelectedObjectModelPath(EditorState& state) {
    if (state.selectedObject == kNoObjectSelection) {
        state.selectedObjectModelPath[0] = '\0';
        state.selectedObjectModelPathFor = kNoObjectSelection;
        state.selectedObjectModelPathDirty = false;
        return;
    }
    if (state.selectedObjectModelPathFor == state.selectedObject) return;
    const auto* object = EditableObject(state, state.selectedObject);
    if (!object) return;
    std::snprintf(state.selectedObjectModelPath, sizeof(state.selectedObjectModelPath), "%s", object->modelPath.c_str());
    state.selectedObjectModelPathFor = state.selectedObject;
    state.selectedObjectModelPathDirty = false;
}

void ApplySelectedObjectModelPath(EditorState& state, const std::string& modelPath) {
    if (modelPath.empty() || state.selectedObject == kNoObjectSelection) return;
    if (state.selectedObject >= 0) {
        if (auto* object = EditableObject(state, state.selectedObject)) object->modelPath = modelPath;
        state.nifMeshRenderer.LoadModelsForSet(state.placementSet, CurrentObjectAssetMapDir(state));
    } else {
        const auto renderIndex = ShmdRenderIndex(state.selectedObject);
        if (!renderIndex || *renderIndex >= state.shmdCategorySource.size()) return;
        const auto [categoryIndex, pathIndex] = state.shmdCategorySource[*renderIndex];
        if (categoryIndex >= state.placementSet.categories.size()) return;
        auto& paths = state.placementSet.categories[categoryIndex].modelPaths;
        if (pathIndex >= paths.size()) return;
        paths[pathIndex] = modelPath;
        RebuildShmdCategoryRenderSet(state, CurrentObjectAssetMapDir(state));
    }
    state.footprintCache.erase(modelPath);
    state.selectedObjectModelPathFor = kNoObjectSelection;
    SyncSelectedObjectModelPath(state);
}

void RefreshShmdCategoryVisibility(EditorState& state) {
    state.shmdCategoryHidden.resize(state.shmdCategoryRenderSet.Count(), 0);
    for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
        bool visible = true;
        if (i < state.shmdCategoryRenderKind.size()) {
            switch (state.shmdCategoryRenderKind[i]) {
                case 0: visible = state.showShmdSky; break;
                case 1: visible = state.showShmdWater; break;
                case 2: visible = state.showShmdGroundObject; break;
                default: break; // unbekannte generische SHMD-Kategorie bleibt sichtbar
            }
        }
        const int id=ShmdSelectionId(i);
        if (IsObjectEditorHidden(state,id)) visible=false;
        state.shmdCategoryHidden[i] = visible ? 0 : 1;
    }
}

// Verteilt ein frisch geöffnetes LegacyMapProject auf die einzelnen Editor-Zustandsfelder -
// setzt außerdem alle Undo-Stacks/Auswahl/Dirty-Flags zurück (neue Karte, alte Historie ungültig).
void ApplyProjectToState(EditorState& state, core::legacy::LegacyMapProject&& project, const std::filesystem::path& mapDir) {
    state.preservedMapFiles = std::move(project.preservedFiles);
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
        state.view2dZoom = 1.0f;
        state.view2dCenterU = state.view2dCenterV = 0.5f;
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
    state.selectedObject = kNoObjectSelection;
    state.selectedObjects.clear();
    state.selectedObjectModelPathFor = kNoObjectSelection;
    state.selectedObjectModelPathDirty = false;
    state.objectListRangeAnchor = -1;
    state.objectEditorHidden.assign(state.placementSet.Count(), 0);
    state.objectEditorLocked.assign(state.placementSet.Count(), 0);
    state.shmdEditorHiddenKeys.clear();
    state.shmdEditorLockedKeys.clear();
    // Versucht, für alle Objekte echte .nif-Meshes zu laden (aktuell nur untexturierte Meshes
    // erfolgreich, siehe docs/MAP_FORMAT.md) - für den Rest bleibt der Platzhalter-Marker.
    state.nifMeshRenderer.LoadModelsForSet(state.placementSet, mapDir);
    RebuildShmdCategoryRenderSet(state, mapDir);

    state.legacySpatialIndex = std::move(project.spatialIndex);
    state.hasLegacySpatialIndex = project.hasSpatialIndex;
    state.legacyZoneMetadata = std::move(project.zone);
    state.selectedZone = 0;
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

constexpr float WalkGridCellSize() { return core::WalkGrid::kCellSize; }

// Vorschau des Block&Walk-Gitters auf ZELL-Ebene (CHANGELOG [0.44.29]): eine Textur-Zeile je Gitter-
// Zeile, ein Pixel je Zelle (6.25 Welteinheiten), 255 = blockiert, 0 = begehbar. Fruehere Fassung
// stellte die 16-Bit-Woerter als Pixel dar (16fach zu grob) und streckte sie auf die Kartenform.
void UpdateWalkPreviewTexture(EditorState& state) {
    const auto& grid = state.walkGrid;
    if (grid.Width() == 0 || grid.Height() == 0) return;
    const std::uint32_t cols = grid.Cols(), rows = grid.Rows();

    std::vector<unsigned char> pixels(static_cast<std::size_t>(cols) * rows);
    for (std::uint32_t z = 0; z < rows; ++z) {
        unsigned char* dst = &pixels[static_cast<std::size_t>(z) * cols];
        for (std::uint32_t wx = 0; wx < grid.Width(); ++wx) {
            const auto word = static_cast<std::uint16_t>(grid.At(wx, z));
            for (std::uint32_t b = 0; b < 16; ++b) dst[wx * 16 + b] = ((word >> b) & 1u) ? 255 : 0;
        }
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(cols), static_cast<int>(rows), 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    state.walkPreviewCols = cols;
    state.walkPreviewRows = rows;
}

// Aktualisiert nur ein Zell-Rechteck der Vorschau (waehrend des Malens - die Gesamttextur kann bei
// grossen Karten ~58 MB haben, ein Vollupload pro Mausbewegung waere zu langsam).
void UpdateWalkPreviewRect(EditorState& state, std::uint32_t x0, std::uint32_t z0, std::uint32_t x1, std::uint32_t z1) {
    const auto& grid = state.walkGrid;
    if (state.walkPreviewTex == 0 || state.walkPreviewCols != grid.Cols() || state.walkPreviewRows != grid.Rows()) { state.walkPreviewDirty = true; return; }
    x1 = std::min(x1, grid.Cols() - 1);
    z1 = std::min(z1, grid.Rows() - 1);
    if (x0 > x1 || z0 > z1) return;
    const std::uint32_t w = x1 - x0 + 1, h = z1 - z0 + 1;
    std::vector<unsigned char> block(static_cast<std::size_t>(w) * h);
    for (std::uint32_t z = 0; z < h; ++z)
        for (std::uint32_t x = 0; x < w; ++x) block[static_cast<std::size_t>(z) * w + x] = grid.CellBlocked(x0 + x, z0 + z) ? 255 : 0;
    glBindTexture(GL_TEXTURE_2D, state.walkPreviewTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<int>(x0), static_cast<int>(z0), static_cast<int>(w), static_cast<int>(h), GL_RED, GL_UNSIGNED_BYTE, block.data());
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
    const bool weOwnCom = SUCCEEDED(comInit);

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
std::optional<std::string> BrowseForShnFileWindows(const char* title, bool kfm = false) {
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool weOwnCom = SUCCEEDED(comInit);
    std::optional<std::string> result;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog))) && dialog) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
        COMDLG_FILTERSPEC filters[] = {{kfm ? L"Fiesta KFM" : L"Fiesta SHN", kfm ? L"*.kfm" : L"*.shn"}, {L"Alle Dateien", L"*.*"}};
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
        if (UI::Button("Asset-Ordner wählen...")) {
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
                if (UI::Selectable(state.discoveredMaps[static_cast<std::size_t>(i)].name.c_str(), selected)) {
                    state.selectedMapIndex = i;
                    std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s",
                                  state.discoveredMaps[static_cast<std::size_t>(i)].iniPath.c_str());
                }
            }
            ImGui::EndChild();
        }

        UI::InputText("Karte-.ini##project", state.legacyMapIniPath, sizeof(state.legacyMapIniPath));
        if (UI::Button("Karte öffnen")) {
            core::legacy::LegacyMapOpenReport report;
            auto result = core::legacy::OpenLegacyMap(state.legacyMapIniPath, &report);
            if (result) {
                const std::filesystem::path iniPath(state.legacyMapIniPath);
                ApplyProjectToState(state, std::move(*result), iniPath.parent_path());
                std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", iniPath.parent_path().string().c_str());
                std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", iniPath.stem().string().c_str());
                state.statusMessage = "Karte ge\u00f6ffnet (" + std::to_string(report.issues.size()) + " Hinweis(e)) - " +
                                       std::to_string(state.placementSet.Count() + state.shmdCategoryRenderSet.Count()) +
                                       " Objekte (" + std::to_string(state.shmdCategoryRenderSet.Count()) + " SHMD-Szenenmodelle), " +
                                       std::to_string(state.textureStack.LayerCount()) + " Textur-Layer.";
                for (const auto& issue : report.issues) {
                    state.statusMessage += "\n- " + issue;
                }
            } else {
                state.statusMessage = "Karte \u00f6ffnen fehlgeschlagen: " + result.error();
            }
        }
        UI::InputText("Ausgabeverzeichnis##project", state.legacySaveDir, sizeof(state.legacySaveDir));
#ifdef _WIN32
        ImGui::SameLine();
        if (UI::Button("Wählen...##saveDir")) {
            if (auto picked = BrowseForFolderWindows("Ausgabeverzeichnis w\u00e4hlen")) {
                std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", picked->c_str());
            }
        }
#endif
        UI::InputText("Kartenname##project", state.legacySaveStem, sizeof(state.legacySaveStem));
        ImGui::BeginDisabled(!state.hasLegacyIniMeta);
        if (UI::Button("Karte speichern")) {
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
        if (UI::MenuItem("Neu (257x257)")) {
            state.heightmap = core::Heightmap(257, 257, 50.0f, 50.0f);
            state.undo.Clear();
            state.meshDirty = true;
            SyncWalkGridSize(state);
            state.selectedLayer = -1;
            state.statusMessage = "Neue leere Heightmap erstellt.";
        }
        ImGui::Separator();

        ImGui::TextDisabled("Legacy-Heightmap-Import/Export (.HTD / .HTDG)");
        UI::InputText("Pfad##legacy", state.legacyPath, sizeof(state.legacyPath));
        UI::InputInt("Breite (aus .ini)", &state.legacyWidth);
        UI::InputInt("Höhe (aus .ini)", &state.legacyHeight);
        UI::InputFloat("Blockbreite", &state.legacyBlockWidth);
        UI::InputFloat("Blockhöhe", &state.legacyBlockHeight);
        if (UI::Button("Importieren##htd")) {
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
        if (UI::Button("Exportieren##htd")) {
            auto result = core::ExportLegacyHtd(state.heightmap, state.legacyPath, state.htdHeader, state.htdTrailingBytes);
            state.statusMessage = result ? "Legacy-HTD exportiert nach: " + std::string(state.legacyPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Texturing-Set (ini + Blend-BMPs)");
        UI::InputText("ini-Pfad##legacyTex", state.legacyIniPath, sizeof(state.legacyIniPath));
        if (UI::Button("Legacy-Set importieren")) {
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
        UI::InputText("Export-Verzeichnis##legacyTex", state.legacyExportDir, sizeof(state.legacyExportDir));
        ImGui::BeginDisabled(!state.hasLegacyIniMeta);
        if (UI::Button("Legacy-Set exportieren")) {
            auto result = core::legacy::ExportLegacyTextureSet(state.textureStack, state.legacyIniMeta, state.legacyExportDir, "Rou.ini");
            state.statusMessage = result ? "Legacy-Set exportiert nach: " + std::string(state.legacyExportDir)
                                          : "Export fehlgeschlagen: " + result.error();
        }
        ImGui::EndDisabled();
        if (!state.hasLegacyIniMeta) {
            ImGui::TextDisabled("(Erst \u00fcber 'Legacy-Set importieren' Metadaten laden.)");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Import/Export (.shbd)");
        UI::InputText("Pfad##walkLegacy", state.walkLegacyPath, sizeof(state.walkLegacyPath));
        UI::InputInt("Breite##walkLegacy", &state.walkLegacyWidth);
        UI::InputInt("Höhe##walkLegacy", &state.walkLegacyHeight);
        if (UI::Button("Importieren##shbd")) {
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
        if (UI::Button("Exportieren##shbd")) {
            auto result = core::ExportLegacyShbd(state.walkGrid, state.walkLegacyPath, state.shbdHeader);
            state.statusMessage = result ? "Legacy-shbd exportiert nach: " + std::string(state.walkLegacyPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Legacy-Objekt-Placement (.shmd)");
        UI::InputText("Pfad##shmd", state.legacyShmdPath, sizeof(state.legacyShmdPath));
        if (UI::Button("Importieren##shmd")) {
            auto result = core::legacy::ParseLegacyShmd(state.legacyShmdPath);
            if (result) {
                state.placementSet = std::move(*result);
                state.selectedObject = kNoObjectSelection;
                state.selectedObjects.clear();
                state.selectedObjectModelPathFor = kNoObjectSelection;
                state.selectedObjectModelPathDirty = false;
                state.objectListRangeAnchor = -1;
                state.objectEditorHidden.assign(state.placementSet.Count(), 0);
                state.objectEditorLocked.assign(state.placementSet.Count(), 0);
                state.shmdEditorHiddenKeys.clear();
                state.shmdEditorLockedKeys.clear();
                const std::filesystem::path shmdMapDir = std::filesystem::path(state.legacyShmdPath).parent_path();
                state.nifMeshRenderer.LoadModelsForSet(state.placementSet, shmdMapDir);
                RebuildShmdCategoryRenderSet(state, shmdMapDir);
                state.statusMessage = "Legacy-shmd importiert (" + std::to_string(state.placementSet.Count()) +
                                      " Placement-Objekte, " + std::to_string(state.shmdCategoryRenderSet.Count()) +
                                      " SHMD-Szenenmodelle): " + std::string(state.legacyShmdPath);
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + result.error();
            }
        }
        ImGui::SameLine();
        if (UI::Button("Exportieren##shmd")) {
            auto result = core::legacy::SerializeLegacyShmd(state.placementSet, state.legacyShmdPath);
            state.statusMessage = result ? "Legacy-shmd exportiert nach: " + std::string(state.legacyShmdPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }

        ImGui::TextDisabled("Legacy räumlicher Index (.idm) - reiner Pass-Through, Semantik ungeklärt");
        UI::InputText("Pfad##idm", state.legacyIdmPath, sizeof(state.legacyIdmPath));
        if (UI::Button("Importieren##idm")) {
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
        if (UI::Button("Exportieren##idm")) {
            auto result = core::legacy::SerializeLegacyIdm(state.legacySpatialIndex, state.legacyIdmPath);
            state.statusMessage = result ? "Legacy-idm exportiert nach: " + std::string(state.legacyIdmPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }
        ImGui::EndDisabled();

        ImGui::TextDisabled("Legacy-Zonen-Metadaten (.aid)");
        UI::InputText("Pfad##aid", state.legacyAidPath, sizeof(state.legacyAidPath));
        if (UI::Button("Importieren##aid")) {
            auto result = core::legacy::ParseLegacyAid(state.legacyAidPath);
            if (result) {
                state.legacyZoneMetadata = *result;
                state.selectedZone = 0;
                state.hasLegacyZoneMetadata = true;
                std::snprintf(state.zoneNameBuf, sizeof(state.zoneNameBuf), "%s", state.legacyZoneMetadata.name.c_str());
                state.statusMessage = "Legacy-aid importiert (Zone '" + state.legacyZoneMetadata.name + "'): " + std::string(state.legacyAidPath);
            } else {
                state.statusMessage = "Import fehlgeschlagen: " + result.error();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.hasLegacyZoneMetadata);
        if (UI::Button("Exportieren##aid")) {
            auto result = core::legacy::SerializeLegacyAid(state.legacyZoneMetadata, state.legacyAidPath);
            state.statusMessage = result ? "Legacy-aid exportiert nach: " + std::string(state.legacyAidPath)
                                          : "Export fehlgeschlagen: " + result.error();
        }
        ImGui::EndDisabled();
        if (state.hasLegacyZoneMetadata) {
            auto& zones = state.legacyZoneMetadata;
            ImGui::Text("Zonen: %zu", zones.AreaCount());
            if (zones.AreaCount() > 0) {
                state.selectedZone = std::clamp(state.selectedZone, 0, static_cast<int>(zones.AreaCount()) - 1);
                if (ImGui::BeginCombo("Zone##aid", zones.Area(state.selectedZone).name.c_str())) {
                    for (std::size_t i = 0; i < zones.AreaCount(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Selectable(zones.Area(i).name.c_str(), state.selectedZone == static_cast<int>(i))) {
                            state.selectedZone = static_cast<int>(i);
                            std::snprintf(state.zoneNameBuf, sizeof(state.zoneNameBuf), "%s", zones.Area(i).name.c_str());
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                auto& area = zones.Area(state.selectedZone);
                if (UI::InputText("Zonenname", state.zoneNameBuf, 33)) area.name = state.zoneNameBuf;
                ImGui::TextDisabled("Datensatztyp %d; %d Werte", area.flag, area.flag == 0 ? 3 : 5);
            }
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

void DrawIconTerrain(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 a[3] = {ImVec2(c.x-r*1.05f,c.y+r*0.75f), ImVec2(c.x-r*0.2f,c.y-r*0.9f), ImVec2(c.x+r*0.45f,c.y+r*0.75f)};
    const ImVec2 b[3] = {ImVec2(c.x-r*0.15f,c.y+r*0.75f), ImVec2(c.x+r*0.55f,c.y-r*0.45f), ImVec2(c.x+r*1.05f,c.y+r*0.75f)};
    dl->AddPolyline(a,3,col,ImDrawFlags_Closed,2.5f); dl->AddPolyline(b,3,col,ImDrawFlags_Closed,2.5f);
}
void DrawIconBrush(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddLine(ImVec2(c.x+r*0.75f,c.y-r*0.85f), ImVec2(c.x-r*0.25f,c.y+r*0.20f), col, 4.5f);
    dl->AddTriangleFilled(ImVec2(c.x-r*0.15f,c.y+r*0.05f), ImVec2(c.x-r*0.95f,c.y+r*0.55f),
                          ImVec2(c.x-r*0.45f,c.y+r*0.95f), col);
}
void DrawIconLayers(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    for(int i=0;i<3;++i){ const float y=c.y-r*0.55f+i*r*0.55f;
        const ImVec2 p[4]={ImVec2(c.x,y-r*0.35f),ImVec2(c.x+r,y),ImVec2(c.x,y+r*0.35f),ImVec2(c.x-r,y)};
        dl->AddPolyline(p,4,col,ImDrawFlags_Closed,2.2f);
    }
}
void DrawIconGrid(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),col,2.0f,0,2.2f);
    for(int i=-1;i<=1;i+=2){ dl->AddLine(ImVec2(c.x+i*r/3,c.y-r),ImVec2(c.x+i*r/3,c.y+r),col,1.7f);
        dl->AddLine(ImVec2(c.x-r,c.y+i*r/3),ImVec2(c.x+r,c.y+i*r/3),col,1.7f); }
}
void DrawIconCube(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 top(c.x,c.y-r), l(c.x-r,c.y-r*0.35f), rr(c.x+r,c.y-r*0.35f);
    const ImVec2 bl(c.x-r,c.y+r*0.65f), br(c.x+r,c.y+r*0.65f), bot(c.x,c.y+r);
    dl->AddLine(top,l,col,2.3f); dl->AddLine(top,rr,col,2.3f); dl->AddLine(l,ImVec2(c.x,c.y+r*0.05f),col,2.3f);
    dl->AddLine(rr,ImVec2(c.x,c.y+r*0.05f),col,2.3f); dl->AddLine(l,bl,col,2.3f); dl->AddLine(rr,br,col,2.3f);
    dl->AddLine(bl,bot,col,2.3f); dl->AddLine(br,bot,col,2.3f); dl->AddLine(ImVec2(c.x,c.y+r*0.05f),bot,col,2.3f);
}
void DrawIconPerson(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x,c.y-r*0.55f),r*0.32f,col);
    dl->AddRectFilled(ImVec2(c.x-r*0.55f,c.y-r*0.05f),ImVec2(c.x+r*0.55f,c.y+r*0.8f),col,r*0.25f);
}
void DrawIconSpawn(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c,r*0.80f,col,24,2.2f); dl->AddCircleFilled(c,r*0.20f,col);
    for(int i=0;i<4;++i){ const float a=0.785398f+i*1.570796f;
        dl->AddCircleFilled(ImVec2(c.x+std::cos(a)*r*0.72f,c.y+std::sin(a)*r*0.72f),r*0.15f,col); }
}
void DrawIconPortal(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddEllipse(c,ImVec2(r*0.82f,r),col,0.0f,28,3.0f);
    dl->AddEllipse(c,ImVec2(r*0.42f,r*0.58f),col,0.0f,24,2.0f);
    dl->AddCircleFilled(c,r*0.12f,col);
}
void DrawIconSave(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),col,2.0f,0,2.2f);
    dl->AddRect(ImVec2(c.x-r*0.55f,c.y-r),ImVec2(c.x+r*0.45f,c.y-r*0.25f),col,1.0f,0,2.0f);
    dl->AddRect(ImVec2(c.x-r*0.55f,c.y+r*0.15f),ImVec2(c.x+r*0.55f,c.y+r*0.75f),col,1.0f,0,2.0f);
}
void DrawIconUndo(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear(); dl->PathArcTo(ImVec2(c.x+r*0.1f,c.y+r*0.15f),r*0.78f,-2.7f,0.65f,24); dl->PathStroke(col,0,2.6f);
    dl->AddTriangleFilled(ImVec2(c.x-r*0.95f,c.y-r*0.1f),ImVec2(c.x-r*0.35f,c.y-r*0.55f),ImVec2(c.x-r*0.35f,c.y+r*0.35f),col);
}
void DrawIconRedo(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear(); dl->PathArcTo(ImVec2(c.x-r*0.1f,c.y+r*0.15f),r*0.78f,2.5f,5.85f,24); dl->PathStroke(col,0,2.6f);
    dl->AddTriangleFilled(ImVec2(c.x+r*0.95f,c.y-r*0.1f),ImVec2(c.x+r*0.35f,c.y-r*0.55f),ImVec2(c.x+r*0.35f,c.y+r*0.35f),col);
}
void DrawIconTable(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    DrawIconGrid(dl,c,r,col);
    dl->AddRectFilled(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y-r*0.48f),col,1.5f);
}
void DrawIconBolt(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const ImVec2 p[6]={ImVec2(c.x+r*0.10f,c.y-r),ImVec2(c.x-r*0.55f,c.y+r*0.05f),ImVec2(c.x-r*0.10f,c.y+r*0.05f),
                       ImVec2(c.x-r*0.25f,c.y+r),ImVec2(c.x+r*0.60f,c.y-r*0.15f),ImVec2(c.x+r*0.12f,c.y-r*0.15f)};
    dl->AddConvexPolyFilled(p,6,col);
}

void DrawIconEye(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear();
    dl->PathLineTo(ImVec2(c.x-r,c.y));
    dl->PathBezierCubicCurveTo(ImVec2(c.x-r*0.5f,c.y-r*0.7f),ImVec2(c.x+r*0.5f,c.y-r*0.7f),ImVec2(c.x+r,c.y));
    dl->PathBezierCubicCurveTo(ImVec2(c.x+r*0.5f,c.y+r*0.7f),ImVec2(c.x-r*0.5f,c.y+r*0.7f),ImVec2(c.x-r,c.y));
    dl->PathStroke(col,ImDrawFlags_Closed,1.8f);
    dl->AddCircleFilled(c,r*0.28f,col);
}
void DrawIconLock(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r*0.72f,c.y-r*0.05f),ImVec2(c.x+r*0.72f,c.y+r*0.85f),col,2.0f,0,1.8f);
    dl->PathClear();
    dl->PathArcTo(ImVec2(c.x,c.y-r*0.05f),r*0.48f,3.14159265f,6.2831853f,16);
    dl->PathStroke(col,0,1.8f);
}
void DrawIconDuplicate(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x-r,c.y-r*0.7f),ImVec2(c.x+r*0.55f,c.y+r*0.85f),col,1.5f,0,1.8f);
    dl->AddRect(ImVec2(c.x-r*0.5f,c.y-r),ImVec2(c.x+r,c.y+r*0.55f),col,1.5f,0,1.8f);
}
void DrawIconDelete(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddLine(ImVec2(c.x-r*0.7f,c.y-r*0.45f),ImVec2(c.x+r*0.7f,c.y-r*0.45f),col,2.0f);
    dl->AddRect(ImVec2(c.x-r*0.52f,c.y-r*0.3f),ImVec2(c.x+r*0.52f,c.y+r*0.85f),col,1.0f,0,1.8f);
    dl->AddLine(ImVec2(c.x-r*0.28f,c.y-r*0.7f),ImVec2(c.x+r*0.28f,c.y-r*0.7f),col,2.0f);
}

using IconDrawFn = void (*)(ImDrawList*, ImVec2, float, ImU32);

bool DrawTinyIconButton(const char* id, IconDrawFn icon, bool active, const char* tooltip,
                        ImVec2 size = ImVec2(22.0f,22.0f)) {
    ImGui::PushID(id);
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton("##tinyIcon",size);
    const bool hovered=ImGui::IsItemHovered();
    ImDrawList* dl=ImGui::GetWindowDrawList();
    if(active || hovered)
        dl->AddRectFilled(p,ImVec2(p.x+size.x,p.y+size.y),
                          active?IM_COL32(12,92,150,210):IM_COL32(28,55,78,190),4.0f);
    if(icon) icon(dl,ImVec2(p.x+size.x*0.5f,p.y+size.y*0.5f),7.0f,
                  active?IM_COL32(125,220,255,255):IM_COL32(180,198,215,240));
    if(hovered && tooltip) ImGui::SetTooltip("%s",tooltip);
    ImGui::PopID();
    return clicked;
}

bool DrawIconButton(const char* id, const char* label, IconDrawFn icon, bool active = false,
                    ImVec2 size = ImVec2(74.0f, 58.0f), bool enabled = true) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton("##iconButton", size);
    ImGui::EndDisabled();
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 bg = active ? IM_COL32(10, 91, 151, 255)
                     : hovered && enabled ? IM_COL32(20, 57, 86, 255)
                                          : IM_COL32(10, 25, 38, 255);
    const ImU32 border = active ? IM_COL32(45, 168, 245, 255) : IM_COL32(28, 54, 76, 255);
    const ImU32 fg = enabled ? (active ? IM_COL32(120, 218, 255, 255) : IM_COL32(214, 229, 244, 255))
                             : IM_COL32(100, 112, 126, 255);
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg, 5.0f);
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), border, 5.0f, 0, active ? 1.7f : 1.0f);
    if (icon) icon(dl, ImVec2(p.x + size.x * 0.5f, p.y + 21.0f), 10.0f, fg);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + size.y - 19.0f), fg, label);
    if (hovered) ImGui::SetTooltip("%s", label);
    ImGui::PopID();
    return clicked && enabled;
}

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
    const bool clicked = UI::Button(T("card.start"), ImVec2(size.x - 20.0f, 34.0f));
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
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(11.0f, 7.0f));

    ImGui::TextColored(ImVec4(0.20f, 0.72f, 1.0f, 1.0f), "NG");
    ImGui::SameLine();
    ImGui::TextUnformatted("NextGen-Editor");
    if (breadcrumbTitle && breadcrumbTitle[0]) {
        ImGui::SameLine();
        ImGui::TextDisabled("/ %s", breadcrumbTitle);
    }
    ImGui::SameLine(205.0f);

    struct PrimaryNav { const char* key; AppScreen target; };
    const PrimaryNav primary[] = {
        {"nav.map", AppScreen::MapEditorLauncher},
        {"nav.data", AppScreen::ShnEditor},
        {"nav.animations", AppScreen::KfmBrowser},
        {"nav.project", AppScreen::ProjectHub},
    };
    for (const auto& item : primary) {
        const bool active =
            (item.target == AppScreen::MapEditorLauncher &&
             (state.screen == AppScreen::MapEditorLauncher || state.screen == AppScreen::MapEditorWorkspace)) ||
            (item.target == AppScreen::ProjectHub &&
             (state.screen == AppScreen::ProjectHub || state.screen == AppScreen::NewProjectConfig)) ||
            state.screen == item.target;
        ImGui::PushStyleColor(ImGuiCol_Button, active ? IM_COL32(10, 91, 151, 255) : IM_COL32(8, 25, 38, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(20, 78, 118, 255));
        if (UI::Button(T(item.key))) state.screen = item.target;
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    }

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (UI::Button(T("nav.new"))) {
        state.project = ProjectConfig{};
        state.screen = AppScreen::NewProjectConfig;
    }
    ImGui::SameLine();
    if (UI::Button(T("nav.open"))) {
#ifdef _WIN32
        if (auto picked = BrowseForFolderWindows("Projekt-Ordner wählen")) {
            std::snprintf(state.project.projectFolder, sizeof(state.project.projectFolder), "%s", picked->c_str());
            TryLoadProjectConfig(state.project);
            state.project.hasProject = true;
            state.statusMessage = "Projekt geladen: " + std::string(state.project.projectFolder);
            state.screen = AppScreen::ProjectHub;
        }
#endif
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.project.hasProject);
    if (UI::Button(T("nav.save"))) {
        std::string err;
        state.statusMessage = SaveProjectConfig(state.project, &err) ? T("newproject.saved")
                                                                      : (T("newproject.savefailed") + err);
    }
    ImGui::EndDisabled();

    const float rightWidth = 130.0f;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - rightWidth);
    if (UI::Button("?##manual")) state.manualOpen = !state.manualOpen;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(64.0f);
    int langIdx = app::CurrentLanguage() == app::Language::German ? 0 : 1;
    const char* langItems[] = {"DE", "EN"};
    if (UI::Combo("##lang", &langIdx, langItems, 2))
        app::SetLanguage(langIdx == 0 ? app::Language::German : app::Language::English);

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
    state.shnInlineEditActive = false;
    state.shnInlineEditFocusPending = false;
    state.shnEditPopupOpen = false;
    state.shnSortColumn = -1;
    state.shnSortAscending = true;
    state.shnColumnFilterFile = -1;
    state.shnColumnFilters.clear();
    state.shnVisibleKey.clear();
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

// Zellstatus-Konstanten für die Grün/Rot-Markierung, siehe EditorState::ShnDocument::cellStatus.
constexpr std::uint8_t kShnCellNormal = 0;
constexpr std::uint8_t kShnCellAutoFilled = 1;   // grün - aus einer anderen Datei übernommen
constexpr std::uint8_t kShnCellNeedsInput = 2;   // rot - braucht noch manuelle Eingabe

void EnsureCellStatusSize(EditorState::ShnDocument& doc);

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

bool ApplyShnCellText(EditorState& state, int document, int row, int column,
                      const std::string& text, bool recordUndo = true) {
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

    if (recordUndo) {
        state.shnUndo.push_back({document, row, column, value, *parsed});
        if (state.shnUndo.size() > 512) state.shnUndo.erase(state.shnUndo.begin());
        state.shnRedo.clear();
    }

    value = std::move(*parsed);
    doc.dirty = true;
    EnsureCellStatusSize(doc);
    doc.cellStatus[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = kShnCellNormal;
    doc.cellDirty[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = 1;
    ++state.shnEditCounter;
    state.shnVisibleKey.clear();
    state.shnStatus = std::string(ShnSourceName(doc.source)) + ": Zelle geändert (noch nicht gespeichert).";
    return true;
}

void UndoShnCellEdit(EditorState& state) {
    if (state.shnUndo.empty()) return;
    const auto edit = state.shnUndo.back();
    state.shnUndo.pop_back();
    if (edit.document < 0 || edit.document >= static_cast<int>(state.shnFiles.size())) return;
    auto& doc = state.shnFiles[static_cast<std::size_t>(edit.document)];
    if (edit.row < 0 || edit.row >= static_cast<int>(doc.file.rows.size()) ||
        edit.column < 0 || edit.column >= static_cast<int>(doc.file.columns.size())) return;
    doc.file.rows[static_cast<std::size_t>(edit.row)].values[static_cast<std::size_t>(edit.column)] = edit.before;
    doc.dirty = true;
    EnsureCellStatusSize(doc);
    doc.cellDirty[static_cast<std::size_t>(edit.row)][static_cast<std::size_t>(edit.column)] = 1;
    state.shnRedo.push_back(edit);
    ++state.shnEditCounter;
    state.shnVisibleKey.clear();
    state.shnSelectedFile = edit.document;
    state.shnSelectedRow = edit.row;
    state.shnSelectedColumn = edit.column;
    state.shnStatus = "SHN-Zelländerung rückgängig gemacht.";
}

void RedoShnCellEdit(EditorState& state) {
    if (state.shnRedo.empty()) return;
    const auto edit = state.shnRedo.back();
    state.shnRedo.pop_back();
    if (edit.document < 0 || edit.document >= static_cast<int>(state.shnFiles.size())) return;
    auto& doc = state.shnFiles[static_cast<std::size_t>(edit.document)];
    if (edit.row < 0 || edit.row >= static_cast<int>(doc.file.rows.size()) ||
        edit.column < 0 || edit.column >= static_cast<int>(doc.file.columns.size())) return;
    doc.file.rows[static_cast<std::size_t>(edit.row)].values[static_cast<std::size_t>(edit.column)] = edit.after;
    doc.dirty = true;
    EnsureCellStatusSize(doc);
    doc.cellDirty[static_cast<std::size_t>(edit.row)][static_cast<std::size_t>(edit.column)] = 1;
    state.shnUndo.push_back(edit);
    ++state.shnEditCounter;
    state.shnVisibleKey.clear();
    state.shnSelectedFile = edit.document;
    state.shnSelectedRow = edit.row;
    state.shnSelectedColumn = edit.column;
    state.shnStatus = "SHN-Zelländerung wiederhergestellt.";
}

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

// Legt in der aktuell ausgewählten Datei eine neue Zeile an (Default-Werte je Spaltentyp) und
// zugleich in allen gerade geladenen Abhängigkeits-Familienmitgliedern (siehe
// FindDependencyPeers) je eine passende neue Zeile - Spalten mit demselben Namen wie in der
// Quelldatei werden aus der neuen Quellzeile übernommen und grün markiert, alle übrigen
// Spalten der Familienmitglieder rot (siehe cellStatus). Mockup-Wunsch, siehe CHANGELOG
// [0.44.14]. Die neue Zeile in der Quelldatei selbst bleibt unmarkiert (normal) - der Nutzer
// legt sie ja gerade bewusst selbst an.
// (Definition weiter unten, nach FindDependencyPeers - siehe dort.)
void AddRowWithPropagation(EditorState& state, int docIndex);

void DrawShnGrid(EditorState& state) {
    if (state.shnSelectedFile < 0 || state.shnSelectedFile >= static_cast<int>(state.shnFiles.size())) {
        ImGui::TextDisabled("Keine SHN-Datei geöffnet.");
        return;
    }

    auto& doc = state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)];
    auto& file = doc.file;
    EnsureCellStatusSize(doc);

    ImGui::TextColored(ShnSourceColor(doc.source), "%s", ShnSourceName(doc.source));
    ImGui::SameLine();
    ImGui::Text("%s", file.FileName().c_str());
    if (doc.dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.20f,0.75f,1.0f,1.0f), "● GEÄNDERT");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu Zeilen · %zu Spalten · V%u · %s",
                        file.rows.size(), file.columns.size(), file.version,
                        file.encrypted ? "verschlüsselt" : "raw");

    if (UI::Button("+ Neue Zeile")) AddRowWithPropagation(state, state.shnSelectedFile);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Legt eine neue Zeile an und propagiert sie – soweit erkannt – in geladene Familienmitglieder.");
    ImGui::SameLine();
    ImGui::BeginDisabled(state.shnSelectedRow < 0 || state.shnSelectedColumn < 0);
    if (UI::Button("Kopieren")) CopySelectedShnCell(state);
    ImGui::SameLine();
    if (UI::Button("Einfügen")) PasteSelectedShnCell(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(state.shnUndo.empty());
    if (UI::Button("Undo")) UndoShnCellEdit(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(state.shnRedo.empty());
    if (UI::Button("Redo")) RedoShnCellEdit(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Doppelklick/F2: Inline · Rechtsklick: Optionen");

    ImGui::Separator();
    const std::string needle = LowerAscii(state.shnSearch);
    ImGui::BeginChild("##shnGrid", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) CopySelectedShnCell(state);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) PasteSelectedShnCell(state);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) UndoShnCellEdit(state);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) RedoShnCellEdit(state);
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
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 55.0f);
        for (const auto& c : file.columns) {
            ImGui::TableSetupColumn(c.name.c_str(),
                                    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortAscending,
                                    std::max(100.0f, std::min(260.0f, 14.0f * static_cast<float>(c.name.size()+2))));
        }
        ImGui::TableHeadersRow();

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

        const std::string visibleKey =
            std::to_string(state.shnSelectedFile) + "|" + needle + "|" +
            (state.shnSearchColumns ? "c" : "-") + (state.shnSearchValues ? "v" : "-") +
            (state.shnFilterActive ? "f" : "-") + "|" + std::to_string(file.rows.size()) + "|" +
            std::to_string(state.shnEditCounter) + "|sort=" + std::to_string(state.shnSortColumn) +
            (state.shnSortAscending ? "a" : "d");

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
                    if (status == kShnCellAutoFilled)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(30, 90, 40, 160));
                    else if (status == kShnCellNeedsInput)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(110, 30, 30, 160));
                    else if (dirtyCell)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(20, 92, 140, 125));

                    const bool selected =
                        state.shnSelectedRow == static_cast<int>(ri) &&
                        state.shnSelectedColumn == static_cast<int>(ci);
                    const bool inlineEditing = selected && state.shnInlineEditActive;

                    ImGui::PushID(static_cast<int>(ri));
                    ImGui::PushID(static_cast<int>(ci));

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
                            ImGui::TextDisabled("%s · Zeile %zu", file.columns[ci].name.c_str(), ri);
                            ImGui::Separator();
                            if (ImGui::MenuItem("Inline bearbeiten", "F2")) {
                                StartShnInlineEdit(state, static_cast<int>(ri), static_cast<int>(ci));
                            }
                            if (ImGui::MenuItem("Erweitert bearbeiten...")) {
                                state.shnEditBuffer = core::legacy::ShnValueToString(row.values[ci]);
                                state.shnEditPopupOpen = true;
                            }
                            ImGui::Separator();
                            if (ImGui::MenuItem("Kopieren", "Strg+C")) CopySelectedShnCell(state);
                            if (ImGui::MenuItem("Einfügen", "Strg+V")) PasteSelectedShnCell(state);
                            ImGui::EndPopup();
                        }

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s\nTyp: %s\nDoppelklick/F2 = Inline bearbeiten",
                                              core::legacy::ShnValueToString(row.values[ci]).c_str(),
                                              file.TypeName(file.columns[ci]).c_str());
                        }
                    }

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
    const auto peers = FindDependencyPeers(state, docIndex);

    // Automatische, freie ID (statt 0): hoechster Wert ueber die Quelldatei UND alle Familien-
    // Dateien + 1, damit die neue Zeile in allen zusammengehoerigen Dateien dieselbe ID bekommt.
    const int idCol = FindShnIdColumn(srcDoc.file);
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
    if (idCol >= 0 && newId >= 0) srcDoc.cellStatus.back()[static_cast<std::size_t>(idCol)] = kShnCellAutoFilled;
    int propagated = 0;
    for (int p : peers) {
        auto& peerDoc = state.shnFiles[static_cast<std::size_t>(p)];
        core::legacy::ShnRow peerRow;
        std::vector<std::uint8_t> peerRowStatus;
        for (auto& peerCol : peerDoc.file.columns) {
            // Spalte mit demselben Namen in der Quelldatei suchen (Groß-/Kleinschreibung exakt,
            // wie auch sonst im Editor üblich) - wenn gefunden, Wert übernehmen und grün
            // markieren, sonst Default-Wert + rot markieren.
            bool matched = false;
            for (std::size_t sc = 0; sc < srcDoc.file.columns.size(); ++sc) {
                if (srcDoc.file.columns[sc].name == peerCol.name && sc < newRow.values.size()) {
                    // Nur übernehmen, wenn der Zieltyp den Quellwert als Text darstellen und
                    // zurückparsen kann - unterschiedliche Spaltentypen gleichen Namens bleiben
                    // sonst einfach rot (braucht manuelle Eingabe).
                    auto asText = core::legacy::ShnValueToString(newRow.values[sc]);
                    auto parsed = core::legacy::ParseShnValue(peerCol, asText);
                    if (parsed) {
                        peerRow.values.push_back(std::move(*parsed));
                        peerRowStatus.push_back(kShnCellAutoFilled);
                        matched = true;
                    }
                    break;
                }
            }
            if (!matched) {
                auto v = core::legacy::ParseShnValue(peerCol, DefaultShnValueText(peerCol));
                peerRow.values.push_back(v ? std::move(*v) : core::legacy::ShnValue(std::uint32_t{0}));
                peerRowStatus.push_back(kShnCellNeedsInput);
            }
        }
        peerDoc.file.rows.push_back(peerRow);
        peerDoc.dirty = true;
        EnsureCellStatusSize(peerDoc);
        peerDoc.cellStatus.back() = peerRowStatus;
        ++propagated;
    }

    ++state.shnEditCounter;
    state.shnSelectedFile = docIndex;
    state.shnSelectedRow = static_cast<int>(newRowIdx);
    state.shnSelectedColumn = 0;
    state.shnStatus = "Neue Zeile angelegt" + (newId >= 0 ? " (freie ID " + std::to_string(newId) + " automatisch vergeben)" : std::string()) + (propagated > 0
        ? (" und in " + std::to_string(propagated) + " Familienmitglied(ern) propagiert (grün=übernommen, rot=braucht Eingabe).")
        : std::string(" (keine geladenen Familienmitglieder gefunden)."));
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
        const auto peers = FindDependencyPeers(state, i);
        if (!peers.empty()) {
            // Gelbes Warndreieck: möglicherweise abhängige Datei(en) gefunden (siehe oben).
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "\xE2\x9A\xA0");
            ImGui::SameLine(0.0f, 3.0f);
        }
        std::string label = doc.file.FileName() + (doc.dirty ? " *" : "");
        if (UI::Selectable((label + "##" + std::to_string(i)).c_str(), selected)) SelectShnDocument(state, i);
        if (ImGui::IsItemHovered()) {
            if (!peers.empty()) {
                std::string tip = doc.file.path.string() + "\n\n\xE2\x9A\xA0 Gleiche Zeilenanzahl (" +
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
    ImGui::Text("Multi SHN Editor");
    ImGui::TextWrapped("Die Aufgabe markiert Kandidaten in CLIENT und SERVER getrennt. Die Markierung basiert auf Dateinamen/Tabellenkontext und ist bewusst eine Kandidatenliste, keine behauptete harte Abhängigkeit.");
    const char* profiles[] = {"Neues Item", "Neuer NPC", "Neuer Mob", "Neuer Skill", "Shop / Preis", "Neue Quest", "XP / Rate"};
    ImGui::SetNextItemWidth(260.0f); UI::Combo("Aufgabe", &state.shnMultiProfile, profiles, static_cast<int>(std::size(profiles)));
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
            if (UI::Selectable(("◆ " + doc.file.FileName() + (doc.dirty ? " *" : "") + "##multi" + std::to_string(i)).c_str(), state.shnSelectedFile == i)) SelectShnDocument(state, i);
            ImGui::PopStyleColor();
            ImGui::SameLine(); ImGui::TextDisabled("Kandidat");
        }
        if (!any) ImGui::TextDisabled("Keine Kandidaten in den geladenen %s-SHN gefunden.", ShnSourceName(source));
        ImGui::EndChild();
    }
    ImGui::TextDisabled("Hinweis: Gelb = prüfen/ggf. ändern. Die tatsächliche Abhängigkeit wird erst als gesichert markiert, wenn sie aus den vorhandenen SHN-/Datenbeziehungen belegt ist.");
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
        auto result = core::legacy::LoadShnFile(it->path());
        if (!result) return -1;
        EditorState::ShnDocument doc;
        doc.file = std::move(*result);
        doc.source = source;
        state.shnFiles.push_back(std::move(doc));
        return static_cast<int>(state.shnFiles.size()) - 1;
    }
    return -1;
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
            for (auto& row : file.rows) {
                if (static_cast<std::size_t>(colIdx) >= row.values.size()) continue;
                long long v;
                if (!ShnValueAsInt(row.values[static_cast<std::size_t>(colIdx)], v)) continue;
                auto scaled = std::llround(static_cast<double>(v) * factor);
                auto parsed = core::legacy::ParseShnValue(file.columns[static_cast<std::size_t>(colIdx)], std::to_string(scaled));
                if (parsed) row.values[static_cast<std::size_t>(colIdx)] = std::move(*parsed);
            }
            doc.dirty = true;
            state.shnStatus = title + std::string(": '") + colName + "' um " + std::to_string(scalePercent) + "% skaliert (noch nicht gespeichert).";
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

// Vorwärtsdeklaration: Definition weiter unten, nach DrawQuestEditor.
void DrawQuestEditor(EditorState& state);
void EnsureMobViewInfoLoaded(EditorState& state); // Definition weiter unten, bei der NPC-3D-Auflösung
void DrawPortalEditor(EditorState& state);

void DrawCustomCreatureEditor(EditorState& state);
void DrawSkillEditor(EditorState& state);

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
    DrawTopNav(state, "Spieldaten");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(8, 20, 31, 255));
    ImGui::BeginChild("##shnEditor", ImVec2(0,0), false);
    ImGui::TextColored(ImVec4(0.40f,0.72f,0.96f,1.0f), "Spieldaten");
    ImGui::SameLine(); ImGui::TextDisabled("SHN · Quest · Portale · Custom NPC/Mob · Skills");
    ImGui::Separator();
    struct DataTool { const char* id; const char* label; IconDrawFn icon; };
    const DataTool dataTools[] = {
        {"single","Single SHN",DrawIconTable},
        {"multi","Multi SHN",DrawIconLayers},
        {"xp","XP Rate",DrawIconBolt},
        {"prices","Preise",DrawIconTable},
        {"quest","Quest",DrawIconBook},
        {"portals","Portale",DrawIconPortal},
        {"creatures","NPC / Mob",DrawIconPerson},
        {"skills","Skills",DrawIconBolt},
    };
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 5.0f));
    for (int i = 0; i < static_cast<int>(std::size(dataTools)); ++i) {
        const auto& tool = dataTools[i];
        if (DrawIconButton(tool.id, tool.label, tool.icon, state.shnSubTab == i, ImVec2(102.0f, 58.0f)))
            state.shnSubTab = i;
        if (i + 1 < static_cast<int>(std::size(dataTools))) ImGui::SameLine();
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    const float leftW = 300.0f;
    const float gap = 8.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const bool rawShnWorkspace = state.shnSubTab <= 3;

    if (rawShnWorkspace) {
        ImGui::BeginChild("##shnLeft", ImVec2(leftW, avail.y), true);
        ImGui::TextColored(ImVec4(0.55f,0.82f,1.0f,1.0f),
                           state.shnSubTab==0 ? "Single SHN Editor" : "SHN Dateien");
        ImGui::Separator();
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
        DrawShnSourceList(state, EditorState::ShnSource::Client, "##shnClientFiles");
        DrawShnSourceList(state, EditorState::ShnSource::Server, "##shnServerFiles");
        if (state.shnSelectedFile>=0 && state.shnSelectedFile<static_cast<int>(state.shnFiles.size())) {
            auto& doc=state.shnFiles[static_cast<std::size_t>(state.shnSelectedFile)];
            auto& f=doc.file;
            if(UI::Button("Speichern",ImVec2(-1,0))) {
                auto r=core::legacy::SaveShnFile(f,f.path);
                state.shnStatus=r?(std::string(ShnSourceName(doc.source))+" gespeichert: "+f.FileName())
                                 :"Speichern fehlgeschlagen: "+r.error();
                if(r) {
                    doc.dirty=false;
                    ClearShnDirtyCells(doc);
                }
            }
            ImGui::Separator();
            UI::InputText("Suche",state.shnSearch,sizeof(state.shnSearch));
            UI::Checkbox("Spalten durchsuchen",&state.shnSearchColumns);
            UI::Checkbox("Werte durchsuchen",&state.shnSearchValues);
            ImGui::TextDisabled("Filter wird %s angewendet.", state.shnFilterActive ? "automatisch" : "nicht");
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
    else { DrawSkillEditor(state); }
    ImGui::EndChild(); ImGui::EndChild(); ImGui::PopStyleColor();
}

void DrawProjectHub(EditorState& state) {
    DrawTopNav(state, "Übersicht");

    ImGui::TextColored(ImVec4(0.74f, 0.86f, 0.96f, 1.0f), "Workspaces");
    ImGui::SameLine();
    ImGui::TextDisabled("Vorhandene Editoren und klar getrennte Erweiterungspunkte");

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float gap = 10.0f;
    const float cardW = std::max(260.0f, (avail.x - gap * 2.0f) / 3.0f);
    const float cardH = std::max(205.0f, (avail.y - gap) * 0.5f);

    enum class HubAction { Map, Data, Quest, Skill, Kfm, Extensions };
    struct CardDef {
        const char* id;
        const char* title;
        std::vector<std::string> features;
        IconDrawFn icon;
        HubAction action;
        bool enabled;
    };
    const CardDef cards[] = {
        {"hub.map", "Karte",
         {"Heightmap & Texturen", "Walk & Block", "Objekte + Sky/Water/GroundObject", "NPCs, Mobs & Portale"},
         DrawIconTerrain, HubAction::Map, true},
        {"hub.data", "Spieldaten",
         {"Single & Multi SHN", "XP Rate / Buy & Sell", "Custom NPC/Mob", "Client + Server Tabellen"},
         DrawIconTable, HubAction::Data, true},
        {"hub.quest", "Quest Editor",
         {"QuestData + QuestDialog", "Ziele & Drops", "Start/Action/Finish Skripte", "Text-ID Auflösung"},
         DrawIconBook, HubAction::Quest, true},
        {"hub.skill", "Skill Editor",
         {"Skills bearbeiten/klonen", "Skill-Stufen", "Animation/Effekt-Auswahl", "Serien skalieren"},
         DrawIconBolt, HubAction::Skill, true},
        {"hub.kfm", "Animationen / KFM",
         {"KFM-Katalog", "Übergänge", "Dateiverweise prüfen", "verlustfreie Kopie exportieren"},
         DrawIconClapper, HubAction::Kfm, true},
        {"hub.extensions", "Erweiterungen",
         {"Interface Editor", "Drop Table Editor", "AI Workspace", "NIF / Material Editing"},
         DrawIconAtom, HubAction::Extensions, false},
    };

    for (int i = 0; i < static_cast<int>(std::size(cards)); ++i) {
        const auto& card = cards[i];
        const bool clicked = DrawEditorCard(card.id, ImVec2(cardW, cardH),
            IM_COL32(8, 24, 36, 255), IM_COL32(9, 42, 63, 255),
            card.icon, card.title, card.features, card.enabled);
        if (clicked) {
            switch (card.action) {
                case HubAction::Map: state.screen = AppScreen::MapEditorLauncher; break;
                case HubAction::Data: state.shnSubTab = 0; state.screen = AppScreen::ShnEditor; break;
                case HubAction::Quest: state.shnSubTab = 4; state.screen = AppScreen::ShnEditor; break;
                case HubAction::Skill: state.shnSubTab = 7; state.screen = AppScreen::ShnEditor; break;
                case HubAction::Kfm: state.screen = AppScreen::KfmBrowser; break;
                case HubAction::Extensions:
                    state.comingSoonTitle = "Erweiterungen";
                    state.screen = AppScreen::ComingSoon;
                    break;
            }
        }
        if (i % 3 != 2) ImGui::SameLine(0.0f, gap);
        else if (i == 2) ImGui::Dummy(ImVec2(0.0f, gap));
    }

    if (!state.statusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }
}



// (Hinweis: die gelben Klebezettel aus den Mockups waren Umsetzungs-Hinweise, keine
// echten UI-Elemente - daher gibt es hier bewusst keine "Sticky Note"-Komponente mehr.)

void DrawNewProjectConfig(EditorState& state) {
    DrawTopNav(state, "Projekt");

    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "PROJEKT KONFIGURIEREN");
    ImGui::SameLine();
    ImGui::TextDisabled("Client, Server und Arbeitsordner einmal zentral festlegen");
    ImGui::Dummy(ImVec2(0,8));

    const float cardW = std::min(820.0f, std::max(560.0f, ImGui::GetContentRegionAvail().x * 0.68f));
    ImGui::BeginChild("##projectConfigCard", ImVec2(cardW, 0), true);

    ImGui::TextDisabled("PROJEKTNAME");
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputText("##projname", state.project.name, sizeof(state.project.name));
    ImGui::TextDisabled("Interner Name des NextGen-Editor-Projekts.");

    ImGui::Dummy(ImVec2(0,6));
    ImGui::TextDisabled("PROJEKTORDNER");
    ImGui::SetNextItemWidth(-44.0f);
    UI::InputText("##projfolder", state.project.projectFolder, sizeof(state.project.projectFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (UI::Button("...##pf", ImVec2(36,0))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.projectfolder")))
            std::snprintf(state.project.projectFolder, sizeof(state.project.projectFolder), "%s", picked->c_str());
    }
#endif
    ImGui::TextDisabled("Hier liegen Projektkonfiguration und später die bearbeiteten Ausgabedateien.");

    ImGui::Dummy(ImVec2(0,6));
    ImGui::TextDisabled("CLIENT");
    ImGui::SetNextItemWidth(-44.0f);
    UI::InputText("##clientfolder", state.project.clientFolder, sizeof(state.project.clientFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (UI::Button("...##cf", ImVec2(36,0))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.clientfolder")))
            std::snprintf(state.project.clientFolder, sizeof(state.project.clientFolder), "%s", picked->c_str());
    }
#endif
    ImGui::TextDisabled("Quelle für resmap, ressystem, reschar, resitem und Client-SHN.");

    ImGui::Dummy(ImVec2(0,6));
    ImGui::TextDisabled("SERVER");
    ImGui::SetNextItemWidth(-44.0f);
    UI::InputText("##serverfolder", state.project.serverFolder, sizeof(state.project.serverFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (UI::Button("...##sf", ImVec2(36,0))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.serverfolder")))
            std::snprintf(state.project.serverFolder, sizeof(state.project.serverFolder), "%s", picked->c_str());
    }
#endif
    ImGui::TextDisabled("Quelle für 9Data/Shine, World, MobRegen, Quest- und Serverdaten.");

    ImGui::Separator();
    const float actionW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
    if (UI::Button(T("newproject.createsave"), ImVec2(actionW, 38.0f))) {
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
    ImGui::SameLine();
    if (UI::Button(T("nav.back"), ImVec2(actionW, 38.0f)))
        state.screen = AppScreen::ProjectHub;

    if (!state.statusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }

    ImGui::EndChild();
}


// Vorwärtsdeklarationen: Definitionen liegen weiter unten, nach GetOrLoadAssetThumbnail (dort
// sind FindResmapRootForAssets & Co. bereits vorhanden) - einzige bewusste Ausnahme vom sonst
// in main.cpp durchgehend eingehaltenen Top-Down-Stil, siehe CHANGELOG [0.44.10].
std::optional<std::filesystem::path> FindResmapRootForAssets(const std::string& clientFolder);
void StartNifThumbnailPrecache(EditorState& state, const std::filesystem::path& resmapRoot);
void AdvanceNifPrecache(EditorState& state, int filesPerFrame);

void DrawMapEditorLauncher(EditorState& state) {
    DrawTopNav(state, "Karte");

    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "KARTEN");
    ImGui::SameLine();
    ImGui::TextDisabled("Neue Karte anlegen oder vorhandene Fiesta-Karte öffnen");
    ImGui::Dummy(ImVec2(0,6));

    const bool onNewMap = state.mapLauncherView == EditorState::MapLauncherView::NewMap;
    const bool onBrowse = state.mapLauncherView == EditorState::MapLauncherView::Browse;
    if (DrawIconButton("launcher.new", "Neue Karte", DrawIconTerrain, onNewMap, ImVec2(108,58)))
        state.mapLauncherView = EditorState::MapLauncherView::NewMap;
    ImGui::SameLine();
    if (DrawIconButton("launcher.open", "Karte öffnen", DrawIconGlobe, onBrowse, ImVec2(108,58)))
        state.mapLauncherView = EditorState::MapLauncherView::Browse;
    ImGui::SameLine();
    if (DrawIconButton("launcher.project", "Projekt", DrawIconPencilPaper, false, ImVec2(92,58)))
        state.screen = AppScreen::ProjectHub;
    ImGui::Separator();

    // Kartenwurzel nur neu scannen, wenn sich der Client-Ordner geändert hat oder der Nutzer
    // explizit einen Rescan anstößt.
    if (state.project.clientFolder[0] != '\0') {
        const std::string clientFolderStr = state.project.clientFolder;
        if (clientFolderStr != state.lastScannedMapRoot) {
            const auto resolution = ResolveMapSearchRootAndScan(state.project.clientFolder);
            state.discoveredMaps = resolution.maps;
            state.lastResmapCandidateCount = resolution.candidateCount;
            state.lastResmapFound = resolution.root.has_value();
            state.lastResmapResolvedPath = resolution.root
                ? (resolution.candidateCount > 1
                       ? (resolution.root->string() + "  (" + std::to_string(resolution.candidateCount) +
                          " resmap-Ordner gefunden; dieser enthält die meisten Karten)")
                       : resolution.root->string())
                : ("Kein resmap-Ordner unter " + clientFolderStr + " gefunden");
            state.lastScannedMapRoot = clientFolderStr;
            state.selectedMapIndex = -1;
            if (resolution.root) StartNifThumbnailPrecache(state, *resolution.root);
        }
    }

    const std::string& resolvedRoot = state.lastResmapResolvedPath;
    const bool resmapFound = state.lastResmapFound;

    if (state.nifPrecacheActive) {
        AdvanceNifPrecache(state, 8);
        const std::size_t totalQ = state.nifPrecacheQueue.size();
        const float frac = totalQ == 0 ? 1.0f
            : static_cast<float>(state.nifPrecacheCursor) / static_cast<float>(totalQ);
        ImGui::BeginChild("##assetPreparation", ImVec2(std::min(680.0f, ImGui::GetContentRegionAvail().x), 150.0f), true);
        ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "ASSET-BIBLIOTHEK");
        ImGui::TextWrapped("NIF-Vorschaubilder werden einmalig vorbereitet. Danach öffnet sich der Asset Browser ohne Erstlade-Ruckler.");
        ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f));
        ImGui::TextDisabled("%zu / %zu Modelle", state.nifPrecacheCursor, totalQ);
        ImGui::EndChild();
        return;
    }

    const float panelW = std::min(760.0f, std::max(520.0f, ImGui::GetContentRegionAvail().x * 0.66f));

    if (onNewMap) {
        ImGui::BeginChild("##createNewMap", ImVec2(panelW, 0), true);
        ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "NEUE KARTE");
        ImGui::SameLine();
        ImGui::TextDisabled("Grunddaten festlegen");
        ImGui::Separator();

        ImGui::TextDisabled("KARTENNAME");
        ImGui::SetNextItemWidth(-1.0f);
        UI::InputText("##newmapname", state.newMapName, sizeof(state.newMapName));

        ImGui::Dummy(ImVec2(0,4));
        ImGui::TextDisabled("HÖHENRASTER");
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f);
        UI::InputInt("Breite##newmapx", &state.newMapWidth);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        UI::InputInt("Höhe##newmapy", &state.newMapHeight);
        state.newMapWidth = std::clamp(state.newMapWidth, 2, 4096);
        state.newMapHeight = std::clamp(state.newMapHeight, 2, 4096);

        ImGui::Dummy(ImVec2(0,4));
        ImGui::TextDisabled("BASIS-LAYER");
        ImGui::SetNextItemWidth(-1.0f);
        UI::InputText("##newmaplayer", state.newMapTextureLayer, sizeof(state.newMapTextureLayer));
        ImGui::TextDisabled("Der Textur-Stack startet mit einem Basis-Layer; weitere Layer werden später im Layer-Dock angelegt.");

        ImGui::Separator();
        ImGui::BeginDisabled(state.newMapName[0] == '\0');
        if (UI::Button("Karte erstellen", ImVec2(-1.0f, 40.0f))) {
            const int w = std::max(2, state.newMapWidth);
            const int h = std::max(2, state.newMapHeight);
            state.heightmap = core::Heightmap(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 50.0f, 50.0f);
            state.undo.Clear();
            state.meshDirty = true;
            SyncWalkGridSize(state);
            state.textureStack = core::TextureLayerStack(1024u, 1024u);
            state.selectedLayer = static_cast<int>(state.textureStack.AddLayer(
                state.newMapTextureLayer[0] != '\0' ? state.newMapTextureLayer : "Base", "base.dds", 1.0f));
            state.layerPreviewDirty = true;
            std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", state.newMapName);
            if (state.project.projectFolder[0] != '\0')
                std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", state.project.projectFolder);
            state.statusMessage = "Neue Karte '" + std::string(state.newMapName) + "' angelegt (" +
                                  std::to_string(w) + "x" + std::to_string(h) + ").";
            state.screen = AppScreen::MapEditorWorkspace;
        }
        ImGui::EndDisabled();
        ImGui::EndChild();
    }

    if (onBrowse) {
        ImGui::BeginChild("##browseMaps", ImVec2(panelW, 0), true);
        ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "KARTE ÖFFNEN");
        ImGui::SameLine();
        ImGui::TextDisabled("%zu gefunden", state.discoveredMaps.size());
        ImGui::Separator();

        if (state.project.clientFolder[0] == '\0') {
            ImGui::TextWrapped("%s", T("mapeditor.noclientfolder"));
            if (UI::Button("Projekt konfigurieren", ImVec2(-1,0)))
                state.screen = AppScreen::NewProjectConfig;
        } else {
            ImGui::TextDisabled("SUCHWURZEL");
            ImGui::TextWrapped("%s", resolvedRoot.empty() ? "(noch nicht gescannt)" : resolvedRoot.c_str());
            if (UI::SmallButton("Neu durchsuchen")) state.lastScannedMapRoot.clear();

            ImGui::Separator();
            ImGui::BeginChild("##mapList", ImVec2(0.0f, std::max(230.0f, ImGui::GetContentRegionAvail().y - 84.0f)), true);
            if (state.discoveredMaps.empty()) {
                ImGui::TextDisabled("%s", resmapFound
                    ? "Keine .ini-Kartendateien im gefundenen resmap-Ordner."
                    : "resmap nicht gefunden – Client-Pfad im Projekt prüfen.");
            }
            for (int i = 0; i < static_cast<int>(state.discoveredMaps.size()); ++i) {
                const bool selected = state.selectedMapIndex == i;
                const auto& map = state.discoveredMaps[static_cast<std::size_t>(i)];
                if (UI::Selectable(map.name.c_str(), selected)) {
                    state.selectedMapIndex = i;
                    std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s", map.iniPath.c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", map.iniPath.c_str());
            }
            ImGui::EndChild();

            ImGui::BeginDisabled(state.selectedMapIndex < 0);
            if (UI::Button("Ausgewählte Karte öffnen", ImVec2(-1.0f, 40.0f))) {
                core::legacy::LegacyMapOpenReport report;
                auto result = core::legacy::OpenLegacyMap(state.legacyMapIniPath, &report);
                if (result) {
                    const std::filesystem::path iniPath(state.legacyMapIniPath);
                    ApplyProjectToState(state, std::move(*result), iniPath.parent_path());
                    if (state.project.projectFolder[0] != '\0')
                        std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", state.project.projectFolder);
                    else
                        std::snprintf(state.legacySaveDir, sizeof(state.legacySaveDir), "%s", iniPath.parent_path().string().c_str());
                    std::snprintf(state.legacySaveStem, sizeof(state.legacySaveStem), "%s", iniPath.stem().string().c_str());
                    state.statusMessage = "Karte geöffnet (" + std::to_string(report.issues.size()) + " Hinweis(e)).";
                    state.screen = AppScreen::MapEditorWorkspace;
                } else {
                    state.statusMessage = "Karte öffnen fehlgeschlagen: " + result.error();
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
    }

    if (!state.statusMessage.empty()) {
        ImGui::Separator();
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
    if (UI::Button(T("nav.back"), ImVec2(120.0f, 34.0f))) {
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

// Lädt rgba (RGBA8, Top-Down, wie von LoadDdsImage/NifEmbeddedTexture geliefert) als kleine,
// nicht gemipmapte GL-Textur hoch - für 32x32-Vorschaubilder lohnt sich Mipmapping nicht.
std::uint32_t UploadThumbnailTexture(const std::vector<std::uint8_t>& rgba, std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || rgba.size() != static_cast<std::size_t>(width) * height * 4) return 0;
    std::uint32_t tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<int>(width), static_cast<int>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// Lädt (einmalig, gecacht in state.assetThumbnails) ein kleines Vorschaubild für einen
// Asset-Picker-Eintrag oder Layer: entweder direkt eine Textur-Datei (isNif=false, z.B. .dds)
// oder die erste Diffuse-Textur eines .nif-Modells (isNif=true - eingebettet über NiPixelData
// ODER extern über den "fieldTexture"-Ordner aufgelöst, siehe ResolveLegacyAssetPath). Gibt ein
// AssetThumbnail zurück (tex==0: kein Vorschaubild, z.B. Mesh ohne Textur oder Ladefehler).
// resmapRoot wird nur für den NIF-Fall gebraucht (externe Textur-Auflösung).
EditorState::AssetThumbnail GetOrLoadAssetThumbnail(EditorState& state,
                                                      const std::filesystem::path& resolvedPath,
                                                      bool isNif,
                                                      const std::filesystem::path& resmapRoot = {}) {
    const std::string key = resolvedPath.string();
    if (auto it = state.assetThumbnails.find(key); it != state.assetThumbnails.end()) {
        return it->second;
    }

    EditorState::AssetThumbnail thumb;
    if (!isNif) {
        if (auto dds = core::LoadDdsImage(resolvedPath)) {
            thumb.tex = UploadThumbnailTexture(dds->rgba, dds->width, dds->height);
            if (thumb.tex && dds->height > 0) thumb.aspect = static_cast<float>(dds->width) / static_cast<float>(dds->height);
        }
    } else if (auto model = core::LoadNifMesh(resolvedPath)) {
        // Zwei-Ebenen-tiefer "Fake"-Kartenordner unter resmapRoot: ResolveLegacyAssetPath leitet
        // den Asset-Root aus mapDir.parent_path().parent_path() her (Layout <AssetRoot>/field/
        // <Karte>/...) - der letzte Pfadbestandteil muss dafür gar nicht existieren, siehe
        // src/core/legacy/LegacyPathResolve.cpp.
        const std::filesystem::path fakeMapDir = resmapRoot / "field" / "_thumbnail";
        for (const auto& part : model->parts) {
            if (part.embeddedDiffuseTexture) {
                thumb.tex = UploadThumbnailTexture(part.embeddedDiffuseTexture->rgba,
                                                    part.embeddedDiffuseTexture->width,
                                                    part.embeddedDiffuseTexture->height);
                if (thumb.tex && part.embeddedDiffuseTexture->height > 0) {
                    thumb.aspect = static_cast<float>(part.embeddedDiffuseTexture->width) /
                                   static_cast<float>(part.embeddedDiffuseTexture->height);
                }
                break;
            }
            if (!part.diffuseTexture.empty()) {
                if (auto texPath = core::legacy::ResolveLegacyAssetPath(fakeMapDir, part.diffuseTexture)) {
                    if (auto dds = core::LoadDdsImage(*texPath)) {
                        thumb.tex = UploadThumbnailTexture(dds->rgba, dds->width, dds->height);
                        if (thumb.tex && dds->height > 0) thumb.aspect = static_cast<float>(dds->width) / static_cast<float>(dds->height);
                        break;
                    }
                }
            }
        }
    }
    return state.assetThumbnails.emplace(key, thumb).first->second;
}

// Startet die einmalige Vorlade-Sequenz aller NIF-Vorschaubilder der GESAMTEN Asset-
// Bibliothek unter resmapRoot (nicht nur der auf einer Karte platzierten Objekte) - siehe
// EditorState::nifPrecacheActive und DrawMapEditorLauncher. Erfasst bewusst ALLE .nif unter
// resmap (field/IDField/KDField/nif(s)/...), weil echte SHMDs diese Quellen mischen.
// Macht nichts, wenn exakt dieser resmapRoot schon einmal komplett durchgelaufen ist.
void StartNifThumbnailPrecache(EditorState& state, const std::filesystem::path& resmapRoot) {
    if (state.nifPrecacheDoneForRoot == resmapRoot.string()) return;
    // SHMD-Verweise liegen nicht nur unter resmap/nif(s), sondern u.a. auch unter field,
    // IDField und KDField. Deshalb die komplette resmap-Hierarchie als Modellbibliothek
    // inventarisieren. Die Vorschaubilder werden weiterhin stückweise pro Frame erzeugt.
    state.availableNifFiles = ListFilesByExtension(resmapRoot, {".nif"});
    state.nifAssetRoot = resmapRoot;
    state.resmapRootForThumbnails = resmapRoot;
    state.nifListScanned = true;
    state.nifPrecacheQueue = state.availableNifFiles;
    state.nifPrecacheCursor = 0;
    state.nifPrecacheActive = !state.nifPrecacheQueue.empty();
    if (!state.nifPrecacheActive) state.nifPrecacheDoneForRoot = resmapRoot.string();
}

// Verarbeitet bis zu filesPerFrame Einträge der Vorlade-Warteschlange (siehe
// StartNifThumbnailPrecache) - bewusst in kleinen Häppchen pro Frame statt einer einzigen
// blockierenden Schleife, damit sich der Fortschrittsbalken sichtbar bewegt. Die
// Gesamtdauer ändert sich dadurch nicht (~4ms/Datei im Schnitt für einen vollen
// LoadNifMesh-Aufruf, siehe CHANGELOG [0.44.8]) - nur, wie fein sie über Frames verteilt
// wird. Bei z.B. 3436 Dateien und 8/Frame sind das ~14s Gesamtdauer, verteilt auf ~430
// Frames statt eines einzigen ~14s-Frames (der wie ein Absturz aussähe).
void AdvanceNifPrecache(EditorState& state, int filesPerFrame) {
    if (!state.nifPrecacheActive) return;
    for (int i = 0; i < filesPerFrame && state.nifPrecacheCursor < state.nifPrecacheQueue.size(); ++i) {
        const std::filesystem::path resolvedPath = state.nifAssetRoot / state.nifPrecacheQueue[state.nifPrecacheCursor];
        GetOrLoadAssetThumbnail(state, resolvedPath, true, state.resmapRootForThumbnails);
        ++state.nifPrecacheCursor;
    }
    if (state.nifPrecacheCursor >= state.nifPrecacheQueue.size()) {
        state.nifPrecacheActive = false;
        state.nifPrecacheDoneForRoot = state.resmapRootForThumbnails.string();
    }
}

// Popup mit Textfilter zur Auswahl einer Datei aus files (relative Pfade). Gibt true zurück
// UND setzt outSelected, wenn der Nutzer in diesem Frame etwas ausgewählt hat. Muss von
// ImGui::OpenPopup(popupId) aus geöffnet werden - zeichnet nur, wenn das Popup offen ist.
// assetRoot: Basis für die relativen Pfade in files (zum Auflösen der Vorschaubilder).
// isNif: steuert, ob ein Eintrag als Textur-Datei oder als .nif-Modell geladen wird.
// resmapRoot: nur für isNif=true gebraucht (externe Textur-Auflösung, siehe GetOrLoadAssetThumbnail).
bool DrawAssetPickerPopup(const char* popupId, const std::vector<std::string>& files,
                           std::string& filterBuf, std::string& outSelected,
                           EditorState& state, const std::filesystem::path& assetRoot, bool isNif,
                           const std::filesystem::path& resmapRoot = {}) {
    bool picked = false;
    ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup(popupId)) {
        char filterCstr[128];
        std::snprintf(filterCstr, sizeof(filterCstr), "%s", filterBuf.c_str());
        if (UI::InputTextWithHint("##filter", "Filtern...", filterCstr, sizeof(filterCstr))) {
            filterBuf = filterCstr;
        }

        // Gefilterte Indizes VORAB bestimmen (reine String-Vergleiche, kein Datei-I/O) - erst
        // danach per ImGuiListClipper nur die tatsächlich sichtbaren Zeilen zeichnen. Nötig,
        // damit Vorschaubilder unten NUR für sichtbare Einträge geladen werden, nicht für die
        // komplette (ggf. tausende Einträge lange) Liste auf einen Schlag.
        std::vector<std::size_t> matching;
        matching.reserve(files.size());
        std::string filterLower = filterBuf;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (std::size_t i = 0; i < files.size(); ++i) {
            if (filterLower.empty()) { matching.push_back(i); continue; }
            std::string fLower = files[i];
            std::transform(fLower.begin(), fLower.end(), fLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (fLower.find(filterLower) != std::string::npos) matching.push_back(i);
        }

        ImGui::Text("%zu Datei(en)", matching.size());
        ImGui::Separator();
        ImGui::BeginChild("##assetList", ImVec2(0.0f, 340.0f), true);
        constexpr float kThumbSize = 32.0f;
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(matching.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const std::string& f = files[matching[static_cast<std::size_t>(row)]];
                ImGui::PushID(row);
                const std::filesystem::path resolvedPath = assetRoot / f;
                const auto thumb = GetOrLoadAssetThumbnail(state, resolvedPath, isNif, resmapRoot);
                if (thumb.tex) {
                    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(thumb.tex)), ImVec2(kThumbSize, kThumbSize));
                } else {
                    ImGui::Dummy(ImVec2(kThumbSize, kThumbSize)); // Platzhalter - hält die Zeilen bündig
                }
                ImGui::SameLine();
                if (UI::Selectable(f.c_str(), false, 0, ImVec2(0.0f, kThumbSize))) {
                    outSelected = f;
                    picked = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        if (UI::Button("Abbrechen")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return picked;
}

// Werkzeug-Inhalt für den aktuell aktiven Modus (siehe EditMode) - der Moduswechsel selbst
// erfolgt jetzt über den Tab-Balken des Arbeitsbereichs (siehe DrawWorkspaceTabBar), nicht
// mehr über Radio-Buttons hier. Wird OHNE eigenes Fenster aufgerufen (eingebettet in die
// "Tools/etc"-Spalte, siehe DrawMapEditorWorkspace) - daher kein ImGui::Begin/End mehr.
// Lädt World/NPC.txt (alle Karten in einer Datei) einmalig aus state.shineTextRoot - dieselbe
// Datei bleibt für die ganze Sitzung geladen, gefiltert wird erst bei der Anzeige (siehe
// NpcRecordsForCurrentMap). Speichern erfolgt minimal-invasiv über SaveShineTextFile.
void EnsureNpcTextLoaded(EditorState& state) {
    if (state.npcTextLoaded || state.shineTextRoot.empty()) return;
    auto path = std::filesystem::path(state.shineTextRoot) / "World" / "NPC.txt";
    auto result = core::legacy::LoadShineTextFile(path);
    if (result) { state.npcTextFile = std::move(*result); state.npcTextLoaded = true; }
    else state.statusMessage = "NPC.txt: " + result.error();
}

// Indizes der "ShineNPC"-Records, deren Map-Spalte (Index 1) exakt der aktuellen Karte
// entspricht (state.legacySaveStem) - Map-Spaltenwerte in NPC.txt entsprechen echten Karten-
// Ordnernamen (z.B. "Rou", "RouN", "EldCem01"), byte-exakt gegen die echten NA2016-Daten
// geprüft, siehe docs (CHANGELOG [0.44.15]).
std::vector<std::size_t> NpcRecordsForCurrentMap(EditorState& state) {
    std::vector<std::size_t> out;
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return out;
    for (std::size_t i = 0; i < table->records.size(); ++i) {
        if (table->records[i].values.size() > 1 && table->records[i].values[1] == state.legacySaveStem) {
            out.push_back(i);
        }
    }
    return out;
}

// Lädt MobRegen/<Karte>.txt neu, wenn die Karte gewechselt hat (Dateiname = Kartenname, 1:1,
// anders als bei NPC.txt wo alle Karten in einer Datei stehen).
void EnsureMobRegenLoaded(EditorState& state) {
    if (state.shineTextRoot.empty()) return;
    if (state.mobRegenTextLoaded && state.mobRegenLoadedForMap == state.legacySaveStem) return;
    auto path = std::filesystem::path(state.shineTextRoot) / "MobRegen" / (std::string(state.legacySaveStem) + ".txt");
    auto result = core::legacy::LoadShineTextFile(path);
    if (result) {
        state.mobRegenTextFile = std::move(*result);
        state.mobRegenTextLoaded = true;
        state.mobRegenLoadedForMap = state.legacySaveStem;
    } else {
        state.mobRegenTextLoaded = false;
        state.statusMessage = std::string("MobRegen/") + state.legacySaveStem + ".txt: " + result.error();
    }
}

void EnsureShopTextLoaded(EditorState& state, const std::string& npcName) {
    if (state.shopTextLoaded && state.shopLoadedForNpc == npcName) return;
    auto path = std::filesystem::path(state.shineTextRoot) / "NPCItemList" / (npcName + ".txt");
    state.shopFileIsNew = false;
    state.shopLoadedForNpc = npcName;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        // Noch keine Shop-Datei - der Editor bietet an, eine anzulegen.
        state.shopTextLoaded = false;
        return;
    }
    auto result = core::legacy::LoadShineTextFile(path);
    if (result) {
        state.shopTextFile = std::move(*result);
        state.shopTextLoaded = true;
    } else {
        state.shopTextLoaded = false;
        state.statusMessage = "NPCItemList/" + npcName + ".txt: " + result.error();
    }
}

// Popup: Händler-Inventar (NPCItemList/<NPC>.txt) - mehrere Tabs (Kategorien), je bis zu 6
// Item-Slots pro Zeile, siehe docs (CHANGELOG [0.44.15]). Werte sind Item-InxNames aus
// ItemInfo.shn (Freitext hier, kein Abgleich gegen ItemInfo - "-" bedeutet leerer Slot).
// Zerlegt den rohen Dialog-Text (NpcDialogData.shn, Spalte "Dialog") in Begrüßungstext +
// Button-Liste (Label, Aktion) - Format byte-exakt gegen echte NA2016-Daten verifiziert
// (siehe CHANGELOG [0.44.16]):
//   <Begrüßungstext, kann [NAME] als Platzhalter für den Spielernamen enthalten>
//   [BUTTON_NPC]=[Label][Aktion]
//   [BUTTON_NPC]=[Label][Aktion]
// Aktionen sind z.B. "server_ack <Zahl>" (Server-Callback, u.a. Kauf/Bestätigen) oder
// "opendlg <anderer-Dialog-Name>" (verketteter Dialog, z.B. zu einem Aufwertungs-Menü).
struct ParsedNpcDialog {
    std::string greeting;
    std::vector<std::pair<std::string, std::string>> buttons;
};

std::vector<std::string> ExtractBracketed(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '[') {
            std::size_t end = s.find(']', i);
            if (end == std::string::npos) break;
            out.push_back(s.substr(i + 1, end - i - 1));
            i = end + 1;
        } else {
            ++i;
        }
    }
    return out;
}

ParsedNpcDialog ParseNpcDialogText(const std::string& raw) {
    ParsedNpcDialog out;
    std::istringstream iss(raw);
    std::string line;
    std::vector<std::string> greetingLines;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("[BUTTON_NPC]=", 0) == 0) {
            auto parts = ExtractBracketed(line.substr(13));
            if (parts.size() >= 2) out.buttons.emplace_back(parts[0], parts[1]);
            else if (parts.size() == 1) out.buttons.emplace_back("", parts[0]); // nur Aktion, kein Label (Client zeigt vermutlich einen Standard-Button wie "Weiter") - byte-exakt gegen echte Daten verifiziert (z.B. RouGaianMaria)
        } else {
            greetingLines.push_back(line);
        }
    }
    while (!greetingLines.empty() && greetingLines.back().empty()) greetingLines.pop_back();
    for (std::size_t i = 0; i < greetingLines.size(); ++i) {
        if (i) out.greeting += "\n";
        out.greeting += greetingLines[i];
    }
    return out;
}

std::string SerializeNpcDialogText(const ParsedNpcDialog& d) {
    std::string out = d.greeting;
    for (auto& [label, action] : d.buttons) {
        if (label.empty()) out += "\n[BUTTON_NPC]=[" + action + "]"; // siehe ParseNpcDialogText
        else out += "\n[BUTTON_NPC]=[" + label + "][" + action + "]";
    }
    return out;
}

// Sucht den ressystem-Ordner (enthält NpcDialogData.shn) unter dem Projekt-Client-Ordner, falls
// noch nicht gesetzt - derselbe Ordner, den auch der SHN-Editor für Client-Dateien nutzen würde,
// hier aber unabhängig ermittelt (siehe EditorState::npcDialogRessystemRoot).
void EnsureNpcDialogRoot(EditorState& state) {
    if (!state.npcDialogRessystemRoot.empty()) return;
    if (auto found = FindNamedSubfolder(state.project.clientFolder, {"ressystem"})) {
        state.npcDialogRessystemRoot = found->string();
    }
}

// Lädt QuestData.shn (Server) einmalig aus state.shnServerRoot - eigenes Format, siehe
// QuestData.hpp/CHANGELOS [0.44.19].
void EnsureQuestDataLoaded(EditorState& state) {
    if (state.questDataLoaded || state.shnServerRoot.empty()) return;
    auto path = std::filesystem::path(state.shnServerRoot) / "QuestData.shn";
    {
        // Ordner war evtl. ein Elternordner von Shine (z.B. Server/9Data) - dann rekursiv suchen.
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            if (auto shine = FindServerShineRoot(state.shnServerRoot)) path = *shine / "QuestData.shn";
        }
    }
    auto result = core::legacy::LoadQuestData(path);
    if (result) { state.questDataFile = std::move(*result); state.questDataLoaded = true; }
    else state.statusMessage = "QuestData.shn: " + result.error();
}

// Lädt QuestDialog.shn (Client, ressystem/) zur Auflösung der Text-IDs in title/description
// und der SAY-Referenzen in den Skripten - selber Ordner wie NpcDialogData.shn.
void EnsureQuestDialogLoaded(EditorState& state) {
    if (state.questDialogLoaded) return;
    EnsureNpcDialogRoot(state);
    if (state.npcDialogRessystemRoot.empty()) return;
    auto path = std::filesystem::path(state.npcDialogRessystemRoot) / "QuestDialog.shn";
    auto result = core::legacy::LoadShnFile(path);
    if (result) { state.questDialogShn = std::move(*result); state.questDialogLoaded = true; }
}

// Löst eine Text-ID über QuestDialog.shn auf (Spalte 0 = ID, Spalte 1 = Dialog) - byte-exakt
// gegen echte Daten geprüft, siehe CHANGELOG [0.44.19] (z.B. ID 200 = Quest-1-Titel "Baby
// Steps", ID 202 = die zugehörige NPC-Begrüßungszeile mit [BUTTON]=[...]-Markup).
std::string ResolveQuestText(EditorState& state, int textId) {
    if (!state.questDialogLoaded) return {};
    for (auto& row : state.questDialogShn.rows) {
        if (row.values.empty()) continue;
        long long id = 0;
        if (!ShnValueAsInt(row.values[0], id)) continue;
        if (id == textId) return row.values.size() > 1 ? core::legacy::ShnValueToString(row.values[1]) : std::string();
    }
    return {};
}

// Lädt ItemInfo.shn (Server bevorzugt, sonst Client) für die Item-Namensauflösung im Quest-
// Editor - siehe CHANGELOG [0.44.21].
void EnsureItemInfoLoadedForQuests(EditorState& state) {
    if (state.itemInfoLoaded) return;
    for (auto src : {EditorState::ShnSource::Server, EditorState::ShnSource::Client}) {
        const std::string& root = src == EditorState::ShnSource::Client ? state.shnClientRoot : state.shnServerRoot;
        if (root.empty()) continue;
        auto path = std::filesystem::path(root) / "ItemInfo.shn";
        auto result = core::legacy::LoadShnFile(path);
        if (result) { state.itemInfoShn = std::move(*result); state.itemInfoLoaded = true; return; }
    }
}

// Liefert "<InxName> (#<ID>)" wenn gefunden, sonst "?? ID <id> nicht gefunden" (rot markiert
// vom Aufrufer) - dient zugleich als einfache Validierung von Item-Referenzen. id==0 gilt als
// "kein Item" (nicht als Fehler gewertet), da 0 in den Quest-Feldern durchgängig "leer/
// deaktiviert" bedeutet.
std::pair<std::string, bool> ResolveItemNameForQuest(EditorState& state, int id) {
    if (id == 0) return {"-", true};
    EnsureItemInfoLoadedForQuests(state);
    if (state.itemInfoLoaded) {
        for (auto& row : state.itemInfoShn.rows) {
            if (row.values.empty()) continue;
            long long rid = 0;
            if (ShnValueAsInt(row.values[0], rid) && rid == id) {
                std::string name = row.values.size() > 1 ? core::legacy::ShnValueToString(row.values[1]) : "?";
                return {name, true};
            }
        }
    }
    return {"nicht gefunden", false};
}

// Dito für Mob-/NPC-IDs über das ohnehin für die NPC-3D-Auflösung geladene MobViewInfo.shn
// (siehe EnsureMobViewInfoLoaded) - selbe ID-Basis für Monster UND benannte NPCs.
std::pair<std::string, bool> ResolveMobNameForQuest(EditorState& state, int id) {
    if (id == 0) return {"-", true};
    EnsureMobViewInfoLoaded(state);
    if (state.mobViewInfoLoaded) {
        for (auto& row : state.mobViewInfoShn.rows) {
            if (row.values.empty()) continue;
            long long rid = 0;
            if (ShnValueAsInt(row.values[0], rid) && rid == id) {
                std::string name = row.values.size() > 1 ? core::legacy::ShnValueToString(row.values[1]) : "?";
                return {name, true};
            }
        }
    }
    return {"nicht gefunden", false};
}

// Zeigt eine ID + ihren aufgelösten Namen (grün=gefunden, rot=nicht gefunden) direkt hinter
// einem InputInt - siehe CHANGELOG [0.44.21] ("Validierung" + "echten Namen zeigen" in einem).
void DrawResolvedIdLabel(const std::pair<std::string, bool>& resolved) {
    ImGui::SameLine();
    if (resolved.second) ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "-> %s", resolved.first.c_str());
    else ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "-> %s", resolved.first.c_str());
}

// Text zu einer Quest-Text-ID; leer, wenn unbekannt oder nur ein Platzhalter aus Strichen ("-----").
static std::string QuestTextOf(EditorState& state, int textId) {
    if (!state.questDialogLoaded) return {};
    if (!state.questTextMapBuilt) {
        state.questTextMapBuilt = true;
        state.questTextMap.clear();
        for (const auto& row : state.questDialogShn.rows) {
            if (row.values.size() < 2) continue;
            long long id = 0;
            if (!ShnValueAsInt(row.values[0], id)) continue;
            state.questTextMap.emplace(static_cast<int>(id), core::legacy::ShnValueToString(row.values[1]));
        }
    }
    const auto it = state.questTextMap.find(textId);
    if (it == state.questTextMap.end()) return {};
    const std::string& t = it->second;
    if (t.find_first_not_of("- \t") == std::string::npos) return {}; // nur Striche/Leerraum
    return t;
}

void DrawQuestEditor(EditorState& state) {
    EnsureQuestDataLoaded(state);
    EnsureQuestDialogLoaded(state);
    if (!state.questDataLoaded) {
        if (state.shnServerRoot.empty()) {
            ImGui::TextWrapped("Kein Server-Ordner bekannt - in den Projekt-Einstellungen den Server-Ordner setzen (Server/9Data/Shine wird automatisch gefunden).");
        } else {
            ImGui::TextWrapped("QuestData.shn konnte unter '%s' nicht geladen werden.", state.shnServerRoot.c_str());
        }
        return;
    }
    auto& quests = state.questDataFile.records;
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "QUEST EDITOR");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu Quests%s", quests.size(),
                        state.questDialogLoaded ? "" : " · QuestDialog fehlt");
    const float questSaveW = 190.0f;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8.0f,
                             ImGui::GetWindowContentRegionMax().x - questSaveW));
    if (UI::Button("QuestData speichern", ImVec2(questSaveW, 0))) {
        auto path = std::filesystem::path(state.shnServerRoot) / "QuestData.shn";
        auto saved = core::legacy::SaveQuestData(state.questDataFile, path);
        state.statusMessage = saved ? std::string("QuestData.shn gespeichert.") : "Fehler: " + saved.error();
    }
    ImGui::Separator();
    ImGui::SetNextItemWidth(430.0f);
    UI::InputTextWithHint("##questsearch", "Quest-ID oder Titeltext suchen...",
                          state.questSearch, sizeof(state.questSearch));

    // Listen-Beschriftungen und Filter nur bei Aenderung neu berechnen.
    const std::string key = std::string(state.questSearch) + "|" + std::to_string(quests.size()) + "|" +
                            (state.questDialogLoaded ? "d" : "-") + "|" + std::to_string(state.questRevision);
    if (key != state.questListKey) {
        state.questListKey = key;
        state.questLabels.assign(quests.size(), std::string());
        state.questVisible.clear();
        const std::string needle = LowerAscii(state.questSearch);
        for (std::size_t i = 0; i < quests.size(); ++i) {
            std::string title = QuestTextOf(state, quests[i].title);
            if (title.empty()) title = QuestTextOf(state, quests[i].description); // viele Quests haben nur eine Beschreibung
            if (title.empty()) title = "(ohne Text)";
            state.questLabels[i] = "#" + std::to_string(quests[i].id) + "  " + title;
            if (needle.empty() || LowerAscii(state.questLabels[i]).find(needle) != std::string::npos) state.questVisible.push_back(i);
        }
    }

    const float listWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.30f, 310.0f, 430.0f);
    ImGui::BeginChild("QuestList", ImVec2(listWidth, 0.0f), true);
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "QUESTS");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu sichtbar", state.questVisible.size());
    ImGui::Separator();
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(state.questVisible.size()));
        while (clipper.Step()) {
            for (int vi = clipper.DisplayStart; vi < clipper.DisplayEnd; ++vi) {
                const std::size_t i = state.questVisible[static_cast<std::size_t>(vi)];
                if (UI::Selectable((state.questLabels[i] + "##q" + std::to_string(i)).c_str(), state.selectedQuestIdx == static_cast<int>(i))) {
                    state.selectedQuestIdx = static_cast<int>(i);
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("QuestDetail", ImVec2(0.0f, 0.0f), true);
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "EIGENSCHAFTEN");
    ImGui::Separator();
    if (state.selectedQuestIdx < 0 || static_cast<std::size_t>(state.selectedQuestIdx) >= quests.size()) {
        ImGui::TextDisabled("Quest links auswählen.");
        ImGui::EndChild();
        return;
    }
    auto& q = quests[static_cast<std::size_t>(state.selectedQuestIdx)];
    const ImVec4 kOk(0.50f, 0.90f, 0.50f, 1.0f), kBad(1.0f, 0.40f, 0.40f, 1.0f), kDim(0.60f, 0.66f, 0.74f, 1.0f);

    // Kleine Bausteine fuer eine ordentliche Formular-Optik: feste Labelspalte, kompakte Felder,
    // aufgeloeste Namen in der dritten Spalte (Umbruch innerhalb der Spalte).
    auto beginForm = [&](const char* id) {
        if (!ImGui::BeginTable(id, 3, ImGuiTableFlags_SizingFixedFit)) return false;
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 210.0f);
        ImGui::TableSetupColumn("##field", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("##info", ImGuiTableColumnFlags_WidthStretch);
        return true;
    };
    auto rowLabel = [&](const char* label) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(110.0f);
    };
    auto infoText = [&](const std::string& text, bool ok, bool known = true) {
        ImGui::TableSetColumnIndex(2);
        ImGui::AlignTextToFramePadding();
        if (text.empty()) ImGui::TextColored(kDim, "%s", known ? "-" : "?");
        else ImGui::TextColored(ok ? kOk : kBad, "%s", text.c_str());
    };
    auto u16Row = [&](const char* label, const char* id, std::uint16_t& value, int maxValue = 65535) {
        rowLabel(label);
        int v = value;
        if (UI::InputInt(id, &v, 0, 0)) { value = static_cast<std::uint16_t>(std::clamp(v, 0, maxValue)); ++state.questRevision; }
    };

    ImGui::Text("Quest #%d", q.id);
    ImGui::SameLine();
    ImGui::TextColored(kDim, "%s", QuestTextOf(state, q.title).c_str());
    ImGui::Separator();

    if (beginForm("##qmain")) {
        u16Row("Quest-ID", "##id", q.id);
        u16Row("Titel-Text-ID", "##title", q.title);
        { const std::string t = QuestTextOf(state, q.title); infoText(t.empty() ? "(kein Text)" : t, !t.empty()); }
        u16Row("Beschreibung-Text-ID", "##desc", q.description);
        { const std::string t = QuestTextOf(state, q.description); infoText(t.empty() ? "(kein Text)" : t, !t.empty()); }
        rowLabel("Mindest-Level");
        { int v = q.minLevel; if (UI::InputInt("##minlv", &v, 0, 0)) q.minLevel = static_cast<std::uint8_t>(std::clamp(v, 0, 255)); }
        rowLabel("Maximal-Level");
        { int v = q.maxLevel; if (UI::InputInt("##maxlv", &v, 0, 0)) q.maxLevel = static_cast<std::uint8_t>(std::clamp(v, 0, 255)); }
        u16Row("Start-NPC (Mob-ID)", "##startnpc", q.startingNpc);
        { auto r = ResolveMobNameForQuest(state, q.startingNpc); infoText(r.first == "-" ? std::string() : r.first, r.second); }
        rowLabel("Aktiviert");
        { bool enable = q.enableQuest != 0; if (UI::Checkbox("##enable", &enable)) q.enableQuest = enable ? 1 : 0; }
        rowLabel("Tägliche Quest");
        { bool daily = q.dailyQuest != 0; if (UI::Checkbox("##daily", &daily)) q.dailyQuest = daily ? 1 : 0; }
        if (q.needItem != 0) {
            u16Row("Benötigtes Item (ID)", "##reqitem", q.itemId);
            auto r = ResolveItemNameForQuest(state, q.itemId); infoText(r.first == "-" ? std::string() : r.first, r.second);
        }
        if (q.needPred != 0) {
            u16Row("Vorgänger-Quest (ID)", "##pred", q.predecessor);
            bool found = false;
            std::string predTitle;
            for (std::size_t oi = 0; oi < quests.size(); ++oi) if (quests[oi].id == q.predecessor) { found = true; predTitle = QuestTextOf(state, quests[oi].title); break; }
            infoText(found ? (predTitle.empty() ? std::string("gefunden") : predTitle) : std::string("Quest-ID nicht gefunden"), found);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (UI::CollapsingHeader("Monster-Ziele", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##qmobs", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Aktiv", ImGuiTableColumnFlags_WidthFixed, 46.0f);
            ImGui::TableSetupColumn("Mob-ID", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Anzahl", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##sp", ImGuiTableColumnFlags_WidthFixed, 1.0f);
            ImGui::TableHeadersRow();
            for (std::size_t mi = 0; mi < q.mobs.size(); ++mi) {
                ImGui::PushID(static_cast<int>(mi));
                auto& m = q.mobs[mi];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); bool active = m.active != 0; if (UI::Checkbox("##a", &active)) m.active = active ? 1 : 0;
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(80.0f); int mid = m.id; if (UI::InputInt("##id", &mid, 0, 0)) m.id = static_cast<std::uint16_t>(std::clamp(mid, 0, 65535));
                ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(70.0f); int amt = m.amount; if (UI::InputInt("##n", &amt, 0, 0)) m.amount = static_cast<std::uint8_t>(std::clamp(amt, 0, 255));
                ImGui::TableSetColumnIndex(3);
                if (m.active != 0 || m.id != 0) { auto r = ResolveMobNameForQuest(state, m.id); ImGui::TextColored(r.second ? kOk : kBad, "%s", r.first.c_str()); }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    if (UI::CollapsingHeader("Item-Ziele", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##qitems", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Aktiv", ImGuiTableColumnFlags_WidthFixed, 46.0f);
            ImGui::TableSetupColumn("Item-ID", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Anzahl", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##sp", ImGuiTableColumnFlags_WidthFixed, 1.0f);
            ImGui::TableHeadersRow();
            for (std::size_t ii = 0; ii < q.items.size(); ++ii) {
                ImGui::PushID(static_cast<int>(ii) + 100);
                auto& it = q.items[ii];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); bool active = it.active != 0; if (UI::Checkbox("##a", &active)) it.active = active ? 1 : 0;
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(80.0f); int itemId = it.id; if (UI::InputInt("##id", &itemId, 0, 0)) it.id = static_cast<std::uint16_t>(std::clamp(itemId, 0, 65535));
                ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(70.0f); int amt = it.amount; if (UI::InputInt("##n", &amt, 0, 0)) it.amount = static_cast<std::uint16_t>(std::clamp(amt, 0, 65535));
                ImGui::TableSetColumnIndex(3);
                if (it.active != 0 || it.id != 0) { auto r = ResolveItemNameForQuest(state, it.id); ImGui::TextColored(r.second ? kOk : kBad, "%s", r.first.c_str()); }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    if (UI::CollapsingHeader("Drops", ImGuiTreeNodeFlags_DefaultOpen)) {
        int removeIdx = -1;
        if (ImGui::BeginTable("##qdrops", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Mob-ID", ImGuiTableColumnFlags_WidthFixed, 86.0f);
            ImGui::TableSetupColumn("Item-ID", ImGuiTableColumnFlags_WidthFixed, 86.0f);
            ImGui::TableSetupColumn("Menge", ImGuiTableColumnFlags_WidthFixed, 66.0f);
            ImGui::TableSetupColumn("Rate", ImGuiTableColumnFlags_WidthFixed, 76.0f);
            ImGui::TableSetupColumn("Mob", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, 26.0f);
            ImGui::TableHeadersRow();
            for (std::size_t di = 0; di < q.drops.size(); ++di) {
                ImGui::PushID(static_cast<int>(di) + 200);
                auto& d = q.drops[di];
                int mobId = static_cast<int>(d.mobId), itemId = static_cast<int>(d.itemId), amount = static_cast<int>(d.amount), rate = static_cast<int>(d.rate);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::SetNextItemWidth(76.0f); if (UI::InputInt("##m", &mobId, 0, 0)) d.mobId = static_cast<std::uint32_t>(std::max(0, mobId));
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(76.0f); if (UI::InputInt("##i", &itemId, 0, 0)) d.itemId = static_cast<std::uint32_t>(std::max(0, itemId));
                ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(56.0f); if (UI::InputInt("##a", &amount, 0, 0)) d.amount = static_cast<std::uint32_t>(std::max(0, amount));
                ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(66.0f); if (UI::InputInt("##r", &rate, 0, 0)) d.rate = static_cast<std::uint32_t>(std::max(0, rate));
                auto mobName = ResolveMobNameForQuest(state, mobId);
                auto itemName = ResolveItemNameForQuest(state, itemId);
                ImGui::TableSetColumnIndex(4); ImGui::TextColored(mobName.second ? kOk : kBad, "%s", mobName.first.c_str());
                ImGui::TableSetColumnIndex(5); ImGui::TextColored(itemName.second ? kOk : kBad, "%s", itemName.first.c_str());
                ImGui::TableSetColumnIndex(6); if (UI::SmallButton("X")) removeIdx = static_cast<int>(di);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeIdx >= 0) q.drops.erase(q.drops.begin() + removeIdx);
        if (q.drops.size() < 11 && UI::Button("+ Drop hinzufügen")) q.drops.push_back({});
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Belohnungen (144 Byte, Struktur noch nicht kartiert) - werden beim Speichern unverändert übernommen.");
    ImGui::Separator();
    ImGui::Text("Skripte (SAY / IF / GOTO / ACCEPT / CREATE_ITEM / ...)");
    auto scriptEditor = [&](const char* label, core::legacy::QuestScript& script) {
        ImGui::TextColored(kDim, "%s", label);
        std::vector<char> buf(script.text.begin(), script.text.end());
        buf.resize(std::max<std::size_t>(buf.size() + 1, 4096));
        if (ImGui::InputTextMultiline((std::string("##script") + label).c_str(), buf.data(), buf.size(), ImVec2(-1.0f, 110.0f))) {
            script.text.assign(buf.data());
        }
    };
    scriptEditor("Start", q.start);
    scriptEditor("Action", q.action);
    scriptEditor("Finish", q.finish);

    ImGui::Separator();
    {
        static char lookupBuf[16] = "";
        ImGui::SetNextItemWidth(90.0f);
        UI::InputText("SAY-Text-ID nachschlagen", lookupBuf, sizeof(lookupBuf), ImGuiInputTextFlags_CharsDecimal);
        if (lookupBuf[0] != '\0') {
            const std::string t = QuestTextOf(state, std::atoi(lookupBuf));
            ImGui::TextWrapped("-> %s", t.empty() ? "(kein Text)" : t.c_str());
        }
    }
    ImGui::EndChild();
}

// Lädt TownPortal.shn (Client, ressystem/) - die Skill-/Schriftrollen-Schnellreiseliste mit
// auswählbarem Ziel (siehe CHANGELOG [0.44.20], Nutzer-Klarstellung zur Portal-Taxonomie).
// Server-Ordner (Server/9Data/Shine) fuer Portal-Daten: derselbe Ordner wie shineTextRoot (Map-
// Editor-Tabs NPCs/Mobs) bzw. shnServerRoot (SHN-Editor) - je nachdem, welcher gesetzt ist.
std::string PortalServerRoot(const EditorState& state) {
    return !state.shineTextRoot.empty() ? state.shineTextRoot : state.shnServerRoot;
}

void EnsureTownPortalLoaded(EditorState& state) {
    if (state.townPortalLoaded) return;
    EnsureNpcDialogRoot(state);
    // Client- und Server-Kopie von TownPortal.shn sind byte-identisch (NA2016 geprueft) - die
    // Server-Kopie dient als Rueckfall, falls nur ein Server-Ordner bekannt ist.
    std::vector<std::filesystem::path> candidates;
    if (!state.npcDialogRessystemRoot.empty()) candidates.push_back(std::filesystem::path(state.npcDialogRessystemRoot) / "TownPortal.shn");
    if (const auto srv = PortalServerRoot(state); !srv.empty()) candidates.push_back(std::filesystem::path(srv) / "TownPortal.shn");
    for (const auto& path : candidates) {
        auto result = core::legacy::LoadShnFile(path);
        if (result) { state.townPortalShn = std::move(*result); state.townPortalLoaded = true; return; }
    }
}

// Lädt RecallCoord.txt (Server, World/) - Schriftrollen mit FESTEM Ziel (ein Eintrag pro
// Schriftrollen-Item), ShineText-Format wie NPC.txt/MobRegen.
void EnsureRecallCoordLoaded(EditorState& state) {
    const std::string serverRoot = PortalServerRoot(state);
    if (state.recallCoordLoaded || serverRoot.empty()) return;
    auto path = std::filesystem::path(serverRoot) / "World" / "RecallCoord.txt";
    auto result = core::legacy::LoadShineTextFile(path);
    if (result) { state.recallCoordFile = std::move(*result); state.recallCoordLoaded = true; }
}

// Speichert TownPortal.shn in die Client-Kopie (ressystem/) UND - falls dort vorhanden - in die
// Server-Kopie (Server/9Data/Shine/). Beide Dateien sind im Original byte-identisch (NA2016
// geprueft); nur eine davon zu aendern wuerde Client und Server auseinanderlaufen lassen.
// Es wird nie eine NEUE Datei im Server-Baum angelegt, nur eine vorhandene ueberschrieben.
void SaveTownPortalFiles(EditorState& state) {
    EnsureNpcDialogRoot(state);
    std::string done, failed;
    auto saveTo = [&](const std::filesystem::path& path, const char* label, bool mustExist) {
        std::error_code ec;
        if (mustExist && !std::filesystem::exists(path, ec)) return;
        auto saved = core::legacy::SaveShnFile(state.townPortalShn, path);
        if (saved) done += std::string(done.empty() ? "" : " + ") + label;
        else failed += std::string(label) + ": " + saved.error() + " ";
    };
    if (!state.npcDialogRessystemRoot.empty()) saveTo(std::filesystem::path(state.npcDialogRessystemRoot) / "TownPortal.shn", "Client", false);
    if (const auto srv = PortalServerRoot(state); !srv.empty()) saveTo(std::filesystem::path(srv) / "TownPortal.shn", "Server", true);
    if (!failed.empty()) state.statusMessage = "Fehler beim Speichern von TownPortal.shn - " + failed;
    else if (done.empty()) state.statusMessage = "TownPortal.shn: kein Ziel-Ordner bekannt (Client-ressystem oder Server-Shine-Ordner nötig).";
    else state.statusMessage = "TownPortal.shn gespeichert (" + done + ").";
}

// Gemeinsamer Editor für die beiden übrigen Portal-/Teleport-Tabellen (siehe Nutzer-Taxonomie:
// 1. TownPortal = Skill mit auswählbarem Ziel, 4a. RecallCoord = Schriftrolle mit festem Ziel).
// Die "normalen Portale" (2./3./5. - Map-zu-Map, NPC-förmige Gates, Instanz-Tore) sind bereits
// über World/NPC.txt im NPC-Tab des Map-Editors abgedeckt, siehe Rollen Gate/IDGate/
// ModeIDGate/RandomGate.
void DrawPortalEditor(EditorState& state) {
    EnsureTownPortalLoaded(state);
    EnsureRecallCoordLoaded(state);

    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "PORTAL-DATEN");
    ImGui::SameLine();
    ImGui::TextDisabled("Client- und Server-Ziele");
    ImGui::SeparatorText("TownPortal · Skill/Menü");
    if (!state.townPortalLoaded) {
        ImGui::TextWrapped("TownPortal.shn konnte nicht geladen werden (Client-ressystem-Ordner nötig).");
    } else if (ImGui::BeginTable("##townportal", static_cast<int>(state.townPortalShn.columns.size()),
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (auto& c : state.townPortalShn.columns) ImGui::TableSetupColumn(c.name.c_str());
        ImGui::TableHeadersRow();
        for (std::size_t ri = 0; ri < state.townPortalShn.rows.size(); ++ri) {
            ImGui::TableNextRow();
            for (std::size_t ci = 0; ci < state.townPortalShn.columns.size(); ++ci) {
                ImGui::TableSetColumnIndex(static_cast<int>(ci));
                ImGui::SetNextItemWidth(-1.0f);
                std::string cur = ShnShortValue(state.townPortalShn.rows[ri].values[ci]);
                std::vector<char> buf(cur.begin(), cur.end());
                buf.resize(std::max<std::size_t>(buf.size() + 1, 32));
                if (UI::InputText(("##tp" + std::to_string(ri) + "_" + std::to_string(ci)).c_str(), buf.data(), buf.size())) {
                    auto parsed = core::legacy::ParseShnValue(state.townPortalShn.columns[ci], buf.data());
                    if (parsed) state.townPortalShn.rows[ri].values[ci] = std::move(*parsed);
                }
            }
        }
        ImGui::EndTable();
    }
    if (UI::Button("TownPortal.shn speichern")) {
        SaveTownPortalFiles(state);
    }

    ImGui::SeparatorText("RecallCoord · Schriftrollen");
    if (!state.recallCoordLoaded) {
        ImGui::TextWrapped("RecallCoord.txt konnte nicht geladen werden (Server-World-Ordner nötig).");
        return;
    }
    auto* table = state.recallCoordFile.FindTable("RecallPoint");
    if (!table) { ImGui::TextDisabled("Keine 'RecallPoint'-Tabelle gefunden."); return; }
    if (ImGui::BeginTable("##recallcoord", static_cast<int>(table->columns.size()), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (auto& c : table->columns) ImGui::TableSetupColumn(c.name.c_str());
        ImGui::TableHeadersRow();
        for (std::size_t ri = 0; ri < table->records.size(); ++ri) {
            ImGui::TableNextRow();
            for (std::size_t ci = 0; ci < table->records[ri].values.size(); ++ci) {
                ImGui::TableSetColumnIndex(static_cast<int>(ci));
                ImGui::SetNextItemWidth(-1.0f);
                std::string& v = table->records[ri].values[ci];
                std::vector<char> buf(v.begin(), v.end());
                buf.resize(std::max<std::size_t>(buf.size() + 1, 40));
                if (UI::InputText(("##rc" + std::to_string(ri) + "_" + std::to_string(ci)).c_str(), buf.data(), buf.size())) {
                    v.assign(buf.data());
                }
            }
        }
        ImGui::EndTable();
    }
    if (UI::Button("RecallCoord.txt speichern")) {
        auto path = std::filesystem::path(PortalServerRoot(state)) / "World" / "RecallCoord.txt";
        auto saved = core::legacy::SaveShineTextFile(state.recallCoordFile, path);
        state.statusMessage = saved ? std::string("RecallCoord.txt gespeichert.") : "Fehler: " + saved.error();
    }
}


// ============================================================================
// Portale-Tab im Map-Editor (EditMode::Portals) - siehe CHANGELOG [0.44.25].
//
// Datengrundlage, gegen die echten NA2016-Dateien verifiziert:
//  * TownPortal.shn: Spalten Index / MinLevel / TP_GroupNo / MapName / X / Y. MapName + X/Y
//    sind ein ORT AUF DER KARTE (Weltkoordinaten wie Coord-X/Coord-Y in World/NPC.txt): bei
//    allen 8 Eintraegen liegt (X,Y) innerhalb ~205 Einheiten des "Gate_Town"-NPCs derselben
//    Karte (RouN: exakt identisch).
//  * RecallCoord.txt (Tabelle RecallPoint): ItemIndex / ItemIdent / MapName / LinkX / LinkY,
//    ebenfalls Weltkoordinaten auf der Karte "MapName".
//  * MapInfo.shn: RegenX / RegenY = Wiederbelebungspunkt (nur Referenz-Marker, nicht editierbar).
// ============================================================================
constexpr int kPortalKindNone = 0;
constexpr int kPortalKindTown = 1;
constexpr int kPortalKindRecall = 2;

struct PortalMarker {
    int kind = kPortalKindNone;
    std::size_t idx = 0;   // Zeilen-Index (TownPortal.shn) bzw. Record-Index (RecallPoint)
    float x = 0.0f;
    float y = 0.0f;
    std::string label;
};

int FindShnColumnByName(const core::legacy::ShnFile& f, const std::string& name) {
    for (std::size_t i = 0; i < f.columns.size(); ++i) {
        if (f.columns[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

long long ShnCellInt(const core::legacy::ShnFile& f, std::size_t row, int col) {
    long long v = 0;
    if (col < 0 || row >= f.rows.size() || static_cast<std::size_t>(col) >= f.rows[row].values.size()) return 0;
    ShnValueAsInt(f.rows[row].values[static_cast<std::size_t>(col)], v);
    return v;
}

// Schreibt einen Ganzzahlwert in eine Zelle; Werte ausserhalb des Spaltentyps (z.B. > 255 bei
// UInt8) lehnt ParseShnValue ab - die Zelle bleibt dann unveraendert.
void SetShnCellInt(core::legacy::ShnFile& f, std::size_t row, int col, long long v) {
    if (col < 0 || row >= f.rows.size() || static_cast<std::size_t>(col) >= f.rows[row].values.size()) return;
    v = std::max(0LL, v);
    auto parsed = core::legacy::ParseShnValue(f.columns[static_cast<std::size_t>(col)], std::to_string(v));
    if (parsed) f.rows[row].values[static_cast<std::size_t>(col)] = std::move(*parsed);
}

std::string TrimAscii(const std::string& in) {
    std::size_t a = 0, b = in.size();
    while (a < b && (in[a] == ' ' || in[a] == '\t' || in[a] == '\r')) ++a;
    while (b > a && (in[b - 1] == ' ' || in[b - 1] == '\t' || in[b - 1] == '\r')) --b;
    return in.substr(a, b - a);
}

void EnsureMapInfoLoaded(EditorState& state) {
    if (state.mapInfoLoaded) return;
    EnsureNpcDialogRoot(state);
    std::vector<std::filesystem::path> candidates;
    if (!state.npcDialogRessystemRoot.empty()) candidates.push_back(std::filesystem::path(state.npcDialogRessystemRoot) / "MapInfo.shn");
    if (const auto srv = PortalServerRoot(state); !srv.empty()) candidates.push_back(std::filesystem::path(srv) / "MapInfo.shn");
    for (const auto& path : candidates) {
        auto result = core::legacy::LoadShnFile(path);
        if (result) { state.mapInfoShn = std::move(*result); state.mapInfoLoaded = true; return; }
    }
}

// Laedt alle Daten des Portale-Tabs - genau einmal pro Ordner-Kombination (sonst wuerde eine
// fehlende Datei in jedem Frame erneut von der Platte gelesen). Aendert der Nutzer einen
// Ordner, wird neu geladen; ungespeicherte Aenderungen werden dabei verworfen.
void EnsurePortalDataLoaded(EditorState& state) {
    if (state.shineTextRoot.empty() && !state.shnServerRoot.empty()) state.shineTextRoot = state.shnServerRoot;
    EnsureNpcDialogRoot(state);
    const std::string sig = state.npcDialogRessystemRoot + "|" + PortalServerRoot(state);
    if (state.portalDataSig == sig) return;
    state.portalDataSig = sig;
    state.townPortalLoaded = false;
    state.recallCoordLoaded = false;
    state.mapInfoLoaded = false;
    state.selectedPortalKind = kPortalKindNone;
    state.selectedPortalIdx = -1;
    EnsureTownPortalLoaded(state);
    EnsureRecallCoordLoaded(state);
    EnsureMapInfoLoaded(state);
    EnsureNpcTextLoaded(state);
}

// Erster aktiver "Gate_Town"-NPC der offenen Karte in World/NPC.txt - Referenzmarker.
bool FindGateTownNpc(EditorState& state, float& x, float& y) {
    if (!state.npcTextLoaded) return false;
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return false;
    for (std::size_t idx : NpcRecordsForCurrentMap(state)) {
        const auto& rec = table->records[idx];
        if (rec.values.size() > 3 && rec.values[0] == "Gate_Town") {
            x = static_cast<float>(std::atoi(rec.values[2].c_str()));
            y = static_cast<float>(std::atoi(rec.values[3].c_str()));
            return true;
        }
    }
    return false;
}

// Wiederbelebungspunkt der offenen Karte (MapInfo.shn: MapName == Kartenname).
bool FindMapRegen(EditorState& state, float& x, float& y) {
    if (!state.mapInfoLoaded) return false;
    const auto& f = state.mapInfoShn;
    const int cName = FindShnColumnByName(f, "MapName");
    const int cX = FindShnColumnByName(f, "RegenX");
    const int cY = FindShnColumnByName(f, "RegenY");
    if (cName < 0 || cX < 0 || cY < 0) return false;
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        if (static_cast<std::size_t>(cName) >= f.rows[r].values.size()) continue;
        if (core::legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(cName)]) == state.legacySaveStem) {
            x = static_cast<float>(ShnCellInt(f, r, cX));
            y = static_cast<float>(ShnCellInt(f, r, cY));
            return true;
        }
    }
    return false;
}

// Alle Teleport-Ziele (TownPortal + Schriftrollen), die auf der offenen Karte liegen.
std::vector<PortalMarker> CollectPortalMarkers(EditorState& state) {
    std::vector<PortalMarker> out;
    if (state.legacySaveStem[0] == '\0') return out;
    const std::string stem = state.legacySaveStem;
    if (state.townPortalLoaded) {
        const auto& f = state.townPortalShn;
        const int cMap = FindShnColumnByName(f, "MapName");
        const int cX = FindShnColumnByName(f, "X");
        const int cY = FindShnColumnByName(f, "Y");
        const int cIdx = FindShnColumnByName(f, "Index");
        if (cMap >= 0 && cX >= 0 && cY >= 0) {
            for (std::size_t r = 0; r < f.rows.size(); ++r) {
                if (static_cast<std::size_t>(cMap) >= f.rows[r].values.size()) continue;
                if (core::legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(cMap)]) != stem) continue;
                PortalMarker m;
                m.kind = kPortalKindTown;
                m.idx = r;
                m.x = static_cast<float>(ShnCellInt(f, r, cX));
                m.y = static_cast<float>(ShnCellInt(f, r, cY));
                m.label = "TownPortal #" + std::to_string(cIdx >= 0 ? ShnCellInt(f, r, cIdx) : static_cast<long long>(r));
                out.push_back(std::move(m));
            }
        }
    }
    if (state.recallCoordLoaded) {
        auto* table = state.recallCoordFile.FindTable("RecallPoint");
        if (table && table->columns.size() >= 5) {
            for (std::size_t r = 0; r < table->records.size(); ++r) {
                const auto& rec = table->records[r];
                if (rec.values.size() < 5 || TrimAscii(rec.values[2]) != stem) continue;
                PortalMarker m;
                m.kind = kPortalKindRecall;
                m.idx = r;
                m.x = static_cast<float>(std::atoi(rec.values[3].c_str()));
                m.y = static_cast<float>(std::atoi(rec.values[4].c_str()));
                m.label = TrimAscii(rec.values[0]);
                out.push_back(std::move(m));
            }
        }
    }
    return out;
}

// Setzt die Position des aktuell gewaehlten Ziels (Klick im 2D-View bzw. Eingabefelder).
void SetSelectedPortalPosition(EditorState& state, long long x, long long y) {
    x = std::max(0LL, x);
    y = std::max(0LL, y);
    if (state.selectedPortalKind == kPortalKindTown && state.selectedPortalIdx >= 0) {
        auto& f = state.townPortalShn;
        SetShnCellInt(f, static_cast<std::size_t>(state.selectedPortalIdx), FindShnColumnByName(f, "X"), x);
        SetShnCellInt(f, static_cast<std::size_t>(state.selectedPortalIdx), FindShnColumnByName(f, "Y"), y);
    } else if (state.selectedPortalKind == kPortalKindRecall && state.selectedPortalIdx >= 0) {
        auto* table = state.recallCoordFile.FindTable("RecallPoint");
        if (!table || static_cast<std::size_t>(state.selectedPortalIdx) >= table->records.size()) return;
        auto& rec = table->records[static_cast<std::size_t>(state.selectedPortalIdx)];
        if (rec.values.size() < 5) return;
        rec.values[3] = std::to_string(x);
        rec.values[4] = std::to_string(y);
    }
}

// Legt einen neuen TownPortal-Eintrag fuer die offene Karte an (Kopie der letzten Zeile als
// Strukturvorlage, naechster freier Index). Startposition: Gate_Town-NPC, sonst Regen-Punkt,
// sonst Kartenmitte. Ob der Client fuer NEUE Ziele weitere Daten braucht (Menuetext o.ae.),
// ist NICHT geprueft - siehe CHANGELOG [0.44.25].
void AddTownPortalForCurrentMap(EditorState& state) {
    auto& f = state.townPortalShn;
    const int cIdx = FindShnColumnByName(f, "Index");
    const int cMap = FindShnColumnByName(f, "MapName");
    const int cX = FindShnColumnByName(f, "X");
    const int cY = FindShnColumnByName(f, "Y");
    if (f.rows.empty() || cMap < 0 || cX < 0 || cY < 0) { state.statusMessage = "TownPortal.shn: Spalten MapName/X/Y nicht gefunden."; return; }
    long long maxIdx = -1;
    if (cIdx >= 0) for (std::size_t r = 0; r < f.rows.size(); ++r) maxIdx = std::max(maxIdx, ShnCellInt(f, r, cIdx));
    core::legacy::ShnRow row = f.rows.back();
    f.rows.push_back(std::move(row));
    const std::size_t r = f.rows.size() - 1;
    if (auto pv = core::legacy::ParseShnValue(f.columns[static_cast<std::size_t>(cMap)], state.legacySaveStem)) {
        f.rows[r].values[static_cast<std::size_t>(cMap)] = std::move(*pv);
    }
    if (cIdx >= 0) SetShnCellInt(f, r, cIdx, maxIdx + 1);
    float px = 0.0f, py = 0.0f;
    if (!FindGateTownNpc(state, px, py) && !FindMapRegen(state, px, py)) {
        px = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth() * 0.5f;
        py = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight() * 0.5f;
    }
    SetShnCellInt(f, r, cX, static_cast<long long>(std::lround(px)));
    SetShnCellInt(f, r, cY, static_cast<long long>(std::lround(py)));
    state.selectedPortalKind = kPortalKindTown;
    state.selectedPortalIdx = static_cast<int>(r);
}

// Generischer Editor fuer einen ShineText-Record: Zahlenspalten (DWRD/BYTE/WORD) als Zahlenfeld,
// alles andere als Textfeld. `firstEditable` = erste bearbeitbare Spalte (z.B. 1, wenn die erste
// der Verknuepfungsname ist). Fehlen hinten getrimmte Leerwerte, wird NUR bei einer Aenderung
// auf die volle Spaltenzahl aufgefuellt (unveraenderte Records bleiben byte-identisch).
bool DrawShineRecordFields(std::vector<std::string>& values, const std::vector<core::legacy::ShineColumn>& columns,
                           std::size_t firstEditable) {
    std::vector<std::string> edited = values;
    if (edited.size() < columns.size()) edited.resize(columns.size(), "0");
    bool changed = false;
    for (std::size_t i = firstEditable; i < columns.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const std::string& type = columns[i].type;
        const std::string label = columns[i].name.empty() ? ("Spalte " + std::to_string(i)) : columns[i].name;
        if (type == "DWRD" || type == "BYTE" || type == "WORD") {
            int v = std::atoi(edited[i].c_str());
            ImGui::SetNextItemWidth(150.0f);
            if (UI::InputInt(label.c_str(), &v, 1, 50)) {
                if (type == "BYTE") v = std::clamp(v, 0, 255);
                edited[i] = std::to_string(v);
                changed = true;
            }
        } else {
            std::vector<char> buf(edited[i].begin(), edited[i].end());
            buf.resize(std::max<std::size_t>(buf.size() + 1, 64));
            ImGui::SetNextItemWidth(150.0f);
            if (UI::InputText(label.c_str(), buf.data(), buf.size())) { edited[i].assign(buf.data()); changed = true; }
        }
        ImGui::PopID();
    }
    if (changed) values = std::move(edited);
    return changed;
}

// Textfeld (+ unter Windows Ordner-Dialog) fuer einen noch nicht gefundenen Ordner.
bool DrawPortalFolderField(const char* hint, const char* id, std::string& value) {
    bool changed = false;
    ImGui::TextWrapped("%s", hint);
    std::vector<char> buf(value.begin(), value.end());
    buf.resize(std::max<std::size_t>(buf.size() + 1, 512));
    ImGui::SetNextItemWidth(-1.0f);
    if (UI::InputText((std::string("##") + id).c_str(), buf.data(), buf.size())) { value.assign(buf.data()); changed = true; }
#ifdef _WIN32
    if (UI::Button((std::string("Ordner wählen...##") + id).c_str())) {
        if (auto p = BrowseForFolderWindows(hint)) { value = *p; changed = true; }
    }
#endif
    return changed;
}

void DrawPortalsToolsPanel(EditorState& state) {
    EnsurePortalDataLoaded(state);

    if (state.npcDialogRessystemRoot.empty()) {
        if (DrawPortalFolderField("Client-ressystem-Ordner (enthält TownPortal.shn, MapInfo.shn) noch nicht gefunden.", "portal_ressystem", state.npcDialogRessystemRoot)) state.portalDataSig.clear();
    }
    if (PortalServerRoot(state).empty()) {
        if (DrawPortalFolderField("Server-Ordner Server/9Data/Shine (enthält World/RecallCoord.txt, World/NPC.txt) noch nicht gewählt.", "portal_server", state.shineTextRoot)) state.portalDataSig.clear();
    }
    if (state.legacySaveStem[0] == '\0') { ImGui::TextDisabled("Keine Karte offen."); return; }

    const auto markers = CollectPortalMarkers(state);
    // Auswahl bereinigen (z.B. nach Kartenwechsel oder Neuladen).
    bool selectionValid = false;
    for (const auto& m : markers) if (m.kind == state.selectedPortalKind && static_cast<int>(m.idx) == state.selectedPortalIdx) selectionValid = true;
    if (!selectionValid) { state.selectedPortalKind = kPortalKindNone; state.selectedPortalIdx = -1; }

    if (!state.townPortalLoaded) ImGui::TextWrapped("TownPortal.shn konnte nicht geladen werden.");
    if (!state.recallCoordLoaded) ImGui::TextWrapped("World/RecallCoord.txt konnte nicht geladen werden.");

    ImGui::TextDisabled("%zu Teleport-Ziele auf '%s' · Auswahl links im Szene-Outliner",
                        markers.size(), state.legacySaveStem);
    ImGui::TextDisabled("Raute = TownPortal · Dreieck = Schriftrolle · Quadrat = Gate_Town · Ring = Wiederbelebung");

    ImGui::SeparatorText("Auswahl");
    if (state.selectedPortalKind == kPortalKindNone)
        ImGui::TextDisabled("Portal im Szene-Outliner oder in der 2D-Ansicht auswählen.");

    for (const auto& m : markers) {
        if (m.kind != state.selectedPortalKind || static_cast<int>(m.idx) != state.selectedPortalIdx) continue;
        ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "%s",
                           m.kind == kPortalKindTown ? m.label.c_str() : ("Schriftrolle " + m.label).c_str());
        ImGui::TextDisabled("%s", m.kind == kPortalKindTown ? "TownPortal · auswählbares Ziel" : "RecallCoord · festes Schriftrollen-Ziel");
        ImGui::SeparatorText("Position");
        int x = static_cast<int>(m.x), y = static_cast<int>(m.y);
        bool changed = false;
        changed |= UI::InputInt("X", &x, 1, 50);
        changed |= UI::InputInt("Y", &y, 1, 50);
        if (changed) SetSelectedPortalPosition(state, x, y);
        if (UI::Button("Kamera auf Portal", ImVec2(-1,0))) {
            const float wx=static_cast<float>(x), wz=static_cast<float>(y);
            state.camera.SetTarget(wx,state.heightmap.SampleWorld(wx,wz)+25.0f,wz);
            state.camera.Zoom(260.0f-state.camera.Distance());
        }

        if (m.kind == kPortalKindTown) {
            ImGui::SeparatorText("Bedingungen");
            auto& f = state.townPortalShn;
            const int cLvl = FindShnColumnByName(f, "MinLevel");
            const int cGrp = FindShnColumnByName(f, "TP_GroupNo");
            if (cLvl >= 0) {
                int v = static_cast<int>(ShnCellInt(f, m.idx, cLvl));
                if (UI::InputInt("Mindestlevel", &v)) SetShnCellInt(f, m.idx, cLvl, std::clamp(v, 0, 255));
            }
            if (cGrp >= 0) {
                int v = static_cast<int>(ShnCellInt(f, m.idx, cGrp));
                if (UI::InputInt("Menü-Gruppe", &v)) SetShnCellInt(f, m.idx, cGrp, std::clamp(v, 0, 255));
            }
        }
        float gx = 0.0f, gy = 0.0f;
        if (FindGateTownNpc(state, gx, gy)) {
            ImGui::TextDisabled("Abstand zum Gate_Town-NPC: %.0f", std::sqrt((gx - m.x) * (gx - m.x) + (gy - m.y) * (gy - m.y)));
        }
        break;
    }
    ImGui::SeparatorText("Positionieren");
    UI::Checkbox("Position per Klick im 2D-View setzen", &state.portalPickMode);
    if (state.portalPickMode && state.selectedPortalKind == kPortalKindNone)
        ImGui::TextDisabled("Zuerst ein Ziel im Szene-Outliner oder in der 2D-Ansicht wählen.");

    ImGui::SeparatorText("Aktionen");
    if (state.townPortalLoaded) {
        bool hasTown = false;
        for (const auto& m : markers) if (m.kind == kPortalKindTown) hasTown = true;
        if (!hasTown) {
            ImGui::TextDisabled("Diese Karte hat noch kein TownPortal-Ziel.");
        }
        if (UI::Button("TownPortal-Ziel hier hinzufügen")) AddTownPortalForCurrentMap(state);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Legt einen neuen Eintrag in TownPortal.shn an. Ob der Client dafür weitere Daten braucht, ist ungeprüft.");
    }

    if (state.townPortalLoaded && UI::Button("TownPortal.shn speichern", ImVec2(-1,0))) SaveTownPortalFiles(state);
    if (state.recallCoordLoaded && UI::Button("RecallCoord.txt speichern", ImVec2(-1,0))) {
        auto path = std::filesystem::path(PortalServerRoot(state)) / "World" / "RecallCoord.txt";
        auto saved = core::legacy::SaveShineTextFile(state.recallCoordFile, path);
        state.statusMessage = saved ? std::string("RecallCoord.txt gespeichert.") : "Fehler: " + saved.error();
    }
    if (UI::Button("Verwerfen und neu laden", ImVec2(-1,0))) state.portalDataSig.clear();
}

// Zeichnet die Portal-Marker in den 2D-View (Farben wie im restlichen Theme: Weiss = gewaehlt,
// Blau = uebrige, helles Grau = Referenz).
void DrawPortalMarkers2D(EditorState& state, const ImVec2& origin, const ImVec2& size, float spanX, float spanZ) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto toScreen = [&](float wx, float wy) {
        return ImVec2(origin.x + (spanX > 0.0f ? wx / spanX : 0.0f) * size.x,
                      origin.y + (1.0f - (spanZ > 0.0f ? wy / spanZ : 0.0f)) * size.y);
    };
    const ImU32 refCol = IM_COL32(170, 185, 205, 210);
    float gx = 0.0f, gy = 0.0f, rx = 0.0f, ry = 0.0f;
    const bool hasGate = FindGateTownNpc(state, gx, gy);
    const bool hasRegen = FindMapRegen(state, rx, ry);
    if (hasGate) {
        const ImVec2 c = toScreen(gx, gy);
        dl->AddRect(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), refCol, 0.0f, 0, 1.5f);
    }
    if (hasRegen) {
        const ImVec2 c = toScreen(rx, ry);
        dl->AddCircle(c, 6.0f, IM_COL32(230, 230, 235, 170), 16, 1.25f);
    }
    for (const auto& m : CollectPortalMarkers(state)) {
        const bool selected = m.kind == state.selectedPortalKind && static_cast<int>(m.idx) == state.selectedPortalIdx;
        const ImU32 col = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 135, 210, 230);
        const ImVec2 c = toScreen(m.x, m.y);
        const float r = selected ? 7.0f : 5.5f;
        if (m.kind == kPortalKindTown) {
            if (hasGate) dl->AddLine(toScreen(gx, gy), c, IM_COL32(170, 185, 205, 90), 1.0f);
            const ImVec2 pts[4] = {ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y)};
            dl->AddConvexPolyFilled(pts, 4, col);
        } else {
            dl->AddTriangleFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y + r), ImVec2(c.x - r, c.y + r), col);
        }
        if (selected) dl->AddText(ImVec2(c.x + r + 4.0f, c.y - r), col, m.label.c_str());
    }
}


void EnsureNpcDialogLoaded(EditorState& state) {
    if (state.npcDialogLoaded || state.npcDialogRessystemRoot.empty()) return;
    auto path = std::filesystem::path(state.npcDialogRessystemRoot) / "NpcDialogData.shn";
    auto result = core::legacy::LoadShnFile(path);
    if (result) { state.npcDialogShn = std::move(*result); state.npcDialogLoaded = true; }
    else state.statusMessage = "NpcDialogData.shn: " + result.error();
}

// Öffnet ein Lua-KI-Skript (LuaScript/AIScript/<Name>.lua, unter state.shnServerRoot) als
// reinen Text-Editor - siehe CHANGELOG [0.44.22]. Echte, vollständige Lua-Dateien (require/
// function/Engine-Aufrufe wie cStaticDamage_smo) - bewusst KEINE Syntaxprüfung, nur
// Text-Bearbeitung, ein Lua-Parser wäre weit außerhalb des Editor-Umfangs.
void OpenAiScriptEditor(EditorState& state, const std::string& mobName) {
    if (state.shnServerRoot.empty()) { state.statusMessage = "Server-SHN-Ordner nötig (Single SHN Editor)."; return; }
    auto path = std::filesystem::path(state.shnServerRoot) / "LuaScript" / "AIScript" / (mobName + ".lua");
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        state.statusMessage = "Kein Lua-KI-Skript gefunden: " + path.string();
        return;
    }
    std::ifstream in(path, std::ios::binary);
    state.aiScriptEditorText.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    state.aiScriptEditorPath = path.string();
    state.aiScriptEditorName = mobName + ".lua";
    state.aiScriptEditorOpen = true;
}

// Dito für PineScript (MobBehaviorDescript/<relativer Pfad ohne .ps>) - relScriptPath kommt
// z.B. direkt aus World/PineScript.txt (Spalte ScriptName, siehe CHANGELOG [0.44.22]).
void OpenPineScriptEditor(EditorState& state, const std::string& relScriptPath) {
    if (state.shnServerRoot.empty()) { state.statusMessage = "Server-SHN-Ordner nötig (Single SHN Editor)."; return; }
    auto path = std::filesystem::path(state.shnServerRoot) / "MobBehaviorDescript" / (relScriptPath + ".ps");
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        state.statusMessage = "Kein PineScript-Verhalten gefunden: " + path.string();
        return;
    }
    std::ifstream in(path, std::ios::binary);
    state.aiScriptEditorText.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    state.aiScriptEditorPath = path.string();
    state.aiScriptEditorName = relScriptPath + ".ps";
    state.aiScriptEditorOpen = true;
}

// Lädt/öffnet die Patrouillenroute eines Mobs/NPCs (MobRoam/<Name>.txt, Server) - ShineText-
// Format, Tabelle "Roaming": ID/X/Y/EventIndex, letzter Wegpunkt trägt oft "return" als
// EventIndex (siehe CHANGELOG [0.44.23], byte-exakt gegen echte Daten wie BerValeDw04.txt
// verifiziert). Nicht jedes Mob/NPC hat eine Route - fehlende Datei ist normal, kein Fehler.
void OpenPatrolRouteEditor(EditorState& state, const std::string& mobName) {
    if (state.shnServerRoot.empty()) { state.statusMessage = "Server-SHN-Ordner nötig (Single SHN Editor)."; return; }
    auto path = std::filesystem::path(state.shnServerRoot) / "MobRoam" / (mobName + ".txt");
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        state.statusMessage = "Keine Patrouillenroute gefunden: " + path.string();
        state.patrolRouteLoaded = false;
        return;
    }
    auto result = core::legacy::LoadShineTextFile(path);
    if (!result) { state.statusMessage = "MobRoam/" + mobName + ".txt: " + result.error(); return; }
    state.patrolRouteFile = std::move(*result);
    state.patrolRouteLoaded = true;
    state.patrolEditorName = mobName;
    state.patrolEditorOpen = true;
}

void DrawPatrolRouteEditorPopup(EditorState& state) {
    if (!state.patrolEditorOpen) return;
    ImGui::OpenPopup("Patrouillenroute bearbeiten");
    ImGui::SetNextWindowSize(ImVec2(520.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Patrouillenroute bearbeiten", &state.patrolEditorOpen)) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", state.patrolEditorName.c_str());
        if (!state.patrolRouteLoaded) {
            ImGui::TextWrapped("Route konnte nicht geladen werden.");
        } else {
            auto* table = state.patrolRouteFile.FindTable("Roaming");
            if (!table) {
                ImGui::TextDisabled("Keine 'Roaming'-Tabelle in dieser Datei gefunden.");
            } else {
                ImGui::TextDisabled("Wegpunkte werden der Reihe nach abgelaufen; 'return' = zurück zum Start.");
                int removeIdx = -1;
                for (std::size_t i = 0; i < table->records.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    auto& rec = table->records[i];
                    if (rec.values.size() < 4) { ImGui::PopID(); continue; }
                    int x = std::atoi(rec.values[1].c_str());
                    int y = std::atoi(rec.values[2].c_str());
                    ImGui::Text("#%s", rec.values[0].c_str());
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(90.0f);
                    if (UI::InputInt("X", &x)) rec.values[1] = std::to_string(x);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(90.0f);
                    if (UI::InputInt("Y", &y)) rec.values[2] = std::to_string(y);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100.0f);
                    std::vector<char> evBuf(rec.values[3].begin(), rec.values[3].end());
                    evBuf.resize(std::max<std::size_t>(evBuf.size() + 1, 32));
                    if (UI::InputText("Event", evBuf.data(), evBuf.size())) rec.values[3].assign(evBuf.data());
                    ImGui::SameLine();
                    if (UI::SmallButton("X")) removeIdx = static_cast<int>(i);
                    ImGui::PopID();
                }
                if (removeIdx >= 0) table->records.erase(table->records.begin() + removeIdx);
                if (UI::Button("+ Wegpunkt hinzufügen")) {
                    std::vector<std::string> vals = {std::to_string(table->records.size()), "0", "0", "-"};
                    table->records.push_back(core::legacy::ShineRecord{vals, 0});
                }
                ImGui::Separator();
                if (UI::Button("Speichern")) {
                    auto path = std::filesystem::path(state.shnServerRoot) / "MobRoam" / (state.patrolEditorName + ".txt");
                    auto saved = core::legacy::SaveShineTextFile(state.patrolRouteFile, path);
                    state.statusMessage = saved ? std::string("Patrouillenroute gespeichert.") : "Fehler: " + saved.error();
                }
            }
        }
        ImGui::SameLine();
        if (UI::Button("Schließen")) { state.patrolEditorOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

void DrawAiScriptEditorPopup(EditorState& state) {
    if (!state.aiScriptEditorOpen) return;
    ImGui::OpenPopup("KI-Skript bearbeiten");
    ImGui::SetNextWindowSize(ImVec2(720.0f, 540.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("KI-Skript bearbeiten", &state.aiScriptEditorOpen)) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", state.aiScriptEditorName.c_str());
        ImGui::TextDisabled("Reiner Text-Editor (Lua/PineScript) - keine Syntaxprüfung, Änderungen 1:1 gespeichert.");
        std::vector<char> buf(state.aiScriptEditorText.begin(), state.aiScriptEditorText.end());
        buf.resize(std::max<std::size_t>(buf.size() + 1, 8192));
        if (ImGui::InputTextMultiline("##aiscript", buf.data(), buf.size(), ImVec2(-1.0f, 420.0f), ImGuiInputTextFlags_AllowTabInput)) {
            state.aiScriptEditorText.assign(buf.data());
        }
        if (UI::Button("Speichern")) {
            std::ofstream out(state.aiScriptEditorPath, std::ios::binary);
            if (out) {
                out.write(state.aiScriptEditorText.data(), static_cast<std::streamsize>(state.aiScriptEditorText.size()));
                state.statusMessage = "Gespeichert: " + state.aiScriptEditorPath;
            } else {
                state.statusMessage = "Fehler beim Speichern: " + state.aiScriptEditorPath;
            }
        }
        ImGui::SameLine();
        if (UI::Button("Schließen")) { state.aiScriptEditorOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

int FindDialogRowForNpc(EditorState& state, const std::string& npcName) {
    for (std::size_t i = 0; i < state.npcDialogShn.rows.size(); ++i) {
        if (state.npcDialogShn.rows[i].values.empty()) continue;
        if (core::legacy::ShnValueToString(state.npcDialogShn.rows[i].values[0]) == npcName) return static_cast<int>(i);
    }
    return -1;
}

// Öffnet den Dialog-Editor für den übergebenen NPC-Namen - lädt Root/Datei bei Bedarf nach,
// sucht die passende Zeile per MobIDX und befüllt die Bearbeitungs-Buffer.
void EnsureMobViewInfoLoaded(EditorState& state) {
    if (state.mobViewInfoLoaded || state.npcDialogRessystemRoot.empty()) return;
    auto path = std::filesystem::path(state.npcDialogRessystemRoot) / "MobViewInfo.shn";
    auto result = core::legacy::LoadShnFile(path);
    if (result) { state.mobViewInfoShn = std::move(*result); state.mobViewInfoLoaded = true; }
}

// Löst den Dateinamen (ohne Endung) des 3D-Modells für einen NPC auf, per InxName-Suche in
// MobViewInfo.shn (Spalte 2 = FileName). Gibt leer zurück, wenn nicht gefunden ODER wenn
// NpcViewIndex (Spalte 9) != 0 ist - dann ist die Erscheinung aus einzeln ausgerüsteten
// Gegenständen zusammengesetzt (siehe NPCViewInfo.shn), dieser komplexere Fall wird noch nicht
// unterstützt (siehe CHANGELOG [0.44.17]). Bei allen gegen echte NA2016-Daten geprüften
// normalen Shop-NPCs (RouSmithJames, RouSoulMctJulia, ...) ist NpcViewIndex==0 - eigenes
// Modell mit demselben Namen wie der NPC selbst.
std::string ResolveMobViewFileName(EditorState& state, const std::string& inxName) {
    if (!state.mobViewInfoLoaded) return {};
    for (auto& row : state.mobViewInfoShn.rows) {
        if (row.values.size() < 10) continue;
        if (core::legacy::ShnValueToString(row.values[1]) != inxName) continue;
        long long npcViewIdx = 0;
        ShnValueAsInt(row.values[9], npcViewIdx);
        if (npcViewIdx != 0) return {};
        return core::legacy::ShnValueToString(row.values[2]);
    }
    return {};
}

// Baut npcRenderSet neu auf (alle NPCs der aktuellen Karte mit auflösbarem 3D-Modell) und lädt
// sie in npcMeshRenderer - nur wenn sich die Karte geändert hat oder npcRenderSetForMap manuell
// geleert wurde (siehe Coord-X/Y-Bearbeitung oben, DrawToolsContent).
// Sucht den "reschar"-Ordner (Client, Geschwister von "resmap"/"ressystem") - enthält pro
// Charakter einen eigenen Unterordner mit .nif (siehe CHANGELOG [0.44.18], vom Nutzer per
// Screenshot des echten Client-Ordners bestätigt: Client/reschar/<Name>/<Name>.nif).
void EnsureRescharRoot(EditorState& state) {
    if (!state.rescharRoot.empty()) return;
    if (auto found = FindNamedSubfolder(state.project.clientFolder, {"reschar"})) {
        state.rescharRoot = found->string();
    }
}

static const char* const kNpcRoles[] = {"QuestNpc", "Merchant", "Guard", "NPCMenu", "StoreManager", "Gate"};
static const char* const kAvatarSlotNames[19] = {
    "Rechte Hand", "Linke Hand", "Körper", "Beine", "Schuhe", "Acc. Körper", "Acc. Beine", "Acc. Schuhe", "Acc. Mund",
    "Acc. Kopf A", "Acc. Augen", "Acc. Kopf", "Acc. Linke Hand", "Acc. Rechte Hand", "Acc. Rücken", "Acc. Taille", "Acc. Hüfte",
    "Mini-Monster", "Mini-Monster R"};
static const char* const kAvatarSlotColumns[19] = {
    "Equ_RightHand", "Equ_LeftHand", "Equ_Body", "Equ_Leg", "Equ_Shoes", "Equ_AccBody", "Equ_AccLeg", "Equ_AccShoes", "Equ_AccMouth",
    "Equ_AccHeadA", "Equ_AccEye", "Equ_AccHead", "Equ_AccLeftHand", "Equ_AccRightHand", "Equ_AccBack", "Equ_AccWeast", "Equ_AccHip",
    "Equ_MiniMon", "Equ_MiniMon_R"};

// Liest/Schreibt eine Zelle per Spaltenname (Text). false, wenn Spalte fehlt oder der Wert nicht passt.
static std::string ShnCellText(const core::legacy::ShnFile& f, std::size_t row, const char* col) {
    const int c = ShnColumnIndexByName(f, col);
    if (c < 0 || row >= f.rows.size() || static_cast<std::size_t>(c) >= f.rows[row].values.size()) return {};
    return core::legacy::ShnValueToString(f.rows[row].values[static_cast<std::size_t>(c)]);
}
static bool SetShnCellText(core::legacy::ShnFile& f, std::size_t row, const char* col, const std::string& text) {
    const int c = ShnColumnIndexByName(f, col);
    if (c < 0 || row >= f.rows.size() || static_cast<std::size_t>(c) >= f.rows[row].values.size()) return false;
    auto parsed = core::legacy::ParseShnValue(f.columns[static_cast<std::size_t>(c)], text);
    if (!parsed) return false;
    f.rows[row].values[static_cast<std::size_t>(c)] = std::move(*parsed);
    return true;
}


core::AvatarRequest MakeAvatarRequest(EditorState& state, int cls, bool male, int face, int hairType, const std::array<std::string, 19>& equ);

// Blickwinkel (Radiant, Drehung um die Hochachse im Editor-Rahmen) eines NPCs aus seiner Richtung (Grad).
static float NpcYawRadians(const EditorState& state, int directionDeg) {
    return (static_cast<float>(state.npcDirSign * directionDeg) + static_cast<float>(state.npcDirOffsetDeg)) * 3.14159265f / 180.0f;
}

// Schreibt Position und Drehung aus einem NPC.txt-Datensatz in ein Render-Objekt.
static void ApplyNpcRecordToObject(const EditorState& state, const core::legacy::ShineRecord& rec, core::PlacedObject& obj) {
    obj.posX = static_cast<float>(std::atoi(rec.values[2].c_str()));
    obj.posZ = static_cast<float>(std::atoi(rec.values[3].c_str()));
    obj.posY = state.heightmap.SampleWorld(obj.posX, obj.posZ);
    const float half = NpcYawRadians(state, std::atoi(rec.values[4].c_str())) * 0.5f;
    obj.rotY = std::sin(half);
    obj.rotW = std::cos(half);
    obj.scale = 1.0f;
}

// Aktualisiert Position/Drehung aller NPC-Render-Objekte OHNE die Modelle neu zu laden (Coord-/Richtung-
// Eingaben sollen bei grossen Charaktermodellen nicht jedes Mal alles neu von der Platte laden).
void RefreshNpcTransforms(EditorState& state) {
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return;
    for (std::size_t i = 0; i < state.npcRenderSet.Count() && i < state.npcRenderRecordIdx.size(); ++i) {
        const std::size_t idx = state.npcRenderRecordIdx[i];
        if (idx >= table->records.size() || table->records[idx].values.size() < 5) continue;
        ApplyNpcRecordToObject(state, table->records[idx], state.npcRenderSet.At(i));
    }
}

static const char* L(const char* de, const char* en);

// Schaetzt Vorzeichen/Versatz der NPC-Blickrichtung fuer die gerade offene Karte (CHANGELOG
// [0.44.34]): testet je Kandidat, wie oft ein NPC "vorne" (in Blickrichtung) auf einer begehbaren
// Zelle und "hinten" auf einer blockierten Zelle steht (Annahme: NPCs stehen mit dem Ruecken zur
// Wand) - dieselbe Methode, mit der der Standardwert ermittelt wurde, hier live auf EINE Karte
// angewendet. Reine Heuristik: Karten ohne nahe Waende (offene Plaetze) liefern kein Signal, kleine
// NPC-Zahlen sind verrauscht - das Ergebnis ersetzt keinen Blick ins echte Spiel.
void EstimateNpcOrientation(EditorState& state) {
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table || state.walkGrid.Width() == 0) {
        state.npcOrientEstimateStatus = L("Keine NPCs oder kein Block&Walk-Gitter geladen.", "No NPCs or no Block&Walk grid loaded.");
        return;
    }
    struct Cand { int sign; int offset; long long net = 0; };
    std::vector<Cand> cands;
    for (const int sign : {1, -1})
        for (int off = -180; off < 180; off += 10) cands.push_back({sign, off});

    const float dists[] = {18.75f, 37.5f, 62.5f}; // 3, 6, 10 Zellen (Zelle = 6.25 Einheiten)
    int sampledNpcs = 0;
    for (const std::size_t idx : NpcRecordsForCurrentMap(state)) {
        const auto& rec = table->records[idx];
        if (rec.values.size() < 5) continue;
        const float x = static_cast<float>(std::atoi(rec.values[2].c_str()));
        const float z = static_cast<float>(std::atoi(rec.values[3].c_str()));
        const int direct = std::atoi(rec.values[4].c_str());
        bool any = false;
        for (const float dist : dists) {
            for (auto& c : cands) {
                const float yaw = static_cast<float>(c.sign * direct + c.offset) * 3.14159265f / 180.0f;
                const float fx = -std::sin(yaw), fz = -std::cos(yaw);
                auto cellOf = [](float wx, float wz) {
                    return std::pair<std::uint32_t, std::uint32_t>{
                        static_cast<std::uint32_t>(std::max(0.0f, wx / core::WalkGrid::kCellSize)),
                        static_cast<std::uint32_t>(std::max(0.0f, wz / core::WalkGrid::kCellSize))};
                };
                const auto [acx, acz] = cellOf(x + fx * dist, z + fz * dist);
                const auto [bcx, bcz] = cellOf(x - fx * dist, z - fz * dist);
                if (acx >= state.walkGrid.Cols() || acz >= state.walkGrid.Rows() ||
                    bcx >= state.walkGrid.Cols() || bcz >= state.walkGrid.Rows()) continue;
                const bool aheadBlocked = state.walkGrid.CellBlocked(acx, acz);
                const bool behindBlocked = state.walkGrid.CellBlocked(bcx, bcz);
                if (!aheadBlocked && behindBlocked) ++c.net;
                if (aheadBlocked && !behindBlocked) --c.net;
                any = true;
            }
        }
        if (any) ++sampledNpcs;
    }
    if (sampledNpcs < 3) {
        state.npcOrientEstimateStatus = L("Zu wenige NPCs mit Wand in der Naehe auf dieser Karte fuer eine Schaetzung.",
                                          "Too few NPCs with a nearby wall on this map for an estimate.");
        return;
    }
    const auto best = std::max_element(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.net < b.net; });
    const long long worst = std::min_element(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.net < b.net; })->net;
    state.npcDirSign = best->sign;
    state.npcDirOffsetDeg = best->offset;
    RefreshNpcTransforms(state);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  L("Geschätzt aus %d NPCs: Vorzeichen %+d, Versatz %d Grad (Signal %+lld, schlechtester Kandidat %+lld - je größer der Abstand, desto verlässlicher).",
                    "Estimated from %d NPCs: sign %+d, offset %d degrees (signal %+lld, worst candidate %+lld - the bigger the gap, the more reliable)."),
                  sampledNpcs, best->sign, best->offset, best->net, worst);
    state.npcOrientEstimateStatus = buf;
}

// Schreibweisen-unabhaengige Suche einer Datei/eines Ordners in einem Ordner.
static std::optional<std::filesystem::path> FindChildCaseInsensitive(const std::filesystem::path& dir, const std::string& name) {
    std::error_code ec;
    const std::string want = LowerAscii(name);
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (LowerAscii(entry.path().filename().string()) == want) return entry.path();
    }
    return std::nullopt;
}

// Sucht das Modell eines NPCs (MobViewInfo.FileName) in ALLEN Modell-Ordnern des Clients: zuerst
// reschar/<Name>/<Name>.nif (Charaktere, siehe CHANGELOG [0.44.18]), dann jeder weitere "res*"-Ordner
// (z.B. resmob) als <Ordner>/<Name>/<Name>.nif bzw. <Ordner>/<Name>.nif, zuletzt die NIF-Bibliothek von
// resmap. Rueckgabe: Pfad relativ zum Client-Ordner (leer = nicht gefunden).
static std::string FindNpcModelPath(EditorState& state, const std::string& fileName) {
    const std::filesystem::path clientRoot(state.project.clientFolder);
    std::vector<std::filesystem::path> roots;
    if (!state.rescharRoot.empty()) roots.emplace_back(state.rescharRoot);
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(clientRoot, ec)) {
            if (!entry.is_directory(ec)) continue;
            const std::string n = LowerAscii(entry.path().filename().string());
            if (n.rfind("res", 0) != 0) continue;
            if (n == "ressystem" || n == "resmenu" || n == "resmap" || n == "resitem" || n == "reschar") continue;
            roots.push_back(entry.path());
        }
    }
    for (const auto& root : roots) {
        std::error_code ec;
        std::filesystem::path candidate;
        if (auto dir = FindChildCaseInsensitive(root, fileName); dir && std::filesystem::is_directory(*dir, ec)) {
            if (auto nif = FindChildCaseInsensitive(*dir, fileName + ".nif")) candidate = *nif;
        }
        if (candidate.empty()) {
            if (auto nif = FindChildCaseInsensitive(root, fileName + ".nif")) candidate = *nif;
        }
        if (candidate.empty()) continue;
        const auto rel = std::filesystem::relative(candidate, clientRoot, ec);
        if (!ec) return rel.string();
    }
    if (!state.nifAssetRoot.empty()) {
        for (auto& nif : state.availableNifFiles) {
            if (LowerAscii(std::filesystem::path(nif).stem().string()) != LowerAscii(fileName)) continue;
            std::error_code ec;
            auto rel = std::filesystem::relative(std::filesystem::path(state.nifAssetRoot) / nif, clientRoot, ec);
            if (!ec) return rel.string();
        }
    }
    return {};
}

// Ergebnis der (GL-freien) Modellauflösung aller NPCs der aktuellen Karte.
struct NpcRenderData {
    core::ObjectPlacementSet set;
    std::vector<std::size_t> recordIdx;
    std::unordered_map<std::string, core::NifModel> customModels;
    int withModel = 0, avatars = 0, missing = 0;
    std::vector<std::string> missingNames;
};

NpcRenderData CollectNpcRenderData(EditorState& state) {
    NpcRenderData out;
    if (!state.npcTextLoaded) return out;
    EnsureMobViewInfoLoaded(state);
    if (!state.mobViewInfoLoaded) return out;
    EnsureRescharRoot(state);
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return out;
    auto& customModels = out.customModels;
    // Spieler-Avatar-NPCs (MobViewInfo.NpcViewIndex != 0): Aussehen aus NPCViewInfo.shn (Klasse, Geschlecht,
    // Gesicht, Frisur, Ausruestung) - werden mit dem Avatar-Baukasten zusammengesetzt (AvatarPreview).
    const int npcViewDoc = FindOrLoadShnDoc(state, "NPCViewInfo.shn", EditorState::ShnSource::Client);

    core::ObjectPlacementSet& newSet = out.set;
    std::vector<std::size_t>& recordIdx = out.recordIdx;
    for (std::size_t idx : NpcRecordsForCurrentMap(state)) {
        auto& rec = table->records[idx];
        if (rec.values.size() < 5) continue;
        // MobViewInfo-Zeile des NPCs (InxName = NPC.txt-Name)
        std::string fileName;
        long long npcViewIdx = 0;
        for (auto& row : state.mobViewInfoShn.rows) {
            if (row.values.size() < 10) continue;
            if (core::legacy::ShnValueToString(row.values[1]) != rec.values[0]) continue;
            ShnValueAsInt(row.values[9], npcViewIdx);
            fileName = core::legacy::ShnValueToString(row.values[2]);
            break;
        }

        std::string modelPath;
        if (npcViewIdx != 0 && npcViewDoc >= 0 && !state.charRoot.empty()) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(npcViewDoc)].file;
            for (std::size_t r = 0; r < f.rows.size(); ++r) {
                if (std::atoll(ShnCellText(f, r, "TypeIndex").c_str()) != npcViewIdx) continue;
                const std::string key = "avatar:" + std::to_string(npcViewIdx);
                if (!customModels.count(key)) {
                    std::array<std::string, 19> equ;
                    for (int q = 0; q < 19; ++q) equ[static_cast<std::size_t>(q)] = ShnCellText(f, r, kAvatarSlotColumns[q]);
                    auto req = MakeAvatarRequest(state, std::atoi(ShnCellText(f, r, "Class").c_str()), std::atoi(ShnCellText(f, r, "Gender").c_str()) != 0,
                                                 std::atoi(ShnCellText(f, r, "FaceShape").c_str()), std::atoi(ShnCellText(f, r, "HairType").c_str()), equ);
                    if (auto built = core::BuildAvatarModel(req)) customModels.emplace(key, core::AvatarToNifModel(*built));
                }
                if (customModels.count(key)) modelPath = key;
                break;
            }
        } else if (!fileName.empty()) {
            modelPath = FindNpcModelPath(state, fileName);
        }
        if (modelPath.empty()) {
            ++out.missing;
            if (out.missingNames.size() < 12) out.missingNames.push_back(rec.values[0] + (fileName.empty() ? "" : " (" + fileName + ")"));
            continue;
        }
        if (modelPath.rfind("avatar:", 0) == 0) ++out.avatars;
        ++out.withModel;

        core::PlacedObject obj;
        obj.modelPath = modelPath;
        ApplyNpcRecordToObject(state, rec, obj);
        newSet.AddObject(std::move(obj));
        recordIdx.push_back(idx);
    }
    return out;
}

void EnsureNpcModelsLoaded(EditorState& state) {
    if (!state.npcTextLoaded) return;
    if (state.npcRenderSetForMap == state.legacySaveStem) return;
    NpcRenderData data = CollectNpcRenderData(state);
    // Fiktiver, zwei Ebenen tiefer "mapDir" - ResolveLegacyAssetPath leitet daraus per
    // parent_path().parent_path() automatisch clientRoot als Such-Basis her (Trick schon bei
    // der Asset-Vorschau in DrawToolsContent verwendet, siehe GetOrLoadAssetThumbnail).
    const std::filesystem::path fakeMapDir = std::filesystem::path(state.project.clientFolder) / "_npcresolve" / "_probe";
    state.npcRenderSet = std::move(data.set);
    state.npcRenderRecordIdx = std::move(data.recordIdx);
    state.npcModelsMissing = data.missing;
    state.npcModelsMissingNames = std::move(data.missingNames);
    state.npcMeshRenderer.LoadModelsForSet(state.npcRenderSet, fakeMapDir, &data.customModels);
    state.npcRenderSetForMap = state.legacySaveStem;
}

void OpenNpcDialogEditor(EditorState& state, const std::string& npcName) {
    EnsureNpcDialogRoot(state);
    EnsureNpcDialogLoaded(state);
    state.dialogEditorRowIdx = state.npcDialogLoaded ? FindDialogRowForNpc(state, npcName) : -1;
    if (state.dialogEditorRowIdx >= 0 && state.npcDialogShn.columns.size() > 2) {
        auto& row = state.npcDialogShn.rows[static_cast<std::size_t>(state.dialogEditorRowIdx)];
        if (row.values.size() > 2) {
            auto parsed = ParseNpcDialogText(core::legacy::ShnValueToString(row.values[2]));
            state.dialogGreetingBuf = parsed.greeting;
            state.dialogButtonsBuf = parsed.buttons;
        }
    } else {
        state.dialogGreetingBuf.clear();
        state.dialogButtonsBuf.clear();
    }
    state.dialogEditorOpen = true;
}

void DrawDialogEditorPopup(EditorState& state) {
    if (!state.dialogEditorOpen) return;
    ImGui::OpenPopup("NPC-Dialog bearbeiten");
    ImGui::SetNextWindowSize(ImVec2(580.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("NPC-Dialog bearbeiten", &state.dialogEditorOpen)) {
        if (state.npcDialogRessystemRoot.empty()) {
            ImGui::TextWrapped("Client-Ordner 'ressystem' (enthält NpcDialogData.shn) nicht gefunden - bitte manuell angeben:");
            std::vector<char> rootBuf(state.npcDialogRessystemRoot.begin(), state.npcDialogRessystemRoot.end());
            rootBuf.resize(std::max<std::size_t>(rootBuf.size() + 1, 512));
            ImGui::SetNextItemWidth(-1.0f);
            if (UI::InputText("##ressystemroot", rootBuf.data(), rootBuf.size())) {
                state.npcDialogRessystemRoot.assign(rootBuf.data()); state.npcDialogLoaded = false;
            }
#ifdef _WIN32
            if (UI::Button("Ordner wählen...##ressystem")) {
                if (auto p = BrowseForFolderWindows("ressystem-Ordner wählen")) { state.npcDialogRessystemRoot = *p; state.npcDialogLoaded = false; }
            }
#endif
        } else if (state.dialogEditorRowIdx < 0) {
            ImGui::TextWrapped("Kein Dialog-Eintrag für diesen NPC in NpcDialogData.shn gefunden (oder Datei konnte nicht geladen werden).");
        } else {
            ImGui::TextDisabled("Begrüßungstext ([NAME] wird durch den Spielernamen ersetzt):");
            std::vector<char> buf(state.dialogGreetingBuf.begin(), state.dialogGreetingBuf.end());
            buf.resize(std::max<std::size_t>(buf.size() + 1, 2048));
            if (ImGui::InputTextMultiline("##greeting", buf.data(), buf.size(), ImVec2(-1.0f, 100.0f))) {
                state.dialogGreetingBuf.assign(buf.data());
            }
            ImGui::Separator();
            ImGui::Text("Buttons:");
            int removeIdx = -1;
            for (std::size_t i = 0; i < state.dialogButtonsBuf.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& [label, action] = state.dialogButtonsBuf[i];
                std::vector<char> lbuf(label.begin(), label.end()); lbuf.resize(std::max<std::size_t>(lbuf.size() + 1, 128));
                std::vector<char> abuf(action.begin(), action.end()); abuf.resize(std::max<std::size_t>(abuf.size() + 1, 128));
                ImGui::SetNextItemWidth(180.0f);
                if (UI::InputText("##label", lbuf.data(), lbuf.size())) label.assign(lbuf.data());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(220.0f);
                if (UI::InputText("##action", abuf.data(), abuf.size())) action.assign(abuf.data());
                ImGui::SameLine();
                if (UI::Button("Entfernen")) removeIdx = static_cast<int>(i);
                ImGui::PopID();
            }
            if (removeIdx >= 0) state.dialogButtonsBuf.erase(state.dialogButtonsBuf.begin() + removeIdx);
            if (UI::Button("+ Button hinzufügen")) state.dialogButtonsBuf.emplace_back("Neuer Button", "server_ack 1");
            ImGui::TextDisabled("Übliche Aktionen: 'server_ack <Zahl>' (Server-Callback, z.B. Kauf) oder\n'opendlg <Name>' (verketteter Dialog, z.B. zu einem Aufwertungs-Menü).");
            ImGui::Separator();
            if (UI::Button("Speichern")) {
                ParsedNpcDialog d; d.greeting = state.dialogGreetingBuf; d.buttons = state.dialogButtonsBuf;
                const auto text = SerializeNpcDialogText(d);
                auto& row = state.npcDialogShn.rows[static_cast<std::size_t>(state.dialogEditorRowIdx)];
                auto parsedVal = core::legacy::ParseShnValue(state.npcDialogShn.columns[2], text);
                if (parsedVal) {
                    row.values[2] = std::move(*parsedVal);
                    auto path = std::filesystem::path(state.npcDialogRessystemRoot) / "NpcDialogData.shn";
                    auto saved = core::legacy::SaveShnFile(state.npcDialogShn, path);
                    state.statusMessage = saved ? std::string("NpcDialogData.shn gespeichert.") : "Fehler: " + saved.error();
                    if (saved) { state.dialogEditorOpen = false; ImGui::CloseCurrentPopup(); }
                } else {
                    state.statusMessage = "Fehler: " + parsedVal.error();
                }
            }
        }
        ImGui::SameLine();
        if (UI::Button("Schließen")) { state.dialogEditorOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// Baut die Item-Auswahlliste aus ItemInfo.shn (Server bevorzugt, sonst Client).
void EnsureItemLookup(EditorState& state) {
    if (state.itemLookupTried) return;
    state.itemLookupTried = true;
    EnsureItemInfoLoadedForQuests(state);
    if (!state.itemInfoLoaded) return;
    const auto& f = state.itemInfoShn;
    int cId = -1, cInx = -1, cName = -1;
    for (std::size_t i = 0; i < f.columns.size(); ++i) {
        if (f.columns[i].name == "ID") cId = static_cast<int>(i);
        else if (f.columns[i].name == "InxName") cInx = static_cast<int>(i);
        else if (f.columns[i].name == "Name") cName = static_cast<int>(i);
    }
    if (cInx < 0) return;
    for (const auto& row : f.rows) {
        if (static_cast<std::size_t>(cInx) >= row.values.size()) continue;
        EditorState::ShopItemEntry e;
        e.inx = core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cInx)]);
        if (e.inx.empty()) continue;
        if (cName >= 0 && static_cast<std::size_t>(cName) < row.values.size()) e.name = core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cName)]);
        long long id = 0;
        if (cId >= 0 && static_cast<std::size_t>(cId) < row.values.size() && ShnValueAsInt(row.values[static_cast<std::size_t>(cId)], id)) e.id = id;
        state.itemEntries.push_back(std::move(e));
    }
    std::sort(state.itemEntries.begin(), state.itemEntries.end(), [](const auto& a, const auto& b) { return a.inx < b.inx; });
    for (std::size_t i = 0; i < state.itemEntries.size(); ++i) state.itemByInx.emplace(state.itemEntries[i].inx, i);
}

// Nummeriert die Records einer Shop-Tabelle neu (Spalte "Rec": 0,1,2,... - im Original mit
// nachgestelltem Leerzeichen "0 ").
void RenumberShopRows(core::legacy::ShineTable& table) {
    for (std::size_t i = 0; i < table.records.size(); ++i) {
        if (table.records[i].values.empty()) table.records[i].values.resize(1);
        table.records[i].values[0] = std::to_string(i) + " ";
    }
}

core::legacy::ShineRecord MakeEmptyShopRow(const core::legacy::ShineTable& table) {
    core::legacy::ShineRecord row;
    row.sourceLine = 0;
    row.values.assign(std::max<std::size_t>(table.columns.size(), 2), "-");
    row.values[0] = "0 ";
    return row;
}

// Legt fuer einen NPC eine neue, leere Shop-Datei an (Kopfzeilen und Tabellenaufbau wie eine
// beliebige vorhandene NPCItemList-Datei). true bei Erfolg (Datei wird erst beim Speichern
// geschrieben).
bool CreateNewShopFile(EditorState& state, const std::string& npcName) {
    const auto dir = std::filesystem::path(state.shineTextRoot) / "NPCItemList";
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) { state.statusMessage = "Ordner NPCItemList nicht gefunden: " + dir.string(); return false; }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".txt") continue;
        auto tmpl = core::legacy::LoadShineTextFile(entry.path());
        if (!tmpl || tmpl->tables.empty()) continue;
        core::legacy::ShineTextFile nf = core::legacy::MakeShineFileWithHeaderOf(*tmpl, dir / (npcName + ".txt"));
        core::legacy::ShineTable t;
        t.name = "Tab00";
        t.isNew = true;
        t.columns = tmpl->tables.front().columns;
        t.records.push_back(MakeEmptyShopRow(t));
        RenumberShopRows(t);
        nf.tables.push_back(std::move(t));
        state.shopTextFile = std::move(nf);
        state.shopTextLoaded = true;
        state.shopLoadedForNpc = npcName;
        state.shopFileIsNew = true;
        return true;
    }
    state.statusMessage = "Keine Vorlage in NPCItemList gefunden.";
    return false;
}

void DrawShopEditorPopup(EditorState& state) {
    if (!state.shopEditorOpen) return;
    ImGui::OpenPopup("Händler-Inventar");
    ImGui::SetNextWindowSize(ImVec2(940.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Händler-Inventar", &state.shopEditorOpen)) {
        EnsureItemLookup(state);
        const auto shopPath = std::filesystem::path(state.shineTextRoot) / "NPCItemList" / (state.shopLoadedForNpc + ".txt");
        ImGui::Text("Shop von: %s", state.shopLoadedForNpc.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s%s)", shopPath.string().c_str(), state.shopFileIsNew ? " - NEU, noch nicht gespeichert" : "");
        ImGui::TextDisabled("Jede Zeile = ein Regal mit bis zu 6 Items, jeder Tab = eine Kategorie im Shop-Fenster. Slot klicken = Item wählen, Rechtsklick = leeren.");
        ImGui::Separator();
        if (!state.shopTextLoaded) {
            ImGui::TextWrapped("Für diesen NPC gibt es noch keine Shop-Datei (NPCItemList/%s.txt).", state.shopLoadedForNpc.c_str());
            if (UI::Button("Shop-Datei neu anlegen")) CreateNewShopFile(state, state.shopLoadedForNpc);
        } else {
            auto& file = state.shopTextFile;
            int deleteTable = -1;
            bool structureChanged = false;
            if (ImGui::BeginTabBar("##shoptabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
                for (std::size_t t = 0; t < file.tables.size(); ++t) {
                    auto& table = file.tables[t];
                    const std::string tabLabel = table.name + " (" + std::to_string(table.records.size()) + ")###shoptab" + std::to_string(t);
                    if (!ImGui::BeginTabItem(tabLabel.c_str())) continue;
                    const int slotCols = static_cast<int>(table.columns.size()) - 1;
                    if (ImGui::BeginTable("##shopgrid", slotCols + 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 380.0f))) {
                        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 32.0f);
                        for (int c = 0; c < slotCols; ++c) ImGui::TableSetupColumn(("Slot " + std::to_string(c + 1)).c_str());
                        ImGui::TableSetupColumn("Zeile", ImGuiTableColumnFlags_WidthFixed, 96.0f);
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableHeadersRow();
                        int moveRow = -1, moveDir = 0, removeRow = -1;
                        for (std::size_t ri = 0; ri < table.records.size(); ++ri) {
                            auto& rec = table.records[ri];
                            if (rec.values.size() < table.columns.size()) rec.values.resize(table.columns.size(), "-");
                            ImGui::PushID(static_cast<int>(ri));
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%zu", ri);
                            for (int c = 0; c < slotCols; ++c) {
                                ImGui::TableSetColumnIndex(c + 1);
                                ImGui::PushID(c);
                                std::string& v = rec.values[static_cast<std::size_t>(c) + 1];
                                const bool isEmpty = v.empty() || v == "-";
                                const auto found = state.itemByInx.find(v);
                                const bool known = isEmpty || found != state.itemByInx.end() || state.itemEntries.empty();
                                if (!known) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(150, 40, 40, 255));
                                else if (isEmpty) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 50, 58, 255));
                                if (UI::Button((isEmpty ? std::string("(leer)") : v).c_str(), ImVec2(-1.0f, 0.0f))) {
                                    state.shopPickTable = static_cast<int>(t);
                                    state.shopPickRow = static_cast<int>(ri);
                                    state.shopPickCol = c + 1;
                                    state.shopItemFilter[0] = '\0';
                                    state.shopPickRequested = true;
                                }
                                if (!known || isEmpty) ImGui::PopStyleColor();
                                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { v = "-"; }
                                if (ImGui::IsItemHovered()) {
                                    if (isEmpty) ImGui::SetTooltip("Leerer Slot - klicken, um ein Item zu wählen");
                                    else if (found != state.itemByInx.end()) {
                                        const auto& e = state.itemEntries[found->second];
                                        ImGui::SetTooltip("%s\nID %lld\nRechtsklick = Slot leeren", e.name.c_str(), e.id);
                                    } else if (!state.itemEntries.empty()) ImGui::SetTooltip("'%s' ist NICHT in ItemInfo.shn - Tippfehler oder fehlendes Item?", v.c_str());
                                }
                                ImGui::PopID();
                            }
                            ImGui::TableSetColumnIndex(slotCols + 1);
                            if (UI::SmallButton("^")) { moveRow = static_cast<int>(ri); moveDir = -1; }
                            ImGui::SameLine();
                            if (UI::SmallButton("v")) { moveRow = static_cast<int>(ri); moveDir = 1; }
                            ImGui::SameLine();
                            if (state.shopDeleteArmed && UI::SmallButton("x")) removeRow = static_cast<int>(ri);
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                        if (moveRow >= 0) {
                            const int other = moveRow + moveDir;
                            if (other >= 0 && other < static_cast<int>(table.records.size())) {
                                std::swap(table.records[static_cast<std::size_t>(moveRow)], table.records[static_cast<std::size_t>(other)]);
                                structureChanged = true;
                            }
                        }
                        if (removeRow >= 0) { table.records.erase(table.records.begin() + removeRow); structureChanged = true; }
                    }
                    if (UI::Button("+ Zeile")) {
                        table.records.push_back(MakeEmptyShopRow(table));
                        structureChanged = true;
                    }
                    if (state.shopDeleteArmed && file.tables.size() > 1) {
                        ImGui::SameLine();
                        if (UI::Button("Diesen Tab löschen")) deleteTable = static_cast<int>(t);
                    }
                    if (structureChanged) RenumberShopRows(table);
                    ImGui::EndTabItem();
                }
                if (UI::TabItemButton("+ Tab", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
                    core::legacy::ShineTable nt;
                    int maxNo = -1;
                    for (const auto& t : file.tables) {
                        if (t.name.size() > 3 && t.name.compare(0, 3, "Tab") == 0) maxNo = std::max(maxNo, std::atoi(t.name.c_str() + 3));
                    }
                    char nm[16];
                    std::snprintf(nm, sizeof(nm), "Tab%02d", maxNo + 1);
                    nt.name = nm;
                    nt.isNew = true;
                    nt.columns = file.tables.empty() ? std::vector<core::legacy::ShineColumn>{} : file.tables.front().columns;
                    if (nt.columns.empty()) {
                        nt.columns.push_back({"Rec", "BYTE"});
                        for (int c = 0; c < 6; ++c) { char cn[16]; std::snprintf(cn, sizeof(cn), "Column%02d", c); nt.columns.push_back({cn, "String[33]"}); }
                    }
                    nt.records.push_back(MakeEmptyShopRow(nt));
                    RenumberShopRows(nt);
                    file.tables.push_back(std::move(nt));
                }
                ImGui::EndTabBar();
            }
            if (deleteTable >= 0) { file.tables.erase(file.tables.begin() + deleteTable); state.shopDeleteArmed = false; }

            // Item-Auswahl (Popup): Suche in InxName und Anzeigename.
            if (state.shopPickRequested) { ImGui::OpenPopup("Item wählen##shop"); state.shopPickRequested = false; }
            if (ImGui::BeginPopup("Item wählen##shop")) {
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                ImGui::SetNextItemWidth(520.0f);
                UI::InputTextWithHint("##itemfilter", "Suche (Name oder Item-Bezeichnung)", state.shopItemFilter, sizeof(state.shopItemFilter));
                const std::string needle = LowerAscii(state.shopItemFilter);
                ImGui::BeginChild("##itemlist", ImVec2(520.0f, 320.0f), true);
                if (UI::Selectable("(leer) -")) {
                    if (state.shopPickTable >= 0 && state.shopPickTable < static_cast<int>(file.tables.size())) {
                        auto& tb = file.tables[static_cast<std::size_t>(state.shopPickTable)];
                        if (state.shopPickRow >= 0 && state.shopPickRow < static_cast<int>(tb.records.size()))
                            tb.records[static_cast<std::size_t>(state.shopPickRow)].values[static_cast<std::size_t>(state.shopPickCol)] = "-";
                    }
                    ImGui::CloseCurrentPopup();
                }
                int shown = 0;
                for (const auto& e : state.itemEntries) {
                    if (!needle.empty() && LowerAscii(e.inx).find(needle) == std::string::npos && LowerAscii(e.name).find(needle) == std::string::npos) continue;
                    if (++shown > 300) { ImGui::TextDisabled("... weitere Treffer - Suche eingrenzen"); break; }
                    const std::string label = e.inx + "   " + e.name + "   (#" + std::to_string(e.id) + ")##" + std::to_string(shown);
                    if (UI::Selectable(label.c_str())) {
                        if (state.shopPickTable >= 0 && state.shopPickTable < static_cast<int>(file.tables.size())) {
                            auto& tb = file.tables[static_cast<std::size_t>(state.shopPickTable)];
                            if (state.shopPickRow >= 0 && state.shopPickRow < static_cast<int>(tb.records.size()))
                                tb.records[static_cast<std::size_t>(state.shopPickRow)].values[static_cast<std::size_t>(state.shopPickCol)] = e.inx;
                        }
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndChild();
                if (state.itemEntries.empty()) ImGui::TextDisabled("ItemInfo.shn nicht geladen - Item-Bezeichnung von Hand eingeben ist im Raster nicht möglich.");
                ImGui::EndPopup();
            }

            ImGui::Separator();
            UI::Checkbox("Löschen freigeben", &state.shopDeleteArmed);
            ImGui::SameLine();
            if (UI::Button("Speichern")) {
                std::error_code ec;
                std::filesystem::create_directories(shopPath.parent_path(), ec);
                auto saved = core::legacy::SaveShineTextFile(state.shopTextFile, shopPath);
                if (saved) {
                    state.statusMessage = "Händler-Inventar gespeichert: " + state.shopLoadedForNpc;
                    // Neu laden: Tabellen/Records bekommen ihre echten Quellzeilen, "neu"-Markierungen entfallen.
                    const std::string npc = state.shopLoadedForNpc;
                    state.shopTextLoaded = false;
                    state.shopFileIsNew = false;
                    EnsureShopTextLoaded(state, npc);
                } else {
                    state.statusMessage = "Fehler: " + saved.error();
                }
            }
        }
        ImGui::SameLine();
        if (UI::Button("Schließen")) { state.shopEditorOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// ============================================================================
// Custom NPC / Mob Assistent (SHN-Editor-Tab "Custom NPC/Mob") - CHANGELOG [0.44.28].
//
// Datengrundlage (gegen NA2016 geprueft): ein NPC oder Monster steckt ueberall unter derselben ID
// und demselben InxName in MobInfo (Client+Server), MobInfoServer, MobViewInfo (Client+Server),
// MobSpecies, QuestSpecies und MobWeapon (Server, ggf. mehrere Zeilen). NPCs zusaetzlich in
// NpcDialogData (Dialog) und World/NPC.txt (Platzierung/Rolle). Ein "Spieler mit Ruestung" ist
// KEIN eigenes Modell, sondern eine Zeile in NPCViewInfo.shn (Klasse, Geschlecht, Gesicht,
// Frisur, Haarfarbe, 19 Ausruestungs-Slots mit Item-InxNames), auf die MobViewInfo.NpcViewIndex
// zeigt. Der Assistent klont eine Vorlage in alle Tabellen (neue Zeilen ans Ende - die Tabellen
// haben dieselbe Zeilenanzahl/-reihenfolge) und vergibt die naechste freie ID.
// ============================================================================
// Generische Auswahl-Liste im Popup (Suche + Liste), fuer Item-Namen und Modell-Dateinamen.
static bool StringPickerPopup(const char* popupId, const std::vector<std::string>& options, char* filterBuf, std::size_t filterSize,
                              std::string& chosen, const std::vector<std::string>* labels = nullptr) {
    bool picked = false;
    if (ImGui::BeginPopup(popupId)) {
        // Popups sind AlwaysAutoResize: eine relative Kindhoehe (0/-x) schrumpfte das Fenster auf ein
        // Minimum ("minimiert"). Deshalb FESTE Breite/Hoehe (CHANGELOG [0.44.29]).
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(520.0f);
        UI::InputTextWithHint("##pickfilter", "Suche", filterBuf, filterSize);
        const std::string needle = LowerAscii(filterBuf);
        ImGui::BeginChild("##picklist", ImVec2(520.0f, 320.0f), true);
        if (UI::Selectable("(leer) -")) { chosen = "-"; picked = true; ImGui::CloseCurrentPopup(); }
        int shown = 0;
        for (std::size_t i = 0; i < options.size(); ++i) {
            const std::string& label = labels ? (*labels)[i] : options[i];
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            if (++shown > 300) { ImGui::TextDisabled("... weitere Treffer - Suche eingrenzen"); break; }
            if (UI::Selectable((label + "##opt" + std::to_string(i)).c_str())) { chosen = options[i]; picked = true; ImGui::CloseCurrentPopup(); }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    return picked;
}

// Klont die Vorlage-Zeilen (ID + Name) einer Tabelle unter neuer ID/neuem Namen ans Ende. Liefert
// die Indizes der neuen Zeilen (leer, wenn Datei fehlt oder keine Vorlage-Zeile existiert).
static std::vector<std::size_t> CloneRowsById(EditorState& state, const char* fileName, EditorState::ShnSource src,
                                              const char* nameCol, long long templateId, const std::string& templateInx,
                                              long long newId, const std::string& newInx, std::vector<std::string>& report) {
    std::vector<std::size_t> created;
    const int di = FindOrLoadShnDoc(state, fileName, src);
    const std::string label = std::string(ShnSourceName(src)) + " " + fileName;
    if (di < 0) { report.push_back("- " + label + ": nicht geladen (übersprungen)"); return created; }
    auto& doc = state.shnFiles[static_cast<std::size_t>(di)];
    auto& f = doc.file;
    const int cId = ShnColumnIndexByName(f, "ID");
    const int cName = ShnColumnIndexByName(f, nameCol);
    if (cId < 0 || cName < 0) { report.push_back("- " + label + ": Spalten ID/" + nameCol + " fehlen (übersprungen)"); return created; }
    std::vector<std::size_t> sources;
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        long long id = -1;
        if (!ShnValueAsInt(f.rows[r].values[static_cast<std::size_t>(cId)], id) || id != templateId) continue;
        if (core::legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(cName)]) != templateInx) continue;
        sources.push_back(r);
    }
    for (const std::size_t r : sources) {
        core::legacy::ShnRow row = f.rows[r];
        f.rows.push_back(std::move(row));
        const std::size_t nr = f.rows.size() - 1;
        SetShnCellText(f, nr, "ID", std::to_string(newId));
        SetShnCellText(f, nr, nameCol, newInx);
        created.push_back(nr);
    }
    if (!created.empty()) {
        doc.dirty = true;
        EnsureCellStatusSize(doc);
        ++state.shnEditCounter;
        report.push_back("+ " + label + ": " + std::to_string(created.size()) + " Zeile(n) angelegt");
    } else {
        report.push_back("- " + label + ": keine Vorlage-Zeile gefunden (übersprungen)");
    }
    return created;
}

static void RunCreateCreature(EditorState& state) {
    auto& w = state.wiz;
    w.report.clear();
    const std::string newInx = w.newInx;
    if (newInx.empty() || w.templateId < 0) { w.report.push_back("Bitte Vorlage und InxName angeben."); return; }
    // Eindeutigkeit des Namens
    {
        const int mi = FindOrLoadShnDoc(state, "MobInfo.shn", EditorState::ShnSource::Client);
        if (mi < 0) { w.report.push_back("MobInfo.shn (Client) nicht geladen - Projekt-Ordner prüfen."); return; }
        const auto& f = state.shnFiles[static_cast<std::size_t>(mi)].file;
        for (std::size_t r = 0; r < f.rows.size(); ++r) {
            if (ShnCellText(f, r, "InxName") == newInx) { w.report.push_back("InxName '" + newInx + "' existiert bereits in MobInfo.shn."); return; }
        }
    }
    // Freie ID ueber ALLE beteiligten Tabellen (Client+Server) - dieselbe ID muss ueberall frei sein.
    struct Spec { const char* file; EditorState::ShnSource src; const char* nameCol; };
    const Spec specs[] = {
        {"MobInfo.shn", EditorState::ShnSource::Client, "InxName"},        {"MobInfo.shn", EditorState::ShnSource::Server, "InxName"},
        {"MobInfoServer.shn", EditorState::ShnSource::Server, "InxName"},  {"MobViewInfo.shn", EditorState::ShnSource::Client, "InxName"},
        {"MobViewInfo.shn", EditorState::ShnSource::Server, "InxName"},    {"MobSpecies.shn", EditorState::ShnSource::Server, "MobName"},
        {"QuestSpecies.shn", EditorState::ShnSource::Server, "MobGroupName"}, {"MobWeapon.shn", EditorState::ShnSource::Server, "InxName"},
    };
    std::set<long long> used;
    const core::legacy::ShnColumn* idColumn = nullptr;
    for (const auto& sp : specs) {
        const int di = FindOrLoadShnDoc(state, sp.file, sp.src);
        if (di < 0) continue;
        const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        const int c = ShnColumnIndexByName(f, "ID");
        CollectShnInts(f, c, used);
        if (!idColumn && std::string(sp.file) == "MobInfo.shn" && c >= 0) idColumn = &f.columns[static_cast<std::size_t>(c)];
    }
    long long newId = w.autoId ? -1 : w.manualId;
    if (w.autoId) {
        if (!idColumn) { w.report.push_back("Keine ID-Spalte gefunden."); return; }
        newId = SuggestFreeShnId(*idColumn, used);
    } else if (used.count(newId)) {
        w.report.push_back("ID " + std::to_string(newId) + " ist bereits vergeben.");
        return;
    }
    if (newId < 0) { w.report.push_back("Keine freie ID gefunden."); return; }
    w.report.push_back("Neue ID: " + std::to_string(newId) + "   InxName: " + newInx + "   Vorlage: " + w.templateInx + " (#" + std::to_string(w.templateId) + ")");

    // 1) Klonen
    std::vector<std::size_t> viewRowsClient, viewRowsServer;
    for (const auto& sp : specs) {
        auto created = CloneRowsById(state, sp.file, sp.src, sp.nameCol, w.templateId, w.templateInx, newId, newInx, w.report);
        const std::string fname = sp.file;
        const int di = FindOrLoadShnDoc(state, sp.file, sp.src);
        if (di < 0 || created.empty()) continue;
        auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        for (const std::size_t nr : created) {
            if (fname == "MobInfo.shn") {
                SetShnCellText(f, nr, "Name", w.displayName);
                SetShnCellText(f, nr, "IsNPC", w.isNpc ? "1" : "0");
                if (!w.isNpc) {
                    SetShnCellText(f, nr, "Level", std::to_string(w.level));
                    SetShnCellText(f, nr, "MaxHP", std::to_string(w.maxHp));
                    SetShnCellText(f, nr, "WalkSpeed", std::to_string(w.walkSpeed));
                    SetShnCellText(f, nr, "RunSpeed", std::to_string(w.runSpeed));
                    SetShnCellText(f, nr, "Size", std::to_string(w.size));
                }
            } else if (fname == "MobViewInfo.shn") {
                (sp.src == EditorState::ShnSource::Client ? viewRowsClient : viewRowsServer).push_back(nr);
                if (w.lookMode == 1 && w.modelFile[0]) SetShnCellText(f, nr, "FileName", w.modelFile);
            }
        }
    }

    // 2) Spieler-Avatar (NPCViewInfo) - nur NPC
    if (w.isNpc && w.lookMode == 2) {
        const int vi = FindOrLoadShnDoc(state, "NPCViewInfo.shn", EditorState::ShnSource::Client);
        if (vi < 0) {
            w.report.push_back("- Client NPCViewInfo.shn: nicht geladen - Avatar-Look nicht angelegt.");
        } else {
            auto& doc = state.shnFiles[static_cast<std::size_t>(vi)];
            auto& f = doc.file;
            std::set<long long> usedTypes;
            CollectShnInts(f, ShnColumnIndexByName(f, "TypeIndex"), usedTypes);
            const int ct = ShnColumnIndexByName(f, "TypeIndex");
            const long long typeIdx = ct >= 0 ? SuggestFreeShnId(f.columns[static_cast<std::size_t>(ct)], usedTypes) : -1;
            // Strukturvorlage: die Zeile, auf die die Vorlage-Kopie zeigt (falls Avatar), sonst die erste.
            std::size_t srcRow = 0;
            if (!viewRowsClient.empty()) {
                long long tplView = 0;
                {
                    const int mvc = FindOrLoadShnDoc(state, "MobViewInfo.shn", EditorState::ShnSource::Client);
                    if (mvc >= 0) {
                        const std::string txt = ShnCellText(state.shnFiles[static_cast<std::size_t>(mvc)].file, viewRowsClient[0], "NpcViewIndex");
                        tplView = std::atoll(txt.c_str());
                    }
                }
                for (std::size_t r = 0; r < f.rows.size(); ++r) {
                    long long t = -1;
                    if (ct >= 0 && ShnValueAsInt(f.rows[r].values[static_cast<std::size_t>(ct)], t) && t == tplView && tplView != 0) { srcRow = r; break; }
                }
            }
            if (typeIdx < 0 || f.rows.empty()) {
                w.report.push_back("- Client NPCViewInfo.shn: keine freie TypeIndex-Nummer - Avatar-Look nicht angelegt.");
            } else {
                core::legacy::ShnRow row = f.rows[srcRow];
                f.rows.push_back(std::move(row));
                const std::size_t nr = f.rows.size() - 1;
                SetShnCellText(f, nr, "TypeIndex", std::to_string(typeIdx));
                SetShnCellText(f, nr, "Class", std::to_string(w.avClass));
                SetShnCellText(f, nr, "Gender", std::to_string(w.avGender));
                SetShnCellText(f, nr, "FaceShape", std::to_string(w.avFace));
                SetShnCellText(f, nr, "HairType", std::to_string(w.avHairType));
                SetShnCellText(f, nr, "HairColor", std::to_string(w.avHairColor));
                for (int i = 0; i < 19; ++i) SetShnCellText(f, nr, kAvatarSlotColumns[i], w.equ[static_cast<std::size_t>(i)].empty() ? "-" : w.equ[static_cast<std::size_t>(i)]);
                doc.dirty = true;
                EnsureCellStatusSize(doc);
                ++state.shnEditCounter;
                w.report.push_back("+ Client NPCViewInfo.shn: Avatar-Zeile TypeIndex " + std::to_string(typeIdx) + " angelegt");
                for (const auto src : {EditorState::ShnSource::Client, EditorState::ShnSource::Server}) {
                    const int mvi = FindOrLoadShnDoc(state, "MobViewInfo.shn", src);
                    if (mvi < 0) continue;
                    auto& mv = state.shnFiles[static_cast<std::size_t>(mvi)];
                    for (const std::size_t vr : (src == EditorState::ShnSource::Client ? viewRowsClient : viewRowsServer)) {
                        SetShnCellText(mv.file, vr, "NpcViewIndex", std::to_string(typeIdx));
                    }
                }
            }
        }
    }

    // 3) Dialog (NPC)
    if (w.isNpc && w.copyDialog) {
        const int di = FindOrLoadShnDoc(state, "NpcDialogData.shn", EditorState::ShnSource::Client);
        if (di >= 0) {
            auto& doc = state.shnFiles[static_cast<std::size_t>(di)];
            for (std::size_t r = 0; r < doc.file.rows.size(); ++r) {
                if (ShnCellText(doc.file, r, "MobIDX") != w.templateInx) continue;
                core::legacy::ShnRow row = doc.file.rows[r];
                doc.file.rows.push_back(std::move(row));
                SetShnCellText(doc.file, doc.file.rows.size() - 1, "MobIDX", newInx);
                doc.dirty = true;
                EnsureCellStatusSize(doc);
                w.report.push_back("+ Client NpcDialogData.shn: Dialog der Vorlage kopiert");
                break;
            }
        }
    }

    // 4) Platzierung in NPC.txt (nur NPC)
    if (w.isNpc && w.placeOnMap && state.legacySaveStem[0] != '\0') {
        EnsureNpcTextLoaded(state);
        if (auto* table = state.npcTextFile.FindTable("ShineNPC")) {
            core::legacy::ShineRecord rec;
            rec.sourceLine = 0;
            const std::string role = kNpcRoles[std::clamp(w.roleIdx, 0, 5)];
            rec.values = {newInx, state.legacySaveStem, std::to_string(w.placeX), std::to_string(w.placeY), std::to_string(w.placeDir), "1", role, w.roleArg};
            table->records.push_back(std::move(rec));
            w.report.push_back(std::string("+ World/NPC.txt: NPC auf '") + state.legacySaveStem + "' bei (" + std::to_string(w.placeX) + ", " + std::to_string(w.placeY) + ") vorgemerkt (Speichern im NPC-Tab oder unten).");
        } else {
            w.report.push_back("- World/NPC.txt nicht geladen - nicht platziert.");
        }
    }
    w.report.push_back("Fertig. Die geänderten Tabellen sind noch NICHT gespeichert - unten 'Alle geänderten SHN speichern'.");
}

// Baut (bei geaenderten Parametern) das Avatar-Vorschaumodell und rendert es in die GL-Textur.
static bool g_disableGlUploads = false; // Tests ohne GL
// Baut die Avatar-Anfrage (Klasse, Geschlecht, Gesicht, Haare, Ausruestung) inkl. ItemViewInfo-/HairInfo-Daten
// - gemeinsam genutzt von der Assistenten-Vorschau und vom Rendern von Avatar-NPCs im 3D-Editor.
core::AvatarRequest MakeAvatarRequest(EditorState& state, int cls, bool male, int face, int hairType, const std::array<std::string, 19>& equ) {
    // Item-Daten (ItemViewInfo: Textur, Set-Nummer, Link-Datei) einmal aufbauen
    if (!state.itemViewBuilt) {
        state.itemViewBuilt = true;
        state.itemViewByInx.clear();
        const int vi = FindOrLoadShnDoc(state, "ItemViewInfo.shn", EditorState::ShnSource::Client);
        if (vi >= 0) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(vi)].file;
            const int cInx = ShnColumnIndexByName(f, "InxName"), cTex = ShnColumnIndexByName(f, "TextureFile"), cLink = ShnColumnIndexByName(f, "LinkFile");
            const int cM = ShnColumnIndexByName(f, "MSetNo"), cF = ShnColumnIndexByName(f, "FSetNo");
            for (std::size_t r = 0; r < f.rows.size() && cInx >= 0; ++r) {
                EditorState::ItemViewData d;
                d.textureFile = cTex >= 0 ? ShnCellText(f, r, "TextureFile") : std::string();
                d.linkFile = cLink >= 0 ? ShnCellText(f, r, "LinkFile") : std::string();
                d.mSet = cM >= 0 ? std::atoi(ShnCellText(f, r, "MSetNo").c_str()) : 0;
                d.fSet = cF >= 0 ? std::atoi(ShnCellText(f, r, "FSetNo").c_str()) : 0;
                state.itemViewByInx.emplace(ShnCellText(f, r, "InxName"), d);
            }
        }
    }
    core::AvatarRequest req;
    req.charRoot = state.charRoot;
    req.itemRoot = state.itemRoot;
    req.classIdx = cls;
    req.male = male;
    req.faceShape = std::max(1, face);
    // Haare aus HairInfo (ID = Frisur)
    {
        const int hi = FindOrLoadShnDoc(state, "HairInfo.shn", EditorState::ShnSource::Client);
        if (hi >= 0) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(hi)].file;
            for (std::size_t r = 0; r < f.rows.size(); ++r) {
                if (std::atoi(ShnCellText(f, r, "ID").c_str()) != hairType) continue;
                req.hairFront = ShnCellText(f, r, "acModelName_Front");
                req.hairBottom = ShnCellText(f, r, "acModelName_Bottom");
                req.hairTop = ShnCellText(f, r, "acModelName_Top");
                req.hairTexture = ShnCellText(f, r, "FrontTex");
                break;
            }
        }
    }
    static const char* const kSlotOf[19] = {"RightHand", "LeftHand", "Body", "Leg", "Shoes"};
    for (int i = 0; i < 5; ++i) {
        const std::string& inx = equ[static_cast<std::size_t>(i)];
        if (inx.empty() || inx == "-") continue;
        const auto it = state.itemViewByInx.find(inx);
        core::AvatarItem item;
        item.slot = kSlotOf[i];
        if (it != state.itemViewByInx.end()) {
            item.textureFile = it->second.textureFile;
            item.linkFile = it->second.linkFile;
            item.setNo = req.male ? it->second.mSet : it->second.fSet;
        }
        req.items.push_back(std::move(item));
    }
    return req;
}

static void UpdateAvatarPreview(EditorState& state) {
    auto& w = state.wiz;
    if (state.charRoot.empty()) { state.avatarError = "Ordner 'reschar' nicht gefunden (Client-Ordner im Projekt prüfen)."; state.avatarModel.reset(); return; }
    core::AvatarRequest req = MakeAvatarRequest(state, w.avClass, w.avGender != 0, w.avFace, w.avHairType, w.equ);
    std::string key = std::to_string(req.classIdx) + (req.male ? "m" : "f") + std::to_string(req.faceShape) + "|" + req.hairFront + "|";
    for (const auto& item : req.items) key += item.slot + ":" + item.textureFile + ":" + std::to_string(item.setNo) + ":" + item.linkFile + ";";
    if (key != state.avatarKey) {
        state.avatarKey = key;
        auto built = core::BuildAvatarModel(req);
        if (built) { state.avatarModel = std::make_shared<const core::AvatarModel>(std::move(*built)); state.avatarError.clear(); }
        else { state.avatarModel.reset(); state.avatarError = built.error(); }
        state.avatarRenderedYaw = 1e9f; // neu rendern
    }
    if (!state.avatarModel || g_disableGlUploads) return;
    if (state.avatarTex != 0 && state.avatarYaw == state.avatarRenderedYaw) return; // nichts geaendert
    state.avatarRenderedYaw = state.avatarYaw;
    const int kW = 300, kH = 460;
    const auto pixels = core::RenderAvatarModel(*state.avatarModel, state.avatarYaw, kW, kH);
    if (state.avatarTex == 0) glGenTextures(1, &state.avatarTex);
    glBindTexture(GL_TEXTURE_2D, state.avatarTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kW, kH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DrawCustomCreatureEditor(EditorState& state) {
    auto& w = state.wiz;
    EnsureItemLookup(state);
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "CUSTOM NPC / MOB");
    ImGui::SameLine();
    ImGui::TextDisabled("Vorlage klonen · Werte anpassen · Aussehen wählen · optional platzieren");
    ImGui::Separator();
    if (DrawIconButton("customNpc", "NPC", DrawIconPerson, w.isNpc, ImVec2(94,58)))
        w.isNpc = true;
    ImGui::SameLine();
    if (DrawIconButton("customMob", "Monster", DrawIconSpawn, !w.isNpc, ImVec2(94,58))) {
        w.isNpc = false;
        if (w.lookMode == 2) w.lookMode = 0;
    }
    ImGui::SameLine();
    ImGui::TextWrapped("Klont die Vorlage konsistent in die zugehörigen Client-/Server-Tabellen. "
                       "Die neue ID wird auf Wunsch automatisch über alle beteiligten Tabellen hinweg gewählt.");

    // --- 1. Vorlage
    ImGui::SeparatorText("1. Vorlage");
    const int mi = FindOrLoadShnDoc(state, "MobInfo.shn", EditorState::ShnSource::Client);
    if (mi < 0) { ImGui::TextWrapped("MobInfo.shn (Client) nicht geladen - Client-Ordner im Projekt prüfen."); return; }
    auto& mobInfo = state.shnFiles[static_cast<std::size_t>(mi)].file;
    ImGui::SetNextItemWidth(260.0f);
    UI::InputTextWithHint("##tplfilter", "Vorlage suchen (Name/InxName)", w.templateFilter, sizeof(w.templateFilter));
    ImGui::SameLine();
    ImGui::TextDisabled(w.templateId >= 0 ? "gewählt: %s (#%lld)" : "noch keine Vorlage", w.templateInx.c_str(), w.templateId);
    ImGui::BeginChild("##tpllist", ImVec2(0.0f, 130.0f), true);
    {
        const std::string needle = LowerAscii(w.templateFilter);
        const int cId = ShnColumnIndexByName(mobInfo, "ID");
        int shown = 0;
        for (std::size_t r = 0; r < mobInfo.rows.size() && shown < 150; ++r) {
            const bool npc = ShnCellText(mobInfo, r, "IsNPC") == "1";
            if (npc != w.isNpc) continue;
            const std::string inx = ShnCellText(mobInfo, r, "InxName");
            const std::string name = ShnCellText(mobInfo, r, "Name");
            if (!needle.empty() && LowerAscii(inx).find(needle) == std::string::npos && LowerAscii(name).find(needle) == std::string::npos) continue;
            ++shown;
            long long id = 0;
            if (cId >= 0) ShnValueAsInt(mobInfo.rows[r].values[static_cast<std::size_t>(cId)], id);
            const bool sel = w.templateId == id && w.templateInx == inx;
            if (UI::Selectable((inx + "   " + name + "   (#" + std::to_string(id) + ")##t" + std::to_string(r)).c_str(), sel)) {
                w.templateId = id;
                w.templateInx = inx;
                std::snprintf(w.displayName, sizeof(w.displayName), "%s", (name + " 2").c_str());
                std::snprintf(w.newInx, sizeof(w.newInx), "Custom%s", inx.c_str());
                w.level = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "Level").c_str()));
                w.maxHp = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "MaxHP").c_str()));
                w.walkSpeed = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "WalkSpeed").c_str()));
                w.runSpeed = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "RunSpeed").c_str()));
                w.size = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "Size").c_str()));
            }
        }
    }
    ImGui::EndChild();

    // --- 2. Werte
    ImGui::SeparatorText("2. Bezeichnung und Werte");
    ImGui::SetNextItemWidth(220.0f); UI::InputText("InxName (eindeutig)", w.newInx, sizeof(w.newInx));
    ImGui::SetNextItemWidth(220.0f); UI::InputText("Anzeigename", w.displayName, sizeof(w.displayName));
    UI::Checkbox("ID automatisch (erste freie im größten freien Block, in allen Tabellen frei)", &w.autoId);
    if (!w.autoId) { ImGui::SetNextItemWidth(160.0f); UI::InputInt("ID", &w.manualId); }
    if (!w.isNpc) {
        ImGui::SetNextItemWidth(160.0f); UI::InputInt("Level", &w.level);
        ImGui::SetNextItemWidth(160.0f); UI::InputInt("Max. HP", &w.maxHp);
        ImGui::SetNextItemWidth(160.0f); UI::InputInt("Gehtempo", &w.walkSpeed);
        ImGui::SetNextItemWidth(160.0f); UI::InputInt("Lauftempo", &w.runSpeed);
        ImGui::SetNextItemWidth(160.0f); UI::InputInt("Größe", &w.size);
        ImGui::TextDisabled("Alle weiteren Werte (EXP, Widerstände, Waffen ...) stammen von der Vorlage und lassen sich danach im Single SHN Editor ändern.");
    }

    // --- 3. Aussehen
    ImGui::SeparatorText("3. Aussehen");
    UI::RadioButton("Wie Vorlage", &w.lookMode, 0);
    ImGui::SameLine(); UI::RadioButton("Anderes Modell", &w.lookMode, 1);
    if (w.isNpc) { ImGui::SameLine(); UI::RadioButton("Spieler-Avatar mit Rüstung", &w.lookMode, 2); }
    if (w.lookMode == 1) {
        static std::vector<std::string> models;
        static bool modelsBuilt = false;
        if (!modelsBuilt) {
            const int vi = FindOrLoadShnDoc(state, "MobViewInfo.shn", EditorState::ShnSource::Client);
            if (vi >= 0) {
                std::set<std::string> names;
                const auto& vf = state.shnFiles[static_cast<std::size_t>(vi)].file;
                for (std::size_t r = 0; r < vf.rows.size(); ++r) { const std::string n = ShnCellText(vf, r, "FileName"); if (!n.empty() && n != "-") names.insert(n); }
                models.assign(names.begin(), names.end());
                modelsBuilt = true;
            }
        }
        ImGui::Text("Modell: %s", w.modelFile[0] ? w.modelFile : "(wie Vorlage)");
        ImGui::SameLine();
        if (UI::Button("Modell wählen...")) { w.modelFilter[0] = '\0'; ImGui::OpenPopup("Modell wählen##wiz"); }
        std::string chosen;
        if (StringPickerPopup("Modell wählen##wiz", models, w.modelFilter, sizeof(w.modelFilter), chosen)) std::snprintf(w.modelFile, sizeof(w.modelFile), "%s", chosen == "-" ? "" : chosen.c_str());
    }
    if (w.isNpc && w.lookMode == 2) {
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(120.0f); UI::InputInt("Klasse (0-5)", &w.avClass);
        w.avClass = std::clamp(w.avClass, 0, 5);
        ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt("Geschlecht (1=männl., 0=weibl.)", &w.avGender);
        w.avGender = std::clamp(w.avGender, 0, 1);
        ImGui::SetNextItemWidth(120.0f); UI::InputInt("Gesicht", &w.avFace);
        ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt("Frisur", &w.avHairType);
        ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt("Haarfarbe", &w.avHairColor);
        w.avFace = std::max(0, w.avFace); w.avHairType = std::max(0, w.avHairType); w.avHairColor = std::max(0, w.avHairColor);
        ImGui::TextDisabled("Ausrüstung (Slot klicken = Item wählen aus ItemInfo, Rechtsklick = leeren):");
        if (ImGui::BeginTable("##equ", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(620.0f, 0.0f))) { // feste Breite: rechts daneben die Vorschau
            for (int i = 0; i < 19; ++i) {
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ImGui::Text("%s", kAvatarSlotNames[i]);
                ImGui::SameLine(140.0f);
                std::string& v = w.equ[static_cast<std::size_t>(i)];
                const bool empty = v.empty() || v == "-";
                const bool known = empty || state.itemByInx.count(v) || state.itemEntries.empty();
                if (!known) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(150, 40, 40, 255));
                if (UI::Button((empty ? std::string("(leer)") : v).c_str(), ImVec2(150.0f, 0.0f))) { w.pickSlot = i; w.itemFilter[0] = '\0'; w.pickRequested = true; }
                if (!known) ImGui::PopStyleColor();
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) v = "-";
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        // OpenPopup MUSS auf derselben ID-Ebene wie BeginPopup passieren - der Button steckt in
        // PushID(i)/Tabelle, daher nur ein Merker dort und das Oeffnen hier (sonst oeffnete sich
        // das Popup nie: "Custom NPC laesst sich nicht ausruesten", CHANGELOG [0.44.29]).
        if (w.pickRequested) { ImGui::OpenPopup("Item wählen##wiz"); w.pickRequested = false; }
        static std::vector<std::string> itemNames, itemLabels;
        if (itemNames.size() != state.itemEntries.size()) {
            itemNames.clear(); itemLabels.clear();
            for (const auto& e : state.itemEntries) { itemNames.push_back(e.inx); itemLabels.push_back(e.inx + "   " + e.name + "   (#" + std::to_string(e.id) + ")"); }
        }
        std::string chosen;
        if (StringPickerPopup("Item wählen##wiz", itemNames, w.itemFilter, sizeof(w.itemFilter), chosen, &itemLabels) && w.pickSlot >= 0 && w.pickSlot < 19) {
            w.equ[static_cast<std::size_t>(w.pickSlot)] = chosen;
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        // ---- Vorschau (Koerper + Ruestung + Gesicht + Haare + Waffen aus reschar/resitem)
        ImGui::BeginGroup();
        UpdateAvatarPreview(state);
        ImGui::TextDisabled("Vorschau (Bindepose)");
        if (state.avatarModel && state.avatarTex != 0) {
            ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(state.avatarTex)), ImVec2(300.0f, 460.0f));
            ImGui::SetNextItemWidth(300.0f);
            UI::SliderFloat("##avyaw", &state.avatarYaw, -3.1416f, 3.1416f, "Drehung %.2f");
            for (const auto& note : state.avatarModel->notes) ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "%s", note.c_str());
        } else if (!state.avatarError.empty()) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 300.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", state.avatarError.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndGroup();
    }

    // --- 4. NPC-Extras
    if (w.isNpc) {
        ImGui::SeparatorText("4. Dialog und Platz auf der Karte");
        UI::Checkbox("Dialog der Vorlage kopieren (NpcDialogData)", &w.copyDialog);
        UI::Checkbox("Auf der offenen Karte platzieren (World/NPC.txt)", &w.placeOnMap);
        if (w.placeOnMap) {
            if (state.legacySaveStem[0] == '\0') {
                ImGui::TextDisabled("Keine Karte offen - im Map-Editor eine Karte öffnen.");
            } else {
                ImGui::Text("Karte: %s", state.legacySaveStem);
                ImGui::SetNextItemWidth(120.0f); UI::InputInt("X", &w.placeX);
                ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt("Y", &w.placeY);
                ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt("Richtung", &w.placeDir);
                if (UI::SmallButton("Kartenmitte")) {
                    w.placeX = static_cast<int>((state.heightmap.Width() - 1) * state.heightmap.BlockWidth() * 0.5f);
                    w.placeY = static_cast<int>((state.heightmap.Height() - 1) * state.heightmap.BlockHeight() * 0.5f);
                }
                ImGui::SetNextItemWidth(180.0f);
                UI::Combo("Rolle", &w.roleIdx, kNpcRoles, 6);
                ImGui::SetNextItemWidth(180.0f);
                UI::InputText("Rollenargument (z.B. Quest, Item, Weapon, Skill)", w.roleArg, sizeof(w.roleArg));
                if (std::string(kNpcRoles[std::clamp(w.roleIdx, 0, 5)]) == "Merchant")
                    ImGui::TextDisabled("Händler: danach im NPC-Tab 'Händler-Inventar bearbeiten' (legt NPCItemList/<InxName>.txt an).");
            }
        }
    }

    // --- Anlegen / Speichern
    ImGui::SeparatorText("Anlegen");
    if (UI::Button("Anlegen", ImVec2(160.0f, 0.0f))) RunCreateCreature(state);
    ImGui::SameLine();
    if (UI::Button("Alle geänderten SHN speichern")) {
        int saved = 0, failed = 0;
        for (auto& d : state.shnFiles) {
            if (!d.dirty) continue;
            auto r = core::legacy::SaveShnFile(d.file, d.file.path);
            if (r) { d.dirty = false; ++saved; } else { ++failed; w.report.push_back("Fehler beim Speichern von " + d.file.FileName() + ": " + r.error()); }
        }
        // NPC.txt (falls ein NPC platziert wurde)
        if (state.npcTextLoaded && !state.shineTextRoot.empty()) {
            auto r = core::legacy::SaveShineTextFile(state.npcTextFile, std::filesystem::path(state.shineTextRoot) / "World" / "NPC.txt");
            if (r) ++saved; else { ++failed; w.report.push_back("Fehler beim Speichern von NPC.txt: " + r.error()); }
        }
        w.report.push_back("Gespeichert: " + std::to_string(saved) + " Datei(en)" + (failed ? ", Fehler: " + std::to_string(failed) : std::string()));
    }
    for (const auto& line : w.report) ImGui::TextWrapped("%s", line.c_str());
}

// Sprachumschaltung fuer Texte ohne T()-Schluessel: liefert je nach eingestellter Sprache Deutsch oder Englisch.
static const char* L(const char* de, const char* en) { return app::CurrentLanguage() == app::Language::German ? de : en; }

// ============================================================================
// Skill-Editor (SHN-Editor-Tab "Skill Editor") - CHANGELOG [0.44.33].
//
// Ein Skill (eine STUFE einer Skillreihe, z.B. TripleHit14) steckt unter derselben ID/InxName in:
//   ActiveSkill.shn            (Client + Server)  Kosten, Zeiten, Schaden, Zustaende, Voraussetzungen
//   ActiveSkillInfoServer.shn  (Server)           Trefferwerte, Aggro, Swing-/Hit-Zeit
//   ActiveSkillView.shn        (Client + Server/View) Icon, Animationen, Effekte, Sounds, Beschreibung
// und (falls lernbar) als Skillbuch-Item mit GLEICHEM InxName in ItemInfo/ItemInfoServer/ItemViewInfo.
// Animationen und Effekte sind NAMEN (Text), die auf vorhandene Assets zeigen - "neue Skills mit
// vorhandenen Animationen/Effekten" heisst daher: Namen aus den bereits verwendeten Werten waehlen.
// Aenderungen gelten fuer alle Kopien der Zeile (Client UND Server).
// ============================================================================
namespace skilled {
enum class Doc { Skill, Server, View };
enum class Kind { Int, Text, Bool, Anim, Effect, Sound, Icon, SkillRef, StateRef };
struct Field {
    Doc doc; const char* col; Kind kind;
    const char* labelDe; const char* labelEn; const char* tipDe; const char* tipEn;
};
struct Section { const char* titleDe; const char* titleEn; const Field* fields; std::size_t count; bool defaultOpen; };

// Bedeutungen: sicher = aus Spaltennamen + Daten eindeutig; "(vermutet)" = aus dem Namen abgeleitet, nicht belegt.
static const Field kBasics[] = {
    {Doc::Skill, "Name", Kind::Text, "Name", "Name", "Anzeigename des Skills im Spiel (z.B. 'Slice and Dice [14]').", "Display name of the skill in game (e.g. 'Slice and Dice [14]')."},
    {Doc::Skill, "Grade", Kind::Int, "Grad", "Grade", "Rang der Skillreihe (vermutet).", "Rank of the skill series (assumed)."},
    {Doc::Skill, "Step", Kind::Int, "Stufe", "Step", "Stufe dieses Skills innerhalb seiner Reihe (TripleHit14 = Stufe 14).", "Level of this skill within its series (TripleHit14 = step 14)."},
    {Doc::Skill, "MaxStep", Kind::Int, "Höchste Stufe", "Max step", "Höchste Stufe der Reihe.", "Highest step of the series."},
    {Doc::Skill, "DemandType", Kind::Int, "Art der Voraussetzung", "Requirement type", "Art der Lernvoraussetzung (Aufzählung, Wert aus vorhandenen Skills übernehmen).", "Kind of learning requirement (enum; copy the value from existing skills)."},
    {Doc::Skill, "DemandSk", Kind::SkillRef, "Voraussetzungs-Skill", "Required skill", "Skill (InxName), der vorher gelernt sein muss - meist die Vorstufe.", "Skill (InxName) that must be learned first - usually the previous step."},
    {Doc::Skill, "UseClass", Kind::Int, "Klassen (Bitmaske)", "Classes (bitmask)", "Welche Klassen den Skill nutzen dürfen (Bitmaske; 7 = mehrere Klassen, Werte aus vorhandenen Skills übernehmen).", "Which classes may use the skill (bitmask; copy values from existing skills)."},
    {Doc::Skill, "Range", Kind::Int, "Reichweite", "Range", "Reichweite in Spieleinheiten; 0 = Nahkampf/Standard.", "Range in game units; 0 = melee/default."},
    {Doc::Skill, "Area", Kind::Int, "Wirkungsradius", "Area radius", "Radius bei Flächenskills; 0 = Einzelziel.", "Radius for area skills; 0 = single target."},
    {Doc::Skill, "TargetNumber", Kind::Int, "Anzahl Ziele", "Number of targets", "Wie viele Ziele der Skill trifft.", "How many targets the skill hits."},
};
static const Field kCosts[] = {
    {Doc::Skill, "SP", Kind::Int, "SP-Kosten", "SP cost", "Verbrauch von SP (Skill-/Manapunkte).", "SP consumption (skill/mana points)."},
    {Doc::Skill, "SPRate", Kind::Int, "SP-Kosten (%)", "SP cost (%)", "Zusätzlicher prozentualer SP-Verbrauch (vermutet).", "Additional percentage SP consumption (assumed)."},
    {Doc::Skill, "HP", Kind::Int, "HP-Kosten", "HP cost", "Verbrauch von HP.", "HP consumption."},
    {Doc::Skill, "HPRate", Kind::Int, "HP-Kosten (%)", "HP cost (%)", "Prozentualer HP-Verbrauch (vermutet).", "Percentage HP consumption (assumed)."},
    {Doc::Skill, "LP", Kind::Int, "LP-Kosten", "LP cost", "Verbrauch von LP (vermutet: Licht-/Sonderpunkte).", "LP consumption (assumed: special points)."},
    {Doc::Skill, "CastTime", Kind::Int, "Zauberzeit (ms)", "Cast time (ms)", "Zeit bis der Skill auslöst, in Millisekunden.", "Time until the skill triggers, in milliseconds."},
    {Doc::Skill, "DlyTime", Kind::Int, "Abklingzeit (ms)", "Cooldown (ms)", "Abklingzeit dieses Skills in Millisekunden.", "Cooldown of this skill in milliseconds."},
    {Doc::Skill, "DlyGroupNum", Kind::Int, "Abklingzeit-Gruppe", "Cooldown group", "Nummer der gemeinsamen Abklingzeit-Gruppe: Skills mit gleicher Nummer sperren sich gegenseitig.", "Number of the shared cooldown group: skills with the same number block each other."},
    {Doc::Skill, "DlyTimeGroup", Kind::Int, "Gruppen-Abklingzeit (ms)", "Group cooldown (ms)", "Abklingzeit, die die Gruppe nach Nutzung bekommt.", "Cooldown applied to the group after use."},
    {Doc::Skill, "UseItem", Kind::Int, "Verbrauchtes Item (ID)", "Consumed item (ID)", "Item-ID, die beim Einsatz verbraucht wird; 0 = keins.", "Item ID consumed on use; 0 = none."},
    {Doc::Skill, "ItemNumber", Kind::Int, "Anzahl Items", "Item count", "Anzahl der verbrauchten Items.", "Number of consumed items."},
    {Doc::Skill, "DemandItem1", Kind::Int, "Benötigtes Item 1 (ID)", "Required item 1 (ID)", "Item, das im Inventar sein muss (z.B. Waffe/Munition).", "Item that must be in the inventory (e.g. weapon/ammo)."},
    {Doc::Skill, "DemandItem2", Kind::Int, "Benötigtes Item 2 (ID)", "Required item 2 (ID)", "Zweites benötigtes Item.", "Second required item."},
    {Doc::Skill, "DemandSoul", Kind::Int, "Benötigte Seelen", "Required souls", "Benötigte Seelenpunkte (vermutet).", "Required soul points (assumed)."},
};
static const Field kDamage[] = {
    {Doc::Skill, "MinWC", Kind::Int, "Phys. Schaden min", "Phys. damage min", "Minimaler physischer Schaden (WC).", "Minimum physical damage (WC)."},
    {Doc::Skill, "MinWCRate", Kind::Int, "Phys. min (%)", "Phys. min (%)", "Prozentualer Anteil an der Waffenkraft (vermutet).", "Percentage share of weapon power (assumed)."},
    {Doc::Skill, "MaxWC", Kind::Int, "Phys. Schaden max", "Phys. damage max", "Maximaler physischer Schaden (WC).", "Maximum physical damage (WC)."},
    {Doc::Skill, "MaxWCRate", Kind::Int, "Phys. max (%)", "Phys. max (%)", "Prozentualer Anteil (vermutet).", "Percentage share (assumed)."},
    {Doc::Skill, "MinMA", Kind::Int, "Magischer Schaden min", "Magic damage min", "Minimaler magischer Schaden (MA).", "Minimum magic damage (MA)."},
    {Doc::Skill, "MinMARate", Kind::Int, "Mag. min (%)", "Magic min (%)", "Prozentualer Anteil (vermutet).", "Percentage share (assumed)."},
    {Doc::Skill, "MaxMA", Kind::Int, "Magischer Schaden max", "Magic damage max", "Maximaler magischer Schaden (MA).", "Maximum magic damage (MA)."},
    {Doc::Skill, "MaxMARate", Kind::Int, "Mag. max (%)", "Magic max (%)", "Prozentualer Anteil (vermutet).", "Percentage share (assumed)."},
    {Doc::Skill, "AC", Kind::Int, "Rüstungswert (AC)", "Armor (AC)", "Verteidigungswert-Änderung durch den Skill (vermutet).", "Defense change by the skill (assumed)."},
    {Doc::Skill, "MR", Kind::Int, "Magieresistenz (MR)", "Magic resist (MR)", "Magieresistenz-Änderung durch den Skill (vermutet).", "Magic resistance change by the skill (assumed)."},
};
static const Field kMisc[] = {
    {Doc::Skill, "IsMovingSkill", Kind::Bool, "Bewegungs-Skill", "Moving skill", "1 = Skill bewegt die Figur (Sprung/Ansturm o.ä.) (vermutet).", "1 = skill moves the character (jump/charge) (assumed)."},
    {Doc::Skill, "UsableDegree", Kind::Int, "Nutzbarer Winkel", "Usable angle", "Öffnungswinkel vor dem Charakter, in dem das Ziel liegen muss (Grad).", "Opening angle in front of the character in which the target must be (degrees)."},
    {Doc::Skill, "SkillDegree", Kind::Int, "Trefferwinkel", "Hit angle", "Winkel des Trefferbereichs (360 = rundum) (vermutet).", "Angle of the hit area (360 = all around) (assumed)."},
    {Doc::Skill, "DirectionRotate", Kind::Int, "Ausrichtung drehen", "Rotate direction", "Drehung der Blickrichtung beim Einsatz (vermutet).", "Rotation of facing on use (assumed)."},
    {Doc::Skill, "First", Kind::Int, "Erstes Ziel (Art)", "First target (kind)", "Aufzählung: Art des ersten Ziels (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of the first target (copy from a similar skill)."},
    {Doc::Skill, "Last", Kind::Int, "Letztes Ziel (Art)", "Last target (kind)", "Aufzählung: Art des letzten Ziels.", "Enum: kind of the last target."},
    {Doc::Skill, "SkillTargetState", Kind::Int, "Ziel-Zustand", "Target state", "Aufzählung: Zustand, den das Ziel haben muss (vermutet).", "Enum: state the target must have (assumed)."},
    {Doc::Skill, "CannotInside", Kind::Bool, "Nicht in Gebäuden", "Not inside", "1 = in Innenräumen nicht nutzbar (vermutet).", "1 = not usable indoors (assumed)."},
    {Doc::Skill, "EffectType", Kind::Int, "Effekt-Art", "Effect type", "Aufzählung: Art der Zusatzwirkung (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of additional effect (copy from a similar skill)."},
    {Doc::Skill, "HitID", Kind::Int, "Treffer-ID", "Hit ID", "Verweis auf die Trefferdefinition (vermutet).", "Reference to the hit definition (assumed)."},
};
static const Field kServer[] = {
    {Doc::Server, "UsualAttack", Kind::Bool, "Zählt als Normalangriff", "Counts as normal attack", "1 = wird wie ein normaler Angriff behandelt (vermutet).", "1 = treated like a normal attack (assumed)."},
    {Doc::Server, "SkilPyHitRate", Kind::Int, "Trefferrate phys.", "Phys. hit rate", "Grundtrefferchance für physische Skills (950 = 95 %) (vermutet).", "Base hit chance for physical skills (950 = 95 %) (assumed)."},
    {Doc::Server, "SkilMaHitRate", Kind::Int, "Trefferrate magisch", "Magic hit rate", "Grundtrefferchance für magische Skills.", "Base hit chance for magic skills."},
    {Doc::Server, "PsySucRate", Kind::Int, "Erfolgsrate phys.", "Phys. success rate", "Erfolgschance der Zusatzwirkung (physisch) (vermutet).", "Success chance of the additional effect (physical) (assumed)."},
    {Doc::Server, "MagSucRate", Kind::Int, "Erfolgsrate magisch", "Magic success rate", "Erfolgschance der Zusatzwirkung (magisch) (vermutet).", "Success chance of the additional effect (magic) (assumed)."},
    {Doc::Server, "StaLevel", Kind::Int, "Zustands-Level", "State level", "Stufe der ausgelösten Zustände (vermutet).", "Level of the applied states (assumed)."},
    {Doc::Server, "DmgIncRate", Kind::Int, "Schadensbonus (%)", "Damage bonus (%)", "Prozentualer Schadenszuwachs (vermutet).", "Percentage damage increase (assumed)."},
    {Doc::Server, "DmgIncValue", Kind::Int, "Schadensbonus (fix)", "Damage bonus (flat)", "Fester Schadenszuwachs (vermutet).", "Flat damage increase (assumed)."},
    {Doc::Server, "SkillHitType", Kind::Int, "Trefferart", "Hit type", "Aufzählung: Art des Treffers (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of hit (copy from a similar skill)."},
    {Doc::Server, "AggroPerDamage", Kind::Int, "Aggro pro Schaden", "Aggro per damage", "Wie viel Bedrohung pro Schadenspunkt entsteht (vermutet).", "How much threat per damage point (assumed)."},
    {Doc::Server, "AbsoluteAggro", Kind::Int, "Feste Aggro", "Fixed aggro", "Feste Bedrohung unabhängig vom Schaden (vermutet).", "Fixed threat independent of damage (assumed)."},
    {Doc::Server, "AttackStart", Kind::Bool, "Löst Kampf aus", "Starts combat", "1 = der Einsatz beginnt den Kampf (vermutet).", "1 = use starts combat (assumed)."},
    {Doc::Server, "AttackEnd", Kind::Bool, "Beendet Kampf", "Ends combat", "1 = beendet den Kampf (vermutet).", "1 = ends combat (assumed)."},
    {Doc::Server, "SwingTime", Kind::Int, "Swing-Zeit (ms)", "Swing time (ms)", "Dauer der Angriffsbewegung in ms - sollte zur Animation passen.", "Duration of the attack motion in ms - should match the animation."},
    {Doc::Server, "HitTime", Kind::Int, "Trefferzeitpunkt (ms)", "Hit time (ms)", "Zeitpunkt des Treffers innerhalb der Animation (vermutet).", "Moment of the hit within the animation (assumed)."},
    {Doc::Server, "AddSoul", Kind::Int, "Seelenpunkte", "Soul points", "Seelenpunkte, die der Skill gibt (vermutet).", "Soul points granted by the skill (assumed)."},
};
static const Field kView[] = {
    {Doc::View, "IconFile", Kind::Icon, "Icon-Datei", "Icon file", "Name der Icon-Bilddatei (z.B. FighterSk00) - Werte aus vorhandenen Skills.", "Name of the icon image file (e.g. FighterSk00) - values from existing skills."},
    {Doc::View, "IconIndex", Kind::Int, "Icon-Nummer", "Icon index", "Position des Icons in der Bilddatei.", "Position of the icon within the image file."},
    {Doc::View, "R", Kind::Int, "Farbe R", "Colour R", "Rotanteil der Skillfarbe (0-255).", "Red part of the skill colour (0-255)."},
    {Doc::View, "G", Kind::Int, "Farbe G", "Colour G", "Grünanteil der Skillfarbe.", "Green part of the skill colour."},
    {Doc::View, "B", Kind::Int, "Farbe B", "Colour B", "Blauanteil der Skillfarbe.", "Blue part of the skill colour."},
    {Doc::View, "CastingType", Kind::Int, "Zauber-Art", "Casting type", "Aufzählung: Art der Zauberdarstellung (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of cast presentation (copy from a similar skill)."},
    {Doc::View, "ActionType", Kind::Int, "Aktions-Art", "Action type", "Aufzählung: Art der Aktion (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of action (copy from a similar skill)."},
    {Doc::View, "CasRdyAction", Kind::Anim, "Animation: Zauber-Bereitschaft", "Animation: cast ready", "Animation beim Beginn des Zauberns (Zaubern-Skills: '..Rd'); '-' = keine.", "Animation when casting begins (cast skills: '..Rd'); '-' = none."},
    {Doc::View, "CasAction", Kind::Anim, "Animation: Zaubern", "Animation: casting", "Animation während des Zauberns ('..Cas'); '-' = keine.", "Animation while casting ('..Cas'); '-' = none."},
    {Doc::View, "SwingAction", Kind::Anim, "Animation: Ausführung", "Animation: swing", "Animation der eigentlichen Ausführung (Schlag/Wurf/Zauberabschluss).", "Animation of the actual execution (strike/throw/cast finish)."},
    {Doc::View, "LastAction", Kind::Anim, "Animation: Abschluss", "Animation: finish", "Abschlussanimation (in den Daten nie benutzt).", "Finishing animation (never used in the data)."},
    {Doc::View, "ShoEffect", Kind::Effect, "Effekt: Geschoss", "Effect: projectile", "Effekt, der vom Wirker zum Ziel fliegt (z.B. MagicBallSho).", "Effect flying from caster to target (e.g. MagicBallSho)."},
    {Doc::View, "ShoEfSpd", Kind::Int, "Geschoss-Tempo", "Projectile speed", "Fluggeschwindigkeit des Geschosses.", "Flight speed of the projectile."},
    {Doc::View, "LastEffectA", Kind::Effect, "Effekt: Treffer", "Effect: impact", "Effekt am Ziel bei Treffer (z.B. IceBlastDem).", "Effect at the target on impact (e.g. IceBlastDem)."},
    {Doc::View, "eLastEffPos", Kind::Int, "Treffereffekt-Position", "Impact effect position", "Aufzählung: wo der Treffereffekt erscheint (Wert aus ähnlichem Skill übernehmen).", "Enum: where the impact effect appears (copy from a similar skill)."},
    {Doc::View, "LastAreaEf", Kind::Effect, "Effekt: Fläche", "Effect: area", "Flächeneffekt am Zielpunkt (Flächenskills).", "Area effect at the target point (area skills)."},
    {Doc::View, "LastAEfWhe", Kind::Int, "Flächeneffekt-Ort", "Area effect place", "Aufzählung/Angabe, wo der Flächeneffekt erscheint (vermutet).", "Enum/value where the area effect appears (assumed)."},
    {Doc::View, "DOTRageEft", Kind::Effect, "Effekt: Dauerschaden", "Effect: damage over time", "Effekt für Dauerschaden/-zustände (Anfang).", "Effect for damage over time/states (start)."},
    {Doc::View, "DOTRageEftLoop", Kind::Effect, "Effekt: Dauerschaden (Schleife)", "Effect: DOT (loop)", "Schleifen-Effekt während des Dauerzustands.", "Looping effect during the ongoing state."},
    {Doc::View, "ShoSnd", Kind::Sound, "Sound: Geschoss", "Sound: projectile", "Sound beim Geschoss.", "Sound for the projectile."},
    {Doc::View, "LastEfASnd", Kind::Sound, "Sound: Treffer", "Sound: impact", "Sound beim Trefferefekt.", "Sound for the impact effect."},
    {Doc::View, "LastAESnd", Kind::Sound, "Sound: Fläche", "Sound: area", "Sound beim Flächeneffekt.", "Sound for the area effect."},
    {Doc::View, "DOTRageEftSnd", Kind::Sound, "Sound: Dauerschaden", "Sound: DOT", "Sound beim Dauerschaden-Effekt.", "Sound for the DOT effect."},
    {Doc::View, "DOTRageEftLoopSnd", Kind::Sound, "Sound: Dauerschaden (Schleife)", "Sound: DOT (loop)", "Sound der Schleife.", "Sound of the loop."},
    {Doc::View, "Descript", Kind::Text, "Beschreibung", "Description", "Beschreibungstext im Skillfenster.", "Description text in the skill window."},
    {Doc::View, "Function", Kind::Text, "Funktion (Kurztext)", "Function (short text)", "Kurztext zur Wirkung (z.B. 'additional Damage.').", "Short text about the effect (e.g. 'additional Damage.')."},
    {Doc::View, "uiDemandLv", Kind::Int, "Benötigtes Level", "Required level", "Charakterlevel, ab dem der Skill lernbar ist (Anzeige).", "Character level from which the skill can be learned (display)."},
    {Doc::View, "HideHandItem", Kind::Bool, "Waffe ausblenden", "Hide hand item", "1 = Waffe in der Hand während der Animation ausblenden.", "1 = hide the hand weapon during the animation."},
    {Doc::View, "CancelCasting", Kind::Int, "Zaubern abbrechbar", "Casting cancellable", "Verhalten beim Abbrechen des Zauberns (vermutet).", "Behaviour when casting is cancelled (assumed)."},
    {Doc::View, "TargetChange", Kind::Bool, "Zielwechsel erlaubt", "Target change allowed", "1 = Ziel darf während des Einsatzes gewechselt werden (vermutet).", "1 = target may change during use (assumed)."},
};
static const Section kSections[] = {
    {"Grunddaten", "Basics", kBasics, sizeof(kBasics) / sizeof(Field), true},
    {"Kosten und Zeiten", "Costs and timing", kCosts, sizeof(kCosts) / sizeof(Field), false},
    {"Schaden", "Damage", kDamage, sizeof(kDamage) / sizeof(Field), false},
    {"Bewegung, Ziele, Sonstiges", "Movement, targets, misc", kMisc, sizeof(kMisc) / sizeof(Field), false},
    {"Server-Werte (Treffer, Aggro, Zeiten)", "Server values (hit, aggro, timing)", kServer, sizeof(kServer) / sizeof(Field), false},
    {"Darstellung: Animationen, Effekte, Sounds, Text", "Presentation: animations, effects, sounds, text", kView, sizeof(kView) / sizeof(Field), true},
};
} // namespace skilled

struct SkillDocs { int skillC = -1, skillS = -1, server = -1, viewC = -1, viewS = -1; };

static SkillDocs ResolveSkillDocs(EditorState& state) {
    SkillDocs d;
    using S = EditorState::ShnSource;
    d.skillC = FindOrLoadShnDoc(state, "ActiveSkill.shn", S::Client);
    d.skillS = FindOrLoadShnDoc(state, "ActiveSkill.shn", S::Server);
    d.server = FindOrLoadShnDoc(state, "ActiveSkillInfoServer.shn", S::Server);
    d.viewC = FindOrLoadShnDoc(state, "ActiveSkillView.shn", S::Client);
    d.viewS = FindOrLoadShnDoc(state, "ActiveSkillView.shn", S::Server);
    return d;
}

static std::vector<int> SkillDocsOf(const SkillDocs& d, skilled::Doc kind) {
    std::vector<int> out;
    auto add = [&](int i) { if (i >= 0) out.push_back(i); };
    if (kind == skilled::Doc::Skill) { add(d.skillC); add(d.skillS); }
    else if (kind == skilled::Doc::Server) add(d.server);
    else { add(d.viewC); add(d.viewS); }
    return out;
}

// Zeile eines Skills (per ID) in einer Tabelle - lineare Suche mit Cache je Dokument.
static long long SkillRowIn(EditorState& state, int docIdx, long long skillId) {
    static std::unordered_map<int, std::pair<std::size_t, std::unordered_map<long long, std::size_t>>> cache;
    auto& f = state.shnFiles[static_cast<std::size_t>(docIdx)].file;
    auto& entry = cache[docIdx];
    if (entry.first != f.rows.size() || entry.second.empty()) {
        entry.second.clear();
        const int cId = ShnColumnIndexByName(f, "ID");
        for (std::size_t r = 0; r < f.rows.size() && cId >= 0; ++r) {
            long long id = 0;
            if (ShnValueAsInt(f.rows[r].values[static_cast<std::size_t>(cId)], id)) entry.second.emplace(id, r);
        }
        entry.first = f.rows.size();
    }
    const auto it = entry.second.find(skillId);
    return it == entry.second.end() ? -1 : static_cast<long long>(it->second);
}

static std::string SkillCellRead(EditorState& state, const SkillDocs& d, skilled::Doc kind, const char* col, long long skillId) {
    for (const int di : SkillDocsOf(d, kind)) {
        const long long row = SkillRowIn(state, di, skillId);
        if (row < 0) continue;
        return ShnCellText(state.shnFiles[static_cast<std::size_t>(di)].file, static_cast<std::size_t>(row), col);
    }
    return {};
}

// Schreibt in ALLE Kopien (Client und Server) der Zeile.
static bool SkillCellWrite(EditorState& state, const SkillDocs& d, skilled::Doc kind, const char* col, long long skillId, const std::string& text) {
    bool any = false;
    for (const int di : SkillDocsOf(d, kind)) {
        const long long row = SkillRowIn(state, di, skillId);
        if (row < 0) continue;
        auto& doc = state.shnFiles[static_cast<std::size_t>(di)];
        if (SetShnCellText(doc.file, static_cast<std::size_t>(row), col, text)) { doc.dirty = true; any = true; }
    }
    if (any) ++state.shnEditCounter;
    return any;
}

// Werte einer Spalte (nach Haeufigkeit) fuer Auswahllisten: Animationen, Effekte, Sounds, Icons, Skills, Zustaende.
static void BuildSkillPickerOptions(EditorState& state, const SkillDocs& d) {
    auto& ed = state.skill;
    const std::string key = std::to_string(state.shnEditCounter) + "|" + std::to_string(d.viewC) + std::to_string(d.skillC);
    if (key == ed.pickerBuiltKey) return;
    ed.pickerBuiltKey = key;
    ed.pickerOptions.clear();
    ed.pickerLabels.clear();
    auto addColumn = [&](int docIdx, const char* col, const std::string& mapKey) {
        if (docIdx < 0) return;
        const auto& f = state.shnFiles[static_cast<std::size_t>(docIdx)].file;
        const int c = ShnColumnIndexByName(f, col);
        if (c < 0) return;
        std::unordered_map<std::string, int> counts;
        for (const auto& row : f.rows) counts[core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(c)])]++;
        std::vector<std::pair<int, std::string>> v;
        for (auto& [val, n] : counts) v.emplace_back(n, val);
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        auto& opts = ed.pickerOptions[mapKey];
        auto& labels = ed.pickerLabels[mapKey];
        for (auto& [n, val] : v) { opts.push_back(val); labels.push_back(val + "   (" + std::to_string(n) + "x)"); }
    };
    for (const auto& sec : skilled::kSections) {
        for (std::size_t i = 0; i < sec.count; ++i) {
            const auto& fld = sec.fields[i];
            if (fld.kind == skilled::Kind::Anim || fld.kind == skilled::Kind::Effect || fld.kind == skilled::Kind::Sound || fld.kind == skilled::Kind::Icon) addColumn(d.viewC, fld.col, fld.col);
        }
    }
    addColumn(d.skillC, "InxName", "SkillRef");
    const int abState = FindOrLoadShnDoc(state, "AbState.shn", EditorState::ShnSource::Server);
    addColumn(abState, "InxName", "StateRef");
}

// Eine Feldzeile im Formular. Aenderungen werden SOFORT in alle Kopien geschrieben.
static void DrawSkillField(EditorState& state, const SkillDocs& d, const skilled::Field& fld, long long skillId, ImVec4 dim) {
    auto& ed = state.skill;
    const bool de = app::CurrentLanguage() == app::Language::German;
    std::string current = SkillCellRead(state, d, fld.doc, fld.col, skillId);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(de ? fld.labelDe : fld.labelEn);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s\n[%s]", de ? fld.tipDe : fld.tipEn, fld.col);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(fld.col);
    using K = skilled::Kind;
    if (fld.kind == K::Int || fld.kind == K::Bool) {
        long long v = std::atoll(current.c_str());
        if (fld.kind == K::Bool) {
            bool b = v != 0;
            if (UI::Checkbox("##v", &b)) SkillCellWrite(state, d, fld.doc, fld.col, skillId, b ? "1" : "0");
        } else {
            int iv = static_cast<int>(std::clamp<long long>(v, -2147483647LL, 2147483647LL));
            ImGui::SetNextItemWidth(130.0f);
            if (UI::InputInt("##v", &iv, 0, 0)) SkillCellWrite(state, d, fld.doc, fld.col, skillId, std::to_string(iv));
        }
    } else {
        std::vector<char> buf(current.begin(), current.end());
        buf.resize(std::max<std::size_t>(buf.size() + 1, 256), '\0');
        const bool picker = fld.kind != K::Text;
        ImGui::SetNextItemWidth(picker ? 300.0f : 520.0f);
        if (UI::InputText("##v", buf.data(), buf.size())) SkillCellWrite(state, d, fld.doc, fld.col, skillId, std::string(buf.data()));
        if (picker) {
            const std::string mapKey = fld.kind == K::SkillRef ? "SkillRef" : fld.kind == K::StateRef ? "StateRef" : fld.col;
            ImGui::SameLine();
            if (UI::Button(de ? "Auswahl..." : "Choose...")) { ed.pickFilter[0] = '\0'; ImGui::OpenPopup("##pick"); }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", de ? "Aus den in vorhandenen Skills verwendeten Werten wählen (Anzahl der Verwendungen in Klammern)." : "Choose from the values used by existing skills (number of uses in brackets).");
            std::string chosen;
            const auto& opts = ed.pickerOptions[mapKey];
            const auto& labels = ed.pickerLabels[mapKey];
            if (StringPickerPopup("##pick", opts, ed.pickFilter, sizeof(ed.pickFilter), chosen, &labels)) SkillCellWrite(state, d, fld.doc, fld.col, skillId, chosen);
        }
    }
    ImGui::PopID();
    (void)dim;
}

// Klont die Zeile(n) der Vorlage-ID in allen betroffenen Tabellen (Skill + optional Skillbuch).
static void CreateSkillFrom(EditorState& state, const SkillDocs& d, long long templateId, const std::string& newInx, const std::string& newName, bool makeBook) {
    auto& ed = state.skill;
    ed.report.clear();
    using S = EditorState::ShnSource;
    if (newInx.empty()) { ed.report.push_back(L("Bitte einen InxName angeben.", "Please enter an InxName.")); return; }
    if (d.skillC < 0 || d.viewC < 0 || d.server < 0) { ed.report.push_back(L("ActiveSkill/ActiveSkillView/ActiveSkillInfoServer nicht geladen (Projekt-Ordner prüfen).", "ActiveSkill/ActiveSkillView/ActiveSkillInfoServer not loaded (check project folders).")); return; }
    const long long tRow = SkillRowIn(state, d.skillC, templateId);
    if (tRow < 0) { ed.report.push_back(L("Vorlage nicht gefunden.", "Template not found.")); return; }
    const std::string templateInx = ShnCellText(state.shnFiles[static_cast<std::size_t>(d.skillC)].file, static_cast<std::size_t>(tRow), "InxName");
    {
        const auto& f = state.shnFiles[static_cast<std::size_t>(d.skillC)].file;
        for (std::size_t r = 0; r < f.rows.size(); ++r)
            if (ShnCellText(f, r, "InxName") == newInx) { ed.report.push_back(L("InxName existiert bereits: ", "InxName already exists: ") + newInx); return; }
    }
    std::set<long long> used;
    const core::legacy::ShnColumn* idCol = nullptr;
    for (const int di : {d.skillC, d.skillS, d.server, d.viewC, d.viewS}) {
        if (di < 0) continue;
        const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        const int c = ShnColumnIndexByName(f, "ID");
        CollectShnInts(f, c, used);
        if (!idCol && di == d.skillC && c >= 0) idCol = &f.columns[static_cast<std::size_t>(c)];
    }
    if (!idCol) { ed.report.push_back("ID?"); return; }
    const long long newId = SuggestFreeShnId(*idCol, used);
    if (newId < 0) { ed.report.push_back(L("Keine freie ID.", "No free ID.")); return; }
    ed.report.push_back(std::string(L("Neue Skill-ID: ", "New skill ID: ")) + std::to_string(newId) + "  InxName: " + newInx + "  (" + L("Vorlage", "template") + ": " + templateInx + ")");
    struct Spec { const char* file; S src; };
    const Spec specs[] = {{"ActiveSkill.shn", S::Client}, {"ActiveSkill.shn", S::Server}, {"ActiveSkillInfoServer.shn", S::Server}, {"ActiveSkillView.shn", S::Client}, {"ActiveSkillView.shn", S::Server}};
    for (const auto& sp : specs) CloneRowsById(state, sp.file, sp.src, "InxName", templateId, templateInx, newId, newInx, ed.report);
    ed.selectedId = newId;
    // Name setzen (beide ActiveSkill-Kopien)
    if (!newName.empty()) SkillCellWrite(state, d, skilled::Doc::Skill, "Name", newId, newName);

    if (makeBook) {
        // Skillbuch: Item mit GLEICHEM InxName wie der Skill (ItemInfo.MarketIndex=Skill)
        const int itemC = FindOrLoadShnDoc(state, "ItemInfo.shn", S::Client);
        long long templateItemId = -1;
        if (itemC >= 0) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(itemC)].file;
            for (std::size_t r = 0; r < f.rows.size(); ++r)
                if (ShnCellText(f, r, "InxName") == templateInx) { templateItemId = std::atoll(ShnCellText(f, r, "ID").c_str()); break; }
        }
        if (templateItemId < 0) {
            ed.report.push_back(L("- Kein Skillbuch-Item zur Vorlage gefunden (übersprungen).", "- No skill book item found for the template (skipped)."));
        } else {
            std::set<long long> usedItems;
            const core::legacy::ShnColumn* itemIdCol = nullptr;
            const Spec itemSpecs[] = {{"ItemInfo.shn", S::Client}, {"ItemInfo.shn", S::Server}, {"ItemInfoServer.shn", S::Server}, {"ItemViewInfo.shn", S::Client}, {"ItemViewInfo.shn", S::Server}};
            for (const auto& sp : itemSpecs) {
                const int di = FindOrLoadShnDoc(state, sp.file, sp.src);
                if (di < 0) continue;
                const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
                const int c = ShnColumnIndexByName(f, "ID");
                CollectShnInts(f, c, usedItems);
                if (!itemIdCol && std::string(sp.file) == "ItemInfo.shn" && c >= 0) itemIdCol = &f.columns[static_cast<std::size_t>(c)];
            }
            const long long newItemId = itemIdCol ? SuggestFreeShnId(*itemIdCol, usedItems) : -1;
            if (newItemId < 0) {
                ed.report.push_back(L("- Keine freie Item-ID (Skillbuch übersprungen).", "- No free item ID (book skipped)."));
            } else {
                ed.report.push_back(std::string(L("Neue Item-ID (Skillbuch): ", "New item ID (skill book): ")) + std::to_string(newItemId));
                for (const auto& sp : itemSpecs) {
                    auto created = CloneRowsById(state, sp.file, sp.src, "InxName", templateItemId, templateInx, newItemId, newInx, ed.report);
                    const int di = FindOrLoadShnDoc(state, sp.file, sp.src);
                    if (di >= 0 && !newName.empty() && std::string(sp.file) != "ItemViewInfo.shn")
                        for (const std::size_t nr : created) SetShnCellText(state.shnFiles[static_cast<std::size_t>(di)].file, nr, "Name", newName);
                }
            }
        }
    }
    ed.report.push_back(L("Fertig - noch NICHT gespeichert (oben 'Alle geänderten SHN speichern').", "Done - NOT saved yet (use 'Save all changed SHN' above)."));
}

// Skalierung einer ganzen Reihe (gleicher InxName ohne Endziffern) in ActiveSkill (Client + Server).
static int ScaleSkillSeries(EditorState& state, const SkillDocs& d, const std::string& seriesKey, int percent, const std::vector<const char*>& columns) {
    int changed = 0;
    for (const int di : {d.skillC, d.skillS}) {
        if (di < 0) continue;
        auto& doc = state.shnFiles[static_cast<std::size_t>(di)];
        auto& f = doc.file;
        for (std::size_t r = 0; r < f.rows.size(); ++r) {
            std::string inx = ShnCellText(f, r, "InxName");
            while (!inx.empty() && std::isdigit(static_cast<unsigned char>(inx.back()))) inx.pop_back();
            if (inx != seriesKey) continue;
            for (const char* col : columns) {
                const std::string cur = ShnCellText(f, r, col);
                if (cur.empty()) continue;
                const long long v = std::atoll(cur.c_str());
                const long long nv = static_cast<long long>(std::llround(static_cast<double>(v) * percent / 100.0));
                if (nv != v && SetShnCellText(f, r, col, std::to_string(nv))) { doc.dirty = true; ++changed; }
            }
        }
    }
    if (changed) ++state.shnEditCounter;
    return changed;
}

void DrawSkillEditor(EditorState& state) {
    auto& ed = state.skill;
    const bool de = app::CurrentLanguage() == app::Language::German;
    const ImVec4 dim(0.60f, 0.66f, 0.74f, 1.0f);
    const SkillDocs d = ResolveSkillDocs(state);
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "%s", L("SKILL EDITOR", "SKILL EDITOR"));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", L("Client + Server synchron bearbeiten", "Edit client + server in sync"));
    ImGui::TextWrapped("%s", L("Vorhandene Skills bearbeiten, neue Skills aus Vorlagen klonen und vorhandene Animationen/Effekte zuweisen. Eine Zeile entspricht einer Stufe einer Skillreihe.",
                               "Edit existing skills, clone new skills from templates and assign existing animations/effects. One row represents one step of a skill series."));
    ImGui::Separator();
    if (d.skillC < 0 || d.viewC < 0 || d.server < 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", L("ActiveSkill.shn / ActiveSkillView.shn / ActiveSkillInfoServer.shn nicht gefunden - Client-/Server-Ordner im Projekt prüfen.", "ActiveSkill.shn / ActiveSkillView.shn / ActiveSkillInfoServer.shn not found - check client/server folders in the project."));
        return;
    }
    BuildSkillPickerOptions(state, d);
    auto& asf = state.shnFiles[static_cast<std::size_t>(d.skillC)].file;

    // Toolbar: speichern
    if (UI::Button(L("Alle geänderten SHN speichern", "Save all changed SHN"))) {
        int saved = 0, failed = 0;
        for (auto& doc : state.shnFiles) {
            if (!doc.dirty) continue;
            auto r = core::legacy::SaveShnFile(doc.file, doc.file.path);
            if (r) {
                doc.dirty = false;
                ClearShnDirtyCells(doc);
                ++saved;
            } else {
                ++failed;
                ed.report.push_back(L("Fehler beim Speichern: ", "Save error: ") + doc.file.FileName() + " " + r.error());
            }
        }
        ed.report.push_back(std::string(L("Gespeichert: ", "Saved: ")) + std::to_string(saved) + (failed ? std::string(", ") + L("Fehler: ", "errors: ") + std::to_string(failed) : std::string()));
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Schreibt alle geänderten SHN-Tabellen (Client und Server) auf die Platte.", "Writes all modified SHN tables (client and server) to disk."));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    UI::InputTextWithHint("##skillfilter", L("Suche: Name oder InxName oder ID", "Search: name, InxName or ID"), ed.filter, sizeof(ed.filter));

    // Liste (Clipper, gecacht)
    const std::string key = std::string(ed.filter) + "|" + std::to_string(asf.rows.size()) + "|" + std::to_string(state.shnEditCounter);
    if (key != ed.listKey) {
        ed.listKey = key;
        ed.visible.clear();
        const std::string needle = LowerAscii(ed.filter);
        for (std::size_t r = 0; r < asf.rows.size(); ++r) {
            if (!needle.empty()) {
                const std::string hay = LowerAscii(ShnCellText(asf, r, "InxName") + " " + ShnCellText(asf, r, "Name") + " " + ShnCellText(asf, r, "ID"));
                if (hay.find(needle) == std::string::npos) continue;
            }
            ed.visible.push_back(r);
        }
    }
    const float skillListW = std::clamp(ImGui::GetContentRegionAvail().x * 0.29f, 320.0f, 430.0f);
    ImGui::BeginChild("##skilllist", ImVec2(skillListW, 0.0f), true);
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "%s", L("SKILLS", "SKILLS"));
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu", ed.visible.size(), asf.rows.size());
    ImGui::Separator();
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(ed.visible.size()));
        while (clipper.Step()) {
            for (int vi = clipper.DisplayStart; vi < clipper.DisplayEnd; ++vi) {
                const std::size_t r = ed.visible[static_cast<std::size_t>(vi)];
                const long long id = std::atoll(ShnCellText(asf, r, "ID").c_str());
                const std::string label = "#" + std::to_string(id) + "  " + ShnCellText(asf, r, "InxName") + "  " + ShnCellText(asf, r, "Name");
                if (UI::Selectable((label + "##sk" + std::to_string(r)).c_str(), ed.selectedId == id)) {
                    ed.selectedId = id;
                    ed.report.clear();
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##skilldetail", ImVec2(0.0f, 0.0f), true);
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "%s", L("EIGENSCHAFTEN", "PROPERTIES"));
    ImGui::Separator();
    const long long sel = ed.selectedId;
    const long long selRow = sel >= 0 ? SkillRowIn(state, d.skillC, sel) : -1;
    if (selRow < 0) {
        ImGui::TextDisabled("%s", L("Links einen Skill wählen.", "Choose a skill on the left."));
    } else {
        const std::string inx = ShnCellText(asf, static_cast<std::size_t>(selRow), "InxName");
        ImGui::Text("%s  #%lld  %s", inx.c_str(), sel, ShnCellText(asf, static_cast<std::size_t>(selRow), "Name").c_str());
        ImGui::TextDisabled("%s", L("Der InxName eines vorhandenen Skills ist fest (Verweise anderer Tabellen). Für einen neuen Namen: unten 'Neuen Skill anlegen'.",
                                    "The InxName of an existing skill is fixed (other tables refer to it). For a new name use 'Create new skill' below."));
        ImGui::Separator();

        // --- Neu anlegen / Stufe / Serie
        if (UI::CollapsingHeader(L("Neuen Skill aus diesem anlegen (Vorlage)", "Create new skill from this one (template)"))) {
            ImGui::TextWrapped("%s", L("Klont ALLE Zeilen dieses Skills (Werte, Server-Werte, Darstellung mit Animationen/Effekten) unter einer neuen ID. Danach Werte, Animationen und Effekte unten anpassen.",
                                       "Clones ALL rows of this skill (values, server values, presentation with animations/effects) under a new ID. Then adjust values, animations and effects below."));
            ImGui::SetNextItemWidth(220.0f); UI::InputText(L("Neuer InxName (eindeutig)", "New InxName (unique)"), ed.newInx, sizeof(ed.newInx));
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Interner Schlüsselname des neuen Skills (z.B. MeinFeuerball01). Muss in ActiveSkill eindeutig sein.", "Internal key name of the new skill (e.g. MyFireball01). Must be unique in ActiveSkill."));
            ImGui::SetNextItemWidth(260.0f); UI::InputText(L("Neuer Anzeigename", "New display name"), ed.newName, sizeof(ed.newName));
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Name, der im Spiel angezeigt wird (leer = Name der Vorlage).", "Name shown in game (empty = name of the template)."));
            UI::Checkbox(L("Skillbuch-Item mit anlegen (lernbar/handelbar)", "Also create skill book item (learnable/tradable)"), &ed.makeBook);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Ein Skill wird im Spiel über ein Item mit gleichem InxName gelernt (ItemInfo, MarketIndex=Skill). Klont dieses Item mit neuer Item-ID.", "A skill is learned through an item with the same InxName (ItemInfo, MarketIndex=Skill). Clones that item with a new item ID."));
            if (UI::Button(L("Neuen Skill anlegen", "Create new skill"))) CreateSkillFrom(state, d, sel, ed.newInx, ed.newName, ed.makeBook);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Klont alle Zeilen des gewählten Skills in Client und Server unter neuer ID.", "Clones all rows of the selected skill in client and server under a new ID."));
            ImGui::SameLine();
            if (UI::Button(L("Nächste Stufe anlegen", "Create next step"))) {
                std::string base = inx;
                while (!base.empty() && std::isdigit(static_cast<unsigned char>(base.back()))) base.pop_back();
                const int step = std::atoi(ShnCellText(asf, static_cast<std::size_t>(selRow), "Step").c_str());
                const std::string nextInx = base + std::to_string(step + 1);
                std::string nm = ShnCellText(asf, static_cast<std::size_t>(selRow), "Name");
                const std::string oldTag = "[" + std::to_string(step) + "]";
                if (const auto p = nm.find(oldTag); p != std::string::npos) nm.replace(p, oldTag.size(), "[" + std::to_string(step + 1) + "]");
                CreateSkillFrom(state, d, sel, nextInx, nm, ed.makeBook);
                if (ed.selectedId != sel) {
                    SkillCellWrite(state, d, skilled::Doc::Skill, "Step", ed.selectedId, std::to_string(step + 1));
                    SkillCellWrite(state, d, skilled::Doc::Skill, "DemandSk", ed.selectedId, inx);
                    ed.report.push_back(L("Stufe gesetzt, Voraussetzung = Vorstufe.", "Step set, requirement = previous step."));
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Legt <Reihe><Stufe+1> an: Stufe +1, Voraussetzungs-Skill = dieser Skill.", "Creates <series><step+1>: step +1, required skill = this skill."));
        }
        if (UI::CollapsingHeader(L("Ganze Reihe skalieren (z.B. Schaden +20 %)", "Scale the whole series (e.g. damage +20 %)"))) {
            std::string series = inx;
            while (!series.empty() && std::isdigit(static_cast<unsigned char>(series.back()))) series.pop_back();
            ImGui::Text("%s: %s", L("Reihe", "Series"), series.c_str());
            ImGui::SetNextItemWidth(140.0f); UI::SliderInt("##scalepct", &ed.scalePercent, 10, 400, "%d %%");
            UI::Checkbox(L("Schaden (MinWC/MaxWC/MinMA/MaxMA)", "Damage (MinWC/MaxWC/MinMA/MaxMA)"), &ed.scaleDamage);
            UI::Checkbox(L("Kosten (SP/HP/LP)", "Costs (SP/HP/LP)"), &ed.scaleCost);
            UI::Checkbox(L("Abklingzeit (DlyTime/DlyTimeGroup)", "Cooldown (DlyTime/DlyTimeGroup)"), &ed.scaleCooldown);
            UI::Checkbox(L("Zauberzeit (CastTime)", "Cast time (CastTime)"), &ed.scaleCast);
            if (UI::Button(L("Auf alle Stufen der Reihe anwenden", "Apply to all steps of the series"))) {
                std::vector<const char*> cols;
                if (ed.scaleDamage) { cols.push_back("MinWC"); cols.push_back("MaxWC"); cols.push_back("MinMA"); cols.push_back("MaxMA"); }
                if (ed.scaleCost) { cols.push_back("SP"); cols.push_back("HP"); cols.push_back("LP"); }
                if (ed.scaleCooldown) { cols.push_back("DlyTime"); cols.push_back("DlyTimeGroup"); }
                if (ed.scaleCast) cols.push_back("CastTime");
                const int n = ScaleSkillSeries(state, d, series, ed.scalePercent, cols);
                ed.report.push_back(std::to_string(n) + L(" Werte geändert (Client+Server).", " values changed (client+server)."));
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Multipliziert die gewählten Werte JEDER Stufe der Reihe mit dem Prozentsatz (Client und Server).", "Multiplies the chosen values of EVERY step of the series by the percentage (client and server)."));
            ImGui::TextDisabled("%s", L("Skaliert jeden Wert jeder Stufe der Reihe um den Prozentsatz (100 % = unverändert).", "Scales every value of every step of the series by the percentage (100 % = unchanged)."));
        }
        for (const auto& line : ed.report) ImGui::TextWrapped("%s", line.c_str());
        ImGui::Separator();

        // --- Formular
        for (const auto& sec : skilled::kSections) {
            if (!UI::CollapsingHeader(de ? sec.titleDe : sec.titleEn, sec.defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0)) continue;
            ImGui::PushID(sec.titleEn);
            if (ImGui::BeginTable("##form", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 250.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
                for (std::size_t i = 0; i < sec.count; ++i) DrawSkillField(state, d, sec.fields[i], sel, dim);
                ImGui::EndTable();
            }
            ImGui::PopID();
        }
        // Zustaende (Buffs/Debuffs): 4 Zeilen Name/Staerke/Chance
        if (UI::CollapsingHeader(L("Zustände (Buffs/Debuffs)", "States (buffs/debuffs)"))) {
            ImGui::TextDisabled("%s", L("Bis zu 4 Zustände (AbState-Namen) mit Stärke und Erfolgschance.", "Up to 4 states (AbState names) with strength and success chance."));
            if (ImGui::BeginTable("##states", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 26.0f);
                ImGui::TableSetupColumn(L("Zustand", "State"), ImGuiTableColumnFlags_WidthFixed, 380.0f);
                ImGui::TableSetupColumn(L("Stärke", "Strength"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn(L("Chance", "Chance"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableHeadersRow();
                static const char* const kSuffix[4] = {"A", "B", "C", "D"};
                for (int i = 0; i < 4; ++i) {
                    ImGui::PushID(i);
                    const std::string nameCol = std::string("StaName") + kSuffix[i], strCol = std::string("StaStrength") + kSuffix[i], rateCol = std::string("StaSucRate") + kSuffix[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", kSuffix[i]);
                    ImGui::TableSetColumnIndex(1);
                    {
                        std::string cur = SkillCellRead(state, d, skilled::Doc::Skill, nameCol.c_str(), sel);
                        std::vector<char> buf(cur.begin(), cur.end()); buf.resize(std::max<std::size_t>(buf.size() + 1, 128), '\0');
                        ImGui::SetNextItemWidth(280.0f);
                        if (UI::InputText("##n", buf.data(), buf.size())) SkillCellWrite(state, d, skilled::Doc::Skill, nameCol.c_str(), sel, std::string(buf.data()));
                        ImGui::SameLine();
                        if (UI::SmallButton("...")) { ed.pickFilter[0] = '\0'; ImGui::OpenPopup("##pickstate"); }
                        std::string chosen;
                        if (StringPickerPopup("##pickstate", ed.pickerOptions["StateRef"], ed.pickFilter, sizeof(ed.pickFilter), chosen, &ed.pickerLabels["StateRef"]))
                            SkillCellWrite(state, d, skilled::Doc::Skill, nameCol.c_str(), sel, chosen);
                    }
                    for (int c = 0; c < 2; ++c) {
                        ImGui::TableSetColumnIndex(2 + c);
                        const std::string& col = c == 0 ? strCol : rateCol;
                        int v = std::atoi(SkillCellRead(state, d, skilled::Doc::Skill, col.c_str(), sel).c_str());
                        ImGui::SetNextItemWidth(96.0f);
                        if (UI::InputInt(c == 0 ? "##s" : "##r", &v, 0, 0)) SkillCellWrite(state, d, skilled::Doc::Skill, col.c_str(), sel, std::to_string(v));
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        // Alle uebrigen Spalten roh
        if (UI::CollapsingHeader(L("Alle übrigen Spalten (roh)", "All remaining columns (raw)"))) {
            ImGui::TextDisabled("%s", L("Spalten ohne eigene Beschreibung (u.a. die Stufen-Reihen nT0..nT3 - je 5 Werte pro Skillstufe). Werte als Text.", "Columns without their own description (incl. the series nT0..nT3 - 5 values per skill step). Values as text."));
            std::set<std::string> curated;
            for (const auto& sec : skilled::kSections) for (std::size_t i = 0; i < sec.count; ++i) curated.insert(sec.fields[i].col);
            for (const char* extra : {"ID", "InxName", "StaNameA", "StaNameB", "StaNameC", "StaNameD", "StaStrengthA", "StaStrengthB", "StaStrengthC", "StaStrengthD", "StaSucRateA", "StaSucRateB", "StaSucRateC", "StaSucRateD"}) curated.insert(extra);
            if (ImGui::BeginTable("##raw", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn(L("Tabelle", "Table"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
                ImGui::TableSetupColumn(L("Spalte", "Column"), ImGuiTableColumnFlags_WidthFixed, 190.0f);
                ImGui::TableSetupColumn(L("Wert", "Value"), ImGuiTableColumnFlags_WidthFixed, 260.0f);
                ImGui::TableHeadersRow();
                struct Src { const char* label; int doc; skilled::Doc kind; };
                const Src srcs[] = {{"ActiveSkill", d.skillC, skilled::Doc::Skill}, {"ActiveSkillInfoServer", d.server, skilled::Doc::Server}, {"ActiveSkillView", d.viewC, skilled::Doc::View}};
                for (const auto& sr : srcs) {
                    if (sr.doc < 0) continue;
                    const auto& f = state.shnFiles[static_cast<std::size_t>(sr.doc)].file;
                    for (const auto& col : f.columns) {
                        if (curated.count(col.name)) continue;
                        ImGui::PushID(sr.label); ImGui::PushID(col.name.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(sr.label);
                        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(col.name.c_str());
                        ImGui::TableSetColumnIndex(2);
                        std::string cur = SkillCellRead(state, d, sr.kind, col.name.c_str(), sel);
                        std::vector<char> buf(cur.begin(), cur.end()); buf.resize(std::max<std::size_t>(buf.size() + 1, 256), '\0');
                        ImGui::SetNextItemWidth(240.0f);
                        if (UI::InputText("##v", buf.data(), buf.size())) SkillCellWrite(state, d, sr.kind, col.name.c_str(), sel, std::string(buf.data()));
                        ImGui::PopID(); ImGui::PopID();
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();
}

// ============================================================================
// Handbuch (F1): Kapitel/Suche links, Text rechts, dazu die "Spalten-Referenz (live)" ueber alle geladenen
// SHN-Tabellen. Inhalte: core::manual (zweisprachig). CHANGELOG [0.44.33].
// ============================================================================
static void DrawManualBody(const char* body) {
    std::istringstream in(body);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) { ImGui::Spacing(); continue; }
        if (line.rfind("# ", 0) == 0) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "%s", line.c_str() + 2);
            ImGui::Separator();
        } else if (line.rfind("- ", 0) == 0) {
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", line.c_str() + 2);
        } else {
            ImGui::TextWrapped("%s", line.c_str());
        }
    }
}

static void DrawColumnReference(EditorState& state) {
    const bool de = app::CurrentLanguage() == app::Language::German;
    ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "%s", L("Spalten-Referenz (live)", "Column reference (live)"));
    ImGui::TextWrapped("%s", L("Zeigt alle Spalten der in den SHN-Editor geladenen Tabellen mit Typ und - wo bekannt - Beschreibung. 'vermutet' = aus dem Namen abgeleitet, nicht belegt; '-' = noch nicht beschrieben.",
                               "Shows all columns of the tables loaded into the SHN editor with type and - where known - a description. 'assumed' = derived from the name, not proven; '-' = not described yet."));
    if (state.shnFiles.empty()) { ImGui::TextDisabled("%s", L("Noch keine SHN geladen (SHN-Editor öffnen).", "No SHN loaded yet (open the SHN editor).")); return; }
    std::vector<std::string> names;
    for (const auto& d : state.shnFiles) names.push_back(std::string(ShnSourceName(d.source)) + ": " + d.file.FileName());
    state.manualColumnDoc = std::clamp(state.manualColumnDoc, 0, static_cast<int>(names.size()) - 1);
    ImGui::SetNextItemWidth(360.0f);
    if (ImGui::BeginCombo("##coldoc", names[static_cast<std::size_t>(state.manualColumnDoc)].c_str())) {
        for (std::size_t i = 0; i < names.size(); ++i)
            if (ImGui::Selectable((names[i] + "##cd" + std::to_string(i)).c_str(), static_cast<int>(i) == state.manualColumnDoc)) state.manualColumnDoc = static_cast<int>(i);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##colfilter", L("Spalte suchen", "Search column"), state.manualColumnFilter, sizeof(state.manualColumnFilter));
    const auto& doc = state.shnFiles[static_cast<std::size_t>(state.manualColumnDoc)];
    std::string table = doc.file.FileName();
    if (table.size() > 4 && LowerAscii(table.substr(table.size() - 4)) == ".shn") table.resize(table.size() - 4);
    const std::string needle = LowerAscii(state.manualColumnFilter);
    ImGui::TextDisabled("%s: %zu %s, %zu %s", table.c_str(), doc.file.columns.size(), L("Spalten", "columns"), doc.file.rows.size(), L("Zeilen", "rows"));
    if (ImGui::BeginTable("##coltbl", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 34.0f);
        ImGui::TableSetupColumn(L("Spalte", "Column"), ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableSetupColumn(L("Typ", "Type"), ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn(L("Beschreibung", "Description"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < doc.file.columns.size(); ++i) {
            const auto& col = doc.file.columns[i];
            if (!needle.empty() && LowerAscii(col.name).find(needle) == std::string::npos) continue;
            const auto* info = core::manual::FindColumn(table, col.name);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", i);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(col.name.c_str());
            ImGui::TableSetColumnIndex(2); {
                static const char* const kKindNames[] = {"UInt8", "UInt16", "UInt32", "Int8", "Int16", "Int32", "Float", "Text", "2xUInt32", "Raw"};
                ImGui::TextDisabled("%s", kKindNames[std::min<std::size_t>(static_cast<std::size_t>(col.kind), 9)]);
            }
            ImGui::TableSetColumnIndex(3);
            if (info) {
                ImGui::TextWrapped("%s%s", de ? info->de : info->en, info->certain ? "" : (de ? "  (vermutet)" : "  (assumed)"));
            } else {
                ImGui::TextDisabled("-");
            }
        }
        ImGui::EndTable();
    }
}

void DrawManualWindow(EditorState& state) {
    if (!state.manualOpen) return;
    const bool de = app::CurrentLanguage() == app::Language::German;
    ImGui::SetNextWindowSize(ImVec2(1050.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(120.0f, 60.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(L("Handbuch", "Manual"), &state.manualOpen)) { ImGui::End(); return; }
    ImGui::SetNextItemWidth(340.0f);
    ImGui::InputTextWithHint("##manualsearch", L("Suchen (z.B. Kamera, Skill, Shop, Walk ...)", "Search (e.g. camera, skill, shop, walk ...)"), state.manualQuery, sizeof(state.manualQuery));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", L("F1 = Handbuch öffnen/schließen", "F1 = open/close the manual"));
    ImGui::Separator();
    ImGui::BeginChild("##manualnav", ImVec2(300.0f, 0.0f), true);
    const bool searching = state.manualQuery[0] != '\0';
    if (searching) {
        const auto hits = core::manual::Search(state.manualQuery, de);
        ImGui::TextDisabled("%zu %s", hits.size(), L("Treffer", "hits"));
        for (const auto* sec : hits) {
            if (ImGui::Selectable(((de ? sec->titleDe : sec->titleEn) + std::string("##s_") + sec->id).c_str(), state.manualSection == sec->id)) state.manualSection = sec->id;
        }
    } else {
        for (const auto& ch : core::manual::Chapters()) {
            if (!ImGui::TreeNodeEx((de ? ch.titleDe : ch.titleEn), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) continue;
            for (const auto& sec : core::manual::Sections()) {
                if (std::string(sec.chapterId) != ch.id) continue;
                if (ImGui::Selectable(((de ? sec.titleDe : sec.titleEn) + std::string("##s_") + sec.id).c_str(), state.manualSection == sec.id)) state.manualSection = sec.id;
            }
            if (std::string(ch.id) == "reference") {
                if (ImGui::Selectable((std::string(L("Spalten-Referenz (live)", "Column reference (live)")) + "##s_reference.columns").c_str(), state.manualSection == "reference.columns")) state.manualSection = "reference.columns";
            }
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##manualbody", ImVec2(0.0f, 0.0f), true);
    if (state.manualSection == "reference.columns") {
        DrawColumnReference(state);
    } else {
        const core::manual::Section* current = nullptr;
        for (const auto& sec : core::manual::Sections()) if (state.manualSection == sec.id) current = &sec;
        if (current == nullptr) {
            ImGui::TextDisabled("%s", L("Abschnitt links wählen.", "Choose a section on the left."));
        } else {
            ImGui::TextColored(ImVec4(0.40f, 0.72f, 0.96f, 1.0f), "%s", de ? current->titleDe : current->titleEn);
            ImGui::Separator();
            DrawManualBody(de ? current->bodyDe : current->bodyEn);
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

// Kleine, immer sichtbare Leiste unten rechts: Handbuch + Sprache (auf allen Bildschirmen erreichbar).
void DrawGlobalHelpBar(EditorState& state, const ImVec2& displaySize) {
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 12.0f, displaySize.y - 10.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 25, 32, 220));
    ImGui::Begin("##HelpBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    if (ImGui::Button(L("? Handbuch (F1)", "? Manual (F1)"))) state.manualOpen = !state.manualOpen;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Öffnet das Handbuch: Tasten, Funktionen, Spalten - alles erklärt.", "Opens the manual: keys, functions, columns - everything explained."));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(64.0f);
    int langIdx = app::CurrentLanguage() == app::Language::German ? 0 : 1;
    const char* langItems[] = {"DE", "EN"};
    if (ImGui::Combo("##lang2", &langIdx, langItems, 2)) app::SetLanguage(langIdx == 0 ? app::Language::German : app::Language::English);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Sprache der Oberfläche, Tooltips und des Handbuchs", "Language of the interface, tooltips and manual"));
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// Globale Tastenkuerzel: F1 = Handbuch, Strg+Z / Strg+Y (Strg+Shift+Z) = Rueckgaengig/Wiederholen im aktiven Karten-Werkzeug.
void HandleGlobalShortcuts(EditorState& state) {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) state.manualOpen = !state.manualOpen;
    if (io.WantTextInput || state.screen != AppScreen::MapEditorWorkspace) return;
    if (state.editMode == EditMode::ObjectPlacement) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) CopySelectedObjects(state);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) PasteObjectClipboard(state);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) DuplicateSelectedObjects(state);
        if (!io.KeyCtrl && !io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F, false))
            FocusSelectedObjects(state);
        if (ImGui::IsKeyPressed(ImGuiKey_End, false)) GroundSelectedObjects(state);
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteSelectedObjects(state);
    }
    const bool undo = io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false);
    const bool redo = io.KeyCtrl && ((ImGui::IsKeyPressed(ImGuiKey_Y, false)) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)));
    if (!undo && !redo) return;
    switch (state.editMode) {
        case EditMode::Heightmap:
            if (undo ? state.undo.Undo(state.heightmap) : state.undo.Redo(state.heightmap)) state.meshDirty = true;
            break;
        case EditMode::TexturePaint:
            if (undo ? state.textureUndo.Undo(state.textureStack) : state.textureUndo.Redo(state.textureStack)) {
                state.layerPreviewDirty = true;
                state.renderer.UpdateBlendTextures(state.textureStack);
            }
            break;
        case EditMode::BlockWalk:
            if (undo ? state.walkUndo.Undo(state.walkGrid) : state.walkUndo.Redo(state.walkGrid)) state.walkPreviewDirty = true;
            break;
        default: break;
    }
}

static void DrawVisibilityPanel(EditorState& state);
static void StampObjectFootprints(EditorState& state, bool blocked);
static void RefreshObjectVisibility(EditorState& state);
static bool IsObjectHidden(const EditorState& state, std::size_t i);
int PlaceObjectAtWorld(EditorState& state, const std::string& modelPath, float x, float y, float z);
float ActiveWorldBrushRadius(const EditorState& state);
ImU32 ActiveBrushColor(const EditorState& state, int alpha = 235);

void DrawToolsContent(EditorState& state) {
    if (state.editMode == EditMode::Heightmap) {
        ImGui::Text("Pinselmodus");
        UI::RadioButton("Anheben", reinterpret_cast<int*>(&state.brushMode), static_cast<int>(core::BrushMode::Raise));
        UI::RadioButton("Absenken", reinterpret_cast<int*>(&state.brushMode), static_cast<int>(core::BrushMode::Lower));
        UI::RadioButton("Glätten", reinterpret_cast<int*>(&state.brushMode), static_cast<int>(core::BrushMode::Smooth));
        UI::RadioButton("Einebnen", reinterpret_cast<int*>(&state.brushMode), static_cast<int>(core::BrushMode::Flatten));

        ImGui::Separator();
        UI::SliderFloat("Radius", &state.brush.radius, 10.0f, 2000.0f);
        ImGui::TextDisabled("Radius-Presets");
        for (float preset : {50.0f,100.0f,250.0f,500.0f}) {
            ImGui::SameLine();
            const std::string label=std::to_string(static_cast<int>(preset))+"##terrainRadius";
            if (UI::SmallButton(label.c_str())) state.brush.radius=preset;
        }
        UI::SliderFloat("Stärke", &state.brush.strength, 0.1f, 100.0f);
        ImGui::TextDisabled("Stärke-Presets");
        for (float preset : {1.0f,5.0f,10.0f,25.0f}) {
            ImGui::SameLine();
            const std::string label=std::to_string(static_cast<int>(preset))+"##terrainStrength";
            if (UI::SmallButton(label.c_str())) state.brush.strength=preset;
        }
        if (state.brushMode == core::BrushMode::Flatten) {
            UI::InputFloat("Zielhöhe", &state.brush.flattenTarget);
        }

        ImGui::Separator();
        ImGui::BeginDisabled(!state.undo.CanUndo());
        if (UI::Button("Rückgängig (Strg+Z)")) {
            if (state.undo.Undo(state.heightmap)) state.meshDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.undo.CanRedo());
        if (UI::Button("Wiederholen (Strg+Y)")) {
            if (state.undo.Redo(state.heightmap)) state.meshDirty = true;
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        const auto [lo, hi] = state.heightmap.MinMax();
        ImGui::Text("Gitter: %u x %u", state.heightmap.Width(), state.heightmap.Height());
        ImGui::Text("Höhen-Range: [%.2f, %.2f]", lo, hi);
    } else if (state.editMode == EditMode::TexturePaint) {
        ImGui::TextDisabled("Layer-Auswahl und Layer-Verwaltung befinden sich im separaten Layer-Dock.");
        ImGui::Separator();
        ImGui::Text("Textur-Rasterauflösung");
        UI::InputInt("Breite##texResolution", &state.textureResolutionWidth);
        UI::InputInt("Höhe##texResolution", &state.textureResolutionHeight);
        state.textureResolutionWidth = std::clamp(state.textureResolutionWidth, 64, 4096);
        state.textureResolutionHeight = std::clamp(state.textureResolutionHeight, 64, 4096);
        ImGui::SameLine();
        if (UI::Button("Auflösung anwenden##texResolution")) {
            if (state.textureStack.Width() != static_cast<std::uint32_t>(state.textureResolutionWidth) ||
                state.textureStack.Height() != static_cast<std::uint32_t>(state.textureResolutionHeight)) {
                state.textureStack.Resize(static_cast<std::uint32_t>(state.textureResolutionWidth),
                                          static_cast<std::uint32_t>(state.textureResolutionHeight));
                state.textureUndo.Clear();
                state.layerPreviewDirty = true;
                state.renderer.UpdateBlendTextures(state.textureStack);
            }
        }
        ImGui::TextDisabled("Hinweis: Anwenden setzt die Layer-Gewichte auf dem neuen Raster zurück.");

        ImGui::Separator();
        ImGui::Text("Pinselmodus (Textur)");
        int paintModeInt = static_cast<int>(state.paintMode);
        UI::RadioButton("Erhöhen", &paintModeInt, static_cast<int>(core::PaintMode::Increase));
        ImGui::SameLine();
        UI::RadioButton("Senken", &paintModeInt, static_cast<int>(core::PaintMode::Decrease));
        state.paintMode = static_cast<core::PaintMode>(paintModeInt);
        UI::SliderFloat("Radius##tex", &state.paintSettings.radius, 10.0f, 2000.0f);
        ImGui::TextDisabled("Radius-Presets");
        for (float preset : {50.0f,100.0f,250.0f,500.0f}) {
            ImGui::SameLine();
            const std::string label=std::to_string(static_cast<int>(preset))+"##textureRadius";
            if (UI::SmallButton(label.c_str())) state.paintSettings.radius=preset;
        }
        UI::SliderFloat("Stärke##tex", &state.paintSettings.strength, 0.01f, 1.0f);
        ImGui::TextDisabled("Stärke-Presets");
        for (float preset : {0.10f,0.25f,0.50f,1.00f}) {
            ImGui::SameLine();
            char label[32]; std::snprintf(label,sizeof(label),"%.2f##textureStrength",preset);
            if (UI::SmallButton(label)) state.paintSettings.strength=preset;
        }

        ImGui::Separator();
        ImGui::BeginDisabled(!state.textureUndo.CanUndo());
        if (UI::Button("Rückgängig (Textur)")) {
            if (state.textureUndo.Undo(state.textureStack)) {
                state.layerPreviewDirty = true;
                state.renderer.UpdateBlendTextures(state.textureStack);
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.textureUndo.CanRedo());
        if (UI::Button("Wiederholen (Textur)")) {
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
        ImGui::TextWrapped("Block & Walk: jede Zelle (6.25 Einheiten) ist blockiert (rot) oder begehbar. "
                           "Klick/Ziehen im 2D-View setzt Zellen im Kreis um den Mauszeiger.");
        ImGui::TextColored(ImVec4(1.0f,0.38f,0.38f,1.0f),"● blockiert");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.32f,0.90f,0.58f,1.0f),"● begehbar");
        int mode = state.walkBlockMode ? 0 : 1;
        if (UI::RadioButton("Sperren (blockiert)", &mode, 0)) state.walkBlockMode = true;
        ImGui::SameLine();
        if (UI::RadioButton("Freigeben (begehbar)", &mode, 1)) state.walkBlockMode = false;
        UI::SliderFloat("Radius##walk", &state.walkSettings.radius, 1.0f, 1000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
        ImGui::TextDisabled("Radius-Presets");
        for (float preset : {6.25f,25.0f,50.0f,100.0f,250.0f}) {
            ImGui::SameLine();
            char label[32]; std::snprintf(label,sizeof(label),"%.0f##walkRadius",preset);
            if (UI::SmallButton(label)) state.walkSettings.radius=preset;
        }
        ImGui::SeparatorText("Aus Objekten");
        if (UI::Button("Grundflächen sichtbarer Objekte SPERREN")) StampObjectFootprints(state, true);
        if (UI::Button("Grundflächen sichtbarer Objekte FREIGEBEN")) StampObjectFootprints(state, false);
        ImGui::TextDisabled("Nutzt die Sichtbarkeit (Kategorien): z.B. nur 'Gebäude' einblenden, dann sperren.");
        ImGui::TextDisabled("Zellen im Gitter: %u x %u (%.0f x %.0f Einheiten)", state.walkGrid.Cols(), state.walkGrid.Rows(),
                            state.walkGrid.Cols() * WalkGridCellSize(), state.walkGrid.Rows() * WalkGridCellSize());

        ImGui::Separator();
        ImGui::BeginDisabled(!state.walkUndo.CanUndo());
        if (UI::Button("Rückgängig (Walk)")) {
            if (state.walkUndo.Undo(state.walkGrid)) state.walkPreviewDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!state.walkUndo.CanRedo());
        if (UI::Button("Wiederholen (Walk)")) {
            if (state.walkUndo.Redo(state.walkGrid)) state.walkPreviewDirty = true;
        }
        ImGui::EndDisabled();
    } else if (state.editMode == EditMode::ObjectPlacement) {
        ImGui::Text("Klick im Editor (2D) unten:");
        UI::RadioButton("Platzieren", &state.objectPlaceMode, 1);
        ImGui::SameLine();
        UI::RadioButton("Auswählen", &state.objectPlaceMode, 0);

        if (state.objectPlaceMode) {
            UI::InputText("Modellpfad", state.newObjectModelPath, sizeof(state.newObjectModelPath));
            ImGui::SameLine();
            if (UI::Button("Durchsuchen...##nif")) {
                if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
                    state.availableNifFiles = ListFilesByExtension(*resmapRoot, {".nif"});
                    state.nifAssetRoot = *resmapRoot;
                    state.resmapRootForThumbnails = *resmapRoot;
                    state.nifListScanned = true;
                    state.assetPickerFilter.clear();
                    if (state.availableNifFiles.empty()) {
                        state.statusMessage = "Keine .nif-Dateien unter " + resmapRoot->string() + " gefunden.";
                    } else {
                        ImGui::OpenPopup("##nifPicker");
                    }
                } else {
                    state.statusMessage = "Kein Client-Ordner/'resmap' aktiv - unter 'Neu' ein Projekt mit Client Ordner anlegen.";
                }
            }
            {
                std::string picked;
                if (DrawAssetPickerPopup("##nifPicker", state.availableNifFiles, state.assetPickerFilter, picked,
                                          state, state.nifAssetRoot, true, state.resmapRootForThumbnails)) {
                    const std::string legacyModelPath = ToLegacyResmapModelPath(picked);
                    std::snprintf(state.newObjectModelPath, sizeof(state.newObjectModelPath), "%s", legacyModelPath.c_str());
                }
            }
            UI::SliderFloat("Rotation um Hochachse (°)##new", &state.newObjectRotDeg, -180.0f, 180.0f);
            UI::SliderFloat("Skalierung##new", &state.newObjectScale, 0.1f, 5.0f);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Auswahl erfolgt im separaten Objekt-Outliner oder direkt in 2D/3D.");

        const bool multipleSelection = state.selectedObjects.size() > 1;
        if (multipleSelection) {
            ImGui::Text("%zu Objekte ausgewählt", state.selectedObjects.size());
            ImGui::TextWrapped("Koordinaten, Rotation, Skalierung und Drag&Drop wirken gemeinsam. "
                               "Relative Abstände und Höhen bleiben beim Verschieben erhalten.");
        }

        SyncSelectedObjectModelPath(state);
        if (auto* obj = EditableObject(state, state.selectedObject)) {
            ImGui::SeparatorText("Transform-Werkzeug");
            if (UI::RadioButton("Move##gizmoProps",&state.objectGizmoOperation,0)) state.objectGizmoMatrixValid=false;
            ImGui::SameLine();
            if (UI::RadioButton("Rotate##gizmoProps",&state.objectGizmoOperation,1)) state.objectGizmoMatrixValid=false;
            ImGui::SameLine();
            if (UI::RadioButton("Scale##gizmoProps",&state.objectGizmoOperation,2)) state.objectGizmoMatrixValid=false;
            if (UI::Checkbox("Local##gizmoProps",&state.objectGizmoLocal)) state.objectGizmoMatrixValid=false;
            ImGui::SameLine();
            UI::Checkbox("Snap##gizmoProps",&state.objectGizmoSnap);
            if (state.objectGizmoSnap) {
                if (state.objectGizmoOperation==0) {
                    UI::InputFloat("Grid-Snap",&state.objectMoveSnap,10.0f,50.0f,"%.1f");
                    state.objectMoveSnap=std::max(0.01f,state.objectMoveSnap);
                } else if (state.objectGizmoOperation==1) {
                    UI::InputFloat("Winkel-Snap (°)",&state.objectRotateSnap,1.0f,5.0f,"%.1f");
                    state.objectRotateSnap=std::max(0.1f,state.objectRotateSnap);
                } else {
                    UI::InputFloat("Scale-Snap",&state.objectScaleSnap,0.01f,0.10f,"%.2f");
                    state.objectScaleSnap=std::max(0.001f,state.objectScaleSnap);
                }
            }
            if (UI::Button("Auswahl fokussieren (F)")) FocusSelectedObjects(state);
            ImGui::SameLine();
            if (UI::Button("Auf Terrain (Ende)")) GroundSelectedObjects(state);
            ImGui::Separator();
            const bool shmdScene = IsShmdSelection(state.selectedObject);
            if (shmdScene) {
                const std::string categoryName = ShmdSelectionCategoryName(state, state.selectedObject);
                ImGui::Text("SHMD-Kategorie: %s", categoryName.empty() ? "Szenenmodell" : categoryName.c_str());
            }
            if (shmdScene || (multipleSelection && std::any_of(state.selectedObjects.begin(), state.selectedObjects.end(),
                                                               [](int id) { return IsShmdSelection(id); }))) {
                ImGui::TextWrapped("Transformänderungen wandeln betroffene Sky/Water/GroundObject-Einträge automatisch "
                                   "in normale Placements um, da die SHMD-Kategorielisten keine Transformfelder besitzen.");
            }

            ImGui::BeginDisabled(multipleSelection);
            if (multipleSelection) ImGui::TextDisabled("Modellwechsel nur bei Einzelauswahl");
            if (UI::InputText("Modellpfad##selected", state.selectedObjectModelPath, sizeof(state.selectedObjectModelPath))) {
                state.selectedObjectModelPathDirty = true;
            }
            if (state.selectedObjectModelPathDirty && ImGui::IsItemDeactivatedAfterEdit()) {
                ApplySelectedObjectModelPath(state, state.selectedObjectModelPath);
                state.selectedObjectModelPathDirty = false;
                obj = EditableObject(state, state.selectedObject);
            }
            ImGui::SameLine();
            if (UI::Button("Durchsuchen...##selectedNif")) {
                if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
                    state.availableNifFiles = ListFilesByExtension(*resmapRoot, {".nif"});
                    state.nifAssetRoot = *resmapRoot;
                    state.resmapRootForThumbnails = *resmapRoot;
                    state.assetPickerFilter.clear();
                    if (!state.availableNifFiles.empty()) ImGui::OpenPopup("##selectedNifPicker");
                }
            }
            {
                std::string picked;
                if (DrawAssetPickerPopup("##selectedNifPicker", state.availableNifFiles, state.assetPickerFilter, picked,
                                         state, state.nifAssetRoot, true, state.resmapRootForThumbnails)) {
                    const std::string legacyModelPath = ToLegacyResmapModelPath(picked);
                    ApplySelectedObjectModelPath(state, legacyModelPath);
                    obj = EditableObject(state, state.selectedObject);
                }
            }
            ImGui::EndDisabled();

            if (obj) {
                const float oldX = obj->posX, oldY = obj->posY, oldZ = obj->posZ;
                float position[3] = {oldX, oldY, oldZ};
                if (ImGui::InputFloat3(multipleSelection ? "Position (aktives Objekt)" : "Position", position, "%.1f")) {
                    MoveSelectedObjectsBy(state, position[0] - oldX, position[1] - oldY, position[2] - oldZ);
                    obj = EditableObject(state, state.selectedObject);
                }

                if (obj) {
                    const EditQuat oldQ=NormalizeEditQuat({obj->rotX,obj->rotY,obj->rotZ,obj->rotW});
                    app::Mat4 matrix=ObjectEditMatrix({obj->posX,obj->posY,obj->posZ},oldQ,std::max(0.001f,obj->scale));
                    float tr[3]{}, rot[3]{}, sc[3]{};
                    ImGuizmo::DecomposeMatrixToComponents(matrix.m,tr,rot,sc);
                    float editedRot[3]={rot[0],rot[1],rot[2]};
                    if (ImGui::InputFloat3("Rotation XYZ (°)",editedRot,"%.1f")) {
                        float uniformScale[3]={std::max(0.001f,obj->scale),std::max(0.001f,obj->scale),std::max(0.001f,obj->scale)};
                        app::Mat4 changed=app::Mat4::Identity();
                        ImGuizmo::RecomposeMatrixFromComponents(tr,editedRot,uniformScale,changed.m);
                        const EditQuat newQ=MatrixRotationQuat(changed);
                        const EditQuat delta=MulEditQuat(newQ,ConjugateEditQuat(oldQ));
                        const auto pivot=ComputeObjectSelectionPivot(state);
                        if(pivot.valid) RotateSelectedObjectsAroundPivot(state,pivot.position,delta);
                        state.objectGizmoMatrixValid=false;
                        obj=EditableObject(state,state.selectedObject);
                    }
                }

                if (obj) {
                    const float oldScale = obj->scale;
                    float scale = oldScale;
                    if (UI::SliderFloat("Skalierung", &scale, 0.01f, 100.0f, "%.3f",
                                        ImGuiSliderFlags_Logarithmic)) {
                        const float factor = oldScale > 1.0e-6f ? scale / oldScale : 1.0f;
                        ScaleSelectedObjectsBy(state, factor);
                        state.objectGizmoMatrixValid=false;
                        obj = EditableObject(state, state.selectedObject);
                    }
                }
            }

            if (UI::Button("Ausgewählte Objekte löschen")) DeleteSelectedObjects(state);
        }

        ImGui::Separator();
        if (UI::Button("Alle normalen Objekte entfernen")) ImGui::OpenPopup("##deleteAllNormalObjects");
        if (ImGui::BeginPopupModal("##deleteAllNormalObjects", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Wirklich alle %zu normalen Placement-Objekte entfernen?", state.placementSet.Count());
            ImGui::TextDisabled("Sky, Water und GroundObject bleiben erhalten.");
            if (UI::Button("Alle normalen entfernen")) {
                DeleteAllNormalObjects(state);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UI::Button("Abbrechen")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    } else if (state.editMode == EditMode::Npcs) {
        if (state.shineTextRoot.empty()) {
            ImGui::TextWrapped("Server-Textdaten-Ordner (Server/9Data/Shine, enthält World/NPC.txt) noch nicht gewählt.");
            std::vector<char> rootBuf(state.shineTextRoot.begin(), state.shineTextRoot.end());
            rootBuf.resize(std::max<std::size_t>(rootBuf.size() + 1, 512));
            ImGui::SetNextItemWidth(-1.0f);
            if (UI::InputText("##shinetextrootnpc", rootBuf.data(), rootBuf.size())) {
                state.shineTextRoot.assign(rootBuf.data()); state.npcTextLoaded = false;
            }
#ifdef _WIN32
            if (UI::Button("Ordner wählen...##shinetextnpc")) {
                if (auto p = BrowseForFolderWindows("Server/9Data/Shine Ordner wählen")) { state.shineTextRoot = *p; state.npcTextLoaded = false; }
            }
#endif
        } else {
            EnsureNpcTextLoaded(state);
            if (!state.npcTextLoaded) {
                ImGui::TextWrapped("World/NPC.txt konnte unter '%s' nicht geladen werden.", state.shineTextRoot.c_str());
            } else if (state.legacySaveStem[0] == '\0') {
                ImGui::TextDisabled("Keine Karte offen.");
            } else {
                auto indices = NpcRecordsForCurrentMap(state);
                auto* table = state.npcTextFile.FindTable("ShineNPC");
                ImGui::TextDisabled("%zu NPCs auf '%s' · Auswahl links im Szene-Outliner",
                                    indices.size(), state.legacySaveStem);
                if (state.selectedNpcRecordIdx >= 0 && static_cast<std::size_t>(state.selectedNpcRecordIdx) < table->records.size()) {
                    auto& rec = table->records[static_cast<std::size_t>(state.selectedNpcRecordIdx)];
                    if (rec.values.size() >= 8) {
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f),"%s",rec.values[0].c_str());
                        ImGui::SeparatorText("Transform");
                        int x = std::atoi(rec.values[2].c_str());
                        int y = std::atoi(rec.values[3].c_str());
                        bool changed = false;
                        changed |= UI::InputInt("Coord-X", &x);
                        changed |= UI::InputInt("Coord-Y", &y);
                        if (changed) {
                            rec.values[2] = std::to_string(x); rec.values[3] = std::to_string(y);
                            RefreshNpcTransforms(state); // Modelle nur neu positionieren (nicht neu laden)
                        }
                        int direct = std::atoi(rec.values[4].c_str());
                        if (UI::InputInt("Richtung", &direct)) { rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        // Schnell drehen (Blickrichtung im 3D-View pruefen) + Zuordnung Richtung -> Blickwinkel einstellbar
                        if (UI::SmallButton("-15")) { direct -= 15; rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        ImGui::SameLine();
                        if (UI::SmallButton("+15")) { direct += 15; rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        ImGui::SameLine();
                        if (UI::SmallButton("+90")) { direct += 90; rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        ImGui::SameLine();
                        if (UI::SmallButton("180")) { direct += 180; rec.values[4] = std::to_string(direct); RefreshNpcTransforms(state); }
                        if (UI::Button("Kamera zu diesem NPC")) {
                            const float nx = static_cast<float>(x), nz = static_cast<float>(y);
                            state.camera.SetTarget(nx, state.heightmap.SampleWorld(nx, nz) + 25.0f, nz);
                            state.camera.Zoom(-state.camera.Distance() + 220.0f);
                        }
                        ImGui::SeparatorText("Darstellung & Blickrichtung");
                        {
                            bool orientChanged = false;
                            int signIdx = state.npcDirSign < 0 ? 1 : 0;
                            const char* signItems[] = {"Richtung wie angegeben (+)", "Richtung gespiegelt (-)"};
                            ImGui::SetNextItemWidth(220.0f);
                            if (UI::Combo("Drehsinn", &signIdx, signItems, 2)) { state.npcDirSign = signIdx == 1 ? -1 : 1; orientChanged = true; }
                            ImGui::SetNextItemWidth(220.0f);
                            // Freier Versatz statt fester 90-Grad-Stufen - die Block&Walk-Analyse fand das beste
                            // Ergebnis nicht exakt auf einem 90-Grad-Vielfachen.
                            if (UI::SliderInt("Versatz bei Richtung 0 (Grad)", &state.npcDirOffsetDeg, -180, 180, "%d Grad")) orientChanged = true;
                            ImGui::SameLine();
                            if (UI::SmallButton("0##offreset")) { state.npcDirOffsetDeg = 0; orientChanged = true; }
                            if (orientChanged) RefreshNpcTransforms(state);
                            ImGui::TextDisabled("Der Pfeil zeigt die Blickrichtung des Modells. Stimmt er nicht mit dem Spiel ueberein,\nDrehsinn/Versatz hier einstellen (gilt fuer alle NPCs) oder unten schaetzen lassen.");
                            if (UI::Button("Versatz aus dieser Karte schaetzen (Block&Walk)")) EstimateNpcOrientation(state);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L(
                                "Testet fuer alle NPCs dieser Karte, welcher Versatz sie im Schnitt am ehesten von der naechsten Wand weg blicken laesst\n"
                                "(Annahme: NPCs stehen normalerweise mit dem Ruecken zur Wand). Grobe Schaetzung, kein Ersatz fuer einen Blick ins echte Spiel.",
                                "For all NPCs on this map, tests which offset most often makes them face away from the nearest wall on average\n"
                                "(assumption: NPCs usually stand with their back to a wall). A rough estimate, not a substitute for checking the real game."));
                            if (!state.npcOrientEstimateStatus.empty()) ImGui::TextWrapped("%s", state.npcOrientEstimateStatus.c_str());
                        }
                        ImGui::SeparatorText("Rolle");
                        {
                            // Rolle und Rollenargument (NPC.txt: Spalten Role/RoleArg0) - bearbeitbar.
                            // Bekannte Kombinationen aus NA2016: QuestNpc/Quest|GBDice, Guard/Quest,
                            // Merchant/Item|Weapon|WeaponTitle|Skill|SoulStone|Guild, NPCMenu/Guild|
                            // ExchangeCoin|RandomOption, StoreManager/-. Merchant mit Item/Weapon/
                            // WeaponTitle/Skill/Guild nutzt NPCItemList/<NPC>.txt (siehe Händler-Inventar).
                            ImGui::SetNextItemWidth(160.0f);
                            if (ImGui::BeginCombo("Rolle", rec.values[6].empty() ? "(keine)" : rec.values[6].c_str())) {
                                for (const char* role : kNpcRoles) {
                                    if (UI::Selectable(role, rec.values[6] == role)) rec.values[6] = role;
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(160.0f);
                            {
                                static const char* const kArgs[] = {"Quest", "GBDice", "Item", "Weapon", "WeaponTitle", "Skill", "SoulStone", "Guild", "ExchangeCoin", "RandomOption", "-"};
                                if (ImGui::BeginCombo("Argument", rec.values[7].empty() ? "(keins)" : rec.values[7].c_str())) {
                                    for (const char* arg : kArgs) {
                                        if (UI::Selectable(arg, rec.values[7] == arg)) rec.values[7] = arg;
                                    }
                                    ImGui::EndCombo();
                                }
                            }
                        }
                        ImGui::SeparatorText("Dialog");
                        if (UI::Button("Dialog bearbeiten",ImVec2(-1,0)))
                            OpenNpcDialogEditor(state,rec.values[0]);

                        ImGui::SeparatorText("AI / Lua & Route");
                        if (UI::Button("KI-Skript (Lua) bearbeiten",ImVec2(-1,0)))
                            OpenAiScriptEditor(state,rec.values[0]);
                        if (UI::Button("Patrouillenroute bearbeiten",ImVec2(-1,0)))
                            OpenPatrolRouteEditor(state,rec.values[0]);

                        if (rec.values[6] == "Merchant") {
                            ImGui::SeparatorText("Händler");
                            if (UI::Button("Händler-Inventar bearbeiten",ImVec2(-1,0))) {
                                EnsureShopTextLoaded(state,rec.values[0]);
                                state.shopEditorOpen=true;
                            }
                        }
                    }
                }
                DrawDialogEditorPopup(state);
                ImGui::Separator();
                if (UI::Button("World/NPC.txt speichern")) {
                    auto path = std::filesystem::path(state.shineTextRoot) / "World" / "NPC.txt";
                    auto saved = core::legacy::SaveShineTextFile(state.npcTextFile, path);
                    state.statusMessage = saved ? std::string("NPC.txt gespeichert.") : "Fehler: " + saved.error();
                }
            }
        }
        DrawShopEditorPopup(state);
    } else if (state.editMode == EditMode::Mobs) {
        if (state.shineTextRoot.empty()) {
            ImGui::TextWrapped("Server-Textdaten-Ordner (Server/9Data/Shine, enthält MobRegen/) noch nicht gewählt.");
            std::vector<char> rootBuf(state.shineTextRoot.begin(), state.shineTextRoot.end());
            rootBuf.resize(std::max<std::size_t>(rootBuf.size() + 1, 512));
            ImGui::SetNextItemWidth(-1.0f);
            if (UI::InputText("##shinetextrootmob", rootBuf.data(), rootBuf.size())) {
                state.shineTextRoot.assign(rootBuf.data()); state.mobRegenTextLoaded = false;
            }
#ifdef _WIN32
            if (UI::Button("Ordner wählen...##shinetextmob")) {
                if (auto p = BrowseForFolderWindows("Server/9Data/Shine Ordner wählen")) { state.shineTextRoot = *p; state.mobRegenTextLoaded = false; }
            }
#endif
        } else if (state.legacySaveStem[0] == '\0') {
            ImGui::TextDisabled("Keine Karte offen.");
        } else {
            EnsureMobRegenLoaded(state);
            if (!state.mobRegenTextLoaded) {
                ImGui::TextWrapped("MobRegen/%s.txt konnte nicht geladen werden.", state.legacySaveStem);
            } else {
                auto* zoneTable = state.mobRegenTextFile.FindTable("MobRegenGroup");
                auto* spawnTable = state.mobRegenTextFile.FindTable("MobRegen");
                if (!zoneTable) {
                    ImGui::TextDisabled("Keine 'MobRegenGroup'-Tabelle gefunden.");
                } else {
                    ImGui::TextDisabled("%zu Spawn-Zonen auf '%s' · Auswahl links im Szene-Outliner",
                                        zoneTable->records.size(), state.legacySaveStem);
                    ImGui::SeparatorText("Zonenverwaltung");
                    UI::Checkbox("Löschen freigeben", &state.mobDeleteArmed);
                    if (UI::Button("+ Zone (Kopie der gewählten)") && state.selectedMobZoneIdx >= 0 &&
                        static_cast<std::size_t>(state.selectedMobZoneIdx) < zoneTable->records.size()) {
                        core::legacy::ShineRecord nz = zoneTable->records[static_cast<std::size_t>(state.selectedMobZoneIdx)];
                        nz.sourceLine = 0;
                        // Freien Namen "<Karte><nn>" suchen - GroupIndex verknüpft Zone und Monster.
                        std::string name;
                        for (int n = 1; n < 1000 && name.empty(); ++n) {
                            char buf[64];
                            std::snprintf(buf, sizeof(buf), "%s%02d", state.legacySaveStem, n);
                            bool used = false;
                            for (auto& z : zoneTable->records) if (!z.values.empty() && z.values[0] == buf) used = true;
                            if (!used) name = buf;
                        }
                        if (!nz.values.empty()) nz.values[0] = name;
                        zoneTable->records.push_back(std::move(nz));
                        state.selectedMobZoneIdx = static_cast<int>(zoneTable->records.size()) - 1;
                        state.statusMessage = "Neue Zone '" + name + "' angelegt (noch ohne Monster).";
                    }
                    if (state.mobDeleteArmed && state.selectedMobZoneIdx >= 0 &&
                        static_cast<std::size_t>(state.selectedMobZoneIdx) < zoneTable->records.size()) {
                        ImGui::SameLine();
                        if (UI::Button("Zone samt Monstern löschen")) {
                            const std::string gone = zoneTable->records[static_cast<std::size_t>(state.selectedMobZoneIdx)].values.empty()
                                ? std::string() : zoneTable->records[static_cast<std::size_t>(state.selectedMobZoneIdx)].values[0];
                            zoneTable->records.erase(zoneTable->records.begin() + state.selectedMobZoneIdx);
                            if (spawnTable) {
                                spawnTable->records.erase(std::remove_if(spawnTable->records.begin(), spawnTable->records.end(),
                                    [&](const core::legacy::ShineRecord& r) { return !r.values.empty() && r.values[0] == gone; }), spawnTable->records.end());
                            }
                            state.selectedMobZoneIdx = -1;
                            state.mobDeleteArmed = false;
                        }
                    }
                    if (state.selectedMobZoneIdx >= 0 && static_cast<std::size_t>(state.selectedMobZoneIdx) < zoneTable->records.size()) {
                        auto& zone = zoneTable->records[static_cast<std::size_t>(state.selectedMobZoneIdx)];
                        if (zone.values.size() >= 7) {
                            ImGui::SeparatorText("Zone");
                            ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f),"Zone: %s",zone.values[0].c_str());
                            ImGui::TextDisabled("Position, Ausdehnung, Spawn-Radius und weitere Zonenwerte.");
                            DrawShineRecordFields(zone.values, zoneTable->columns, 1);
                            if (spawnTable) {
                                ImGui::SeparatorText("Monstergruppe");
                                ImGui::TextDisabled("Monster, Anzahl, Spawnwerte sowie AI/Lua und Route je Eintrag.");
                                int removeSpawn = -1;
                                for (std::size_t si = 0; si < spawnTable->records.size(); ++si) {
                                    auto& sr = spawnTable->records[si];
                                    if (sr.values.empty() || sr.values[0] != zone.values[0] || sr.values.size() < 3) continue;
                                    ImGui::PushID(static_cast<int>(si));
                                    const std::string header = sr.values[1] + " x" + sr.values[2] + "##spawn";
                                    if (UI::CollapsingHeader(header.c_str())) {
                                        DrawShineRecordFields(sr.values, spawnTable->columns, 1);
                                        if (UI::SmallButton("KI")) OpenAiScriptEditor(state, sr.values[1]);
                                        ImGui::SameLine();
                                        if (UI::SmallButton("Route")) OpenPatrolRouteEditor(state, sr.values[1]);
                                        ImGui::SameLine();
                                        if (state.mobDeleteArmed && UI::SmallButton("Monster entfernen")) removeSpawn = static_cast<int>(si);
                                    }
                                    ImGui::PopID();
                                }
                                if (removeSpawn >= 0) spawnTable->records.erase(spawnTable->records.begin() + removeSpawn);
                                // Neues Monster in dieser Zone: Name (MobIndex aus MobInfoServer.shn) + Anzahl.
                                ImGui::SetNextItemWidth(160.0f);
                                UI::InputTextWithHint("##newmob", "Monstername (MobIndex)", state.newMobSpawnName, sizeof(state.newMobSpawnName));
                                ImGui::SameLine();
                                if (UI::Button("+ Monster hinzufügen") && state.newMobSpawnName[0] != '\0') {
                                    core::legacy::ShineRecord nr;
                                    nr.sourceLine = 0;
                                    nr.values.assign(std::max<std::size_t>(spawnTable->columns.size(), 3), "0");
                                    nr.values[0] = zone.values[0];
                                    nr.values[1] = state.newMobSpawnName;
                                    nr.values[2] = "1";
                                    spawnTable->records.push_back(std::move(nr));
                                    state.statusMessage = std::string("Monster '") + state.newMobSpawnName + "' zur Zone hinzugefügt (Werte danach anpassen).";
                                }
                            }
                        }
                    }
                }
                ImGui::Separator();
                if (UI::Button("MobRegen speichern")) {
                    auto path = std::filesystem::path(state.shineTextRoot) / "MobRegen" / (std::string(state.legacySaveStem) + ".txt");
                    auto saved = core::legacy::SaveShineTextFile(state.mobRegenTextFile, path);
                    state.statusMessage = saved ? std::string("MobRegen gespeichert.") : "Fehler: " + saved.error();
                }
            }
        }
    } else if (state.editMode == EditMode::Portals) {
        DrawPortalsToolsPanel(state);
    } else {
        // NpcAi / MobAi - laut Mockup vorgesehene, aber noch nicht gebaute Bereiche.
        ImGui::TextDisabled("%s", T("workspace.notimplemented"));
    }
    DrawAiScriptEditorPopup(state); // aus Npcs- UND Mobs-Tab erreichbar, siehe CHANGELOG [0.44.22]
    DrawPatrolRouteEditorPopup(state); // dito, siehe CHANGELOG [0.44.23]

}

// ---------------------------------------------------------------------------
// Objekt-Kategorien (Sichtbarkeit): aus dem Modell-Dateinamen abgeleitet (Schluesselwoerter).
// Die Reihenfolge der Pruefung ist wichtig ("flowerpot" ist Deko, nicht Blume; "lightbug" ein Effekt).
// ---------------------------------------------------------------------------
enum ObjCategory { kCatTrees = 0, kCatGrass, kCatRocks, kCatBuildings, kCatFences, kCatProps, kCatWater, kCatCreatures, kCatEffects, kCatOther, kCatCount };
static const char* const kCategoryNames[kCatCount] = {"Bäume & Büsche", "Gras & Blumen", "Felsen & Steine", "Gebäude", "Zäune, Mauern & Brücken",
                                                      "Dekoration & Möbel", "Wasser & Schiffe", "Tiere & Kreaturen", "Effekte & Licht", "Sonstiges"};

static int ClassifyObjectModel(const std::string& modelPath) {
    std::string name = LowerAscii(modelPath);
    if (const auto slash = name.find_last_of("\\/"); slash != std::string::npos) name = name.substr(slash + 1);
    if (const auto dot = name.rfind('.'); dot != std::string::npos) name.resize(dot);
    auto has = [&](std::initializer_list<const char*> words) {
        for (const char* w : words) if (name.find(w) != std::string::npos) return true;
        return false;
    };
    if (has({"lightbug", "effect", "fire", "smoke", "glow", "particle", "light", "torch", "flame", "sparkle", "spark", "cloud", "sky", "fog"})) return kCatEffects;
    if (has({"flowerpot", "pot", "box", "barrel", "chair", "table", "lamp", "sign", "banner", "bench", "cart", "chest", "crate", "statue", "fountain", "bed", "book", "bottle", "ladder", "flag", "sack", "bucket", "basket", "shelf", "stove", "anvil", "ticker", "machine", "post_lamp", "rope", "candle", "drum", "tube", "wool", "totem", "umbrella", "wagon", "satue", "nest", "notice", "swing", "garden", "safety", "portal"})) return kCatProps;
    if (has({"waterfall", "water", "ship", "boat", "sea", "dock", "anchor", "wave", "sail", "harbor", "fishing", "raft"})) return kCatWater;
    if (has({"chicken", "cat", "dog", "bird", "pig", "horse", "duck", "frog", "mouse", "rabbit", "crab", "seagull", "butterfly", "bee", "fish", "cow", "sheep", "goat", "boar", "bug", "spider", "snake", "wolf", "bat", "butter", "dove", "elephant", "beaver", "turtle", "fox", "cheeta", "deer", "pigeon", "lizard", "monkey"})) return kCatCreatures;
    if (has({"grass", "flower", "flws", "weed", "reed", "lawn", "clover", "mushroom", "fern", "crop", "wheat", "jabpul"})) return kCatGrass;
    if (has({"tree", "palm", "bush", "plant", "leaf", "leaves", "vine", "bamboo", "cactus", "sapling", "wood", "log", "stump", "branch", "shrub", "hedge", "bark"})) return kCatTrees;
    if (has({"rock", "stone", "boulder", "cliff", "cave", "mountain", "pebble", "crystal", "spike", "coral", "reef", "ore", "ston", "dolmen"})) return kCatRocks;
    if (has({"fence", "wall", "gate", "bridge", "stair", "door", "pillar", "arch", "rail", "tunnel", "fort", "barrier", "post"})) return kCatFences;
    if (has({"house", "hall", "shop", "tower", "build", "cabin", "tent", "church", "castle", "smith", "inn", "temple", "hut", "mill", "barn", "stable", "guild", "monolith", "altar", "lodge", "villa", "hotel", "office", "store", "roof", "market", "santuary", "sanctuary", "pavilion"})) return kCatBuildings;
    return kCatOther;
}

// Berechnet (bei Aenderung) Kategorie und "ausgeblendet" je Objekt neu.
static void RefreshObjectVisibility(EditorState& state) {
    SyncObjectEditorMetadata(state);
    std::string key = std::to_string(state.placementSet.Count());
    for (int c = 0; c < kCatCount; ++c) key += state.categoryVisible[c] ? '1' : '0';
    for (char h : state.objectEditorHidden) key += h ? 'h' : '-';
    if (key == state.objectVisKey) return;
    state.objectVisKey = key;
    const std::size_t n = state.placementSet.Count();
    if (state.objectCategory.size() != n) {
        state.objectCategory.assign(n, kCatOther);
        for (std::size_t i = 0; i < n; ++i) {
            const std::string& path = state.placementSet.At(i).modelPath;
            auto it = state.modelCategoryCache.find(path);
            if (it == state.modelCategoryCache.end()) it = state.modelCategoryCache.emplace(path, ClassifyObjectModel(path)).first;
            state.objectCategory[i] = it->second;
        }
    }
    state.objectHidden.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const bool categoryHidden = !state.categoryVisible[state.objectCategory[i]];
        const bool editorHidden = i < state.objectEditorHidden.size() && state.objectEditorHidden[i] != 0;
        state.objectHidden[i] = (categoryHidden || editorHidden) ? 1 : 0;
    }
}
static bool IsObjectHidden(const EditorState& state, std::size_t i) {
    return i < state.objectHidden.size() && state.objectHidden[i] != 0;
}

// Bereich "Sichtbarkeit" im Werkzeug-Panel des Map-Editors.
static void DrawVisibilityPanel(EditorState& state) {
    RefreshObjectVisibility(state);
    UI::Checkbox("Terrain", &state.showTerrain);
    UI::Checkbox("Objekt-Modelle (3D)", &state.showObjectMeshes);
    UI::Checkbox("Objekt-Platzhalter (3D)", &state.showObjectMarkers);

    if (!state.shmdCategoryRenderKind.empty()) {
        int shmdCounts[3] = {};
        for (const int kind : state.shmdCategoryRenderKind) {
            if (kind >= 0 && kind < 3) ++shmdCounts[kind];
        }
        ImGui::SeparatorText("SHMD-Szenenmodelle");
        UI::Checkbox("Sky (SHMD)", &state.showShmdSky);
        ImGui::SameLine(); ImGui::TextDisabled("%d Modell(e)", shmdCounts[0]);
        UI::Checkbox("Water (SHMD)", &state.showShmdWater);
        ImGui::SameLine(); ImGui::TextDisabled("%d Modell(e)", shmdCounts[1]);
        UI::Checkbox("GroundObject (SHMD)", &state.showShmdGroundObject);
        ImGui::SameLine(); ImGui::TextDisabled("%d Modell(e)", shmdCounts[2]);
    }

    UI::Checkbox("NPC-Modelle (3D)", &state.showNpcModels);
    UI::Checkbox("NPC-Namen und Blickpfeile (3D, NPC-Modus)", &state.showNpcLabels);
    if (state.npcModelsMissing > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "%d NPC(s) ohne Modell (Platzhalter fehlen):", state.npcModelsMissing);
        for (const auto& n : state.npcModelsMissingNames) ImGui::BulletText("%s", n.c_str());
    }
    UI::Checkbox("Objekte im 2D-View", &state.showObjects2D);

    if (state.textureStack.LayerCount() > 0) {
        ImGui::SeparatorText("Terrain-Layer");
        state.layerHidden.resize(state.textureStack.LayerCount(), 0);
        for (std::size_t i = 0; i < state.textureStack.LayerCount(); ++i) {
            bool visible = state.layerHidden[i] == 0;
            const std::string label = state.textureStack.Layer(i).name + "##vislayer" + std::to_string(i);
            if (UI::Checkbox(label.c_str(), &visible)) state.layerHidden[i] = visible ? 0 : 1;
        }
        if (UI::SmallButton("Alle Layer an")) std::fill(state.layerHidden.begin(), state.layerHidden.end(), 0);
    }

    if (state.placementSet.Count() > 0) {
        ImGui::SeparatorText("Objekt-Kategorien");
        int counts[kCatCount] = {};
        for (const int c : state.objectCategory) if (c >= 0 && c < kCatCount) ++counts[c];
        for (int c = 0; c < kCatCount; ++c) {
            if (counts[c] == 0) continue;
            ImGui::PushID(c);
            UI::Checkbox((std::string(kCategoryNames[c]) + " (" + std::to_string(counts[c]) + ")").c_str(), &state.categoryVisible[c]);
            ImGui::SameLine();
            if (UI::SmallButton("nur")) { for (int k = 0; k < kCatCount; ++k) state.categoryVisible[k] = (k == c); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Nur diese Kategorie zeigen");
            ImGui::PopID();
        }
        if (UI::SmallButton("Alle an")) for (bool& v : state.categoryVisible) v = true;
        ImGui::SameLine();
        if (UI::SmallButton("Alle aus")) for (bool& v : state.categoryVisible) v = false;
    }
}

// Berechnet (gecacht) die Grundfläche eines Objekt-Modells als Bounding-Box in lokalen X/Z-
// Koordinaten, direkt aus den echten NIF-Vertex-Positionen (core::NifMeshPart::positions) -
// siehe CHANGELOG [0.44.24], Nutzerwunsch "die Grundfläche die den Boden berührt". NIF-Daten
// sind bereits Y-up (X/Z = horizontale Ebene, siehe Achsen-Remap-Doku), passend zur 2D-
// Draufsicht. Auflösung nutzt denselben Kartenordner (state.legacySaveDir) wie
// NifMeshRenderer::LoadModelsForSet für die 3D-Anzeige. Liefert valid=false, wenn Datei nicht
// geladen werden kann (z.B. Model noch nicht im lokal bekannten Assets) - Aufrufer fällt dann
// auf den einfachen Punkt-Marker zurück, kein Fehler.
const EditorState::ObjectFootprint& GetOrComputeFootprint(EditorState& state, const std::string& modelPath) {
    auto it = state.footprintCache.find(modelPath);
    if (it != state.footprintCache.end()) return it->second;

    EditorState::ObjectFootprint fp;
    const auto mapDir = CurrentObjectAssetMapDir(state);
    if (!mapDir.empty()) {
        auto resolved = core::legacy::ResolveLegacyAssetPath(mapDir, modelPath);
        if (resolved) {
            auto model = core::LoadNifMesh(*resolved);
            if (model) {
                bool first = true;
                for (auto& part : model->parts) {
                    for (auto& p : part.positions) {
                        if (first) { fp.minX = fp.maxX = p.x; fp.minZ = fp.maxZ = p.z; first = false; }
                        else {
                            fp.minX = std::min(fp.minX, p.x); fp.maxX = std::max(fp.maxX, p.x);
                            fp.minZ = std::min(fp.minZ, p.z); fp.maxZ = std::max(fp.maxZ, p.z);
                        }
                    }
                }
                fp.valid = !first;
                fp.hull = core::ComputeFootprintHull(*model);
            }
        }
    }
    return state.footprintCache.emplace(modelPath, fp).first->second;
}

// Zeichnet die Grundfläche eines platzierten Objekts als (Y-)rotiertes, skaliertes Rechteck im
// 2D-View - fällt auf den einfachen Punkt-Marker zurück, wenn keine Grundfläche berechnet
// werden konnte. Nur die Y-Rotation aus dem Quaternion wird berücksichtigt (Kippung um X/Z ist
// bei platzierten Objekten praktisch immer 0, siehe ObjectPlacement.hpp) - für die 2D-
// Draufsicht ausreichend.
// Weltpolygon der Grundflaeche eines Objekts (konvexe Huelle der untersten Modellschicht, sonst
// Bounding-Box), gedreht/skaliert/verschoben wie das Objekt im 3D-View.
static std::vector<std::pair<float, float>> ObjectFootprintWorldPolygon(EditorState& state, const core::PlacedObject& obj);

static void StampObjectFootprints(EditorState& state, bool blocked) {
    RefreshObjectVisibility(state);
    core::WalkUndoPatch patch;
    std::vector<std::uint64_t> seen;
    int used = 0;
    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
        if (IsObjectHidden(state, i)) continue;
        const auto polygon = ObjectFootprintWorldPolygon(state, state.placementSet.At(i));
        if (polygon.size() < 3) continue;
        core::ApplyWalkConvexPolygon(state.walkGrid, polygon, blocked, patch, seen);
        ++used;
    }
    RefreshShmdCategoryVisibility(state);
    for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
        if (i < state.shmdCategoryHidden.size() && state.shmdCategoryHidden[i]) continue;
        const auto polygon = ObjectFootprintWorldPolygon(state, state.shmdCategoryRenderSet.At(i));
        if (polygon.size() < 3) continue;
        core::ApplyWalkConvexPolygon(state.walkGrid, polygon, blocked, patch, seen);
        ++used;
    }
    if (!patch.entries.empty()) {
        state.walkUndo.Push(std::move(patch));
        state.walkPreviewDirty = true;
    }
    state.statusMessage = std::to_string(used) + " Objekt-Grundflächen " + (blocked ? "gesperrt" : "freigegeben") + " (Rückgängig möglich).";
}

void DrawObjectFootprint2D(EditorState& state, const core::PlacedObject& obj, ImDrawList* drawList,
                            const ImVec2& cursorScreenPos, const ImVec2& imageSize,
                            float spanX, float spanZ, ImU32 color, float thickness) {
    const auto& fp = GetOrComputeFootprint(state, obj.modelPath);
    if (!fp.valid) return;
    const float angle = 2.0f * std::atan2(obj.rotY, obj.rotW);
    const float c = std::cos(angle), s = std::sin(angle);
    // Praezise Grundflaeche: konvexe Huelle der untersten Modellschicht (CHANGELOG [0.44.27]),
    // Rueckfall auf die Bounding-Box, wenn keine Huelle berechenbar war.
    std::vector<std::pair<float, float>> localPoints = fp.hull;
    if (localPoints.size() < 3) {
        localPoints = {{fp.minX, fp.minZ}, {fp.maxX, fp.minZ}, {fp.maxX, fp.maxZ}, {fp.minX, fp.maxZ}};
    }
    std::vector<ImVec2> screenCorners;
    screenCorners.reserve(localPoints.size());
    for (const auto& [lx, lz] : localPoints) {
        const float sx = lx * obj.scale, sz = lz * obj.scale;
        // R_y(angle) wie das 3D-Quaternion (0,sin,0,cos): x' = x*c + z*s, z' = -x*s + z*c.
        const float wx = obj.posX + (sx * c + sz * s);
        const float wz = obj.posZ + (-sx * s + sz * c);
        const float ou = spanX > 0.0f ? wx / spanX : 0.0f;
        const float ov = 1.0f - (spanZ > 0.0f ? wz / spanZ : 0.0f);
        screenCorners.emplace_back(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
    }
    drawList->AddPolyline(screenCorners.data(), static_cast<int>(screenCorners.size()), color, ImDrawFlags_Closed, thickness);
}

static std::vector<std::pair<float, float>> ObjectFootprintWorldPolygon(EditorState& state, const core::PlacedObject& obj) {
    const auto& fp = GetOrComputeFootprint(state, obj.modelPath);
    if (!fp.valid) return {};
    const float angle = 2.0f * std::atan2(obj.rotY, obj.rotW);
    const float c = std::cos(angle), s = std::sin(angle);
    std::vector<std::pair<float, float>> local = fp.hull;
    if (local.size() < 3) local = {{fp.minX, fp.minZ}, {fp.maxX, fp.minZ}, {fp.maxX, fp.maxZ}, {fp.minX, fp.maxZ}};
    std::vector<std::pair<float, float>> world;
    world.reserve(local.size());
    for (const auto& [lx, lz] : local) {
        const float sx = lx * obj.scale, sz = lz * obj.scale;
        world.emplace_back(obj.posX + (sx * c + sz * s), obj.posZ + (-sx * s + sz * c));
    }
    return world;
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
    // Sichtbarer Bereich (Viewport) = ganze Kartenbreite; Zoom/Verschiebung (CHANGELOG [0.44.32]):
    // `imageSize`/`cursorScreenPos` beschreiben ab hier das VIRTUELLE, gezoomte Kartenbild (Kartenrand
    // links oben = cursorScreenPos, Groesse = Viewport * Zoom) - alle Marker- und Mausumrechnungen
    // unten (u = (Maus - cursorScreenPos) / imageSize) gelten damit unveraendert auch gezoomt. Der
    // Viewport zeigt nur den passenden Ausschnitt (Renderer-Fenster + Clip-Rechteck).
    const ImVec2 viewSize(availW, availW * aspect);
    const ImVec2 viewMin = ImGui::GetCursorScreenPos();
    const float zoom = std::clamp(state.view2dZoom, 1.0f, 40.0f);
    state.view2dZoom = zoom;
    state.view2dCenterU = std::clamp(state.view2dCenterU, 0.5f / zoom, 1.0f - 0.5f / zoom);
    state.view2dCenterV = std::clamp(state.view2dCenterV, 0.5f / zoom, 1.0f - 0.5f / zoom);
    const ImVec2 imageSize(viewSize.x * zoom, viewSize.y * zoom);
    const ImVec2 cursorScreenPos(viewMin.x + viewSize.x * 0.5f - state.view2dCenterU * imageSize.x,
                                 viewMin.y + viewSize.y * 0.5f - state.view2dCenterV * imageSize.y);

    // Weltfenster: u -> X, Bildzeile v -> Z = (1 - v) * spanZ.
    const float winCenterX = state.view2dCenterU * spanX;
    const float winCenterZ = (1.0f - state.view2dCenterV) * spanZ;
    state.renderer.SetTopDownWindow(winCenterX, winCenterZ, spanX * 0.5f / zoom, spanZ * 0.5f / zoom);
    state.renderer.BeginTopDownScene(static_cast<int>(viewSize.x), static_cast<int>(viewSize.y));
    if (walkMode) {
        // Das Walk-Gitter ist quadratisch (Zelle 6.25) - bei nicht-quadratischen Karten deckt die Karte
        // nur einen Teil ab: UV-Bereich = Kartenspanne / Gitterspanne.
        const float gridSpanX = std::max(1.0f, static_cast<float>(state.walkGrid.Cols()) * core::WalkGrid::kCellSize);
        const float gridSpanZ = std::max(1.0f, static_cast<float>(state.walkGrid.Rows()) * core::WalkGrid::kCellSize);
        const float mapU = std::min(1.0f, spanX / gridSpanX), mapV = std::min(1.0f, spanZ / gridSpanZ);
        // Overlay-UV = Kartenausschnitt (normiert) * Anteil der Textur, den die Karte belegt.
        const float x0n = state.view2dCenterU - 0.5f / zoom, z0n = (1.0f - state.view2dCenterV) - 0.5f / zoom;
        state.renderer.DrawTopDownOverlay(state.walkPreviewTex, 0.75f, mapU / zoom, mapV / zoom, x0n * mapU, z0n * mapV);
    }
    const GLuint tex = state.renderer.EndTopDownScene();
    state.renderer.ResetTopDownWindow();

    // WICHTIG (CHANGELOG [0.44.27]): Norden (groesseres Legacy-Y = groesseres Z) liegt im Bild
    // OBEN, wie bei einer echten Karte und wie die Kartenbitmaps (BMP-Zeile 0 = unten). Die FBO-
    // Textur hat Z=0 unten (OpenGL), daher hier die UVs NICHT vertauschen. Alle Marker/Maus-
    // Umrechnungen unten nutzen deshalb v = 1 - z/spanZ. Der Grund: die Achsenvertauschung
    // Legacy->Editor ist eine Spiegelung, ohne diese Umkehr waere die Karte im Bild kopfueber.
    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), viewSize,
                 ImVec2(0, 0), ImVec2(1, 1));
    const bool hoveredView = ImGui::IsItemHovered();
    if (objectMode && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_NIF_ASSET")) {
            const char* model=static_cast<const char*>(payload->Data);
            const ImVec2 mouse=ImGui::GetMousePos();
            const float u=(mouse.x-cursorScreenPos.x)/imageSize.x;
            const float v=(mouse.y-cursorScreenPos.y)/imageSize.y;
            const float worldX=std::clamp(u,0.0f,1.0f)*spanX;
            const float worldZ=(1.0f-std::clamp(v,0.0f,1.0f))*spanZ;
            PlaceObjectAtWorld(state,model,worldX,state.heightmap.SampleWorld(worldX,worldZ),worldZ);
            state.statusMessage=std::string("Objekt in 2D platziert: ")+model;
        }
        ImGui::EndDragDropTarget();
    }
    {
        // Zoom (Mausrad, um den Mauszeiger) und Verschieben (mittlere/rechte Taste ziehen).
        ImGuiIO& io2 = ImGui::GetIO();
        if (hoveredView && io2.MouseWheel != 0.0f) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const float mu = (mouse.x - cursorScreenPos.x) / imageSize.x;
            const float mv = (mouse.y - cursorScreenPos.y) / imageSize.y;
            const float newZoom = std::clamp(zoom * std::pow(1.2f, io2.MouseWheel), 1.0f, 40.0f);
            const ImVec2 newSize(viewSize.x * newZoom, viewSize.y * newZoom);
            // Der Kartenpunkt unter dem Mauszeiger bleibt unter dem Mauszeiger.
            state.view2dCenterU = (viewMin.x + viewSize.x * 0.5f - mouse.x) / newSize.x + mu;
            state.view2dCenterV = (viewMin.y + viewSize.y * 0.5f - mouse.y) / newSize.y + mv;
            state.view2dZoom = newZoom;
        }
        if (hoveredView && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))) {
            state.view2dCenterU -= io2.MouseDelta.x / imageSize.x;
            state.view2dCenterV -= io2.MouseDelta.y / imageSize.y;
        }
    }
    // Alles Folgende (Marker, Fussabdruecke, Pinsel) wird auf den Viewport beschnitten.
    ImGui::GetWindowDrawList()->PushClipRect(viewMin, ImVec2(viewMin.x + viewSize.x, viewMin.y + viewSize.y), true);
    struct ClipPop { ~ClipPop() { ImGui::GetWindowDrawList()->PopClipRect(); } } clipPop;

    if (hoveredView && (state.editMode==EditMode::Heightmap || texMode || walkMode)) {
        const float radius=ActiveWorldBrushRadius(state);
        const ImVec2 mouse=ImGui::GetMousePos();
        const float rx=spanX>0.0f ? radius/spanX*imageSize.x : 0.0f;
        const float ry=spanZ>0.0f ? radius/spanZ*imageSize.y : rx;
        const float rp=std::max(2.0f,(rx+ry)*0.5f);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        dl->AddCircle(mouse,rp,ActiveBrushColor(state),64,2.0f);
        if(!walkMode) dl->AddCircle(mouse,rp*0.5f,ActiveBrushColor(state,110),48,1.0f);
        dl->AddCircleFilled(mouse,2.5f,ActiveBrushColor(state));
    }

    if (objectMode || walkMode) {
        // Marker für alle platzierten Objekte einzeichnen - im Objekt-Modus normal (weiß =
        // ausgewählt, blau = übrige), im Block/Walk-Modus zusätzlich als dezente REFERENZ
        // (halbtransparent, keine Auswahl/Bearbeitung hier) - Nutzerwunsch: man muss sehen,
        // wo Gebäude stehen, um die Lauf-Sperren drumherum korrekt zu setzen, siehe CHANGELOG
        // [0.44.23]. Objekte bleiben im Block/Walk-Modus rein informativ, nicht anklickbar -
        // versehentliches Verschieben beim Grid-Malen wäre schlimmer als gar keine Anzeige.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        RefreshObjectVisibility(state);
        for (std::size_t i = 0; i < state.placementSet.Count() && state.showObjects2D; ++i) {
            if (IsObjectHidden(state, i)) continue;
            const auto& obj = state.placementSet.At(i);
            const float ou = spanX > 0.0f ? obj.posX / spanX : 0.0f;
            const float ov = 1.0f - (spanZ > 0.0f ? obj.posZ / spanZ : 0.0f);
            const ImVec2 screenPos(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
            if (walkMode) {
                // Dezenter Referenz-Punkt + dünner Ring als Fallback (falls keine Grundfläche
                // berechenbar), PLUS die echte Grundfläche (rotiertes Rechteck aus den NIF-
                // Vertex-Positionen) wo möglich - Nutzerwunsch, siehe CHANGELOG [0.44.24].
                drawList->AddCircleFilled(screenPos, 3.0f, IM_COL32(230, 230, 235, 150));
                drawList->AddCircle(screenPos, 7.0f, IM_COL32(230, 230, 235, 90), 16, 1.0f);
                DrawObjectFootprint2D(state, obj, drawList, cursorScreenPos, imageSize, spanX, spanZ,
                                       IM_COL32(230, 230, 235, 180), 1.5f);
            } else {
                const bool selected = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), static_cast<int>(i)) != state.selectedObjects.end();
                drawList->AddCircleFilled(screenPos, selected ? 5.0f : 3.5f,
                                           selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 135, 210, 220));
                DrawObjectFootprint2D(state, obj, drawList, cursorScreenPos, imageSize, spanX, spanZ,
                                       selected ? IM_COL32(255, 255, 255, 200) : IM_COL32(70, 135, 210, 160), 1.5f);
            }
        }

        // Sky/Water/GroundObject besitzen keinen SHMD-Transform, aber ihre NIF-Geometrie kann
        // trotzdem bereits an Weltkoordinaten liegen. Deshalb werden ihre echten Grundflächen
        // ebenfalls im 2D-Editor gezeichnet und können über die Liste ausgewählt werden.
        RefreshShmdCategoryVisibility(state);
        if (state.showObjects2D) {
            for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
                if (i < state.shmdCategoryHidden.size() && state.shmdCategoryHidden[i]) continue;
                const auto& obj = state.shmdCategoryRenderSet.At(i);
                const int id = ShmdSelectionId(i);
                const bool selected = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), id) != state.selectedObjects.end();
                DrawObjectFootprint2D(state, obj, drawList, cursorScreenPos, imageSize, spanX, spanZ,
                                      selected ? IM_COL32(255, 255, 255, 220) : IM_COL32(70, 190, 210, 170),
                                      selected ? 2.5f : 1.5f);
                const auto polygon = ObjectFootprintWorldPolygon(state, obj);
                if (!polygon.empty()) {
                    float cx = 0.0f, cz = 0.0f;
                    for (const auto& p : polygon) { cx += p.first; cz += p.second; }
                    cx /= static_cast<float>(polygon.size());
                    cz /= static_cast<float>(polygon.size());
                    const float ou = spanX > 0.0f ? cx / spanX : 0.0f;
                    const float ov = 1.0f - (spanZ > 0.0f ? cz / spanZ : 0.0f);
                    const ImVec2 center(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
                    const float radius = selected ? 5.0f : 3.5f;
                    drawList->AddRectFilled(ImVec2(center.x - radius, center.y - radius),
                                            ImVec2(center.x + radius, center.y + radius),
                                            selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 190, 210, 230));
                }
            }
        }
    }
    if (state.editMode == EditMode::Npcs && state.npcTextLoaded && state.legacySaveStem[0] != '\0') {
        // NPC-Marker als Quadrate (statt Kreise wie bei Objekten) - selbe Weiß/Blau-Logik,
        // aber optisch unterscheidbar, ohne eine neue Farbe außerhalb des Themes einzuführen
        // (siehe CHANGELOG [0.44.12]). Koordinaten aus World/NPC.txt (Coord-X/Coord-Y) nutzen
        // dieselbe Weltraum-Konvention wie posX/posZ der regulären Objekt-Platzierung.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        auto* table = state.npcTextFile.FindTable("ShineNPC");
        if (table) {
            for (std::size_t idx : NpcRecordsForCurrentMap(state)) {
                const auto& rec = table->records[idx];
                if (rec.values.size() < 4) continue;
                const float npcX = static_cast<float>(std::atoi(rec.values[2].c_str()));
                const float npcY = static_cast<float>(std::atoi(rec.values[3].c_str()));
                const float ou = spanX > 0.0f ? npcX / spanX : 0.0f;
                const float ov = 1.0f - (spanZ > 0.0f ? npcY / spanZ : 0.0f);
                const ImVec2 c(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
                const bool selected = state.selectedNpcRecordIdx == static_cast<int>(idx);
                const float r = selected ? 5.0f : 3.5f;
                drawList->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r),
                                         selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 135, 210, 220));
                if (rec.values.size() >= 5) {
                    // Blickrichtung: Modell schaut bei Winkel 0 nach -Z (Charaktermodelle), gedreht um die Hochachse.
                    const float yaw = NpcYawRadians(state, std::atoi(rec.values[4].c_str()));
                    const float fx = -std::sin(yaw), fz = -std::cos(yaw);
                    const float len = selected ? 22.0f : 15.0f;
                    const ImVec2 tip(c.x + fx * len, c.y - fz * len); // Norden = oben im Bild
                    drawList->AddLine(c, tip, selected ? IM_COL32(255, 210, 90, 255) : IM_COL32(255, 210, 90, 170), selected ? 2.5f : 1.5f);
                    drawList->AddCircleFilled(tip, selected ? 3.0f : 2.0f, IM_COL32(255, 210, 90, 255));
                }
            }
        }
    }
    if (state.editMode == EditMode::Mobs && state.mobRegenTextLoaded) {
        // Mob-Spawn-Zonen: kleines Icon (gefüllter Kreis) in der Mitte + dünner Umriss-Kreis
        // für den Spawn-Radius (Nutzerwunsch, siehe CHANGELOG [0.44.17]). Radius-Näherung: bei
        // Width/Height > 0 (rechteckig/länglich wirkende Zonen) die größere der beiden Seiten
        // halbiert, sonst (Width=Height=0, häufigster Fall) RangeDegree direkt als Radius -
        // letzteres Feld ist trotz seines Namens in diesen Fällen ein Weltraum-Radius, kein
        // Winkel (Werte wie 507 passen nicht in 0-360°, siehe docs/SHN_DEPENDENCIES.md wäre
        // der richtige Ort für eine spätere, genauere Form-Unterscheidung).
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        auto* zoneTable = state.mobRegenTextFile.FindTable("MobRegenGroup");
        if (zoneTable) {
            for (std::size_t i = 0; i < zoneTable->records.size(); ++i) {
                const auto& rec = zoneTable->records[i];
                if (rec.values.size() < 7) continue;
                const float cx = static_cast<float>(std::atoi(rec.values[2].c_str()));
                const float cy = static_cast<float>(std::atoi(rec.values[3].c_str()));
                const float w = static_cast<float>(std::atoi(rec.values[4].c_str()));
                const float h = static_cast<float>(std::atoi(rec.values[5].c_str()));
                const float rangeVal = static_cast<float>(std::atoi(rec.values[6].c_str()));
                const float radius = (w > 0.0f || h > 0.0f) ? std::max(w, h) * 0.5f : rangeVal;
                const float ou = spanX > 0.0f ? cx / spanX : 0.0f;
                const float ov = 1.0f - (spanZ > 0.0f ? cy / spanZ : 0.0f);
                const ImVec2 c(cursorScreenPos.x + ou * imageSize.x, cursorScreenPos.y + ov * imageSize.y);
                const float screenRadius = spanX > 0.0f ? std::max((radius / spanX) * imageSize.x, 2.0f) : 2.0f;
                const bool selected = state.selectedMobZoneIdx == static_cast<int>(i);
                const ImU32 col = selected ? IM_COL32(255, 255, 255, 220) : IM_COL32(70, 135, 210, 180);
                drawList->AddCircle(c, screenRadius, col, 32, selected ? 2.0f : 1.25f);
                drawList->AddCircleFilled(c, selected ? 5.0f : 3.5f, selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 135, 210, 220));
            }
        }
    }

    if (state.editMode == EditMode::Portals && state.legacySaveStem[0] != '\0') {
        DrawPortalMarkers2D(state, cursorScreenPos, imageSize, spanX, spanZ);
    }

    const bool hovered = hoveredView;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) state.objectDragActive = false;

    // Shift + Ziehen = Rechteckauswahl. Strg+Shift fügt zur bestehenden Auswahl hinzu.
    bool suppressObjectClickForBox = false;
    if (objectMode && !state.objectPlaceMode) {
        const ImGuiIO& ioBox=ImGui::GetIO();
        if (hovered && ioBox.KeyShift && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            state.objectBoxSelectActive=true;
            state.objectBoxSelectStart=ImGui::GetMousePos();
            state.objectDragActive=false;
        }
        if (state.objectBoxSelectActive) {
            suppressObjectClickForBox=true;
            const ImVec2 cur=ImGui::GetMousePos();
            const ImVec2 lo(std::min(state.objectBoxSelectStart.x,cur.x),std::min(state.objectBoxSelectStart.y,cur.y));
            const ImVec2 hi(std::max(state.objectBoxSelectStart.x,cur.x),std::max(state.objectBoxSelectStart.y,cur.y));
            ImDrawList* dl=ImGui::GetWindowDrawList();
            dl->AddRectFilled(lo,hi,IM_COL32(40,150,225,30));
            dl->AddRect(lo,hi,IM_COL32(80,190,255,235),0.0f,0,1.5f);

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                std::vector<int> hits;
                RefreshObjectVisibility(state);
                auto inside=[&](const ImVec2& p){return p.x>=lo.x&&p.x<=hi.x&&p.y>=lo.y&&p.y<=hi.y;};
                for(std::size_t i=0;i<state.placementSet.Count();++i) {
                    if(IsObjectHidden(state,i)||IsObjectEditorLocked(state,static_cast<int>(i))) continue;
                    const auto& obj=state.placementSet.At(i);
                    const float ou=spanX>0.0f?obj.posX/spanX:0.0f;
                    const float ov=1.0f-(spanZ>0.0f?obj.posZ/spanZ:0.0f);
                    const ImVec2 p(cursorScreenPos.x+ou*imageSize.x,cursorScreenPos.y+ov*imageSize.y);
                    if(inside(p)) hits.push_back(static_cast<int>(i));
                }
                RefreshShmdCategoryVisibility(state);
                for(std::size_t i=0;i<state.shmdCategoryRenderSet.Count();++i) {
                    if(i<state.shmdCategoryHidden.size()&&state.shmdCategoryHidden[i]) continue;
                    const auto poly=ObjectFootprintWorldPolygon(state,state.shmdCategoryRenderSet.At(i));
                    if(poly.empty()) continue;
                    float cx=0.0f,cz=0.0f;
                    for(const auto& p:poly){cx+=p.first;cz+=p.second;}
                    cx/=static_cast<float>(poly.size());cz/=static_cast<float>(poly.size());
                    const ImVec2 p(cursorScreenPos.x+(cx/spanX)*imageSize.x,
                                   cursorScreenPos.y+(1.0f-cz/spanZ)*imageSize.y);
                    if(inside(p)) hits.push_back(ShmdSelectionId(i));
                }
                if(!ioBox.KeyCtrl) state.selectedObjects.clear();
                for(const int id:hits)
                    if(std::find(state.selectedObjects.begin(),state.selectedObjects.end(),id)==state.selectedObjects.end())
                        state.selectedObjects.push_back(id);
                state.selectedObject=state.selectedObjects.empty()?kNoObjectSelection:state.selectedObjects.back();
                state.selectedObjectModelPathFor=kNoObjectSelection;
                state.objectGizmoMatrixValid=false;
                state.objectBoxSelectActive=false;
                state.statusMessage=std::to_string(hits.size())+" Objekt(e) im Rechteck ausgewählt.";
            }
        }
    }

    bool suppressWalkClickForRect=false;
    if (walkMode) {
        const ImGuiIO& ioWalk=ImGui::GetIO();
        if (hovered && ioWalk.KeyShift && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            state.walkRectActive=true;
            state.walkRectStart=ImGui::GetMousePos();
        }
        if (state.walkRectActive) {
            suppressWalkClickForRect=true;
            const ImVec2 cur=ImGui::GetMousePos();
            const ImVec2 lo(std::min(state.walkRectStart.x,cur.x),std::min(state.walkRectStart.y,cur.y));
            const ImVec2 hi(std::max(state.walkRectStart.x,cur.x),std::max(state.walkRectStart.y,cur.y));
            ImDrawList* dl=ImGui::GetWindowDrawList();
            const ImU32 col=state.walkBlockMode?IM_COL32(245,85,85,235):IM_COL32(80,220,145,235);
            dl->AddRectFilled(lo,hi,state.walkBlockMode?IM_COL32(245,85,85,28):IM_COL32(80,220,145,28));
            dl->AddRect(lo,hi,col,0.0f,0,1.5f);
            if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                auto toWorld=[&](const ImVec2& p) {
                    const float u=std::clamp((p.x-cursorScreenPos.x)/imageSize.x,0.0f,1.0f);
                    const float v=std::clamp((p.y-cursorScreenPos.y)/imageSize.y,0.0f,1.0f);
                    return std::pair<float,float>{u*spanX,(1.0f-v)*spanZ};
                };
                const auto a=toWorld(lo), b=toWorld(hi);
                const float x0=std::min(a.first,b.first), x1=std::max(a.first,b.first);
                const float z0=std::min(a.second,b.second), z1=std::max(a.second,b.second);
                std::vector<std::pair<float,float>> polygon={{x0,z0},{x1,z0},{x1,z1},{x0,z1}};
                core::WalkUndoPatch patch;
                std::vector<std::uint64_t> seenWords;
                core::ApplyWalkConvexPolygon(state.walkGrid,polygon,state.walkBlockMode,patch,seenWords);
                if(!patch.entries.empty()) {
                    state.walkUndo.Push(std::move(patch));
                    state.walkPreviewDirty=true;
                }
                state.walkRectActive=false;
                state.statusMessage=state.walkBlockMode?"Walk-Rechteck gesperrt.":"Walk-Rechteck freigegeben.";
            }
        }
    }

    // Object placement benötigt sowohl den initialen Klick als auch die folgenden
    // Mouse-Down-Frames für flüssiges Verschieben. Die übrigen Modi behalten ihr
    // bisheriges Klick-/Drag-Verhalten.
    const bool clickTrigger = objectMode
                                   ? (!suppressObjectClickForBox && (state.objectPlaceMode
                                          ? ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                                          : (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                                             ImGui::IsMouseDown(ImGuiMouseButton_Left))))
                                   : ((state.editMode == EditMode::Npcs || state.editMode == EditMode::Mobs ||
                                       state.editMode == EditMode::Portals)
                                          ? ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                                          : (!suppressWalkClickForRect && ImGui::IsMouseDown(ImGuiMouseButton_Left)));
    if (hovered && clickTrigger && imageSize.x > 0 && imageSize.y > 0) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const float u = (mouse.x - cursorScreenPos.x) / imageSize.x;
        const float v = (mouse.y - cursorScreenPos.y) / imageSize.y;

        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
            // Weltraum-Ausdehnung immer aus der Heightmap ableiten (alle Gitter decken dieselbe
            // physische Kartenfläche ab, nur mit unterschiedlicher Auflösung) - so bleibt die
            // Maus-Zuordnung über alle Bearbeitungsmodi hinweg konsistent.
            const float worldX = u * spanX;
            const float worldZ = (1.0f - v) * spanZ;

            if (objectMode) {
                // Bereits ausgewählte Objekte mit der linken Maustaste verschieben. Die
                // Bildschirmbewegung wird direkt in Weltkoordinaten umgerechnet, sodass
                // Mehrfachauswahl gemeinsam bewegt werden kann.
                if (!state.objectPlaceMode && state.objectDragActive && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                    ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f) && !state.selectedObjects.empty()) {
                    const ImVec2 delta = ImGui::GetIO().MouseDelta;
                    const float dx = imageSize.x > 0.0f ? delta.x * spanX / imageSize.x : 0.0f;
                    const float dz = imageSize.y > 0.0f ? -delta.y * spanZ / imageSize.y : 0.0f;
                    // 2D-Drag verändert nur X/Z. Y bleibt bewusst unverändert, damit mehrere
                    // Objekte ihre individuellen Höhen/Offsets nicht beim ersten Pixel verlieren.
                    MoveSelectedObjectsBy(state, dx, 0.0f, dz);
                }
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
                    state.objectEditorHidden.push_back(0);
                    state.objectEditorLocked.push_back(0);
                    state.selectedObjects = {state.selectedObject};
                    state.selectedObjectModelPathFor = kNoObjectSelection;
                    state.objectListRangeAnchor = -1;
                    ReloadObjectRenderers(state);
                } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    // Auswahl wird ausschließlich auf dem initialen Klick geändert. Danach darf
                    // der gehaltene Klick nur noch die bereits ausgewählten Objekte verschieben.
                    // So bleibt Ctrl-Mehrfachauswahl stabil und Dragging überschreibt sie nicht.
                    // Nächstes Objekt innerhalb einer kleinen Toleranz suchen und auswählen.
                    const float tolerance = spanX * 0.04f / std::max(zoom, 0.25f);
                    float bestDist = tolerance;
                    int bestIdx = -1;
                    for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
                        if (IsObjectEditorLocked(state, static_cast<int>(i))) continue;
                        const auto& obj = state.placementSet.At(i);
                        const float dx = obj.posX - worldX;
                        const float dz = obj.posZ - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestIdx = static_cast<int>(i);
                        }
                    }
                    RefreshShmdCategoryVisibility(state);
                    for (std::size_t i = 0; i < state.shmdCategoryRenderSet.Count(); ++i) {
                        if (i < state.shmdCategoryHidden.size() && state.shmdCategoryHidden[i]) continue;
                        const auto polygon = ObjectFootprintWorldPolygon(state, state.shmdCategoryRenderSet.At(i));
                        if (polygon.empty()) continue;
                        float cx = 0.0f, cz = 0.0f;
                        for (const auto& p : polygon) { cx += p.first; cz += p.second; }
                        cx /= static_cast<float>(polygon.size());
                        cz /= static_cast<float>(polygon.size());
                        const float dx = cx - worldX, dz = cz - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestIdx = ShmdSelectionId(i);
                        }
                    }
                    if (bestIdx != kNoObjectSelection) {
                        SelectObjectOnCanvas(state, bestIdx, ImGui::GetIO().KeyCtrl);
                    } else if (!ImGui::GetIO().KeyCtrl) {
                        ClearObjectSelection(state);
                    }
                    if (bestIdx != kNoObjectSelection) {
                        state.objectDragActive =
                            std::find(state.selectedObjects.begin(), state.selectedObjects.end(), bestIdx) != state.selectedObjects.end();
                    }
                }
            } else if (state.editMode == EditMode::Npcs && state.npcTextLoaded) {
                // Nächsten NPC innerhalb einer kleinen Toleranz suchen und auswählen (analog zur
                // Objekt-Platzierung oben) - Klicken in diesem Modus darf NICHT die Heightmap
                // bearbeiten (siehe "else if (!texMode && !walkMode)" unten).
                auto* table = state.npcTextFile.FindTable("ShineNPC");
                if (table) {
                    const float tolerance = spanX * 0.02f / zoom;
                    float bestDist = tolerance;
                    int bestIdx = -1;
                    for (std::size_t idx : NpcRecordsForCurrentMap(state)) {
                        const auto& rec = table->records[idx];
                        if (rec.values.size() < 4) continue;
                        const float dx = static_cast<float>(std::atoi(rec.values[2].c_str())) - worldX;
                        const float dz = static_cast<float>(std::atoi(rec.values[3].c_str())) - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) { bestDist = dist; bestIdx = static_cast<int>(idx); }
                    }
                    state.selectedNpcRecordIdx = bestIdx;
                }
            } else if (state.editMode == EditMode::Mobs && state.mobRegenTextLoaded) {
                // Nächste Spawn-Zone auswählen (Toleranz etwas großzügiger als bei NPCs, da
                // Zonen typischerweise ausgedehnter sind).
                auto* zoneTable = state.mobRegenTextFile.FindTable("MobRegenGroup");
                if (zoneTable) {
                    const float tolerance = spanX * 0.04f / zoom;
                    float bestDist = tolerance;
                    int bestIdx = -1;
                    for (std::size_t i = 0; i < zoneTable->records.size(); ++i) {
                        const auto& rec = zoneTable->records[i];
                        if (rec.values.size() < 4) continue;
                        const float dx = static_cast<float>(std::atoi(rec.values[2].c_str())) - worldX;
                        const float dz = static_cast<float>(std::atoi(rec.values[3].c_str())) - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) { bestDist = dist; bestIdx = static_cast<int>(i); }
                    }
                    state.selectedMobZoneIdx = bestIdx;
                }
            } else if (state.editMode == EditMode::Portals) {
                // Klick setzt entweder die Position des gewaehlten Ziels (Pick-Modus) oder
                // waehlt das naechste Ziel aus. Darf NIE die Heightmap bearbeiten.
                if (state.portalPickMode && state.selectedPortalKind != kPortalKindNone) {
                    SetSelectedPortalPosition(state, std::llround(worldX), std::llround(worldZ));
                } else {
                    const float tolerance = spanX * 0.02f / zoom;
                    float bestDist = tolerance;
                    int bestKind = kPortalKindNone, bestIdx = -1;
                    for (const auto& m : CollectPortalMarkers(state)) {
                        const float dx = m.x - worldX, dz = m.y - worldZ;
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        if (dist < bestDist) { bestDist = dist; bestKind = m.kind; bestIdx = static_cast<int>(m.idx); }
                    }
                    state.selectedPortalKind = bestKind;
                    state.selectedPortalIdx = bestIdx;
                }
            } else if (state.editMode == EditMode::Heightmap) {
                core::UndoPatch patch = core::ApplyBrush(state.heightmap, state.brushMode, state.brush, worldX, worldZ);
                if (!patch.entries.empty()) {
                    state.undo.Push(std::move(patch));
                    state.meshDirty = true;
                }
            } else if (texMode) {
                // Eigene Zellgröße des Textur-Gitters (unabhängige Auflösung, siehe
                // docs/MAP_FORMAT.md) statt der Heightmap-Blockgröße - sonst wäre der Pinsel bei
                // z.B. 512x512-Textur-Layern auf einer 257x257-Heightmap deutlich daneben.
                float texCellW = spanX / static_cast<float>(state.textureStack.Width());
                float texCellH = spanZ / static_cast<float>(state.textureStack.Height());
                // Layer mit eigener Region (z.B. Adl: linke/rechte Kartenhaelfte, siehe
                // TextureLayer::regionStartX): Welt-Position in die Region umrechnen, das Gitter
                // deckt nur die Region ab; nur Layer mit derselben Region nehmen an der
                // Normalisierung teil. Ohne Region (0) bleibt alles wie bisher.
                float paintX = worldX, paintZ = worldZ;
                core::TexturePaintSettings paintSettings = state.paintSettings;
                std::vector<char> sameRegion;
                if (state.selectedLayer >= 0 && static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount()) {
                    const auto& active = state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer));
                    if (active.regionWidth > 0.0f && active.regionHeight > 0.0f) {
                        const float bw = state.heightmap.BlockWidth(), bh = state.heightmap.BlockHeight();
                        paintX = worldX - active.regionStartX * bw;
                        paintZ = worldZ - active.regionStartY * bh;
                        texCellW = active.regionWidth * bw / static_cast<float>(state.textureStack.Width());
                        texCellH = active.regionHeight * bh / static_cast<float>(state.textureStack.Height());
                    }
                    sameRegion.assign(state.textureStack.LayerCount(), 0);
                    for (std::size_t li = 0; li < state.textureStack.LayerCount(); ++li) {
                        const auto& other = state.textureStack.Layer(li);
                        sameRegion[li] = other.regionStartX == active.regionStartX && other.regionStartY == active.regionStartY &&
                                         other.regionWidth == active.regionWidth && other.regionHeight == active.regionHeight;
                    }
                    paintSettings.participating = &sameRegion;
                }
                core::TexturePaintPatch patch = core::PaintLayerWeight(
                    state.textureStack, static_cast<std::size_t>(state.selectedLayer), state.paintMode,
                    paintSettings, paintX, paintZ, texCellW, texCellH);
                if (!patch.entries.empty()) {
                    state.textureUndo.Push(std::move(patch));
                    state.layerPreviewDirty = true;
                    state.renderer.UpdateBlendTextures(state.textureStack);
                }
            } else if (walkMode) {
                // Zell-genauer Stempel (Zelle = 6.25 Einheiten, Bit gesetzt = blockiert) - siehe
                // WalkGrid::kCellSize; die Weltposition entspricht direkt der Zellposition.
                std::uint32_t changed[4] = {0, 0, 0, 0};
                core::WalkUndoPatch patch = core::ApplyWalkBitStamp(
                    state.walkGrid, state.walkSettings.radius, worldX, worldZ, state.walkBlockMode, changed);
                if (!patch.entries.empty()) {
                    state.walkUndo.Push(std::move(patch));
                    if (!state.walkPreviewDirty) UpdateWalkPreviewRect(state, changed[0], changed[1], changed[2], changed[3]);
                }
            }
        }
    }

    // Zoom-Knoepfe oben rechts im Viewport (Mausrad geht ebenfalls) - nach ALLEN Klick-Auswertungen,
    // damit "zuletzt gezeichnetes Element" fuer die Hover-Logik oben nicht verfaelscht wird.
    {
        const ImVec2 after = ImGui::GetCursorScreenPos();
        const ImVec2 btn(28.0f, 26.0f);
        ImGui::SetCursorScreenPos(ImVec2(viewMin.x + viewSize.x - btn.x - 10.0f, viewMin.y + 10.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(28, 34, 42, 230));
        if (UI::Button("+##zoom2dIn", btn)) state.view2dZoom = std::clamp(zoom * 1.5f, 1.0f, 40.0f);
        ImGui::SetCursorScreenPos(ImVec2(viewMin.x + viewSize.x - btn.x - 10.0f, viewMin.y + 10.0f + btn.y + 4.0f));
        if (UI::Button("-##zoom2dOut", btn)) state.view2dZoom = std::clamp(zoom / 1.5f, 1.0f, 40.0f);
        ImGui::SetCursorScreenPos(ImVec2(viewMin.x + viewSize.x - btn.x - 10.0f, viewMin.y + 10.0f + (btn.y + 4.0f) * 2.0f));
        if (UI::Button("1:1##zoom2dFit", btn)) { state.view2dZoom = 1.0f; state.view2dCenterU = state.view2dCenterV = 0.5f; }
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(after);
        char zoomText[48];
        std::snprintf(zoomText, sizeof(zoomText), "Zoom %.1fx  (Mausrad / Mitte ziehen)", zoom);
        ImGui::GetWindowDrawList()->AddText(ImVec2(viewMin.x + 10.0f, viewMin.y + viewSize.y - 20.0f), IM_COL32(220, 226, 235, 220), zoomText);
    }
}

// Zeichnet im 3D-View Namen und Blickpfeile der NPCs (Projektion per ImGui) - damit sich NPCs
// richtig ausrichten lassen (CHANGELOG [0.44.32]).
static void DrawNpcOverlay3D(EditorState& state, const ImVec2& imagePos, int w, int h) {
    if (!state.showNpcModels || !state.showNpcLabels || state.editMode != EditMode::Npcs || !state.npcTextLoaded || w <= 0 || h <= 0) return;
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return;
    const app::Mat4 vp = app::OrbitCamera::PerspectiveMatrix(0.9f, static_cast<float>(w) / static_cast<float>(h), state.camera.NearPlane(), state.camera.FarPlane()) * state.camera.ViewMatrix();
    auto project = [&](float x, float y, float z, ImVec2& out) {
        const float cx = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
        const float cy = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
        const float cw = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
        if (cw <= 0.01f) return false; // hinter der Kamera
        out = ImVec2(imagePos.x + (cx / cw * 0.5f + 0.5f) * static_cast<float>(w), imagePos.y + (1.0f - (cy / cw * 0.5f + 0.5f)) * static_cast<float>(h));
        return true;
    };
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos, ImVec2(imagePos.x + static_cast<float>(w), imagePos.y + static_cast<float>(h)), true);
    for (const std::size_t idx : NpcRecordsForCurrentMap(state)) {
        const auto& rec = table->records[idx];
        if (rec.values.size() < 5) continue;
        const float x = static_cast<float>(std::atoi(rec.values[2].c_str()));
        const float z = static_cast<float>(std::atoi(rec.values[3].c_str()));
        const float y = state.heightmap.SampleWorld(x, z);
        const bool selected = state.selectedNpcRecordIdx == static_cast<int>(idx);
        const float yaw = NpcYawRadians(state, std::atoi(rec.values[4].c_str()));
        const float fx = -std::sin(yaw), fz = -std::cos(yaw);
        ImVec2 base, tip, head;
        if (!project(x, y + 25.0f, z, base) || !project(x + fx * 70.0f, y + 25.0f, z + fz * 70.0f, tip) || !project(x, y + 75.0f, z, head)) continue;
        const ImU32 col = selected ? IM_COL32(255, 210, 90, 255) : IM_COL32(120, 200, 255, 220);
        dl->AddLine(base, tip, col, selected ? 3.0f : 2.0f);
        dl->AddCircleFilled(tip, selected ? 5.0f : 3.5f, col);
        dl->AddCircleFilled(base, 3.0f, col);
        const std::string label = rec.values[0];
        const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        dl->AddRectFilled(ImVec2(head.x - ts.x * 0.5f - 3.0f, head.y - ts.y - 3.0f), ImVec2(head.x + ts.x * 0.5f + 3.0f, head.y + 1.0f), IM_COL32(12, 15, 20, 180), 3.0f);
        dl->AddText(ImVec2(head.x - ts.x * 0.5f, head.y - ts.y - 1.0f), selected ? IM_COL32(255, 235, 170, 255) : IM_COL32(215, 225, 240, 240), label.c_str());
    }
    dl->PopClipRect();
}



bool InvertEditMat4(const app::Mat4& m, app::Mat4& out) {
    const float* a=m.m; float inv[16];
    inv[0]=a[5]*a[10]*a[15]-a[5]*a[11]*a[14]-a[9]*a[6]*a[15]+a[9]*a[7]*a[14]+a[13]*a[6]*a[11]-a[13]*a[7]*a[10];
    inv[4]=-a[4]*a[10]*a[15]+a[4]*a[11]*a[14]+a[8]*a[6]*a[15]-a[8]*a[7]*a[14]-a[12]*a[6]*a[11]+a[12]*a[7]*a[10];
    inv[8]=a[4]*a[9]*a[15]-a[4]*a[11]*a[13]-a[8]*a[5]*a[15]+a[8]*a[7]*a[13]+a[12]*a[5]*a[11]-a[12]*a[7]*a[9];
    inv[12]=-a[4]*a[9]*a[14]+a[4]*a[10]*a[13]+a[8]*a[5]*a[14]-a[8]*a[6]*a[13]-a[12]*a[5]*a[10]+a[12]*a[6]*a[9];
    inv[1]=-a[1]*a[10]*a[15]+a[1]*a[11]*a[14]+a[9]*a[2]*a[15]-a[9]*a[3]*a[14]-a[13]*a[2]*a[11]+a[13]*a[3]*a[10];
    inv[5]=a[0]*a[10]*a[15]-a[0]*a[11]*a[14]-a[8]*a[2]*a[15]+a[8]*a[3]*a[14]+a[12]*a[2]*a[11]-a[12]*a[3]*a[10];
    inv[9]=-a[0]*a[9]*a[15]+a[0]*a[11]*a[13]+a[8]*a[1]*a[15]-a[8]*a[3]*a[13]-a[12]*a[1]*a[11]+a[12]*a[3]*a[9];
    inv[13]=a[0]*a[9]*a[14]-a[0]*a[10]*a[13]-a[8]*a[1]*a[14]+a[8]*a[2]*a[13]+a[12]*a[1]*a[10]-a[12]*a[2]*a[9];
    inv[2]=a[1]*a[6]*a[15]-a[1]*a[7]*a[14]-a[5]*a[2]*a[15]+a[5]*a[3]*a[14]+a[13]*a[2]*a[7]-a[13]*a[3]*a[6];
    inv[6]=-a[0]*a[6]*a[15]+a[0]*a[7]*a[14]+a[4]*a[2]*a[15]-a[4]*a[3]*a[14]-a[12]*a[2]*a[7]+a[12]*a[3]*a[6];
    inv[10]=a[0]*a[5]*a[15]-a[0]*a[7]*a[13]-a[4]*a[1]*a[15]+a[4]*a[3]*a[13]+a[12]*a[1]*a[7]-a[12]*a[3]*a[5];
    inv[14]=-a[0]*a[5]*a[14]+a[0]*a[6]*a[13]+a[4]*a[1]*a[14]-a[4]*a[2]*a[13]-a[12]*a[1]*a[6]+a[12]*a[2]*a[5];
    inv[3]=-a[1]*a[6]*a[11]+a[1]*a[7]*a[10]+a[5]*a[2]*a[11]-a[5]*a[3]*a[10]-a[9]*a[2]*a[7]+a[9]*a[3]*a[6];
    inv[7]=a[0]*a[6]*a[11]-a[0]*a[7]*a[10]-a[4]*a[2]*a[11]+a[4]*a[3]*a[10]+a[8]*a[2]*a[7]-a[8]*a[3]*a[6];
    inv[11]=-a[0]*a[5]*a[11]+a[0]*a[7]*a[9]+a[4]*a[1]*a[11]-a[4]*a[3]*a[9]-a[8]*a[1]*a[7]+a[8]*a[3]*a[5];
    inv[15]=a[0]*a[5]*a[10]-a[0]*a[6]*a[9]-a[4]*a[1]*a[10]+a[4]*a[2]*a[9]+a[8]*a[1]*a[6]-a[8]*a[2]*a[5];
    float det=a[0]*inv[0]+a[1]*inv[4]+a[2]*inv[8]+a[3]*inv[12];
    if (std::abs(det)<1.0e-8f) return false;
    det=1.0f/det; for(int i=0;i<16;++i) out.m[i]=inv[i]*det; return true;
}

std::optional<EditVec3> Unproject3D(const EditorState& state, const ImVec2& imagePos,
                                    int w, int h, const ImVec2& mouse, float ndcZ) {
    const app::Mat4 view=state.camera.ViewMatrix();
    const app::Mat4 proj=app::OrbitCamera::PerspectiveMatrix(
        0.9f,static_cast<float>(std::max(1,w))/static_cast<float>(std::max(1,h)),
        state.camera.NearPlane(),state.camera.FarPlane());
    app::Mat4 inv{};
    if(!InvertEditMat4(proj*view,inv)) return std::nullopt;
    const float x=((mouse.x-imagePos.x)/static_cast<float>(std::max(1,w)))*2.0f-1.0f;
    const float y=1.0f-((mouse.y-imagePos.y)/static_cast<float>(std::max(1,h)))*2.0f;
    const float cx=x,cy=y,cz=ndcZ,cw=1.0f;
    const float ox=inv.m[0]*cx+inv.m[4]*cy+inv.m[8]*cz+inv.m[12]*cw;
    const float oy=inv.m[1]*cx+inv.m[5]*cy+inv.m[9]*cz+inv.m[13]*cw;
    const float oz=inv.m[2]*cx+inv.m[6]*cy+inv.m[10]*cz+inv.m[14]*cw;
    const float ow=inv.m[3]*cx+inv.m[7]*cy+inv.m[11]*cz+inv.m[15]*cw;
    if(std::abs(ow)<1.0e-8f) return std::nullopt;
    return EditVec3{ox/ow,oy/ow,oz/ow};
}

std::optional<EditVec3> PickTerrainFrom3D(EditorState& state, const ImVec2& imagePos,
                                          int w, int h, const ImVec2& mouse) {
    const auto nearP=Unproject3D(state,imagePos,w,h,mouse,-1.0f);
    const auto farP=Unproject3D(state,imagePos,w,h,mouse,1.0f);
    if(!nearP||!farP) return std::nullopt;
    EditVec3 dir{farP->x-nearP->x,farP->y-nearP->y,farP->z-nearP->z};
    const float len=std::sqrt(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);
    if(len<1.0e-6f||std::abs(dir.y)<1.0e-6f) return std::nullopt;
    dir.x/=len;dir.y/=len;dir.z/=len;

    float planeY=state.camera.TargetY();
    float t=(planeY-nearP->y)/dir.y;
    if(t<0.0f) t=state.camera.Distance();
    const float spanX=static_cast<float>(state.heightmap.Width()>1?state.heightmap.Width()-1:1)*state.heightmap.BlockWidth();
    const float spanZ=static_cast<float>(state.heightmap.Height()>1?state.heightmap.Height()-1:1)*state.heightmap.BlockHeight();

    EditVec3 p{};
    for(int i=0;i<7;++i) {
        p={nearP->x+dir.x*t,nearP->y+dir.y*t,nearP->z+dir.z*t};
        if(p.x<0.0f||p.z<0.0f||p.x>spanX||p.z>spanZ) return std::nullopt;
        const float terrainY=state.heightmap.SampleWorld(p.x,p.z);
        t=(terrainY-nearP->y)/dir.y;
        if(t<0.0f) return std::nullopt;
    }
    p={nearP->x+dir.x*t,nearP->y+dir.y*t,nearP->z+dir.z*t};
    p.y=state.heightmap.SampleWorld(p.x,p.z);
    return p;
}

int PlaceObjectAtWorld(EditorState& state, const std::string& modelPath, float x, float y, float z) {
    if(modelPath.empty()) return kNoObjectSelection;
    core::PlacedObject obj;
    obj.modelPath=modelPath; obj.posX=x; obj.posY=y; obj.posZ=z; obj.scale=state.newObjectScale;
    const float half=state.newObjectRotDeg*3.14159265f/180.0f*0.5f;
    obj.rotY=std::sin(half); obj.rotW=std::cos(half);
    const int id=static_cast<int>(state.placementSet.AddObject(std::move(obj)));
    SyncObjectEditorMetadata(state);
    state.selectedObjects={id}; state.selectedObject=id; state.objectPlaceMode=0;
    state.objectGizmoMatrixValid=false;
    ReloadObjectRenderers(state);
    return id;
}


bool ProjectWorldTo3DView(const EditorState& state, const ImVec2& imagePos, int w, int h,
                          const EditVec3& world, ImVec2& screen) {
    const app::Mat4 vp=app::OrbitCamera::PerspectiveMatrix(
        0.9f,static_cast<float>(std::max(1,w))/static_cast<float>(std::max(1,h)),
        state.camera.NearPlane(),state.camera.FarPlane())*state.camera.ViewMatrix();
    const float cx=vp.m[0]*world.x+vp.m[4]*world.y+vp.m[8]*world.z+vp.m[12];
    const float cy=vp.m[1]*world.x+vp.m[5]*world.y+vp.m[9]*world.z+vp.m[13];
    const float cw=vp.m[3]*world.x+vp.m[7]*world.y+vp.m[11]*world.z+vp.m[15];
    if(cw<=1.0e-5f) return false;
    const float nx=cx/cw, ny=cy/cw;
    screen=ImVec2(imagePos.x+(nx*0.5f+0.5f)*w,
                  imagePos.y+(1.0f-(ny*0.5f+0.5f))*h);
    return true;
}

float ActiveWorldBrushRadius(const EditorState& state) {
    if(state.editMode==EditMode::Heightmap) return state.brush.radius;
    if(state.editMode==EditMode::TexturePaint) return state.paintSettings.radius;
    if(state.editMode==EditMode::BlockWalk) return state.walkSettings.radius;
    return 0.0f;
}

ImU32 ActiveBrushColor(const EditorState& state, int alpha) {
    if(state.editMode==EditMode::BlockWalk)
        return state.walkBlockMode?IM_COL32(245,85,85,alpha):IM_COL32(80,220,145,alpha);
    if(state.editMode==EditMode::TexturePaint) return IM_COL32(90,190,255,alpha);
    return IM_COL32(75,205,255,alpha);
}

void Draw3DBrushOverlay(EditorState& state, const ImVec2& imagePos, int w, int h, bool hovered) {
    if(!hovered || (state.editMode!=EditMode::Heightmap &&
                    state.editMode!=EditMode::TexturePaint &&
                    state.editMode!=EditMode::BlockWalk)) return;
    const auto hit=PickTerrainFrom3D(state,imagePos,w,h,ImGui::GetMousePos());
    if(!hit) return;
    const float radius=ActiveWorldBrushRadius(state);
    if(radius<=0.0f) return;
    ImDrawList* dl=ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos,ImVec2(imagePos.x+w,imagePos.y+h),true);
    auto ring=[&](float r,ImU32 col,float thickness) {
        std::array<ImVec2,49> pts{};
        int count=0;
        for(int i=0;i<=48;++i) {
            const float a=static_cast<float>(i)*(2.0f*3.14159265f/48.0f);
            const float x=hit->x+std::cos(a)*r;
            const float z=hit->z+std::sin(a)*r;
            const float y=state.heightmap.SampleWorld(x,z)+3.0f;
            ImVec2 p;
            if(ProjectWorldTo3DView(state,imagePos,w,h,{x,y,z},p)) pts[count++]=p;
        }
        if(count>=3) dl->AddPolyline(pts.data(),count,col,ImDrawFlags_None,thickness);
    };
    ring(radius,ActiveBrushColor(state),2.0f);
    if(state.editMode!=EditMode::BlockWalk)
        ring(radius*0.5f,ActiveBrushColor(state,120),1.0f);
    ImVec2 center;
    if(ProjectWorldTo3DView(state,imagePos,w,h,*hit,center))
        dl->AddCircleFilled(center,3.0f,ActiveBrushColor(state));
    dl->PopClipRect();
}

void DrawMobZones3D(EditorState& state, const ImVec2& imagePos, int w, int h) {
    if(state.editMode!=EditMode::Mobs || !state.mobRegenTextLoaded) return;
    auto* zoneTable=state.mobRegenTextFile.FindTable("MobRegenGroup");
    if(!zoneTable) return;

    ImDrawList* dl=ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos,ImVec2(imagePos.x+w,imagePos.y+h),true);
    for(std::size_t i=0;i<zoneTable->records.size();++i) {
        const auto& rec=zoneTable->records[i];
        if(rec.values.size()<7) continue;
        const float cx=static_cast<float>(std::atoi(rec.values[2].c_str()));
        const float cz=static_cast<float>(std::atoi(rec.values[3].c_str()));
        const float zw=static_cast<float>(std::atoi(rec.values[4].c_str()));
        const float zh=static_cast<float>(std::atoi(rec.values[5].c_str()));
        const float rangeVal=static_cast<float>(std::atoi(rec.values[6].c_str()));
        const float radius=(zw>0.0f||zh>0.0f)?std::max(zw,zh)*0.5f:rangeVal;
        if(radius<=0.0f) continue;

        const bool selected=state.selectedMobZoneIdx==static_cast<int>(i);
        const ImU32 col=selected?IM_COL32(255,255,255,235):IM_COL32(72,150,235,155);
        std::array<ImVec2,49> pts{};
        int count=0;
        for(int k=0;k<=48;++k) {
            const float a=static_cast<float>(k)*(2.0f*3.14159265f/48.0f);
            const float x=cx+std::cos(a)*radius;
            const float z=cz+std::sin(a)*radius;
            const float y=state.heightmap.SampleWorld(x,z)+5.0f;
            ImVec2 p;
            if(ProjectWorldTo3DView(state,imagePos,w,h,{x,y,z},p)) pts[count++]=p;
        }
        if(count>=3) dl->AddPolyline(pts.data(),count,col,ImDrawFlags_None,selected?2.5f:1.25f);

        ImVec2 center;
        if(ProjectWorldTo3DView(state,imagePos,w,h,{cx,state.heightmap.SampleWorld(cx,cz)+8.0f,cz},center)) {
            dl->AddCircleFilled(center,selected?5.0f:3.0f,col);
            if(selected && !rec.values.empty())
                dl->AddText(ImVec2(center.x+8.0f,center.y-8.0f),col,rec.values[0].c_str());
        }
    }
    dl->PopClipRect();
}

void DrawPortals3D(EditorState& state, const ImVec2& imagePos, int w, int h) {
    if(state.editMode!=EditMode::Portals || state.legacySaveStem[0]=='\0') return;
    const auto markers=CollectPortalMarkers(state);
    if(markers.empty()) return;
    ImDrawList* dl=ImGui::GetWindowDrawList();
    dl->PushClipRect(imagePos,ImVec2(imagePos.x+w,imagePos.y+h),true);
    for(const auto& m:markers) {
        ImVec2 p;
        const float worldY=state.heightmap.SampleWorld(m.x,m.y)+12.0f;
        if(!ProjectWorldTo3DView(state,imagePos,w,h,{m.x,worldY,m.y},p)) continue;
        const bool selected=m.kind==state.selectedPortalKind && static_cast<int>(m.idx)==state.selectedPortalIdx;
        const ImU32 col=selected?IM_COL32(255,255,255,245):IM_COL32(80,185,255,210);
        const float r=selected?8.0f:6.0f;
        if(m.kind==kPortalKindTown) {
            const ImVec2 d[4]={ImVec2(p.x,p.y-r),ImVec2(p.x+r,p.y),ImVec2(p.x,p.y+r),ImVec2(p.x-r,p.y)};
            dl->AddPolyline(d,4,col,ImDrawFlags_Closed,selected?2.5f:1.7f);
        } else {
            const ImVec2 t[3]={ImVec2(p.x,p.y-r),ImVec2(p.x+r,p.y+r),ImVec2(p.x-r,p.y+r)};
            dl->AddPolyline(t,3,col,ImDrawFlags_Closed,selected?2.5f:1.7f);
        }
        if(selected) dl->AddText(ImVec2(p.x+r+5.0f,p.y-r),col,m.label.c_str());
    }
    dl->PopClipRect();
}


std::string CurrentGizmoSelectionKey(const EditorState& state) {
    std::string key = std::to_string(state.objectGizmoOperation) + "|" +
                      (state.objectGizmoLocal ? "L|" : "W|");
    for (const int id : state.selectedObjects) key += std::to_string(id) + ",";
    return key;
}

bool DrawObjectTransformGizmo(EditorState& state, const ImVec2& imageScreenPos, int w, int h) {
    if (state.editMode != EditMode::ObjectPlacement || state.selectedObjects.empty()) {
        state.objectGizmoMatrixValid = false;
        state.objectGizmoWasUsing = false;
        return false;
    }

    const ObjectSelectionPivot pivot = ComputeObjectSelectionPivot(state);
    if (!pivot.valid) return false;

    const std::string selectionKey = CurrentGizmoSelectionKey(state);
    if (!state.objectGizmoMatrixValid || state.objectGizmoSelectionKey != selectionKey ||
        !state.objectGizmoWasUsing) {
        const app::Mat4 start = ObjectEditMatrix(pivot.position, pivot.rotation, pivot.scale);
        std::copy(std::begin(start.m), std::end(start.m), state.objectGizmoMatrix.begin());
        state.objectGizmoMatrixValid = true;
        state.objectGizmoSelectionKey = selectionKey;
    }

    app::Mat4 before{};
    std::copy(state.objectGizmoMatrix.begin(), state.objectGizmoMatrix.end(), std::begin(before.m));

    const app::Mat4 view = state.camera.ViewMatrix();
    const app::Mat4 projection = app::OrbitCamera::PerspectiveMatrix(
        0.9f, static_cast<float>(std::max(1,w)) / static_cast<float>(std::max(1,h)),
        state.camera.NearPlane(), state.camera.FarPlane());

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(imageScreenPos.x, imageScreenPos.y, static_cast<float>(w), static_cast<float>(h));
    ImGuizmo::SetID(0x4E47);

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    if (state.objectGizmoOperation == 1) operation = ImGuizmo::ROTATE;
    else if (state.objectGizmoOperation == 2) operation = ImGuizmo::SCALE;

    const ImGuizmo::MODE mode = state.objectGizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    float snap3[3] = {state.objectMoveSnap, state.objectMoveSnap, state.objectMoveSnap};
    float snap1 = state.objectGizmoOperation == 1 ? state.objectRotateSnap : state.objectScaleSnap;
    const float* snap = nullptr;
    if (state.objectGizmoSnap) snap = state.objectGizmoOperation == 0 ? snap3 : &snap1;

    ImGuizmo::Manipulate(view.m, projection.m, operation, mode,
                         state.objectGizmoMatrix.data(), nullptr, snap);

    const bool usingGizmo = ImGuizmo::IsUsing();
    const bool overGizmo = ImGuizmo::IsOver();

    if (usingGizmo) {
        app::Mat4 after{};
        std::copy(state.objectGizmoMatrix.begin(), state.objectGizmoMatrix.end(), std::begin(after.m));
        const EditVec3 oldPivot{before.m[12], before.m[13], before.m[14]};

        if (state.objectGizmoOperation == 0) {
            const float dx = after.m[12] - before.m[12];
            const float dy = after.m[13] - before.m[13];
            const float dz = after.m[14] - before.m[14];
            if (std::abs(dx)+std::abs(dy)+std::abs(dz) > 1.0e-6f)
                MoveSelectedObjectsBy(state, dx, dy, dz);
        } else if (state.objectGizmoOperation == 1) {
            const EditQuat beforeQ = MatrixRotationQuat(before);
            const EditQuat afterQ = MatrixRotationQuat(after);
            const EditQuat delta = MulEditQuat(afterQ, ConjugateEditQuat(beforeQ));
            RotateSelectedObjectsAroundPivot(state, oldPivot, delta);
        } else {
            const float beforeScale = MatrixUniformScale(before);
            const float afterScale = MatrixUniformScale(after);
            const float factor = beforeScale > 1.0e-6f ? afterScale / beforeScale : 1.0f;
            if (std::isfinite(factor) && std::abs(factor - 1.0f) > 1.0e-6f)
                ScaleSelectedObjectsAroundPivot(state, oldPivot, factor);
        }
    }

    if (state.objectGizmoWasUsing && !usingGizmo) state.objectGizmoMatrixValid = false;
    state.objectGizmoWasUsing = usingGizmo;
    return usingGizmo || overGizmo;
}

void DrawObjectGizmoToolbar(EditorState& state, const ImVec2& imageScreenPos) {
    if (state.editMode != EditMode::ObjectPlacement) return;
    const ImVec2 restore = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(imageScreenPos.x + 10.0f, imageScreenPos.y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(11, 27, 41, 235));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(18, 67, 104, 245));

    auto opButton = [&](const char* label, int op) {
        const bool active = state.objectGizmoOperation == op;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(12, 100, 165, 245));
        if (UI::SmallButton(label)) {
            state.objectGizmoOperation = op;
            state.objectGizmoMatrixValid = false;
        }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    opButton("Move",0);
    opButton("Rotate",1);
    opButton("Scale",2);

    if (UI::SmallButton(state.objectGizmoLocal ? "Local" : "World")) {
        state.objectGizmoLocal = !state.objectGizmoLocal;
        state.objectGizmoMatrixValid = false;
    }
    ImGui::SameLine();
    UI::Checkbox("Snap##gizmoOverlay", &state.objectGizmoSnap);
    ImGui::SameLine();
    if (UI::SmallButton("Fokus")) FocusSelectedObjects(state);
    ImGui::SameLine();
    if (UI::SmallButton("Boden")) GroundSelectedObjects(state);

    ImGui::PopStyleColor(2);
    ImGui::SetCursorScreenPos(restore);
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
    RefreshObjectVisibility(state);
    state.renderer.SetTerrainVisible(state.showTerrain);
    for (std::size_t li = 0; li < state.textureStack.LayerCount(); ++li) {
        state.renderer.SetLayerVisible(static_cast<int>(li), li >= state.layerHidden.size() || state.layerHidden[li] == 0);
    }
    const std::unordered_set<int> selectedPlacementIds(state.selectedObjects.begin(), state.selectedObjects.end());
    state.objectMarkerRenderer.RebuildInstances(state.placementSet, state.selectedObjects,
        [&state, &selectedPlacementIds](std::size_t i) {
            const bool selected = selectedPlacementIds.contains(static_cast<int>(i));
            return IsObjectHidden(state, i) ||
                   (!state.showObjectMarkers && !selected) ||
                   (state.nifMeshRenderer.HasRealMesh(i) && !selected);
        });
    state.objectMarkerRenderer.Draw(state.camera, w, h);
    if (state.editMode == EditMode::Portals) {
        // Portal-Ziele als grosse Marker (wie die Objekt-Platzhalter, aber 3-4x so gross): TownPortal
        // etwas groesser als Schriftrollen-Ziele; das gewaehlte Ziel wird weiss. Jeden Frame neu
        // aufgebaut (nur wenige Eintraege).
        EnsurePortalDataLoaded(state);
        core::ObjectPlacementSet portalSet;
        int selectedIdx = -1;
        for (const auto& m : CollectPortalMarkers(state)) {
            core::PlacedObject marker;
            marker.posX = m.x;
            marker.posZ = m.y;
            marker.posY = state.heightmap.SampleWorld(m.x, m.y) + 150.0f;
            marker.scale = m.kind == kPortalKindTown ? 4.0f : 3.0f;
            const std::size_t at = portalSet.AddObject(std::move(marker));
            if (m.kind == state.selectedPortalKind && static_cast<int>(m.idx) == state.selectedPortalIdx) selectedIdx = static_cast<int>(at);
        }
        state.portalMarkerRenderer.RebuildInstances(portalSet, selectedIdx);
        state.portalMarkerRenderer.Draw(state.camera, w, h);
    }
    if (state.showObjectMeshes) state.nifMeshRenderer.Draw(state.placementSet, state.camera, w, h, &state.objectHidden);
    RefreshShmdCategoryVisibility(state);
    if (state.showObjectMeshes && state.shmdCategoryRenderSet.Count() > 0) {
        state.shmdCategoryMeshRenderer.Draw(state.shmdCategoryRenderSet, state.camera, w, h, &state.shmdCategoryHidden);
    }
    EnsureNpcModelsLoaded(state);
    if (state.showNpcModels) state.npcMeshRenderer.Draw(state.npcRenderSet, state.camera, w, h);
    const GLuint tex = state.renderer.EndScene();
    if (tex != 0) {
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), ImVec2(static_cast<float>(w), static_cast<float>(h)),
                     ImVec2(0, 1), ImVec2(1, 0)); // FBO-Textur ist vertikal gespiegelt -> UVs tauschen
    }
    const bool viewImageHovered = ImGui::IsItemHovered();
    if (state.editMode == EditMode::ObjectPlacement && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_NIF_ASSET")) {
            const char* model=static_cast<const char*>(payload->Data);
            if (const auto hit=PickTerrainFrom3D(state,imageScreenPos,w,h,ImGui::GetMousePos())) {
                PlaceObjectAtWorld(state,model,hit->x,hit->y,hit->z);
                state.statusMessage=std::string("Objekt im 3D-Viewport platziert: ")+model;
            }
        }
        ImGui::EndDragDropTarget();
    }
    Draw3DBrushOverlay(state,imageScreenPos,w,h,viewImageHovered);
    DrawMobZones3D(state,imageScreenPos,w,h);
    DrawPortals3D(state,imageScreenPos,w,h);
    const bool gizmoCapturing = DrawObjectTransformGizmo(state, imageScreenPos, w, h);
    DrawObjectGizmoToolbar(state, imageScreenPos);
    DrawNpcOverlay3D(state, imageScreenPos, w, h);

    // Direktes 3D-Picking: nächster projizierter Objektursprung unter dem Mauszeiger.
    if (state.editMode==EditMode::ObjectPlacement && viewImageHovered && !gizmoCapturing &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 mouse=ImGui::GetMousePos();
        float best=18.0f;
        int bestId=kNoObjectSelection;
        RefreshObjectVisibility(state);
        for(std::size_t i=0;i<state.placementSet.Count();++i) {
            if(IsObjectHidden(state,i)||IsObjectEditorLocked(state,static_cast<int>(i))) continue;
            const auto& obj=state.placementSet.At(i);
            ImVec2 p;
            if(!ProjectWorldTo3DView(state,imageScreenPos,w,h,{obj.posX,obj.posY,obj.posZ},p)) continue;
            const float dx=p.x-mouse.x,dy=p.y-mouse.y;
            const float d=std::sqrt(dx*dx+dy*dy);
            if(d<best){best=d;bestId=static_cast<int>(i);}
        }
        RefreshShmdCategoryVisibility(state);
        for(std::size_t i=0;i<state.shmdCategoryRenderSet.Count();++i) {
            if(i<state.shmdCategoryHidden.size()&&state.shmdCategoryHidden[i]) continue;
            const auto poly=ObjectFootprintWorldPolygon(state,state.shmdCategoryRenderSet.At(i));
            if(poly.empty()) continue;
            float x=0.0f,z=0.0f;
            for(const auto& q:poly){x+=q.first;z+=q.second;}
            x/=static_cast<float>(poly.size());z/=static_cast<float>(poly.size());
            ImVec2 p;
            if(!ProjectWorldTo3DView(state,imageScreenPos,w,h,{x,state.heightmap.SampleWorld(x,z),z},p)) continue;
            const float dx=p.x-mouse.x,dy=p.y-mouse.y;
            const float d=std::sqrt(dx*dx+dy*dy);
            if(d<best){best=d;bestId=ShmdSelectionId(i);}
        }
        if(bestId!=kNoObjectSelection) {
            state.objectPlaceMode=0;
            SelectObjectOnCanvas(state,bestId,ImGui::GetIO().KeyCtrl);
            state.objectGizmoMatrixValid=false;
        } else if(!ImGui::GetIO().KeyCtrl) {
            ClearObjectSelection(state);
        }
    }

    // ---- Kamera-Steuerung (CHANGELOG [0.44.32]): Ego-Kamera wie in einem Level-Editor
    //  rechte Maustaste halten + Maus = umsehen | W/A/S/D = laufen | Q/E = runter/hoch | Shift = schnell,
    //  Strg = langsam | Mausrad = Zoom (multiplikativ, bis ganz nah) | mittlere Taste = schieben |
    //  linke Maustaste ziehen = um das Ziel kreisen (wie bisher).
    {
        ImGuiIO& io = ImGui::GetIO();
        const bool hovered3d = viewImageHovered;
        if (hovered3d && !gizmoCapturing && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) state.cameraLooking = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) state.cameraLooking = false;
        if (state.cameraLooking) {
            const ImVec2 delta = io.MouseDelta;
            // Maus nach rechts = nach rechts drehen (Yaw sinkt), Maus nach oben = nach oben schauen (Pitch sinkt).
            state.camera.LookBy(-delta.x * 0.0045f, delta.y * 0.0045f);
        }
        if (hovered3d && !gizmoCapturing) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const ImVec2 delta = io.MouseDelta;
                state.camera.OrbitBy(delta.x * 0.01f, -delta.y * 0.01f);
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                const ImVec2 delta = io.MouseDelta;
                const float panScale = std::max(state.camera.Distance(), 30.0f) * 0.0015f;
                state.camera.PanBy(-delta.x * panScale, delta.y * panScale);
            }
            if (io.MouseWheel != 0.0f) state.camera.ZoomSteps(io.MouseWheel);
        }
        if ((hovered3d || state.cameraLooking) && !gizmoCapturing && !io.WantTextInput) {
            float fwd = 0.0f, right = 0.0f, up = 0.0f;
            if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) fwd += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) fwd -= 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) right += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) right -= 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) up += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) up -= 1.0f;
            if (fwd != 0.0f || right != 0.0f || up != 0.0f) {
                // Tempo waechst mit der Entfernung (weit weg = grosse Spruenge, nah dran = feinfuehlig).
                float speed = std::clamp(state.camera.Distance() * 1.0f, 150.0f, 6000.0f);
                if (io.KeyShift) speed *= 4.0f;
                if (io.KeyCtrl) speed *= 0.2f;
                const float step = speed * std::min(io.DeltaTime, 0.1f);
                state.camera.MoveLocal(fwd * step, right * step, up * step);
            }
        }
        // Kurzhilfe unten links im Bild
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const char* hint = "Rechte Maustaste + Maus: umsehen   WASD: laufen   Q/E: runter/hoch   Shift: schnell   Mausrad: Zoom   Mitte: schieben   Links ziehen: kreisen";
        const ImVec2 hp(imageScreenPos.x + 10.0f, imageScreenPos.y + avail.y - 22.0f);
        dl->AddRectFilled(ImVec2(hp.x - 6.0f, hp.y - 3.0f), ImVec2(hp.x + ImGui::CalcTextSize(hint).x + 6.0f, hp.y + 17.0f), IM_COL32(12, 15, 20, 170), 4.0f);
        dl->AddText(hp, IM_COL32(190, 200, 215, 230), hint);
    }

    // Zoom +/- Knöpfe unten rechts über dem 3D-Bild (siehe Mockup) - zusätzlich zum
    // Mausrad, für Nutzer ohne Maus mit Rad bzw. als deutlicher sichtbarer Zugriffspunkt.
    const ImVec2 zoomBtnSize(26.0f, 26.0f);
    const ImVec2 zoomPos(imageScreenPos.x + avail.x - zoomBtnSize.x - 10.0f, imageScreenPos.y + avail.y - zoomBtnSize.y * 2.0f - 16.0f);
    ImGui::SetCursorScreenPos(zoomPos);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(28, 34, 42, 230));
    if (UI::Button("+##zoomIn", zoomBtnSize)) state.camera.ZoomSteps(2.0f);
    ImGui::SetCursorScreenPos(ImVec2(zoomPos.x, zoomPos.y + zoomBtnSize.y + 4.0f));
    if (UI::Button("-##zoomOut", zoomBtnSize)) state.camera.ZoomSteps(-2.0f);
    ImGui::PopStyleColor();
}

// Tab-Leiste des Arbeitsbereichs (Hightmap/Texturing/Block-Walk/Objects/NPCs/NPC AI/Mobs/
// Mob AI + Zurück, siehe Mockup) - steuert denselben EditMode, der früher über Radio-Buttons
// im Werkzeuge-Panel gewählt wurde.
void DrawWorkspaceTabBar(EditorState& state) {
    auto setMode = [&](EditMode mode) {
        if (state.editMode != mode) {
            state.editMode = mode;
            state.portalPickMode = false;
            state.layerPreviewDirty = true;
            state.walkPreviewDirty = true;
        }
        if (mode == EditMode::TexturePaint) ImGui::SetWindowFocus("Layer##layerManager");
        if (mode == EditMode::ObjectPlacement || mode == EditMode::Npcs ||
            mode == EditMode::Mobs || mode == EditMode::Portals)
            ImGui::SetWindowFocus("Szene##sceneOutliner");
    };
    auto undoAvailable = [&]() {
        switch (state.editMode) {
            case EditMode::Heightmap: return state.undo.CanUndo();
            case EditMode::TexturePaint: return state.textureUndo.CanUndo();
            case EditMode::BlockWalk: return state.walkUndo.CanUndo();
            default: return false;
        }
    };
    auto redoAvailable = [&]() {
        switch (state.editMode) {
            case EditMode::Heightmap: return state.undo.CanRedo();
            case EditMode::TexturePaint: return state.textureUndo.CanRedo();
            case EditMode::BlockWalk: return state.walkUndo.CanRedo();
            default: return false;
        }
    };
    auto doUndo = [&]() {
        switch (state.editMode) {
            case EditMode::Heightmap:
                if (state.undo.Undo(state.heightmap)) state.meshDirty = true;
                break;
            case EditMode::TexturePaint:
                if (state.textureUndo.Undo(state.textureStack)) {
                    state.layerPreviewDirty = true;
                    state.renderer.UpdateBlendTextures(state.textureStack);
                }
                break;
            case EditMode::BlockWalk:
                if (state.walkUndo.Undo(state.walkGrid)) state.walkPreviewDirty = true;
                break;
            default: break;
        }
    };
    auto doRedo = [&]() {
        switch (state.editMode) {
            case EditMode::Heightmap:
                if (state.undo.Redo(state.heightmap)) state.meshDirty = true;
                break;
            case EditMode::TexturePaint:
                if (state.textureUndo.Redo(state.textureStack)) {
                    state.layerPreviewDirty = true;
                    state.renderer.UpdateBlendTextures(state.textureStack);
                }
                break;
            case EditMode::BlockWalk:
                if (state.walkUndo.Redo(state.walkGrid)) state.walkPreviewDirty = true;
                break;
            default: break;
        }
    };

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 5.0f));
    ImGui::BeginGroup();

    const bool canSave = state.hasLegacyIniMeta || state.legacySaveDir[0] != '\0';
    if (DrawIconButton("cmd.save", "Speichern", DrawIconSave, false, ImVec2(78,58), canSave)) {
        auto project = BuildProjectFromState(state);
        auto result = core::legacy::SaveLegacyMap(project, state.legacySaveDir, state.legacySaveStem);
        if (result) {
            state.legacyIniMeta = project.ini;
            state.statusMessage = std::string(T("workspace.savedas")) + state.legacySaveDir;
        } else {
            state.statusMessage = "Fehler: " + result.error();
        }
    }
    ImGui::SameLine();
    if (DrawIconButton("cmd.undo", "Undo", DrawIconUndo, false, ImVec2(68,58), undoAvailable())) doUndo();
    ImGui::SameLine();
    if (DrawIconButton("cmd.redo", "Redo", DrawIconRedo, false, ImVec2(68,58), redoAvailable())) doRedo();

    ImGui::SameLine(); ImGui::Dummy(ImVec2(8.0f, 1.0f)); ImGui::SameLine();

    struct Tool { const char* id; const char* label; EditMode mode; IconDrawFn icon; };
    const Tool tools[] = {
        {"terrain","Terrain",EditMode::Heightmap,DrawIconTerrain},
        {"texture","Textur",EditMode::TexturePaint,DrawIconBrush},
        {"walk","Block & Walk",EditMode::BlockWalk,DrawIconGrid},
        {"objects","Objekte",EditMode::ObjectPlacement,DrawIconCube},
        {"npcs","NPCs",EditMode::Npcs,DrawIconPerson},
        {"mobs","Mobs",EditMode::Mobs,DrawIconSpawn},
        {"portals","Portale",EditMode::Portals,DrawIconPortal},
    };
    for (const auto& tool : tools) {
        if (DrawIconButton(tool.id, tool.label, tool.icon, state.editMode == tool.mode))
            setMode(tool.mode);
        ImGui::SameLine();
    }

    ImGui::Dummy(ImVec2(8.0f, 1.0f)); ImGui::SameLine();
    if (DrawIconButton("cmd.data","Spieldaten",DrawIconTable,false,ImVec2(86,58))) {
        state.screen = AppScreen::ShnEditor;
    }
    ImGui::SameLine();
    if (DrawIconButton("cmd.kfm","KFM",DrawIconClapper,false,ImVec2(70,58))) {
        state.screen = AppScreen::KfmBrowser;
    }
    ImGui::SameLine();
    if (DrawIconButton("cmd.back","Zurück",DrawIconUndo,false,ImVec2(70,58))) {
        state.screen = AppScreen::MapEditorLauncher;
    }

    ImGui::EndGroup();
    ImGui::PopStyleVar();
}




void DrawSceneOutlinerPanel(EditorState& state) {
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "SZENE");
    ImGui::SameLine();

    const char* context = "Objekte";
    if (state.editMode == EditMode::Npcs) context = "NPCs";
    else if (state.editMode == EditMode::Mobs) context = "Mob-Zonen";
    else if (state.editMode == EditMode::Portals) context = "Portale";
    ImGui::TextDisabled("%s", context);
    ImGui::Separator();

    UI::InputTextWithHint("##objectOutlinerFilter", "Szene filtern...",
                          state.objectOutlinerFilter, sizeof(state.objectOutlinerFilter));
    const std::string needle = LowerAscii(state.objectOutlinerFilter);

    if (state.editMode == EditMode::Npcs) {
        EnsureNpcTextLoaded(state);
        if (!state.npcTextLoaded || state.legacySaveStem[0] == '\0') {
            ImGui::TextDisabled("Keine NPC-Daten für die aktuelle Karte verfügbar.");
            return;
        }
        auto* table = state.npcTextFile.FindTable("ShineNPC");
        if (!table) { ImGui::TextDisabled("ShineNPC-Tabelle fehlt."); return; }
        const auto indices = NpcRecordsForCurrentMap(state);
        ImGui::TextDisabled("%zu NPCs", indices.size());
        ImGui::BeginChild("##sceneNpcList", ImVec2(0,0), true);
        for (std::size_t idx : indices) {
            auto& rec = table->records[idx];
            if (rec.values.size() < 8) continue;
            const std::string label = rec.values[0] + "  ·  " + rec.values[6];
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            if (UI::Selectable((label + "##npcScene" + std::to_string(idx)).c_str(),
                               state.selectedNpcRecordIdx == static_cast<int>(idx))) {
                state.selectedNpcRecordIdx = static_cast<int>(idx);
            }
        }
        ImGui::EndChild();
        return;
    }

    if (state.editMode == EditMode::Mobs) {
        EnsureMobRegenLoaded(state);
        if (!state.mobRegenTextLoaded || state.legacySaveStem[0] == '\0') {
            ImGui::TextDisabled("Keine MobRegen-Daten für die aktuelle Karte verfügbar.");
            return;
        }
        auto* zones = state.mobRegenTextFile.FindTable("MobRegenGroup");
        if (!zones) { ImGui::TextDisabled("MobRegenGroup-Tabelle fehlt."); return; }
        ImGui::TextDisabled("%zu Spawn-Zonen", zones->records.size());
        ImGui::BeginChild("##sceneMobZoneList", ImVec2(0,0), true);
        for (std::size_t i = 0; i < zones->records.size(); ++i) {
            auto& rec = zones->records[i];
            if (rec.values.empty()) continue;
            const std::string label = rec.values[0];
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            if (UI::Selectable((label + "##mobScene" + std::to_string(i)).c_str(),
                               state.selectedMobZoneIdx == static_cast<int>(i))) {
                state.selectedMobZoneIdx = static_cast<int>(i);
            }
        }
        ImGui::EndChild();
        return;
    }

    if (state.editMode == EditMode::Portals) {
        EnsurePortalDataLoaded(state);
        const auto markers = CollectPortalMarkers(state);
        ImGui::TextDisabled("%zu Ziele", markers.size());
        ImGui::BeginChild("##scenePortalList", ImVec2(0,0), true);
        for (std::size_t i = 0; i < markers.size(); ++i) {
            const auto& m = markers[i];
            std::string label = m.kind == kPortalKindTown ? m.label : ("Schriftrolle · " + m.label);
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            const bool selected = m.kind == state.selectedPortalKind && static_cast<int>(m.idx) == state.selectedPortalIdx;
            if (UI::Selectable((label + "##portalScene" + std::to_string(i)).c_str(), selected)) {
                state.selectedPortalKind = m.kind;
                state.selectedPortalIdx = static_cast<int>(m.idx);
            }
        }
        ImGui::EndChild();
        return;
    }

    const std::size_t shmdSceneCount = state.shmdCategoryRenderSet.Count();
    const std::size_t total = state.placementSet.Count() + shmdSceneCount;
    ImGui::TextDisabled("%zu Objekte", total);

    if (!ImGui::GetIO().WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        SelectAllNormalObjects(state);
    }

    if (UI::SmallButton("Alle normalen")) SelectAllNormalObjects(state);
    ImGui::SameLine();
    if (UI::SmallButton("Auswahl aufheben")) ClearObjectSelection(state);
    ImGui::SameLine();
    ImGui::TextDisabled("%zu gewählt", state.selectedObjects.size());

    if (!state.selectedObjects.empty()) {
        if (DrawTinyIconButton("outlinerDuplicate",DrawIconDuplicate,false,"Duplizieren (Strg+D)"))
            DuplicateSelectedObjects(state);
        ImGui::SameLine();
        if (DrawTinyIconButton("outlinerFocus",DrawIconCube,false,"Auswahl fokussieren (F)"))
            FocusSelectedObjects(state);
        ImGui::SameLine();
        if (DrawTinyIconButton("outlinerDelete",DrawIconDelete,false,"Auswahl löschen (Entf)"))
            DeleteSelectedObjects(state);
        ImGui::SameLine();
        ImGui::TextDisabled("Strg+C/V · F · Ende=Boden");
    }

    SyncObjectEditorMetadata(state);
    std::vector<int> visibleIds;
    std::vector<std::string> labels;
    visibleIds.reserve(total);
    labels.reserve(total);

    auto appendIfMatch = [&](int id, std::string label) {
        if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) return;
        visibleIds.push_back(id);
        labels.push_back(std::move(label));
    };

    for (std::size_t i = 0; i < shmdSceneCount; ++i) {
        const int id = ShmdSelectionId(i);
        const std::string category = ShmdSelectionCategoryName(state, id);
        appendIfMatch(id, "[" + (category.empty() ? std::string("SHMD") : category) + "] " +
                          state.shmdCategoryRenderSet.At(i).modelPath);
    }
    for (std::size_t i = 0; i < state.placementSet.Count(); ++i)
        appendIfMatch(static_cast<int>(i), "[Placement] " + state.placementSet.At(i).modelPath);

    ImGui::BeginChild("##sceneObjectList", ImVec2(0,0), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visibleIds.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const int id = visibleIds[static_cast<std::size_t>(row)];
            const bool selected = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), id) != state.selectedObjects.end();
            ImGui::PushID(id);

            const bool hidden=IsObjectEditorHidden(state,id);
            const bool locked=IsObjectEditorLocked(state,id);
            if (DrawTinyIconButton("eye",DrawIconEye,!hidden,hidden?"Einblenden":"Ausblenden")) {
                if(id>=0) {
                    state.objectEditorHidden[static_cast<std::size_t>(id)]=hidden?0:1;
                    state.objectVisKey.clear();
                } else {
                    const std::string key=ShmdEditorObjectKey(state,id);
                    if(!key.empty()) {
                        if(hidden) state.shmdEditorHiddenKeys.erase(key);
                        else state.shmdEditorHiddenKeys.insert(key);
                    }
                }
            }
            ImGui::SameLine(0,2);
            if (DrawTinyIconButton("lock",DrawIconLock,locked,locked?"Entsperren":"Sperren")) {
                if(id>=0) state.objectEditorLocked[static_cast<std::size_t>(id)]=locked?0:1;
                else {
                    const std::string key=ShmdEditorObjectKey(state,id);
                    if(!key.empty()) {
                        if(locked) state.shmdEditorLockedKeys.erase(key);
                        else state.shmdEditorLockedKeys.insert(key);
                    }
                }
                if(!locked) state.objectDragActive=false;
            }
            ImGui::SameLine(0,5);
            if(id<0) {
                const int kind=ShmdRenderIndex(id) && *ShmdRenderIndex(id)<state.shmdCategoryRenderKind.size()
                    ? state.shmdCategoryRenderKind[*ShmdRenderIndex(id)] : -1;
                const char* badge=kind==0?"SKY":kind==1?"WATER":kind==2?"GROUND":"SHMD";
                ImGui::TextColored(ImVec4(0.30f,0.78f,0.95f,1.0f),"%s",badge);
                ImGui::SameLine(0,5);
            }
            ImGui::BeginDisabled(locked);
            const bool clicked=UI::Selectable(labels[static_cast<std::size_t>(row)].c_str(),selected,
                                               ImGuiSelectableFlags_AllowDoubleClick);
            ImGui::EndDisabled();
            if (clicked) {
                state.editMode=EditMode::ObjectPlacement;
                state.objectPlaceMode=0;
                SelectObjectFromList(state,id,row,visibleIds,ImGui::GetIO().KeyCtrl,ImGui::GetIO().KeyShift);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) FocusSelectedObjects(state);
            }

            if (ImGui::BeginPopupContextItem("##objectContext")) {
                if (!selected) SelectObjectFromList(state,id,row,visibleIds,false,false);
                ImGui::TextDisabled("%s",labels[static_cast<std::size_t>(row)].c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Fokussieren","F")) FocusSelectedObjects(state);
                if (ImGui::MenuItem("Auf Terrain setzen","Ende",false,id<0 || !IsObjectEditorLocked(state,id)))
                    GroundSelectedObjects(state);
                if (ImGui::MenuItem("Kopieren","Strg+C")) CopySelectedObjects(state);
                if (ImGui::MenuItem("Duplizieren","Strg+D")) DuplicateSelectedObjects(state);
                {
                    const bool menuHidden=IsObjectEditorHidden(state,id);
                    const bool menuLocked=IsObjectEditorLocked(state,id);
                    if (ImGui::MenuItem(menuHidden?"Einblenden":"Ausblenden")) {
                        if(id>=0) {
                            state.objectEditorHidden[static_cast<std::size_t>(id)]=menuHidden?0:1;
                            state.objectVisKey.clear();
                        } else {
                            const std::string key=ShmdEditorObjectKey(state,id);
                            if(menuHidden) state.shmdEditorHiddenKeys.erase(key);
                            else if(!key.empty()) state.shmdEditorHiddenKeys.insert(key);
                        }
                    }
                    if (ImGui::MenuItem(menuLocked?"Entsperren":"Sperren")) {
                        if(id>=0) state.objectEditorLocked[static_cast<std::size_t>(id)]=menuLocked?0:1;
                        else {
                            const std::string key=ShmdEditorObjectKey(state,id);
                            if(menuLocked) state.shmdEditorLockedKeys.erase(key);
                            else if(!key.empty()) state.shmdEditorLockedKeys.insert(key);
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Löschen","Entf",false,id<0 || !IsObjectEditorLocked(state,id)))
                    DeleteSelectedObjects(state);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void DrawLayerManagerPanel(EditorState& state) {
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "LAYER");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %d", state.textureStack.LayerCount(), app::HeightmapRenderer::kMaxTextureLayers);
    ImGui::Separator();

    if (state.textureAssetRoot.empty()) {
        if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
            if (const auto texRoot = FindNamedSubfolder(*resmapRoot, {"fieldtexture"})) state.textureAssetRoot = *texRoot;
        }
    }

    ImGui::BeginChild("##layerManagerList", ImVec2(0, 220), true);
    for (std::size_t i = 0; i < state.textureStack.LayerCount(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const bool selected = state.selectedLayer == static_cast<int>(i);
        auto& layer = state.textureStack.Layer(i);
        if (!layer.diffuseFileName.empty() && !state.textureAssetRoot.empty()) {
            const auto thumb = GetOrLoadAssetThumbnail(state, state.textureAssetRoot / layer.diffuseFileName, false);
            if (thumb.tex) ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(thumb.tex)), ImVec2(42,42));
            else ImGui::Dummy(ImVec2(42,42));
        } else {
            ImGui::Dummy(ImVec2(42,42));
        }
        ImGui::SameLine();
        const std::string label = layer.name.empty() ? ("Layer " + std::to_string(i + 1)) : layer.name;
        if (state.layerRenameIndex == static_cast<int>(i)) {
            ImGui::SetNextItemWidth(-1.0f);
            const bool enter=ImGui::InputText("##layerRename",state.layerRenameBuffer,
                                              sizeof(state.layerRenameBuffer),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere(-1);
            if (enter || ImGui::IsItemDeactivatedAfterEdit()) {
                if (state.layerRenameBuffer[0] != '\0') layer.name=state.layerRenameBuffer;
                state.layerRenameIndex=-1;
            }
        } else if (UI::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0,42))) {
            state.selectedLayer = static_cast<int>(i);
            state.editMode = EditMode::TexturePaint;
            state.layerPreviewDirty = true;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                state.layerRenameIndex=static_cast<int>(i);
                std::snprintf(state.layerRenameBuffer,sizeof(state.layerRenameBuffer),"%s",label.c_str());
            }
        }

        if (ImGui::BeginDragDropSource()) {
            const int sourceIndex=static_cast<int>(i);
            ImGui::SetDragDropPayload("NEXTGEN_LAYER_INDEX",&sourceIndex,sizeof(sourceIndex));
            ImGui::Text("Layer verschieben");
            ImGui::TextDisabled("%s",label.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_DDS_ASSET")) {
                const char* rel=static_cast<const char*>(payload->Data);
                layer.diffuseFileName=rel;
                state.renderer.LoadTerrainTextures(state.textureStack,
                    !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
                state.layerPreviewDirty=true;
                state.statusMessage="Layer-Textur ersetzt: "+std::string(rel);
            }
            if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_LAYER_INDEX")) {
                const int from=*static_cast<const int*>(payload->Data);
                const int to=static_cast<int>(i);
                if(from>=0 && from<static_cast<int>(state.textureStack.LayerCount()) && from!=to) {
                    state.textureStack.MoveLayer(static_cast<std::size_t>(from),static_cast<std::size_t>(to));
                    if (state.selectedLayer==from) state.selectedLayer=to;
                    else if (from<state.selectedLayer && state.selectedLayer<=to) --state.selectedLayer;
                    else if (to<=state.selectedLayer && state.selectedLayer<from) ++state.selectedLayer;
                    state.layerHidden.assign(state.textureStack.LayerCount(),0);
                    state.renderer.LoadTerrainTextures(state.textureStack,
                        !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
                    state.layerPreviewDirty=true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("##layerContext")) {
            if (ImGui::MenuItem("Duplizieren")) {
                const auto copy=layer;
                const std::size_t ni=state.textureStack.AddLayer(copy.name+" Kopie",copy.diffuseFileName,copy.uvScaleDiffuse);
                state.textureStack.Layer(ni)=copy;
                state.textureStack.Layer(ni).name=copy.name+" Kopie";
                state.selectedLayer=static_cast<int>(ni);
                state.layerHidden.resize(state.textureStack.LayerCount(),0);
                state.renderer.LoadTerrainTextures(state.textureStack,
                    !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
                state.layerPreviewDirty=true;
            }
            if (ImGui::MenuItem("Entfernen",nullptr,false,state.textureStack.LayerCount()>1)) {
                state.textureStack.RemoveLayer(i);
                state.selectedLayer=state.textureStack.LayerCount()==0?-1:
                    std::min(state.selectedLayer,static_cast<int>(state.textureStack.LayerCount())-1);
                state.layerHidden.resize(state.textureStack.LayerCount(),0);
                state.renderer.LoadTerrainTextures(state.textureStack,
                    !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
                state.layerPreviewDirty=true;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::InvisibleButton("##ddsNewLayerDrop",ImVec2(-1,34));
    const ImVec2 dropMin=ImGui::GetItemRectMin(), dropMax=ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRect(dropMin,dropMax,IM_COL32(45,110,155,180),4.0f,0,1.2f);
    const char* dropText="DDS hier ablegen = neuer Layer";
    const ImVec2 ts=ImGui::CalcTextSize(dropText);
    ImGui::GetWindowDrawList()->AddText(ImVec2((dropMin.x+dropMax.x-ts.x)*0.5f,(dropMin.y+dropMax.y-ts.y)*0.5f),
                                        IM_COL32(150,200,230,230),dropText);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload=ImGui::AcceptDragDropPayload("NEXTGEN_DDS_ASSET")) {
            const char* rel=static_cast<const char*>(payload->Data);
            std::filesystem::path rp(rel);
            const std::size_t ni=state.textureStack.AddLayer(rp.stem().string(),rel,1.0f);
            state.selectedLayer=static_cast<int>(ni);
            state.layerHidden.resize(state.textureStack.LayerCount(),0);
            state.renderer.LoadTerrainTextures(state.textureStack,
                !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
            state.layerPreviewDirty=true;
            state.statusMessage="Neuer Layer aus DDS: "+std::string(rel);
        }
        ImGui::EndDragDropTarget();
    }

    if (state.selectedLayer >= 0 && static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount()) {
        const auto& layer = state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer));
        ImGui::TextDisabled("Ausgewählt");
        ImGui::TextWrapped("%s", layer.name.c_str());
        ImGui::TextDisabled("Diffuse: %s", layer.diffuseFileName.c_str());
        ImGui::TextDisabled("UV Scale: %.3f", layer.uvScaleDiffuse);
    }

    ImGui::Separator();
    ImGui::TextDisabled("Neuer Layer");
    UI::InputText("Name##layerDock", state.newLayerName, sizeof(state.newLayerName));
    UI::InputText("Diffuse##layerDock", state.newLayerDiffuse, sizeof(state.newLayerDiffuse));
    ImGui::TextDisabled("Textur kann auch unten im Asset Browser gewählt werden.");
    UI::InputFloat("UV-Scale##layerDock", &state.newLayerUvScale);

    ImGui::BeginDisabled(state.textureStack.LayerCount() >= static_cast<std::size_t>(app::HeightmapRenderer::kMaxTextureLayers));
    if (UI::Button("Layer hinzufügen##layerDock", ImVec2(-1,0))) {
        if (state.textureStack.Width() == 0)
            state.textureStack = core::TextureLayerStack(1024u, 1024u);
        const auto newIndex = state.textureStack.AddLayer(state.newLayerName, state.newLayerDiffuse, state.newLayerUvScale);
        state.selectedLayer = static_cast<int>(newIndex);
        state.editMode = EditMode::TexturePaint;
        state.layerPreviewDirty = true;
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(state.selectedLayer < 0);
    if (UI::Button("Layer duplizieren##layerDock", ImVec2(-1,0))) {
        const auto copy=state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer));
        const std::size_t ni=state.textureStack.AddLayer(copy.name+" Kopie",copy.diffuseFileName,copy.uvScaleDiffuse);
        state.textureStack.Layer(ni)=copy;
        state.textureStack.Layer(ni).name=copy.name+" Kopie";
        state.selectedLayer=static_cast<int>(ni);
        state.layerHidden.resize(state.textureStack.LayerCount(),0);
        state.renderer.LoadTerrainTextures(state.textureStack,
            !state.textureAssetRoot.empty()?state.textureAssetRoot:CurrentObjectAssetMapDir(state));
        state.layerPreviewDirty=true;
    }
    if (UI::Button("Ausgewählten Layer entfernen##layerDock", ImVec2(-1,0))) {
        state.textureStack.RemoveLayer(static_cast<std::size_t>(state.selectedLayer));
        if (state.textureStack.LayerCount() == 0) state.selectedLayer = -1;
        else state.selectedLayer = std::min(state.selectedLayer, static_cast<int>(state.textureStack.LayerCount()) - 1);
        state.layerPreviewDirty = true;
    }
    ImGui::EndDisabled();
}

void DrawWorkspaceAssetBrowser(EditorState& state) {
    const bool objectMode = state.editMode == EditMode::ObjectPlacement;
    const bool textureMode = state.editMode == EditMode::TexturePaint;

    if (objectMode && !state.nifListScanned) {
        if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
            state.availableNifFiles = ListFilesByExtension(*resmapRoot, {".nif"});
            state.nifAssetRoot = *resmapRoot;
            state.resmapRootForThumbnails = *resmapRoot;
            state.nifListScanned = true;
        }
    }
    if (textureMode && !state.textureListScanned) {
        if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
            if (const auto texRoot = FindNamedSubfolder(*resmapRoot, {"fieldtexture"})) {
                state.availableTextureFiles = ListFilesByExtension(*texRoot, {".dds"});
                state.textureAssetRoot = *texRoot;
                state.textureListScanned = true;
            }
        }
    }

    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "ASSET BROWSER");
    ImGui::SameLine();
    ImGui::TextDisabled(objectMode ? "NIF Modelle" : textureMode ? "Texturen" : "kontextsensitiv");
    ImGui::Separator();

    if (!objectMode && !textureMode) {
        ImGui::TextWrapped("Der Asset Browser wird bei 'Objekte' und 'Textur' aktiv. "
                           "Weitere Asset-Typen können später hier ergänzt werden.");
        return;
    }

    UI::InputTextWithHint("##workspaceAssetFilter", "Assets filtern...",
                          state.workspaceAssetFilter, sizeof(state.workspaceAssetFilter));

    const auto& files = objectMode ? state.availableNifFiles : state.availableTextureFiles;
    const auto& root = objectMode ? state.nifAssetRoot : state.textureAssetRoot;
    std::string needle = LowerAscii(state.workspaceAssetFilter);
    std::vector<std::size_t> matching;
    matching.reserve(files.size());
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (needle.empty() || LowerAscii(files[i]).find(needle) != std::string::npos) matching.push_back(i);
    }
    ImGui::TextDisabled("%zu / %zu Assets", matching.size(), files.size());

    ImGui::BeginChild("##workspaceAssetList", ImVec2(0,0), true);
    constexpr float thumbSize = 38.0f;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(matching.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const std::size_t index = matching[static_cast<std::size_t>(row)];
            const std::string& rel = files[index];
            ImGui::PushID(static_cast<int>(index));
            const auto thumb = GetOrLoadAssetThumbnail(
                state, root / rel, objectMode, objectMode ? state.resmapRootForThumbnails : std::filesystem::path{});
            if (thumb.tex) {
                ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(thumb.tex)), ImVec2(thumbSize,thumbSize));
            } else {
                ImGui::Dummy(ImVec2(thumbSize,thumbSize));
            }
            ImGui::SameLine();
            if (UI::Selectable(rel.c_str(), false, 0, ImVec2(0,thumbSize))) {
                if (objectMode) {
                    const std::string legacy = ToLegacyResmapModelPath(rel);
                    std::snprintf(state.newObjectModelPath, sizeof(state.newObjectModelPath), "%s", legacy.c_str());
                    state.statusMessage = "Objekt-Asset gewählt: " + legacy;
                } else {
                    std::snprintf(state.newLayerDiffuse, sizeof(state.newLayerDiffuse), "%s", rel.c_str());
                    state.statusMessage = "Textur-Asset gewählt: " + rel;
                }
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                if (objectMode) {
                    const std::string legacy=ToLegacyResmapModelPath(rel);
                    ImGui::SetDragDropPayload("NEXTGEN_NIF_ASSET",legacy.c_str(),legacy.size()+1);
                    ImGui::Text("NIF platzieren");
                    ImGui::TextDisabled("%s",legacy.c_str());
                } else {
                    ImGui::SetDragDropPayload("NEXTGEN_DDS_ASSET",rel.c_str(),rel.size()+1);
                    ImGui::Text("Textur zuweisen");
                    ImGui::TextDisabled("%s",rel.c_str());
                }
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

// Der eigentliche Arbeitsbereich (siehe Mockup, zweites/rechtes Bild): Tab-Leiste oben,
// darunter drei Spalten - "Datei"+"Tools/etc" links, "2D View" Mitte, "3D View" rechts.
void DrawMapEditorWorkspace(EditorState& state) {
    DrawTopNav(state, "Karte");
    DrawWorkspaceTabBar(state);
    ImGui::Separator();

    const ImGuiID dockspaceId = ImGui::GetID("##MapEditorDockspace");
    if (!state.mapEditorDockspaceBuilt) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetContentRegionAvail());

        ImGuiID center = dockspaceId;
        ImGuiID leftId = 0, inspectorId = 0, bottomId = 0;
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, &leftId, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, &inspectorId, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.34f, &bottomId, &center);

        ImGuiID navigatorId = 0, sceneId = leftId;
        ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Up, 0.34f, &navigatorId, &sceneId);

        ImGuiID assetId = 0, view2dId = bottomId;
        ImGui::DockBuilderSplitNode(bottomId, ImGuiDir_Left, 0.43f, &assetId, &view2dId);
        const ImGuiID view3dId = center;

        ImGui::DockBuilderDockWindow("Navigator##mapNavigator", navigatorId);
        ImGui::DockBuilderDockWindow("Layer##layerManager", sceneId);
        ImGui::DockBuilderDockWindow("Sichtbarkeit##visibilityPanel", sceneId);
        ImGui::DockBuilderDockWindow("Szene##sceneOutliner", sceneId);
        ImGui::DockBuilderDockWindow("Eigenschaften##fileToolsCol", inspectorId);
        ImGui::DockBuilderDockWindow("Asset Browser##assetBrowser", assetId);
        ImGui::DockBuilderDockWindow("2D-Ansicht##view2d", view2dId);
        ImGui::DockBuilderDockWindow("3D-Ansicht##view3d", view3dId);
        ImGui::DockBuilderFinish(dockspaceId);
        state.mapEditorDockspaceBuilt = true;
    }

    const float statusH = 28.0f;
    const float dockH = std::max(120.0f, ImGui::GetContentRegionAvail().y - statusH);
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, dockH), ImGuiDockNodeFlags_None);

    auto modeName = [&]() -> const char* {
        switch (state.editMode) {
            case EditMode::Heightmap: return "Terrain / Heightmap";
            case EditMode::TexturePaint: return "Textur-Layer";
            case EditMode::BlockWalk: return "Block & Walk";
            case EditMode::ObjectPlacement: return "Objekte";
            case EditMode::Npcs: return "NPCs";
            case EditMode::Mobs: return "Mobs / Spawn-Zonen";
            case EditMode::Portals: return "Portale";
            case EditMode::NpcAi: return "NPC AI";
            case EditMode::MobAi: return "Mob AI";
        }
        return "Werkzeug";
    };

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(7, 18, 28, 255));
    ImGui::Begin("Navigator##mapNavigator");
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "KARTE");
    ImGui::Separator();
    ImGui::Text("%s", state.legacySaveStem[0] ? state.legacySaveStem : "(keine Karte)");
    if (state.legacySaveDir[0]) ImGui::TextDisabled("%s", state.legacySaveDir);
    ImGui::Dummy(ImVec2(0,4));
    ImGui::TextDisabled("Größe");
    ImGui::Text("%u × %u", state.heightmap.Width(), state.heightmap.Height());
    ImGui::TextDisabled("Textur-Layer");
    ImGui::Text("%zu", state.textureStack.LayerCount());
    ImGui::TextDisabled("Objekte");
    ImGui::Text("%zu", state.placementSet.Count() + state.shmdCategoryRenderSet.Count());
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "AKTIVES WERKZEUG");
    ImGui::TextWrapped("%s", modeName());
    ImGui::Dummy(ImVec2(0,6));
    if (UI::Button("Karte wechseln", ImVec2(-1,0))) state.screen = AppScreen::MapEditorLauncher;
    if (UI::Button("Spieldaten öffnen", ImVec2(-1,0))) state.screen = AppScreen::ShnEditor;
    if (UI::Button("Animationen / KFM", ImVec2(-1,0))) state.screen = AppScreen::KfmBrowser;
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(7, 18, 28, 255));
    ImGui::Begin("Szene##sceneOutliner");
    DrawSceneOutlinerPanel(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(7, 18, 28, 255));
    ImGui::Begin("Layer##layerManager");
    DrawLayerManagerPanel(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(7, 18, 28, 255));
    ImGui::Begin("Sichtbarkeit##visibilityPanel");
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "SICHTBARKEIT");
    ImGui::Separator();
    DrawVisibilityPanel(state);
    ImGui::SeparatorText("Ansicht");
    UI::Checkbox(T("workspace.wireframe"), &state.wireframe);
    if (UI::Button(T("workspace.centercamera"), ImVec2(-1,0))) {
        const float spanX = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth();
        const float spanZ = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight();
        const auto [lo, hi] = state.heightmap.MinMax();
        state.camera.SetTarget(spanX * 0.5f, (lo + hi) * 0.5f, spanZ * 0.5f);
    }
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(8, 20, 31, 255));
    ImGui::Begin("Eigenschaften##fileToolsCol");
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "EIGENSCHAFTEN");
    ImGui::SameLine(); ImGui::TextDisabled("%s", modeName());
    ImGui::Separator();
    DrawToolsContent(state);

    if (UI::CollapsingHeader("Datei & Fiesta Import/Export")) {
        if (UI::Button(T("workspace.saveas"), ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("##saveAsPopup");
        if (ImGui::BeginPopup("##saveAsPopup")) {
            UI::InputText("Ordner", state.legacySaveDir, sizeof(state.legacySaveDir));
            UI::InputText("Name", state.legacySaveStem, sizeof(state.legacySaveStem));
            if (UI::Button(T("workspace.save"))) {
                auto project = BuildProjectFromState(state);
                auto result = core::legacy::SaveLegacyMap(project, state.legacySaveDir, state.legacySaveStem);
                state.statusMessage = result ? std::string(T("workspace.savedas")) + state.legacySaveDir
                                             : "Fehler: " + result.error();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        DrawAdvancedFileOps(state);
    }
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(7, 17, 27, 255));
    ImGui::Begin("3D-Ansicht##view3d");
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "3D ANSICHT");
    ImGui::SameLine(); ImGui::TextDisabled("%s", modeName());
    ImGui::Separator();
    DrawPreview3DContent(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(7, 17, 27, 255));
    ImGui::Begin("Asset Browser##assetBrowser");
    DrawWorkspaceAssetBrowser(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(7, 17, 27, 255));
    ImGui::Begin("2D-Ansicht##view2d");
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "2D DRAUFSICHT");
    ImGui::SameLine(); ImGui::TextDisabled("Nord oben");
    ImGui::Separator();
    DrawEditor2DContent(state);
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::TextDisabled("Map: %s  |  Werkzeug: %s  |  Auswahl: %zu  |  %.0f FPS",
                        state.legacySaveStem[0] ? state.legacySaveStem : "-",
                        modeName(), state.selectedObjects.size(), ImGui::GetIO().Framerate);
    if (state.selectedObject != kNoObjectSelection) {
        if (const auto* selected = EditableObject(state, state.selectedObject)) {
            ImGui::SameLine();
            ImGui::TextDisabled(" | XYZ %.1f / %.1f / %.1f", selected->posX, selected->posY, selected->posZ);
        }
    }
    if (!state.statusMessage.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled(" | ");
        ImGui::SameLine();
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

    GLFWwindow* window = glfwCreateWindow(1600, 900, "NextGen-Editor", nullptr, nullptr);
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
    state.portalMarkerRenderer.Init();
    state.nifMeshRenderer.Init();
    state.shmdCategoryMeshRenderer.Init();
    state.npcMeshRenderer.Init();
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

        SyncProjectRoots(state);
        switch (state.screen) {
            case AppScreen::ProjectHub: DrawProjectHub(state); break;
            case AppScreen::NewProjectConfig: DrawNewProjectConfig(state); break;
            case AppScreen::MapEditorLauncher: DrawMapEditorLauncher(state); break;
            case AppScreen::MapEditorWorkspace: DrawMapEditorWorkspace(state); break;
            case AppScreen::ShnEditor: DrawShnEditor(state); break;
            case AppScreen::KfmBrowser:
                DrawTopNav(state, "Animationen");
                ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(8, 20, 31, 255));
                ImGui::BeginChild("##kfmWorkspace", ImVec2(0,0), false);
                ImGui::TextColored(ImVec4(0.40f,0.72f,0.96f,1.0f), "Animationen / KFM");
                ImGui::SameLine();
                ImGui::TextDisabled("Katalog · Übergänge · Dateiverweise · Kopie-Export");
                ImGui::Separator();
#ifdef _WIN32
                state.kfmPanel.Draw([] { return BrowseForShnFileWindows("Fiesta KFM", true); });
#else
                state.kfmPanel.Draw({});
#endif
                ImGui::EndChild();
                ImGui::PopStyleColor();
                break;
            case AppScreen::ComingSoon: DrawComingSoon(state); break;
        }

        ImGui::End();
        ImGui::PopStyleVar();

        HandleGlobalShortcuts(state);
        DrawManualWindow(state);

        ImGui::Render();
        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.025f, 0.035f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    for (auto& [path, thumb] : state.assetThumbnails) {
        if (thumb.tex) glDeleteTextures(1, &thumb.tex);
    }
    state.assetThumbnails.clear();
    state.renderer.Shutdown();
    state.objectMarkerRenderer.Shutdown();
    state.portalMarkerRenderer.Shutdown();
    state.nifMeshRenderer.Shutdown();
    state.shmdCategoryMeshRenderer.Shutdown();
    state.npcMeshRenderer.Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
