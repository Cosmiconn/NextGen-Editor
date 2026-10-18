// MobAttackSequence.cpp - siehe MobAttackSequence.hpp.

#include "mapeditor/core/legacy/MobAttackSequence.hpp"

namespace theseed::mapeditor::core::legacy {
namespace {

std::string Trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string ValueAt(const ShineRecord& r, std::size_t i) { return i < r.values.size() ? Trim(r.values[i]) : std::string(); }

// Schreibt n Records mit den Werten aus make(i); vorhandene Records werden überschrieben, fehlende
// angehängt (Form der letzten vorhandenen Zeile), überzählige entfernt.
template <class Make>
void WriteRecords(ShineTable& table, std::size_t n, Make make) {
    const std::size_t style = table.records.empty() ? 0 : table.records.back().sourceLine;
    for (std::size_t i = 0; i < n; ++i) {
        std::vector<std::string> values = make(i);
        if (i < table.records.size()) {
            auto& rec = table.records[i];
            // Unveränderte Werte samt Original-Leerzeichen stehen lassen.
            for (std::size_t c = 0; c < values.size(); ++c)
                if (c < rec.values.size() && Trim(rec.values[c]) == values[c]) values[c] = rec.values[c];
            rec.values = std::move(values);
        } else {
            ShineRecord rec;
            rec.values = std::move(values);
            rec.styleLine = style;
            table.records.push_back(std::move(rec));
        }
    }
    if (table.records.size() > n) table.records.resize(n);
}

} // namespace

std::expected<AttackSequence, std::string> ReadAttackSequence(const ShineTextFile& file) {
    const ShineTable* att = file.FindTable("AttSeq");
    if (!att) return std::unexpected("Tabelle AttSeq fehlt");
    AttackSequence seq;
    for (std::size_t i = 0; i < att->records.size(); ++i) {
        const auto& r = att->records[i];
        if (ValueAt(r, 0) != std::to_string(i)) seq.ordersSequential = false;
        std::string attack = ValueAt(r, 1);
        seq.attacks.push_back(attack.empty() ? std::string(kAttackEmpty) : attack);
    }
    if (const ShineTable* rules = file.FindTable("SkillChange")) {
        seq.hasRuleTable = true;
        for (const auto& r : rules->records)
            seq.rules.push_back({ValueAt(r, 0), ValueAt(r, 1), ValueAt(r, 2), ValueAt(r, 3)});
    }
    return seq;
}

std::expected<void, std::string> ApplyAttackSequence(ShineTextFile& file, const AttackSequence& seq) {
    ShineTable* att = file.FindTable("AttSeq");
    if (!att) return std::unexpected("Tabelle AttSeq fehlt");
    for (const auto& a : seq.attacks)
        if (a.empty() || a.size() > 32) return std::unexpected("Ungültiger Angriff: '" + a + "' (1-32 Zeichen)");
    WriteRecords(*att, seq.attacks.size(), [&](std::size_t i) {
        return std::vector<std::string>{std::to_string(i), seq.attacks[i]};
    });

    ShineTable* rules = file.FindTable("SkillChange");
    if (!rules && !seq.rules.empty()) {
        ShineTable table;
        table.name = "SkillChange";
        table.columns = {{"Condition", "Index"}, {"Value", "Index"}, {"From", "String[33]"}, {"To", "String[33]"}};
        table.isNew = true;
        file.tables.push_back(std::move(table));
        rules = &file.tables.back();
    }
    if (rules) {
        for (const auto& r : seq.rules)
            if (r.condition.empty()) return std::unexpected("Regel ohne Bedingung");
        WriteRecords(*rules, seq.rules.size(), [&](std::size_t i) {
            const auto& r = seq.rules[i];
            return std::vector<std::string>{r.condition, r.value.empty() ? "0" : r.value, r.from.empty() ? "-" : r.from,
                                            r.to.empty() ? "-" : r.to};
        });
        // Eine im Editor neu angelegte, wieder leere Regeltabelle nicht als leeren Kopf schreiben.
        if (rules->isNew && rules->records.empty())
            std::erase_if(file.tables, [](const ShineTable& t) { return t.isNew && t.name == "SkillChange" && t.records.empty(); });
    }
    return {};
}

} // namespace theseed::mapeditor::core::legacy
