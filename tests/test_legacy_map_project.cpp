// test_legacy_map_project.cpp
// Integrationstest für den vereinheitlichten "Karte öffnen/speichern"-Workflow: öffnet eine
// komplette echte Kartendatei (alle vier Module auf einmal), speichert sie, öffnet die
// gespeicherte Version erneut und vergleicht Kernwerte. Läuft nur, wenn ein Pfad zu einer
// echten .ini als Kommandozeilenargument übergeben wird (z.B. .../field/Bera/bera.ini) - die
// hochgeladenen Kartensets sind nicht Teil des Repos.

#include "mapeditor/core/legacy/LegacyMapProject.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace theseed::mapeditor::core;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "[FEHLER] %s\n", what);
        ++g_failures;
    } else {
        std::printf("[ok]     %s\n", what);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("(Test \u00fcbersprungen - Aufruf mit: %s <pfad/zu/karte.ini>)\n", argv[0]);
        return 0;
    }

    const std::filesystem::path iniPath = argv[1];
    const std::string stem = iniPath.stem().string();

    std::printf("== Karte '%s' vollst\u00e4ndig \u00f6ffnen ==\n", stem.c_str());
    legacy::LegacyMapOpenReport openReport;
    auto project = legacy::OpenLegacyMap(iniPath, &openReport);
    Check(project.has_value(), "OpenLegacyMap erfolgreich");
    if (!project) {
        std::fprintf(stderr, "Fehler: %s\n", project.error().c_str());
        return 1;
    }
    for (const auto& issue : openReport.issues) {
        std::printf("         Hinweis: %s\n", issue.c_str());
    }

    std::printf("Heightmap: %s (%ux%u)\n", project->hasHeightmap ? "geladen" : "FEHLT",
                project->heightmap.Width(), project->heightmap.Height());
    std::printf("Texturing: %zu Layer, %ux%u\n", project->textureStack.LayerCount(),
                project->textureStack.Width(), project->textureStack.Height());
    std::printf("Block&Walk: %s (%ux%u)\n", project->hasWalkGrid ? "geladen" : "FEHLT",
                project->walkGrid.Width(), project->walkGrid.Height());
    std::printf("Objekt-Placement: %s (%zu Objekte, %zu Kategorien)\n", project->hasObjects ? "geladen" : "FEHLT",
                project->objects.Count(), project->objects.categories.size());
    std::printf("R\u00e4umlicher Index: %s\n", project->hasSpatialIndex ? "geladen" : "nicht vorhanden");
    std::printf("Zonen-Metadaten: %s\n", project->hasZone ? ("geladen ('" + project->zone.name + "')").c_str() : "nicht vorhanden");

    Check(project->hasHeightmap, "Heightmap wurde gefunden und geladen");
    Check(project->textureStack.LayerCount() > 0, "Mindestens ein Textur-Layer wurde geladen");
    Check(project->hasWalkGrid, "Block&Walk-Gitter wurde gefunden und geladen");
    Check(project->hasObjects, "Objekt-Placement wurde gefunden und geladen");

    // --- Speichern + erneut öffnen ---
    const auto outDir = std::filesystem::temp_directory_path() / "map_editor_project_roundtrip_test";
    std::filesystem::remove_all(outDir);

    std::printf("\n== Speichern nach %s ==\n", outDir.string().c_str());
    auto saveResult = legacy::SaveLegacyMap(*project, outDir, stem);
    Check(saveResult.has_value(), "SaveLegacyMap erfolgreich");
    if (!saveResult) {
        std::fprintf(stderr, "Fehler: %s\n", saveResult.error().c_str());
        return 1;
    }

    std::printf("\n== Gespeicherte Karte erneut \u00f6ffnen ==\n");
    legacy::LegacyMapOpenReport reopenReport;
    auto reopened = legacy::OpenLegacyMap(outDir / (stem + ".ini"), &reopenReport);
    Check(reopened.has_value(), "Erneutes \u00d6ffnen erfolgreich");
    if (!reopened) {
        std::fprintf(stderr, "Fehler: %s\n", reopened.error().c_str());
        return 1;
    }
    for (const auto& issue : reopenReport.issues) {
        std::printf("         Hinweis: %s\n", issue.c_str());
    }

    Check(reopened->heightmap.Width() == project->heightmap.Width() &&
              reopened->heightmap.Height() == project->heightmap.Height(),
          "Heightmap-Dimensionen nach Rundlauf identisch");
    {
        bool heightsMatch = true;
        const auto a = project->heightmap.Data();
        const auto b = reopened->heightmap.Data();
        for (std::size_t i = 0; i < a.size() && heightsMatch; ++i) {
            if (a[i] != b[i]) heightsMatch = false;
        }
        Check(heightsMatch, "Heightmap-Werte nach Rundlauf byte-exakt identisch");
    }

    Check(reopened->textureStack.LayerCount() == project->textureStack.LayerCount(),
          "Textur-Layer-Anzahl nach Rundlauf identisch");
    Check(reopened->textureStack.Width() == project->textureStack.Width() &&
              reopened->textureStack.Height() == project->textureStack.Height(),
          "Textur-Auflösung nach Rundlauf identisch");

    Check(reopened->walkGrid.Width() == project->walkGrid.Width() &&
              reopened->walkGrid.Height() == project->walkGrid.Height(),
          "Block&Walk-Dimensionen nach Rundlauf identisch");

    Check(reopened->objects.Count() == project->objects.Count(),
          "Objekt-Anzahl nach Rundlauf identisch");
    if (reopened->objects.Count() == project->objects.Count() && project->objects.Count() > 0) {
        const auto& a = project->objects.At(0);
        const auto& b = reopened->objects.At(0);
        const float diff = std::abs(a.posX - b.posX) + std::abs(a.posY - b.posY) + std::abs(a.posZ - b.posZ);
        Check(diff < 0.01f, "Erstes Objekt: Position nach Rundlauf identisch");
    }

    std::filesystem::remove_all(outDir);

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
