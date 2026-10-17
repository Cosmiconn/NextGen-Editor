#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <expected>

namespace theseed::mapeditor::core::legacy {

// Parser für das server-seitige "Shine"-Text-Tabellenformat (#Table/#ColumnType/#ColumnName/
// #record), das eigenständig NEBEN den binären .shn-Dateien existiert - z.B.
// Server/9Data/Shine/World/NPC.txt (NPC-Platzierung), Server/9Data/Shine/MobRegen/<Karte>.txt
// (Mob-Spawns, zwei verknüpfte Tabellen pro Datei) und Server/9Data/Shine/NPCItemList/
// <NPC>.txt (Händler-Inventar, mehrere Tabs pro Datei). Byte-exakt gegen die echten NA2016-
// Dateien verifiziert: die Datei behauptet in ihrem eigenen Kopf "#delimiter\x20;" (Leerzeichen)
// - das TATSÄCHLICHE Trennzeichen ist aber fast durchgängig TAB (\t), nicht Leerzeichen. Diesem Fund
// folgt der Parser; nur Record-Zeilen ganz ohne Tab-Trennung werden dann an Leerzeichen getrennt
// (siehe ShineTextFile::spaceDelimiter).
//
// Eine Datei kann MEHRERE Tabellen enthalten (z.B. MobRegen/<Karte>.txt: erst
// "MobRegenGroup", dann "MobRegen"). Records werden entweder mit "#record" (gehört zur zuletzt
// per "#Table" deklarierten Tabelle) oder "#recordin <Tabellenname>" (explizit benannt, siehe
// World/NPC.txt) eingefügt - beide Schreibweisen kommen in echten Dateien vor, Groß-/
// Kleinschreibung der Direktiven variiert ebenfalls zwischen Dateien ("#Table" vs "#table").
//
// Werte werden bewusst als reine Strings gehalten (nicht in int/float konvertiert) - die realen
// Spaltentypen (INDEX/STRING[n]/DWRD/BYTE/WORD) sind Hinweise, keine strikte Kodierung, und
// einige Datensätze (Kommentarzeilen mit ";" am Anfang) werden ohnehin übersprungen.
//
// ";" leitet auch MITTEN in einer Zeile einen Kommentar ein (Kopf- wie Record-Zeilen, z. B.
// "#Record\tOutOfRange\t-\t-\tKarenWingAtk\t; Kommentar"): ab dem ersten Token, das mit ";" beginnt,
// gehört der Rest nicht mehr zu den Werten bzw. Spalten. Er bleibt beim Speichern unverändert
// stehen, auch wenn der Record bearbeitet wird. Werte jenseits der Spaltenzahl (belegt an
// Script/Honeying.txt) werden genauso unverändert mitgeführt, aber nicht als Werte gelesen.

struct ShineColumn {
    std::string name;
    std::string type; // Rohtext aus #ColumnType, z.B. "DWRD", "STRING[33]", "INDEX", "BYTE"
};

struct ShineRecord {
    std::vector<std::string> values;
    // Index der Quellzeile in ShineTextFile::rawLines - für minimal-invasives Speichern
    // (siehe SaveShineTextFile): nur diese Zeile wird beim Bearbeiten ersetzt, der Rest der
    // Datei (Kommentare, unbekannte Direktiven, auch nicht-ASCII/koreanische Textteile) bleibt
    // byte-identisch erhalten.
    std::size_t sourceLine = 0;
    // Nur für neue Records (sourceLine == 0): Quellzeile einer Musterzeile derselben Tabelle, deren
    // Form der neue Record übernimmt (führende Tabs, Direktive, Leerzeichen hinter Zahlen,
    // Auffüll-Tabs) - z. B. die Vorlagenzeile, aus der er geklont wurde. 0 = letzte vorhandene
    // Zeile der Tabelle als Muster.
    std::size_t styleLine = 0;
    // Nur zur Information (vom Loader gesetzt): Anzahl der Folgezeilen eines Texts, der über mehrere
    // Zeilen läuft (belegt an Script/Scenario.txt). Der letzte Wert enthält dann den ganzen Text samt
    // Zeilenumbrüchen. Solche Records lassen sich nur unverändert speichern.
    std::size_t continuationLines = 0;
};

struct ShineTable {
    std::string name;
    std::vector<ShineColumn> columns;
    std::vector<ShineRecord> records;
    // Zeilenindex der letzten Zeile dieser Tabelle (letzter Record oder Spaltendefinition,
    // je nachdem was später kommt) - neue Records werden direkt danach eingefügt.
    std::size_t lastLine = 0;
    // Zeilen der Kopfdefinition (#Table ... letzte #ColumnName-Zeile) in der Originaldatei - damit
    // das Entfernen einer Tabelle auch ihren Kopf aus der Datei nimmt (npos = neu angelegt).
    std::size_t headerFirstLine = static_cast<std::size_t>(-1);
    std::size_t headerLastLine = 0;
    // true: vom Editor neu angelegte Tabelle - wird beim Speichern mit Kopf (#Table/#ColumnType/
    // #ColumnName) und "#Record"-Zeilen ans Dateiende geschrieben.
    bool isNew = false;
    // Einige breite Shine-Tabellen (belegt an World/ItemDropTable.txt) hängen in
    // #ColumnType/#ColumnName ein zusätzliches "\t;" als Zeilen-/Kommentar-Sentinel an.
    // Das ist KEINE Datenspalte. Der Loader entfernt es aus columns/records, merkt sich hier
    // aber die Form, damit SaveShineTextFile bestehende und neue Records formatgetreu schreibt.
    bool trailingSemicolonSentinel = false;
};

struct ShineTextFile {
    std::filesystem::path path;
    std::vector<ShineTable> tables;
    // Alle Originalzeilen roh (als Bytes->String ohne Encoding-Annahme) - Grundlage für
    // minimal-invasives Speichern. NICHT für die Anzeige gedacht (kann nicht-UTF8-Bytes
    // enthalten, z.B. EUC-KR-Kommentare in manchen Dateien).
    std::vector<std::string> rawLines;
    // Zeilenende je Originalzeile ("\r\n", "\n" oder "" fuer eine letzte Zeile ohne Zeilenumbruch),
    // parallel zu rawLines - damit unveraenderte Zeilen beim Speichern byte-identisch bleiben
    // (echte Dateien sind teils LF, teils CRLF, teils gemischt). Neue Zeilen erhalten
    // defaultLineEnding (Zeilenende der ersten Zeile der Datei, sonst CRLF).
    // Quellzeilen ALLER beim Laden erkannten Records (aller Tabellen) - Grundlage fuer das
    // Loeschen: ein Record, der aus ShineTable::records entfernt wurde, verschwindet beim
    // Speichern auch aus der Datei. Leer bei von Hand gebauten Objekten (dann wird nie geloescht).
    std::vector<std::size_t> loadedRecordLines;
    // Kopfzeilen-Bereiche (erste, letzte Zeile) aller beim Laden erkannten Tabellen.
    std::vector<std::pair<std::size_t, std::size_t>> loadedTableHeaders;
    std::vector<std::string> rawLineEndings;
    std::string defaultLineEnding = "\r\n";
    // Die Datei deklariert "#delimiter \x20" (nicht auskommentiert). Dann werden Record-Zeilen, die
    // statt Tabs Leerzeichen als Trenner nutzen (belegt an World/NPC.txt: drei ShineNPC-Zeilen), an
    // Leerzeichenläufen getrennt - sonst bleiben sie mit Tabs getrennt wie alle anderen.
    bool spaceDelimiter = false;

    ShineTable* FindTable(const std::string& name);
    const ShineTable* FindTable(const std::string& name) const;
};

std::expected<ShineTextFile, std::string> LoadShineTextFile(const std::filesystem::path& path);

// Minimal-invasiv: ersetzt nur die Zeilen bereits vorhandener (per sourceLine referenzierter)
// Records, die sich gegenüber dem Original unterscheiden, und hängt neue Records (sourceLine
// == 0 und nicht in den Originalzeilen enthalten) direkt nach der jeweiligen Tabelle an - der
// Rest der Datei bleibt byte-identisch. Neue Records übernehmen die Form einer Musterzeile
// derselben Tabelle (ShineRecord::styleLine, sonst ihre letzte Zeile): führende Tabs, "#Record"
// bzw. "#recordin <Tabelle>", Leerzeichen hinter Zahlen und Auffüll-Tabs.
std::expected<void, std::string> SaveShineTextFile(const ShineTextFile& file, const std::filesystem::path& path);

// Neue, noch tabellenlose Datei mit denselben Kopfzeilen (#ignore/#exchange/Kommentare vor der
// ersten Tabelle) und Zeilenenden wie `templateFile` - Grundlage, um z.B. fuer einen neuen NPC eine
// NPCItemList/<NPC>.txt anzulegen. Tabellen danach mit isNew=true anhaengen und speichern.
ShineTextFile MakeShineFileWithHeaderOf(const ShineTextFile& templateFile, const std::filesystem::path& newPath);

} // namespace theseed::mapeditor::core::legacy
