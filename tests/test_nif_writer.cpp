// test_nif_writer.cpp
// NIF-Writer v1: Texturname (andere Länge), Transformation und Material umschreiben, neu parsen,
// zurückschreiben -> bytegleich zum Original. Argumente: NIF-Dateien oder Ordner (rekursiv); Dateien,
// die nicht strikt laden, werden gezählt und übersprungen (an ihnen schreibt der Editor nie).

#include "mapeditor/core/NifWriter.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace theseed::mapeditor::core;

namespace {
int g_failures = 0;
void Check(bool ok, const std::string& what) {
    if (ok) return;
    std::fprintf(stderr, "[FEHLER] %s\n", what.c_str());
    ++g_failures;
}

std::vector<std::uint8_t> ReadAll(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Counts { int files = 0, strict = 0, textures = 0, transforms = 0, materials = 0, embedded = 0; };

void TestFile(const std::filesystem::path& path, Counts& n, bool verbose) {
    ++n.files;
    const auto bytes = ReadAll(path);
    const auto layout = LoadNifByteLayout(bytes);
    if (!layout) return;
    ++n.strict;
    const std::string name = path.filename().string();

    // Texturname: länger, dann zurück.
    for (const auto& tex : NifTextureSources(*layout)) {
        if (!tex.external) {
            ++n.embedded;
            Check(!RewriteNifTextureFile(bytes, tex.block, "x.dds").has_value(), name + ": eingebettete Textur wird abgelehnt");
            continue;
        }
        const std::string longer = "NextGenWriterTest_" + tex.file + "_longer.dds";
        const auto changed = RewriteNifTextureFile(bytes, tex.block, longer);
        Check(changed.has_value(), name + ": Textur umbenennen (" + (changed ? std::string() : changed.error()) + ")");
        if (!changed) continue;
        Check(changed->size() == bytes.size() + longer.size() - tex.file.size(), name + ": Dateigröße wächst um die Namensdifferenz");
        const auto reread = LoadNifByteLayout(*changed);
        bool seen = false;
        if (reread)
            for (const auto& t : NifTextureSources(*reread)) seen = seen || (t.block == tex.block && t.file == longer);
        Check(seen, name + ": neuer Name wird wieder gelesen");
        const auto back = RewriteNifTextureFile(*changed, tex.block, tex.file);
        Check(back && *back == bytes, name + ": zurückbenannt = Original bytegleich");
        ++n.textures;
        if (n.textures > 2000) break;
    }
    // Transformation des ersten NiAVObject: Skalierung x2 und verschoben, dann zurück.
    for (std::uint32_t i = 0; i < layout->blocks.size(); ++i) {
        const auto& b = layout->blocks[i];
        if (b.transformOffset == 0) continue;
        NifTransform t;
        t.rotation = b.rotation;
        t.translation = b.translation;
        t.scale = b.scale;
        NifTransform moved = t;
        moved.scale = b.scale * 2.0f;
        moved.translation.x += 10.0f;
        const auto changed = RewriteNifTransform(bytes, i, moved);
        Check(changed && changed->size() == bytes.size(), name + ": Transformation schreiben (" + (changed ? std::string() : changed.error()) + ")");
        if (changed) {
            const auto back = RewriteNifTransform(*changed, i, t);
            Check(back && *back == bytes, name + ": Transformation zurück = Original bytegleich");
        }
        ++n.transforms;
        break;
    }
    // Material: Diffuse rot, dann zurück.
    for (std::uint32_t i = 0; i < layout->blocks.size(); ++i) {
        if (layout->blocks[i].materialOffset == 0) continue;
        const auto original = ReadNifMaterial(bytes, *layout, i);
        Check(original.has_value(), name + ": Material lesen");
        if (!original) break;
        auto red = *original;
        red.diffuse = {1.0f, 0.0f, 0.0f};
        red.alpha = 0.5f;
        const auto changed = RewriteNifMaterial(bytes, i, red);
        Check(changed.has_value(), name + ": Material schreiben (" + (changed ? std::string() : changed.error()) + ")");
        if (changed) {
            const auto back = RewriteNifMaterial(*changed, i, *original);
            Check(back && *back == bytes, name + ": Material zurück = Original bytegleich");
        }
        ++n.materials;
        break;
    }
    if (verbose) std::printf("  %s: ok\n", name.c_str());
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Aufruf: test_nif_writer <nif|ordner>...\n");
        return 2;
    }
    Counts n;
    for (int a = 1; a < argc; ++a) {
        const std::filesystem::path p = argv[a];
        std::error_code ec;
        if (std::filesystem::is_directory(p, ec)) {
            for (auto it = std::filesystem::recursive_directory_iterator(p, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                auto ext = it->path().extension().string();
                for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (it->is_regular_file(ec) && ext == ".nif") TestFile(it->path(), n, false);
            }
        } else {
            TestFile(p, n, true);
        }
    }
    // Ungültige Eingaben werden abgelehnt.
    Check(!RewriteNifTextureFile({}, 0, "a.dds").has_value(), "leere Datei wird abgelehnt");
    std::printf("NIF-Writer: %d Dateien, %d strikt lesbar, %d Texturen umbenannt (%d eingebettet abgelehnt), %d Transformationen, %d Materialien\n",
                n.files, n.strict, n.textures, n.embedded, n.transforms, n.materials);
    if (n.strict == 0) { std::fprintf(stderr, "[FEHLER] keine strikt lesbare Datei\n"); return 1; }
    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
