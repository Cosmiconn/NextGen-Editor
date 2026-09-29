#include "mapeditor/core/NifModel.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace core = theseed::mapeditor::core;

namespace {

struct Candidate {
    fs::path root;
    fs::path path;
    fs::path relativePath;
    std::string reason;
    std::uint64_t score = 0;
};

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string Clean(std::string value) {
    for (char& ch : value) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    }
    return value;
}

bool IsNifAssetPath(const fs::path& path) {
    if (Lower(path.extension().string()) != ".nif") return false;
    if (path.filename().string().rfind("._", 0) == 0) return false;
    for (const auto& component : path) {
        if (Lower(component.string()) == "__macosx") return false;
    }
    return true;
}

bool ContainsAny(const std::string& value, std::initializer_list<std::string_view> needles) {
    for (const auto needle : needles) {
        if (value.find(needle) != std::string::npos) return true;
    }
    return false;
}

bool ShaderIs(const core::NifMeshPart& part, std::string_view wanted) {
    return Lower(part.shaderName) == Lower(std::string(wanted));
}

void Consider(std::map<std::string, Candidate>& best,
              const std::string& category,
              const fs::path& root,
              const fs::path& path,
              const fs::path& relativePath,
              std::uint64_t score,
              std::string reason) {
    if (score == 0) return;
    auto found = best.find(category);
    const std::string pathKey = path.generic_string();
    if (found != best.end()) {
        const std::string currentKey = found->second.path.generic_string();
        if (score < found->second.score || (score == found->second.score && pathKey >= currentKey))
            return;
    }
    best[category] = Candidate{root, path, relativePath, std::move(reason), score};
}

std::string Counts(std::initializer_list<std::pair<std::string_view, std::uint64_t>> values) {
    std::ostringstream out;
    bool first = true;
    for (const auto& [name, value] : values) {
        if (!first) out << "; ";
        first = false;
        out << name << '=' << value;
    }
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    bool strict = false;
    std::optional<fs::path> manifestPath;
    std::vector<fs::path> roots;

    for (int arg = 1; arg < argc; ++arg) {
        const std::string value = argv[arg];
        if (value == "--strict") {
            strict = true;
        } else if (value == "--manifest") {
            if (arg + 1 >= argc) {
                std::cerr << "--manifest requires a path\n";
                return 2;
            }
            manifestPath = fs::path(argv[++arg]);
        } else if (value == "--help" || value == "-h") {
            std::cout
                << "Usage: nif_visual_matrix [--strict] [--manifest <tsv>] <root> [root ...]\n"
                << "Selects deterministic real-NIF representatives for the ResMap visual QA matrix.\n";
            return 0;
        } else {
            roots.emplace_back(value);
        }
    }

    if (roots.empty()) {
        std::cerr << "No scan roots supplied.\n";
        return 2;
    }

    const std::vector<std::string> requiredCategories{
        "vegetation",
        "building_static",
        "alpha_cutout",
        "environment_water",
        "glow_emissive",
        "bump",
        "lod",
        "billboard",
        "texture_transform",
        "flip_controller",
        "particles_classic",
        "particles_mesh",
        "particles_world_space",
        "pgterrain",
        "alpha_texture_blender",
        "alpha_texture_blender11",
        "glass"
    };

    std::map<std::string, Candidate> best;
    std::size_t files = 0;
    std::size_t loaded = 0;
    std::size_t failed = 0;

    for (auto root : roots) {
        std::error_code ec;
        root = fs::absolute(root, ec);
        if (ec || !fs::is_directory(root)) {
            std::cerr << "Not a directory: " << root << '\n';
            return 2;
        }

        fs::recursive_directory_iterator it(
            root, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; it != end; it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec) || ec || !IsNifAssetPath(it->path())) {
                ec.clear();
                continue;
            }

            ++files;
            const auto model = core::LoadNifMesh(it->path(), false);
            if (!model) {
                ++failed;
                std::cerr << "LOADFAIL\tpath=" << Clean(it->path().string())
                          << "\treason=" << Clean(model.error()) << '\n';
                continue;
            }
            ++loaded;

            fs::path relative = fs::relative(it->path(), root, ec);
            if (ec) {
                ec.clear();
                relative = it->path().filename();
            }

            std::uint64_t triangles = 0;
            std::uint64_t alphaTestParts = 0;
            std::uint64_t alphaBlendParts = 0;
            std::uint64_t glowParts = 0;
            std::uint64_t bumpParts = 0;
            std::uint64_t lodParts = 0;
            std::uint64_t billboardParts = 0;
            std::uint64_t textureTransformControllers = 0;
            std::uint64_t authoredTextureTransforms = 0;
            std::uint64_t flipControllers = 0;
            std::uint64_t pgTerrainParts = 0;
            std::uint64_t alphaTextureBlenderParts = 0;
            std::uint64_t alphaTextureBlender11Parts = 0;
            std::uint64_t glassParts = 0;

            for (const auto& part : model->parts) {
                triangles += part.triangleIndices.size() / 3u;
                alphaTestParts += part.alphaTest ? 1u : 0u;
                alphaBlendParts += part.alphaBlend ? 1u : 0u;
                glowParts += part.textureSlots[4].present ? 1u : 0u;
                bumpParts += part.textureSlots[5].present ? 1u : 0u;
                lodParts += part.lodControlled ? 1u : 0u;
                billboardParts += part.billboard ? 1u : 0u;
                textureTransformControllers += part.textureTransformAnimations.size();
                flipControllers += part.textureFlipAnimations.size();
                for (const auto& slot : part.textureSlots)
                    authoredTextureTransforms += (slot.present && slot.hasTransform) ? 1u : 0u;
                pgTerrainParts += ShaderIs(part, "PgTerrain") ? 1u : 0u;
                alphaTextureBlender11Parts += ShaderIs(part, "AlphaTextureBlender11") ? 1u : 0u;
                alphaTextureBlenderParts += ShaderIs(part, "AlphaTextureBlender") ? 1u : 0u;
                glassParts += (part.glassShader.has_value() || ShaderIs(part, "Glass")) ? 1u : 0u;
            }

            std::uint64_t classicParticles = 0;
            std::uint64_t meshParticles = 0;
            std::uint64_t worldSpaceParticles = 0;
            for (const auto& system : model->particleSystems) {
                if (system.meshParticles) ++meshParticles;
                else ++classicParticles;
                if (system.worldSpace) ++worldSpaceParticles;
            }

            const std::uint64_t envSphere =
                static_cast<std::uint64_t>(model->textureEffectEnvironmentSphereBlocks);
            const std::string pathLower = Lower(relative.generic_string());
            const bool vegetationName = ContainsAny(pathLower, {
                "tree", "bush", "grass", "plant", "leaf", "leav", "flower",
                "fern", "palm", "weed", "mushroom", "shrub"
            });
            const bool buildingName = ContainsAny(pathLower, {
                "house", "building", "castle", "tower", "bridge", "wall", "gate",
                "temple", "church", "shop", "warehouse", "fort", "palace", "statue"
            });
            const bool waterName = ContainsAny(pathLower, {
                "water", "river", "lake", "fountain", "pond", "sea", "ocean"
            });

            if (vegetationName && triangles > 0) {
                Consider(best, "vegetation", root, it->path(), relative,
                         1000000000000000ull + alphaTestParts * 1000000000000ull + triangles,
                         Counts({{"nameKeyword", 1}, {"alphaTestParts", alphaTestParts}, {"triangles", triangles}}));
            }

            if (triangles > 0 && model->particleSystems.empty()) {
                const std::uint64_t semanticBonus = buildingName ? 1000000000000000ull : 0ull;
                Consider(best, "building_static", root, it->path(), relative,
                         semanticBonus + triangles,
                         Counts({{"buildingKeyword", buildingName ? 1u : 0u}, {"triangles", triangles}}));
            }

            if (alphaTestParts > 0) {
                Consider(best, "alpha_cutout", root, it->path(), relative,
                         alphaTestParts * 1000000000000ull + alphaBlendParts * 1000000000ull + triangles,
                         Counts({{"alphaTestParts", alphaTestParts}, {"alphaBlendParts", alphaBlendParts}, {"triangles", triangles}}));
            }

            if (envSphere > 0 || glassParts > 0) {
                const std::uint64_t waterBonus = waterName ? 100000000000000ull : 0ull;
                Consider(best, "environment_water", root, it->path(), relative,
                         envSphere * 1000000000000000ull + waterBonus + glassParts * 1000000000000ull + triangles,
                         Counts({{"environmentSphereBlocks", envSphere}, {"waterKeyword", waterName ? 1u : 0u}, {"glassParts", glassParts}}));
            }

            if (glowParts > 0) {
                Consider(best, "glow_emissive", root, it->path(), relative,
                         glowParts * 1000000000000ull + triangles,
                         Counts({{"glowParts", glowParts}, {"triangles", triangles}}));
            }

            if (bumpParts > 0) {
                Consider(best, "bump", root, it->path(), relative,
                         bumpParts * 1000000000000ull + triangles,
                         Counts({{"bumpParts", bumpParts}, {"triangles", triangles}}));
            }

            if (lodParts > 0) {
                Consider(best, "lod", root, it->path(), relative,
                         lodParts * 1000000000000ull + triangles,
                         Counts({{"lodParts", lodParts}, {"triangles", triangles}}));
            }

            if (billboardParts > 0) {
                Consider(best, "billboard", root, it->path(), relative,
                         billboardParts * 1000000000000ull + triangles,
                         Counts({{"billboardParts", billboardParts}, {"triangles", triangles}}));
            }

            if (textureTransformControllers > 0 || authoredTextureTransforms > 0) {
                Consider(best, "texture_transform", root, it->path(), relative,
                         textureTransformControllers * 1000000000000000ull +
                         authoredTextureTransforms * 1000000000000ull + triangles,
                         Counts({{"transformControllers", textureTransformControllers},
                                 {"authoredTransforms", authoredTextureTransforms},
                                 {"triangles", triangles}}));
            }

            if (flipControllers > 0) {
                Consider(best, "flip_controller", root, it->path(), relative,
                         flipControllers * 1000000000000ull + triangles,
                         Counts({{"flipControllers", flipControllers}, {"triangles", triangles}}));
            }

            if (classicParticles > 0) {
                Consider(best, "particles_classic", root, it->path(), relative,
                         classicParticles * 1000000000000ull + model->particleSystems.size(),
                         Counts({{"classicSystems", classicParticles},
                                 {"allParticleSystems", static_cast<std::uint64_t>(model->particleSystems.size())}}));
            }

            if (meshParticles > 0) {
                Consider(best, "particles_mesh", root, it->path(), relative,
                         meshParticles * 1000000000000ull + model->particleSystems.size(),
                         Counts({{"meshSystems", meshParticles},
                                 {"allParticleSystems", static_cast<std::uint64_t>(model->particleSystems.size())}}));
            }

            if (worldSpaceParticles > 0) {
                Consider(best, "particles_world_space", root, it->path(), relative,
                         worldSpaceParticles * 1000000000000ull + model->particleSystems.size(),
                         Counts({{"worldSpaceSystems", worldSpaceParticles},
                                 {"allParticleSystems", static_cast<std::uint64_t>(model->particleSystems.size())}}));
            }

            if (pgTerrainParts > 0) {
                Consider(best, "pgterrain", root, it->path(), relative,
                         pgTerrainParts * 1000000000000ull + triangles,
                         Counts({{"pgTerrainParts", pgTerrainParts}, {"triangles", triangles}}));
            }

            if (alphaTextureBlenderParts > 0) {
                Consider(best, "alpha_texture_blender", root, it->path(), relative,
                         alphaTextureBlenderParts * 1000000000000ull + triangles,
                         Counts({{"alphaTextureBlenderParts", alphaTextureBlenderParts}, {"triangles", triangles}}));
            }

            if (alphaTextureBlender11Parts > 0) {
                Consider(best, "alpha_texture_blender11", root, it->path(), relative,
                         alphaTextureBlender11Parts * 1000000000000ull + triangles,
                         Counts({{"alphaTextureBlender11Parts", alphaTextureBlender11Parts}, {"triangles", triangles}}));
            }

            if (glassParts > 0) {
                Consider(best, "glass", root, it->path(), relative,
                         glassParts * 1000000000000ull + triangles,
                         Counts({{"glassParts", glassParts}, {"triangles", triangles}}));
            }
        }
    }

    std::ostringstream table;
    table << "category\troot\tpath\treason\n";
    std::size_t missing = 0;
    for (const auto& category : requiredCategories) {
        const auto found = best.find(category);
        if (found == best.end()) {
            ++missing;
            std::cout << "MATRIX_MISSING\tcategory=" << category << '\n';
            continue;
        }
        const auto& candidate = found->second;
        table << category << '\t'
              << Clean(candidate.root.string()) << '\t'
              << Clean(candidate.relativePath.generic_string()) << '\t'
              << Clean(candidate.reason) << '\n';
        std::cout << "MATRIX\tcategory=" << category
                  << "\troot=" << Clean(candidate.root.string())
                  << "\tpath=" << Clean(candidate.relativePath.generic_string())
                  << "\treason=" << Clean(candidate.reason) << '\n';
    }

    if (manifestPath) {
        const auto parent = manifestPath->parent_path();
        if (!parent.empty()) fs::create_directories(parent);
        std::ofstream out(*manifestPath, std::ios::binary);
        if (!out) {
            std::cerr << "Cannot write manifest: " << *manifestPath << '\n';
            return 2;
        }
        out << table.str();
    }

    std::cout << "SUMMARY"
              << "\tfiles=" << files
              << "\tloaded=" << loaded
              << "\tfailed=" << failed
              << "\tselected=" << best.size()
              << "\tmissingCategories=" << missing
              << '\n';

    if (failed != 0) return 1;
    if (strict && missing != 0) return 1;
    return 0;
}
