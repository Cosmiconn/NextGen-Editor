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
void Check(bool condition, const char* what) { if (!condition) { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; } else std::printf("[ok]     %s\n", what); }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("(Test übersprungen - Aufruf mit: %s <pfad/zu/karte.ini>)\n", argv[0]); return 0; }
    const std::filesystem::path iniPath = argv[1];
    const std::string stem = iniPath.stem().string();
    std::printf("== Karte '%s' vollständig öffnen ==\n", stem.c_str());
    legacy::LegacyMapOpenReport openReport;
    auto project = legacy::OpenLegacyMap(iniPath, &openReport);
    Check(project.has_value(), "OpenLegacyMap erfolgreich");
    if (!project) { std::fprintf(stderr, "Fehler: %s\n", project.error().c_str()); return 1; }
    for (const auto& issue : openReport.issues) std::printf("         Hinweis: %s\n", issue.c_str());
    Check(project->hasHeightmap, "Heightmap wurde gefunden und geladen");
    Check(project->textureStack.LayerCount() > 0, "Mindestens ein Textur-Layer wurde geladen");
    Check(project->hasWalkGrid, "Block&Walk-Gitter wurde gefunden und geladen");
    Check(project->hasObjects, "Objekt-Placement wurde gefunden und geladen");
    const auto outDir = std::filesystem::temp_directory_path() / "map_editor_project_roundtrip_test";
    std::filesystem::remove_all(outDir);
    auto saveResult = legacy::SaveLegacyMap(*project, outDir, stem);
    Check(saveResult.has_value(), "SaveLegacyMap erfolgreich");
    if (!saveResult) return 1;
    legacy::LegacyMapOpenReport reopenReport;
    auto reopened = legacy::OpenLegacyMap(outDir / (stem + ".ini"), &reopenReport);
    Check(reopened.has_value(), "Erneutes Öffnen erfolgreich");
    if (!reopened) return 1;
    Check(reopened->heightmap.Width() == project->heightmap.Width() && reopened->heightmap.Height() == project->heightmap.Height(), "Heightmap-Dimensionen nach Rundlauf identisch");
    bool heightsMatch = project->heightmap.Data().size() == reopened->heightmap.Data().size();
    for (std::size_t i = 0; i < project->heightmap.Data().size() && heightsMatch; ++i) heightsMatch = project->heightmap.Data()[i] == reopened->heightmap.Data()[i];
    Check(heightsMatch, "Heightmap-Werte nach Rundlauf byte-exakt identisch");
    Check(reopened->textureStack.LayerCount() == project->textureStack.LayerCount(), "Textur-Layer-Anzahl nach Rundlauf identisch");
    Check(reopened->textureStack.Width() == project->textureStack.Width() && reopened->textureStack.Height() == project->textureStack.Height(), "Textur-Auflösung nach Rundlauf identisch");
    Check(reopened->walkGrid.Width() == project->walkGrid.Width() && reopened->walkGrid.Height() == project->walkGrid.Height(), "Block&Walk-Dimensionen nach Rundlauf identisch");
    Check(reopened->objects.Count() == project->objects.Count(), "Objekt-Anzahl nach Rundlauf identisch");
    if (reopened->objects.Count() == project->objects.Count() && project->objects.Count() > 0) { const auto& a = project->objects.At(0); const auto& b = reopened->objects.At(0); Check(std::abs(a.posX-b.posX)+std::abs(a.posY-b.posY)+std::abs(a.posZ-b.posZ) < 0.01f, "Erstes Objekt: Position nach Rundlauf identisch"); }
    std::filesystem::remove_all(outDir);
    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
