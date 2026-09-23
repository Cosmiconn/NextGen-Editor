#include "mapeditor/core/NifModel.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace theseed::mapeditor::core;

namespace fs = std::filesystem;

static bool IsNif(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".nif";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <fixture-directory>\n", argv[0]);
        return 2;
    }

    const fs::path root = argv[1];
    if (!fs::is_directory(root)) {
        std::fprintf(stderr, "fixture directory not found: %s\n", root.string().c_str());
        return 2;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && IsNif(entry.path())) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    std::size_t ok = 0;
    std::size_t noGeometry = 0;
    std::size_t failed = 0;

    for (const auto& file : files) {
        auto model = LoadNifMesh(file);
        const auto rel = fs::relative(file, root).generic_string();
        if (!model) {
            ++failed;
            std::fprintf(stderr, "[FAIL] %s: %s\n", rel.c_str(), model.error().c_str());
            continue;
        }

        bool hasGeometry = false;
        for (const auto& part : model->parts) {
            if (!part.positions.empty() && !part.triangleIndices.empty()) {
                hasGeometry = true;
                break;
            }
        }

        if (hasGeometry) {
            ++ok;
            std::printf("[OK]   %s parts=%zu\n", rel.c_str(), model->parts.size());
        } else {
            ++noGeometry;
            std::printf("[NOGEO] %s\n", rel.c_str());
        }
    }

    std::printf("\nNIF fixture corpus: total=%zu ok=%zu no_geometry=%zu failed=%zu\n",
                files.size(), ok, noGeometry, failed);
    return failed == 0 ? 0 : 1;
}
