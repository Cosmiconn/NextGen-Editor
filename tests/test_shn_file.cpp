#include "mapeditor/core/legacy/ShnFile.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace theseed::mapeditor::core::legacy;

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: test_shn_file file.shn\n"; return 2; }
    auto src = std::filesystem::path(argv[1]);
    auto loaded = LoadShnFile(src);
    assert(loaded);
    auto& f = *loaded;
    assert(!f.columns.empty());
    assert(!f.rows.empty());
    assert(f.rows.front().values.size() == f.columns.size());
    auto out = std::filesystem::temp_directory_path() / "nextgen_shn_roundtrip_test.shn";
    auto saved = SaveShnFile(f, out);
    assert(saved);
    std::ifstream a(src, std::ios::binary), b(out, std::ios::binary);
    std::vector<unsigned char> av((std::istreambuf_iterator<char>(a)), {}), bv((std::istreambuf_iterator<char>(b)), {});
    assert(av == bv && "unchanged SHN must round-trip byte-identically");
    auto edited = f;
    bool changed = false;
    for (std::size_t ci = 0; ci < edited.columns.size() && !changed; ++ci) {
        auto& v = edited.rows.front().values[ci];
        if (auto* u = std::get_if<std::uint16_t>(&v)) { ++*u; changed = true; }
        else if (auto* u32 = std::get_if<std::uint32_t>(&v)) { ++*u32; changed = true; }
        else if (auto* str = std::get_if<std::string>(&v)) { *str += "_test"; changed = true; }
    }
    assert(changed);
    auto editedPath = std::filesystem::temp_directory_path() / "nextgen_shn_edit_test.shn";
    auto editedSave = SaveShnFile(edited, editedPath);
    assert(editedSave);
    auto reloaded = LoadShnFile(editedPath);
    assert(reloaded);
    assert(ShnValueToString(reloaded->rows.front().values[0]) == ShnValueToString(edited.rows.front().values[0]));
    std::filesystem::remove(out);
    std::filesystem::remove(editedPath);
    std::cout << "SHN OK: " << src.filename().string() << " rows=" << f.rows.size() << " cols=" << f.columns.size() << "\n";
}
