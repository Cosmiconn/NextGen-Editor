// ShnGroups.cpp - siehe ShnGroups.hpp.

#include "mapeditor/core/legacy/ShnGroups.hpp"

#include <unordered_map>

namespace theseed::mapeditor::core::legacy {
namespace {

std::string Trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

bool ValidColumn(const ShnFile& file, int keyColumn) {
    return keyColumn >= 0 && static_cast<std::size_t>(keyColumn) < file.columns.size();
}

std::string KeyOf(const ShnFile& file, std::size_t row, int keyColumn) {
    const auto& values = file.rows[row].values;
    if (static_cast<std::size_t>(keyColumn) >= values.size()) return {};
    return Trim(ShnValueToString(values[static_cast<std::size_t>(keyColumn)]));
}

} // namespace

std::vector<ShnGroup> GroupShnRows(const ShnFile& file, int keyColumn) {
    std::vector<ShnGroup> groups;
    if (!ValidColumn(file, keyColumn)) return groups;
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t r = 0; r < file.rows.size(); ++r) {
        std::string key = KeyOf(file, r, keyColumn);
        const auto [it, inserted] = index.try_emplace(key, groups.size());
        if (inserted) groups.push_back({std::move(key), {}});
        groups[it->second].rows.push_back(r);
    }
    return groups;
}

std::vector<std::size_t> ShnGroupRows(const ShnFile& file, int keyColumn, const std::string& key) {
    std::vector<std::size_t> rows;
    if (!ValidColumn(file, keyColumn)) return rows;
    for (std::size_t r = 0; r < file.rows.size(); ++r)
        if (KeyOf(file, r, keyColumn) == key) rows.push_back(r);
    return rows;
}

std::size_t ShnGroupInsertIndex(const ShnFile& file, int keyColumn, const std::string& key) {
    const auto rows = ShnGroupRows(file, keyColumn, key);
    return rows.empty() ? file.rows.size() : rows.back() + 1;
}

} // namespace theseed::mapeditor::core::legacy
