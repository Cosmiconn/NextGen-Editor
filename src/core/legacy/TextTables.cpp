// TextTables.cpp - siehe TextTables.hpp.

#include "mapeditor/core/legacy/TextTables.hpp"

#include <algorithm>
#include <map>

namespace theseed::mapeditor::core::legacy {
namespace {

bool IsTextColumn(const ShnColumn& c) {
    if (c.kind != ShnValueKind::String) return false;
    return c.type == 26 || c.length >= 64;
}

std::string CsvField(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string out = "\"";
    for (const char ch : value) {
        if (ch == '"') out += '"';
        out += ch;
    }
    out += '"';
    return out;
}

} // namespace

std::vector<int> TextColumns(const ShnFile& file) {
    std::vector<int> out;
    for (std::size_t c = 0; c < file.columns.size(); ++c)
        if (IsTextColumn(file.columns[c])) out.push_back(static_cast<int>(c));
    return out;
}

std::vector<std::string> TextRowKeys(const ShnFile& file) {
    std::vector<std::string> keys;
    keys.reserve(file.rows.size());
    const bool byRow = file.columns.empty() || IsTextColumn(file.columns.front());
    std::map<std::string, int> seen;
    for (std::size_t r = 0; r < file.rows.size(); ++r) {
        if (byRow || file.rows[r].values.empty()) { keys.push_back("#" + std::to_string(r)); continue; }
        std::string key = ShnValueToString(file.rows[r].values.front());
        const int n = ++seen[key];
        if (n > 1) key += "#" + std::to_string(n);
        keys.push_back(std::move(key));
    }
    return keys;
}

std::string WriteTextCsv(const std::vector<TextCsvRow>& rows) {
    std::string out = "table,key,column,text\r\n";
    for (const auto& r : rows)
        out += CsvField(r.table) + "," + CsvField(r.key) + "," + CsvField(r.column) + "," + CsvField(r.text) + "\r\n";
    return out;
}

std::expected<std::vector<TextCsvRow>, std::string> ParseTextCsv(const std::string& csv) {
    std::vector<std::vector<std::string>> records;
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false, fieldStarted = false;
    std::size_t line = 1;
    std::size_t i = 0;
    if (csv.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3; // UTF-8-BOM aus Tabellenkalkulationen
    const auto endField = [&] { fields.push_back(std::move(field)); field.clear(); fieldStarted = false; };
    const auto endRecord = [&] {
        endField();
        if (!(fields.size() == 1 && fields.front().empty())) records.push_back(std::move(fields));
        fields.clear();
    };
    for (; i < csv.size(); ++i) {
        const char ch = csv[i];
        if (quoted) {
            if (ch == '"') {
                if (i + 1 < csv.size() && csv[i + 1] == '"') { field += '"'; ++i; }
                else quoted = false;
            } else {
                if (ch == '\n') ++line;
                field += ch;
            }
            continue;
        }
        if (ch == '"' && !fieldStarted && field.empty()) { quoted = true; fieldStarted = true; }
        else if (ch == ',') endField();
        else if (ch == '\r') { if (i + 1 < csv.size() && csv[i + 1] == '\n') ++i; endRecord(); ++line; }
        else if (ch == '\n') { endRecord(); ++line; }
        else { field += ch; fieldStarted = true; }
    }
    if (quoted) return std::unexpected("CSV: Anführungszeichen ab Zeile " + std::to_string(line) + " nicht geschlossen");
    if (fieldStarted || !field.empty() || !fields.empty()) endRecord();

    if (records.empty()) return std::unexpected("CSV: leer");
    const auto& head = records.front();
    if (head.size() < 4 || head[0] != "table" || head[1] != "key" || head[2] != "column" || head[3] != "text")
        return std::unexpected("CSV: Kopfzeile muss \"table,key,column,text\" lauten");
    std::vector<TextCsvRow> out;
    for (std::size_t r = 1; r < records.size(); ++r) {
        const auto& rec = records[r];
        if (rec.size() != 4)
            return std::unexpected("CSV: Datensatz " + std::to_string(r) + " hat " + std::to_string(rec.size()) + " statt 4 Feldern");
        out.push_back({rec[0], rec[1], rec[2], rec[3]});
    }
    return out;
}

std::vector<TextCsvRow> TextCsvRowsFor(const ShnFile& file, const std::string& tableName) {
    std::vector<TextCsvRow> out;
    const auto columns = TextColumns(file);
    const auto keys = TextRowKeys(file);
    for (std::size_t r = 0; r < file.rows.size(); ++r)
        for (const int c : columns)
            if (static_cast<std::size_t>(c) < file.rows[r].values.size())
                out.push_back({tableName, keys[r], file.columns[static_cast<std::size_t>(c)].name,
                               ShnValueToString(file.rows[r].values[static_cast<std::size_t>(c)])});
    return out;
}

TextCsvDiff DiffTextCsv(const ShnFile& file, const std::string& tableName, const std::vector<TextCsvRow>& rows) {
    TextCsvDiff diff;
    std::map<std::string, std::size_t> rowByKey;
    const auto keys = TextRowKeys(file);
    for (std::size_t r = 0; r < keys.size(); ++r) rowByKey.emplace(keys[r], r);
    const auto textColumns = TextColumns(file);
    for (const auto& csv : rows) {
        if (csv.table != tableName) continue;
        const auto rowIt = rowByKey.find(csv.key);
        if (rowIt == rowByKey.end()) { diff.problems.push_back(tableName + " " + csv.key + ": Schlüssel nicht gefunden"); continue; }
        int column = -1;
        for (const int c : textColumns)
            if (file.columns[static_cast<std::size_t>(c)].name == csv.column) column = c;
        if (column < 0) { diff.problems.push_back(tableName + " " + csv.key + ": keine Textspalte " + csv.column); continue; }
        const auto& col = file.columns[static_cast<std::size_t>(column)];
        const auto& values = file.rows[rowIt->second].values;
        const std::string current = static_cast<std::size_t>(column) < values.size()
                                        ? ShnValueToString(values[static_cast<std::size_t>(column)]) : std::string();
        if (current == csv.text) { ++diff.unchanged; continue; }
        if (col.type != 26 && col.length > 0 && csv.text.size() > col.length - 1) {
            diff.problems.push_back(tableName + " " + csv.key + " " + csv.column + ": Text zu lang (" +
                                    std::to_string(csv.text.size()) + " > " + std::to_string(col.length - 1) + " Byte)");
            continue;
        }
        diff.changes.push_back({rowIt->second, column, csv.text});
    }
    return diff;
}

} // namespace theseed::mapeditor::core::legacy
