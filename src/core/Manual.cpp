#include "mapeditor/core/Manual.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace theseed::mapeditor::core::manual {
namespace {

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

struct TipPair { const char* de; const char* en; };
const std::unordered_map<std::string, TipPair>& TipTable() {
    static const std::unordered_map<std::string, TipPair> table = [] {
        std::unordered_map<std::string, TipPair> t;
        ForEachTip([](const char* key, const char* de, const char* en, void* user) {
            static_cast<std::unordered_map<std::string, TipPair>*>(user)->emplace(key, TipPair{de, en});
        }, &t);
        return t;
    }();
    return table;
}

} // namespace

const std::vector<Chapter>& Chapters() {
    static const std::vector<Chapter> v = [] { std::size_t n = 0; const Chapter* d = ChapterData(n); return std::vector<Chapter>(d, d + n); }();
    return v;
}
const std::vector<Section>& Sections() {
    static const std::vector<Section> v = [] { std::size_t n = 0; const Section* d = SectionData(n); return std::vector<Section>(d, d + n); }();
    return v;
}
const std::vector<ColumnDoc>& Columns() {
    static const std::vector<ColumnDoc> v = [] { std::size_t n = 0; const ColumnDoc* d = ColumnData(n); return std::vector<ColumnDoc>(d, d + n); }();
    return v;
}

const char* TooltipFor(const std::string& kind, const std::string& label, bool german) {
    const auto& table = TipTable();
    const std::string base = label.substr(0, label.find("##"));
    for (const std::string& key : {kind + ":" + label, label, kind + ":" + base, base}) {
        if (const auto it = table.find(key); it != table.end()) return german ? it->second.de : it->second.en;
    }
    return nullptr;
}

const ColumnDoc* FindColumn(const std::string& table, const std::string& column) {
    const ColumnDoc* generic = nullptr;
    for (const auto& c : Columns()) {
        if (column != c.column) continue;
        if (table == c.table) return &c;
        if (std::string(c.table) == "*") generic = &c;
    }
    return generic;
}

std::vector<const Section*> Search(const std::string& query, bool german) {
    std::vector<std::string> words;
    {
        std::istringstream in(Lower(query));
        std::string w;
        while (in >> w) words.push_back(w);
    }
    std::vector<std::pair<int, const Section*>> hits;
    for (const auto& s : Sections()) {
        if (words.empty()) { hits.emplace_back(0, &s); continue; }
        const std::string title = Lower(german ? s.titleDe : s.titleEn);
        const std::string hay = title + "\n" + Lower(german ? s.bodyDe : s.bodyEn) + "\n" + Lower(s.keywords);
        int score = 0;
        bool all = true;
        for (const auto& w : words) {
            if (hay.find(w) == std::string::npos) { all = false; break; }
            if (title.find(w) != std::string::npos) score += 2; else score += 1;
        }
        if (all) hits.emplace_back(-score, &s);
    }
    std::stable_sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<const Section*> out;
    for (const auto& h : hits) out.push_back(h.second);
    return out;
}

} // namespace theseed::mapeditor::core::manual
