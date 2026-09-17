#include "mapeditor/core/legacy/LegacyMapProject.hpp"

#include "mapeditor/core/ObjectPlacementIO.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"
#include "mapeditor/core/legacy/LegacyTextureSetIO.hpp"

#include <algorithm>

namespace theseed::mapeditor::core::legacy {

namespace {

void AddIssue(LegacyMapOpenReport* report, const std::string& msg) {
    if (report != nullptr) report->issues.push_back(msg);
}

} // namespace

std::expected<LegacyMapProject, std::string> OpenLegacyMap(
    const std::filesystem::path& iniPath, LegacyMapOpenReport* report) {

    auto iniResult = ParseLegacyMapIni(iniPath);
    if (!iniResult) {
        return std::unexpected(iniResult.error());
    }

    LegacyMapProject project;
    project.ini = *iniResult;

    const std::filesystem::path mapDir = iniPath.parent_path();
    const std::string stem = iniPath.stem().string();

    // --- Heightmap: primär über #HeightFileName aus der ini auflösen (deckt den Fall ab, dass
    // der Dateiname vom ini-Stamm abweicht, z.B. RouVal01 -> "darkVally.HTD"). ---
    std::optional<std::filesystem::path> htdPath;
    if (!project.ini.heightFileName.empty()) {
        htdPath = ResolveLegacyAssetPath(mapDir, project.ini.heightFileName);
    }
    if (!htdPath) {
        htdPath = FindSiblingFileByStem(mapDir, stem, ".htd");
    }
    if (htdPath && project.ini.heightmapWidth > 0 && project.ini.heightmapHeight > 0) {
        auto hmResult = ImportLegacyHtd(
            *htdPath, project.ini.heightmapWidth, project.ini.heightmapHeight,
            project.ini.oneBlockWidth, project.ini.oneBlockHeight, &project.htdHeader, &project.htdTrailingBytes);
        if (hmResult) {
            project.heightmap = std::move(*hmResult);
            project.hasHeightmap = true;
        } else {
            AddIssue(report, "Heightmap (.HTD): " + hmResult.error());
        }
    } else {
        AddIssue(report, "Keine .HTD-Datei gefunden (weder \u00fcber HeightFileName noch \u00fcber Namens-Konvention) - diese Karte nutzt m\u00f6glicherweise ein anderes Format, siehe docs/MAP_FORMAT.md (z.B. 'Eld').");
    }

    // --- Texturing: nutzt bereits eigene, robuste Pfadaufl\u00f6sung (siehe LegacyTextureSetIO). ---
    TextureSetImportReport texReport;
    auto texResult = ImportLegacyTextureSet(iniPath, &texReport);
    if (texResult) {
        project.textureStack = std::move(*texResult);
    } else {
        AddIssue(report, "Texturing: " + texResult.error());
    }
    for (const auto& msg : texReport.missingBlendFiles) {
        AddIssue(report, "Texturing: " + msg);
    }

    // --- Block&Walk: Aufl\u00f6sung bevorzugt DIREKT aus dem Datei-Header lesen (zweites
    // Header-Feld = tats\u00e4chliche Gitterh\u00f6he, verifiziert an 4 echten Karten - robuster als
    // eine reine Formel-Herleitung, siehe docs/MAP_FORMAT.md). Formel als Fallback, falls der
    // Header nicht lesbar/plausibel ist. WICHTIGE KORREKTUR: das Gitter ist NICHT quadratisch
    // (fr\u00fchere Annahme war falsch, siehe Changelog v0.11.0). ---
    if (auto shbdPath = FindSiblingFileByStem(mapDir, stem, ".shbd")) {
        std::uint32_t gridWidth = 0, gridHeight = 0;

        auto headerInfo = PeekLegacyShbdHeader(*shbdPath);
        if (headerInfo && headerInfo->height > 0) {
            std::error_code sizeEc;
            const auto fileSize = std::filesystem::file_size(*shbdPath, sizeEc);
            if (!sizeEc && fileSize > 8) {
                const std::uint64_t totalElements = (fileSize - 8) / 2;
                if (totalElements % headerInfo->height == 0) {
                    gridHeight = headerInfo->height;
                    gridWidth = static_cast<std::uint32_t>(totalElements / headerInfo->height);
                }
            }
        }
        if (gridWidth == 0 || gridHeight == 0) {
            // Fallback: Breite = QuadsBreite/2, H\u00f6he = QuadsBreite*8 (siehe SyncWalkGridSize
            // in main.cpp f\u00fcr dieselbe Formel/Herleitung).
            const std::uint32_t quadsW = project.ini.heightmapWidth > 1 ? project.ini.heightmapWidth - 1 : 0;
            gridWidth = quadsW / 2;
            gridHeight = quadsW * 8;
        }

        if (gridWidth > 0 && gridHeight > 0) {
            auto walkResult = ImportLegacyShbd(*shbdPath, gridWidth, gridHeight, &project.shbdHeader);
            if (walkResult) {
                project.walkGrid = std::move(*walkResult);
                project.hasWalkGrid = true;
            } else {
                AddIssue(report, "Block&Walk (.shbd): " + walkResult.error());
            }
        }
    } else {
        AddIssue(report, "Keine .shbd-Datei neben der ini gefunden.");
    }

    // --- Objekt-Placement. ---
    if (auto shmdPath = FindSiblingFileByStem(mapDir, stem, ".shmd")) {
        auto shmdResult = ParseLegacyShmd(*shmdPath);
        if (shmdResult) {
            project.objects = std::move(*shmdResult);
            project.hasObjects = true;
        } else {
            AddIssue(report, "Objekt-Placement (.shmd): " + shmdResult.error());
        }
    } else {
        AddIssue(report, "Keine .shmd-Datei neben der ini gefunden.");
    }

    // --- R\u00e4umlicher Index (optional, reiner Pass-Through). ---
    if (auto idmPath = FindSiblingFileByStem(mapDir, stem, ".idm")) {
        auto idmResult = ParseLegacyIdm(*idmPath);
        if (idmResult) {
            project.spatialIndex = std::move(*idmResult);
            project.hasSpatialIndex = true;
        } else {
            AddIssue(report, "R\u00e4umlicher Index (.idm): " + idmResult.error());
        }
    }

    // --- Zonen-Metadaten (optional). ---
    if (auto aidPath = FindSiblingFileByStem(mapDir, stem, ".aid")) {
        auto aidResult = ParseLegacyAid(*aidPath);
        if (aidResult) {
            project.zone = std::move(*aidResult);
            project.hasZone = true;
        } else {
            AddIssue(report, "Zonen-Metadaten (.aid): " + aidResult.error());
        }
    }

    return project;
}

std::expected<void, std::string> SaveLegacyMap(
    LegacyMapProject& project, const std::filesystem::path& outDir, const std::string& mapStem) {

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec) {
        return std::unexpected("Konnte Ausgabeverzeichnis nicht anlegen: " + outDir.string() + " (" + ec.message() + ")");
    }

    // Heightmap-Dimensionsfelder der ini müssen immer zur aktuellen Heightmap passen (kann sich
    // durch "Neu"/Resize seit dem letzten Parse geändert haben).
    if (project.hasHeightmap) {
        project.ini.heightmapWidth = project.heightmap.Width();
        project.ini.heightmapHeight = project.heightmap.Height();
        project.ini.oneBlockWidth = project.heightmap.BlockWidth();
        project.ini.oneBlockHeight = project.heightmap.BlockHeight();

        auto htdResult = ExportLegacyHtd(project.heightmap, outDir / (mapStem + ".HTD"), project.htdHeader, project.htdTrailingBytes);
        if (!htdResult) {
            return std::unexpected(htdResult.error());
        }
        project.ini.heightFileName = ".\\" + mapStem + ".HTD";
    }

    // ini.layers an den aktuellen TextureLayerStack angleichen (Layer können in der GUI
    // hinzugefügt/entfernt worden sein) - neue Layer bekommen einen abgeleiteten Blend-Dateinamen,
    // damit ExportLegacyTextureSet einen gültigen Zielpfad hat.
    if (project.ini.layers.size() != project.textureStack.LayerCount()) {
        std::vector<LegacyLayerDef> newLayers;
        newLayers.reserve(project.textureStack.LayerCount());
        for (std::size_t i = 0; i < project.textureStack.LayerCount(); ++i) {
            if (i < project.ini.layers.size()) {
                newLayers.push_back(project.ini.layers[i]);
            } else {
                LegacyLayerDef def;
                def.name = project.textureStack.Layer(i).name;
                def.diffuseFileName = project.textureStack.Layer(i).diffuseFileName;
                def.blendFileName = ".\\" + mapStem + "_layer" + std::to_string(i) + ".bmp";
                def.width = static_cast<float>(project.textureStack.Width());
                def.height = static_cast<float>(project.textureStack.Height());
                def.uvScaleDiffuse = project.textureStack.Layer(i).uvScaleDiffuse;
                def.uvScaleBlend = 1.0f;
                newLayers.push_back(def);
            }
        }
        project.ini.layers = std::move(newLayers);
    }

    // Texturing (schreibt auch die .ini - siehe ExportLegacyTextureSet).
    auto texResult = ExportLegacyTextureSet(project.textureStack, project.ini, outDir, mapStem + ".ini");
    if (!texResult) {
        return std::unexpected(texResult.error());
    }

    if (project.hasWalkGrid) {
        auto r = ExportLegacyShbd(project.walkGrid, outDir / (mapStem + ".shbd"), project.shbdHeader);
        if (!r) return std::unexpected(r.error());
    }
    if (project.hasObjects) {
        auto r = SerializeLegacyShmd(project.objects, outDir / (mapStem + ".shmd"));
        if (!r) return std::unexpected(r.error());
    }
    if (project.hasSpatialIndex) {
        auto r = SerializeLegacyIdm(project.spatialIndex, outDir / (mapStem + ".idm"));
        if (!r) return std::unexpected(r.error());
    }
    if (project.hasZone) {
        auto r = SerializeLegacyAid(project.zone, outDir / (mapStem + ".aid"));
        if (!r) return std::unexpected(r.error());
    }

    return {};
}

} // namespace theseed::mapeditor::core::legacy
