// test_nif_model.cpp
// GUI-freier Test für den NIF-Parser. Läuft nur, wenn Pfade zu echten .nif-Dateien als
// Kommandozeilenargumente übergeben werden (die Kartensets sind nicht Teil des Repos).
// Aufruf: test_nif_model <einfache_untexturierte.nif> <texturierte.nif, sollte fehlschlagen>

#include "mapeditor/core/NifModel.hpp"

#include <algorithm>
#include <cmath>
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

    if (simple) {
        std::printf("\n== NIF-Node-Hierarchie ==\n");
        bool parentIndicesValid = true;
        bool localTransformsFinite = true;
        for (const auto& node : simple->nodes) {
            if (node.parentIndex < -1 ||
                (node.parentIndex >= 0 && static_cast<std::size_t>(node.parentIndex) >= simple->nodes.size()))
                parentIndicesValid = false;
            localTransformsFinite = localTransformsFinite &&
                std::isfinite(node.localTranslation.x) &&
                std::isfinite(node.localTranslation.y) &&
                std::isfinite(node.localTranslation.z) &&
                std::isfinite(node.localScale);
            for (const float v : node.localRotation) localTransformsFinite = localTransformsFinite && std::isfinite(v);
        }
        Check(parentIndicesValid, "Alle NIF-Node-Parent-Indizes liegen innerhalb der exportierten Hierarchie");
        Check(localTransformsFinite, "Lokale NIF-Bind-Transforms sind endlich und für KF-Preview nutzbar");
    }

    if (simple) {
        std::printf("\n== UV-Sets und eingebettete Materialtexturen ==\n");
        bool uvSetsSane = true;
        bool embeddedSlotsValid = true;
        std::size_t embeddedSlotCount = 0;
        for (const auto& part : simple->parts) {
            for (const auto& uvSet : part.uvSets) {
                for (const auto& uv : uvSet) {
                    if (!std::isfinite(uv.u) || !std::isfinite(uv.v) ||
                        std::abs(uv.u) > 1000.0f || std::abs(uv.v) > 1000.0f) {
                        uvSetsSane = false;
                    }
                }
            }
            for (const auto& slot : part.textureSlots) {
                if (!slot.embeddedTexture) continue;
                ++embeddedSlotCount;
                const auto& tex = *slot.embeddedTexture;
                if (tex.width == 0 || tex.height == 0 ||
                    tex.rgba.size() != static_cast<std::size_t>(tex.width) * tex.height * 4) {
                    embeddedSlotsValid = false;
                }
            }
        }
        Check(uvSetsSane, "Alle erhaltenen UV-Sets sind endlich/plausibel oder wurden verworfen");
        Check(embeddedSlotsValid, "Alle eingebetteten Materialslot-Texturen sind vollständig dekodiert");
        std::printf("     Eingebettete Materialslots: %zu, PixelData dekodiert/nicht dekodiert: %u/%u\n",
                    embeddedSlotCount, simple->decodedEmbeddedTextures, simple->undecodedEmbeddedTextures);
    }

    if (simple) {
        // Konsistenz (seit [0.44.30]): kein Dreiecksindex hinter dem letzten Vertex eines Parts
        // (vorher bei Modellen mit mehreren Detailstufen: Vertices eines Blocks + Indizes aller).
        bool consistent = true;
        for (const auto& part : simple->parts) for (const auto idx : part.triangleIndices) if (idx >= part.positions.size()) consistent = false;
        Check(consistent, "Alle Dreiecksindizes liegen innerhalb der Vertices des Parts");
    }

    if (simple) {
        // Szenengraph (seit [0.44.28]): Knoten-Transformationen werden angewendet - ein Baum darf
        // nicht mehr tief unter seinem Ursprung haengen (vorher tree05: Y -1104..-10).
        std::printf("\n== Szenengraph-Transformationen ==\n");
        float minY = 1e30f;
        for (const auto& part : simple->parts) for (const auto& v : part.positions) minY = std::min(minY, v.y);
        Check(minY > -50.0f, "Modell haengt nicht unter dem Ursprung (Knoten-Transformationen angewendet)");
    }

    if (simple) {
        std::printf("\n== Grundflaechen-Huelle (ComputeFootprintHull) ==\n");
        const auto hull = ComputeFootprintHull(*simple);
        Check(hull.size() >= 3, "Huelle hat mindestens 3 Punkte");
        double area = 0.0;
        for (std::size_t i = 0; i < hull.size(); ++i) {
            const auto& p = hull[i]; const auto& q = hull[(i + 1) % hull.size()];
            area += static_cast<double>(p.first) * q.second - static_cast<double>(q.first) * p.second;
        }
        float minX = 1e30f, maxX = -1e30f, minZ = 1e30f, maxZ = -1e30f;
        for (const auto& part : simple->parts) for (const auto& v : part.positions) {
            minX = std::min(minX, v.x); maxX = std::max(maxX, v.x); minZ = std::min(minZ, v.z); maxZ = std::max(maxZ, v.z);
        }
        const double boxArea = static_cast<double>(maxX - minX) * (maxZ - minZ);
        Check(area > 0.0 && area / 2.0 <= boxArea * 1.0001, "Huelle liegt (gegen den Uhrzeigersinn) innerhalb der Bounding-Box");
    }

    for (int arg = 2; arg < argc; ++arg) {
        std::printf("\n== Zusätzliche NIF-Regression: %s ==\n", argv[arg]);
        auto extra = LoadNifMesh(argv[arg]);
        Check(extra.has_value(), "Zusätzliche NIF-Datei wird vollständig geladen");
        if (!extra) {
            std::fprintf(stderr, "     Fehler: %s\n", extra.error().c_str());
            continue;
        }

        bool geometryValid = !extra->parts.empty();
        bool uvSetsSane = true;
        bool embeddedSlotsValid = true;
        std::size_t embeddedSlots = 0;
        for (const auto& part : extra->parts) {
            if (part.positions.empty() || part.triangleIndices.empty() ||
                part.triangleIndices.size() % 3 != 0) {
                geometryValid = false;
            }
            for (const auto& uvSet : part.uvSets) {
                for (const auto& uv : uvSet) {
                    if (!std::isfinite(uv.u) || !std::isfinite(uv.v) ||
                        std::abs(uv.u) > 1000.0f || std::abs(uv.v) > 1000.0f) {
                        uvSetsSane = false;
                    }
                }
            }
            for (const auto& slot : part.textureSlots) {
                if (!slot.sourceUsesEmbeddedPixelData) continue;
                if (slot.embeddedTexture) {
                    ++embeddedSlots;
                    const auto& tex = *slot.embeddedTexture;
                    if (tex.width == 0 || tex.height == 0 ||
                        tex.rgba.size() != static_cast<std::size_t>(tex.width) * tex.height * 4) {
                        embeddedSlotsValid = false;
                    }
                }
            }
        }
        Check(geometryValid, "Zusätzliche NIF-Datei liefert gültige Geometrie");
        Check(uvSetsSane, "Zusätzliche NIF-Datei enthält nur plausible erhaltene UV-Sets");
        Check(embeddedSlotsValid, "Zusätzliche eingebettete Materialslots sind vollständig dekodiert");
        std::printf("     Parts=%zu, Embedded-Slots=%zu, PixelData dekodiert/nicht dekodiert=%u/%u\n",
                    extra->parts.size(), embeddedSlots,
                    extra->decodedEmbeddedTextures, extra->undecodedEmbeddedTextures);
    }

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
