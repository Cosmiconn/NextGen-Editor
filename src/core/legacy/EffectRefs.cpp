// EffectRefs.cpp - siehe EffectRefs.hpp.

#include "mapeditor/core/legacy/EffectRefs.hpp"

#include <algorithm>
#include <cctype>

namespace theseed::mapeditor::core::legacy {
namespace {

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
std::string Trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

} // namespace

std::set<std::string> EffectFileStems(const std::vector<std::string>& fileNames) {
    std::set<std::string> out;
    for (const auto& name : fileNames) {
        const auto dot = name.find_last_of('.');
        out.insert(Lower(dot == std::string::npos ? name : name.substr(0, dot)));
    }
    return out;
}

std::vector<MissingEffect> FindMissingEffects(const ShnFile& file, const std::vector<std::string>& columns,
                                              const std::set<std::string>& lowercaseStems,
                                              const std::set<std::string>& specialValues) {
    std::vector<MissingEffect> out;
    std::vector<std::pair<int, std::string>> cols;
    for (const auto& name : columns)
        for (std::size_t c = 0; c < file.columns.size(); ++c)
            if (file.columns[c].name == name) cols.emplace_back(static_cast<int>(c), name);
    for (std::size_t r = 0; r < file.rows.size(); ++r)
        for (const auto& [c, name] : cols) {
            if (static_cast<std::size_t>(c) >= file.rows[r].values.size()) continue;
            const std::string value = Trim(ShnValueToString(file.rows[r].values[static_cast<std::size_t>(c)]));
            if (value.empty() || value == "-" || value == "0") continue;
            if (lowercaseStems.contains(Lower(value))) continue;
            out.push_back({r, name, value, specialValues.contains(value)});
        }
    return out;
}

} // namespace theseed::mapeditor::core::legacy
