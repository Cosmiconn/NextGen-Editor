#include "UiIconAssets.hpp"

#include "NifMeshRenderer.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace theseed::mapeditor::app {
namespace {

struct IconPathEntry {
    std::string_view semantic;
    std::string_view groupDir;
    std::string_view fileName;
};

// Keep this table deliberately boring and explicit. The semantic IDs are frozen in
// assets/ui/icons/icon-map.json and docs/ui-vision/ICON_INVENTORY.md.
constexpr std::array kIconPaths{
    IconPathEntry{"file.new",             "01_toolbar_icons",    "01_neu.png"},
    IconPathEntry{"file.open",            "01_toolbar_icons",    "02_oeffnen.png"},
    IconPathEntry{"file.save",            "01_toolbar_icons",    "03_speichern.png"},
    IconPathEntry{"history.undo",         "01_toolbar_icons",    "04_rueckgaengig.png"},
    IconPathEntry{"history.redo",         "01_toolbar_icons",    "05_wiederholen.png"},
    IconPathEntry{"transform.select",     "01_toolbar_icons",    "06_auswaehlen.png"},
    IconPathEntry{"transform.move",       "01_toolbar_icons",    "07_verschieben.png"},
    IconPathEntry{"transform.rotate",     "01_toolbar_icons",    "08_rotieren.png"},
    IconPathEntry{"transform.scale",      "01_toolbar_icons",    "09_skalieren.png"},
    IconPathEntry{"tool.brush",           "01_toolbar_icons",    "10_pinsel.png"},
    IconPathEntry{"world.terrain",        "01_toolbar_icons",    "11_terrain.png"},
    IconPathEntry{"world.layers",         "01_toolbar_icons",    "12_layer.png"},
    IconPathEntry{"world.objects",        "01_toolbar_icons",    "13_objekte.png"},
    IconPathEntry{"gameplay.block_walk",  "01_toolbar_icons",    "14_block_and_walk.png"},
    IconPathEntry{"gameplay.collision",   "01_toolbar_icons",    "15_kollision.png"},
    IconPathEntry{"gameplay.path",        "01_toolbar_icons",    "16_pfad.png"},
    IconPathEntry{"world.light",          "01_toolbar_icons",    "17_licht.png"},
    IconPathEntry{"system.playtest",      "01_toolbar_icons",    "18_play-test.png"},

    IconPathEntry{"nav.world",            "02_navigation_icons", "01_welt.png"},
    IconPathEntry{"nav.terrain",          "02_navigation_icons", "02_terrain.png"},
    IconPathEntry{"nav.paint",            "02_navigation_icons", "03_malen.png"},
    IconPathEntry{"nav.layers",           "02_navigation_icons", "04_ebenen.png"},
    IconPathEntry{"nav.objects",          "02_navigation_icons", "05_objekte.png"},
    IconPathEntry{"nav.water",            "02_navigation_icons", "06_wasser.png"},
    IconPathEntry{"nav.sky",              "02_navigation_icons", "07_himmel.png"},
    IconPathEntry{"nav.flora",            "02_navigation_icons", "08_flora.png"},
    IconPathEntry{"nav.lighting",         "02_navigation_icons", "09_beleuchtung.png"},
    IconPathEntry{"nav.rendering",        "02_navigation_icons", "10_rendering.png"},
    IconPathEntry{"nav.npcs",             "02_navigation_icons", "11_npcs.png"},
    IconPathEntry{"nav.points",           "02_navigation_icons", "12_punkte.png"},
    IconPathEntry{"nav.spawns",           "02_navigation_icons", "13_spawnpunkte.png"},
    IconPathEntry{"nav.trigger",          "02_navigation_icons", "14_trigger.png"},
    IconPathEntry{"nav.events",           "02_navigation_icons", "15_events.png"},

    IconPathEntry{"panel.project",        "03_panel_icons",      "01_projekt.png"},
    IconPathEntry{"panel.asset_browser",  "03_panel_icons",      "02_asset_browser.png"},
    IconPathEntry{"panel.outliner",       "03_panel_icons",      "03_objektliste.png"},
    IconPathEntry{"panel.properties",     "03_panel_icons",      "04_eigenschaften.png"},
    IconPathEntry{"panel.minimap",        "03_panel_icons",      "05_minimap.png"},
    IconPathEntry{"panel.tools",          "03_panel_icons",      "06_werkzeuge.png"},
    IconPathEntry{"panel.search",         "03_panel_icons",      "07_suche.png"},
    IconPathEntry{"panel.filter",         "03_panel_icons",      "08_filter.png"},
    IconPathEntry{"panel.settings",       "03_panel_icons",      "09_einstellungen.png"},
    IconPathEntry{"panel.help",           "03_panel_icons",      "10_hilfe.png"},
    IconPathEntry{"panel.validation",     "03_panel_icons",      "11_verifizierung.png"},
    IconPathEntry{"panel.export",         "03_panel_icons",      "12_export.png"},

    IconPathEntry{"module.shn.single",     "04_extra_icons",      "01_shn_editor.png"},
    IconPathEntry{"module.shn.multi",      "04_extra_icons",      "02_multi_shn.png"},
    IconPathEntry{"module.quest",          "04_extra_icons",      "03_quest_editor.png"},
    IconPathEntry{"module.skill",          "04_extra_icons",      "04_skill_editor.png"},
    IconPathEntry{"module.interface",      "04_extra_icons",      "05_interface_editor.png"},
    IconPathEntry{"module.droptable",      "04_extra_icons",      "06_droptable.png"},
    IconPathEntry{"module.custom_npc",     "04_extra_icons",      "07_custom_npc.png"},
    IconPathEntry{"module.custom_mob",     "04_extra_icons",      "08_custom_mob.png"},
    IconPathEntry{"brand.ng",              "04_extra_icons",      "09_ng_icon.png"},

    IconPathEntry{"view.2d",               "05_additional_ui_icons", "01_2d.png"},
    IconPathEntry{"view.3d",               "05_additional_ui_icons", "02_3d.png"},
    IconPathEntry{"module.kfm",            "05_additional_ui_icons", "03_kfm.png"},
    IconPathEntry{"module.ai",             "05_additional_ui_icons", "04_ai.png"},
    IconPathEntry{"module.xp",             "05_additional_ui_icons", "05_xp.png"},
    IconPathEntry{"module.prices",         "05_additional_ui_icons", "06_preise.png"},
    IconPathEntry{"state.visibility",      "05_additional_ui_icons", "07_visibility_eye.png"},
    IconPathEntry{"state.lock",            "05_additional_ui_icons", "08_lock.png"},
    IconPathEntry{"state.unlock",          "05_additional_ui_icons", "09_unlock.png"},
    IconPathEntry{"edit.copy",             "05_additional_ui_icons", "10_copy.png"},
    IconPathEntry{"edit.duplicate",        "05_additional_ui_icons", "11_duplicate.png"},
    IconPathEntry{"edit.delete",           "05_additional_ui_icons", "12_delete.png"},
    IconPathEntry{"system.command_palette","05_additional_ui_icons", "13_command_palette.png"},
    IconPathEntry{"system.recent_projects","05_additional_ui_icons", "14_recent_projects.png"},
};

constexpr std::array kSupportedSizes{16, 24, 32, 48, 64, 128};

std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len > 0 && len < buffer.size()) {
        buffer.resize(len);
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    std::error_code ec;
    return std::filesystem::current_path(ec);
}

} // namespace

UiIconAssets::~UiIconAssets() {
    Shutdown();
}

int UiIconAssets::NormalizeSize(int requestedSize) {
    int best = kSupportedSizes.front();
    int bestDistance = std::abs(requestedSize - best);
    for (const int size : kSupportedSizes) {
        const int distance = std::abs(requestedSize - size);
        if (distance < bestDistance) {
            best = size;
            bestDistance = distance;
        }
    }
    return best;
}

std::filesystem::path UiIconAssets::FindAssetRoot() {
    if (const char* explicitRoot = std::getenv("NEXTGEN_UI_ASSET_ROOT"); explicitRoot && *explicitRoot) {
        const std::filesystem::path p(explicitRoot);
        if (std::filesystem::exists(p / "png")) return p;
    }

    const auto exeDir = ExecutableDirectory();
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    const std::array candidates{
        exeDir / "assets" / "ui" / "icons",
        exeDir / ".." / "assets" / "ui" / "icons",
        cwd / "assets" / "ui" / "icons",
        cwd / ".." / "assets" / "ui" / "icons",
    };
    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate / "png")) {
            return std::filesystem::weakly_canonical(candidate, ec);
        }
    }
    return {};
}

bool UiIconAssets::Init() {
    Shutdown();
    assetRoot_ = FindAssetRoot();
    return !assetRoot_.empty();
}

void UiIconAssets::Shutdown() {
    for (auto& [key, texture] : textures_) {
        if (texture != 0) glDeleteTextures(1, &texture);
    }
    textures_.clear();
    warnedUnknownSemantics_.clear();
    assetRoot_.clear();
}

bool UiIconAssets::IsKnownSemantic(std::string_view semanticId) {
    return std::any_of(kIconPaths.begin(), kIconPaths.end(),
                       [semanticId](const IconPathEntry& entry) {
                           return entry.semantic == semanticId;
                       });
}

std::filesystem::path UiIconAssets::RelativePath(std::string_view semanticId, int size) {
    for (const auto& entry : kIconPaths) {
        if (entry.semantic == semanticId) {
            return std::filesystem::path("png") / std::to_string(size) /
                   std::filesystem::path(entry.groupDir) / std::filesystem::path(entry.fileName);
        }
    }
    return {};
}

std::uint32_t UiIconAssets::Texture(std::string_view semanticId, int requestedSize) {
    if (!IsKnownSemantic(semanticId)) {
        const std::string semantic(semanticId);
        if (warnedUnknownSemantics_.insert(semantic).second) {
            std::fprintf(stderr,
                         "[UiIconAssets] Unbekannte semantische Icon-ID: %s\n",
                         semantic.c_str());
        }
        return 0;
    }
    if (assetRoot_.empty()) return 0;

    const int preferredSize = NormalizeSize(requestedSize);
    const std::string cacheKey = std::string(semanticId) + "#" + std::to_string(preferredSize);
    if (const auto it = textures_.find(cacheKey); it != textures_.end()) {
        return it->second;
    }

    // A checkout may intentionally contain only the runtime sizes that have already been
    // migrated. Prefer the exact size, then gracefully fall back to the nearest available
    // approved export. On equal distance prefer the larger source so ImGui downsamples it.
    auto candidates = kSupportedSizes;
    std::sort(candidates.begin(), candidates.end(), [preferredSize](int a, int b) {
        const int da = std::abs(a - preferredSize);
        const int db = std::abs(b - preferredSize);
        if (da != db) return da < db;
        return a > b;
    });

    std::filesystem::path file;
    for (const int candidateSize : candidates) {
        const auto relative = RelativePath(semanticId, candidateSize);
        if (relative.empty()) break;
        const auto candidate = assetRoot_ / relative;
        if (std::filesystem::exists(candidate)) {
            file = candidate;
            break;
        }
    }
    if (file.empty()) {
        textures_.emplace(cacheKey, 0);
        return 0;
    }

    auto image = LoadPlatformRasterImage(file);
    if (!image || image->width == 0 || image->height == 0 ||
        image->rgba.size() != static_cast<std::size_t>(image->width) * image->height * 4) {
        textures_.emplace(cacheKey, 0);
        return 0;
    }

    std::uint32_t texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(image->width), static_cast<GLsizei>(image->height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image->rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    textures_.emplace(cacheKey, texture);
    return texture;
}

} // namespace theseed::mapeditor::app
