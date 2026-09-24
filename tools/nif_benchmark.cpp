#include "mapeditor/core/NifModel.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

// Warm filesystem-cache measurement of complete CPU parsing/texture decoding.
// This measures no GPU upload, rendering, or cold disk access.
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const unsigned repeats = static_cast<unsigned>(std::stoul(argv[2]));
        if (!repeats || repeats > 10000) return 2;
        std::cout << "file\trepeats\tmedian_ms\tp95_ms\n";
        for (const auto& entry : std::filesystem::directory_iterator(argv[1])) {
            if (entry.path().extension() != ".nif") continue;
            if (!theseed::mapeditor::core::LoadNifMesh(entry.path(), false)) return 1;
            std::vector<double> samples;
            for (unsigned i = 0; i < repeats; ++i) {
                const auto begin = std::chrono::steady_clock::now();
                const auto model = theseed::mapeditor::core::LoadNifMesh(entry.path(), false);
                if (!model) return 1;
                samples.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
            }
            std::sort(samples.begin(), samples.end());
            std::cout << entry.path().filename().string() << '\t' << repeats << '\t' << samples[samples.size()/2]
                      << '\t' << samples[(samples.size()-1)*95/100] << '\n';
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
