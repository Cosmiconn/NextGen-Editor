// test_nif_model.cpp
// GUI-freier Test für den NIF-Parser. Läuft nur, wenn Pfade zu echten .nif-Dateien als
// Kommandozeilenargumente übergeben werden (die Kartensets sind nicht Teil des Repos).
// Aufruf: test_nif_model <einfache_untexturierte.nif> <texturierte.nif, sollte fehlschlagen>

#include "mapeditor/core/NifModel.hpp"

#include <cstdio>

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
        std::printf("(Test \u00fcbersprungen - Aufruf mit: %s <einfache.nif> [<texturierte.nif>])\n", argv[0]);
        return 0;
    }

    std::printf("== Einfaches, untexturiertes Mesh laden ==\n");
    auto simple = LoadNifMesh(argv[1]);
    Check(simple.has_value(), "LoadNifMesh(einfache Datei) erfolgreich");
    if (simple) {
        std::printf("     root='%s', %zu Teil(e)\n", simple->rootName.c_str(), simple->parts.size());
        Check(!simple->parts.empty(), "Mindestens ein Mesh-Teil vorhanden");
        if (!simple->parts.empty()) {
            const auto& part = simple->parts.front();
            Check(!part.positions.empty(), "Vertex-Positionen vorhanden");
            Check(!part.triangleIndices.empty(), "Dreiecks-Indizes vorhanden");
            Check(part.triangleIndices.size() % 3 == 0, "Dreiecks-Indexanzahl ist durch 3 teilbar");
            bool indicesInRange = true;
            for (auto idx : part.triangleIndices) {
                if (idx >= part.positions.size()) indicesInRange = false;
            }
            Check(indicesInRange, "Alle Dreiecks-Indizes zeigen auf gültige Vertices");
            if (simple->parts.front().embeddedDiffuseTexture) {
                const auto& tex = *simple->parts.front().embeddedDiffuseTexture;
                Check(tex.width > 0 && tex.height > 0 &&
                      tex.rgba.size() == static_cast<std::size_t>(tex.width) * tex.height * 4,
                      "Eingebettete NiPixelData-Textur ist vollständig dekodiert");
            }
        }
    } else {
        std::fprintf(stderr, "     Fehler: %s\n", simple.error().c_str());
    }

    if (argc >= 3) {
        std::printf("\n== Zweite Testdatei (texturiert oder mit weiteren Blocktypen) ==\n");
        auto second = LoadNifMesh(argv[2]);
        if (second) {
            std::printf("     Erfolgreich geladen: %zu Teil(e)\n", second->parts.size());
            bool allValid = !second->parts.empty();
            for (const auto& part : second->parts) {
                if (part.positions.empty() || part.triangleIndices.empty() ||
                    part.triangleIndices.size() % 3 != 0) {
                    allValid = false;
                }
            }
            Check(allValid, "Zweite Datei: falls erfolgreich geladen, liefert sie gültige Geometrie");
        } else {
            std::printf("     Fehlermeldung (sauberes Scheitern, kein Absturz): %s\n", second.error().c_str());
            Check(true, "Zweite Datei: falls nicht unterstützt, schlägt sie sauber fehl (kein Absturz)");
        }
    }

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
