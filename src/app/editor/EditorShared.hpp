#pragma once
// EditorShared.hpp
// Gemeinsame Grundlage aller Editor-Module (src/app/editor/*.cpp): Includes, Design-Tokens,
// EditorState und die Deklarationen aller Editor-Funktionen in der Reihenfolge ihrer Definition.
// Die Funktionen selbst stehen thematisch gruppiert in den Modulen; main.cpp enthält nur main().

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
#include "mapeditor/core/DataDependency.hpp"
#include "mapeditor/core/Manual.hpp"
#include "mapeditor/core/EditOps.hpp"
#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/HeightmapIO.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"
#include "mapeditor/core/ObjectPlacementIO.hpp"
#include "mapeditor/core/ProjectOutput.hpp"
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
#include "mapeditor/core/legacy/ItemDropGroups.hpp"
#include "mapeditor/core/LevelEditorTools.hpp"
#include "mapeditor/core/MapCheck.hpp"
#include "mapeditor/core/Autosave.hpp"
#include "mapeditor/core/WalkGenerate.hpp"
#include "mapeditor/core/HeightmapExchange.hpp"
#include "mapeditor/core/TextureRules.hpp"
#include "mapeditor/core/ObjectScatter.hpp"
#include "mapeditor/core/Prefab.hpp"
#include "mapeditor/core/Spline.hpp"
#include <random>
#include "mapeditor/core/legacy/NewMapFromTemplate.hpp"
#include "mapeditor/core/legacy/Minimap.hpp"
#include "mapeditor/core/legacy/TownPortalImage.hpp"
#include "mapeditor/core/legacy/MapRenderSettings.hpp"
#include "mapeditor/app/Localization.hpp"

#include "Camera.hpp"
#include "KfmPanel.hpp"
#include "ObjectMarkerRenderer.hpp"
#include "NifMeshRenderer.hpp"
#include "Renderer.hpp"
#include "UiIconAssets.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <future>
#include <chrono>
#include <ctime>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace theseed::mapeditor;
using theseed::mapeditor::app::T;

namespace nge {

// Gemeinsame DE/EN-Hilfe; Definition steht weiter unten bei den spezialisierten Editoren.
const char* L(const char* de, const char* en);
extern app::UiIconAssets gUiIcons;
// Verbindliche Design-Tokens aus docs/ui-vision/01_BRAND_AND_THEME.md.
// UI-Code soll semantisch auf diese Palette zurückgreifen statt lokale Blau-/Grautöne zu erfinden.
namespace UiTheme {
const ImVec4 Root          = ImVec4(0.031f, 0.071f, 0.114f, 1.0f); // #08121D
const ImVec4 Panel         = ImVec4(0.051f, 0.106f, 0.165f, 1.0f); // #0D1B2A
const ImVec4 PanelAlt      = ImVec4(0.063f, 0.137f, 0.220f, 1.0f); // #102338
const ImVec4 PanelRaised   = ImVec4(0.055f, 0.157f, 0.247f, 1.0f); // premium raised chrome
const ImVec4 PanelDeep     = ImVec4(0.020f, 0.055f, 0.094f, 1.0f); // deep shell/header
const ImVec4 Input         = ImVec4(0.039f, 0.086f, 0.137f, 1.0f); // #0A1623
const ImVec4 Border        = ImVec4(0.125f, 0.231f, 0.333f, 1.0f); // #203B55
const ImVec4 BorderStrong  = ImVec4(0.078f, 0.400f, 0.680f, 0.95f);
const ImVec4 GlowBlue      = ImVec4(0.078f, 0.710f, 1.000f, 0.32f);
const ImVec4 AccentBlue    = ImVec4(0.075f, 0.549f, 1.000f, 1.0f); // #138CFF
const ImVec4 AccentCyan    = ImVec4(0.125f, 0.867f, 0.949f, 1.0f); // #20DDF2
const ImVec4 AccentDeep    = ImVec4(0.043f, 0.373f, 0.843f, 1.0f); // #0B5FD7
const ImVec4 TextPrimary   = ImVec4(0.929f, 0.965f, 1.000f, 1.0f); // #EDF6FF
const ImVec4 TextSecondary = ImVec4(0.624f, 0.706f, 0.788f, 1.0f); // #9FB4C9
const ImVec4 Success       = ImVec4(0.153f, 0.788f, 0.471f, 1.0f); // #27C978
const ImVec4 Warning       = ImVec4(1.000f, 0.702f, 0.239f, 1.0f); // #FFB33D
const ImVec4 Error         = ImVec4(0.941f, 0.322f, 0.369f, 1.0f); // #F0525E
const ImVec4 Purple        = ImVec4(0.584f, 0.412f, 1.000f, 1.0f); // #9569FF
}
void ApplyEditorTheme();
#ifdef _WIN32
extern GLFWwindow* g_appWindowForDialogs;
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
template <class... A> bool InputScalar(const char* l, A&&... a) { const bool r = ImGui::InputScalar(l, std::forward<A>(a)...); Tip("InputScalar", l); return r; }
template <class... A> bool InputText(const char* l, A&&... a) { const bool r = ImGui::InputText(l, std::forward<A>(a)...); Tip("InputText", l); return r; }
template <class... A> bool InputTextWithHint(const char* l, A&&... a) { const bool r = ImGui::InputTextWithHint(l, std::forward<A>(a)...); Tip("InputText", l); return r; }
template <class... A> bool Combo(const char* l, A&&... a) { const bool r = ImGui::Combo(l, std::forward<A>(a)...); Tip("Combo", l); return r; }
template <class... A> bool CollapsingHeader(const char* l, A&&... a) { const bool r = ImGui::CollapsingHeader(l, std::forward<A>(a)...); Tip("CollapsingHeader", l); return r; }
template <class... A> bool TabItemButton(const char* l, A&&... a) { const bool r = ImGui::TabItemButton(l, std::forward<A>(a)...); Tip("TabItemButton", l); return r; }
template <class... A> bool Selectable(const char* l, A&&... a) { const bool r = ImGui::Selectable(l, std::forward<A>(a)...); Tip("Selectable", l); return r; }
template <class... A> bool MenuItem(const char* l, A&&... a) { const bool r = ImGui::MenuItem(l, std::forward<A>(a)...); Tip("MenuItem", l); return r; }
}
 // namespace UI

enum class EditMode { Heightmap, TexturePaint, BlockWalk, ObjectPlacement, Npcs, NpcAi, Mobs, MobAi, Portals };
// Oberste Navigationsebene der neuen, an den Mockups orientierten Oberfläche (siehe
// HANDOFF.md / MAP_FORMAT.md für den Kontext). ComingSoon deckt die vier Editor-Karten ab,
// die im Mockup als "Noch nicht entschieden" markiert sind bzw. noch nicht gebaut wurden
// (Quest/Interface/Drop Table/Skill+Action) sowie den SHN Editor.
enum class AppScreen { ProjectHub, NewProjectConfig, MapEditorLauncher, MapEditorWorkspace, ShnEditor, KfmBrowser, ComingSoon };
// Projekt-Ebene (NEU): getrennt von den Client-/Server-Ordnern, siehe die Erläuterung im
// Mockup ("Neues Projekt konfigurieren") - im Projekt-Ordner werden geänderte Dateien mit
// der korrekten Ordnerstruktur für Client/Server abgelegt, NIE direkt im Client- oder
// Server-Ordner. Client und Server sind ausschließlich read-only Quellen; alle schreibenden
// Workflows müssen über den zentralen Projekt-Output-Layer nach <Projekt>/Client bzw.
// <Projekt>/Server gehen. Die Client-Ordner-Angabe bleibt zusätzlich Suchwurzel für
// "Map Öffnen" und read-only Asset-Quellen.
struct ProjectConfig {
    char name[256] = "";
    char projectFolder[512] = "";
    char clientFolder[512] = "";
    char serverFolder[512] = "";
    bool hasProject = false;
};
std::expected<void, std::string> ValidateProjectConfigOutputRoots(const ProjectConfig& cfg);
std::expected<std::filesystem::path, std::string> ProjectPathForSource(
    const ProjectConfig& cfg,
    core::ProjectOutputSide side,
    const std::filesystem::path& sourcePath);
std::expected<std::filesystem::path, std::string> ProjectPathForRelative(
    const ProjectConfig& cfg,
    core::ProjectOutputSide side,
    const std::filesystem::path& relativePath);
bool PrepareProjectOutput(
    const std::expected<std::filesystem::path, std::string>& target,
    std::filesystem::path& out,
    std::string* errorOut = nullptr);
std::filesystem::path PreferProjectOverride(
    const ProjectConfig& cfg,
    core::ProjectOutputSide side,
    const std::filesystem::path& sourcePath);
std::expected<std::filesystem::path, std::string> ProjectClientRessystemPath(
    const ProjectConfig& cfg, const std::filesystem::path& relative);
std::expected<std::filesystem::path, std::string> ProjectServerShinePath(
    const ProjectConfig& cfg, const std::filesystem::path& relative);
std::expected<std::filesystem::path, std::string> SaveProjectShineOverride(
    core::legacy::ShineTextFile& file,
    const std::expected<std::filesystem::path, std::string>& target);
bool IsProjectSidePath(
    const ProjectConfig& cfg,
    core::ProjectOutputSide side,
    const std::filesystem::path& path);
bool HasParentTraversal(const std::filesystem::path& path);
std::expected<std::filesystem::path, std::string> ProjectClientOutputForRequestedPath(
    const ProjectConfig& cfg,
    const std::filesystem::path& requestedPath);
std::expected<std::filesystem::path, std::string> PrepareProjectClientRequestedOutput(
    const ProjectConfig& cfg,
    const std::filesystem::path& requestedPath);
bool SaveProjectConfig(const ProjectConfig& cfg, std::string* errorOut);
void TryLoadProjectConfig(ProjectConfig& cfg);
// Eine über den Ordner-Scan gefundene Karte (Name = Verzeichnisname, iniPath = Pfad zur .ini).
struct DiscoveredMap {
    std::string name;
    // Legacy maps point to <Map>/<Map>.ini. Mesh-only maps point directly to
    // <Map>/<Map>.nif and intentionally have no HTD/HTDG terrain files.
    std::string iniPath;
    bool standaloneNif = false;
    bool projectOnly = false;
};
enum class MapHistoryDomain {
    Heightmap,
    TexturePaint,
    TextureLayers,
    Walk,
    Objects,
};
struct ObjectEditSnapshot {
    core::ObjectPlacementSet placementSet;
    std::vector<int> selectedObjects;
    int selectedObject = -1;
    std::vector<char> editorHidden;
    std::vector<char> editorLocked;
    std::vector<std::string> editorLabels;
    std::vector<std::string> editorGroups;
    std::unordered_set<std::string> shmdHiddenKeys;
    std::unordered_set<std::string> shmdLockedKeys;
    std::unordered_map<std::string,std::string> shmdLabels;
    std::unordered_map<std::string,std::string> shmdGroups;
};
struct TextureLayerMetadata {
    std::string name;
    std::string diffuseFileName;
    float uvScaleDiffuse = 1.0f;
    float regionStartX = 0.0f;
    float regionStartY = 0.0f;
    float regionWidth = 0.0f;
    float regionHeight = 0.0f;
};
struct TextureLayerHistoryPatch {
    enum class Kind { Remove, Insert, Move, Metadata, ReplaceStack };
    Kind kind = Kind::Remove;
    std::size_t a = 0;
    std::size_t b = 0;
    std::optional<core::TextureLayer> layer;
    TextureLayerMetadata metadata;
    std::optional<core::TextureLayerStack> stack;
    int selectedLayer = -1;
};
struct EditorState {
    app::KfmPanel kfmPanel;
    core::Heightmap heightmap{257, 257, 50.0f, 50.0f};
    // Explicit capability flag: the editor has a historical preview grid by default, but
    // that grid is NOT evidence that the currently opened map owns HTD/HTDG terrain data.
    bool mapHasHeightmap = false;
    // Direct <Map>/<Map>.nif maps are renderable source scenes, but there is deliberately
    // no NIF writer in the editor yet. Persistent scene-geometry edits must stay read-only.
    bool standaloneNifMap = false;
    core::UndoStack undo;
    std::uint64_t mapEditRevision = 0; // zählt jede Kartenänderung über die Historie (Autosave)
    std::vector<MapHistoryDomain> mapHistoryUndo;
    std::vector<MapHistoryDomain> mapHistoryRedo;
    std::vector<ObjectEditSnapshot> objectUndo;
    std::vector<ObjectEditSnapshot> objectRedo;
    std::vector<TextureLayerHistoryPatch> textureLayerUndo;
    std::vector<TextureLayerHistoryPatch> textureLayerRedo;
    std::optional<ObjectEditSnapshot> pendingObjectEdit;
    bool pendingObjectEditChanged = false;
    core::BrushMode brushMode = core::BrushMode::Raise;
    bool flattenPickOnStroke = true; // Einebnen: Zielhöhe beim Strichbeginn unter dem Pinsel aufnehmen (wie Unreal)
    std::optional<std::array<float, 3>> rampStart; // Rampe: erster Klick (x, Höhe, z)
    core::BrushSettings brush;

    core::TextureLayerStack textureStack{1024, 1024}; // feinere Malauflösung; unabhängig von Heightmap
    core::TexturePaintUndoStack textureUndo;
    int selectedLayer = -1;
    int layerRenameIndex = -1;
    bool layerRenameFocusPending = false;
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
    bool walkFootprintPreviewActive = false;
    bool walkFootprintPreviewBlocked = true;
    // Sperren nur für Modelle, deren Grundflächen im beim Laden vorhandenen SHBD überwiegend
    // gesperrt sind (Gebäude ja, Gras/Schiffe nein). Modelle ohne Vergleich werden gesperrt.
    bool walkFootprintOnlyBlocking = true;
    core::WalkGrid walkGridAtLoad;
    // Stand beim Laden: Grundlage für "Block&Walk nur für Geändertes neu berechnen".
    core::Heightmap heightmapAtLoad;
    core::ObjectPlacementSet placementAtLoad;
    core::WalkGenerateSettings walkGenSettings;
    int walkGenScope = 0;      // 0 = nur Geändertes, 1 = ganze Karte
    bool walkGenObjects = true;
    std::optional<core::WalkGrid> walkProposal;
    core::WalkGridDiff walkProposalDiff, walkProposalVsOriginal;
    std::size_t walkProposalRegionCells = 0;
    std::string walkGenStatus;
    GLuint walkDiffTex = 0;
    bool walkDiffDirty = false;
    std::string modelBlockStatsKey;
    std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> modelBlockStats; // Modell -> (Zellen, davon gesperrt)
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
    bool objectLassoSelectActive = false;
    std::vector<ImVec2> objectLassoPoints;
    char selectedObjectModelPath[512] = "";
    int selectedObjectModelPathFor = -1;
    bool selectedObjectModelPathDirty = false;
    int objectListRangeAnchor = -1; // Shift-Bereichsauswahl in der Objektliste
    int objectPlaceMode = 1; // 1 = Platzieren, 0 = Auswählen, 2 = Malen (Foliage) (ImGui::RadioButton braucht int*)
    // Objekte malen (Foliage): Einstellungen, Zufall, laufender Strich (ein Undo-Schritt je Strich).
    core::ScatterSettings scatter;
    std::mt19937 scatterRng{20261016u};
    bool scatterErase = false;
    bool scatterStrokeActive = false;
    double scatterLastTime = 0.0;
    // Weg-Werkzeug (Terrain): Kontrollpunkte und Einstellungen.
    std::vector<core::SplinePoint> splinePoints;
    bool splineEdit = false;
    core::SplineWidth splineWidth;
    float splineSmooth = 300.0f;
    int splineLayer = -1;
    core::SplineObjectSettings splineObjects;
    char splineModel[512] = "";
    std::string splineStatus;
    // Prefabs (<Projekt>/Prefabs/*.prefab): gewähltes Prefab für Modus 3 (Einsetzen).
    char prefabName[64] = "";
    std::string prefabSelected;
    std::string prefabStatus;

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

    // ---- Unreal-aehnliche Viewport-Werkzeuge (core/LevelEditorTools.hpp, docs/LEVEL_EDITOR_PARITY.md) ----
    // Reiner Editor-Zustand: nichts davon wird in Fiesta-Dateien geschrieben. Persistiert in
    // <Einstellungen>/viewport.txt (Bedienung) bzw. bookmarks/<Karte>.txt (Kamera-Lesezeichen).
    bool viewportSettingsLoaded = false;
    bool viewportSettingsDirty = false;
    // Unreal-Navigation: WASD/Q/E nur bei gehaltener rechter Maustaste, Alt+LMB = Orbit,
    // Alt+RMB = Dolly, LMB-Ziehen = vor/zurueck + drehen, W/E/R = Gizmo, Ziffern = Lesezeichen.
    bool unrealNavigation = true;
    int cameraSpeedSetting = core::level::kDefaultCameraSpeedSetting;
    bool surfaceSnap = false;         // Verschieben setzt Objekte laufend auf das HTD-Terrain
    bool pivotAtActiveObject = false; // Gizmo-Pivot: false = Auswahlmitte, true = aktives (zuletzt gewähltes) Objekt
    bool actorSnap = false;           // Verschieben rastet am Pivot anderer Objekte ein (Radius = Raster-Snap)
    // Frei gesetzter Pivot (Alt+MMB-Klick, wie Unreal): gilt bis zur nächsten Auswahländerung.
    std::optional<std::array<float, 3>> pivotOverride;
    std::string pivotOverrideSelectionKey;
    // ---- Mehrere 3D-Viewports (Layout 1 / 2 / 4 wie in Unreal) ----
    // Der AKTIVE Viewport hält seine Werte direkt in den normalen Feldern (camera, viewportViewMode,
    // showTerrain, ...), damit Picking, Gizmo, Kürzel und Spieltest unverändert funktionieren. Die
    // übrigen Viewports liegen in viewportSlots und werden nur zum Zeichnen eingetauscht.
    struct ViewportSlot {
        app::OrbitCamera camera;
        int viewPreset = 0;
        bool cameraLooking = false;
        int viewMode = 0;
        bool wireframe = false;
        bool showTerrain = true, showObjectMeshes = true, showObjectMarkers = true, showNpcModels = true;
        bool showGrid = false, showStats = true, showWalkCollision = false, gameView = false;
        bool marqueeActive = false;
        ImVec2 marqueeStart{};
        std::optional<std::array<float, 3>> cursorWorld;
        ImVec2 pos{}, size{};
        bool initialized = false;
    };
    std::array<ViewportSlot, 4> viewportSlots{};
    std::array<ImVec2, 4> viewportRectPos{}, viewportRectSize{};
    int viewportLayout = 1;          // 1 = einzeln, 2 = nebeneinander, 4 = 2x2
    int activeViewport = 0;
    bool drawingActiveViewport = true; // false, während ein nicht aktiver Viewport gezeichnet wird
    bool gameView = false;            // G: alle Editor-Helfer im Viewport ausblenden
    bool showViewportStats = true;
    bool showViewportGrid = false;
    bool showViewportAxes = true;
    bool showWalkCollision3D = false; // SHBD-Zellen um den Mauszeiger im 3D-View
    int viewportViewPreset = 0;       // 0 Perspektive, 1 Oben, 2 Sueden, 3 Norden, 4 Westen, 5 Osten
    core::level::MarqueeMode marqueeMode = core::level::MarqueeMode::Crossing;
    bool marqueeActive = false;
    ImVec2 marqueeStart{};
    std::optional<std::array<float, 3>> viewportCursorWorld; // Terraintreffer unter der Maus
    ImVec2 viewport3dPos{};   // Bildschirmrechteck des 3D-Bilds im letzten Frame (Automatisierung)
    ImVec2 viewport3dSize{};
    core::level::CameraBookmarkSet cameraBookmarks;
    std::string cameraBookmarksForMap;
    // Ausgabeprotokoll (Output Log): alle Statusmeldungen mit Zeit und Schweregrad.
    core::level::EditorLog editorLog;
    std::string lastLoggedStatus;
    char outputLogFilter[128] = "";
    bool outputLogShowInfo = true;
    bool outputLogShowWarnings = true;
    bool outputLogShowErrors = true;
    bool outputLogAutoScroll = true;
    // Spieltest (Play-in-Editor): Spielfigur laeuft auf dem geladenen SHBD-Block&Walk-Gitter.
    bool playtestActive = false;
    core::level::PlaytestPawn playtestPawn;
    core::level::PlaytestConfig playtestConfig;
    bool playtestWalkGridUsable = false;
    std::string playtestSpawnSource;
    float playtestBlockedFlash = 0.0f;
    // ---- Karten-Renderdaten (Fiesta) und Darstellung ----
    // <Karte>.conf ([WorldSetting]/[GlowScreenEffect]) und die Vertex-Color-Bitmap aus der .ini.
    core::legacy::MapRenderConfig mapRenderConfig;
    std::string mapRenderConfigPath;
    std::string mapVertexColorPath;
    std::string mapRenderDataNote; // Hinweis, falls etwas fehlt/nicht lesbar ist
    int viewportViewMode = 0;      // 0 beleuchtet, 1 unbeleuchtet, 2 nur Licht, 3 Normalen, 4 Vertexfarbe, 5 UV0, 6 Alpha
    int viewportMsaaSamples = 4;   // 1 = aus
    bool viewportGlow = true;      // Glow aus <Karte>.conf
    bool viewportObjectAnimation = true; // im NIF eingebettete Knoten-Animationen (Windmühlen, Boote)
    bool viewportVertexColors = true;
    // SHMD-Szenenumgebung (GlobalLight, DirectionLight*, Fog, BackGroundColor, Frustum).
    bool sceneEnvironmentFromShmd = false;
    bool viewportMapLighting = true;   // Licht aus der SHMD statt Editor-Standardlicht
    bool viewportMapBackground = true; // BackGroundColor als Viewport-Hintergrund
    bool viewportFog = false;          // Nebel im Editor (im Spieltest immer aktiv)
    std::vector<core::PlacedObject> objectClipboard;
    std::vector<std::string> objectClipboardLabels;
    std::vector<std::string> objectClipboardGroups;
    std::vector<char> objectEditorHidden; // nur Editor-Sichtbarkeit, nicht SHMD-Export
    std::vector<char> objectEditorLocked; // nur Editor-Lock, nicht SHMD-Export
    std::vector<std::string> objectEditorLabels; // reine Editor-Metadaten, nicht SHMD-Export
    std::vector<std::string> objectEditorGroups; // flache Editor-Ordner/Gruppe, nicht SHMD-Export
    std::unordered_set<std::string> shmdEditorHiddenKeys;
    std::unordered_set<std::string> shmdEditorLockedKeys;
    std::unordered_map<std::string,std::string> shmdEditorLabels;
    std::unordered_map<std::string,std::string> shmdEditorGroups;
    char objectMetaLabelBuffer[128] = "";
    char objectMetaGroupBuffer[128] = "";
    char newObjectModelPath[512] = "resmap\\field\\Rou\\GuildHall.nif";
    float newObjectScale = 1.0f;
    float newObjectRotDeg = 0.0f;
    char legacyShmdPath[512] = "";
    char legacyIdmPath[512] = "";
    char legacyAidPath[512] = "";
    core::ObjectSpatialIndex legacySpatialIndex;
    std::int32_t legacyShmdSourceCount = -1; // Objektanzahl der geladenen SHMD (IDM-Remap)
    std::string legacySourceStem;                               // Original-Dateinamen der Kartenmodule,
    std::map<std::string, std::string> legacySourceFileNames;   // siehe LegacyMapProject::sourceFileNames
    bool hasLegacySpatialIndex = false;
    core::legacy::ZoneMetadata legacyZoneMetadata;
    std::vector<core::legacy::PreservedMapFile> preservedMapFiles;
    bool hasLegacyZoneMetadata = false;
    int selectedZone = 0;
    char zoneNameBuf[256] = "";

    app::OrbitCamera camera;
    app::OrbitCamera playtestSavedCamera; // Editor-Kamera vor dem Spieltest (wird danach wiederhergestellt)
    bool cameraLooking = false;       // rechte Maustaste im 3D-View gedrueckt (Umsehen + WASD)
    // 2D-Ansicht: Zoom (1 = ganze Karte) und Mittelpunkt des sichtbaren Ausschnitts in Kartenkoordinaten
    // (u = x/spanX, v = Bildzeile, Norden oben, siehe DrawEditor2DContent).
    float view2dZoom = 1.0f;
    float view2dCenterU = 0.5f, view2dCenterV = 0.5f;
    // Minimap is an editor-only overview until docs/MINIMAP_FORMAT.md has enough real
    // client-image evidence for a Fiesta exporter.
    bool minimapShowViewport = true;
    bool minimapShowObjects = false;
    bool minimapShowGame = true;        // Spiel-Minimap (resmenu\minimap) statt Editor-Vorschau
    bool minimapStyled = true;          // beim Erzeugen: Stil der Originale (Konturen, Pergament-Rand)
    bool minimapWithObjects = true;     // beim Erzeugen: Objekte mitzeichnen
    bool minimapPendingGenerate = false; // Assistent "Neue Karte": nach dem Laden erzeugen
    float townPortalImageYaw = 0.0f;    // Blickrichtung der Portal-Aufnahme (0 = nach Norden)
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
    // Terrain-less SHMD/NIF maps cannot be framed from a heightmap. Their camera is
    // focused from real NIF bounds on the first 3D frame after the scene is loaded.
    bool focusLoadedNifScenePending = false;
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
    std::unordered_map<long long, std::string> itemInxById;            // ItemInfo ID -> InxName (für Icons)
    // Icons (resmenu/Icon): InxName -> (IconFile, IconIndex) aus ItemViewInfo.shn, Atlasgrößen je Pfad.
    std::unordered_map<std::string, std::pair<std::string, int>> itemIconByInx;
    bool itemIconsTried = false;
    std::unordered_map<std::string, std::pair<std::uint32_t, std::uint32_t>> iconAtlasSizes;
    bool skillIconPickerOpen = false;
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
        std::string pickerPreviewKey;
        std::string pickerPreviewValue;
        char newInx[40] = "";
        char newName[64] = "";
        bool makeBook = true;
        bool rawOpen = false;
        int scalePercent = 100;
        bool scaleDamage = true, scaleCost = false, scaleCooldown = false, scaleCast = false;
        int quickFilter = 0; // 0 alle, 1 geändert, 2 Sync-Probleme
        std::unordered_set<long long> syncIssues;
        std::vector<std::string> report;
    } skill;
    struct CreatureWizard {
        int step = 0; // 0 Vorlage, 1 Identität/Werte, 2 Aussehen, 3 Rolle/Platzierung, 4 Zusammenfassung
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
    // NPC-Auswahl im Quest-Editor (Start-NPC, Ziel "NPC aufsuchen"): MobInfo + Platzierungen aus NPC.txt.
    struct QuestNpcEntry {
        int id = 0;
        std::string inx, name;
        bool isNpc = false;
        std::vector<std::string> placements; // "Karte X/Y · Rolle"
    };
    std::vector<QuestNpcEntry> questNpcCatalog;
    bool questNpcCatalogBuilt = false;
    int questNpcPickerTarget = -2; // -1 = Start-NPC, 0..4 = Monster-/NPC-Ziel, -2 = keins
    bool questNpcPickerOpenRequest = false; // Zielzeilen öffnen das Popup außerhalb ihres ID-Scopes

    // Kartenprüfung (Werkzeuge > Kartenprüfung): Ergebnisliste der letzten Prüfung.
    bool mapCheckOpen = false;
    bool mapCheckRunRequest = false;
    std::vector<core::MapCheckIssue> mapCheckIssues;
    std::string mapCheckForMap;          // Karte, für die die Liste gilt
    std::string mapCheckSummary;
    bool mapCheckShowErrors = true, mapCheckShowWarnings = true, mapCheckShowInfo = false;
    bool mapCheckIncludeQuests = true;
    std::map<std::string, std::shared_ptr<core::WalkGrid>> mapCheckGridCache; // Zielkarten der Gates

    // Automatisches Speichern der offenen Karte nach <Projekt>/Autosave (siehe core/Autosave.hpp).
    bool autosaveEnabled = true;
    int autosaveMinutes = 5;
    double autosaveLastTime = 0.0;          // ImGui::GetTime() der letzten Sicherung bzw. des Kartenöffnens
    std::uint64_t autosaveSavedRevision = 0; // mapEditRevision zum Zeitpunkt der letzten Sicherung
    bool autosaveWasDirty = false;
    std::future<std::string> autosaveTask;   // Hintergrund-Schreibvorgang; Ergebnis = Fehlertext oder leer
    std::string autosaveStatus;
    bool autosaveRecoveryPrompt = false;
    core::AutosaveMeta autosaveRecoveryMeta;
    char questNpcFilter[64] = "";
    bool questNpcOnlyPlaced = true;
    int automationQuestSection = 0; // nur Automatisierung (Screenshots): 1 Drops, 2 Belohnungen, 3 NPC-Auswahl, 4 Ziele
    // Quest-Editor-Caches: Text-ID -> Text (statt Linearsuche pro Aufruf), Listen-Beschriftungen und
    // sichtbare Eintraege (Filter) - der alte Editor durchsuchte QuestDialog fuer JEDE Quest in JEDEM
    // Frame (CHANGELOG [0.44.29]).
    std::unordered_map<int, std::string> questTextMap;
    bool questTextMapBuilt = false;
    std::vector<std::string> questLabels;
    std::string questListKey;
    std::vector<std::size_t> questVisible;
    std::uint64_t questRevision = 0;
    bool questDirty = false;
    int questQuickFilter = 0; // 0 alle, 1 aktiv, 2 täglich, 3 Referenzprobleme
    bool questShowFlow = false; // read-only relationship view; no script semantics are inferred
    char questSearch[128] = "";

    // Für Item-/Mob-Namensauflösung im Quest-Editor (dient zugleich als einfache Validierung -
    // leerer Name = ID existiert nicht, siehe CHANGELOG [0.44.21]).
    core::legacy::ShnFile itemInfoShn;
    bool itemInfoLoaded = false;

    // Portal-Editor (TownPortal.shn + RecallCoord.txt) - siehe CHANGELOG [0.44.20].
    core::legacy::ShnFile townPortalShn;
    bool townPortalLoaded = false;
    bool townPortalDirty = false;

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
    std::string aiScriptEditorPath;       // immutable source identity
    std::string aiScriptEditorText;
    std::string aiScriptEditorName;
    bool aiScriptDirty = false;
    char aiScriptLookupBuf[128] = "";

    // Eigenständiger AI-Workspace: katalogisiert die bereits unterstützten/verifizierten
    // LuaScript/AIScript/*.lua- und MobBehaviorDescript/*.ps-Dateien des Server-Shine-Baums.
    std::string aiWorkspaceScanKey;
    std::vector<std::filesystem::path> aiWorkspaceFiles;
    std::vector<std::string> aiWorkspaceLabels;
    std::vector<int> aiWorkspaceKinds; // 0 Lua, 1 PineScript
    char aiWorkspaceFilter[128] = "";
    int aiWorkspaceSelected = -1;

    // Patrouillenrouten-Editor (MobRoam/<Name>.txt, Server) - siehe CHANGELOG [0.44.23].
    bool patrolEditorOpen = false;
    std::string patrolEditorName;
    core::legacy::ShineTextFile patrolRouteFile;
    bool patrolRouteLoaded = false;

    // Permanente, rein lesende Szene-Overlays aus MobRoam/<Name>.txt. Der Cache wird nur
    // bei Auswahl-/Ordnerwechsel neu aufgebaut und verändert keine Fiesta-Dateien.
    struct RoamOverlayRoute {
        std::string name;
        std::vector<std::pair<float, float>> points; // X/Z im Kartenkoordinatensystem
        bool returnsToStart = false;
    };
    std::string roamOverlayKey;
    std::vector<RoamOverlayRoute> roamOverlayRoutes;
    bool showRoamRoutes = true;
    bool showAllRoamRoutes = false; // false = nur aktuelle Auswahl, true = alle Routen des aktuellen NPC-/Mob-Kontexts

    // Gecachte 2D-Geometrie je NIF-Modell. contactSegments ist die exakte Schnittkontur der
    // Mesh-Dreiecke mit der authored Boden-/Pivotebene und damit die primaere Editor-Darstellung.
    // Die konvexe Huelle bleibt vorerst nur fuer den bestehenden Walk/Block-Polygonpfad erhalten.
    struct ObjectFootprint {
        float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f, minZ = 0.0f, maxZ = 0.0f;
        std::vector<core::NifGroundContactSegment> contactSegments;
        std::vector<std::pair<float, float>> hull;
        bool valid = false;
    };
    std::unordered_map<std::string, ObjectFootprint> footprintCache;
    core::legacy::ShineTextFile recallCoordFile;
    bool recallCoordLoaded = false;
    bool recallCoordDirty = false;
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
        // Immutable provenance: the configured Client/Server source tree is read-only.
        // file.path may switch to the project override after the first save.
        std::filesystem::path sourcePath;
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
    int shnSubTab = 0; // 0=Single, 1=Multi, 2=XP, 3=Buy&Sell, 4=Quest, 5=Portal, 6=NPC/Mob, 7=Skill, 8=AI, 9=Interface
    char shnPath[1024] = "";
    char shnFileFilter[128] = "";
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
    bool shnHighlightKnownReferences = true;
    std::string shnStatus;

    // --- Interface Browser --------------------------------------------------
    // Erste sichere Ausbaustufe des Interface-Editors: rein lesender Katalog des echten
    // Client/resmenu-Bestands. TGA/DDS werden direkt vorgeschaut, NIFs verwenden denselben
    // verifizierten read-only NIF-/Materialinspektor wie der Karten-Asset-Browser.
    std::string interfaceRoot;
    std::vector<std::string> interfaceAssets;
    bool interfaceAssetsScanned = false;
    int interfaceSelectedAsset = -1;
    char interfaceAssetFilter[128] = "";
    bool interfaceOverridesOnly = false;
    bool interfaceChangedOverridesOnly = false;
    // lower-case relativer resmenu-Pfad -> InterfaceOverrideState als int.
    // Der Cache wird beim Scan aufgebaut und nach Create/Replace/Remove gezielt aktualisiert,
    // damit keine Dateivergleiche in jedem UI-Frame stattfinden.
    std::unordered_map<std::string, int> interfaceOverrideStateByAsset;

    // --- Drop Table Browser -------------------------------------------------
    // Semantische Ansicht von Server/9Data/Shine/World/ItemDropTable.txt. Die Datei ist
    // extrem breit (NA2016: 290 echte Datenspalten); deshalb wird sie nicht als Roh-Grid
    // dargestellt, sondern in Mob-Basisdaten + bis zu 45 Drop-Slots aufgeteilt.
    core::legacy::ShineTextFile dropTableFile;
    bool dropTableLoaded = false;
    bool dropTableDirty = false;
    std::string dropTableLoadError;
    int dropTableSelectedRecord = -1;
    char dropTableFilter[128] = "";
    bool dropTableOnlyActive = true;
    bool dropTableProblemsOnly = false;
    // ItemDropGroup.txt + ItemInfoServer DropGroupA/B: löst DrItem-Gruppennamen auf (s. ItemDropGroups.hpp).
    core::legacy::DropGroupCatalog dropGroups;
    bool dropGroupsTried = false;
    std::string dropGroupsStatus;

    std::string statusMessage;

    // --- Neue Navigationsebene (Projekt-Hub / Projekt-Konfiguration / Map-Editor-Start) ---
    AppScreen screen = AppScreen::ProjectHub;
    ProjectConfig project;
    bool mapDirty = false; // Legacy-Map-Module wurden seit dem letzten Öffnen/Speichern verändert.
    bool recentEntriesLoaded = false;
    std::vector<std::string> recentProjects;
    std::vector<std::string> recentMaps;

    struct ShortcutBinding {
        ImGuiKey key = ImGuiKey_None;
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
    };
    bool shortcutsLoaded = false;
    bool settingsOpen = false;
    ShortcutBinding shortcutPalette{ImGuiKey_P, true, false, false};
    ShortcutBinding shortcutSave{ImGuiKey_S, true, false, false};
    ShortcutBinding shortcutGizmoMove{ImGuiKey_1, false, false, false};
    ShortcutBinding shortcutGizmoRotate{ImGuiKey_2, false, false, false};
    ShortcutBinding shortcutGizmoScale{ImGuiKey_3, false, false, false};
    ShortcutBinding shortcutFocus{ImGuiKey_F, false, false, false};
    ShortcutBinding shortcutGround{ImGuiKey_End, false, false, false};
    ShortcutBinding shortcutDuplicate{ImGuiKey_D, true, false, false};
    ShortcutBinding shortcutDelete{ImGuiKey_Delete, false, false, false};

    bool commandPaletteOpen = false;
    char commandPaletteQuery[128] = "";
    int commandPaletteSelection = 0;
    struct Toast {
        std::string text;
        float remaining = 4.0f;
        bool error = false;
    };
    std::deque<Toast> toasts;
    std::string lastToastedStatus;

    std::string comingSoonTitle; // Titel der Karte, über die der Platzhalter-Bildschirm erreicht wurde

    // "Create New Map"-Formular (siehe DrawMapEditorLauncher) - ersetzt das bisherige feste
    // "Neu (257x257)" durch nutzerdefinierte Maße, wie im Mockup vorgesehen.
    char newMapName[128] = "";
    int newMapWidth = 257;
    int newMapHeight = 257;
    char newMapTextureLayer[128] = "Base";
    // Assistent "Neue Karte" (Vorlage, Größe, Spieldaten-Einträge, optionales Gate).
    char newMapDisplayName[64] = "";
    int newMapTemplate = -1;    // Index in discoveredMaps
    int newMapSize = 1;         // 0 = 129, 1 = 257, 2 = 513 Punkte
    float newMapBaseHeight = 0.0f;
    bool newMapWriteMapInfo = true;
    bool newMapWriteField = true;
    bool newMapGate = false;
    char newMapGateFrom[32] = "";
    int newMapGateX = 0, newMapGateY = 0;
    std::vector<std::string> newMapReport;
    // Texturregeln, gesperrte Layer (nach Name, übersteht Umsortieren) und Pinselform aus Bild.
    core::TextureRule texRule;
    bool texRuleBrush = false;
    std::set<std::string> lockedTextureLayers;
    core::BrushAlpha brushAlpha;
    bool brushAlphaEnabled = false;
    char brushAlphaPath[512] = "";
    std::string brushAlphaStatus, texRuleStatus;
    // Höhendaten-Austausch (PNG 16 Bit / R16) im Terrain-Werkzeug.
    char heightExchangePath[512] = "";
    float heightExchangeMin = 0.0f, heightExchangeMax = 1000.0f;
    std::string heightExchangeStatus;

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
    // Read-only NIF-/Material-Inspector im Asset Browser. Der Parser ist verifiziert,
    // schreibende NIF-Aenderungen bleiben bewusst deaktiviert, bis ein verlustfreier Writer
    // fuer die betroffenen Blocktypen belegt ist.
    std::string nifInspectorAsset;
    std::optional<core::NifModel> nifInspectorModel;
    std::string nifInspectorError;
    char nifInspectorFilter[128] = "";
    bool nifInspectorMissingOnly = false;
    // Texturpfade werden pro ausgewaehltem NIF gecacht. Ein leerer Wert bedeutet:
    // Referenz wurde geprueft, aber keine Datei gefunden. "Neu laden" leert den Cache.
    std::unordered_map<std::string, std::string> nifInspectorResolvedTextureCache;
    char objectOutlinerFilter[128] = "";
    int sceneNpcQuickFilter = 0;    // 0 alle, 1 Quest, 2 Handel, 3 Service, 4 Gates
    int sceneMobQuickFilter = 0;    // 0 alle, 1 leer, 2 eine Art, 3 gemischt
    int scenePortalQuickFilter = 0; // 0 alle, 1 Gates, 2 TownPortal, 3 Recall
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
    bool resetMapDockLayout = false;
    bool workspacePresetLoaded = false;
    int mapWorkspacePreset = 0;        // 0 Standard, 1 3D-Fokus, 2 Terrain/2D, 3 Daten/Szene
    int pendingMapWorkspacePreset = -1;
    std::string nifPrecacheDoneForRoot; // resmapRoot, für den die Vorladung zuletzt komplett durchlief
    std::vector<std::string> nifPrecacheQueue; // Kopie von availableNifFiles zum Abarbeiten
    std::size_t nifPrecacheCursor = 0;
};
std::filesystem::path NextGenUserSettingsDir();
bool MapSupportsTerrainEditing(const EditorState& state);
bool MapSupportsObjectGeometryEditing(const EditorState& state);
bool RequireWritableMapGeometry(EditorState& state);
const char* MapWorkspacePresetName(int preset);
void SaveWorkspaceSettings(const EditorState& state);
void LoadWorkspaceSettings(EditorState& state);
void RequestMapWorkspacePreset(EditorState& state,int preset);
std::string NormalizedRecentPath(const std::string& input);
void SaveRecentEntries(const EditorState& state);
void LoadRecentEntries(EditorState& state);
void TouchRecentPath(EditorState& state, std::vector<std::string>& list, const std::string& path);
void TouchRecentProject(EditorState& state, const std::string& path);
void TouchRecentMap(EditorState& state, const std::string& path);
std::size_t DirtyShnDocumentCount(const EditorState& state);
bool LoadProjectFolderIntoState(EditorState& state, const std::string& folder);
struct ShortcutKeyOption {
    ImGuiKey key;
    const char* name;
};
const std::vector<ShortcutKeyOption>& ShortcutKeyOptions();
const char* ShortcutKeyName(ImGuiKey key);
std::string ShortcutLabel(const EditorState::ShortcutBinding& binding);
bool SameShortcut(const EditorState::ShortcutBinding& a, const EditorState::ShortcutBinding& b);
bool ShortcutPressed(const EditorState::ShortcutBinding& binding);
void ResetShortcutSettings(EditorState& state);
std::array<std::pair<const char*, EditorState::ShortcutBinding*>, 9>
ShortcutSettings(EditorState& state);
void SaveShortcutSettings(EditorState& state);
void LoadShortcutSettings(EditorState& state);
void SaveViewportSettings(EditorState& state);
void LoadViewportSettings(EditorState& state);
std::filesystem::path CameraBookmarkFile(const std::string& mapStem);
void EnsureCameraBookmarksLoaded(EditorState& state);
void SaveCameraBookmarks(const EditorState& state);
std::string CompactToastText(const std::string& message);
void PushToast(EditorState& state, const std::string& message, bool error = false);
void SyncStatusToast(EditorState& state);
void DrawToasts(EditorState& state);
std::vector<std::string> ListFilesByExtension(const std::filesystem::path& root,
                                               const std::vector<std::string>& extensionsLower,
                                               int maxDepth = -1);
std::string ToLegacyResmapModelPath(std::string relativePath);
std::optional<std::filesystem::path> FindNamedSubfolder(
    const std::filesystem::path& resmapRoot, const std::vector<std::string>& namesLower);
std::optional<std::filesystem::path> FindDirBreadthFirst(
    const std::filesystem::path& root, const std::string& nameLower, int maxDepth,
    const std::function<bool(const std::filesystem::path&)>& accept);
std::optional<std::filesystem::path> FindClientRessystem(const std::filesystem::path& clientFolder);
std::optional<std::filesystem::path> FindServerShineRoot(const std::filesystem::path& serverFolder);
void SyncProjectRoots(EditorState& state);
core::legacy::LegacyMapProject BuildProjectFromState(const EditorState& state,
                                                     core::legacy::SpatialIndexRemapReport* idmReport = nullptr);
void FinishMapAutosave(EditorState& state, bool wait);
std::string CurrentMapServerName(EditorState& state);
std::string PortalServerRoot(const EditorState& state);
bool SaveLegacyMapProject(EditorState& state);
int ShmdCategoryKind(const std::string& name);
void RebuildShmdCategoryRenderSet(EditorState& state, const std::filesystem::path& mapDir);
constexpr int kNoObjectSelection = -1;
int ShmdSelectionId(std::size_t renderIndex);
bool IsShmdSelection(int selectionId);
std::optional<std::size_t> ShmdRenderIndex(int selectionId);
std::filesystem::path CurrentObjectAssetMapDir(const EditorState& state);
core::PlacedObject* EditableObject(EditorState& state, int selectionId);
const core::PlacedObject* EditableObject(const EditorState& state, int selectionId);
std::string ShmdSelectionCategoryName(const EditorState& state, int selectionId);
bool PromoteSelectedShmdObjectsToPlacements(EditorState& state,
                                               std::optional<int> onlySelection = std::nullopt);
void ReloadObjectRenderers(EditorState& state);
ObjectEditSnapshot CaptureObjectEditSnapshot(const EditorState& state);
void RestoreObjectEditSnapshot(EditorState& state, ObjectEditSnapshot snapshot);
void RegisterMapHistoryAction(EditorState& state, MapHistoryDomain domain);
TextureLayerMetadata CaptureTextureLayerMetadata(const core::TextureLayer& layer);
void RestoreTextureLayerMetadata(core::TextureLayer& layer, const TextureLayerMetadata& meta);
void RefreshTextureLayerHistoryState(EditorState& state, int selectedLayer);
std::optional<TextureLayerHistoryPatch> ApplyTextureLayerHistoryPatch(
    EditorState& state, TextureLayerHistoryPatch patch);
void PushTextureLayerHistory(EditorState& state, TextureLayerHistoryPatch patch);
void RecordTextureLayerAdded(EditorState& state, std::size_t index, int selectedBefore);
void RecordTextureLayerRemoved(EditorState& state, std::size_t index,
                               core::TextureLayer layer, int selectedBefore);
void RecordTextureLayerMove(EditorState& state, std::size_t from, std::size_t to, int selectedBefore);
void RecordTextureLayerMetadata(EditorState& state, std::size_t index,
                                TextureLayerMetadata before, int selectedBefore);
void RecordTextureStackReplacement(EditorState& state, core::TextureLayerStack before, int selectedBefore);
void InvalidateMapHistoryRedo(EditorState& state);
void RegisterMapHistoryAction(EditorState& state, MapHistoryDomain domain);
void ClearMapHistory(EditorState& state);
void PushHeightmapHistory(EditorState& state, core::UndoPatch patch);
void PushTexturePaintHistory(EditorState& state, core::TexturePaintPatch patch);
void PushWalkHistory(EditorState& state, core::WalkUndoPatch patch);
void PushObjectHistory(EditorState& state, ObjectEditSnapshot before);
void BeginObjectEditTransaction(EditorState& state);
void CommitObjectEditTransaction(EditorState& state);
std::optional<ObjectEditSnapshot> BeginObjectMutation(EditorState& state);
void FinishObjectMutation(EditorState& state, std::optional<ObjectEditSnapshot> before, bool changed = true);
bool CanUndoMapEdit(const EditorState& state);
bool CanRedoMapEdit(const EditorState& state);
bool UndoMapEdit(EditorState& state);
bool RedoMapEdit(EditorState& state);
void SyncObjectEditorMetadata(EditorState& state);
std::string ShmdEditorObjectKey(const EditorState& state, int id);
std::string ObjectEditorLabel(const EditorState& state, int id);
std::string ObjectEditorGroup(const EditorState& state, int id);
void SetObjectEditorLabel(EditorState& state, int id, std::string value);
void SetObjectEditorGroup(EditorState& state, int id, std::string value);
void TransferShmdEditorMetadataKey(EditorState& state, const std::string& oldKey,
                                   const std::string& newKey);
bool IsObjectEditorLocked(const EditorState& state, int id);
bool IsObjectEditorHidden(const EditorState& state, int id);
struct EditVec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
// Vorwärtsdeklaration: Route-/Overlay-Code steht vor der gemeinsamen 3D-Projektionshilfe.
bool ProjectWorldTo3DView(const EditorState& state, const ImVec2& imagePos, int w, int h,
                          const EditVec3& world, ImVec2& screen);
struct EditQuat { float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f; };
EditQuat NormalizeEditQuat(EditQuat q);
EditQuat MulEditQuatRaw(const EditQuat& a, const EditQuat& b);
EditQuat MulEditQuat(const EditQuat& a, const EditQuat& b);
EditQuat ConjugateEditQuat(const EditQuat& q);
EditVec3 RotateEditVec(const EditQuat& qn, const EditVec3& v);
app::Mat4 ObjectEditMatrix(const EditVec3& position, const EditQuat& qn, float scale);
float MatrixUniformScale(const app::Mat4& m);
EditQuat MatrixRotationQuat(const app::Mat4& m);
struct ObjectSelectionPivot {
    bool valid = false;
    EditVec3 position{};
    EditQuat rotation{};
    float scale = 1.0f;
    std::size_t editableCount = 0;
};
ObjectSelectionPivot ComputeObjectSelectionPivot(const EditorState& state);
void RotateSelectedObjectsAroundPivot(EditorState& state, const EditVec3& pivot, const EditQuat& delta);
void ScaleSelectedObjectsAroundPivot(EditorState& state, const EditVec3& pivot, float factor);
void CopySelectedObjects(EditorState& state);
void PasteObjectClipboard(EditorState& state);
void DuplicateSelectedObjects(EditorState& state, std::optional<float> offsetOverride = std::nullopt);
void GroundSelectedObjects(EditorState& state);
void FocusSelectedObjects(EditorState& state);
void SetObjectEditorHiddenFlag(EditorState& state, int id, bool hidden);
std::size_t CountEditorHiddenObjects(const EditorState& state);
void HideSelectedObjects(EditorState& state);
void HideUnselectedObjects(EditorState& state);
void ShowAllObjects(EditorState& state);
std::vector<int> EditableSelectedPlacementIds(const EditorState& state);
float& PlacementAxis(core::PlacedObject& object, int axis);
void AlignSelectedObjects(EditorState& state, int axis, core::level::AlignTarget target);
void DistributeSelectedObjects(EditorState& state, int axis);
void MatchSelectedObjectsToActive(EditorState& state, bool rotation, bool scale);
std::optional<float> GroundObjectHeightAt(const EditorState& state, float x, float z);
float SurfaceHeightAt(const EditorState& state, float x, float z, float fallback, bool* found = nullptr);
void SnapSelectedObjectsToTerrainSilently(EditorState& state);
void SelectObjectId(EditorState& state, int id, bool ctrl);
void ClearObjectSelection(EditorState& state);
void SelectAllNormalObjects(EditorState& state);
void SelectObjectFromList(EditorState& state, int id, int ordinal,
                          const std::vector<int>& orderedIds, bool ctrl, bool shift);
void SelectObjectOnCanvas(EditorState& state, int id, bool ctrl);
void EraseShmdCategorySources(EditorState& state,
                              std::vector<std::pair<std::size_t, std::size_t>> sources);
void DeleteSelectedObjects(EditorState& state);
void DeleteAllNormalObjects(EditorState& state);
bool PromoteSelectedShmdObjectsToPlacements(EditorState& state, std::optional<int> onlySelection);
void MoveSelectedObjectsBy(EditorState& state, float dx, float dy, float dz);
void RotateSelectedObjectsYawBy(EditorState& state, float deltaRadians);
void ScaleSelectedObjectsBy(EditorState& state, float factor);
void SyncSelectedObjectModelPath(EditorState& state);
void ApplySelectedObjectModelPath(EditorState& state, const std::string& modelPath);
void RefreshShmdCategoryVisibility(EditorState& state);
// Himmelskuppeln (SHMD-Kategorie Sky) sind um den Ursprung modelliert und folgen im Client der Kamera.
void PlaceSkyAtCamera(EditorState& state, const app::OrbitCamera& cam);
void ApplyProjectToState(EditorState& state, core::legacy::LegacyMapProject&& project, const std::filesystem::path& mapDir);
std::filesystem::path LegacyMapWorkingIniPath(
    const EditorState& state,
    const std::filesystem::path& iniPath,
    bool preferProjectOutput = true);
void LoadMapRenderData(EditorState& state, const std::filesystem::path& mapDir, const std::string& stem);
void CheckAutosaveRecovery(EditorState& state);
bool OpenLegacyMapIntoState(EditorState& state, const std::filesystem::path& iniPath,
                            bool preferProjectOutput = true);
bool OpenStandaloneNifMapIntoState(EditorState& state, const std::filesystem::path& nifPath);
bool OpenMapPathIntoState(EditorState& state, const std::filesystem::path& path,
                          bool preferProjectOutput = true);
bool MapAutosaveSupported(const EditorState& state);
void CheckAutosaveRecovery(EditorState& state);
bool StartMapAutosave(EditorState& state);
void FinishMapAutosave(EditorState& state, bool wait);
void TickAutosave(EditorState& state);
bool RestoreMapAutosave(EditorState& state);
void DrawAutosaveRecoveryPopup(EditorState& state);
void SyncWalkGridSize(EditorState& state);
void UpdatePreviewTexture(EditorState& state);
void UpdateLayerPreviewTexture(EditorState& state);
constexpr float WalkGridCellSize() { return core::WalkGrid::kCellSize; }
void UpdateWalkPreviewTexture(EditorState& state);
void UpdateWalkPreviewRect(EditorState& state, std::uint32_t x0, std::uint32_t z0, std::uint32_t x1, std::uint32_t z1);
#ifdef _WIN32
std::optional<std::string> BrowseForFolderWindows(const char* title);
#endif
#ifdef _WIN32
std::optional<std::string> BrowseForShnFileWindows(const char* title, bool kfm = false);
#endif
#ifdef _WIN32
std::optional<std::string> BrowseForInterfaceAssetWindows(const char* title, const std::string& extension);
#endif
void FindAllResmapCandidates(const std::filesystem::path& root, int depthRemaining,
                              std::vector<std::filesystem::path>& outCandidates);
std::vector<DiscoveredMap> ScanForMaps(
    const std::filesystem::path& root,
    bool allowDirectNifMapFolders = false);
// Ergebnis der Kartensuche: welcher "resmap"-Kandidat letztlich verwendet wurde (falls
// überhaupt einer gefunden wurde), die darin gefundenen Karten, und wie viele Kandidaten
// insgesamt in Frage kamen (zur Anzeige, falls mehrere "resmap"-Ordner existieren).
struct ResmapResolution {
    std::optional<std::filesystem::path> root;
    std::vector<DiscoveredMap> maps;
    int candidateCount = 0;
};
ResmapResolution ResolveMapSearchRootAndScan(const std::filesystem::path& clientFolder);
std::size_t MergeProjectOnlyMaps(
    const ProjectConfig& cfg,
    std::vector<DiscoveredMap>& maps);
void DrawAdvancedFileOps(EditorState& state);
void DrawIconGlobe(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconPencilPaper(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconBook(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconMonitorEye(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconAtom(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconClapper(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconTerrain(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconBrush(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconLayers(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconGrid(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconCube(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconPerson(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconSpawn(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconShop(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconRoute(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconDialog(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconCode(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconMove(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconRotate(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconScale(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconSnap(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconPortal(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconSave(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconUndo(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconRedo(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconTable(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconBolt(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconEye(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconLock(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconDuplicate(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconDelete(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconGear(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconSword(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconGem(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconCoin(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconDice(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconBanner(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
void DrawIconPack(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
using IconDrawFn = void (*)(ImDrawList*, ImVec2, float, ImU32);
struct SceneSemanticIcon {
    IconDrawFn icon = DrawIconPerson;
    ImU32 color = IM_COL32(120,200,255,240);
    std::string tooltip;
};
SceneSemanticIcon ResolveNpcSceneIcon(const std::string& role, const std::string& arg);
SceneSemanticIcon ResolveMobZoneSceneIcon(int speciesCount);
int NpcSceneGroupRank(const std::string& role, const std::string& arg);
std::string NpcSceneGroupLabel(const std::string& role, const std::string& arg);
int MobSceneGroupRank(int speciesCount);
const char* MobSceneGroupLabel(int speciesCount);
bool SceneQuickFilterButton(const char* id,const char* label,bool active);
void DrawInlineIcon(const char* id, IconDrawFn icon, ImU32 color,
                    const char* tooltip = nullptr, ImVec2 size = ImVec2(20.0f,20.0f),
                    const char* semanticIcon = nullptr);
bool DrawSearchInput(const char* id, const char* hint, char* buffer, std::size_t bufferSize,
                     float inputWidth = -1.0f, bool focusInput = false);
void DrawContextMenuHeader(const char* id, const char* title, IconDrawFn fallbackIcon,
                           const char* semanticIcon, const char* subtitle = nullptr);
void DrawPanelHeader(const char* id, const char* title, IconDrawFn fallbackIcon,
                     const char* semanticIcon, const char* subtitle = nullptr);
bool DrawTinyIconButton(const char* id, IconDrawFn icon, bool active, const char* tooltip,
                        ImVec2 size = ImVec2(22.0f,22.0f), const char* semanticIcon = nullptr);
bool DrawIconButton(const char* id, const char* label, IconDrawFn icon, bool active = false,
                    ImVec2 size = ImVec2(74.0f, 58.0f), bool enabled = true,
                    const char* semanticIcon = nullptr);
bool DrawCompactIconTextButton(const char* id, const char* label, IconDrawFn fallbackIcon,
                               const char* semanticIcon, bool enabled = true,
                               const char* tooltip = nullptr, bool active = false);
bool DrawEditorCard(const char* id, ImVec2 size, ImU32 bodyColor, ImU32 headerColor, IconDrawFn icon,
                    const char* title, const std::vector<std::string>& features, bool startEnabled,
                    const char* semanticIcon = nullptr);
// Später definierte Aktionen, die auch aus der globalen Menüleiste erreichbar sein müssen.
void SaveTownPortalFiles(EditorState& state);
bool SaveRecallCoordFile(EditorState& state);
bool FocusCurrentSceneSelection(EditorState& state);
// Obere Navigationsleiste, auf allen Bildschirmen der neuen Oberfläche sichtbar - links die
// Tabs, rechts Credits/Donate/? und die Sprachumschaltung (DE/EN, siehe Localization.hpp).
// tabs==nullptr blendet die linken Tabs aus (Detail-Bildschirme zeigen stattdessen NUR den
// aktuellen Titel als "Breadcrumb", siehe Mockup rechtes/zweites Bild).
// Vorwärtsdeklarationen der Viewport-/Spieltest-Werkzeuge (Definitionen beim 3D-Viewport).
const char* ViewportPresetName(int preset);
void ApplyViewportPreset(EditorState& state, int preset);
void DrawViewportShowFlagsMenu(EditorState& state);
void TogglePlaytest(EditorState& state);
const char* ViewportLayoutName(int layout);
void SetViewportLayout(EditorState& state, int layout);
void DrawTopNav(EditorState& state, const char* breadcrumbTitle);
std::string ShnShortValue(const core::legacy::ShnValue& value);
const char* ShnSourceName(EditorState::ShnSource source);
ImVec4 ShnSourceColor(EditorState::ShnSource source);
std::string LowerAscii(std::string value);
EditorState::ShnSource InferShnSource(const std::filesystem::path& path, EditorState::ShnSource fallback);
void SelectShnDocument(EditorState& state, int index);
std::filesystem::path ShnWorkingPath(
    const EditorState& state,
    const std::filesystem::path& sourcePath,
    EditorState::ShnSource source);
void OpenShnFile(EditorState& state, const std::filesystem::path& path,
                std::optional<EditorState::ShnSource> sourceOverride = std::nullopt);
void ScanShnFolder(EditorState& state, const std::filesystem::path& root, EditorState::ShnSource source);
std::vector<int> ShnIndicesForSource(const EditorState& state, EditorState::ShnSource source);
int FindShnCounterpart(const EditorState& state, int document);
std::vector<int> MatchShnColumnsByName(const core::legacy::ShnFile& source,
                                       const core::legacy::ShnFile& counterpart);
const core::DataDependencyProfile& ShnDependencyProfile(int profile);
bool ShnDependencyRuleMatchesDocument(const core::DataDependencyRule& rule,
                                      const EditorState::ShnDocument& doc);
bool ShnProfileMatch(const EditorState::ShnDocument& doc, int profile);
const char* ShnProfileName(int profile);
bool ShnValueAsInt(const core::legacy::ShnValue& v, long long& out);
// High-confidence cross-file ID families verified against the supplied NA2016 corpus.
// Only these exact families participate in semantic reference/error highlighting. We
// deliberately do NOT infer references from generic numeric overlap or column names.
struct KnownShnIdFamily {
    const char* label;
    std::array<const char*,3> files;
};
const KnownShnIdFamily* KnownShnFamilyFor(const std::string& fileName);
int ExactShnColumnIndex(const core::legacy::ShnFile& file, const char* name);
int BestLoadedShnDocument(const EditorState& state, const char* fileName,
                                 EditorState::ShnSource preferredSource, int excludeDocument);
struct LoadedShnFamilyPeer {
    int document = -1;
    int idColumn = -1;
    std::string fileName;
    std::unordered_map<long long,int> rowById;
    std::unordered_set<long long> duplicateIds;
};
std::vector<LoadedShnFamilyPeer> BuildKnownShnFamilyPeers(const EditorState& state, int currentDocument);
enum class ShnReferenceState { None, Valid, Missing, Ambiguous };
std::unordered_set<long long> DuplicateIdsInColumn(const core::legacy::ShnFile& file, int idColumn);
ShnReferenceState KnownShnReferenceState(const std::vector<LoadedShnFamilyPeer>& peers,
                                                long long id, bool currentIdDuplicate = false);
std::string KnownShnReferenceTooltip(const std::vector<LoadedShnFamilyPeer>& peers,
                                            long long id);
bool DrawKnownShnFamilyReferenceStrip(EditorState& state, int currentDocument,
                                             const std::vector<LoadedShnFamilyPeer>& peers);
// Zellstatus-Konstanten für die Grün/Rot-Markierung, siehe EditorState::ShnDocument::cellStatus.
constexpr std::uint8_t kShnCellNormal = 0;
constexpr std::uint8_t kShnCellAutoFilled = 1;
   // grün - aus einer anderen Datei übernommen
constexpr std::uint8_t kShnCellNeedsInput = 2;
   // rot - braucht noch manuelle Eingabe

void EnsureCellStatusSize(EditorState::ShnDocument& doc);
bool ShnValueAsNumber(const core::legacy::ShnValue& value, long double& out);
int CompareShnValues(const core::legacy::ShnValue& a, const core::legacy::ShnValue& b);
void ClearShnDirtyCells(EditorState::ShnDocument& doc);
std::expected<void, std::string> ValidateShnPatch(EditorState::ShnDocument& doc);
std::vector<char> ReadFileBytes(const std::filesystem::path& path);
std::expected<std::filesystem::path, std::string> ShnProjectTarget(
    const EditorState& state, const EditorState::ShnDocument& doc);
bool SaveShnDocument(EditorState& state,int document);
std::pair<std::size_t,std::size_t> SaveAllDirtyShnDocuments(EditorState& state);
bool ReloadShnDocument(EditorState& state,int document);
bool ApplyShnCellText(EditorState& state, int document, int row, int column,
                      const std::string& text, bool recordUndo = true);
void UndoShnCellEdit(EditorState& state);
void RedoShnCellEdit(EditorState& state);
void CopySelectedShnCell(EditorState& state);
void PasteSelectedShnCell(EditorState& state);
void StartShnInlineEdit(EditorState& state, int row, int column);
void DrawShnCellEditor(EditorState& state);
void EnsureCellStatusSize(EditorState::ShnDocument& doc);
std::string DefaultShnValueText(const core::legacy::ShnColumn& column);
// Legt in der aktuell ausgewählten Datei eine neue Zeile an. Automatische Propagation ist
// absichtlich auf die gegen den NA2016-Korpus verifizierten ID-Familien Item/Mob/ActiveSkill
// begrenzt. FindDependencyPeers (Namensstamm/Zeilenzahl) bleibt nur Diagnose und darf niemals
// Dateien mutieren.
void AddRowWithPropagation(EditorState& state, int docIndex);
void DrawShnGrid(EditorState& state);
std::string ShnNameStem(const std::string& fileNameNoExt);
std::vector<int> FindRowCountPeers(const EditorState& state, int docIndex);
std::vector<int> FindNameStemPeers(const EditorState& state, int docIndex);
std::vector<int> FindDependencyPeers(const EditorState& state, int docIndex);
int ShnColumnIndexByName(const core::legacy::ShnFile& f, const std::string& name);
int FindShnIdColumn(const core::legacy::ShnFile& f);
long long ShnMaxInt(const core::legacy::ShnFile& f, int col);
void CollectShnInts(const core::legacy::ShnFile& f, int col, std::set<long long>& out);
long long SuggestFreeShnId(const core::legacy::ShnColumn& column, const std::set<long long>& used);
void AddRowWithPropagation(EditorState& state, int docIndex);
void DrawShnSourceList(EditorState& state, EditorState::ShnSource source, const char* id);
void DrawShnMultiProfiles(EditorState& state);
int FindOrLoadShnDoc(EditorState& state, const std::string& fileName, EditorState::ShnSource source);
bool OpenShnRecordById(EditorState& state, const std::vector<std::string>& fileNames,
                       EditorState::ShnSource preferredSource, long long id);
void DrawScalableFieldEditor(EditorState& state, int docIndex, const std::vector<std::string>& scaleCols,
                              const char* title);
// Vorwärtsdeklaration: Definition weiter unten, nach DrawQuestEditor.
void DrawQuestEditor(EditorState& state);
void EnsureMobViewInfoLoaded(EditorState& state);
 // Definition weiter unten, bei der NPC-3D-Auflösung
void DrawPortalEditor(EditorState& state);
void DrawCustomCreatureEditor(EditorState& state);
void DrawSkillEditor(EditorState& state);
void DrawAiWorkspace(EditorState& state);
void DrawInterfaceWorkspace(EditorState& state);
void DrawDropTableEditor(EditorState& state);
bool SaveDropTable(EditorState& state);
void DrawShnEditor(EditorState& state);
void DrawProjectHub(EditorState& state);
void DrawNewProjectConfig(EditorState& state);
// Vorwärtsdeklarationen: Definitionen liegen weiter unten, nach GetOrLoadAssetThumbnail (dort
// sind FindResmapRootForAssets & Co. bereits vorhanden) - einzige bewusste Ausnahme vom sonst
// in main.cpp durchgehend eingehaltenen Top-Down-Stil, siehe CHANGELOG [0.44.10].
std::optional<std::filesystem::path> FindResmapRootForAssets(const std::string& clientFolder);
void StartNifThumbnailPrecache(EditorState& state, const std::filesystem::path& resmapRoot);
void AdvanceNifPrecache(EditorState& state, int filesPerFrame);
bool CreateNewMapFromWizard(EditorState& state);
void DrawMapEditorLauncher(EditorState& state);
void DrawComingSoon(EditorState& state);
std::optional<std::filesystem::path> FindResmapRootForAssets(const std::string& clientFolder);
std::uint32_t UploadThumbnailTexture(const std::vector<std::uint8_t>& rgba, std::uint32_t width, std::uint32_t height);
// Rendert ein NIF-Modell als Vorschaubild (eigener NifMeshRenderer + Offscreen-Framebuffer):
// Kamera schräg von oben, an der Welt-AABB des Modells ausgerichtet. Gibt eine normale
// RGBA-Thumbnail-Textur zurück (0 = Modell nicht ladbar/leer).
struct NifPreviewRenderTarget {
    app::NifMeshRenderer renderer;
    bool initialized = false;
    GLuint fbo = 0, color = 0, depth = 0;
    int size = 0;
};
NifPreviewRenderTarget& NifPreviewTarget();
std::uint32_t RenderNifPreviewThumbnail(const std::filesystem::path& nifPath, int size = 192,
                                        std::vector<std::uint8_t>* pixelsOut = nullptr);
EditorState::AssetThumbnail GetOrLoadAssetThumbnail(EditorState& state,
                                                      const std::filesystem::path& resolvedPath,
                                                      bool isNif,
                                                      const std::filesystem::path& resmapRoot = {});
void StartNifThumbnailPrecache(EditorState& state, const std::filesystem::path& resmapRoot);
void AdvanceNifPrecache(EditorState& state, int filesPerFrame);
bool DrawAssetPickerPopup(const char* popupId, const std::vector<std::string>& files,
                           std::string& filterBuf, std::string& outSelected,
                           EditorState& state, const std::filesystem::path& assetRoot, bool isNif,
                           const std::filesystem::path& resmapRoot = {});
void EnsureNpcTextLoaded(EditorState& state);
// Indizes der "ShineNPC"-Records, deren Map-Spalte (Index 1) exakt der aktuellen Karte
// entspricht (state.legacySaveStem) - Map-Spaltenwerte in NPC.txt entsprechen echten Karten-
// Ordnernamen (z.B. "Rou", "RouN", "EldCem01"), byte-exakt gegen die echten NA2016-Daten
// geprüft, siehe docs (CHANGELOG [0.44.15]).
// Servername der offenen Karte für World/NPC.txt, MobRegen, TownPortal und MapInfo. Der Kartenordner
// kann anders geschrieben sein als der Servername (NA2016: bera/bera.ini, Server "Bera"); maßgeblich
// ist MapInfo.shn (MapFolderName -> MapName), sonst der Ordnername.
std::string CurrentMapServerName(EditorState& state);
std::string TrimAscii(const std::string& in);
bool SameMapName(const std::string& a, const std::string& b);
std::vector<std::size_t> NpcRecordsForCurrentMap(EditorState& state);
void EnsureMobRegenLoaded(EditorState& state);
void EnsureShopTextLoaded(EditorState& state, const std::string& npcName);
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
std::vector<std::string> ExtractBracketed(const std::string& s);
ParsedNpcDialog ParseNpcDialogText(const std::string& raw);
std::string SerializeNpcDialogText(const ParsedNpcDialog& d);
void EnsureNpcDialogRoot(EditorState& state);
void EnsureQuestDataLoaded(EditorState& state);
void EnsureQuestDialogLoaded(EditorState& state);
std::string ResolveQuestText(EditorState& state, int textId);
void EnsureItemInfoLoadedForQuests(EditorState& state);
std::pair<std::string, bool> ResolveItemNameForQuest(EditorState& state, int id);
std::pair<std::string, bool> ResolveMobNameForQuest(EditorState& state, int id);
void DrawResolvedIdLabel(const std::pair<std::string, bool>& resolved);
std::string QuestTextOf(EditorState& state, int textId);
void DrawQuestFlowView(EditorState& state,
                       const std::vector<core::legacy::QuestRecord>& quests,
                       std::size_t currentIndex);
std::expected<bool, std::string> RegularFileState(const std::filesystem::path& path);
bool SaveQuestDataProject(EditorState& state);
void EnsureQuestNpcCatalog(EditorState& state);
const EditorState::QuestNpcEntry* FindQuestNpc(EditorState& state, int id);
std::string QuestNpcPlacementText(const EditorState::QuestNpcEntry& e);
int DrawQuestNpcPickerPopup(EditorState& state);
void DrawQuestEditor(EditorState& state);
std::string PortalServerRoot(const EditorState& state);
void EnsureTownPortalLoaded(EditorState& state);
void EnsureRecallCoordLoaded(EditorState& state);
void SaveTownPortalFiles(EditorState& state);
bool SaveRecallCoordFile(EditorState& state);
void DrawPortalEditor(EditorState& state);
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
constexpr int kPortalKindGateLink = 3;
struct PortalMarker {
    int kind = kPortalKindNone;
    std::size_t idx = 0;   // TownPortal-Zeile, RecallPoint-Record oder ShineNPC-Record
    float x = 0.0f;
    float y = 0.0f;
    std::string label;

    // Nur für echte ausgehende Gate-Verknüpfungen aus World/NPC.txt:
    // ShineNPC.RoleArg0 -> LinkTable.argument. Die NA2016-Referenzdaten belegen damit
    // Zielkarte, Zielkoordinate, Richtung und Party-Flag ohne Heuristik.
    std::string linkKey;
    std::string sourceRole;
    std::string targetMapServer;
    std::string targetMapClient;
    float targetX = 0.0f;
    float targetY = 0.0f;
    int targetDirect = 0;
    bool targetParty = false;
};
int FindShnColumnByName(const core::legacy::ShnFile& f, const std::string& name);
long long ShnCellInt(const core::legacy::ShnFile& f, std::size_t row, int col);
void SetShnCellInt(core::legacy::ShnFile& f, std::size_t row, int col, long long v);
std::string TrimAscii(const std::string& in);
void EnsureMapInfoLoaded(EditorState& state);
void EnsurePortalDataLoaded(EditorState& state);
bool FindGateTownNpc(EditorState& state, float& x, float& y);
std::string CurrentMapServerName(EditorState& state);
bool FindMapRegen(EditorState& state, float& x, float& y);
// LinkTable-Spalten aus World/NPC.txt per Namen: in NA2016 stehen vor "argument" sieben leere
// Spalten (die Records beginnen mit 7 leeren Werten); feste Indizes 0..6 lasen deshalb nichts.
struct LinkTableColumns {
    int argument = 0, mapServer = 1, mapClient = 2, x = 3, y = 4, direct = 5, party = 6;
};
LinkTableColumns FindLinkTableColumns(const core::legacy::ShineTable& table);
const std::string& LinkValue(const core::legacy::ShineRecord& record, int column);
std::vector<PortalMarker> CollectPortalMarkers(EditorState& state);
std::string PortalTargetMapName(const PortalMarker& marker);
void EnsureDiscoveredMaps(EditorState& state);
int FindDiscoveredMapByName(const EditorState& state, const std::string& mapName);
bool NavigateToPortalTarget(EditorState& state, const PortalMarker& marker);
void SetSelectedPortalPosition(EditorState& state, long long x, long long y);
std::string TownPortalMapName(EditorState& state, std::size_t row);
void AddTownPortalForCurrentMap(EditorState& state);
bool DrawShineRecordFields(std::vector<std::string>& values, const std::vector<core::legacy::ShineColumn>& columns,
                           std::size_t firstEditable);
bool DrawPortalFolderField(const char* hint, const char* id, std::string& value);
void DrawPortalsToolsPanel(EditorState& state);
void DrawPortalMarkers2D(EditorState& state, const ImVec2& origin, const ImVec2& size, float spanX, float spanZ);
bool FocusCurrentSceneSelection(EditorState& state);
void EnsureNpcDialogLoaded(EditorState& state);
bool LoadAiScriptFile(EditorState& state, const std::filesystem::path& path,
                      const std::string& displayName, bool openPopup, bool discardDirty=false);
bool SaveAiScript(EditorState& state);
void ScanAiWorkspace(EditorState& state);
void OpenAiScriptEditor(EditorState& state, const std::string& mobName);
void OpenPineScriptEditor(EditorState& state, const std::string& relScriptPath);
void OpenPatrolRouteEditor(EditorState& state, const std::string& mobName);
void DrawPatrolRouteEditorPopup(EditorState& state);
void DrawAiScriptEditorPopup(EditorState& state);
void DrawAiWorkspace(EditorState& state);
int FindDialogRowForNpc(EditorState& state, const std::string& npcName);
void EnsureMobViewInfoLoaded(EditorState& state);
std::string ResolveMobViewFileName(EditorState& state, const std::string& inxName);
void EnsureRescharRoot(EditorState& state);
static const char* const kNpcRoles[] = {"QuestNpc", "Merchant", "Guard", "NPCMenu", "StoreManager", "Gate"};
static const char* const kAvatarSlotNames[19] = {
    "Rechte Hand", "Linke Hand", "Körper", "Beine", "Schuhe", "Acc. Körper", "Acc. Beine", "Acc. Schuhe", "Acc. Mund",
    "Acc. Kopf A", "Acc. Augen", "Acc. Kopf", "Acc. Linke Hand", "Acc. Rechte Hand", "Acc. Rücken", "Acc. Taille", "Acc. Hüfte",
    "Mini-Monster", "Mini-Monster R"};
static const char* const kAvatarSlotNamesEn[19] = {
    "Right hand", "Left hand", "Body", "Legs", "Shoes", "Acc. body", "Acc. legs", "Acc. shoes", "Acc. mouth",
    "Acc. head A", "Acc. eyes", "Acc. head", "Acc. left hand", "Acc. right hand", "Acc. back", "Acc. waist", "Acc. hip",
    "Mini monster", "Mini monster R"};
static const char* const kAvatarSlotColumns[19] = {
    "Equ_RightHand", "Equ_LeftHand", "Equ_Body", "Equ_Leg", "Equ_Shoes", "Equ_AccBody", "Equ_AccLeg", "Equ_AccShoes", "Equ_AccMouth",
    "Equ_AccHeadA", "Equ_AccEye", "Equ_AccHead", "Equ_AccLeftHand", "Equ_AccRightHand", "Equ_AccBack", "Equ_AccWeast", "Equ_AccHip",
    "Equ_MiniMon", "Equ_MiniMon_R"};
std::string ShnCellText(const core::legacy::ShnFile& f, std::size_t row, const char* col);
bool SetShnCellText(core::legacy::ShnFile& f, std::size_t row, const char* col, const std::string& text);
core::AvatarRequest MakeAvatarRequest(EditorState& state, int cls, bool male, int face, int hairType, const std::array<std::string, 19>& equ);
float NpcYawRadians(const EditorState& state, int directionDeg);
void ApplyNpcRecordToObject(const EditorState& state, const core::legacy::ShineRecord& rec, core::PlacedObject& obj);
void RefreshNpcTransforms(EditorState& state);
void EstimateNpcOrientation(EditorState& state);
std::optional<std::filesystem::path> FindChildCaseInsensitive(const std::filesystem::path& dir, const std::string& name);
std::string FindNpcModelPath(EditorState& state, const std::string& fileName);
// Ergebnis der (GL-freien) Modellauflösung aller NPCs der aktuellen Karte.
struct NpcRenderData {
    core::ObjectPlacementSet set;
    std::vector<std::size_t> recordIdx;
    std::unordered_map<std::string, core::NifModel> customModels;
    int withModel = 0, avatars = 0, missing = 0;
    std::vector<std::string> missingNames;
};
NpcRenderData CollectNpcRenderData(EditorState& state);
void EnsureNpcModelsLoaded(EditorState& state);
void OpenNpcDialogEditor(EditorState& state, const std::string& npcName);
void DrawDialogEditorPopup(EditorState& state);
void EnsureItemLookup(EditorState& state);
void RenumberShopRows(core::legacy::ShineTable& table);
core::legacy::ShineRecord MakeEmptyShopRow(const core::legacy::ShineTable& table);
bool CreateNewShopFile(EditorState& state, const std::string& npcName);
void DrawShopEditorPopup(EditorState& state);
bool StringPickerPopup(const char* popupId, const std::vector<std::string>& options, char* filterBuf, std::size_t filterSize,
                              std::string& chosen, const std::vector<std::string>* labels = nullptr);
std::vector<std::size_t> CloneRowsById(EditorState& state, const char* fileName, EditorState::ShnSource src,
                                              const char* nameCol, long long templateId, const std::string& templateInx,
                                              long long newId, const std::string& newInx, std::vector<std::string>& report);
bool CreateNewMapFromWizard(EditorState& state);
void RunCreateCreature(EditorState& state);
extern bool g_disableGlUploads;
core::AvatarRequest MakeAvatarRequest(EditorState& state, int cls, bool male, int face, int hairType, const std::array<std::string, 19>& equ);
void UpdateAvatarPreview(EditorState& state);
void DrawCreatureWizardPreview(EditorState& state);
void DrawCustomCreatureEditor(EditorState& state);
const char* L(const char* de, const char* en);
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
    {"Allgemein & Voraussetzungen", "General & requirements", kBasics, sizeof(kBasics) / sizeof(Field), true},
    {"Kosten & Cooldown", "Costs & cooldown", kCosts, sizeof(kCosts) / sizeof(Field), false},
    {"Schaden", "Damage", kDamage, sizeof(kDamage) / sizeof(Field), false},
    {"Ziele & Bewegung", "Targets & movement", kMisc, sizeof(kMisc) / sizeof(Field), false},
    {"Serverwerte", "Server values", kServer, sizeof(kServer) / sizeof(Field), false},
    {"Animation / VFX / Darstellung", "Animation / VFX / presentation", kView, sizeof(kView) / sizeof(Field), true},
};
}
 // namespace skilled

struct SkillDocs { int skillC = -1, skillS = -1, server = -1, viewC = -1, viewS = -1; };
SkillDocs ResolveSkillDocs(EditorState& state);
std::vector<int> SkillDocsOf(const SkillDocs& d, skilled::Doc kind);
long long SkillRowIn(EditorState& state, int docIdx, long long skillId);
std::string SkillCellRead(EditorState& state, const SkillDocs& d, skilled::Doc kind, const char* col, long long skillId);
bool SkillCellCopiesDiffer(EditorState& state,const SkillDocs& d,skilled::Doc kind,
                                  const char* col,long long skillId,std::string* details=nullptr);
bool SkillCellWrite(EditorState& state, const SkillDocs& d, skilled::Doc kind, const char* col, long long skillId, const std::string& text);
void BuildSkillPickerOptions(EditorState& state, const SkillDocs& d);
const char* SkillPickerKey(skilled::Kind kind,const char* column);
bool SkillPresentationPickerPopup(EditorState& state,const SkillDocs& d,
                                         const skilled::Field& field,const std::string& mapKey,
                                         const std::string& current,std::string& chosen);
void DrawSkillField(EditorState& state, const SkillDocs& d, const skilled::Field& fld, long long skillId, ImVec4 dim);
void CreateSkillFrom(EditorState& state, const SkillDocs& d, long long templateId, const std::string& newInx, const std::string& newName, bool makeBook);
int ScaleSkillSeries(EditorState& state, const SkillDocs& d, const std::string& seriesKey, int percent, const std::vector<const char*>& columns);
void DrawSkillEditor(EditorState& state);
void DrawManualBody(const char* body);
void DrawColumnReference(EditorState& state);
void DrawManualWindow(EditorState& state);
void DrawGlobalHelpBar(EditorState& state, const ImVec2& displaySize);
void LeaveAxisView(EditorState& state);
void SetCameraBookmark(EditorState& state, int slot);
bool JumpToCameraBookmark(EditorState& state, int slot);
const char* ViewportPresetName(int preset);
void ApplyViewportPreset(EditorState& state, int preset);
// ---- Spieltest (Play-in-Editor) --------------------------------------------------------------
constexpr float kPlaytestEyeHeight = 45.0f;
    // Editorwert für die Kamerahöhe über dem Boden
constexpr float kPlaytestPawnHeight = 60.0f;
float PlaytestGroundHeight(const EditorState& state, float x, float z, float fallback);
void StartPlaytest(EditorState& state);
void StopPlaytest(EditorState& state);
void TogglePlaytest(EditorState& state);
bool GizmoShortcutPressed(const EditorState& state, const EditorState::ShortcutBinding& binding);
bool DigitReservedByShortcut(EditorState& state, ImGuiKey key);
void SetGizmoOperation(EditorState& state, int op);
void HandleGlobalShortcuts(EditorState& state);
std::string MapCheckFolderForMapName(EditorState& state, const std::string& mapName);
const core::WalkGrid* MapCheckGridForMap(EditorState& state, const std::string& mapName);
void RunMapCheck(EditorState& state);
std::string MapCheckMessage(const core::MapCheckIssue& i);
const char* MapCheckCategoryLabel(core::MapCheckCode code);
void JumpToMapCheckIssue(EditorState& state, const core::MapCheckIssue& i);
void DrawMapCheckWindow(EditorState& state);
void DrawSettingsWindow(EditorState& state);
void DrawCommandPalette(EditorState& state);
void DrawVisibilityPanel(EditorState& state);
void DrawBrushAlphaControls(EditorState& state, const char* id);
void DrawScatterControls(EditorState& state);
void DrawPrefabControls(EditorState& state);
void DrawSplineControls(EditorState& state);
void DrawSpline2D(const EditorState& state, ImDrawList* drawList, const ImVec2& origin, const ImVec2& size, float spanX, float spanZ);
bool PlacePrefabAtWorld(EditorState& state, float worldX, float worldZ);
void DrawTextureRuleControls(EditorState& state);
std::vector<std::vector<std::pair<float, float>>> CollectVisibleObjectFootprints(EditorState& state);
const std::unordered_map<std::string, std::pair<std::size_t, std::size_t>>& ModelBlockStats(EditorState& state);
void StampObjectFootprints(EditorState& state, bool blocked);
void DrawWalkGeneratePanel(EditorState& state);
void UpdateWalkDiffTexture(EditorState& state);
void ComputeWalkProposal(EditorState& state);
void ApplyWalkProposal(EditorState& state);
void RefreshObjectVisibility(EditorState& state);
bool IsObjectHidden(const EditorState& state, std::size_t i);
int PlaceObjectAtWorld(EditorState& state, const std::string& modelPath, float x, float y, float z);
float ActiveWorldBrushRadius(const EditorState& state);
ImU32 ActiveBrushColor(const EditorState& state, int alpha = 235);
void DrawToolsContent(EditorState& state);
// ---------------------------------------------------------------------------
// Objekt-Kategorien (Sichtbarkeit): aus dem Modell-Dateinamen abgeleitet (Schluesselwoerter).
// Die Reihenfolge der Pruefung ist wichtig ("flowerpot" ist Deko, nicht Blume; "lightbug" ein Effekt).
// ---------------------------------------------------------------------------
enum ObjCategory { kCatTrees = 0, kCatGrass, kCatRocks, kCatBuildings, kCatFences, kCatProps, kCatWater, kCatCreatures, kCatEffects, kCatOther, kCatCount };
const char* ObjectCategoryDisplayName(int category);
int ClassifyObjectModel(const std::string& modelPath);
void RefreshObjectVisibility(EditorState& state);
bool IsObjectHidden(const EditorState& state, std::size_t i);
void DrawVisibilityPanel(EditorState& state);
const EditorState::ObjectFootprint& GetOrComputeFootprint(EditorState& state, const std::string& modelPath);
// Zeichnet die Grundfläche eines platzierten Objekts als (Y-)rotiertes, skaliertes Rechteck im
// 2D-View - fällt auf den einfachen Punkt-Marker zurück, wenn keine Grundfläche berechnet
// werden konnte. Nur die Y-Rotation aus dem Quaternion wird berücksichtigt (Kippung um X/Z ist
// bei platzierten Objekten praktisch immer 0, siehe ObjectPlacement.hpp) - für die 2D-
// Draufsicht ausreichend.
// Weltpolygon der Grundflaeche eines Objekts (konvexe Huelle der untersten Modellschicht, sonst
// Bounding-Box), gedreht/skaliert/verschoben wie das Objekt im 3D-View.
std::vector<std::pair<float, float>> ObjectFootprintWorldPolygon(EditorState& state, const core::PlacedObject& obj);
std::pair<std::size_t, std::size_t> CountFootprintCells(core::WalkGrid& scratch, const core::WalkGrid& reference,
                                                               const std::vector<std::pair<float, float>>& polygon);
const std::unordered_map<std::string, std::pair<std::size_t, std::size_t>>& ModelBlockStats(EditorState& state);
bool ModelWalkableInSource(EditorState& state, const std::string& modelPath);
std::vector<std::vector<std::pair<float, float>>> CollectVisibleObjectFootprints(EditorState& state);
void StampObjectFootprints(EditorState& state, bool blocked);
std::string ObjectChangeKey(const core::PlacedObject& o);
void StampPolygonsBlocked(core::WalkGrid& grid, const std::vector<std::vector<std::pair<float, float>>>& polygons);
void ComputeWalkProposal(EditorState& state);
void ApplyWalkProposal(EditorState& state);
void UpdateWalkDiffTexture(EditorState& state);
void DrawWalkGeneratePanel(EditorState& state);
std::vector<core::NifGroundContactSegment> ObjectGroundContactWorldSegments(
    EditorState& state, const core::PlacedObject& obj);
float DistanceToObjectGroundContactXZ(EditorState& state, const core::PlacedObject& obj,
                                      float worldX, float worldZ);
bool DrawObjectFootprint2D(EditorState& state, const core::PlacedObject& obj, ImDrawList* drawList,
                           const ImVec2& cursorScreenPos, const ImVec2& imageSize,
                           float spanX, float spanZ, ImU32 color, float thickness);
void RefreshRoamOverlayRoutes(EditorState& state);
void DrawRoamRoutes2D(EditorState& state, ImDrawList* drawList,
                      const ImVec2& mapOrigin, const ImVec2& mapSize,
                      float spanX, float spanZ);
std::vector<std::pair<float, float>> ObjectFootprintWorldPolygon(EditorState& state, const core::PlacedObject& obj);
bool FocusLoadedNifSceneFromBounds(EditorState& state);
void DrawWalkFootprintPreview(EditorState& state, ImDrawList* drawList,
                                     const ImVec2& mapOrigin, const ImVec2& mapSize,
                                     float spanX, float spanZ);
void DrawMinimapPreviewContent(EditorState& state);
// MapViewInfo-Zeile der offenen Karte (MapName oder MapFolderName passt).
struct MapViewRow {
    int document = -1;
    std::size_t row = 0;
    std::string folder;       // MapFolderName = Dateiname der Minimap
    bool minimapView = false; // MinimapView
    core::legacy::MinimapView view;
};
std::optional<MapViewRow> FindMapViewRow(EditorState& state, EditorState::ShnSource source);
std::filesystem::path GameMinimapPath(EditorState& state, const std::string& folder, bool* fromProject = nullptr);
std::vector<std::uint8_t> RenderMinimapRgba(EditorState& state, std::uint32_t size, bool withObjects);
std::vector<std::uint8_t> MinimapWalkMask(const EditorState& state, std::uint32_t size);
bool GenerateMinimap(EditorState& state, std::vector<std::string>& report);
void ProcessPendingMinimap(EditorState& state);
// Offscreen-Aufnahme der Szene (eigenes Render-Ziel); Zeile 0 = obere Bildkante.
std::vector<std::uint8_t> CaptureSceneRgba(EditorState& state, const app::OrbitCamera& cam, std::uint32_t size,
                                           bool withObjects, bool withSky, bool withFog);
std::vector<std::uint8_t> CaptureSceneRgba(EditorState& state, const app::OrbitCamera& cam, std::uint32_t width,
                                           std::uint32_t height, bool withObjects, bool withSky, bool withFog);
// Datei relativ zu resmenu: Projekt-Override vor Client (Groß-/Kleinschreibung egal); leer = fehlt.
std::filesystem::path GameResmenuPath(EditorState& state, const std::string& rel, bool* fromProject = nullptr);
// Stadtportal-Bilder <MapName>.dds und _disable.dds für ein Ziel (Weltposition x / Server-y) ins Projekt.
bool GenerateTownPortalImages(EditorState& state, const std::string& mapName, float x, float y,
                              std::vector<std::string>& report);
// Item-/Skill-Icons (IconTools.cpp).
void EnsureItemIcons(EditorState& state);
std::optional<std::pair<std::string, int>> ItemIconForInx(EditorState& state, const std::string& inx);
std::optional<std::pair<std::string, int>> ItemIconForId(EditorState& state, long long id);
bool DrawAtlasIcon(EditorState& state, const std::string& iconFile, int index, float size);
bool DrawItemIconByInx(EditorState& state, const std::string& inx, float size);
bool DrawItemIconById(EditorState& state, long long id, float size);
bool DrawIconAtlasPicker(EditorState& state, const std::string& iconFile, int& index);
bool ScatterAtWorld(EditorState& state, float worldX, float worldZ);
std::filesystem::path PrefabDir(const EditorState& state);
bool PlacePrefabAtWorld(EditorState& state, float worldX, float worldZ);
void DrawPrefabControls(EditorState& state);
bool TextureLayerLocked(const EditorState& state, int layer);
void DrawSplineControls(EditorState& state);
void DrawSpline2D(const EditorState& state, ImDrawList* drawList, const ImVec2& origin, const ImVec2& size, float spanX, float spanZ);
void FinishScatterStrokeIfReleased(EditorState& state);
void DrawScatterControls(EditorState& state);
bool TextureLayerLocked(const EditorState& state, int layer);
bool LoadBrushAlpha(EditorState& state, const std::filesystem::path& file);
void DrawBrushAlphaControls(EditorState& state, const char* id);
void DrawTextureRuleControls(EditorState& state);
bool ApplyBrushAtWorld(EditorState& state, float worldX, float worldZ);
void DrawEditor2DContent(EditorState& state);
std::optional<EditorState::RoamOverlayRoute> LoadRoamOverlayRoute(
    const EditorState& state, const std::string& name);
void RefreshRoamOverlayRoutes(EditorState& state);
void DrawRoamRoutes2D(EditorState& state, ImDrawList* drawList,
                      const ImVec2& mapOrigin, const ImVec2& mapSize,
                      float spanX, float spanZ);
void DrawRoamRoutes3D(EditorState& state, const ImVec2& imagePos, int w, int h);
void DrawNpcOverlay3D(EditorState& state, const ImVec2& imagePos, int w, int h);
bool InvertEditMat4(const app::Mat4& m, app::Mat4& out);
std::optional<EditVec3> Unproject3D(const EditorState& state, const ImVec2& imagePos,
                                    int w, int h, const ImVec2& mouse, float ndcZ);
std::optional<EditVec3> PickTerrainFrom3D(EditorState& state, const ImVec2& imagePos,
                                          int w, int h, const ImVec2& mouse);
int PlaceObjectAtWorld(EditorState& state, const std::string& modelPath, float x, float y, float z);
bool ProjectWorldTo3DView(const EditorState& state, const ImVec2& imagePos, int w, int h,
                          const EditVec3& world, ImVec2& screen);
float ActiveWorldBrushRadius(const EditorState& state);
ImU32 ActiveBrushColor(const EditorState& state, int alpha);
void Draw3DBrushOverlay(EditorState& state, const ImVec2& imagePos, int w, int h, bool hovered);
void DrawMobZones3D(EditorState& state, const ImVec2& imagePos, int w, int h);
void DrawPortals3D(EditorState& state, const ImVec2& imagePos, int w, int h);
std::optional<EditVec3> VertexSnapTarget(EditorState& state, const ImVec2& imagePos, int w, int h, const ImVec2& mouse);
std::string CurrentGizmoSelectionKey(const EditorState& state);
bool DrawObjectTransformGizmo(EditorState& state, const ImVec2& imageScreenPos, int w, int h);
// ---- Unreal-ähnliche Viewport-Leiste --------------------------------------------------------
// Links: Ansicht ▾ / Anzeigen ▾ / Transform-Modi / World-Local / Surface-, Grid-, Winkel- und
// Skalierungs-Snap. Rechts: Kameratempo, Spielansicht, Spieltest. Alles editorseitig.
namespace viewport_ui {

inline void Tooltip(const char* text) {
    if (text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", text);
}

inline bool ToggleChip(const char* label, bool* value, const char* tooltip) {
    const bool on = *value;
    if (on) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(18, 96, 150, 245));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(26, 120, 186, 255));
    }
    const bool clicked = ImGui::SmallButton(label);
    if (on) ImGui::PopStyleColor(2);
    Tooltip(tooltip);
    if (clicked) *value = !*value;
    return clicked;
}

// Die Editorschrift enthält keine Pfeil-/Symbolglyphen (▾ ▶ ■) - Symbole werden gezeichnet.
enum class ButtonGlyph { DropDown, Play, Stop };

inline bool GlyphButton(const char* label, ButtonGlyph glyph) {
    const bool leading = glyph != ButtonGlyph::DropDown;
    // Abstand für das gezeichnete Symbol muss VOR einem "##id"-Suffix stehen, sonst ist er unsichtbar.
    const std::string_view full(label);
    const std::size_t idPos = full.find("##");
    const std::string visible(full.substr(0, idPos));
    const std::string idSuffix = idPos == std::string_view::npos ? std::string() : std::string(full.substr(idPos));
    const std::string text = (leading ? "    " + visible : visible + "    ") + idSuffix;
    const bool clicked = ImGui::SmallButton(text.c_str());
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cy = (mn.y + mx.y) * 0.5f;
    const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
    if (glyph == ButtonGlyph::DropDown) {
        const float cx = mx.x - 10.0f;
        dl->AddTriangleFilled(ImVec2(cx - 4.0f, cy - 2.0f), ImVec2(cx + 4.0f, cy - 2.0f), ImVec2(cx, cy + 3.0f), col);
    } else if (glyph == ButtonGlyph::Play) {
        const float cx = mn.x + 11.0f;
        dl->AddTriangleFilled(ImVec2(cx - 3.0f, cy - 5.0f), ImVec2(cx - 3.0f, cy + 5.0f), ImVec2(cx + 5.0f, cy), col);
    } else {
        const float cx = mn.x + 11.0f;
        dl->AddRectFilled(ImVec2(cx - 4.0f, cy - 4.0f), ImVec2(cx + 4.0f, cy + 4.0f), col);
    }
    return clicked;
}

inline std::string FormatSnap(float v, const char* suffix) {
    char buf[32];
    if (std::abs(v - std::round(v)) < 1.0e-4f) std::snprintf(buf, sizeof(buf), "%.0f%s", v, suffix);
    else std::snprintf(buf, sizeof(buf), "%.3g%s", v, suffix);
    return buf;
}

// Snap-Schalter + Wert-Dropdown (Presets in Fiesta-Einheiten, siehe core::level).
inline bool SnapCombo(const char* id, const char* label, bool* enabled, float* value, std::span<const float> presets,
               const char* suffix, const char* tooltip) {
    bool changed = false;
    ImGui::PushID(id);
    const std::string current = FormatSnap(*value, suffix);
    changed |= ToggleChip(label, enabled, tooltip);
    ImGui::SameLine(0.0f, 1.0f);
    ImGui::SetNextItemWidth(std::max(46.0f, ImGui::CalcTextSize(current.c_str()).x + 26.0f));
    // Gleiche Höhe wie SmallButton (FramePadding.y = 0), damit die Leiste bündig bleibt.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 0.0f));
    const bool comboOpen = ImGui::BeginCombo("##snapValue", current.c_str(), ImGuiComboFlags_HeightLarge);
    ImGui::PopStyleVar();
    if (comboOpen) {
        for (const float preset : presets) {
            const std::string label = FormatSnap(preset, suffix);
            if (ImGui::Selectable(label.c_str(), std::abs(preset - *value) < 1.0e-5f)) {
                *value = preset;
                *enabled = true;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

}
void DrawViewportShowFlagsMenu(EditorState& state);
void DrawViewportViewMenu(EditorState& state);
bool DrawViewportToolbar(EditorState& state, const ImVec2& imageScreenPos, float width);
std::optional<EditVec3> PickViewportGround(EditorState& state, const ImVec2& imagePos, int w, int h,
                                           const ImVec2& mouse);
void DrawViewportGrid(EditorState& state, const ImVec2& imagePos, int w, int h);
void DrawWalkCollision3D(EditorState& state, const ImVec2& imagePos, int w, int h, float centerX, float centerZ);
void DrawViewportAxisIndicator(EditorState& state, const ImVec2& imagePos, int h);
void DrawViewportStats(EditorState& state, const ImVec2& imagePos, int w);
void DrawPlaytestOverlay(EditorState& state, const ImVec2& imagePos, int w, int h, bool showHud = true);
void UpdatePlaytest(EditorState& state, bool viewportHovered);
std::vector<int> CollectMarqueeHits(EditorState& state, const ImVec2& imagePos, int w, int h,
                                    const core::level::ScreenRect& rect);
void DrawPreview3DContent(EditorState& state);
// ---- Mehrere 3D-Viewports -------------------------------------------------------------------
template <class F>
void ForEachViewportField(EditorState& state, EditorState::ViewportSlot& v, F&& f) {
    f(state.camera, v.camera);
    f(state.viewportViewPreset, v.viewPreset);
    f(state.cameraLooking, v.cameraLooking);
    f(state.viewportViewMode, v.viewMode);
    f(state.wireframe, v.wireframe);
    f(state.showTerrain, v.showTerrain);
    f(state.showObjectMeshes, v.showObjectMeshes);
    f(state.showObjectMarkers, v.showObjectMarkers);
    f(state.showNpcModels, v.showNpcModels);
    f(state.showViewportGrid, v.showGrid);
    f(state.showViewportStats, v.showStats);
    f(state.showWalkCollision3D, v.showWalkCollision);
    f(state.gameView, v.gameView);
    f(state.marqueeActive, v.marqueeActive);
    f(state.marqueeStart, v.marqueeStart);
    f(state.viewportCursorWorld, v.cursorWorld);
    f(state.viewport3dPos, v.pos);
    f(state.viewport3dSize, v.size);
}
void SwapViewportState(EditorState& state, EditorState::ViewportSlot& v);
void SetActiveViewport(EditorState& state, int index);
void InitializeViewportSlots(EditorState& state);
const char* ViewportLayoutName(int layout);
void SetViewportLayout(EditorState& state, int layout);
void DrawViewportLayout(EditorState& state);
void DrawWorkspaceTabBar(EditorState& state);
void DrawSceneOutlinerPanel(EditorState& state);
void DrawLayerManagerPanel(EditorState& state);
const char* NifTextureSlotLabel(std::size_t slot);
const char* NifTextureTransformOperationLabel(std::uint32_t operation);
const char* NifTextureInterpolationLabel(std::uint32_t interpolation);
const char* NifTextureExtrapolationLabel(std::uint8_t extrapolation);
void LoadNifAssetInspector(EditorState& state, const std::filesystem::path& path,
                           const std::string& relativeAsset);
std::optional<std::filesystem::path> ResolveNifInspectorTexturePath(
    const std::filesystem::path& root, const std::string& relativeAsset,
    const std::string& textureName);
std::optional<std::filesystem::path> ResolveNifInspectorTexturePathCached(
    EditorState& state, const std::filesystem::path& root,
    const std::string& textureName);
EditorState::AssetThumbnail GetOrLoadEmbeddedNifInspectorThumbnail(
    EditorState& state, const core::NifEmbeddedTexture& image,
    std::size_t partIndex, std::size_t slotIndex);
void DrawNifAssetInspector(EditorState& state, const std::filesystem::path& root);
int FindShineColumn(const core::legacy::ShineTable& table, const std::string& name);
std::string ShineRecordValue(const core::legacy::ShineRecord& record, int column);
bool IsDropUnsignedInteger(const std::string& value);
void EnsureDropTableLoaded(EditorState& state);
void EnsureDropGroupCatalog(EditorState& state);
bool SaveDropTable(EditorState& state);
bool EditDropTableValue(const char* id, std::string& value, float width = 0.0f);
void DrawDropTableEditor(EditorState& state);
std::filesystem::path InterfaceProjectOverrideRoot(const EditorState& state);
std::filesystem::path InterfaceProjectOverridePath(const EditorState& state, const std::string& rel);
bool IsRegularFileNoThrow(const std::filesystem::path& path);
enum class InterfaceOverrideState {
    None = 0,
    Identical = 1,
    Modified = 2,
    ProjectOnly = 3,
};
bool BinaryFilesEqualNoThrow(const std::filesystem::path& a, const std::filesystem::path& b);
InterfaceOverrideState InspectInterfaceOverride(const EditorState& state,
                                                const std::filesystem::path& sourceRoot,
                                                const std::string& rel);
InterfaceOverrideState CachedInterfaceOverrideState(const EditorState& state,
                                                    const std::string& rel);
void RefreshInterfaceOverrideState(EditorState& state, const std::string& rel);
void RebuildInterfaceOverrideStates(EditorState& state, const std::filesystem::path& sourceRoot);
std::filesystem::path InterfaceEffectiveAssetPath(const EditorState& state,
                                                  const std::filesystem::path& sourceRoot,
                                                  const std::string& rel);
void InvalidateInterfaceAssetPreview(EditorState& state,
                                     const std::filesystem::path& sourcePath,
                                     const std::filesystem::path& projectPath);
bool CreateInterfaceProjectOverride(EditorState& state,
                                    const std::filesystem::path& sourcePath,
                                    const std::string& rel);
bool RemoveInterfaceProjectOverride(EditorState& state,
                                    const std::filesystem::path& sourcePath,
                                    const std::string& rel);
std::pair<std::size_t, std::size_t> RemoveIdenticalInterfaceOverrides(
    EditorState& state, const std::filesystem::path& sourceRoot);
void DrawInterfaceWorkspace(EditorState& state);
void DrawWorkspaceAssetBrowser(EditorState& state);
void DrawOutputLogPanel(EditorState& state);
void DrawMapEditorWorkspace(EditorState& state);
// ---- Automatisierung für Screenshots / visuelle Regression ------------------------------------
// NEXTGEN_EDITOR_SCRIPT="open tests/fixtures/Rou.ini; wait 30; mode objects; screenshot a.ppm; quit"
// Befehle werden der Reihe nach ausgeführt (je Frame höchstens einer, "wait N" pausiert N Frames).
// Nur für Entwicklung/CI gedacht; ohne Umgebungsvariable ist die Funktion vollständig inaktiv.
struct AutomationScript {
    std::deque<std::string> commands;
    int waitFrames = 0;
    std::string pendingScreenshot;
    bool quitRequested = false;
};
extern GLFWwindow* g_automationWindow;
AutomationScript LoadAutomationScriptFromEnvironment();
void RunAutomationStep(EditorState& state, AutomationScript& script);
void WriteFramebufferPpm(const std::string& file, int w, int h);

} // namespace nge
