#include "mapeditor/core/legacy/ShineText.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <utility>

namespace theseed::mapeditor::core::legacy {

namespace {

std::string TrimAscii(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && static_cast<unsigned char>(s[a]) <= ' ') ++a;
    while (b > a && static_cast<unsigned char>(s[b - 1]) <= ' ') --b;
    return s.substr(a, b - a);
}

std::string LowerAsciiCopy(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::vector<std::string> SplitTab(const std::string& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == '\t') {
            out.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

// Entfernt rechtsseitig komplett leere Tokens (die vielen Auffüll-Tabs am Zeilenende, die die
// echten Dateien nutzen, um alle Zeilen auf eine feste Spaltenzahl zu bringen) - aber nur ganz
// am Ende, damit absichtlich leere Felder MITTEN in einer Zeile (z.B. "-" wird oft für "leer"
// benutzt, ein wirklich leerer String kann aber auch vorkommen) erhalten bleiben.
void TrimTrailingEmpty(std::vector<std::string>& tokens) {
    while (!tokens.empty() && TrimAscii(tokens.back()).empty()) tokens.pop_back();
}

bool IsCommentToken(const std::string& token) {
    const std::string t = TrimAscii(token);
    return !t.empty() && t[0] == ';';
}

std::size_t FirstNonEmpty(const std::vector<std::string>& tokens) {
    std::size_t i = 0;
    while (i < tokens.size() && TrimAscii(tokens[i]).empty()) ++i;
    return i;
}

// Zerlegte Zeile mit den Trennern dazwischen, damit eine bearbeitete Zeile mit genau ihren
// Original-Trennern (Tab oder Leerzeichenlauf) wieder zusammengesetzt werden kann.
struct LineTokens {
    std::string prefix;              // führender Weißraum (nur bei Leerzeichen-Trennung)
    std::vector<std::string> tokens;
    std::vector<std::string> seps;   // seps[i] steht zwischen tokens[i] und tokens[i + 1]
    std::string defaultSep = "\t";
};

LineTokens TokenizeLine(const std::string& raw, bool spaceDelimiter) {
    LineTokens lt;
    lt.tokens = SplitTab(raw);
    lt.seps.assign(lt.tokens.empty() ? 0 : lt.tokens.size() - 1, "\t");
    if (!spaceDelimiter) return lt;
    // "#delimiter \x20": eine Record-Zeile ohne Tab zwischen Direktive und Werten
    // ("    #recordin    ShineNPC    Xiaoming ...") wird an Leerzeichenläufen getrennt.
    const std::size_t d = FirstNonEmpty(lt.tokens);
    if (d >= lt.tokens.size()) return lt;
    const std::string first = LowerAsciiCopy(TrimAscii(lt.tokens[d]));
    if (first.rfind("#record", 0) != 0 || first == "#record" || first == "#recordin") return lt;
    LineTokens sp;
    sp.defaultSep = " ";
    auto isWs = [](char c) { return c == ' ' || c == '\t'; };
    std::size_t i = 0;
    while (i < raw.size() && isWs(raw[i])) ++i;
    sp.prefix = raw.substr(0, i);
    while (i < raw.size()) {
        std::size_t j = i;
        while (j < raw.size() && !isWs(raw[j])) ++j;
        sp.tokens.push_back(raw.substr(i, j - i));
        std::size_t k = j;
        while (k < raw.size() && isWs(raw[k])) ++k;
        if (k > j) {
            sp.seps.push_back(raw.substr(j, k - j));
            if (k == raw.size()) sp.tokens.emplace_back(); // Weißraum am Zeilenende
        }
        i = k;
    }
    return sp;
}

// Setzt (Token, Trenner danach)-Paare wieder zusammen; ein fehlender Trenner vor einem weiteren
// Token wird durch den Standard-Trenner der Zeile ersetzt.
std::string JoinTokens(const std::string& prefix, const std::vector<std::pair<std::string, std::string>>& parts,
                       const std::string& defaultSep) {
    std::string out = prefix;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        out += parts[i].first;
        if (i + 1 < parts.size()) out += parts[i].second.empty() ? defaultSep : parts[i].second;
        else out += parts[i].second;
    }
    return out;
}

struct RecordDirective {
    bool isRecord = false;
    bool recordin = false;
    std::size_t firstArg = 0; // Index des ersten Werts (nach Direktive bzw. Tabellenname)
    std::string table;        // nur bei #recordin
};

RecordDirective ParseRecordDirective(const LineTokens& lt) {
    RecordDirective d;
    const std::size_t idx = FirstNonEmpty(lt.tokens);
    if (idx >= lt.tokens.size()) return d;
    const std::string lower = LowerAsciiCopy(TrimAscii(lt.tokens[idx]));
    if (lower == "#record") {
        d.isRecord = true;
        d.firstArg = idx + 1;
    } else if (lower == "#recordin" && idx + 1 < lt.tokens.size()) {
        d.isRecord = true;
        d.recordin = true;
        d.table = TrimAscii(lt.tokens[idx + 1]);
        d.firstArg = idx + 2;
    }
    return d;
}

// Tokenbereich [begin, end) der Werte eines Records. Danach folgen unverändert Kommentar, ein
// ";"-Sentinel, überzählige Werte und Auffüll-Tabs.
struct ValueSpan {
    std::size_t begin = 0;
    std::size_t end = 0;
    bool truncated = false; // Werte jenseits der Spaltenzahl abgeschnitten
};

ValueSpan FindValueSpan(const LineTokens& lt, const RecordDirective& d, std::size_t columnCount) {
    ValueSpan s;
    const std::size_t n = lt.tokens.size();
    s.begin = std::min(d.firstArg, n);
    std::size_t comment = s.begin;
    while (comment < n && !IsCommentToken(lt.tokens[comment])) ++comment;
    s.end = comment;
    while (s.end > s.begin && TrimAscii(lt.tokens[s.end - 1]).empty()) --s.end;
    // Manche Dateien (z.B. MobRegen/<Karte>.txt) haben bei "#record" ein zusätzliches Leer-Tab vor dem
    // ersten Wert - genau ein Wert zu viel, und der erste ist leer (siehe #ColumnType/#ColumnName).
    if (!d.recordin && s.end - s.begin == columnCount + 1 && TrimAscii(lt.tokens[s.begin]).empty()) ++s.begin;
    // Daten breiter als der Kopf (Script/Honeying.txt): nur die deklarierten Spalten sind Werte.
    if (columnCount > 0 && s.end - s.begin > columnCount) {
        s.end = s.begin + columnCount;
        s.truncated = true;
    }
    return s;
}

std::vector<std::string> SpanValues(const LineTokens& lt, const ValueSpan& s) {
    return {lt.tokens.begin() + static_cast<std::ptrdiff_t>(s.begin), lt.tokens.begin() + static_cast<std::ptrdiff_t>(s.end)};
}

// Folgezeilen eines Records, dessen Text über mehrere Zeilen läuft: Zeilen direkt danach, die weder
// leer noch Direktive noch Kommentar sind (in NA2016 nur Script/Scenario.txt).
std::size_t CountContinuationLines(const std::vector<std::string>& lines, std::size_t recordLine) {
    std::size_t count = 0;
    for (std::size_t i = recordLine + 1; i < lines.size(); ++i) {
        const auto tokens = SplitTab(lines[i]);
        const std::size_t d = FirstNonEmpty(tokens);
        if (d >= tokens.size()) break;
        const std::string first = TrimAscii(tokens[d]);
        if (first[0] == '#' || first[0] == ';') break;
        ++count;
    }
    return count;
}

// Hängt den Text der Folgezeilen (bis zu ihrem Kommentar) samt Zeilenumbruch an den letzten Wert an.
void AppendContinuation(std::vector<std::string>& values, const std::vector<std::string>& lines,
                        const std::vector<std::string>& endings, std::size_t recordLine, std::size_t count) {
    if (values.empty()) return;
    for (std::size_t i = recordLine + 1; i <= recordLine + count && i < lines.size(); ++i) {
        auto tokens = SplitTab(lines[i]);
        std::size_t end = 0;
        while (end < tokens.size() && !IsCommentToken(tokens[end])) ++end;
        tokens.resize(end);
        TrimTrailingEmpty(tokens);
        std::string text;
        for (std::size_t t = 0; t < tokens.size(); ++t) { if (t) text += '\t'; text += tokens[t]; }
        const std::string ending = i - 1 < endings.size() && !endings[i - 1].empty() ? endings[i - 1] : std::string("\n");
        values.back() += ending + text;
    }
}

} // namespace

ShineTable* ShineTextFile::FindTable(const std::string& name) {
    for (auto& t : tables) if (t.name == name) return &t;
    return nullptr;
}
const ShineTable* ShineTextFile::FindTable(const std::string& name) const {
    for (auto& t : tables) if (t.name == name) return &t;
    return nullptr;
}

std::expected<ShineTextFile, std::string> LoadShineTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::unexpected("Datei konnte nicht geöffnet werden: " + path.string());

    ShineTextFile file;
    file.path = path;
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    bool defaultEndingSet = false;
    for (std::size_t pos = 0; pos < content.size();) {
        const std::size_t nl = content.find('\n', pos);
        std::string line;
        std::string ending;
        if (nl == std::string::npos) {
            line = content.substr(pos);       // letzte Zeile ohne Zeilenumbruch
            pos = content.size();
        } else {
            line = content.substr(pos, nl - pos);
            ending = "\n";
            if (!line.empty() && line.back() == '\r') { line.pop_back(); ending = "\r\n"; }
            pos = nl + 1;
        }
        if (!ending.empty() && !defaultEndingSet) { file.defaultLineEnding = ending; defaultEndingSet = true; }
        file.rawLines.push_back(std::move(line));
        file.rawLineEndings.push_back(std::move(ending));
    }

    ShineTable* current = nullptr;
    bool typeLineSemicolon = false; // #ColumnType der aktuellen Tabelle endete mit dem ";"-Sentinel
    // Spalten einer Kopfzeile: bis zum ersten Kommentar-Token (z. B. "#ColumnName\tScriptName\t; *.ps").
    // Liefert, ob die Zeile genau mit einem einzelnen ";" endet (Sentinel, siehe ShineTable).
    auto headerTokens = [](const std::vector<std::string>& tokens, std::size_t argBase, std::size_t& end) {
        end = argBase;
        while (end < tokens.size() && !IsCommentToken(tokens[end])) ++end;
        if (end >= tokens.size() || TrimAscii(tokens[end]) != ";") return false;
        for (std::size_t i = end + 1; i < tokens.size(); ++i)
            if (!TrimAscii(tokens[i]).empty()) return false;
        return true;
    };
    for (std::size_t lineIdx = 0; lineIdx < file.rawLines.size(); ++lineIdx) {
        const std::string& raw = file.rawLines[lineIdx];
        if (raw.empty()) continue;
        const LineTokens lt = TokenizeLine(raw, file.spaceDelimiter);
        const auto& tokens = lt.tokens;
        if (tokens.empty()) continue;
        // Manche Dateien (z.B. World/NPC.txt) haben ein führendes Leer-Tab VOR der Direktive
        // ("\t#Table\tShineNPC\t...") - erstes NICHT-leeres Token suchen statt blind tokens[0]
        // zu nehmen, und den Rest relativ dazu behandeln.
        std::size_t directiveIdx = 0;
        while (directiveIdx < tokens.size() && TrimAscii(tokens[directiveIdx]).empty()) ++directiveIdx;
        if (directiveIdx >= tokens.size()) continue;
        std::string first = TrimAscii(tokens[directiveIdx]);
        if (first.empty()) continue;
        if (first[0] == ';') continue; // Kommentarzeile

        std::string firstLower = LowerAsciiCopy(first);
        const std::size_t argBase = directiveIdx + 1; // erstes Argument-Token nach der Direktive

        if (firstLower == "#delimiter") {
            for (std::size_t i = argBase; i < tokens.size(); ++i) {
                const std::string t = LowerAsciiCopy(TrimAscii(tokens[i]));
                if (t.empty()) continue;
                if (t == "\\x20") file.spaceDelimiter = true;
                break;
            }
        } else if (firstLower == "#table") {
            std::string name;
            for (std::size_t i = argBase; i < tokens.size(); ++i) {
                std::string t = TrimAscii(tokens[i]);
                if (!t.empty()) { name = t; break; }
            }
            file.tables.push_back(ShineTable{});
            current = &file.tables.back();
            current->name = name;
            current->lastLine = lineIdx;
            current->headerFirstLine = lineIdx;
            current->headerLastLine = lineIdx;
            typeLineSemicolon = false;
        } else if (firstLower == "#columntype") {
            if (!current) continue;
            std::size_t end = 0;
            typeLineSemicolon = headerTokens(tokens, argBase, end);
            for (std::size_t i = argBase; i < end; ++i) {
                std::string t = TrimAscii(tokens[i]);
                if (t.empty()) continue;
                std::size_t col = i - argBase;
                if (current->columns.size() <= col) current->columns.resize(col + 1);
                current->columns[col].type = t;
            }
            current->lastLine = lineIdx;
            current->headerLastLine = lineIdx;
        } else if (firstLower == "#columnname") {
            if (!current) continue;
            std::size_t end = 0;
            const bool nameLineSemicolon = headerTokens(tokens, argBase, end);
            for (std::size_t i = argBase; i < end; ++i) {
                std::string t = TrimAscii(tokens[i]);
                if (t.empty()) continue;
                std::size_t col = i - argBase;
                if (current->columns.size() <= col) current->columns.resize(col + 1);
                current->columns[col].name = t;
            }
            // Manche Tabellen (beobachtet in World/NPC.txt, Tabelle "ShineNPC") haben nach
            // "#ColumnType"/"#ColumnName" ein zusätzliches Leer-Tab VOR dem ersten echten Wert
            // ("\t#ColumnType\t\tSTRING[33]\t...") - die "#recordin"/"#record"-Datenzeilen
            // haben dieses zusätzliche Leer-Tab aber NICHT. Ohne Korrektur wäre die Spalten-
            // liste dadurch um eins gegenüber den echten Werten verschoben. Erkennbar daran,
            // dass die erste Spalte weder Namen noch Typ hat, obwohl die Tabelle echte Records
            // besitzt - dann wird sie entfernt.
            if (!current->columns.empty() && current->columns[0].name.empty() && current->columns[0].type.empty()) {
                current->columns.erase(current->columns.begin());
            }
            // World/ItemDropTable.txt (NA2016) besitzt 290 echte Spalten, hängt aber sowohl
            // an #ColumnType als auch #ColumnName noch ein "\t;" an. Dieses Semikolon ist
            // ein Shine-Zeilen-/Kommentar-Sentinel, keine 291. Datenspalte. Einige Records
            // tragen an derselben Position ";" und andere nur ein leeres Auffüllfeld.
            if (typeLineSemicolon && nameLineSemicolon) current->trailingSemicolonSentinel = true;
            current->lastLine = lineIdx;
            current->headerLastLine = lineIdx;
        } else if (firstLower == "#record" || firstLower == "#recordin") {
            // "#record" gehört zur zuletzt deklarierten Tabelle, "#recordin <Tabellenname> <werte...>"
            // (siehe World/NPC.txt) zielt explizit auf eine per Namen benannte Tabelle.
            const RecordDirective d = ParseRecordDirective(lt);
            if (!d.isRecord) continue;
            ShineTable* target = d.recordin ? file.FindTable(d.table) : current;
            if (!target) continue;
            const ValueSpan span = FindValueSpan(lt, d, target->columns.size());
            ShineRecord rec{SpanValues(lt, span), lineIdx};
            rec.continuationLines = CountContinuationLines(file.rawLines, lineIdx);
            if (!span.truncated) AppendContinuation(rec.values, file.rawLines, file.rawLineEndings, lineIdx, rec.continuationLines);
            for (std::size_t i = 0; i <= rec.continuationLines; ++i) file.loadedRecordLines.push_back(lineIdx + i);
            target->lastLine = lineIdx + rec.continuationLines;
            lineIdx += rec.continuationLines;
            target->records.push_back(std::move(rec));
        }
        // Andere Direktiven (#ignore, #exchange, #delimiter, #end, unbekannte) werden bewusst
        // übersprungen - sie beeinflussen nur eine Vorverarbeitung (Zeichen-Ersetzung), die für
        // die bisher betrachteten Felder (Namen, Koordinaten, Kartennamen) nicht relevant war.
    }

    for (const auto& t : file.tables) {
        if (t.headerFirstLine != static_cast<std::size_t>(-1)) file.loadedTableHeaders.emplace_back(t.headerFirstLine, t.headerLastLine);
    }
    return file;
}

namespace {
bool IsNumberToken(const std::string& s) {
    std::size_t i = 0;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
    bool digits = false, dot = false;
    for (; i < s.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[i]))) digits = true;
        else if (s[i] == '.' && !dot) dot = true;
        else return false;
    }
    return digits;
}

// Neuer Record in der Form einer vorhandenen Zeile derselben Tabelle (siehe ShineRecord::styleLine).
// recordDirectiveSafe: "#record" ohne Tabellenname würde beim Einlesen an der Einfügestelle zu
// genau dieser Tabelle gehören (sie ist dort die zuletzt per "#Table" deklarierte).
std::string FormatLikeStyleLine(const std::string& styleRaw, const ShineTable& table, const std::vector<std::string>& values,
                                bool recordDirectiveSafe) {
    const LineTokens lt = TokenizeLine(styleRaw, false);
    const auto& tokens = lt.tokens;
    const RecordDirective rd = ParseRecordDirective(lt);
    if (!rd.isRecord) {
        // Keine tab-getrennte Record-Zeile als Muster (z. B. eine leerzeichengetrennte): schlichte Form.
        std::string line = "#recordin\t" + table.name;
        for (const auto& v : values) line += "\t" + v;
        if (table.trailingSemicolonSentinel) line += "\t;";
        return line;
    }
    const std::size_t dIdx = FirstNonEmpty(tokens);
    const std::string directive = tokens[dIdx];
    const std::size_t valueBegin = FindValueSpan(lt, rd, table.columns.size()).begin;

    std::string line(dIdx, '\t');
    std::size_t valueStart = dIdx + 1; // Index des ersten Werts in der Musterzeile
    if (rd.recordin) {
        line += directive + "\t" + tokens[dIdx + 1];
        valueStart = dIdx + 2;
    } else if (recordDirectiveSafe) {
        line += directive;
        // "#record\t\t<werte>": zusätzliches Leer-Tab wie in der Musterzeile.
        if (valueBegin == dIdx + 2) {
            line += "\t" + tokens[dIdx + 1];
            ++valueStart;
        }
    } else {
        line += "#recordin\t" + table.name;
    }
    std::size_t written = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::string v = values[i];
        const std::size_t si = valueStart + i;
        if (si < tokens.size() && !v.empty() && v.back() != ' ') {
            const std::string& st = tokens[si];
            const std::string trimmed = TrimAscii(st);
            std::size_t end = st.size();
            while (end > 0 && st[end - 1] == ' ') --end;
            if (end < st.size() && IsNumberToken(trimmed) && IsNumberToken(v)) v += st.substr(end);
        }
        line += "\t" + v;
        ++written;
    }
    std::size_t tokenCount = valueStart + written;
    if (table.trailingSemicolonSentinel) {
        line += "\t;";
        ++tokenCount;
    }
    // Auffüllen auf die Feldzahl der Musterzeile (echte Dateien füllen jede Zeile mit Tabs auf).
    for (; tokenCount < tokens.size(); ++tokenCount) line += "\t";
    return line;
}
} // namespace

std::expected<void, std::string> SaveShineTextFile(const ShineTextFile& file, const std::filesystem::path& path) {
    std::vector<std::string> outLines = file.rawLines;
    std::vector<std::string> outEndings = file.rawLineEndings;
    // Von Hand aufgebaute/aeltere ShineTextFile-Objekte ohne Endungs-Liste: alles wie bisher CRLF.
    if (outEndings.size() != outLines.size()) outEndings.assign(outLines.size(), file.defaultLineEnding);

    // Loeschen/Einfuegen aendert Zeilenindizes - deshalb wird zuerst NUR in-place ersetzt
    // (Indizes bleiben stabil), Loeschen und Einfuegen erst beim Ausgeben in einem Durchlauf
    // (frueher verschob ein Einfuegen in Tabelle 1 die Zeilen aller spaeteren Tabellen, sodass
    // dort spaetere Aenderungen in falsche Zeilen geschrieben wurden; entfernte Records blieben
    // in der Datei stehen). Siehe CHANGELOG [0.44.27].
    std::vector<char> deleted(outLines.size(), 0);
    for (std::size_t line : file.loadedRecordLines) if (line < deleted.size()) deleted[line] = 1;
    std::vector<std::pair<std::size_t, std::string>> inserts; // (nach Quellzeile, neue Zeile) in Reihenfolge
    // Entfernte Tabellen: ihr Kopf (#Table..#ColumnName) verschwindet mit (die Records sind oben
    // bereits als geloescht markiert, weil sie nicht mehr in file.tables stehen).
    if (!file.loadedTableHeaders.empty()) {
        std::set<std::size_t> presentHeaders;
        for (const auto& t : file.tables) if (t.headerFirstLine != static_cast<std::size_t>(-1)) presentHeaders.insert(t.headerFirstLine);
        for (const auto& [first, last] : file.loadedTableHeaders) {
            if (presentHeaders.count(first)) continue;
            for (std::size_t line = first; line <= last && line < deleted.size(); ++line) deleted[line] = 1;
        }
    }
    const std::size_t kAtEnd = static_cast<std::size_t>(-1);
    for (const auto& table : file.tables) {
        if (table.isNew) {
            // Neue Tabelle komplett ans Dateiende: Leerzeile, Kopf, Records.
            auto joinTab = [](const std::vector<std::string>& parts) {
                std::string out;
                for (std::size_t i = 0; i < parts.size(); ++i) { if (i) out += '\t'; out += parts[i]; }
                return out;
            };
            std::vector<std::string> types, names;
            for (const auto& c : table.columns) { types.push_back(c.type); names.push_back(c.name); }
            inserts.emplace_back(kAtEnd, std::string());
            inserts.emplace_back(kAtEnd, "#Table\t" + table.name);
            inserts.emplace_back(kAtEnd, "#ColumnType\t" + joinTab(types));
            inserts.emplace_back(kAtEnd, "#ColumnName\t" + joinTab(names));
            for (const auto& rec : table.records) {
                std::string line = "#Record\t" + joinTab(rec.values);
                if (table.trailingSemicolonSentinel) line += "\t;";
                inserts.emplace_back(kAtEnd, std::move(line));
            }
            continue;
        }
        std::size_t insertAfter = table.lastLine;
        for (const auto& rec : table.records) {
            std::string joined;
            for (std::size_t i = 0; i < rec.values.size(); ++i) {
                if (i) joined += '\t';
                joined += rec.values[i];
            }
            if (rec.sourceLine > 0 && rec.sourceLine < outLines.size()) {
                // Bestehender Record - nur seine Werte-Tokens ersetzen. Alles andere der Zeile bleibt
                // exakt stehen: führende Tabs, Direktive ("#record"/"#recordin <Name>"), das
                // zusätzliche Leer-Tab, die Trenner, ein Kommentar ab ";", der ";"-Sentinel,
                // überzählige Werte und die Auffüll-Tabs. Die Zerlegung ist dieselbe wie beim Laden
                // (FindValueSpan), ein unveränderter Record bleibt so byte-identisch.
                const LineTokens lt = TokenizeLine(outLines[rec.sourceLine], file.spaceDelimiter);
                const RecordDirective rd = ParseRecordDirective(lt);
                const std::size_t cont = CountContinuationLines(file.rawLines, rec.sourceLine);
                if (!rd.isRecord) {
                    // Sollte nicht vorkommen (sourceLine zeigt immer auf eine Record-Zeile).
                    return std::unexpected("Zeile " + std::to_string(rec.sourceLine + 1) + " ist kein Record: " + path.string());
                }
                const ValueSpan span = FindValueSpan(lt, rd, table.columns.size());
                auto original = SpanValues(lt, span);
                if (!span.truncated) AppendContinuation(original, file.rawLines, outEndings, rec.sourceLine, cont);
                for (std::size_t i = 0; i <= cont && rec.sourceLine + i < deleted.size(); ++i) deleted[rec.sourceLine + i] = 0;
                if (rec.values == original) continue; // unverändert: Zeile(n) bleiben wie sie sind
                if (cont > 0) {
                    return std::unexpected("Zeile " + std::to_string(rec.sourceLine + 1) +
                                           ": mehrzeiliger Text kann nur unverändert gespeichert werden: " + path.string());
                }
                std::vector<std::pair<std::string, std::string>> parts;
                const std::size_t n = lt.tokens.size();
                auto sepAfter = [&](std::size_t i) { return i + 1 < n ? lt.seps[i] : std::string(); };
                for (std::size_t i = 0; i < span.begin; ++i) parts.emplace_back(lt.tokens[i], sepAfter(i));
                const std::size_t m = rec.values.size();
                for (std::size_t k = 0; k < m; ++k) {
                    std::string sep;
                    if (k + 1 < m) sep = span.begin + k + 1 < span.end ? sepAfter(span.begin + k) : lt.defaultSep;
                    else if (span.end > span.begin) sep = sepAfter(span.end - 1);
                    else if (span.end < n) sep = lt.defaultSep;
                    parts.emplace_back(rec.values[k], sep);
                }
                // Fielen alle Werte weg, verbindet der Trenner nach dem letzten Wert den Rest der Zeile.
                if (m == 0 && span.end > span.begin && !parts.empty()) parts.back().second = sepAfter(span.end - 1);
                for (std::size_t i = span.end; i < n; ++i) parts.emplace_back(lt.tokens[i], sepAfter(i));
                outLines[rec.sourceLine] = JoinTokens(lt.prefix, parts, lt.defaultSep);
            } else {
                // Neuer Record (sourceLine==0, nicht Teil der Originaldatei) - direkt nach dem
                // letzten bekannten Zeilenindex dieser Tabelle einfügen. IMMER "#recordin
                // <Tabellenname>" statt nur "#record" verwenden, auch wenn die Originaldatei
                // "#record" nutzt: "#record" gehört beim erneuten Einlesen zur zuletzt per
                // "#Table" deklarierten Tabelle IM DATEISTROM, nicht zwingend zu der Tabelle,
                // in die hier eingefügt wird (siehe World/NPC.txt: "ShineNPC"-Records liegen
                // GANZ am Ende der Datei, weit hinter der letzten "#Table"-Zeile, die "Link-
                // Table" deklariert) - "#recordin" adressiert die Zieltabelle dagegen immer
                // eindeutig per Name.
                // Musterzeile: styleLine, sonst die letzte vorhandene Zeile dieser Tabelle.
                std::size_t style = rec.styleLine;
                if (style == 0 || style >= file.rawLines.size()) {
                    // Letzte tab-getrennte, einzeilige Zeile der Tabelle (nicht die leerzeichengetrennten
                    // Ausnahmezeilen in World/NPC.txt und keine mehrzeiligen Texte).
                    style = 0;
                    for (const auto& other : table.records) {
                        if (other.sourceLine == 0 || other.sourceLine >= file.rawLines.size() || other.sourceLine <= style) continue;
                        if (TokenizeLine(file.rawLines[other.sourceLine], file.spaceDelimiter).defaultSep != "\t") continue;
                        if (CountContinuationLines(file.rawLines, other.sourceLine) > 0) continue;
                        style = other.sourceLine;
                    }
                }
                std::string line;
                if (style > 0) {
                    // "#record" ist nur sicher, wenn vor der Einfügestelle zuletzt diese Tabelle deklariert wurde.
                    std::size_t lastHeader = 0;
                    bool found = false;
                    bool safe = false;
                    for (const auto& t : file.tables) {
                        if (t.headerFirstLine == static_cast<std::size_t>(-1) || t.headerFirstLine > insertAfter) continue;
                        if (!found || t.headerFirstLine >= lastHeader) {
                            lastHeader = t.headerFirstLine;
                            found = true;
                            safe = LowerAsciiCopy(t.name) == LowerAsciiCopy(table.name);
                        }
                    }
                    line = FormatLikeStyleLine(file.rawLines[style], table, rec.values, safe);
                } else {
                    line = "#recordin\t" + table.name + "\t" + joined;
                    if (table.trailingSemicolonSentinel) line += "\t;";
                }
                inserts.emplace_back(insertAfter, std::move(line));
            }
        }
    }

    // Ausgabe: geloeschte Zeilen weglassen, neue Zeilen hinter ihrer Bezugszeile einfuegen.
    std::vector<std::string> finalLines;
    std::vector<std::string> finalEndings;
    finalLines.reserve(outLines.size() + inserts.size());
    finalEndings.reserve(outLines.size() + inserts.size());
    std::size_t nextInsert = 0;
    std::stable_sort(inserts.begin(), inserts.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (std::size_t i = 0; i < outLines.size(); ++i) {
        if (!deleted[i]) {
            finalLines.push_back(outLines[i]);
            finalEndings.push_back(outEndings[i]);
        }
        while (nextInsert < inserts.size() && inserts[nextInsert].first == i) {
            // Fehlte der Zeile davor der Zeilenumbruch (letzte Zeile der Datei), jetzt nachholen.
            if (!finalEndings.empty() && finalEndings.back().empty()) finalEndings.back() = file.defaultLineEnding;
            finalLines.push_back(inserts[nextInsert].second);
            finalEndings.push_back(file.defaultLineEnding);
            ++nextInsert;
        }
    }
    // Bezugszeilen ausserhalb der Datei (sollte nicht vorkommen) ans Ende anhaengen.
    for (; nextInsert < inserts.size(); ++nextInsert) {
        if (!finalEndings.empty() && finalEndings.back().empty()) finalEndings.back() = file.defaultLineEnding;
        finalLines.push_back(inserts[nextInsert].second);
        finalEndings.push_back(file.defaultLineEnding);
    }
    outLines = std::move(finalLines);
    outEndings = std::move(finalEndings);

    std::ofstream out(path, std::ios::binary);
    if (!out) return std::unexpected("Datei konnte nicht zum Schreiben geöffnet werden: " + path.string());
    for (std::size_t i = 0; i < outLines.size(); ++i) out << outLines[i] << outEndings[i];
    return {};
}

ShineTextFile MakeShineFileWithHeaderOf(const ShineTextFile& templateFile, const std::filesystem::path& newPath) {
    ShineTextFile out;
    out.path = newPath;
    out.defaultLineEnding = templateFile.defaultLineEnding;
    std::size_t firstTableLine = templateFile.rawLines.size();
    for (const auto& t : templateFile.tables) {
        if (t.headerFirstLine != static_cast<std::size_t>(-1)) firstTableLine = std::min(firstTableLine, t.headerFirstLine);
    }
    for (std::size_t i = 0; i < firstTableLine && i < templateFile.rawLines.size(); ++i) {
        out.rawLines.push_back(templateFile.rawLines[i]);
        out.rawLineEndings.push_back(i < templateFile.rawLineEndings.size() && !templateFile.rawLineEndings[i].empty()
                                          ? templateFile.rawLineEndings[i] : templateFile.defaultLineEnding);
    }
    return out;
}

} // namespace theseed::mapeditor::core::legacy
