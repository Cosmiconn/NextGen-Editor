// KoreanOnline.cpp - siehe KoreanOnline.hpp.

#include "mapeditor/core/legacy/KoreanOnline.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace theseed::mapeditor::core::legacy {
namespace {

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : s_(text) {}

    std::expected<JsonValue, std::string> Run() {
        JsonValue v;
        if (!Value(v, 0)) return std::unexpected(error_);
        Skip();
        if (i_ != s_.size()) return std::unexpected("JSON: Zeichen nach dem Ende bei " + std::to_string(i_));
        return v;
    }

private:
    std::string_view s_;
    std::size_t i_ = 0;
    std::string error_;

    bool Fail(const char* what) {
        error_ = std::string("JSON: ") + what + " bei " + std::to_string(i_);
        return false;
    }
    void Skip() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }
    bool Literal(std::string_view word) {
        if (s_.substr(i_, word.size()) != word) return Fail("unbekanntes Wort");
        i_ += word.size();
        return true;
    }
    static void AppendUtf8(std::string& out, std::uint32_t cp) {
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
    bool Hex4(std::uint32_t& out) {
        if (i_ + 4 > s_.size()) return Fail("\\u zu kurz");
        out = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s_[i_++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return Fail("\\u ungültig");
        }
        return true;
    }
    bool String(std::string& out) {
        ++i_; // "
        while (i_ < s_.size()) {
            const char c = s_[i_++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (i_ >= s_.size()) break;
            const char e = s_[i_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!Hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && s_.substr(i_, 2) == "\\u") {
                        i_ += 2;
                        std::uint32_t low = 0;
                        if (!Hex4(low)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: return Fail("unbekannte Escape-Folge");
            }
        }
        return Fail("String nicht beendet");
    }
    bool Value(JsonValue& v, int depth) {
        if (depth > 64) return Fail("zu tief verschachtelt");
        Skip();
        if (i_ >= s_.size()) return Fail("Ende erwartet keinen Wert");
        const char c = s_[i_];
        if (c == '{') {
            v.kind = JsonValue::Kind::Object;
            ++i_;
            Skip();
            if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }
            for (;;) {
                Skip();
                if (i_ >= s_.size() || s_[i_] != '"') return Fail("Schlüssel erwartet");
                std::string key;
                if (!String(key)) return false;
                Skip();
                if (i_ >= s_.size() || s_[i_] != ':') return Fail("':' erwartet");
                ++i_;
                if (!Value(v.object[key], depth + 1)) return false;
                Skip();
                if (i_ < s_.size() && s_[i_] == ',') { ++i_; continue; }
                if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }
                return Fail("',' oder '}' erwartet");
            }
        }
        if (c == '[') {
            v.kind = JsonValue::Kind::Array;
            ++i_;
            Skip();
            if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }
            for (;;) {
                v.array.emplace_back();
                if (!Value(v.array.back(), depth + 1)) return false;
                Skip();
                if (i_ < s_.size() && s_[i_] == ',') { ++i_; continue; }
                if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }
                return Fail("',' oder ']' erwartet");
            }
        }
        if (c == '"') { v.kind = JsonValue::Kind::String; return String(v.string); }
        if (c == 't') { v.kind = JsonValue::Kind::Bool; v.boolean = true; return Literal("true"); }
        if (c == 'f') { v.kind = JsonValue::Kind::Bool; return Literal("false"); }
        if (c == 'n') { v.kind = JsonValue::Kind::Null; return Literal("null"); }
        if (c == '-' || (c >= '0' && c <= '9')) {
            const std::size_t start = i_;
            while (i_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[i_])) || s_[i_] == '-' || s_[i_] == '+' ||
                                      s_[i_] == '.' || s_[i_] == 'e' || s_[i_] == 'E')) ++i_;
            v.kind = JsonValue::Kind::Number;
            v.number = std::strtod(std::string(s_.substr(start, i_ - start)).c_str(), nullptr);
            return true;
        }
        return Fail("unerwartetes Zeichen");
    }
};

bool IsAscii(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
}

} // namespace

const JsonValue* JsonValue::Find(const std::string& key) const {
    if (kind != Kind::Object) return nullptr;
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::string JsonValue::StringOr(const std::string& key, std::string fallback) const {
    const JsonValue* v = Find(key);
    return v && v->kind == Kind::String ? v->string : fallback;
}

std::expected<JsonValue, std::string> ParseJson(std::string_view text) { return JsonParser(text).Run(); }

std::string JsonQuote(std::string_view text) {
    std::string out = "\"";
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    return out + "\"";
}

std::string BuildKoreanTranslationRequest(const std::vector<KoreanOnlineItem>& items, const KoreanDictionary& glossary) {
    std::vector<const std::pair<const std::string, KoreanTranslation>*> terms;
    for (const auto& e : glossary.Entries()) terms.push_back(&e);
    std::sort(terms.begin(), terms.end(), [](const auto* a, const auto* b) { return a->first < b->first; });
    std::string glossaryText;
    for (const auto* t : terms) glossaryText += t->first + " = " + t->second.english + " / " + t->second.german + "\n";

    const std::string system =
        "You translate Korean strings from the game data of Fiesta Online, a fantasy MMORPG, into English and German. "
        "The strings are names of skills, monsters, items, item sets, quests and short descriptions or developer comments. "
        "Rules: keep placeholders such as %s, %d, [NAME], <...> and line breaks exactly; keep digits, Latin letters and "
        "punctuation that appear in the source; write names of monsters and places as natural English names (transliterate "
        "when there is no meaning); the English text must use plain ASCII only and must not exceed max_bytes bytes when "
        "max_bytes is greater than 0 (shorten or abbreviate if needed); the German text may use umlauts and has no length "
        "limit. Return exactly one entry per input string, with the korean field copied unchanged.\n\nGame glossary "
        "(korean = english / german):\n" + glossaryText;

    std::string input = "Translate these strings (JSON list of {korean, max_bytes}):\n[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) input += ",";
        input += "{\"korean\":" + JsonQuote(items[i].korean) + ",\"max_bytes\":" + std::to_string(items[i].maxBytes) + "}";
    }
    input += "]";

    const std::string schema =
        "{\"type\":\"object\",\"properties\":{\"translations\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
        "\"properties\":{\"korean\":{\"type\":\"string\"},\"english\":{\"type\":\"string\"},\"german\":{\"type\":\"string\"}},"
        "\"required\":[\"korean\",\"english\",\"german\"],\"additionalProperties\":false}}},"
        "\"required\":[\"translations\"],\"additionalProperties\":false}";

    return std::string("{\"model\":") + JsonQuote(kKoreanOnlineModel) + ",\"max_tokens\":16000,\"fallbacks\":\"default\"," +
           "\"system\":" + JsonQuote(system) + ",\"messages\":[{\"role\":\"user\",\"content\":" + JsonQuote(input) + "}]," +
           "\"output_config\":{\"effort\":\"low\",\"format\":{\"type\":\"json_schema\",\"schema\":" + schema + "}}}";
}

std::expected<KoreanOnlineResult, std::string> ParseKoreanTranslationResponse(int httpStatus, std::string_view body,
                                                                             const std::vector<KoreanOnlineItem>& items) {
    const auto json = ParseJson(body);
    if (httpStatus < 200 || httpStatus >= 300) {
        std::string message;
        if (json)
            if (const JsonValue* err = json->Find("error")) message = err->StringOr("type") + ": " + err->StringOr("message");
        if (message.empty()) message = std::string(body.substr(0, 300));
        if (httpStatus == 401) message = "API-Schlüssel ungültig (401) - " + message;
        return std::unexpected("HTTP " + std::to_string(httpStatus) + ": " + message);
    }
    if (!json) return std::unexpected("Antwort ist kein JSON: " + json.error());
    const std::string stop = json->StringOr("stop_reason");
    if (stop == "refusal") return std::unexpected("Die Anfrage wurde vom Modell abgelehnt (stop_reason: refusal).");
    if (stop == "max_tokens") return std::unexpected("Antwort abgeschnitten (max_tokens) - kleinere Blöcke verwenden.");
    std::string text;
    if (const JsonValue* content = json->Find("content"); content && content->kind == JsonValue::Kind::Array)
        for (const auto& block : content->array)
            if (block.StringOr("type") == "text") text = block.StringOr("text");
    if (text.empty()) return std::unexpected("Antwort ohne Text (stop_reason: " + stop + ").");
    const auto data = ParseJson(text);
    if (!data) return std::unexpected("Übersetzung ist kein gültiges JSON: " + data.error());
    const JsonValue* list = data->Find("translations");
    if (!list || list->kind != JsonValue::Kind::Array) return std::unexpected("Antwort ohne \"translations\".");

    KoreanOnlineResult result;
    std::map<std::string, const KoreanOnlineItem*> wanted;
    for (const auto& item : items) wanted[item.korean] = &item;
    for (const auto& entry : list->array) {
        const std::string korean = entry.StringOr("korean");
        KoreanTranslation tr{entry.StringOr("english"), entry.StringOr("german")};
        const auto it = wanted.find(korean);
        if (it == wanted.end()) { result.problems.push_back("nicht angefragt: " + korean); continue; }
        if (tr.english.empty()) { result.problems.push_back("leer: " + korean); continue; }
        if (!IsAscii(tr.english)) { result.problems.push_back("Englisch nicht ASCII: " + korean + " -> " + tr.english); continue; }
        if (tr.german.empty()) tr.german = tr.english;
        if (it->second->maxBytes > 0 && tr.english.size() > it->second->maxBytes)
            result.problems.push_back("zu lang (" + std::to_string(tr.english.size()) + " > " + std::to_string(it->second->maxBytes) +
                                      " Byte, gespeichert, beim Übernehmen übersprungen): " + korean + " -> " + tr.english);
        result.translations.emplace_back(korean, std::move(tr));
        wanted.erase(it);
    }
    for (const auto& [korean, item] : wanted) result.problems.push_back("fehlt in der Antwort: " + korean);
    return result;
}

} // namespace theseed::mapeditor::core::legacy
