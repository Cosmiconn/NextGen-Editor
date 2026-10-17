#include "mapeditor/core/legacy/QuestText.hpp"

#include <algorithm>

namespace theseed::mapeditor::core::legacy {

namespace {
bool IsBlank(char c) { return c == ' ' || c == '\t'; }
bool IsDigit(char c) { return c >= '0' && c <= '9'; }
} // namespace

std::vector<SayReference> FindSayReferences(std::string_view script) {
    std::vector<SayReference> out;
    std::size_t lineStart = 0;
    while (lineStart < script.size()) {
        std::size_t lineEnd = script.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string_view::npos) lineEnd = script.size();
        std::size_t p = lineStart;
        while (p < lineEnd && IsBlank(script[p])) ++p;
        if (lineEnd - p > 3 && script.substr(p, 3) == "SAY" && IsBlank(script[p + 3])) {
            p += 3;
            while (p < lineEnd && IsBlank(script[p])) ++p;
            const std::size_t digits = p;
            std::uint64_t value = 0;
            while (p < lineEnd && IsDigit(script[p]) && value <= 0xFFFFFFFFull) value = value * 10 + static_cast<unsigned>(script[p++] - '0');
            const bool endsToken = p == lineEnd || IsBlank(script[p]) || script[p] == ','; // "SAY 1502, NPC" (Quest 15)
            if (p > digits && endsToken && value <= 0xFFFFFFFFull)
                out.push_back({digits, p - digits, static_cast<std::uint32_t>(value)});
        }
        lineStart = lineEnd + 1;
    }
    return out;
}

std::string RemapSayIds(std::string_view script, const std::map<std::uint32_t, std::uint32_t>& remap) {
    std::string out;
    out.reserve(script.size() + 16);
    std::size_t copied = 0;
    for (const auto& ref : FindSayReferences(script)) {
        const auto it = remap.find(ref.id);
        if (it == remap.end()) continue;
        out.append(script.substr(copied, ref.offset - copied));
        out += std::to_string(it->second);
        copied = ref.offset + ref.length;
    }
    out.append(script.substr(copied));
    return out;
}

std::vector<std::uint32_t> QuestTextIds(const QuestRecord& quest) {
    std::vector<std::uint32_t> ids;
    auto add = [&](std::uint32_t id) {
        if (id != 0 && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    };
    add(quest.title);
    add(quest.description);
    for (const auto* script : {&quest.start, &quest.action, &quest.finish})
        for (const auto& ref : FindSayReferences(script->text)) add(ref.id);
    return ids;
}

std::optional<std::uint32_t> FindFreeTextBlock(const std::set<std::uint32_t>& used, std::size_t count,
                                               std::uint32_t minStart, std::uint32_t maxId) {
    if (count == 0) return minStart;
    std::uint64_t start = minStart;
    for (auto it = used.lower_bound(minStart);; ++it) {
        const std::uint64_t limit = it == used.end() ? std::uint64_t{maxId} + 1 : *it;
        if (limit > start && limit - start >= count) return static_cast<std::uint32_t>(start);
        if (it == used.end()) return std::nullopt;
        start = std::uint64_t{*it} + 1;
        if (start > maxId) return std::nullopt;
    }
}

std::map<std::uint32_t, std::uint32_t> AssignNewQuestTextIds(QuestRecord& quest, const std::set<std::uint32_t>& used) {
    const auto ids = QuestTextIds(quest);
    if (ids.empty()) return {};
    const std::uint32_t after = used.empty() ? 1u : *used.rbegin() + 1u;
    const auto start = FindFreeTextBlock(used, ids.size(), after == 0 ? 1u : after);
    if (!start) return {};
    std::map<std::uint32_t, std::uint32_t> remap;
    for (std::size_t i = 0; i < ids.size(); ++i) remap[ids[i]] = *start + static_cast<std::uint32_t>(i);
    if (quest.title != 0) quest.title = remap[quest.title];
    if (quest.description != 0) quest.description = remap[quest.description];
    for (auto* script : {&quest.start, &quest.action, &quest.finish}) script->text = RemapSayIds(script->text, remap);
    return remap;
}

} // namespace theseed::mapeditor::core::legacy
