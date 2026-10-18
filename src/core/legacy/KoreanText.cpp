// KoreanText.cpp - siehe KoreanText.hpp.

#include "mapeditor/core/legacy/KoreanText.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>

namespace theseed::mapeditor::core::legacy {
namespace {

constexpr unsigned kLeadFirst = 0x81, kLeadLast = 0xFE, kTrailFirst = 0x41, kTrailLast = 0xFE;
constexpr unsigned kTrailCount = kTrailLast - kTrailFirst + 1;

constexpr std::uint16_t kCp949[] = {
#include "Cp949Table.inc"
};
static_assert(std::size(kCp949) == (kLeadLast - kLeadFirst + 1) * kTrailCount);

std::uint32_t Cp949Pair(unsigned char lead, unsigned char trail) {
    if (lead < kLeadFirst || lead > kLeadLast || trail < kTrailFirst || trail > kTrailLast) return 0;
    return kCp949[(lead - kLeadFirst) * kTrailCount + (trail - kTrailFirst)];
}

bool IsHangul(std::uint32_t cp) { return (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0x3131 && cp <= 0x318E); }

void AppendUtf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Nächstes UTF-8-Zeichen; 0xFFFFFFFF bei Fehler.
std::uint32_t NextUtf8(std::string_view s, std::size_t& i) {
    const auto c = static_cast<unsigned char>(s[i]);
    int n = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
    if (n < 0 || i + static_cast<std::size_t>(n) >= s.size() + (n == 0 ? 1 : 0)) { ++i; return 0xFFFFFFFFu; }
    std::uint32_t cp = n == 0 ? c : c & (0x3F >> n);
    for (int k = 1; k <= n; ++k) {
        const auto cc = static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]);
        if ((cc & 0xC0) != 0x80) { ++i; return 0xFFFFFFFFu; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    i += static_cast<std::size_t>(n) + 1;
    return cp;
}

const std::unordered_map<std::uint32_t, std::uint16_t>& ReverseCp949() {
    static const auto map = [] {
        std::unordered_map<std::uint32_t, std::uint16_t> m;
        for (unsigned lead = kLeadFirst; lead <= kLeadLast; ++lead)
            for (unsigned trail = kTrailFirst; trail <= kTrailLast; ++trail)
                if (const std::uint32_t cp = Cp949Pair(static_cast<unsigned char>(lead), static_cast<unsigned char>(trail)); cp != 0)
                    m.emplace(cp, static_cast<std::uint16_t>((lead << 8) | trail));
        return m;
    }();
    return map;
}

constexpr std::array<const char*, 19> kInitial = {"g", "kk", "n", "d", "tt", "r", "m", "b", "pp", "s",
                                                 "ss", "", "j", "jj", "ch", "k", "t", "p", "h"};
constexpr std::array<const char*, 21> kVowel = {"a", "ae", "ya", "yae", "eo", "e", "yeo", "ye", "o", "wa", "wae",
                                               "oe", "yo", "u", "wo", "we", "wi", "yu", "eu", "ui", "i"};
constexpr std::array<const char*, 28> kFinal = {"", "k", "k", "k", "n", "n", "n", "t", "l", "k", "m", "l", "l", "l",
                                               "p", "l", "m", "p", "p", "t", "t", "ng", "t", "t", "k", "t", "p", "t"};

} // namespace

bool IsKoreanText(std::string_view bytes) {
    bool hangul = false;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c < 0x80) continue;
        if (i + 1 >= bytes.size()) return false;
        const std::uint32_t cp = Cp949Pair(c, static_cast<unsigned char>(bytes[i + 1]));
        if (cp == 0) return false;
        hangul = hangul || IsHangul(cp);
        ++i;
    }
    return hangul;
}

std::optional<std::string> DecodeCp949(std::string_view bytes) {
    std::string out;
    out.reserve(bytes.size() * 3 / 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c < 0x80) { out += static_cast<char>(c); continue; }
        if (i + 1 >= bytes.size()) return std::nullopt;
        const std::uint32_t cp = Cp949Pair(c, static_cast<unsigned char>(bytes[i + 1]));
        if (cp == 0) return std::nullopt;
        AppendUtf8(out, cp);
        ++i;
    }
    return out;
}

std::optional<std::string> EncodeCp949(std::string_view utf8) {
    const auto& reverse = ReverseCp949();
    std::string out;
    for (std::size_t i = 0; i < utf8.size();) {
        const std::uint32_t cp = NextUtf8(utf8, i);
        if (cp == 0xFFFFFFFFu) return std::nullopt;
        if (cp < 0x80) { out += static_cast<char>(cp); continue; }
        const auto it = reverse.find(cp);
        if (it == reverse.end()) return std::nullopt;
        out += static_cast<char>(it->second >> 8);
        out += static_cast<char>(it->second & 0xFF);
    }
    return out;
}

std::string RomanizeHangul(std::string_view utf8) {
    std::string out;
    for (std::size_t i = 0; i < utf8.size();) {
        const std::size_t start = i;
        const std::uint32_t cp = NextUtf8(utf8, i);
        if (cp >= 0xAC00 && cp <= 0xD7A3) {
            const std::uint32_t s = cp - 0xAC00;
            out += kInitial[s / (21 * 28)];
            out += kVowel[(s % (21 * 28)) / 28];
            out += kFinal[s % 28];
        } else {
            out.append(utf8.substr(start, i - start));
        }
    }
    return out;
}

bool IsKoreanKeyColumn(std::string_view columnName) {
    std::string lower;
    for (const char c : columnName) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const auto endsWith = [&](std::string_view suffix) {
        return lower.size() >= suffix.size() && lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return lower == "als_cmd" || endsWith("idx") || endsWith("inx") || endsWith("inxname") || endsWith("index");
}

std::string EscapeTsvField(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\t') out += "\\t";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

std::string UnescapeTsvField(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) { out += text[i]; continue; }
        const char n = text[++i];
        out += n == 't' ? '\t' : n == 'n' ? '\n' : n == 'r' ? '\r' : n;
    }
    return out;
}

bool KoreanDictionary::LoadTsv(const std::filesystem::path& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { if (error) *error = "nicht lesbar: " + path.string(); return false; }
    std::string line;
    std::size_t lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (lineNo == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        if (line.empty() || line.front() == '#') continue;
        const auto t1 = line.find('\t');
        if (t1 == std::string::npos) continue;
        const auto t2 = line.find('\t', t1 + 1);
        KoreanTranslation tr;
        tr.english = UnescapeTsvField(std::string_view(line).substr(t1 + 1, t2 == std::string::npos ? std::string::npos : t2 - t1 - 1));
        tr.german = t2 == std::string::npos ? tr.english : UnescapeTsvField(std::string_view(line).substr(t2 + 1));
        if (tr.german.empty()) tr.german = tr.english;
        entries_[UnescapeTsvField(std::string_view(line).substr(0, t1))] = std::move(tr);
    }
    return true;
}

bool KoreanDictionary::SaveTsv(const std::filesystem::path& path, std::string* error) const {
    std::vector<const std::pair<const std::string, KoreanTranslation>*> sorted;
    for (const auto& e : entries_) sorted.push_back(&e);
    std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) { return a->first < b->first; });
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "# korean\tenglish\tgerman\n";
    for (const auto* e : sorted)
        out << EscapeTsvField(e->first) << '\t' << EscapeTsvField(e->second.english) << '\t' << EscapeTsvField(e->second.german) << '\n';
    if (!out) { if (error) *error = "Schreiben fehlgeschlagen: " + path.string(); return false; }
    return true;
}

void KoreanDictionary::Merge(const KoreanDictionary& other) {
    for (const auto& [k, v] : other.entries_) entries_[k] = v;
}

void KoreanDictionary::Set(const std::string& korean, KoreanTranslation translation) { entries_[korean] = std::move(translation); }

const KoreanTranslation* KoreanDictionary::Find(const std::string& korean) const {
    const auto it = entries_.find(korean);
    return it == entries_.end() ? nullptr : &it->second;
}

std::string GlossaryTranslate(std::string_view utf8, const KoreanDictionary& glossary, bool english) {
    // Schlüssel nach Länge absteigend (längstes Wort zuerst); einmal je Glossar-Größe aufgebaut.
    std::vector<const std::pair<const std::string, KoreanTranslation>*> keys;
    for (const auto& e : glossary.Entries()) keys.push_back(&e);
    std::sort(keys.begin(), keys.end(), [](const auto* a, const auto* b) { return a->first.size() > b->first.size(); });

    std::string out;
    std::string pendingHangul;
    const auto flushHangul = [&] {
        if (pendingHangul.empty()) return;
        if (!out.empty() && out.back() != ' ' && out.back() != '(' && out.back() != '[') out += ' ';
        out += RomanizeHangul(pendingHangul);
        pendingHangul.clear();
    };
    for (std::size_t i = 0; i < utf8.size();) {
        bool matched = false;
        for (const auto* k : keys) {
            if (k->first.empty() || utf8.compare(i, k->first.size(), k->first) != 0) continue;
            flushHangul();
            if (!out.empty() && out.back() != ' ' && out.back() != '(' && out.back() != '[') out += ' ';
            out += english ? k->second.english : k->second.german;
            i += k->first.size();
            matched = true;
            break;
        }
        if (matched) continue;
        const std::size_t start = i;
        const std::uint32_t cp = NextUtf8(utf8, i);
        if (cp >= 0xAC00 && cp <= 0xD7A3) {
            pendingHangul.append(utf8.substr(start, i - start));
        } else {
            flushHangul();
            out.append(utf8.substr(start, i - start));
        }
    }
    flushHangul();
    return out;
}

} // namespace theseed::mapeditor::core::legacy
