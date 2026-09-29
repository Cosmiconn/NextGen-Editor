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
    for (std::size_t lineIdx = 0; lineIdx < file.rawLines.size(); ++lineIdx) {
        const std::string& raw = file.rawLines[lineIdx];
        if (raw.empty()) continue;
        auto tokens = SplitTab(raw);
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

        if (firstLower == "#table") {
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
        } else if (firstLower == "#columntype") {
            if (!current) continue;
            for (std::size_t i = argBase; i < tokens.size(); ++i) {
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
            for (std::size_t i = argBase; i < tokens.size(); ++i) {
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
            if (!current->columns.empty() &&
                TrimAscii(current->columns.back().name) == ";" &&
                TrimAscii(current->columns.back().type) == ";") {
                current->columns.pop_back();
                current->trailingSemicolonSentinel = true;
            }
            current->lastLine = lineIdx;
            current->headerLastLine = lineIdx;
        } else if (firstLower == "#record") {
            if (!current) continue;
            std::vector<std::string> values(tokens.begin() + static_cast<std::ptrdiff_t>(argBase), tokens.end());
            TrimTrailingEmpty(values);
            if (current->trailingSemicolonSentinel && !values.empty() &&
                TrimAscii(values.back()) == ";") {
                values.pop_back();
            }
            // Dasselbe Leer-Tab-Muster wie bei #ColumnType/#ColumnName (siehe dort) tritt bei
            // manchen Dateien (z.B. MobRegen/<Karte>.txt) auch bei "#record" selbst auf - genau
            // ein Wert zu viel, und der erste ist leer, während die Spaltenliste bereits korrekt
            // bereinigt wurde.
            if (values.size() == current->columns.size() + 1 && !values.empty() && TrimAscii(values[0]).empty()) {
                values.erase(values.begin());
            }
            current->records.push_back(ShineRecord{values, lineIdx});
            file.loadedRecordLines.push_back(lineIdx);
            current->lastLine = lineIdx;
        } else if (firstLower == "#recordin") {
            // "#recordin <Tabellenname> <werte...>" - siehe World/NPC.txt. Zielt explizit auf
            // eine per Namen benannte Tabelle statt der zuletzt deklarierten.
            if (tokens.size() < argBase + 1) continue;
            std::string targetName = TrimAscii(tokens[argBase]);
            ShineTable* target = file.FindTable(targetName);
            if (!target) continue;
            std::vector<std::string> values(tokens.begin() + static_cast<std::ptrdiff_t>(argBase) + 1, tokens.end());
            TrimTrailingEmpty(values);
            if (target->trailingSemicolonSentinel && !values.empty() &&
                TrimAscii(values.back()) == ";") {
                values.pop_back();
            }
            target->records.push_back(ShineRecord{values, lineIdx});
            file.loadedRecordLines.push_back(lineIdx);
            target->lastLine = lineIdx;
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
                // Bestehender Record - nur diese eine Zeile ersetzen, Format ("#record"/
                // "#recordin <Name>") und eventuelle Auffüll-Tabs am Ende beibehalten, damit
                // der Rest der Zeile (und der Datei) unverändert bleibt. Manche Dateien haben
                // ein führendes Leer-Tab VOR der Direktive (siehe LoadShineTextFile) - deshalb
                // hier genauso das erste NICHT-leere Token suchen statt origTokens[0] blind zu
                // nehmen (sonst geht z.B. "#recordin" mitsamt Tabellen-Name verloren).
                auto origTokens = SplitTab(outLines[rec.sourceLine]);
                std::size_t dIdx = 0;
                while (dIdx < origTokens.size() && TrimAscii(origTokens[dIdx]).empty()) ++dIdx;
                std::string leadingBlanks(dIdx, '\t');
                std::string directive = dIdx < origTokens.size() ? origTokens[dIdx] : "#record";
                std::string directiveLower = LowerAsciiCopy(TrimAscii(directive));
                std::string newLine = leadingBlanks + directive;
                if (directiveLower == "#recordin" && origTokens.size() > dIdx + 1) {
                    newLine += "\t" + origTokens[dIdx + 1];
                }
                // Letztes nicht-leeres Token der Originalzeile (Grundlage fuer Auffuell-Tabs unten).
                std::size_t lastNonEmpty = origTokens.size();
                for (std::size_t ti = origTokens.size(); ti-- > 0;) {
                    if (!TrimAscii(origTokens[ti]).empty()) { lastNonEmpty = ti; break; }
                }
                const bool hadTrailingSemicolon =
                    table.trailingSemicolonSentinel && lastNonEmpty < origTokens.size() &&
                    TrimAscii(origTokens[lastNonEmpty]) == ";";
                const std::size_t logicalLastNonEmpty =
                    hadTrailingSemicolon && lastNonEmpty > 0 ? lastNonEmpty - 1 : lastNonEmpty;

                // "#record\t\t<wert>...": das zusaetzliche Leer-Tab nach der Direktive hat der Loader
                // aus den Werten entfernt (siehe LoadShineTextFile) - hier exakt wieder einsetzen.
                if (directiveLower == "#record" && origTokens.size() > dIdx + 1 &&
                    TrimAscii(origTokens[dIdx + 1]).empty() && logicalLastNonEmpty < origTokens.size() &&
                    logicalLastNonEmpty > dIdx &&
                    (logicalLastNonEmpty - dIdx) == table.columns.size() + 1) {
                    newLine += "\t" + origTokens[dIdx + 1];
                }
                newLine += "\t" + joined;
                if (hadTrailingSemicolon) newLine += "\t" + origTokens[lastNonEmpty];

                // Auffuell-Tabs (und alles, was hinter dem letzten nicht-leeren Token der
                // Originalzeile steht) unveraendert anhaengen - echte Dateien fuellen jede Zeile
                // auf eine feste Spaltenzahl auf. Ein unveraenderter Record bleibt so exakt gleich.
                if (lastNonEmpty < origTokens.size()) {
                    for (std::size_t ti = lastNonEmpty + 1; ti < origTokens.size(); ++ti) newLine += "\t" + origTokens[ti];
                }
                outLines[rec.sourceLine] = newLine;
                deleted[rec.sourceLine] = 0;
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
                std::string line = "#recordin\t" + table.name + "\t" + joined;
                if (table.trailingSemicolonSentinel) line += "\t;";
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
