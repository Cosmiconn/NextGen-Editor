// test_avatar_preview.cpp
// Prueft die Avatar-Vorschau (CHANGELOG [0.44.30]): Zusammensetzen aus reschar/ (Koerper, Set, Gesicht,
// Haare) und CPU-Rendering. Aufruf: test_avatar_preview <reschar-Ordner> [resitem-Ordner]
// (ohne Argument wird nur die Fehlerbehandlung geprueft).

#include "mapeditor/core/AvatarPreview.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

using namespace theseed::mapeditor::core;

namespace {
int g_failures = 0;
void Check(bool ok, const char* what) {
    if (ok) std::printf("[ok]     %s\n", what);
    else { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; }
}
} // namespace

int main(int argc, char** argv) {
    std::printf("== AvatarPreview ==\n");
    Check(AvatarClassFolder(0) == "Fighter" && AvatarClassFolder(2) == "Cleric" && AvatarClassFolder(5) == "Sentinel", "Klassen-Ordnernamen");
    Check(AvatarClassFolder(1).empty(), "Archer (Klasse 1) hat keinen Ordner");
    {
        AvatarRequest r;
        r.classIdx = 1;
        auto m = BuildAvatarModel(r);
        Check(!m.has_value(), "Archer liefert eine Fehlermeldung statt eines Modells");
    }
    if (argc >= 2) {
        AvatarRequest r;
        r.charRoot = argv[1];
        if (argc >= 3) r.itemRoot = argv[2];
        r.classIdx = 0;
        r.male = true;
        r.faceShape = 2;
        r.items.push_back({"Body", "-", 0, "-"});
        auto m = BuildAvatarModel(r);
        Check(m.has_value(), "Fighter-m (Standardkoerper) wird zusammengesetzt");
        if (m) {
            Check(m->parts.size() >= 4, "Koerper (3 Teile) + Gesicht (Haut, Augen)");
            for (const auto& p : m->parts) {
                bool ok = true;
                for (const auto i : p.triangleIndices) if (i >= p.positions.size()) { ok = false; break; }
                if (!ok) { Check(false, "Teil mit Dreiecksindex hinter dem letzten Vertex"); break; }
            }
            const auto img = RenderAvatarModel(*m, 0.0f, 200, 300);
            Check(img.size() == 200u * 300u * 4u, "Renderpuffer hat die richtige Groesse");
            int painted = 0;
            for (std::size_t i = 0; i + 3 < img.size(); i += 4) if (img[i] != 24 || img[i + 1] != 28 || img[i + 2] != 36) ++painted;
            Check(painted > 2000, "Figur ist gezeichnet (nicht nur Hintergrund)");
        }
        // Charakter-NIF fuer die Anzeige aufbereiten (NPC-Modelle aus reschar/<Name>/<Name>.nif)
        {
            auto body = LoadNifMesh(std::filesystem::path(argv[1]) / "Fighter-m" / "Fighter-m.nif");
            Check(body.has_value(), "Fighter-m.nif laedt");
            if (body) {
                const std::size_t before = body->parts.size();
                SimplifyCharacterModel(*body);
                Check(body->parts.size() < before && body->parts.size() >= 3 && body->parts.size() <= 5, "SimplifyCharacterModel: Knochen-Huellen und Detailstufen-Duplikate entfernt");
                for (const auto& p : body->parts) if (p.diffuseTexture.empty()) { Check(false, "kein untexturierter Teil bleibt uebrig"); break; }
            }
        }
        if (m) {
            const NifModel nm = AvatarToNifModel(*m);
            Check(nm.parts.size() == m->parts.size(), "AvatarToNifModel: gleiche Anzahl Teile");
            bool tex = false;
            for (const auto& p : nm.parts) if (p.embeddedDiffuseTexture && p.embeddedDiffuseTexture->width > 0) tex = true;
            Check(tex, "AvatarToNifModel: Texturen als eingebettete Texturen uebernommen");
        }
    } else {
        std::printf("(Modell-Test uebersprungen - Aufruf mit: %s <reschar-Ordner> [resitem-Ordner])\n", argv[0]);
    }
    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
