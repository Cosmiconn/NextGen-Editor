// Fenster-Layouts: Elemente einer NIF verschieben/skalieren, ohne die Datei neu aufzubauen
// (core::PatchNifPartRect). Geprüft an Fixture-NIFs aus tests/fixtures.
#include "mapeditor/core/NifPatch.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

using namespace theseed::mapeditor::core;

static void Require(bool ok, const char* what) {
    if (ok) return;
    std::cerr << "[FEHLER] " << what << "\n";
    std::exit(1);
}

static std::vector<std::uint8_t> ReadAll(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static bool Near(float a, float b, float tol = 0.01f) { return std::abs(a - b) <= tol; }

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: test_nif_patch <NiTriShapeData.nif> <NiTriStripsData.nif>\n";
        return 2;
    }
    const auto bytes = ReadAll(argv[1]);
    Require(!bytes.empty(), "Fixture lesen");
    auto layout = LoadNifByteLayout(bytes);
    Require(layout.has_value(), "strikter Standardpfad liefert die Byte-Lage");
    std::size_t part = layout->model.parts.size();
    for (std::size_t p = 0; p < layout->model.parts.size(); ++p) {
        const auto r = NifPartRect(layout->model.parts[p]);
        if (CheckNifPartPatchable(*layout, p).patchable && r.width > 1.0f && r.height > 1.0f) {
            part = p;
            break;
        }
    }
    Require(part < layout->model.parts.size(), "ein verschiebbares Element vorhanden");
    const auto& data = layout->blocks[static_cast<std::size_t>(layout->model.parts[part].dataBlock)];
    Require(data.type == "NiTriShapeData" && data.vertexOffset > 0 && data.boundOffset > data.vertexOffset, "Byte-Lage des Datenblocks");
    const NifRect original = NifPartRect(layout->model.parts[part]);

    // Verschieben: gleiche Dateigröße, nur Vertex- und Bound-Bytes geändert, Ziel erreicht, Rest gleich.
    NifRect moved = original;
    moved.x += 12.5f;
    moved.y -= 4.0f;
    auto out = PatchNifPartRect(bytes, part, moved);
    Require(out.has_value(), "Verschieben gelingt");
    Require(out->size() == bytes.size(), "Dateigröße bleibt gleich");
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if ((*out)[i] == bytes[i]) continue;
        const bool inVerts = i >= data.vertexOffset && i < data.vertexOffset + std::size_t{12} * data.vertexCount;
        const bool inBound = i >= data.boundOffset && i < data.boundOffset + 16;
        Require(inVerts || inBound, "nur Vertex- und Bound-Bytes ändern sich");
    }
    auto reloaded = LoadNifByteLayout(*out);
    Require(reloaded.has_value(), "verschobene Datei lädt strikt");
    const NifRect now = NifPartRect(reloaded->model.parts[part]);
    Require(Near(now.x, moved.x) && Near(now.y, moved.y) && Near(now.width, original.width) && Near(now.height, original.height),
            "Element liegt im Zielrechteck");
    for (std::size_t q = 0; q < layout->model.parts.size(); ++q) {
        if (q == part) continue;
        const auto& a = layout->model.parts[q].positions;
        const auto& b = reloaded->model.parts[q].positions;
        Require(a.size() == b.size(), "andere Elemente unverändert (Anzahl)");
        for (std::size_t i = 0; i < a.size(); ++i) Require(a[i].x == b[i].x && a[i].y == b[i].y && a[i].z == b[i].z, "andere Elemente unverändert");
    }

    // Skalieren auf das 1,5-Fache.
    NifRect scaled = original;
    scaled.width *= 1.5f;
    scaled.height *= 1.5f;
    auto big = PatchNifPartRect(bytes, part, scaled);
    Require(big.has_value(), "Skalieren gelingt");
    auto bigLayout = LoadNifByteLayout(*big);
    Require(bigLayout.has_value(), "skalierte Datei lädt strikt");
    const NifRect bigRect = NifPartRect(bigLayout->model.parts[part]);
    Require(Near(bigRect.x, scaled.x) && Near(bigRect.y, scaled.y) && Near(bigRect.width, scaled.width, 0.05f) &&
                Near(bigRect.height, scaled.height, 0.05f),
            "skaliertes Element hat die Zielgröße");

    // Zurückschieben ergibt das Ausgangsrechteck.
    auto back = PatchNifPartRect(*out, part, original);
    Require(back.has_value(), "Zurückschieben gelingt");
    auto backLayout = LoadNifByteLayout(*back);
    Require(backLayout.has_value(), "zurückgeschobene Datei lädt strikt");
    const NifRect backRect = NifPartRect(backLayout->model.parts[part]);
    Require(Near(backRect.x, original.x, 1e-3f) && Near(backRect.y, original.y, 1e-3f), "Hin und zurück ergibt das Ausgangsrechteck");

    // NiTriStripsData wird nicht angefasst.
    const auto strips = ReadAll(argv[2]);
    auto stripsLayout = LoadNifByteLayout(strips);
    Require(stripsLayout.has_value() && !stripsLayout->model.parts.empty(), "Strips-Fixture lädt strikt");
    for (std::size_t p = 0; p < stripsLayout->model.parts.size(); ++p)
        Require(!CheckNifPartPatchable(*stripsLayout, p).patchable, "Strips-Elemente sind nicht verschiebbar");
    Require(!PatchNifPartRect(strips, 0, NifPartRect(stripsLayout->model.parts[0])).has_value(), "Patch eines Strips-Elements wird abgelehnt");

    // Beschädigte Datei: kein strikter Lauf, also kein Patch.
    auto broken = bytes;
    broken.pop_back();
    Require(!LoadNifByteLayout(broken).has_value(), "abgeschnittene Datei liefert keine Byte-Lage");
    Require(!PatchNifPartRect(broken, part, moved).has_value(), "abgeschnittene Datei wird nicht gepatcht");

    std::cout << "NIF-Elemente verschieben/skalieren: ok\n";
    return 0;
}
