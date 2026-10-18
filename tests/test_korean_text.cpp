// test_korean_text.cpp
// Koreanische Texte: Erkennung (CP949/EUC-KR gegen Windows-1252), Dekodieren und Kodieren aller
// Hangul-Silben ohne Verlust, Umschrift, Wörterbuch (TSV, Escapes, Vorrang beim Zusammenführen),
// Glossar-Automatik, Online-Anfrage bauen und Antwort lesen (ohne Netz). Optional: Pfad zum mitgelieferten Wörterbuch (muss laden und nicht leer sein).

#include "mapeditor/core/legacy/KoreanOnline.hpp"
#include "mapeditor/core/legacy/KoreanText.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace theseed::mapeditor::core::legacy;

namespace {

void Require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "[FEHLER] %s\n", what);
    std::exit(1);
}

std::string Utf8(char32_t cp) {
    std::string s;
    if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    return s;
}

} // namespace

int main(int argc, char** argv) {
    // "검" (sword) = B0 CB in EUC-KR; "공격" (attack) = B0 F8 B0 DD.
    const std::string sword = "\xB0\xCB";
    Require(IsKoreanText(sword), "EUC-KR-Silbe erkannt");
    Require(IsKoreanText("Skill \xB0\xF8\xB0\xDD 01"), "gemischter Text erkannt");
    Require(!IsKoreanText("My conclusion is, um\x85"), "Windows-1252-Auslassung ist kein Koreanisch");
    Require(!IsKoreanText("Title : \xA0%s"), "geschütztes Leerzeichen ist kein Koreanisch");
    Require(!IsKoreanText("plain ascii"), "ASCII ist kein Koreanisch");
    Require(!IsKoreanText("\xB0"), "abgeschnittenes Doppelbyte ist kein Koreanisch");

    const auto decoded = DecodeCp949(sword);
    Require(decoded && *decoded == Utf8(0xAC80), "B0 CB -> U+AC80 (검)");
    Require(DecodeCp949("\xB0\xF8\xB0\xDD") == Utf8(0xACF5) + Utf8(0xACA9), "공격 dekodiert");

    // Alle 11.172 Hangul-Silben über CP949 hin und zurück.
    std::size_t roundtrips = 0;
    for (char32_t cp = 0xAC00; cp <= 0xD7A3; ++cp) {
        const auto bytes = EncodeCp949(Utf8(cp));
        Require(bytes.has_value() && bytes->size() == 2, "jede Hangul-Silbe hat zwei CP949-Bytes");
        Require(IsKoreanText(*bytes), "kodierte Silbe wird erkannt");
        Require(DecodeCp949(*bytes) == Utf8(cp), "Silbe übersteht Hin- und Rückweg");
        ++roundtrips;
    }
    Require(roundtrips == 11172, "11.172 Silben geprüft");
    Require(!EncodeCp949("\xF0\x9F\x98\x80").has_value(), "Emoji ist nicht in CP949");

    Require(RomanizeHangul(Utf8(0xACF5) + Utf8(0xACA9)) == "gonggyeok", "Umschrift 공격");
    Require(RomanizeHangul("A " + Utf8(0xAC80) + "!") == "A geom!", "Umschrift lässt anderes stehen");

    // Wörterbuch: TSV mit Escapes, Merge mit Vorrang.
    KoreanDictionary dict;
    dict.Set(Utf8(0xAC80), {"Sword", "Schwert"});
    dict.Set("a\tb\nc\\", {"x\ty", "z"});
    const auto path = std::filesystem::temp_directory_path() / "test_korean_text.tsv";
    Require(dict.SaveTsv(path), "Wörterbuch gespeichert");
    KoreanDictionary loaded;
    Require(loaded.LoadTsv(path) && loaded.Size() == 2, "Wörterbuch geladen");
    Require(loaded.Find(Utf8(0xAC80)) && loaded.Find(Utf8(0xAC80))->german == "Schwert", "Eintrag gefunden");
    Require(loaded.Find("a\tb\nc\\") && loaded.Find("a\tb\nc\\")->english == "x\ty", "Escapes überstehen Speichern/Laden");
    KoreanDictionary user;
    user.Set(Utf8(0xAC80), {"Blade", "Klinge"});
    loaded.Merge(user);
    Require(loaded.Find(Utf8(0xAC80))->english == "Blade", "Merge überschreibt");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    // Glossar: längstes Wort zuerst, Rest umgeschrieben.
    KoreanDictionary glossary;
    glossary.Set(Utf8(0xACF5) + Utf8(0xACA9), {"attack", "Angriff"});
    glossary.Set(Utf8(0xACF5), {"ball", "Ball"});
    const std::string text = Utf8(0xAC80) + " " + Utf8(0xACF5) + Utf8(0xACA9) + "01";
    Require(GlossaryTranslate(text, glossary, true) == "geom attack01", "Glossar: längstes Wort, Umschrift für den Rest");
    Require(GlossaryTranslate(text, glossary, false) == "geom Angriff01", "Glossar deutsch");

    Require(IsKoreanKeyColumn("ALS_Cmd") && IsKoreanKeyColumn("MobIDX") && IsKoreanKeyColumn("MoverIDX") &&
                IsKoreanKeyColumn("InxName") && IsKoreanKeyColumn("SetItemIndex"), "Schlüsselspalten erkannt");
    Require(!IsKoreanKeyColumn("Name") && !IsKoreanKeyColumn("TitleName") && !IsKoreanKeyColumn("Descript"), "Textspalten sind keine Schlüssel");

    // JSON: Escapes, Unicode, Verschachtelung.
    const auto j = ParseJson(R"({"a":[1,-2.5e1,true,null,"x\"y\u00e4\ud83d\ude00"],"b":{"c":"d"}})");
    Require(j.has_value(), "JSON wird gelesen");
    Require(j->Find("a")->array.size() == 5 && j->Find("a")->array[1].number == -25.0, "JSON-Zahlen");
    Require(j->Find("a")->array[4].string == "x\"y\xC3\xA4\xF0\x9F\x98\x80", "JSON-Escapes und Ersatzpaare");
    Require(j->Find("b")->StringOr("c") == "d", "JSON-Objekt");
    Require(!ParseJson("{\"a\":1,}").has_value() && !ParseJson("[1 2]").has_value(), "ungültiges JSON wird abgelehnt");
    Require(JsonQuote("a\"b\n\x01") == "\"a\\\"b\\n\\u0001\"", "JsonQuote");

    // Anfrage: gültiges JSON mit Modell, Schema, Ausweichmodellen und den Texten samt Byte-Grenze.
    const std::vector<KoreanOnlineItem> items{{Utf8(0xAC80), 31}, {Utf8(0xACF5) + Utf8(0xACA9) + " \"x\"", 0}};
    const std::string request = BuildKoreanTranslationRequest(items, glossary);
    const auto req = ParseJson(request);
    Require(req.has_value(), "Anfrage ist gültiges JSON");
    Require(req->StringOr("model") == kKoreanOnlineModel && req->StringOr("fallbacks") == "default", "Modell und Ausweichmodelle");
    Require(req->Find("thinking") == nullptr, "kein thinking-Feld");
    const JsonValue* format = req->Find("output_config")->Find("format");
    Require(format && format->StringOr("type") == "json_schema" && format->Find("schema")->Find("properties")->Find("translations"),
            "strukturierte Ausgabe über JSON-Schema");
    const std::string userText = req->Find("messages")->array.at(0).StringOr("content");
    Require(userText.find("\"max_bytes\":31") != std::string::npos, "Byte-Grenze in der Anfrage");
    const auto userList = ParseJson(userText.substr(userText.find('[')));
    Require(userList && userList->array.size() == 2 && userList->array[1].StringOr("korean") == items[1].korean,
            "Texte unverändert in der Anfrage");
    Require(req->StringOr("system").find(Utf8(0xACF5) + Utf8(0xACA9) + " = attack / Angriff") != std::string::npos, "Glossar im System-Prompt");

    // Antwort: Text-Block mit JSON; zu lang, fremd, fehlend und Nicht-ASCII werden gemeldet.
    const auto response = [&](const std::string& inner, const std::string& stop) {
        return "{\"content\":[{\"type\":\"thinking\",\"thinking\":\"\"},{\"type\":\"text\",\"text\":" + JsonQuote(inner) +
               "}],\"stop_reason\":\"" + stop + "\"}";
    };
    const std::string inner = "{\"translations\":[{\"korean\":" + JsonQuote(items[0].korean) +
                              ",\"english\":\"Sword\",\"german\":\"Schwert\"},{\"korean\":\"zz\",\"english\":\"x\",\"german\":\"x\"}]}";
    const auto parsed = ParseKoreanTranslationResponse(200, response(inner, "end_turn"), items);
    Require(parsed.has_value() && parsed->translations.size() == 1 && parsed->translations[0].second.german == "Schwert",
            "Antwort gelesen");
    Require(parsed->problems.size() == 2, "fremder und fehlender Text gemeldet");
    const std::string tooLong = "{\"translations\":[{\"korean\":" + JsonQuote(items[0].korean) +
                                ",\"english\":\"" + std::string(40, 'a') + "\",\"german\":\"\"}]}";
    const auto longParsed = ParseKoreanTranslationResponse(200, response(tooLong, "end_turn"), {items[0]});
    Require(longParsed && longParsed->translations.size() == 1 && longParsed->translations[0].second.german.size() == 40 &&
                longParsed->problems.size() == 1, "zu lang: gespeichert, gemeldet; Deutsch fällt auf Englisch zurück");
    const std::string nonAscii = "{\"translations\":[{\"korean\":" + JsonQuote(items[0].korean) +
                                 ",\"english\":\"Schw\xC3\xA4rt\",\"german\":\"x\"}]}";
    Require(ParseKoreanTranslationResponse(200, response(nonAscii, "end_turn"), {items[0]})->translations.empty(), "Englisch muss ASCII sein");
    Require(!ParseKoreanTranslationResponse(200, response("", "refusal"), items).has_value(), "Ablehnung ist ein Fehler");
    Require(!ParseKoreanTranslationResponse(200, response("{\"transl", "max_tokens"), items).has_value(), "abgeschnitten ist ein Fehler");
    const auto http = ParseKoreanTranslationResponse(401, R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})", items);
    Require(!http && http.error().find("invalid x-api-key") != std::string::npos, "API-Fehlermeldung wird weitergegeben");

    if (argc > 1) {
        KoreanDictionary shipped;
        std::string err;
        Require(shipped.LoadTsv(argv[1], &err), "mitgeliefertes Wörterbuch lädt");
        std::printf("Wörterbuch %s: %zu Einträge\n", argv[1], shipped.Size());
        Require(shipped.Size() > 0, "mitgeliefertes Wörterbuch ist nicht leer");
        for (const auto& [ko, tr] : shipped.Entries()) {
            Require(EncodeCp949(ko).has_value(), "jeder Wörterbuch-Schlüssel ist in CP949 darstellbar");
            Require(!tr.english.empty() && !tr.german.empty(), "jeder Eintrag hat Englisch und Deutsch");
        }
    }
    std::printf("Koreanische Texte: ok\n");
    return 0;
}
