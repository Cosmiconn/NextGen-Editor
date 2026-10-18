#pragma once
// KoreanText.hpp
// Koreanische Texte in Fiesta-Daten: NA2016 enthält rund 1.300 verschiedene Texte in CP949/EUC-KR
// (Mob-Skillnamen, Item-Titel, Set-Namen ...). Dieses Modul erkennt sie, wandelt sie in UTF-8 und
// zurück, schreibt Hangul in lateinische Buchstaben um (revidierte Romanisierung, vereinfacht ohne
// Lautangleichung) und übersetzt über ein Wörterbuch bzw. ein Wort-Glossar.
//
// Einzelne hohe Bytes aus Windows-1252 (z. B. 0xA0 in TextData3) sind KEIN Koreanisch: Ein Text gilt
// nur als koreanisch, wenn alle hohen Bytes gültige CP949-Doppelbytes bilden und mindestens ein
// Hangul-Zeichen darunter ist.

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace theseed::mapeditor::core::legacy {

[[nodiscard]] bool IsKoreanText(std::string_view bytes);

// CP949 -> UTF-8; nullopt bei ungültigen Bytefolgen.
[[nodiscard]] std::optional<std::string> DecodeCp949(std::string_view bytes);
// UTF-8 -> CP949; nullopt, wenn ein Zeichen in CP949 nicht vorkommt oder das UTF-8 ungültig ist.
[[nodiscard]] std::optional<std::string> EncodeCp949(std::string_view utf8);

// Hangul-Silben (UTF-8) in lateinische Buchstaben; alle anderen Zeichen bleiben stehen.
[[nodiscard]] std::string RomanizeHangul(std::string_view utf8);

struct KoreanTranslation {
    std::string english;
    std::string german;
};

// Wörterbuch koreanischer Text (UTF-8) -> Übersetzung. TSV mit drei Spalten (korean, english, german);
// Tab, Zeilenumbruch und Backslash in Texten als \t, \n, \\. Zeilen mit '#' am Anfang sind Kommentare.
class KoreanDictionary {
public:
    bool LoadTsv(const std::filesystem::path& path, std::string* error = nullptr);
    bool SaveTsv(const std::filesystem::path& path, std::string* error = nullptr) const;
    // Fügt die Einträge eines anderen Wörterbuchs hinzu; vorhandene werden überschrieben.
    void Merge(const KoreanDictionary& other);
    void Set(const std::string& korean, KoreanTranslation translation);
    [[nodiscard]] const KoreanTranslation* Find(const std::string& korean) const;
    [[nodiscard]] std::size_t Size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::unordered_map<std::string, KoreanTranslation>& Entries() const noexcept { return entries_; }

private:
    std::unordered_map<std::string, KoreanTranslation> entries_;
};

// Offline-Automatik: bekannte Wörter aus dem Glossar (längstes Wort zuerst) übersetzen, übrige
// Hangul-Folgen umschreiben. english=false: deutsche Glossarspalte.
[[nodiscard]] std::string GlossaryTranslate(std::string_view utf8, const KoreanDictionary& glossary, bool english);

// Spalten, deren Werte Schlüssel sind (Verweise wie *IDX/*Inx/InxName/*Index, Admin-Befehle ALS_Cmd):
// dort wird die Übersetzung angezeigt, aber nie in die Daten übernommen - sonst brächen Verweise oder es
// entstünden doppelte Befehle (z. B. "Hide" neben "hide").
[[nodiscard]] bool IsKoreanKeyColumn(std::string_view columnName);

// TSV-Feld schützen bzw. lesen (\t, \n, \\).
[[nodiscard]] std::string EscapeTsvField(std::string_view text);
[[nodiscard]] std::string UnescapeTsvField(std::string_view text);

} // namespace theseed::mapeditor::core::legacy
