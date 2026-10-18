#pragma once
// KoreanOnline.hpp
// Optionale Online-Übersetzung unbekannter koreanischer Texte über die Claude-API (Messages-Endpunkt).
// Hier liegt nur, was sich ohne Netz testen lässt: Anfrage als JSON bauen und Antwort lesen. Den
// Transport (WinHTTP bzw. curl) und den API-Schlüssel verwaltet die App (KoreanOnlineTranslate.cpp).
//
// Anfrage: strukturierte Ausgabe über output_config.format (JSON-Schema), damit die Antwort sicher
// als {"translations":[{korean, english, german}]} kommt; je Text eine Byte-Grenze für die englische
// Fassung (feste SHN-Spalte minus Abschluss-0). Server-seitige Ausweichmodelle sind eingeschaltet
// ("fallbacks": "default"), damit eine abgelehnte Anfrage automatisch neu läuft.

#include "mapeditor/core/legacy/KoreanText.hpp"

#include <cstddef>
#include <expected>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace theseed::mapeditor::core::legacy {

inline constexpr const char* kKoreanOnlineHost = "api.anthropic.com";
inline constexpr const char* kKoreanOnlinePath = "/v1/messages";
inline constexpr const char* kKoreanOnlineModel = "claude-opus-5-5";
inline constexpr const char* kKoreanOnlineApiVersion = "2023-06-01";
inline constexpr const char* kKoreanOnlineBeta = "server-side-fallback-2026-07-01";
inline constexpr std::size_t kKoreanOnlineBatchSize = 50;

struct KoreanOnlineItem {
    std::string korean;       // UTF-8
    std::size_t maxBytes = 0; // höchstens so viele Byte für die englische Fassung; 0 = keine Grenze
};

// Kleiner JSON-Wert (genug für API-Antworten); Zahlen als double.
struct JsonValue {
    enum class Kind { Null, Bool, Number, String, Array, Object } kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;

    [[nodiscard]] const JsonValue* Find(const std::string& key) const;
    [[nodiscard]] std::string StringOr(const std::string& key, std::string fallback = {}) const;
};

[[nodiscard]] std::expected<JsonValue, std::string> ParseJson(std::string_view text);
// Zeichenkette als JSON-String samt Anführungszeichen.
[[nodiscard]] std::string JsonQuote(std::string_view text);

// Body für POST /v1/messages; das Glossar geht als Begriffsliste in den System-Prompt.
[[nodiscard]] std::string BuildKoreanTranslationRequest(const std::vector<KoreanOnlineItem>& items, const KoreanDictionary& glossary);

struct KoreanOnlineResult {
    std::vector<std::pair<std::string, KoreanTranslation>> translations; // nur angefragte Texte
    std::vector<std::string> problems;                                     // ausgelassene/zu lange Einträge
};

// Liest die HTTP-Antwort (Status und Body). Fehler: API-Fehlermeldung, Ablehnung, abgeschnittene
// Ausgabe oder ungültiges JSON - jeweils als lesbarer Text.
[[nodiscard]] std::expected<KoreanOnlineResult, std::string> ParseKoreanTranslationResponse(int httpStatus, std::string_view body,
                                                                                           const std::vector<KoreanOnlineItem>& items);

} // namespace theseed::mapeditor::core::legacy
