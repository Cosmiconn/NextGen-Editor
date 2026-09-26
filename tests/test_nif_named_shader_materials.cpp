#include "mapeditor/core/NifModel.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace core = theseed::mapeditor::core;

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) {
        std::printf("[ok]     %s\n", what.c_str());
    } else {
        std::fprintf(stderr, "[FEHLER] %s\n", what.c_str());
        ++failures;
    }
}

std::optional<fs::path> FindFixture(const fs::path& root, const std::string& fileName) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(
             root, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        if (it->path().filename() == fileName) return it->path();
    }
    return std::nullopt;
}

struct ShaderExpectation {
    const char* shader = nullptr;
    std::vector<const char*> files;
    std::size_t parts = 0;
    std::size_t embeddedBase = 0;
    std::size_t externalBase = 0;
};

void CheckNamedShader(const fs::path& root, const ShaderExpectation& expected) {
    std::size_t partCount = 0;
    std::size_t embeddedBaseCount = 0;
    std::size_t externalBaseCount = 0;

    for (const char* fileName : expected.files) {
        const auto path = FindFixture(root, fileName);
        Check(path.has_value(), std::string("Fixture vorhanden: ") + fileName);
        if (!path) continue;

        const auto model = core::LoadNifMesh(*path, false);
        Check(model.has_value(), std::string("Fixture lädt vollständig: ") + fileName);
        if (!model) {
            std::fprintf(stderr, "         %s\n", model.error().c_str());
            continue;
        }

        std::size_t fileShaderParts = 0;
        for (const auto& part : model->parts) {
            if (part.shaderName != expected.shader) continue;
            ++fileShaderParts;
            ++partCount;

            Check(part.textureApplyMode == 2u,
                  std::string(fileName) + " / " + expected.shader +
                      ": APPLY_MODULATE bleibt erhalten");
            Check(part.textureSlots[0].present,
                  std::string(fileName) + " / " + expected.shader +
                      ": klassischer Base-Slot 0 vorhanden");

            bool extraClassicSlot = false;
            for (std::size_t slot = 1; slot < part.textureSlots.size(); ++slot)
                extraClassicSlot = extraClassicSlot || part.textureSlots[slot].present;
            Check(!extraClassicSlot,
                  std::string(fileName) + " / " + expected.shader +
                      ": keine unbelegten zusätzlichen klassischen Texturslots");
            Check(part.shaderTextureSlots.empty(),
                  std::string(fileName) + " / " + expected.shader +
                      ": Fixture enthält keine ShaderTexDesc");

            const auto& base = part.textureSlots[0];
            if (base.sourceUsesEmbeddedPixelData) {
                ++embeddedBaseCount;
                Check(base.embeddedTexture != nullptr,
                      std::string(fileName) + " / " + expected.shader +
                          ": Embedded-Base-PixelData dekodiert");
            } else {
                ++externalBaseCount;
                Check(!base.texture.empty(),
                      std::string(fileName) + " / " + expected.shader +
                          ": externer Base-Pfad bleibt erhalten");
            }
        }

        Check(fileShaderParts > 0,
              std::string(fileName) + ": erwarteter Shader " + expected.shader +
                  " ist an mindestens einem Mesh-Part gebunden");
    }

    Check(partCount == expected.parts,
          std::string(expected.shader) + ": erwartete Mesh-Part-Anzahl " +
              std::to_string(expected.parts) + ", gefunden " + std::to_string(partCount));
    Check(embeddedBaseCount == expected.embeddedBase,
          std::string(expected.shader) + ": erwartete Embedded-Base-Anzahl " +
              std::to_string(expected.embeddedBase) + ", gefunden " +
              std::to_string(embeddedBaseCount));
    Check(externalBaseCount == expected.externalBase,
          std::string(expected.shader) + ": erwartete externe Base-Anzahl " +
              std::to_string(expected.externalBase) + ", gefunden " +
              std::to_string(externalBaseCount));
}

void CheckHilite2Fixture(const fs::path& root) {
    constexpr const char* kFile = "MapLinkGate2.nif";
    const auto path = FindFixture(root, kFile);
    Check(path.has_value(), std::string("Fixture vorhanden: ") + kFile);
    if (!path) return;

    const auto model = core::LoadNifMesh(*path, false);
    Check(model.has_value(), std::string("Fixture lädt vollständig: ") + kFile);
    if (!model) {
        std::fprintf(stderr, "         %s\n", model.error().c_str());
        return;
    }

    std::size_t hilite2Parts = 0;
    for (const auto& part : model->parts)
        if (part.textureApplyMode == 4u) ++hilite2Parts;

    Check(hilite2Parts == 3u,
          std::string(kFile) +
              ": genau drei Mesh-Parts behalten APPLY_HILIGHT2 (mode 4)");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s <fixture-root>\n", argv[0]);
        return 2;
    }

    const fs::path root(argv[1]);
    Check(fs::is_directory(root), "Fixture-Root ist ein Verzeichnis");
    if (!fs::is_directory(root)) return 1;

    CheckNamedShader(root, ShaderExpectation{
        "FxSkinningBaseMap",
        {"EglackMad.nif", "M_MajesticLion.nif"},
        3u,
        3u,
        0u,
    });

    CheckNamedShader(root, ShaderExpectation{
        "NsPgToonNoAni",
        {"Female_Hat_Antler00.nif", "KingdomC00.nif", "Male_Hat_Antler00.nif"},
        44u,
        26u,
        18u,
    });

    CheckHilite2Fixture(root);

    std::printf("\n%d Fehler.\n", failures);
    return failures == 0 ? 0 : 1;
}
