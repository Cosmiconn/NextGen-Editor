#include "mapeditor/core/NifModel.hpp"
#include "mapeditor/core/DdsImage.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace theseed::mapeditor::core {

namespace {

// ---------------------------------------------------------------------------------------------
// Byte-Reader mit Bounds-Check (wirft niemals, meldet Fehler über den Rückgabewert der
// aufrufenden Funktion via ok_-Flag - vermeidet Exceptions in einem Parser, der mit potenziell
// unbekannten/fehlerhaften Block-Layouts umgehen muss).
// ---------------------------------------------------------------------------------------------
class ByteReader {
public:
    explicit ByteReader(const std::vector<std::uint8_t>& data) : data_(data) {}

    [[nodiscard]] bool Ok() const noexcept { return ok_; }
    [[nodiscard]] std::size_t Pos() const noexcept { return pos_; }
    void SetPos(std::size_t p) { pos_ = p; }
    // Erzwingt einen sauberen Fehlschlag (z.B. bei einer erkannten, aber nicht unterstützten
    // Sub-Variante eines Blocks) - führt zur selben "Unerwartetes Dateiende"-Behandlung wie ein
    // echter Bounds-Fehler, statt mit falscher Byte-Position weiterzulesen.
    void Invalidate() noexcept { ok_ = false; }
    // Für gezielte, eng begrenzte Rückfallversuche (siehe ParseNiTexturingProperty): erlaubt,
    // eine durch CountU32/CountU16 ausgelöste Invalidierung gezielt rückgängig zu machen,
    // NACHDEM die Position zurückgesetzt wurde und ein alternativer Parse-Pfad versucht wird.
    void SetOk(bool v) noexcept { ok_ = v; }

    std::uint8_t U8() { return Read<std::uint8_t>(); }
    std::uint16_t U16() { return Read<std::uint16_t>(); }
    std::int16_t I16() { return static_cast<std::int16_t>(Read<std::uint16_t>()); }
    std::uint32_t U32() { return Read<std::uint32_t>(); }
    std::int32_t I32() { return static_cast<std::int32_t>(Read<std::uint32_t>()); }
    float F32() {
        const std::uint32_t bits = Read<std::uint32_t>();
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }

    // Wie U32(), aber mit Plausibilitätsgrenze für Array-/Listenlängen - verhindert riesige
    // Allokationsversuche (bad_alloc), wenn der Parser durch einen unbekannten/unerwarteten
    // Block-Aufbau "vom Kurs abkommt" und Datenbytes fälschlich als Zähler liest.
    std::uint32_t CountU32(std::uint32_t maxPlausible = 200'000u) {
        const std::uint32_t v = U32();
        if (v > maxPlausible) {
            ok_ = false;
            return 0;
        }
        return v;
    }
    std::uint16_t CountU16(std::uint16_t maxPlausible = 60000u) {
        const std::uint16_t v = U16();
        if (v > maxPlausible) {
            ok_ = false;
            return 0;
        }
        return v;
    }

    std::string SizedString() {
        const std::uint32_t len = CountU32(100000u); // Strings sind nie extrem lang
        if (!ok_ || pos_ + len > data_.size()) {
            ok_ = false;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(data_.data() + pos_), len);
        pos_ += len;
        return s;
    }

    void Skip(std::size_t n) {
        if (pos_ + n > data_.size()) {
            ok_ = false;
            return;
        }
        pos_ += n;
    }

    std::vector<std::uint8_t> Bytes(std::size_t n) {
        if (pos_ + n > data_.size()) { ok_ = false; return {}; }
        std::vector<std::uint8_t> out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                                      data_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
        pos_ += n;
        return out;
    }

    // Liest einen uint32 an einer Position relativ zur aktuellen, OHNE die Position zu
    // verändern - für Plausibilitäts-Heuristiken (siehe LooksLikeFreshName).
    [[nodiscard]] std::uint32_t PeekU32(std::size_t offset) const {
        if (pos_ + offset + 4 > data_.size()) return 0xFFFFFFFFu;
        std::uint32_t value{};
        std::memcpy(&value, data_.data() + pos_ + offset, sizeof(value));
        return value;
    }

    // Wie PeekU32, aber ein einzelnes Byte - für Plausibilitäts-Heuristiken.
    [[nodiscard]] std::uint8_t PeekU8(std::size_t offset) const {
        if (pos_ + offset + 1 > data_.size()) return 0xFFu;
        return data_[pos_ + offset];
    }

private:
    template <typename T>
    T Read() {
        if (!ok_ || pos_ + sizeof(T) > data_.size()) {
            ok_ = false;
            return T{};
        }
        T value{};
        std::memcpy(&value, data_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return value;
    }

    const std::vector<std::uint8_t>& data_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

// Allzweck-Plausibilitätsheuristik: prüft, ob an der aktuellen Position (plus optionalem
// Offset) ein "frischer" SizedString-Blockanfang beginnen könnte, wie ihn praktisch jeder
// Blocktyp dieses Formats als erstes Feld hat (der Name aus NiObjectNET/AVObjectBase). Eine
// Länge von 0 (leerer Name, der häufigste Fall) gilt als plausibel; eine Länge zwischen 1 und
// 64 gilt nur dann als plausibel, wenn die entsprechenden Bytes ausschließlich druckbares
// ASCII sind (Namen sind reine Text-Bezeichner wie "__MAX_Default_Light"). Verwendet als
// robuster Kompatibilitäts-Check für konditionale Trailer, deren genaue Bedingung nicht immer
// abschließend geklärt ist (siehe Verwendungsstellen).
bool LooksLikeFreshName(const ByteReader& r, std::size_t offset) {
    const std::uint32_t len = r.PeekU32(offset);
    if (len == 0) return true;
    if (len > 64) return false;
    for (std::uint32_t i = 0; i < len; ++i) {
        const std::uint8_t b = r.PeekU8(offset + 4 + i);
        if (b < 0x20 || b > 0x7E) return false;
    }
    return true;
}

struct NifHeader {
    std::uint32_t version = 0;
    std::uint32_t numBlocks = 0;
    std::vector<std::string> blockTypes;
    std::vector<std::uint16_t> blockTypeIndex;
};

std::expected<NifHeader, std::string> ParseHeader(ByteReader& r, const std::vector<std::uint8_t>& data) {
    const auto it = std::find(data.begin(), data.end(), static_cast<std::uint8_t>('\n'));
    if (it == data.end()) {
        return std::unexpected("Kein NIF-Header-String gefunden (fehlendes '\\n')");
    }
    const std::string headerStr(data.begin(), it);
    if (headerStr.find("Gamebryo") == std::string::npos && headerStr.find("NetImmerse") == std::string::npos) {
        return std::unexpected("Keine erkennbare Gamebryo/NetImmerse-Signatur: " + headerStr);
    }
    r.SetPos(static_cast<std::size_t>(it - data.begin()) + 1);

    NifHeader hdr;
    hdr.version = r.U32();
    // KORRIGIERT: ältere Dateien (Version 10.1.0.0 und 10.2.0.0 - 177 Stück im Testkorpus)
    // haben KEIN Endian-Byte zwischen Version und User-Version, anders als 20.0.0.4. Byte-
    // exakt ermittelt durch systematisches Ausprobieren mehrerer Kandidaten-Offsets an
    // horse2.nif (10.2.0.0): mit Endian-Byte ergeben sich Garbage-Werte für numBlockTypes,
    // ohne Endian-Byte (direkt user_version(u32)+num_blocks(u32)+num_block_types(u16))
    // landet man exakt auf einer gültigen, mit "NiNode" beginnenden Blocktyp-Tabelle -
    // verifiziert an 3 echten Dateien (2x Version 10.2.0.0, 1x Version 10.1.0.0).
    if (hdr.version == 0x14000004u) {
        r.U8(); // endian
    }
    r.U32(); // user_version
    hdr.numBlocks = r.CountU32(100000u);
    const std::uint16_t numBlockTypes = r.CountU16(2000u);
    hdr.blockTypes.reserve(numBlockTypes);
    for (std::uint16_t i = 0; i < numBlockTypes; ++i) {
        hdr.blockTypes.push_back(r.SizedString());
    }
    hdr.blockTypeIndex.reserve(hdr.numBlocks);
    for (std::uint32_t i = 0; i < hdr.numBlocks; ++i) {
        hdr.blockTypeIndex.push_back(r.U16());
    }
    const std::uint32_t numGroups = r.CountU32(10000u);
    for (std::uint32_t i = 0; i < numGroups; ++i) {
        r.U32();
    }
    // KORRIGIERT: Version 10.1.0.0 (NICHT 10.2.0.0 - dort tritt dies NICHT auf) hat direkt
    // nach den Groups ein zusätzliches, in der autoritativen nif.xml nicht dokumentiertes
    // 4-Byte-Feld (Wert in allen 7 betroffenen Testdateien exakt 0 - Bedeutung ungeklärt,
    // evtl. ein Relikt eines älteren Root-Objekt-Zeigers). Byte-exakt an ALLEN 7 Dateien
    // dieser Version im Testkorpus verifiziert (EnvSet.nif, RouTempDn01_ground.nif,
    // TreeThin2.nif, thornOnly.nif, thornOnlytop.nif, treeThin.nif, MapLinkGate.nif): ohne
    // dieses Feld beginnt Block 0 mit einem impliziten Fehlversuch, mit ihm landet man exakt
    // auf einem gültigen, lesbaren Namensfeld ("Scene Root" - der bei 3D-Exportern übliche
    // Standardname für den Wurzelknoten).
    if (hdr.version == 0x0a010000u) {
        r.U32();
    }

    if (!r.Ok()) {
        return std::unexpected("Unerwartetes Dateiende im NIF-Header");
    }
    // Versionen 10.1.0.0 und 10.2.0.0 werden strukturell wie 20.0.0.4 weiterverarbeitet (die
    // Blocktyp-Namen sind identisch benannt, siehe oben) - ob die BLOCK-INHALTE selbst
    // durchgängig byte-kompatibel sind, ist noch nicht vollständig verifiziert; einzelne
    // Blocktypen könnten in dieser älteren Ära abweichen und schlagen dann sauber (Bounds-
    // Check) statt mit falscher Geometrie fehl.
    if (hdr.version != 0x14000004u && hdr.version != 0x0a020000u && hdr.version != 0x0a010000u) {
        return std::unexpected("Nicht unterst\u00fctzte NIF-Version (nur 20.0.0.4, 10.2.0.0, 10.1.0.0 verifiziert)");
    }
    return hdr;
}

struct ObjectNetBase {
    std::string name;
    std::int32_t controller = -1;
};

ObjectNetBase ParseObjectNetBase(ByteReader& r) {
    ObjectNetBase base;
    base.name = r.SizedString();
    // KORRIGIERT (siehe docs/MAP_FORMAT.md Abschnitt 16/17/23): das num_extra_data_refs-Feld
    // fehlt nicht nur, wenn der Wert an dieser Stelle exakt 0xFFFFFFFF ist (controller=-1),
    // sondern auch, wenn der Controller ein KLEINER, gültiger Block-Index ist (z.B. 6) - an
    // AdlFH_field_burn_ground.nif gefunden: peek=6 wurde bisher als "6 echte Extra-Daten-Refs"
    // fehlinterpretiert, deren Werte allesamt als Fließkomma-Bitmuster (1065353216 = 1.0f)
    // aussahen - eindeutig KEINE gültigen Block-Referenzen. Generalisierter, aber weiterhin
    // konservativer Peek: bei einem potenziellen Zähler zwischen 1 und 1000 werden die
    // dadurch implizierten Extra-Daten-Refs UND der direkt danach folgende Controller-Wert auf
    // Plausibilität geprüft (jeweils entweder -1 oder ein "kleiner" Wert, großzügige Grenze
    // 100000 - weit über jeder realistischen Blockzahl, aber weit unter einem als Ganzzahl
    // reinterpretierten Float-Bitmuster). Nur wenn diese Prüfung fehlschlägt, wird das Feld
    // als abwesend behandelt. Bei peek=0 (der weit überwiegenden Mehrheit aller Dateien)
    // ändert sich nichts.
    const std::uint32_t peek0 = r.PeekU32(0);
    bool numExtraAbsent = false;
    if (peek0 == 0) {
        // Normalfall: unverändert.
    } else if (peek0 > 1000u) {
        numExtraAbsent = true;
    } else {
        bool allPlausible = true;
        for (std::uint32_t i = 0; i < peek0 && allPlausible; ++i) {
            const std::uint32_t v = r.PeekU32(4 + 4 * i);
            if (v != 0xFFFFFFFFu && v > 100000u) allPlausible = false;
        }
        if (allPlausible) {
            const std::uint32_t afterExtras = r.PeekU32(4 + 4 * peek0);
            if (afterExtras != 0xFFFFFFFFu && afterExtras > 100000u) allPlausible = false;
        }
        numExtraAbsent = !allPlausible;
    }
    if (!numExtraAbsent) {
        const std::uint32_t numExtra = r.CountU32(1000u);
        for (std::uint32_t i = 0; i < numExtra; ++i) {
            r.I32();
        }
    }
    base.controller = r.I32();
    return base;
}

struct AVObjectBase {
    ObjectNetBase net;
    NifVec3 translation;
    std::array<float, 9> rotation{};
    float scale = 1.0f;
    std::vector<std::int32_t> properties;
};

AVObjectBase ParseAVObjectBase(ByteReader& r) {
    AVObjectBase base;
    base.net = ParseObjectNetBase(r);
    r.U16(); // flags
    base.translation = {r.F32(), r.F32(), r.F32()};
    for (float& v : base.rotation) v = r.F32();
    base.scale = r.F32();
    const std::uint32_t numProps = r.CountU32(1000u);
    base.properties.reserve(numProps);
    for (std::uint32_t i = 0; i < numProps; ++i) {
        base.properties.push_back(r.I32());
    }
    r.I32(); // collision_object ref
    return base;
}

// NiPortal: NiAVObject-Basis + portal_flags(u16) + plane_count(u16, laut Referenz in Version
// 20.x unbenutzt, aber weiterhin im Byte-Layout vorhanden) + num_vertices(u16) +
// vertices(Vector3-Liste) + adjoiner_ptr(i32).
void SkipNiPortal(ByteReader& r) {
    ParseAVObjectBase(r);
    r.U16(); // portal_flags
    r.U16(); // plane_count
    const std::uint32_t numVertices = r.CountU16(256u);
    r.Skip(static_cast<std::size_t>(numVertices) * 12u);
    r.I32(); // adjoiner
}

struct NiNodeBlock {
    AVObjectBase base;
    std::vector<std::int32_t> children;
};

NiNodeBlock ParseNiNode(ByteReader& r) {
    NiNodeBlock node;
    node.base = ParseAVObjectBase(r);
    const std::uint32_t numChildren = r.CountU32(10000u);
    node.children.reserve(numChildren);
    for (std::uint32_t i = 0; i < numChildren; ++i) {
        node.children.push_back(r.I32());
    }
    const std::uint32_t numEffects = r.CountU32(1000u);
    for (std::uint32_t i = 0; i < numEffects; ++i) {
        r.I32();
    }
    return node;
}

// NiSortAdjustNode: NiNode + sorting_mode(u32-Enum). Das früher hier vorhandene
// "Accumulator"-Ref-Feld gilt laut autoritativer Referenz nur bis Version 20.0.0.3 - unsere
// Version (20.0.0.4) liegt bereits danach, das Feld entfällt also.
NiNodeBlock ParseNiSortAdjustNode(ByteReader& r) {
    NiNodeBlock node = ParseNiNode(r);
    r.U32(); // sorting_mode
    return node;
}

// NiRoomGroup: NiNode + shell(Ptr, i32) + num_rooms(u32) + rooms(Ptr-Liste, je i32).
NiNodeBlock ParseNiRoomGroup(ByteReader& r) {
    NiNodeBlock node = ParseNiNode(r);
    r.I32(); // shell
    const std::uint32_t numRooms = r.CountU32(1000u);
    for (std::uint32_t i = 0; i < numRooms; ++i) {
        r.I32();
    }
    return node;
}

// NiRoom: NiNode + num_walls(u32) + wall_planes(NiPlane=16 Byte je Eintrag, seit Version
// 4.0.0.0 - das ältere "Walls"-Ref-Feld gilt nur bis 3.3.0.13 und entfällt bei uns) +
// num_in_portals(u32) + in_portals(Ptr-Liste) + num_out_portals(u32) + out_portals
// (Ptr-Liste) + num_fixtures(u32) + fixtures(Ptr-Liste).
NiNodeBlock ParseNiRoom(ByteReader& r) {
    NiNodeBlock node = ParseNiNode(r);
    const std::uint32_t numWalls = r.CountU32(256u);
    r.Skip(static_cast<std::size_t>(numWalls) * 16u); // NiPlane je Wand
    const std::uint32_t numInPortals = r.CountU32(256u);
    for (std::uint32_t i = 0; i < numInPortals; ++i) r.I32();
    const std::uint32_t numOutPortals = r.CountU32(256u);
    for (std::uint32_t i = 0; i < numOutPortals; ++i) r.I32();
    const std::uint32_t numFixtures = r.CountU32(1000u);
    for (std::uint32_t i = 0; i < numFixtures; ++i) r.I32();
    return node;
}

void SkipNiZBufferProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    r.Skip(6);
}

void SkipNiVertexColorProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    r.Skip(10);
}

// NiAlphaProperty: ObjectNetBase (bei allen geprüften Dateien leer -> 8 Byte) + flags(u16) +
// threshold(u8) + 4 weitere Byte = 15 Byte gesamt. Länge empirisch bestimmt durch Subtraktion
// der bekannten Nachbarblock-Längen zwischen zwei String-Landmarken (siehe docs/MAP_FORMAT.md).
// Häufigster Blocker im Massentest (1989 von 3436 echten Dateien enthalten diesen Typ).
void SkipNiAlphaProperty(ByteReader& r) {
    ParseObjectNetBase(r); // 12 Byte bei leerem Namen
    r.Skip(3);             // flags(u16) + threshold(u8) -> 15 Byte gesamt, exakt vermessen
}

// NiStencilProperty: gleiche Grundform wie die anderen Property-Blöcke. Zweithäufigster
// Blocker (1394 Dateien). Länge analog zu NiAlphaProperty bestimmt - falls sich das im
// Massentest als falsch erweist, schlägt der Parser für diese Dateien sauber fehl (Bounds-
// Check im ByteReader), statt falsche Geometrie zu liefern.
// NiStencilProperty: kurze Basis (0,-1, 8 Byte - wie NiTexturingProperty's frühere
// Fehlannahme, hier aber empirisch bestätigt) + 8 unbekannte uint32-Felder (vermutlich
// enable/function/ref/mask/fail/zfail/pass/draw-mode) + ein eingebettetes Namens-/
// Beschreibungsfeld (Sized-String, variable Länge - z.B. "21 - Default" bei der
// Referenzdatei). Länge empirisch bestimmt: Struktur endet exakt dort, wo die nachfolgende
// NiMaterialProperty (eigene kurze Basis + Farbfloats beginnend bei 1.0,1.0,...) plausibel
// weitergeht (siehe docs/MAP_FORMAT.md, Rou_M_Tube.nif).
void SkipNiStencilProperty(ByteReader& r) {
    // KORRIGIERT: bisher wurde hier fest von der "kurzen" Variante (name_len=0 direkt gefolgt
    // von controller, OHNE num_extra_data_refs) ausgegangen - das galt aber nur zufällig für
    // die ursprünglichen Testdateien. An FighterDown.nif hat diese NiStencilProperty-Instanz
    // SEHR WOHL ein explizites num_extra_data_refs=0-Feld (die normale 12-Byte-Basis) - die
    // feste 8-Byte-Annahme verschob alles Nachfolgende um 4 Byte und ließ die anschließende
    // NiMaterialProperty mit Datenmüll als Namen beginnen. ParseObjectNetBase() entscheidet
    // das jetzt korrekt per Peek (siehe dortige Korrektur) - byte-exakt verifiziert: danach
    // landet man exakt auf plausible Materialfarben-Floats.
    ParseObjectNetBase(r);
    for (int i = 0; i < 7; ++i) r.U32(); // KORRIGIERT: 7 Felder, nicht 8 - byte-genau vermessen
    r.U8();  // zusätzliches Einzelbyte vor dem Namensfeld
    r.SizedString(); // Namens-/Beschreibungsfeld, Inhalt nicht weiterverwendet
}

// NiSpecularProperty: laut Referenzimplementierung denkbar einfach - NiObjectNET-Basis +
// EIN einzelnes flags(u16)-Feld. Deutlich kürzer als der zuvor bei NiStencilProperty
// vermutete "8-unbekannte-Felder"-Aufbau.
void SkipNiSpecularProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    r.U16(); // flags
}

// NiDitherProperty: NiObjectNET-Basis + flags(u16). Identisch aufgebaut zu
// NiSpecularProperty.
void SkipNiDitherProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    r.U16(); // flags
}

// NiFogProperty: NiObjectNET-Basis + flags(u16) + fog_depth(float) + fog_color(Color3=12 Byte).
void SkipNiFogProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    r.U16(); // flags
    r.F32(); // fog_depth
    r.Skip(12); // fog_color (Color3)
}

// NiExtraData-Basis: NUR ein Namensfeld (Sized-String) - anders als ObjectNetBase/NiObjectNET
// (kein Extra-Data-Liste-Zähler, kein Controller-Ref). NiExtraData erbt direkt von NiObject,
// nicht von NiObjectNET. Byte-exakt verifiziert an mehreren sauberen Instanzen (z.B.
// olber_sword.nif Block 4: Name="LODDistance = 0.0\r\n", danach direkt ein leerer
// String-Wert - landet exakt auf dem Namen der folgenden NiStencilProperty).
//
// Bei einer kleinen Minderheit (~34 von 3436 Dateien, bisher nur im Kontext eines benannten
// Multi-Textur-Blend-Shaders wie "VCAlphaTextureBlender" beobachtet, siehe
// bossroom_wall.nif) geht dem Namensfeld zusätzlich ein int32-Feld mit Wert -1 voraus
// (vermutlich ein Rest eines alten Ketten-/Controller-Zeigers, der in den meisten Dateien
// fehlt). 0xFFFFFFFF ist als String-Länge ohnehin nie plausibel (SizedString bricht sonst mit
// "Unerwartetes Dateiende" ab), daher hier sicher per Peek erkennbar und übersprungen, statt
// die Datei unnötig scheitern zu lassen.
void SkipNiExtraDataBase(ByteReader& r) {
    if (r.PeekU32(0) == 0xFFFFFFFFu) {
        r.I32(); // seltenes führendes Ketten-/Controller-Feld, siehe oben
    }
    r.SizedString(); // name
}

// NiPalette: KEINE NiObjectNET-Basis (reines NiObject, kein Name/ExtraData/Controller!) -
// has_alpha(u8) + num_entries(u32, meist 256, kann auch 16 sein) + palette(ByteColor4-Liste,
// je 4 Byte, Anzahl = num_entries). Struktur aus der autoritativen offiziellen
// niftools/nifxml-Referenzdatei übernommen.
void SkipNiPalette(ByteReader& r) {
    r.U8(); // has_alpha
    const std::uint32_t numEntries = r.CountU32(256u);
    r.Skip(static_cast<std::size_t>(numEntries) * 4u);
}

// NiStringExtraData: NiExtraData-Basis + ein weiteres Sized-String-Feld (der eigentliche Wert,
// z.B. "Collision" o.ä. - Inhalt nicht weiterverwendet). Verifiziert an mehreren echten Dateien
// (z.B. AdlF_maingate_A.nif, Block 37/50/54/...): die berechnete Länge landet exakt auf dem
// Namensfeld des jeweils folgenden Blocks (siehe docs/MAP_FORMAT.md). Zweithäufigster Blocker
// im Massentest (331 von 3436 Dateien).
void SkipNiStringExtraData(ByteReader& r) {
    SkipNiExtraDataBase(r);
    r.SizedString(); // string_data
}

// NiIntegerExtraData: NiExtraData-Basis + ein uint32-Feld.
void SkipNiIntegerExtraData(ByteReader& r) {
    SkipNiExtraDataBase(r);
    r.U32(); // integer_data
}

// NiIntegersExtraData: NiExtraData-Basis + num_integers(u32) + data(u32-Liste).
void SkipNiIntegersExtraData(ByteReader& r) {
    SkipNiExtraDataBase(r);
    const std::uint32_t numIntegers = r.CountU32(10000u);
    r.Skip(static_cast<std::size_t>(numIntegers) * 4u);
}

// NiTextKeyExtraData: NiExtraData-Basis + num_text_keys(u32) + je Key: time(f32) +
// value(SizedString). Byte-exakt verifiziert an SD_Vale01_machine01.nif: 2 Keys, deren Werte
// eindeutig lesbare Animationskommandos sind ("start -name idle01 ... -loop", "end") - KEIN
// zusätzliches "unknown_int_1"-Feld vor num_text_keys (entgegen einer älteren, unbestätigten
// Referenzangabe "always 0 in official files" - hier schlicht nicht vorhanden).
void SkipNiTextKeyExtraData(ByteReader& r) {
    SkipNiExtraDataBase(r);
    const std::uint32_t numTextKeys = r.CountU32(10000u);
    for (std::uint32_t i = 0; i < numTextKeys; ++i) {
        r.F32(); // time
        r.SizedString(); // value
    }
}

// NiFloatExtraData: NiExtraData-Basis + ein float-Feld. Byte-exakt verifiziert an
// UrgSwa_swamp.nif (Name "ambient", Wert 0.0) - landet exakt auf den Namen der nächsten
// NiExtraData ("baseColor").
void SkipNiFloatExtraData(ByteReader& r) {
    SkipNiExtraDataBase(r);
    r.F32();
}

// NiColorExtraData: NiExtraData-Basis + Color4(4 Floats, 16 Byte). Byte-exakt verifiziert an
// NewDesign.nif (Name "paramedgecolor", Wert (1,1,1,1) - plausibles Weiß) - landet exakt auf
// den Namen der nächsten NiExtraData.
void SkipNiColorExtraData(ByteReader& r) {
    SkipNiExtraDataBase(r);
    r.Skip(16);
}

// NiBooleanExtraData: NiExtraData-Basis + ein Byte (bool_data). Laut Referenz die einfachste
// aller Extra-Data-Varianten - nicht unabhängig byte-exakt verifiziert (die einzige
// verfügbare Testdatei hatte bereits vorgelagerte Fehlausrichtung), aber strukturell
// analog zu den anderen, bereits verifizierten einfachen Extra-Data-Typen.
void SkipNiBooleanExtraData(ByteReader& r) {
    SkipNiExtraDataBase(r);
    r.U8();
}

// NiCollisionData: NiCollisionObject-Basis (nur ein "Target"-Ptr auf das besitzende
// NiAVObject, int32 - KEIN Name/ObjectNetBase) + propagation_mode(u32) + collision_mode(u32)
// + use_abv(u8) + OPTIONAL eine Bounding-Volume-Struktur (nur falls use_abv != 0):
// collision_type(u32: 0=Sphere,1=Box,2=Capsule,3=Union,4=Halfspace) + typspezifische Floats
// (Sphere: Center-Vector3+Radius=16 Byte; Box: Center-Vector3+Rotation-Matrix33+Extent-
// Vector3=60 Byte; Capsule: Center-Vector3+Achsen-Vector3+Extent+Radius=32 Byte). Union/
// Halfspace in den 3436 Testdateien nicht beobachtet - bewusst NICHT geraten, schlägt sauber
// fehl statt mit falscher Byte-Position weiterzulesen (siehe Invalidate()).
// Byte-exakt verifiziert (Box-Variante) an AdlF_maingate_A.nif, Block 3: target=0 (zeigt auf
// den eigenen Scene-Root-NiNode), propagation_mode=3, collision_mode=2 (USE_ABV), use_abv=1,
// collision_type=1 (Box), danach identische Rotationsmatrix (Einheitsmatrix) - die berechnete
// Gesamtlänge (77 Byte) landet EXAKT auf dem Namensfeld des folgenden NiTriStrips
// ("AdlF_maingate01", länge-präfixiert). Häufigster Einzel-Blocker im Massentest (852 von
// 3436 Dateien) - größter Hebel aller offenen .nif-Blocktypen.
void SkipNiCollisionData(ByteReader& r) {
    r.I32(); // target ref (Ptr, nicht refcounted, aber trotzdem im Stream vorhanden)
    r.U32(); // propagation_mode
    r.U32(); // collision_mode
    const std::uint8_t useAbv = r.U8();
    if (!useAbv) return;
    const std::uint32_t collisionType = r.U32();
    switch (collisionType) {
        case 0: r.Skip(16); break; // Sphere: Center(12) + Radius(4)
        case 1: r.Skip(60); break; // Box: Center(12) + Rotation(36) + Extent(12)
        case 2: r.Skip(32); break; // Capsule: Center(12) + Achse(12) + Extent(4) + Radius(4)
        default: r.Invalidate(); break; // Union/Halfspace - nicht unterstützt, siehe oben
    }
}

// NiSkinInstance: data_ref(i32) + skin_partition(i32) + skeleton_root(i32) + num_bones(u32) +
// je Knochen ein Ref(i32). Referenziert eine NiSkinData (Bindungspose je Knochen) und optional
// eine NiSkinPartition (GPU-Partitionierung für Hardware-Skinning).
void SkipNiSkinInstance(ByteReader& r) {
    r.I32(); // data_ref
    r.I32(); // skin_partition
    r.I32(); // skeleton_root
    const std::uint32_t numBones = r.CountU32(256u);
    r.Skip(static_cast<std::size_t>(numBones) * 4u);
}

// NiSkinData: skin_transform(NiTransform = Matrix33(36) + Vector3(12) + f32 Scale = 52 Byte) +
// num_bones(u32) + has_vertex_weights(u8) + je Knochen ein BoneData-Eintrag: eigene
// skin_transform(52 Byte) + bounding_sphere(NiBound = Vector3+f32 = 16 Byte) + num_vertices
// (u16) + FALLS has_vertex_weights: je Vertex ein Gewicht (index(u16)+weight(f32) = 6 Byte).
void SkipNiSkinData(ByteReader& r) {
    r.Skip(52); // skin_transform
    const std::uint32_t numBones = r.CountU32(256u);
    const std::uint8_t hasVertexWeights = r.U8();
    for (std::uint32_t b = 0; b < numBones; ++b) {
        r.Skip(52); // Knochen-eigene skin_transform
        r.Skip(16); // bounding_sphere
        const std::uint32_t numVerts = r.CountU16(20000u);
        if (hasVertexWeights) {
            r.Skip(static_cast<std::size_t>(numVerts) * 6u);
        }
    }
}

// NiSkinPartition: num_partitions(u32) + je Partition eine (relativ komplexe) Struktur für
// GPU-Hardware-Skinning - siehe Referenzimplementierung. Nur übersprungen, nicht für
// Rendering verwendet (der Editor stellt Meshes aktuell nur in Bindungspose dar, keine
// Animation/Skinning-Deformation nötig).
void SkipNiSkinPartition(ByteReader& r) {
    const std::uint32_t numPartitions = r.CountU32(256u);
    for (std::uint32_t p = 0; p < numPartitions; ++p) {
        const std::uint32_t numVerts = r.CountU16(20000u);
        const std::uint32_t numTriangles = r.CountU16(60000u);
        const std::uint32_t numBones = r.CountU16(2000u);
        const std::uint32_t numStrips = r.CountU16(20000u);
        const std::uint32_t numWeightsPerVertex = r.CountU16(16u);
        r.Skip(static_cast<std::size_t>(numBones) * 2u); // bones[u16]
        const std::uint8_t hasVertexMap = r.U8();
        if (hasVertexMap) {
            r.Skip(static_cast<std::size_t>(numVerts) * 2u); // vertex_map[u16]
        }
        const std::uint8_t hasVertexWeights = r.U8();
        if (hasVertexWeights) {
            r.Skip(static_cast<std::size_t>(numVerts) * numWeightsPerVertex * 4u); // Floats
        }
        std::uint32_t stripLengthSum = 0;
        for (std::uint32_t s = 0; s < numStrips; ++s) {
            stripLengthSum += r.CountU16(60000u);
        }
        const std::uint8_t hasFaces = r.U8();
        if (hasFaces && numStrips != 0) {
            r.Skip(static_cast<std::size_t>(stripLengthSum) * 2u); // strips[u16]
        } else if (hasFaces && numStrips == 0) {
            r.Skip(static_cast<std::size_t>(numTriangles) * 6u); // Triangles(3xu16) je 6 Byte
        }
        const std::uint8_t hasBoneIndices = r.U8();
        if (hasBoneIndices) {
            r.Skip(static_cast<std::size_t>(numVerts) * numWeightsPerVertex); // u8 je Eintrag
        }
    }
}

// KeyGroup<T>: num_keys(u32) + [falls != 0] key_type(u32: 1=LINEAR, 2=QUADRATIC, 3=TBC) +
// num_keys Einträge, deren Größe vom Key-Typ abhängt (LINEAR: Zeit+Wert; QUADRATIC: Zeit+Wert+
// Vorwärts-/Rückwärts-Tangente je Wertgröße; TBC: Zeit+Wert+3 Floats Tension/Bias/Continuity).
// valueFloats = Anzahl Floats pro Wert (1 für Skalar-Keygroups wie Scale/XYZ-Rotation, 3 für
// Vector3-Keygroups wie Translation). Byte-exakt verifiziert (siehe SkipNiTransformData).
void SkipKeyGroup(ByteReader& r, int valueFloats) {
    const std::uint32_t numKeys = r.CountU32(200000u);
    if (numKeys == 0) return;
    const std::uint32_t keyType = r.U32();
    std::size_t perKeyFloats = 0;
    switch (keyType) {
        case 1: perKeyFloats = 1 + valueFloats; break;                  // LINEAR: Zeit+Wert
        case 2: perKeyFloats = 1 + 3 * valueFloats; break;               // QUADRATIC: +2 Tangenten
        case 3: perKeyFloats = 1 + valueFloats + 3; break;               // TBC: +3 (Tension/Bias/Cont.)
        case 5: perKeyFloats = 1 + valueFloats; break;                   // CONST: gleiche Größe wie LINEAR
        default: r.Invalidate(); return;
    }
    r.Skip(static_cast<std::size_t>(numKeys) * perKeyFloats * 4);
}

// Wie SkipKeyGroup, aber für KeyGroup<u8> (z.B. NiBoolData) - der Wert selbst ist 1 Byte statt
// eines Floats, Zeit und Tangenten bleiben aber Floats (4 Byte). Referenz: NiBoolData = eine
// einzelne KeyGroup<u8> (docs.rs "nif"-Crate, Ziel-Version 20.0.0.4).
void SkipKeyGroupBytes(ByteReader& r) {
    const std::uint32_t numKeys = r.CountU32(200000u);
    if (numKeys == 0) return;
    const std::uint32_t keyType = r.U32();
    std::size_t perKeyBytes = 0;
    switch (keyType) {
        case 1: perKeyBytes = 4 + 1; break;                  // LINEAR: Zeit(f32)+Wert(u8)
        case 2: perKeyBytes = 4 + 3 * 1; break;               // QUADRATIC: +2 Tangenten(u8)
        case 3: perKeyBytes = 4 + 1 + 3 * 4; break;           // TBC: +3 Floats Tension/Bias/Cont.
        case 5: perKeyBytes = 4 + 1; break;                   // CONST: gleiche Größe wie LINEAR
        default: r.Invalidate(); return;
    }
    r.Skip(static_cast<std::size_t>(numKeys) * perKeyBytes);
}

// Gemeinsamer Vertex-/Normalen-/Farben-/UV-Kopf von NiGeometryData, wiederverwendet für
// Partikel-Daten (NiParticlesData/NiPSysData erben ebenfalls von NiGeometryData). Liefert die
// Vertex-Anzahl zurück, die für die anschließenden partikel-spezifischen Arrays (Radii,
// Sizes, Rotationen, ...) gebraucht wird. Bewusst separat von ParseNiTriStripsData/
// ParseNiTriShapeData gehalten (keine gemeinsame Funktion), um deren bereits ausführlich
// verifizierten Code nicht anzufassen - inhaltlich identisch (gleiche Feldreihenfolge, gleicher
// UV-Fix aus v0.20.0), da für Partikel-Meshes keine Rendering-Daten extrahiert werden (nur
// Länge muss stimmen).
std::uint32_t SkipNiGeometryDataHeader(ByteReader& r, bool isOlderVersion) {
    const std::uint32_t numVerts = r.CountU16(20000u);
    r.U8(); r.U8(); // keep_flags, compress_flags
    const std::uint8_t hasVerts = r.U8();
    if (hasVerts) r.Skip(static_cast<std::size_t>(numVerts) * 12u);
    const std::uint16_t dataFlags = r.CountU16(0xFFFFu);
    const std::uint32_t numUvSets = dataFlags & 0x3Fu;
    const bool hasTangentSpace = (dataFlags & 0x1000u) != 0;
    const std::uint8_t hasNormals = r.U8();
    if (hasNormals) {
        r.Skip(static_cast<std::size_t>(numVerts) * 12u);
        if (hasTangentSpace) r.Skip(static_cast<std::size_t>(numVerts) * 12u * 2u);
    }
    r.Skip(16); // Bounding-Sphere
    const std::uint8_t hasColors = r.U8();
    if (hasColors) r.Skip(static_cast<std::size_t>(numVerts) * 16u);
    for (std::uint32_t set = 0; set < numUvSets; ++set) {
        r.Skip(static_cast<std::size_t>(numVerts) * 8u);
    }
    r.U16(); // mysteriöses Feld NACH den UV-Daten, siehe ParseNiTriStripsData (v0.20.0-Fix)
    // KORRIGIERT (siehe docs/MAP_FORMAT.md Abschnitt 36): "Additional Data" ist laut
    // autoritativer Referenz erst SEIT Version 20.0.0.4 vorhanden - bei älteren Versionen
    // (10.1.0.0/10.2.0.0) entfällt dieses Feld komplett.
    if (!isOlderVersion) {
        r.I32(); // consistency_flags/additional_data ref
    } else {
        r.U16(); // nur consistency_flags, kein additional_data ref
    }
    return numVerts;
}

// NiParticlesData : NiGeometryData + has_radii+Radii + num_active(u16) + has_sizes+Sizes +
// has_rotations+Rotations(Quaternion je 16 Byte) + has_rotation_angles+Angles +
// has_rotation_axes+Achsen(Vector3 je 12 Byte).
std::uint32_t SkipNiParticlesData(ByteReader& r, std::uint32_t version) {
    // NiGeometryData starts with an unknown uint32 since 10.2.0.0. The old skip path
    // started directly at num_vertices, shifting every 10.2/20.0 particle block by 4 bytes.
    if (version >= 0x0A020000u) {
        r.U32(); // NiGeometryData::unknownInt
    }

    // Additional Data was introduced with 20.0.0.4; 10.1/10.2 only carry the older
    // consistency-flags tail.
    const bool isOlderVersion = version != 0x14000004u;
    const std::uint32_t numVerts = SkipNiGeometryDataHeader(r, isOlderVersion);

    if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 4u);  // has_radii
    r.U16();                                                       // num_active
    if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 4u);  // has_sizes
    if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 16u); // has_rotations
    if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 4u);  // has_rotation_angles
    if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 12u); // has_rotation_axes
    return numVerts;
}

// NiPSysData : NiParticlesData + je Vertex ein NiParticleInfo (Velocity-Vector3(12) +
// age/life_span/last_update(je f32=4) + spawn_generation/code(je u16=2) = 28 Byte) +
// has_unknown_floats+Floats + 2 abschließende u16-Felder.
// NiMeshPSysData (isMeshVariant=true) hat laut Referenz zusätzliche, NUR grob dokumentierte
// Felder ("Unknown"-benannt) - byte-exakt an stone03.nif verifiziert: exakt 17 zusätzliche
// Byte (u32+u8+u32+u32+i32) VOR dem nächsten Block. Der letzte i32 ist ein plausibler,
// gültiger Ref (zeigt in der Testdatei sogar exakt auf den nachfolgenden NiNode-Block selbst -
// wird nirgends weiterverwendet, da keine Partikel gerendert werden, daher genügt die
// GESAMTLÄNGE ohne Feld-Interpretation).
void SkipNiPSysData(ByteReader& r, std::uint32_t version, bool isMeshVariant = false) {
    const std::uint32_t numVerts = SkipNiParticlesData(r, version);
    r.Skip(static_cast<std::size_t>(numVerts) * 28u); // ParticleDesc per vertex

    // hasUnknownFloats3 exists from 20.0.0.4 onward. Reading this byte in 10.1/10.2
    // shifts all following fields and was a second independent old-version alignment bug.
    if (version >= 0x14000004u) {
        if (r.U8()) {
            r.Skip(static_cast<std::size_t>(numVerts) * 4u);
        }
    }

    r.U16(); // unknown_short_1
    r.U16(); // unknown_short_2

    if (isMeshVariant) {
        // NiMeshPSysData has a variable-length trailer. The former fixed 17-byte skip
        // only happened to match instances where numUnknownInts1 == 1.
        if (version >= 0x0A020000u) {
            r.U32(); // unknownInt2
            r.U8();  // unknownByte3
            const std::uint32_t numUnknownInts1 = r.CountU32(4096u);
            for (std::uint32_t i = 0; i < numUnknownInts1; ++i) {
                r.U32();
            }
        }
        r.I32(); // unknownNode ref (present for all supported versions)
    }
}

// NiPSysModifier-Basis (gemeinsam für alle Partikel-Modifier/Emitter): name(String) +
// order(u32) + target_ref(i32) + active(u8).
void SkipNiPSysModifierBase(ByteReader& r) {
    r.SizedString(); // name
    r.U32();         // order
    r.I32();         // target_ref
    r.U8();          // active
}

// NiPSysColliderManager: NiPSysModifier-Basis + collider_ref(i32).
void SkipNiPSysColliderManager(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.I32(); // collider_ref
}

// NiPSysCollider-Basis (gemeinsam für alle Kollider-Typen): KEINE NiObjectNET-Basis (reines
// NiObject!) - bounce(f32) + spawn_on_collide(u8) + die_on_collide(u8) +
// spawn_modifier_ref(i32) + parent_ptr(i32) + next_collider_ref(i32) + collider_object_ptr
// (i32) = 22 Byte.
void SkipNiPSysColliderBase(ByteReader& r) {
    r.F32(); // bounce
    r.U8();  // spawn_on_collide
    r.U8();  // die_on_collide
    r.I32(); // spawn_modifier_ref
    r.I32(); // parent
    r.I32(); // next_collider_ref
    r.I32(); // collider_object
}

// NiPSysPlanarCollider: NiPSysCollider-Basis + width(f32) + height(f32) + x_axis(Vector3) +
// y_axis(Vector3).
void SkipNiPSysPlanarCollider(ByteReader& r) {
    SkipNiPSysColliderBase(r);
    r.Skip(4 + 4 + 12 + 12);
}

// NiPSysEmitter-Basis (gemeinsam für alle Emitter-Typen): NiPSysModifier + 6 Floats
// (speed/speed_variation/declination/declination_variation/planar_angle/
// planar_angle_variation) + initial_color(Color4=16 Byte) + 4 Floats (initial_radius/
// radius_variation/life_span/life_span_variation).
void SkipNiPSysEmitterBase(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.Skip(6 * 4);  // 6 Floats
    r.Skip(16);     // initial_color (Color4)
    r.Skip(4 * 4);  // 4 weitere Floats
}

// NiPSysVolumeEmitter-Basis: NiPSysEmitter + emitter_object_ref(i32).
void SkipNiPSysVolumeEmitterBase(ByteReader& r) {
    SkipNiPSysEmitterBase(r);
    r.I32(); // emitter_object_ref
}

// NiPSysBoxEmitter: NiPSysVolumeEmitter + width/height/depth (3 Floats).
void SkipNiPSysBoxEmitter(ByteReader& r) {
    SkipNiPSysVolumeEmitterBase(r);
    r.Skip(3 * 4);
}

// NiPSysMeshEmitter: NiPSysEmitter + num_emitter_meshes(u32)+Refs + initial_velocity_type(u32)
// + emission_axis(Vector3) + emission_type(u32) laut Referenz (PyFFI) - deren genaue
// Reihenfolge/Typgrößen aber NICHT eindeutig dokumentiert sind (siehe docs/MAP_FORMAT.md
// Abschnitt 19). STATT die einzelnen Felder zu raten, wird hier nur die GESAMTLÄNGE
// empirisch verwendet: über 70 reale Dateien hinweg beginnt der jeweils nächste Block
// (erkennbar an seiner NiPSysModifierBase) mit überwältigender Mehrheit (41 von 70, weitere
// 4 nur 2 Byte versetzt) rund 315 Byte nach Blockbeginn - unabhängig vom genauen Feld-
// Layout innerhalb dieser Byte (die Werte selbst werden ohnehin nirgends weiterverwendet, da
// keine Partikel gerendert werden). Ein systematischer Massentest-Scan über viele
// Kandidatenwerte (236-260) zeigte KEIN einzelnes eindeutiges Optimum, sondern ein Plateau
// mehrerer Werte (236/243/244/247/258/260) bei gleichauf bestem Ergebnis (1818/3436) - ein
// klares Indiz, dass `num_emitter_meshes` in der Praxis PRO INSTANZ variiert (eine fest
// codierte Länge kann also grundsätzlich nie für alle Dateien exakt stimmen). 244 gewählt,
// da es der ursprünglichen 246-Byte-Schätzung am nächsten liegt.
// NiPSysMeshEmitter: NiPSysEmitter + num_emitter_meshes(u32) + emitter_meshes(Ptr, je i32) +
// initial_velocity_type(u32-Enum VelocityType) + emission_type(u32-Enum EmitFrom) +
// emission_axis(Vector3). Struktur aus der autoritativen offiziellen niftools/nifxml-
// Referenzdatei (nif.xml, direkt von GitHub geladen) übernommen - ersetzt den früheren
// empirischen Kompromiss (fester Skip von 244 Byte, siehe docs/MAP_FORMAT.md Abschnitt 19),
// der auf zwischenzeitlich durch andere Fixes veränderte (jetzt falsche) Blockpositionen
// zurückging.
void SkipNiPSysMeshEmitter(ByteReader& r) {
    SkipNiPSysEmitterBase(r);
    const std::uint32_t numEmitterMeshes = r.CountU32(256u);
    for (std::uint32_t i = 0; i < numEmitterMeshes; ++i) {
        r.I32();
    }
    r.U32(); // initial_velocity_type
    r.U32(); // emission_type
    r.Skip(12); // emission_axis (Vector3)
}

// NiPSysAgeDeathModifier: NiPSysModifier + spawn_on_death(u8) + spawn_modifier_ref(i32).
void SkipNiPSysAgeDeathModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.U8();
    r.I32();
}

// NiPSysSpawnModifier: NiPSysModifier + num_spawn_generations(u16) + percentage_spawned(f32) +
// min/max_num_to_spawn(je u16) + spawn_speed_variation/spawn_dir_variation/life_span/
// life_span_variation (je f32).
void SkipNiPSysSpawnModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.U16(); r.F32(); r.U16(); r.U16(); r.F32(); r.F32(); r.F32(); r.F32();
}

// NiPSysGrowFadeModifier: NiPSysModifier + grow_time(f32) + grow_generation(u16) +
// fade_time(f32) + fade_generation(u16).
void SkipNiPSysGrowFadeModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.F32(); r.U16(); r.F32(); r.U16();
}

// NiPSysColorModifier: NiPSysModifier + data_ref(i32, zeigt auf NiColorData).
void SkipNiPSysColorModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.I32();
}

// NiPSysRotationModifier: NiPSysModifier + 4 Floats (initial_rotation_speed/_variation,
// initial_rotation_angle/_variation) + 2 Bytes (random_rot_speed_sign, random_initial_axis) +
// initial_axis (Vector3).
void SkipNiPSysRotationModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.Skip(4 * 4);
    r.U8(); r.U8();
    r.Skip(12);
}

// NiPSysGravityModifier: NiPSysModifier + gravity_object_ref(i32) + gravity_axis(Vector3) +
// decay/strength(je f32) + force_type(u32) + turbulence/turbulence_scale(je f32).
void SkipNiPSysGravityModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.I32();
    r.Skip(12);
    r.F32(); r.F32();
    r.U32();
    r.F32(); r.F32();
}

// NiPSysDragModifier: NiPSysModifier + drag_object_ptr(i32) + drag_axis(Vector3) +
// percentage/range/range_falloff (je f32). Struktur aus der autoritativen nif.xml-Referenz.
void SkipNiPSysDragModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.I32();    // drag_object
    r.Skip(12); // drag_axis (Vector3)
    r.F32();    // percentage
    r.F32();    // range
    r.F32();    // range_falloff
}

// NiPSysPositionModifier: NiPSysModifier, keine eigenen Felder.
void SkipNiPSysPositionModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
}

// NiPSysBoundUpdateModifier: NiPSysModifier + update_skip(u16).
void SkipNiPSysBoundUpdateModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.U16();
}

// NiPSysMeshUpdateModifier: NiPSysModifier + num_meshes(u32) + meshes(Refs, je i32) - laut
// Referenz (PyFFI) eine einfache, unzweideutige Ref-Liste (kein Sonderfall wie bei
// NiPSysMeshEmitter, siehe Abschnitt 19).
void SkipNiPSysMeshUpdateModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    const std::uint32_t numMeshes = r.CountU32(256u);
    for (std::uint32_t i = 0; i < numMeshes; ++i) {
        r.I32();
    }
}

// NiDynamicEffect-Basis (gemeinsam für NiTextureEffect und NiLight/NiDirectionalLight):
// AVObjectBase + switch_state(u8) + num_affected_nodes(u32) + je Knoten ein Ref(i32).
void SkipNiDynamicEffectBase(ByteReader& r) {
    ParseAVObjectBase(r);
    r.U8(); // switch_state
    const std::uint32_t numAffected = r.CountU32(256u);
    r.Skip(static_cast<std::size_t>(numAffected) * 4u);
}

// NiTextureEffect: NiDynamicEffect + model_projection_matrix(Matrix33=9 Floats) +
// model_projection_translation(Vector3) + texture_filtering(u32) + texture_clamping(u32) +
// texture_type(u32) + coordinate_generation_type(u32) + source_texture_ref(i32) +
// enable_plane(u8) + plane(NiPlane: normal(Vector3)+constant(f32) = 16 Byte).
void SkipNiTextureEffect(ByteReader& r) {
    SkipNiDynamicEffectBase(r);
    r.Skip(36); // model_projection_matrix (Matrix33)
    r.Skip(12); // model_projection_translation (Vector3)
    r.U32(); r.U32(); r.U32(); r.U32(); // filtering, clamping, texture_type, coord_gen_type
    r.I32(); // source_texture_ref
    r.U8();  // enable_plane
    r.Skip(16); // plane (Vector3 + f32)
}

// NiLight-Basis (gemeinsam für alle Licht-Typen): NiDynamicEffect + dimmer(f32) +
// ambient/diffuse/specular_color (je Color3 = 12 Byte).
void SkipNiLightBase(ByteReader& r) {
    SkipNiDynamicEffectBase(r);
    r.F32(); // dimmer
    r.Skip(36); // 3x Color3
}

// NiDirectionalLight: reine NiLight-Basis, keine eigenen Felder (Richtung kommt aus der
// AVObject-eigenen Transform/Rotation).
void SkipNiDirectionalLight(ByteReader& r) {
    SkipNiLightBase(r);
}

// NiPointLight: NiLight-Basis + constant/linear/quadratic_attenuation (je float). Struktur
// aus der autoritativen offiziellen niftools/nifxml-Referenzdatei übernommen.
void SkipNiPointLight(ByteReader& r) {
    SkipNiLightBase(r);
    r.Skip(12); // 3 Floats
}

// NiTransformData (referenziert von NiTransformInterpolator): num_rotation_keys(u32) + FALLS
// >0 rotation_type(u32) + entweder (rotation_type==4, XYZ_ROTATION_KEY) drei skalare
// KeyGroups für X/Y/Z-Rotation, ODER (sonst) num_rotation_keys Quaternion-Keys (Zeit+4 Floats,
// bei TBC-Typ +3 weitere Floats - Quaternion-Keys haben laut Format nie eigene Tangenten, d.h.
// LINEAR und QUADRATIC sind hier gleich groß) - danach Translation-KeyGroup (Vector3) und
// Scale-KeyGroup (Skalar). Byte-exakt verifiziert an AdlF_Flower.nif (XYZ_ROTATION_KEY-Zweig,
// gemischt LINEAR/QUADRATIC je Achse, plausible Y-Rotation von 0 auf -0.042 rad über 6.667s -
// eine schaukelnde Blume): die berechnete Gesamtlänge landet exakt auf dem Namensfeld
// ("Object12") des folgenden NiTriStrips.
void SkipNiTransformData(ByteReader& r) {
    const std::uint32_t numRotationKeys = r.CountU32(200000u);
    if (numRotationKeys > 0) {
        const std::uint32_t rotationType = r.U32();
        if (rotationType == 4) {
            // XYZ_ROTATION_KEY: num_rotation_keys ist hier laut Format immer 1 (Platzhalter),
            // die eigentlichen Keys stecken in den drei folgenden KeyGroups.
            SkipKeyGroup(r, 1); // X
            SkipKeyGroup(r, 1); // Y
            SkipKeyGroup(r, 1); // Z
        } else {
            std::size_t perKeyFloats = 0;
            switch (rotationType) {
                case 1: case 2: perKeyFloats = 1 + 4; break; // Zeit + Quaternion(4), keine Tangenten
                case 3: perKeyFloats = 1 + 4 + 3; break;     // + TBC
                default: r.Invalidate(); return;
            }
            r.Skip(static_cast<std::size_t>(numRotationKeys) * perKeyFloats * 4);
        }
    }
    SkipKeyGroup(r, 3); // Translation (Vector3)
    SkipKeyGroup(r, 1); // Scale (Skalar)
}

// NiTimeController-Basis (gemeinsam für alle Controller-Typen): next_controller(Ref,i32) +
// flags(u16) + frequency(f32) + phase(f32) + start_time(f32) + stop_time(f32) +
// target(Ptr<NiObjectNET>, i32) = 26 Byte. NiSingleInterpController (u.a. Basis von
// NiTransformController) ergänzt einen interpolator_ref(i32) = 30 Byte gesamt.
// Byte-exakt verifiziert an AdlF_Flower.nif, Block 4: next_controller=-1, frequency=1.0,
// phase=0.0, start_time=0.0, stop_time=6.667 (Sekunden), target=3 (zeigt exakt auf den
// animierten NiNode), interpolator_ref=5 (zeigt exakt auf die folgende
// NiTransformInterpolator) - zweithäufigster Blocker im Massentest (247 von 3436 Dateien).
void SkipNiTransformController(ByteReader& r) {
    r.I32();   // next_controller ref
    r.U16();   // flags
    r.F32(); r.F32(); r.F32(); r.F32(); // frequency, phase, start_time, stop_time
    r.I32();   // target ptr
    r.I32();   // interpolator ref
}

// NiVisController: NiBoolInterpController = identische 30-Byte-NiSingleInterpController-
// Basis, KEINE eigenen Felder (das frühere "Data"-Ref-Feld gilt laut autoritativer Referenz
// nur bis Version 10.1.0.103 - unsere Version liegt danach).
void SkipNiVisController(ByteReader& r) {
    SkipNiTransformController(r);
}

// NiMultiTargetTransformController: NiInterpController = NiTimeController OHNE
// interpolator_ref (26 Byte, KEIN 30-Byte-Single-Interp) + num_extra_targets(u16) +
// extra_targets(Ptr-Liste, je i32).
void SkipNiMultiTargetTransformController(ByteReader& r) {
    r.I32();   // next_controller ref
    r.U16();   // flags
    r.F32(); r.F32(); r.F32(); r.F32(); // frequency, phase, start_time, stop_time
    r.I32();   // target ptr
    const std::uint32_t numExtraTargets = r.CountU16(1000u);
    for (std::uint32_t i = 0; i < numExtraTargets; ++i) {
        r.I32();
    }
}

// NiGeomMorpherController: NiInterpController = 26-Byte-NiTimeController-Basis (OHNE
// interpolator_ref) + morpher_flags(u16) + data_ref(i32) + always_update(u8) +
// num_interpolators(u32) + interpolators(Ref-Liste, je i32 - für unsere Version 20.0.0.4
// noch eine einfache Ref-Liste, das "Interpolator Weights"-Feld gilt erst ab 20.1.0.3).
void SkipNiGeomMorpherController(ByteReader& r) {
    r.I32();   // next_controller ref
    r.U16();   // flags
    r.F32(); r.F32(); r.F32(); r.F32(); // frequency, phase, start_time, stop_time
    r.I32();   // target ptr
    r.U16();   // morpher_flags
    r.I32();   // data_ref
    r.U8();    // always_update
    const std::uint32_t numInterpolators = r.CountU32(256u);
    for (std::uint32_t i = 0; i < numInterpolators; ++i) {
        r.I32();
    }
}

// NiMorphData: num_morphs(u32) + num_vertices(u32) + relative_targets(u8) + je Morph:
// frame_name(SizedString) + legacy_weight(f32) + vectors(Vector3 * num_vertices). Struktur
// aus der autoritativen nif.xml-Referenz - für unsere Version (20.0.0.4) gelten
// "Frame Name" und "Legacy Weight", NICHT die älteren "Num Keys/Interpolation/Keys"-Felder.
// Byte-exakt an zzz_kong.nif verifiziert: num_morphs=0, landet exakt auf einem gültigen,
// leeren Namensfeld des nächsten Blocks.
void SkipNiMorphData(ByteReader& r) {
    const std::uint32_t numMorphs = r.CountU32(256u);
    const std::uint32_t numVertices = r.CountU32(20000u);
    r.U8(); // relative_targets
    for (std::uint32_t i = 0; i < numMorphs; ++i) {
        r.SizedString(); // frame_name
        r.F32();          // legacy_weight
        r.Skip(static_cast<std::size_t>(numVertices) * 12u); // vectors
    }
}

// NiPSysUpdateCtlr: NUR die reine NiTimeController-Basis (26 Byte, OHNE interpolator_ref) -
// anders als die meisten anderen Controller in dieser Datei, die über
// NiSingleInterpController laufen. Treibt pro Frame die Partikelsimulation an.
void SkipNiPSysUpdateCtlr(ByteReader& r) {
    r.I32();   // next_controller
    r.U16();   // flags
    r.F32(); r.F32(); r.F32(); r.F32(); // frequency, phase, start_time, stop_time
    r.I32();   // target
}

// NiPSysEmitterCtlr: NiPSysModifierCtlr (= NiSingleInterpController(30 Byte) +
// modifier_name(String)) + visibility_interpolator_ref(i32).
void SkipNiPSysEmitterCtlr(ByteReader& r) {
    SkipNiTransformController(r); // identische 30-Byte-NiSingleInterpController-Basis
    r.SizedString(); // modifier_name
    r.I32();          // visibility_interpolator_ref
}

// NiPSysModifierActiveCtlr: NiPSysModifierCtlr = NiSingleInterpController(30 Byte) +
// modifier_name(String) - KEIN zusätzliches Feld danach (anders als NiPSysEmitterCtlr, das
// noch visibility_interpolator_ref ergänzt). Byte-exakt verifiziert an
// Yak_VaporGenerater.nif: target=116 zeigt exakt auf die zugehörige NiParticleSystem,
// modifier_name="NiPSysDragModifier(Z-Axis):10" ist ein eindeutig lesbarer, gültiger Name.
void SkipNiPSysModifierActiveCtlr(ByteReader& r) {
    SkipNiTransformController(r);
    r.SizedString(); // modifier_name
}

// NiFlipController: NiFloatInterpController (= identische 30-Byte-NiSingleInterpController-
// Basis) + texture_slot(u32) + num_sources(u32) + source_refs (je i32). Byte-exakt
// verifiziert an Water.nif: target=8 zeigt exakt auf die zugehörige NiTexturingProperty,
// texture_slot=0 (Basistextur), 30 source_refs bilden eine regelmäßige, aufsteigende Folge
// (13,15,17,...,71) - eindeutig gültige Block-Referenzen für einen Textur-Flipbook-Effekt
// (z.B. animiertes Wasser).
void SkipNiFlipController(ByteReader& r) {
    SkipNiTransformController(r);
    r.U32(); // texture_slot
    const std::uint32_t numSources = r.CountU32(256u);
    for (std::uint32_t i = 0; i < numSources; ++i) {
        r.I32();
    }
}

// NiTransformInterpolator: Translation(Vector3) + Rotation(Quaternion, 4 Floats) +
// Scale(float) + data_ref(i32, zeigt auf NiTransformData) = 36 Byte. Nicht angewandte
// Komponenten sind mit -FLT_MAX (0xFF7FFFFF) belegt, nicht 0 - als Sentinel für "keine
// Override-Bewegung", empirisch an AdlF_Flower.nif bestätigt (Rotation blieb dort Identität
// 1,0,0,0, Translation/Scale beide -FLT_MAX). data_ref zeigt exakt auf die folgende
// NiTransformData.
void SkipNiTransformInterpolator(ByteReader& r) {
    r.Skip(12); // translation
    r.Skip(16); // rotation (quaternion)
    r.Skip(4);  // scale
    r.I32();    // data ref
}

// NiFloatInterpolator: aktueller Wert(float) + data_ref(i32, zeigt auf NiFloatData) = 8 Byte.
// Byte-exakt verifiziert an AdlFH_field_burn_ground.nif: value=0.5, data_ref zeigt exakt auf
// die folgende NiFloatData.
//
// OFFENES PROBLEM (nicht gelöst, siehe docs/MAP_FORMAT.md): Wenn eine NiFloatInterpolator von
// einer NiTextureTransformController referenziert wird, scheint die Struktur GRÖSSER zu sein
// (12 statt 8 Byte - ein zusätzliches Float zwischen Wert und data_ref), UND
// NiTextureTransformController selbst hat in echten Dateien UNTERSCHIEDLICHE Gesamtlängen
// (39 vs. 43 Byte) für augenscheinlich identisch aufgebaute, aufeinanderfolgende Instanzen
// im selben Objekt (SD_Vale01_machine02.nif, AdlFH_field_burn_ground.nif) - beide Varianten
// je für sich genommen byte-exakt plausibel (next_controller/target/interpolator_ref/
// data_ref landen alle exakt auf die erwarteten Nachbarblöcke), aber ohne erkennbares
// unterscheidendes Merkmal, das VOR dem Lesen verrät, welche Variante vorliegt. Deshalb bleibt
// NiTextureTransformController bewusst NICHT unterstützt (110 von 3436 Dateien blockiert),
// statt zu raten und das Risiko einzugehen, in einem unglücklichen Fall still falsche Byte-
// Positionen als Erfolg durchzureichen. Nächster Schritt wäre, mehr Instanzen (idealerweise
// mit strukturell unterschiedlichem "Operation"-Wert) zu vergleichen, um das
// unterscheidende Feld zu finden.
void SkipNiFloatInterpolator(ByteReader& r) {
    r.F32(); // aktueller Wert
    r.I32(); // data ref
}

// NiFloatData: eine einzelne KeyGroup<float> (Skalar-Keyframes, z.B. Alpha- oder
// Textur-Transform-Wert über die Zeit). Byte-exakt verifiziert an
// AdlFH_field_burn_ground.nif: 3 QUADRATIC-Keys (Alpha oszilliert 0.5→1.0→0.5 über 3.33s,
// ein flackernder Brand-Boden-Effekt) - die berechnete Länge landet exakt auf dem
// Namensfeld-Beginn der folgenden NiVertexColorProperty (volle ObjectNetBase, leerer Name).
void SkipNiFloatData(ByteReader& r) {
    SkipKeyGroup(r, 1);
}

// NiMaterialColorController: wie NiAlphaController/NiTransformController dieselbe 30-Byte-
// NiSingleInterpController-Basis, plus ein zusätzliches target_color(u16)-Feld (welcher
// Materialfarbkanal animiert wird - Ambient/Diffuse/Specular/Emissive). Byte-exakt verifiziert
// an Eff_2.nif: target=15 zeigt exakt auf die zugehörige NiMaterialProperty, interpolator_ref=18
// zeigt exakt auf die folgende NiPoint3Interpolator, target_color=3 (vermutlich Emissive) -
// inhaltlich bestätigt durch eine Farbanimation (0,0.45,1)→(0.52,1,0)→(1,0.87,0) über 5
// Sekunden, deren erster Keyframe-Wert exakt dem "aktuellen Wert" der Interpolator übereinstimmt.
void SkipNiMaterialColorController(ByteReader& r) {
    SkipNiTransformController(r); // 30-Byte-Basis, identisch zu Alpha-/Transform-Controller
    r.U16(); // target_color
}

// NiPoint3Interpolator: aktueller Wert(Vector3, 12 Byte) + data_ref(i32, zeigt auf NiPosData).
// Byte-exakt verifiziert an Eff_2.nif: Wert (0,0.447,1) stimmt exakt mit dem ersten Keyframe
// der referenzierten NiPosData überein, data_ref=19 trifft exakt.
void SkipNiPoint3Interpolator(ByteReader& r) {
    r.Skip(12); // aktueller Wert (Vector3)
    r.I32();    // data ref
}

// NiPosData: eine einzelne KeyGroup<Vector3> (Vektor-Keyframes, z.B. Farbe oder Position über
// die Zeit - hier für Materialfarben zweckentfremdet, siehe NiMaterialColorController). Byte-
// exakt verifiziert an Eff_2.nif: 3 QUADRATIC-Keys, die berechnete Länge landet exakt auf die
// folgende NiFloatInterpolator, deren eigener data_ref wiederum exakt auf die erwartete
// NiFloatData trifft.
void SkipNiPosData(ByteReader& r) {
    SkipKeyGroup(r, 3);
}

// NiBoolInterpolator: aktueller Wert(u8, als bool) + data_ref(i32, zeigt auf NiBoolData) =
// 5 Byte. Gleiches Muster wie NiFloatInterpolator/NiPoint3Interpolator, nur mit einem
// Byte statt eines Floats als aktuellem Wert.
void SkipNiBoolInterpolator(ByteReader& r) {
    r.U8();  // aktueller Wert
    r.I32(); // data ref
}

// NiLookAtInterpolator: flags(u16) + look_at_ref(i32, Ptr auf NiNode) + look_at_name(String) +
// NiQuatTransform (translation(Vector3=12) + rotation(Quaternion=16) + scale(float=4) = 32
// Byte - das "TRS Valid"-Feld entfällt laut autoritativer Referenz seit Version 10.1.0.109,
// betrifft unsere 20.0.0.4 also nicht) + 3 weitere Interpolator-Refs (Translation/Roll/
// Scale, je i32). NiInterpolator/NiObject selbst haben KEINE eigenen Felder (wie bei
// NiFloatInterpolator/NiPoint3Interpolator/NiBoolInterpolator auch), das Objekt beginnt
// also direkt mit seinen eigenen Feldern. Struktur aus der autoritativen offiziellen
// niftools/nifxml-Referenzdatei übernommen. Byte-exakt verifiziert an H_AIRDOLL.nif Block
// 144: flags=4, look_at_ref=145 zeigt exakt auf das nächste NiNode, Rotation ist ein exakter
// Einheits-Quaternion (Betrag ≈1.0), Translation/Scale sind beide -FLT_MAX (bekannter
// "nicht gesetzt"-Sentinelwert), alle 3 Interpolator-Refs sauber -1 - die berechnete Länge
// landet exakt auf dem lesbaren Namensfeld "Camera01.Target" des folgenden NiNode.
void SkipNiLookAtInterpolator(ByteReader& r) {
    r.U16();     // flags
    r.I32();     // look_at ref
    r.SizedString(); // look_at_name
    r.Skip(12);  // NiQuatTransform: translation (Vector3)
    r.Skip(16);  // NiQuatTransform: rotation (Quaternion)
    r.F32();     // NiQuatTransform: scale
    r.I32();     // interpolator: translation ref
    r.I32();     // interpolator: roll ref
    r.I32();     // interpolator: scale ref
}

// NiBoolData: eine einzelne KeyGroup<u8> (Boolean-Keyframes, z.B. Partikel-Emitter
// sichtbar/unsichtbar über die Zeit).
void SkipNiBoolData(ByteReader& r) {
    SkipKeyGroupBytes(r);
}

// NiColorData: eine einzelne KeyGroup<Color4> (4 Floats pro Wert, z.B. Partikelfarbe über
// die Zeit, referenziert von NiPSysColorModifier).
void SkipNiColorData(ByteReader& r) {
    SkipKeyGroup(r, 4);
}

// NiPathInterpolator: NiKeyBasedInterpolator (leere Basis) + flags(u16) + bank_dir(i32) +
// max_bank_angle(f32) + smoothing(f32) + follow_axis(i16) + path_data_ref(i32) +
// percent_data_ref(i32). Steuert Objekte, die einem Pfad folgen (z.B. Wegpunkt-Animationen).
void SkipNiPathInterpolator(ByteReader& r) {
    r.U16();  // flags
    r.I32();  // bank_dir
    r.F32();  // max_bank_angle
    r.F32();  // smoothing
    r.I16();  // follow_axis
    r.I32();  // path_data_ref
    r.I32();  // percent_data_ref
}

struct NiTriStripsBlock {
    AVObjectBase base;
    std::int32_t dataRef = -1;
};

// Gemeinsame Kopf-Struktur für NiTriShape UND NiTriStrips ("NiTriBasedGeom"): AVObjectBase +
// data_ref + skin_instance_ref + ein unbekanntes Byte (immer 0) + Freitext-Feld. Byte-exakt
// identisch für beide Geometrie-Typen - verifiziert an mehreren echten NiTriShape-Dateien
// (die berechnete Länge landet exakt auf dem Namensfeld/den Header-Feldern des jeweils
// referenzierten Property-Blocks, analog zur seit v0.13 verifizierten NiTriStrips-Variante).
// WICHTIG (siehe docs/MAP_FORMAT.md): ZWEI unabhängige Versuche, das Freitextfeld hier
// konditional zu machen (einmal über ein has_shader-Byte analog zu NiParticleSystem, einmal
// über eine rein lesende Peek-Heuristik analog zum NiTriStripsData-Trailer-Fix) verursachten
// jeweils einen deutlichen Rückschritt im Massentest (1664 -> 4 bzw. 1664 -> 1280) und wurden
// beide sofort zurückgenommen. Diese Funktion ist die Grundlage für die überwiegende Mehrheit
// aller erfolgreich geladenen Dateien - das unbedingte Lesen ist der einzige bisher stabil
// verifizierte Ansatz und bleibt bewusst UNVERÄNDERT. Die an CynDN_Tree00.nif beobachtete
// 4-Byte-Verschiebung betrifft nur eine kleine Minderheit und ihre tatsächliche Ursache bleibt
// ungeklärt - NICHT über weitere Änderungen an dieser Funktion angehen.
NiTriStripsBlock ParseNiTriStripsHeader(ByteReader& r) {
    NiTriStripsBlock block;
    block.base = ParseAVObjectBase(r);
    block.dataRef = r.I32();
    r.I32(); // skin_instance ref
    r.U8();  // unbekanntes Byte, in Beispieldaten immer 0
    r.SizedString(); // Freitextfeld, Bedeutung unklar (Shader-/Effekt-Name?)
    return block;
}


// NiParticleSystem : NiParticles : NiGeometry. Anders als bei NiTriStrips/NiTriShape (siehe
// ParseNiTriStripsHeader) ist das Byte nach data_ref/skin_instance_ref hier ECHT konditional
// (has_shader) - bei allen bisher verifizierten NiTriStrips-Dateien war zufällig immer ein
// gültiges Freitextfeld vorhanden (auch bei Byte=0 ein Leerstring), weshalb der unbedingte
// SizedString-Read dort nie auffiel. Bei NiParticleSystem ist has_shader dagegen meist 0
// (kein Shader) - ein unbedingtes SizedString-Read liest dann mitten in die folgenden
// Nutzdaten hinein und zerstört die Ausrichtung komplett. KORRIGIERT: has_shader wird jetzt
// echt geprüft, das Freitextfeld (MaterialDataShader: Name + extra_data_ref) nur bei
// has_shader=1 gelesen. Anschließend world_space(u8) + num_modifiers(u32) + je Modifier ein
// Ref(i32). Byte-exakt verifiziert an Leviathan_altar_water_effect01.nif: has_shader=0,
// world_space=1, 9 Modifier-Refs (allesamt plausible Blockindizes 25-34) - die berechnete
// Länge landet exakt auf dem folgenden NiPSysEmitterCtlr (target=6 zeigt exakt hierher zurück).
NiTriStripsBlock SkipNiParticleSystem(ByteReader& r) {
    NiTriStripsBlock block;
    block.base = ParseAVObjectBase(r);
    block.dataRef = r.I32();
    r.I32(); // skin_instance ref
    const std::uint8_t hasShader = r.U8();
    if (hasShader) {
        r.SizedString(); // MaterialDataShader.name
        r.I32();          // MaterialDataShader.extra_data_ref
    }
    r.U8(); // world_space
    const std::uint32_t numModifiers = r.CountU32(256u);
    r.Skip(static_cast<std::size_t>(numModifiers) * 4u);
    return block;
}

NifMaterial ParseNiMaterialProperty(ByteReader& r, bool meshHasTexturing) {
    // KORRIGIERT (dieselbe Ursache wie bei SkipNiStencilProperty, siehe dort): die feste
    // Annahme "name_len=0 direkt gefolgt von controller, ohne num_extra_data_refs" galt nur
    // zufällig für die ursprünglichen Testdateien. ParseObjectNetBase() entscheidet das jetzt
    // korrekt per Peek.
    ParseObjectNetBase(r);
    NifMaterial mat;
    for (float& v : mat.ambient) v = r.F32();
    for (float& v : mat.diffuse) v = r.F32();
    for (float& v : mat.specular) v = r.F32();
    for (float& v : mat.emissive) v = r.F32();
    mat.glossiness = r.F32();
    mat.alpha = r.F32();
    // 14 Kernfelder (4x3 Farben + Glossiness + Alpha) + EIN optionales 15. Float (immer 0.0).
    // Vorhanden bei untexturierten Meshes (kein NiTexturingProperty in der Properties-Liste des
    // zugehörigen NiTriStrips), NICHT vorhanden bei texturierten - verifiziert an 4 echten
    // Dateien (Eld_CD.nif/AddSharpCD.nif untexturiert -> 15; santuary.nif/R_Helga01GL.nif
    // texturiert -> 14, jeweils byte-exakt bis zum Dateiende). Grund für die Korrelation
    // ungeklärt, aber empirisch eindeutig.
    // WICHTIG (siehe docs/MAP_FORMAT.md): ein Versuch, dies über einen LooksLikeFreshName-Peek
    // abzusichern (analog zum NiTriStripsData-Trailer), verursachte einen deutlichen
    // Rückschritt im Massentest (1676 -> 1508) und wurde sofort zurückgenommen - der Peek ist
    // hier offenbar unzuverlässig (Materialfarben-Floats erzeugen anscheinend oft zufällig
    // täuschend "plausible" Namens-Muster in beide Richtungen). Bewusst NICHT
    // weiterverfolgt, die einfache meshHasTexturing-Regel bleibt Standard. Der an
    // S_Tower02_ScanLine.nif beobachtete Gegenfall (kein NiTexturingProperty, aber trotzdem
    // 14 statt 15 Floats) bleibt ein ungelöster Einzelfall.
    if (!meshHasTexturing) {
        r.F32();
    }
    return mat;
}

struct RawTriStripsData {
    std::vector<NifVec3> vertices;
    std::vector<NifVec3> normals;
    std::vector<NifVec2> uvs;
    std::vector<std::vector<std::uint16_t>> strips;
};

// NiTexturingProperty: volle ObjectNetBase (12 Byte bei leerem Namen - KORRIGIERT, siehe
// ParseNiMaterialProperty für die Herleitung) + apply_mode + texture_count + je Slot ein
// Vorhanden-Flag + optional ein TexDesc (Quell-Referenz + Clamp/Filter-Modus + UV-Set +
// optionale Transform). Verifiziert: die source_ref-Werte belegter Slots zeigen exakt auf die
// erwarteten NiSourceTexture-Blöcke (siehe docs/MAP_FORMAT.md). Nur der erste belegte Slot
// (Base-Textur) wird zurückgegeben - Dark/Detail/Gloss/... werden aktuell nicht separat
// verwendet (Multi-Textur-Blending wäre ein eigener Ausbauschritt).
//
// KORRIGIERT: die optionale Transform pro Slot ist 32 Byte (8 Felder: Translation-U/V,
// Scale-U/V, Rotation, Transform-Type(u32!), Center-U/V), NICHT 28 Byte (7 Felder) wie zuvor
// angenommen - die vorherige Annahme ließ das "Transform-Type"-u32-Feld aus. Bug gefunden an
// SD_Vale01_machine02.nif: mit 28 Byte pro Transform landeten nachfolgende Texturslots
// zunehmend außerhalb der gültigen Werte (z.B. ein "Vorhanden"-Flag-Byte von 63 statt 0/1);
// mit 32 Byte ergeben sich für alle 4 belegten Slots plausible, identische Center-Werte
// (0.5, 0.5) und Transform-Type=1, und die vier source_ref-Werte (35, 37, 39, 41) treffen
// exakt auf die vier NiSourceTexture-Blöcke der Datei.
std::int32_t ParseNiTexturingProperty(ByteReader& r, bool hasPS2Fields) {
    // KORRIGIERT: hatte bisher eine eigene, DUPLIZIERTE und veraltete Kopie der ObjectNetBase-
    // Header-Logik (nur geprüft, ob der Peek > 1000 ist) - hat dadurch NICHT von der
    // generalisierten, wertbasierten Plausibilitätsprüfung aus ParseObjectNetBase (Abschnitt
    // 23, v0.33.0) profitiert. An AdlF_field_burn_ground.nif gefunden: peek=6 wurde
    // fälschlich als "6 echte Extra-Daten-Refs" gelesen (texture_count landete dadurch bei
    // absurden 3073) - mit ParseObjectNetBase (controller=6 korrekt erkannt) ergeben sich
    // plausible Werte (apply_mode=2, texture_count=7).
    //
    // VERSUCHT UND VERWORFEN: ein zusätzlicher Rückfallversuch ("wenn texture_count==0,
    // alternative Header-Interpretation erneut versuchen") wurde getestet, verursachte aber
    // einen Netto-RÜCKSCHRITT (-1 Datei) gegenüber der einfachen Variante - die "falschen"
    // Werte sehen in mindestens einem anderen Fall zufällig ebenfalls plausibel genug aus, um
    // den Rückfall zu Unrecht auszulösen. Nicht weiterverfolgt (siehe Abschnitt 20/25 für
    // ähnliche, bereits dokumentierte Grenzen von Peek-Heuristiken).
    const std::size_t startPos = r.Pos();
    const ObjectNetBase base = ParseObjectNetBase(r);
    std::int32_t controllerRef = base.controller;

    r.U32(); // apply_mode
    std::uint32_t textureCount = r.CountU32(64);

    // WEITERER FUND (siehe docs/MAP_FORMAT.md Abschnitt 44): anders als der in Abschnitt 30
    // verworfene "texture_count==0"-Rückfallversuch (schwaches, oft zufällig zutreffendes
    // Signal) ist eine tatsächliche Überschreitung der CountU32-Obergrenze (64) ein sehr
    // starkes, praktisch nie zufällig auftretendes Signal - texture_count ist laut Referenz
    // hart auf einstellige bis niedrige zweistellige Werte begrenzt. Byte-exakt an
    // adel_terrain_root_town.nif verifiziert: peek=8 wurde fälschlich als "8 echte Extra-
    // Daten-Refs" gelesen (alle zufällig <100000, bestehen also die generelle
    // Plausibilitätsprüfung aus ParseObjectNetBase) - texture_count landete dadurch bei
    // absurden ~1.6 Milliarden. Mit der Alternative (kein num_extra_data_refs-Feld,
    // controller=8 direkt) ergeben sich apply_mode=2, texture_count=7 - beide plausibel.
    if (!r.Ok()) {
        r.SetOk(true);
        r.SetPos(startPos);
        r.SizedString(); // name (identisch erneut lesen)
        controllerRef = r.I32(); // Alternative: kein num_extra_data_refs-Feld
        r.U32(); // apply_mode
        textureCount = r.CountU32(64);
    }


    std::int32_t baseSourceRef = -1;
    for (std::uint32_t i = 0; i < textureCount; ++i) {
        const std::uint8_t hasTexture = r.U8();
        if (!hasTexture) continue;
        const std::int32_t sourceRef = r.I32();
        r.U32(); // clamp_mode
        r.U32(); // filter_mode
        r.U32(); // uv_set
        if (hasPS2Fields) {
            // KORRIGIERT: bei älteren NIF-Versionen (vor 10.4.0.1 - bei uns 10.1.0.0/10.2.0.0,
            // siehe autoritative nif.xml-Referenz) hat TexDesc zwei zusätzliche Felder
            // "PS2 L"(short) + "PS2 K"(short) zwischen UV Set und Has Texture Transform, die
            // bei Version 20.0.0.4 bereits entfallen sind. Byte-exakt an
            // skeleton_monolith_blood.nif (Version 10.2.0.0) verifiziert: 2 Texturen (base
            // src=7, dark src=9), beide mit PS2 K=-115 (plausibel, Referenz-Wertebereich
            // -2047..2047) - ohne diese 2 Felder wäre jede Textur-Slot-Grenze ab dem zweiten
            // Slot um 4 Byte fehlausgerichtet.
            r.I16(); // PS2 L
            r.I16(); // PS2 K
        }
        const std::uint8_t hasTransform = r.U8();
        if (hasTransform) {
            r.F32(); r.F32(); // translation u/v
            r.F32(); r.F32(); // scale u/v
            r.F32();          // rotation
            r.U32();          // transform_type (KORRIGIERT - fehlte zuvor)
            r.F32(); r.F32(); // center u/v
        }
        // KORRIGIERT (autoritative nif.xml-Referenz): Slot-Index 5 ("Has Bump Map Texture",
        // NUR vorhanden wenn Texture Count > 5 - was die Schleifengrenze bereits von selbst
        // sicherstellt) hat DREI zusätzliche Felder nach der normalen TexDesc-Struktur:
        // bump_map_luma_scale(f32) + bump_map_luma_offset(f32) + bump_map_matrix(Matrix22 =
        // 4 Floats = 16 Byte) = 24 Byte zusätzlich. Byte-exakt an
        // AdlF_field_burn_ground.nif verifiziert.
        if (i == 5) {
            r.Skip(24);
        }
        if (i == 0 && baseSourceRef < 0) {
            baseSourceRef = sourceRef;
        }
    }
    // Der zuvor hier vermutete 8-Byte-Trailer wurde entfernt: er gehört NICHT zu
    // NiTexturingProperty, sondern ist Teil der NiSourceTexture-Präambel (siehe
    // ParseNiSourceTexture - dort wieder 17 statt 9 Byte). Aufgefallen an Dateien, bei denen
    // nach NiTexturingProperty NICHT direkt eine NiSourceTexture folgt (z.B. R_Helga01GL.nif
    // mit zusätzlichen Property-Blöcken dazwischen) - dort fehlten sonst 8 Byte.
    // GROSSER FUND (mit Hilfe einer unabhängigen Referenzimplementierung, siehe
    // docs/MAP_FORMAT.md Abschnitt 7): nach den 7 Textur-Slots folgt KONDITIONAL noch ein
    // weiteres u32-Feld (vermutlich num_shader_textures o.ä., Bedeutung nicht abschließend
    // verifiziert) - NUR wenn diese NiTexturingProperty einen Controller referenziert
    // (controller_ref != -1). Byte-exakt an 4 echten Dateien verifiziert: `santuary.nif` und
    // `BH_Albi_Ground.nif` (jeweils controller=-1) brauchen das Feld NICHT - direktes Landen
    // auf die 17-Byte-NiSourceTexture-Präambel; `AdlFH_field_burn_ground.nif` und
    // `SD_Vale01_machine02.nif` (controller=12 bzw. 17) brauchen es SEHR WOHL - erst danach
    // treffen alle nachfolgenden NiTextureTransformController-Ketten exakt auf ihre erwarteten
    // Nachbarblöcke (siehe SkipNiTransformController-Verwendung unten). Ohne diese Bedingung
    // (das Feld immer lesen) bricht santuary.nif; ohne das Feld nie zu lesen, bleibt
    // NiTextureTransformController weiterhin scheinbar "inkonsistent lang" (das ursprüngliche,
    // jetzt aufgelöste Rätsel).
    // KORRIGIERT (autoritative offizielle niftools/nifxml-Referenz, direkt geladen): das Feld
    // "Num Shader Textures" ist laut Spezifikation UNBEDINGT vorhanden ab Version 10.0.1.0 -
    // KEINE Bedingung auf controller_ref, wie zuvor vermutet (siehe Abschnitt 25:
    // Eff_2.nif hatte das Feld TROTZ controller_ref=-1, was die alte Regel bereits widerlegt
    // hatte). Ersetzt die bisherige Bedingung durch unbedingtes Lesen.
    const std::uint32_t numShaderTextures = r.CountU32(64);
    // ShaderTexDesc: has_map(bool) + [map(TexDesc) + map_id(u32)] nur wenn has_map. Struktur
    // aus der autoritativen nif.xml-Referenz, byte-exakt an bossroom_wall.nif verifiziert:
    // 3 Shader-Texturen mit source_refs 11/13/15 (konsekutive, gültige Referenzen) und
    // map_id 0/1/2.
    for (std::uint32_t i = 0; i < numShaderTextures; ++i) {
        const std::uint8_t hasMap = r.U8();
        if (!hasMap) continue;
        r.I32();    // source_ref (Shader-Texturen werden aktuell nicht weiterverwendet)
        r.U32();    // clamp_mode
        r.U32();    // filter_mode
        r.U32();    // uv_set
        const std::uint8_t hasTransform = r.U8();
        if (hasTransform) {
            r.Skip(4 * 8); // dieselben 8 Felder wie bei der normalen TexDesc-Transform
        }
        r.U32();    // map_id
    }
    return baseSourceRef;
}

// NiSourceTexture: 9-Byte-Präambel + Dateiname (Sized-String) + pixel_data-Referenz.
// Verifiziert: pixel_data-Referenz zeigt exakt auf den erwarteten NiPixelData-Block, und die
// Gesamtlänge (inkl. des nachfolgenden NiPixelData) trifft exakt auf den nächsten
// Textur-Dateinamen bei Meshes mit mehreren Texturen.
struct NifTextureSource {
    std::string filename;
    std::uint8_t useExternal = 1;
    std::int32_t pixelDataRef = -1;
};

NifTextureSource ParseNiSourceTexture(ByteReader& r) {
    r.Skip(4);
    if (r.PeekU32(0) == 0u) r.U32();
    if (r.PeekU32(0) != 0xFFFFFFFFu) r.U32();
    r.I32();
    NifTextureSource source;
    source.useExternal = r.U8();
    source.filename = r.SizedString();
    source.pixelDataRef = r.I32();
    if (source.useExternal == 1) {
        r.U32(); r.U32(); r.U32();
        r.U8(); r.U8();
        r.U32();
    }
    return source;
}

std::shared_ptr<const NifEmbeddedTexture> ParseNiPixelData(ByteReader& r, bool isOlderVersion) {
    const std::uint32_t pixelFormat = r.PeekU32(0);
    r.Skip(isOlderVersion ? 50 : 72);
    r.I32();
    const std::uint32_t numMipmaps = r.CountU32(32);
    r.U32();
    struct Mip { std::uint32_t width, height, offset; };
    std::vector<Mip> mips;
    mips.reserve(numMipmaps);
    for (std::uint32_t i = 0; i < numMipmaps; ++i) {
        mips.push_back({r.U32(), r.U32(), r.U32()});
    }
    const std::uint32_t dataSize = r.CountU32(64u * 1024u * 1024u);
    const auto allPixels = r.Bytes(dataSize);
    if (!r.Ok() || mips.empty()) return {};
    const auto& top = mips.front();
    if (top.width == 0 || top.height == 0 || top.offset >= allPixels.size()) return {};
    std::size_t topSize = allPixels.size() - top.offset;
    if (mips.size() > 1 && mips[1].offset > top.offset)
        topSize = std::min(topSize, static_cast<std::size_t>(mips[1].offset - top.offset));
    std::vector<std::uint8_t> topData(allPixels.begin() + static_cast<std::ptrdiff_t>(top.offset),
                                      allPixels.begin() + static_cast<std::ptrdiff_t>(top.offset + topSize));
    auto decoded = DecodeBcImage(top.width, top.height, pixelFormat, topData);
    if (!decoded) {
        std::fprintf(stderr, "[NifModel] Eingebettete NiPixelData nicht dekodierbar (Format=%u, %ux%u): %s\n",
                     pixelFormat, top.width, top.height, decoded.error().c_str());
        return {};
    }
    const std::size_t rowBytes = static_cast<std::size_t>(decoded->width) * 4;
    std::vector<std::uint8_t> row(rowBytes);
    for (std::uint32_t y = 0; y < decoded->height / 2; ++y) {
        auto* a = decoded->rgba.data() + static_cast<std::size_t>(y) * rowBytes;
        auto* b = decoded->rgba.data() + static_cast<std::size_t>(decoded->height - 1 - y) * rowBytes;
        std::memcpy(row.data(), a, rowBytes); std::memcpy(a, b, rowBytes); std::memcpy(b, row.data(), rowBytes);
    }
    auto out = std::make_shared<NifEmbeddedTexture>();
    out->width = decoded->width; out->height = decoded->height; out->rgba = std::move(decoded->rgba);
    return out;
}

// NiPixelData: enthält eingebettete Rohpixel-Daten (vermutlich ein Asset-Browser-Thumbnail,
// BC1/DXT1-komprimiert in den geprüften Beispielen - die eigentliche Textur liegt separat als
// .dds vor und wird darüber geladen, siehe DdsImage.hpp). Wird hier nur korrekt ÜBERSPRUNGEN,
// nicht inhaltlich verwendet. Struktur vollständig verifiziert: die aus Mipmap-Anzahl und
// -Größen berechnete Gesamtlänge trifft bei mehreren Testdateien exakt auf den jeweils
// nächsten Block (siehe docs/MAP_FORMAT.md - Herleitung über BC1-Blockkompressions-Mathematik:
// jede Mipmap-Stufe benötigt exakt (Breite/4)*(Höhe/4)*8 Byte, minimal 8 Byte).
void SkipNiPixelData(ByteReader& r, bool isOlderVersion) {
    // KORRIGIERT: für ältere NIF-Versionen (10.1.0.0/10.2.0.0) ist die NiPixelFormat-
    // Kopfstruktur NICHT 72 Byte (18x u32, empirisch für 20.0.0.4 hergeleitet) lang, sondern
    // 50 Byte - weicht auch von der offiziellen "bis 10.4.0.1"-Referenzstruktur (36 Byte) ab
    // (siehe docs/MAP_FORMAT.md Abschnitt 33: dieser custom Engine-Fork hat ein eigenes
    // Pixelformat-Layout, das von BEIDEN Standard-Varianten abweicht). Byte-exakt an
    // skeleton_monolith_blood.nif verifiziert: palette_ref=-1 (kein Palette), gefolgt von
    // num_mipmaps=9 - passt EXAKT zur unmittelbar folgenden, sauberen Mipmap-Kette
    // 256,128,64,32,16,8,4,2,1 (vollständige Zweierpotenz-Reihe mit stimmigen,
    // aufsteigenden Offsets für ein komprimiertes DXT5-Format).
    if (isOlderVersion) {
        r.Skip(50);
    } else {
        for (int i = 0; i < 18; ++i) r.U32(); // Pixelformat/Masken/u.a., Bedeutung im Detail ungeklärt
    }
    r.I32(); // palette ref
    const std::uint32_t numMipmaps = r.CountU32(32);
    r.U32(); // unbekanntes Feld, empirisch 0
    for (std::uint32_t i = 0; i < numMipmaps; ++i) {
        r.U32(); r.U32(); r.U32(); // width, height, offset - nicht weiterverwendet
    }
    const std::uint32_t dataSize = r.CountU32(64u * 1024u * 1024u); // großzügige Obergrenze (64 MB)
    r.Skip(dataSize);
}

// WICHTIGER, GRÖSSERER FUND (siehe docs/MAP_FORMAT.md, Abschnitt "UV-Koordinaten..."):
// Die aus NiTriStripsData/NiTriShapeData extrahierten UV-Koordinaten sind in praktisch JEDER
// bisher geprüften echten Datei unbrauchbar (extrem große/kleine, inkonsistente Float-Werte),
// UNABHÄNGIG von Textur, Geometrie oder numUvSets - selbst am bislang am gründlichsten
// verifizierten Referenzobjekt (santuary.nif). Die Byte-LÄNGE des UV-Abschnitts ist dabei
// zweifelsfrei korrekt (mehrfach bestätigt: alle Felder danach - inkl. Dreieckszahl, Streifen-
// länge und der bekannte 8-Byte-Trailer - treffen exakt bis zum Dateiende; auch alle Vertex-
// und Normalen-Daten VOR den UVs sind einwandfrei, alle 86 Normalen von santuary.nif sind
// exakte Einheitsvektoren). Die Ursache bleibt ungeklärt (evtl. Datenqualitätsproblem in den
// Originaldateien - z.B. ungenutzter/nie befüllter UV-Kanal - oder ein noch nicht gefundenes
// Detail der echten Kodierung). Bis das geklärt ist: lieber KEINE Textur anzeigen als eine
// mit Sicherheit falsch gemappte - die Prüfung hier verwirft unplausible UV-Sets komplett
// (der Aufrufer fällt dann automatisch auf die Materialfarbe zurück, siehe NifMeshPart::uvs
// Kommentar). Betrifft NICHT die Vertex-Positionen/Normalen/Dreiecke - nur die UV-Koordinaten.
void SanitizeUvs(std::vector<NifVec2>& uvs) {
    for (const auto& uv : uvs) {
        const bool implausible =
            !std::isfinite(uv.u) || !std::isfinite(uv.v) ||
            std::abs(uv.u) > 1000.0f || std::abs(uv.v) > 1000.0f;
        if (implausible) {
            uvs.clear();
            return;
        }
    }
}

RawTriStripsData ParseNiTriStripsData(ByteReader& r, bool hasTrailer, bool isOlderVersion) {
    RawTriStripsData d;
    // KORRIGIERT: "Num Vertices" ist laut Format ein uint16, gefolgt von Keep-Flags(u8) +
    // Compress-Flags(u8) - NICHT ein einzelnes uint32 wie zuvor angenommen. Der u32-Read war
    // nur deshalb "byte-exakt verifiziert", weil Keep-/Compress-Flags in allen bisher
    // getesteten Dateien zufällig 0 waren (macht den u32-Wert zahlengleich zum echten u16-Wert).
    // Bei mindestens einer echten Datei sind sie ungleich 0 (siehe ParseNiTriShapeData für die
    // Herleitung mit plausiblen Vertex-Koordinaten) - dort führte der alte u32-Read zu einer
    // absurd großen "Vertex-Anzahl". Reine Präzisierung, kein Verhalten für die weit
    // überwiegende Mehrheit der Dateien geändert (dort bleiben die Byte-Positionen identisch).
    const std::uint32_t numVerts = r.CountU16(20000u);
    r.U8(); // keep_flags
    r.U8(); // compress_flags
    const std::uint8_t hasVerts = r.U8();
    if (hasVerts) {
        d.vertices.reserve(numVerts);
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            // Achsen-Remap wie bei der Objekt-Platzierung (siehe ObjectPlacementIO.cpp):
            // .nif ist wie das gesamte Legacy-Format Z-up (X/Y = horizontale Ebene, Z = Höhe),
            // intern durchgängig Y-up. Ohne diese Umrechnung erscheinen Meshes um 90° verdreht
            // ("liegend" statt "stehend").
            const float legacyX = r.F32();
            const float legacyY = r.F32();
            const float legacyZ = r.F32();
            d.vertices.push_back({legacyX, legacyZ, legacyY});
        }
    }
    // KORRIGIERT/BESTÄTIGT (autoritative nif.xml-Referenz, direkt von GitHub geladen): dieses
    // Feld ist "Data Flags" (NiGeometryDataFlags, u16) - die unteren 6 Bit sind die Anzahl der
    // UV-Sets, was die bereits vorher (unabhängig über einen Rust-Referenzparser) gefundene
    // Maskierung "& 0x3F" bestätigt. NEU aus der autoritativen Referenz: Bit 12 (0x1000)
    // zeigt an, ob zusätzlich zu den Normalen auch Tangenten+Binormalen (je Vector3 pro
    // Vertex) vorhanden sind - direkt nach den Normalen, VOR der Bounding Sphere. Bisher
    // nicht behandelt (immer implizit als "nicht vorhanden" angenommen).
    const std::uint16_t dataFlags = r.CountU16(0xFFFFu);
    const std::uint32_t numUvSets = dataFlags & 0x3Fu;
    const bool hasTangentSpace = (dataFlags & 0x1000u) != 0;
    const std::uint8_t hasNormals = r.U8();
    if (hasNormals) {
        d.normals.reserve(numVerts);
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            // Gleiches Achsen-Remap wie bei den Positionen.
            const float legacyX = r.F32();
            const float legacyY = r.F32();
            const float legacyZ = r.F32();
            d.normals.push_back({legacyX, legacyZ, legacyY});
        }
        if (hasTangentSpace) {
            // Tangenten + Binormalen (je Vector3 pro Vertex) - werden aktuell nicht
            // weiterverwendet (kein Normal-Mapping im Editor), müssen aber für die korrekte
            // Byte-Ausrichtung konsumiert werden.
            r.Skip(static_cast<std::size_t>(numVerts) * 12u * 2u);
        }
    }
    r.F32(); r.F32(); r.F32(); r.F32(); // Bounding-Sphere (center xyz + radius)
    const std::uint8_t hasColors = r.U8();
    if (hasColors) {
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            r.F32(); r.F32(); r.F32(); r.F32();
        }
    }
    // HAUPTFUND DIESER KORREKTUR: das früher hier (VOR den UV-Daten) gelesene "uv_flags"-u16
    // existiert an dieser Stelle laut Referenzimplementierung GAR NICHT - auf die Vertexfarben
    // folgen die UV-Sets direkt. Das bisherige Skippen dieser 2 Byte hat JEDE UV-Koordinate um
    // 2 Byte fehlausgerichtet gelesen (führte zu astronomisch großen/winzigen Werten, siehe
    // docs/MAP_FORMAT.md) - obwohl die GESAMTLÄNGE zufällig trotzdem exakt bis zum Dateiende
    // aufging, weil ein bislang unidentifiziertes 2-Byte-Feld tatsächlich HINTER den UV-Daten
    // liegt (statt davor) und die 2 Byte dort weiterhin konsumiert werden müssen (siehe unten).
    for (std::uint32_t set = 0; set < numUvSets; ++set) {
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            const float u = r.F32();
            const float v = r.F32();
            if (set == 0) {
                d.uvs.push_back({u, v});
            }
        }
    }
    // Das mysteriöse 2-Byte-Feld (Bedeutung weiterhin ungeklärt) gehört HIER hin, nicht vor die
    // UV-Daten. Byte-exakt verifiziert an santuary.nif: mit dieser Anordnung sind ALLE 86 UV-
    // Paare plausible, normalisierte Texturkoordinaten (0..1-Bereich, z.B. (0.213, 0.015)) UND
    // alle nachfolgenden Felder (consistency_flags=-1, num_triangles=284 - stimmt exakt mit der
    // Streifenlänge 286 überein, der bekannte 8-Byte-Trailer) treffen weiterhin exakt bis zum
    // letzten Byte der Datei.
    r.U16();
    // KORRIGIERT (siehe docs/MAP_FORMAT.md Abschnitt 36): "Additional Data" ist laut
    // autoritativer Referenz erst SEIT Version 20.0.0.4 vorhanden - bei älteren Versionen
    // (10.1.0.0/10.2.0.0) entfällt dieses Feld komplett, sonst verschieben sich num_triangles/
    // num_strips/strip_lengths/points um 4 Byte. Byte-exakt an skeleton_monolith_blood.nif
    // (Version 10.2.0.0) verifiziert: ohne additional_data_ref ergeben sich consistency_flags
    // =0x4000 (CT_STATIC, gültig), num_triangles=513, ein einzelner Streifen der Länge 515
    // (passt exakt zur Dreieckszahl+2) und eine klassische Streifen-Indexfolge
    // (0,1,2,2,3,3,3,4,5,5,...) - mit dem Feld dagegen unplausible Werte.
    if (!isOlderVersion) {
        r.I32(); // consistency_flags/additional_data ref, empirisch -1
    } else {
        r.U16(); // nur consistency_flags
    }
    r.U16(); // num_triangles
    const std::uint16_t numStrips = r.CountU16(20000u);
    std::vector<std::uint16_t> stripLengths(numStrips);
    for (auto& sl : stripLengths) sl = r.CountU16(60000u);
    const std::uint8_t hasPoints = r.U8();
    if (hasPoints) {
        for (const auto sl : stripLengths) {
            std::vector<std::uint16_t> strip(sl);
            for (auto& p : strip) p = r.U16();
            d.strips.push_back(std::move(strip));
        }
    }
    // Trailer NICHT vorhanden, wenn direkt ein weiteres NiTriStrips/NiTriShape ODER eine
    // NiSkinInstance folgt (siehe Aufrufer) - analog zum konditionalen Trailer bei NiPixelData
    // (siehe SkipNiPixelData). Empirisch an Rou_M_Tube.nif und Tunnel02_Wood3.nif verifiziert.
    // ERGÄNZT: diese Bedingung ist NICHT vollständig - bei mindestens einer Datei
    // (ItemShop02.nif, NiTriStripsData gefolgt von NiDirectionalLight) fehlt der Trailer
    // EBENFALLS, obwohl der Folgeblock keiner der oben explizit ausgeschlossenen Typen ist.
    // Per Plausibilitäts-Peek robust behandelt (siehe LooksLikeFreshName) - ABER NUR im
    // "hasTrailer"-Standardfall: der explizite Ausschluss (hasTrailer=false, vom Aufrufer
    // anhand des konkreten Folgeblocktyps bestimmt) wird NICHT durch den Peek in Frage
    // gestellt, da z.B. NiSkinInstance nicht mit einem Namensfeld beginnt und der Peek dort
    // fälschlich "nicht plausibel" ergäbe (byte-exakt an Tunnel02_Wood3.nif entdeckt - ein
    // ungeprüfter Peek-Override hätte den expliziten, korrekten Ausschluss sonst rückgängig
    // gemacht).
    if (hasTrailer) {
        const bool freshWithTrailer = LooksLikeFreshName(r, 8);
        const bool freshWithoutTrailer = LooksLikeFreshName(r, 0);
        if (!freshWithTrailer && freshWithoutTrailer) {
            // Trailer wie vermutet nicht vorhanden - nichts weiter überspringen.
        } else {
            r.Skip(8);
        }
    }
    SanitizeUvs(d.uvs);
    return d;
}


void ExpandTriangleStrip(const std::vector<std::uint16_t>& strip, std::vector<std::uint32_t>& outIndices) {
    if (strip.size() < 3) return;
    for (std::size_t i = 0; i + 2 < strip.size(); ++i) {
        const std::uint16_t a = strip[i];
        const std::uint16_t b = strip[i + 1];
        const std::uint16_t c = strip[i + 2];
        if (a == b || b == c || a == c) continue;
        if (i % 2 == 0) {
            outIndices.push_back(a);
            outIndices.push_back(b);
            outIndices.push_back(c);
        } else {
            outIndices.push_back(a);
            outIndices.push_back(c);
            outIndices.push_back(b);
        }
    }
}

struct RawTriShapeData {
    std::vector<NifVec3> vertices;
    std::vector<NifVec3> normals;
    std::vector<NifVec2> uvs;
    std::vector<std::uint16_t> triangleIndices; // flach, 3 pro Dreieck, direkt in `vertices` indiziert
};

// NiTriShapeData ("NiTriBasedGeomData"): TEILT sich den kompletten Vertex-/Normalen-/
// Farben-/UV-Kopf byte-exakt mit NiTriStripsData (bewusst dupliziert statt geteilt, um das
// bereits verifizierte NiTriStripsData NICHT anzufassen) - bis einschließlich des
// gemeinsamen "num_triangles"(u16)-Felds nach dem consistency_flags-Ref. Danach divergiert die
// Struktur: NiTriShapeData hat statt Streifen eine FLACHE Dreiecksliste:
// num_triangle_points(u32, = num_triangles*3) + has_triangles(u8) + falls vorhanden
// num_triangle_points Vertex-Indizes (u16, direkt 3er-Gruppen = Dreiecke, keine
// Streifen-Expansion nötig) + num_match_groups(u16) + je Gruppe num_vertices(u16) +
// Vertex-Indizes (geteilte-Normalen-Gruppen, nicht weiterverwendet).
// Byte-exakt verifiziert an `BH_Albi_Ground.nif`: NiTriShapeData ist dort der LETZTE Block der
// Datei - die berechnete Endposition trifft exakt auf das tatsächliche Dateiende (24346 Byte,
// hartes Constraint wie bei den kleinen NiTriStripsData-Referenzdateien in v0.13).
RawTriShapeData ParseNiTriShapeData(ByteReader& r, bool hasTrailer, bool isOlderVersion) {
    RawTriShapeData d;
    // "Num Vertices"(u16) + Keep-Flags(u8) + Compress-Flags(u8) - siehe ausführliche Herleitung
    // bei ParseNiTriStripsData. Byte-exakt verifiziert an BerFrz01_IceSmog.nif: mit dem alten
    // u32-Read ergab sich eine absurde Vertex-Anzahl (3.342.408); mit u16(72)+keep(0x33=51,
    // Bedeutung ungeklärt)+compress ergeben sich 72 plausible Vertex-Koordinaten (z.B. eine
    // radialsymmetrische Anordnung: v1.y ≈ v4.x ≈ 492.24 - passend zu einem kegel-/
    // ringförmigen Partikel-Mesh).
    const std::uint32_t numVerts = r.CountU16(20000u);
    r.U8(); // keep_flags
    r.U8(); // compress_flags
    const std::uint8_t hasVerts = r.U8();
    if (hasVerts) {
        d.vertices.reserve(numVerts);
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            const float legacyX = r.F32();
            const float legacyY = r.F32();
            const float legacyZ = r.F32();
            d.vertices.push_back({legacyX, legacyZ, legacyY}); // Z-up -> Y-up, siehe oben
        }
    }
    // Siehe ParseNiTriStripsData: "Data Flags" ist ein NiGeometryDataFlags (u16) - untere 6
    // Bit = Anzahl UV-Sets, Bit 12 (0x1000) = Tangenten+Binormalen vorhanden. Der frühere
    // CountU16(16u)-Cap hätte jede Datei mit gesetzten höheren Bits fälschlich abgelehnt.
    const std::uint16_t dataFlags = r.CountU16(0xFFFFu);
    const std::uint32_t numUvSets = dataFlags & 0x3Fu;
    const bool hasTangentSpace = (dataFlags & 0x1000u) != 0;
    const std::uint8_t hasNormals = r.U8();
    if (hasNormals) {
        d.normals.reserve(numVerts);
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            const float legacyX = r.F32();
            const float legacyY = r.F32();
            const float legacyZ = r.F32();
            d.normals.push_back({legacyX, legacyZ, legacyY});
        }
        if (hasTangentSpace) {
            r.Skip(static_cast<std::size_t>(numVerts) * 12u * 2u);
        }
    }
    r.F32(); r.F32(); r.F32(); r.F32(); // Bounding-Sphere (center xyz + radius)
    const std::uint8_t hasColors = r.U8();
    if (hasColors) {
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            r.F32(); r.F32(); r.F32(); r.F32();
        }
    }
    // Kein "uv_flags" vor den UV-Daten - siehe ParseNiTriStripsData für die vollständige
    // Herleitung dieser Korrektur.
    for (std::uint32_t set = 0; set < numUvSets; ++set) {
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            const float u = r.F32();
            const float v = r.F32();
            if (set == 0) {
                d.uvs.push_back({u, v});
            }
        }
    }
    // WEITERER FUND (siehe docs/MAP_FORMAT.md Abschnitt 43): anders als bei
    // ParseNiTriStripsData bleibt dieses "mysteriöse" u16-Feld NICHT für alle Versionen
    // bestehen - bei älteren Versionen (10.1.0.0/10.2.0.0) entfällt es komplett (nicht nur
    // additional_data_ref). Byte-exakt an horse2.nif (Version 10.2.0.0) verifiziert: ohne
    // dieses Feld ergeben sich consistency_flags=0x4000 (CT_STATIC, gültig),
    // num_triangles=700 und num_triangle_points=2100 (exakt num_triangles*3) - mit dem Feld
    // dagegen durchgehend unplausible Werte.
    if (!isOlderVersion) {
        r.U16(); // mysteriöses 2-Byte-Feld NACH den UV-Daten - siehe ParseNiTriStripsData
    }
    // KORRIGIERT (siehe docs/MAP_FORMAT.md Abschnitt 36): "Additional Data" entfällt bei
    // älteren Versionen komplett - analog zu ParseNiTriStripsData.
    if (!isOlderVersion) {
        r.I32(); // consistency_flags/additional_data ref, empirisch -1
    } else {
        r.U16(); // nur consistency_flags
    }
    r.U16(); // num_triangles (gemeinsames Feld, hier nicht direkt verwendet)

    // Ab hier divergiert die Struktur von NiTriStripsData:
    const std::uint32_t numTrianglePoints = r.CountU32(600000u); // = num_triangles * 3
    const std::uint8_t hasTriangles = r.U8();
    if (hasTriangles) {
        d.triangleIndices.reserve(numTrianglePoints);
        for (std::uint32_t i = 0; i < numTrianglePoints; ++i) {
            d.triangleIndices.push_back(r.U16());
        }
    }
    const std::uint16_t numMatchGroups = r.CountU16(20000u);
    for (std::uint16_t g = 0; g < numMatchGroups; ++g) {
        const std::uint16_t numVertsInGroup = r.CountU16(60000u);
        for (std::uint16_t i = 0; i < numVertsInGroup; ++i) r.U16();
    }
    // Gleicher konditionaler 8-Byte-Trailer wie bei NiTriStripsData (siehe dort) - fehlt, wenn
    // direkt ein weiteres NiTriShape/NiTriStrips oder eine NiSkinInstance folgt
    // (Mehrfach-Mesh-Objekt bzw. geskinntes Mesh). Gleiche Peek-Absicherung wie dort (siehe
    // LooksLikeFreshName) NUR im Standardfall (hasTrailer=true) - der explizite Ausschluss wird
    // nicht durch den Peek in Frage gestellt (siehe ausführliche Begründung dort).
    if (hasTrailer) {
        const bool freshWithTrailer = LooksLikeFreshName(r, 8);
        const bool freshWithoutTrailer = LooksLikeFreshName(r, 0);
        if (!freshWithTrailer && freshWithoutTrailer) {
            // Trailer wie vermutet nicht vorhanden.
        } else {
            r.Skip(8);
        }
    }
    SanitizeUvs(d.uvs);
    return d;
}

// WEITERER FUND (siehe docs/MAP_FORMAT.md Abschnitt 39/42): folgt auf einen Property-Block
// (NiAlphaProperty, NiZBufferProperty, ...) DIREKT eine NiTriStripsData/NiTriShapeData (statt
// des üblichen NiTriStrips/NiTriShape-Wrappers dazwischen), fehlen 4 zusätzliche Byte -
// Ursache weiterhin ungeklärt (nicht in der autoritativen nif.xml-Referenz zu finden,
// unabhängig von der NIF-Version bestätigt). Byte-exakt an mehreren Dateien verifiziert
// (Leviathan_lightA_non.nif für NiAlphaProperty, skeleton_monolith_blood.nif und
// field_sky_01.nif für NiZBufferProperty - jeweils landet man erst mit den +4 Byte auf einem
// plausiblen num_vertices/keep_flags/compress_flags/has_vertices-Muster).
void SkipExtraBytesIfFollowedByTriData(ByteReader& r, const NifHeader& hdr, std::size_t blockIdx) {
    if (blockIdx + 1 < hdr.blockTypeIndex.size()) {
        const std::string& nextType = hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]];
        if (nextType == "NiTriStripsData" || nextType == "NiTriShapeData") {
            r.Skip(4);
        }
    }
}

// GENERALISIERTE VARIANTE (Abschnitt 42): prüft DIREKT an der vermuteten NiTriStripsData/
// NiTriShapeData-Position, ob num_vertices(u16)+keep_flags(u8)+compress_flags(u8)+
// has_vertices(u8) an gegebenem Offset plausibel aussehen - unabhängig davon, welcher
// Blocktyp unmittelbar davor stand. Ersetzt die Notwendigkeit, jeden möglichen Vorgänger-Typ
// einzeln aufzuzählen (byte-exakt auch für NiFloatData als Vorgänger bestätigt, siehe
// Abschnitt 42).
bool LooksLikeTriDataHeader(const ByteReader& r, std::size_t offset) {
    const std::uint32_t numVerts = r.PeekU32(offset) & 0xFFFFu;
    if (numVerts < 3 || numVerts > 20000) return false;
    const std::uint8_t keep = r.PeekU8(offset + 2);
    const std::uint8_t compress = r.PeekU8(offset + 3);
    const std::uint8_t hasVerts = r.PeekU8(offset + 4);
    return keep <= 1 && compress <= 1 && hasVerts <= 1;
}

} // namespace

std::expected<NifModel, std::string> LoadNifMesh(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte NIF-Datei nicht \u00f6ffnen: " + file.string());
    }
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    ByteReader r(data);
    auto headerResult = ParseHeader(r, data);
    if (!headerResult) {
        return std::unexpected(headerResult.error());
    }
    const NifHeader& hdr = *headerResult;
    const bool traceBlocks = std::getenv("NEXTGEN_NIF_TRACE") != nullptr;

    static const std::unordered_set<std::string> kSupportedPropertyTypes = {
        "NiZBufferProperty", "NiVertexColorProperty", "NiMaterialProperty", "NiTexturingProperty",
        "NiAlphaProperty", "NiStencilProperty", "NiSpecularProperty", "NiFogProperty",
        "NiDitherProperty",
    };

    NifModel model;
    bool rootNameSet = false;
    // Ob das aktuell verarbeitete NiTriStrips eine NiTexturingProperty referenziert - bestimmt,
    // ob dessen NiMaterialProperty 14 oder 15 Floats hat (siehe ParseNiMaterialProperty).
    bool currentMeshHasTexturing = false;
    // Für die Auflösung "welcher Mesh-Teil bekommt welche Diffuse-Textur": NiTexturingProperty
    // kennt nur die Block-Referenz (die zugehörige NiSourceTexture kommt sequentiell erst
    // SPÄTER), daher zweistufig - erst sammeln, nach der Hauptschleife auflösen.
    std::unordered_map<std::uint32_t, std::string> sourceTextureFilenames; // legacy/cube-map names
    std::unordered_map<std::uint32_t, NifTextureSource> sourceTextures;
    std::unordered_map<std::uint32_t, std::shared_ptr<const NifEmbeddedTexture>> embeddedPixelTextures;
    std::unordered_map<std::size_t, std::int32_t> partBaseTextureRef;      // Part-Index -> Blockindex

    for (std::uint32_t blockIdx = 0; blockIdx < hdr.numBlocks; ++blockIdx) {
        if (blockIdx >= hdr.blockTypeIndex.size()) break;
        const std::string& type = hdr.blockTypes[hdr.blockTypeIndex[blockIdx]];
        const std::size_t blockStart = r.Pos();
        if (traceBlocks) {
            std::fprintf(stderr, "[NifTrace] begin block=%u type=%s offset=%zu\n",
                         blockIdx, type.c_str(), blockStart);
        }

        if (type == "NiNode") {
            NiNodeBlock node = ParseNiNode(r);
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiBillboardNode") {
            // NiBillboardNode : NiNode + ein zusätzliches uint16-Feld (Billboard-Modus,
            // z.B. für Gras/Blätter, die immer zur Kamera zeigen). Rendering-seitig aktuell
            // wie ein normaler NiNode behandelt (kein eigenes Billboard-Verhalten im Editor,
            // da die Objekte nur platziert, nicht animiert live gerendert werden müssen).
            NiNodeBlock node = ParseNiNode(r);
            r.U16(); // billboard_mode
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiLODNode") {
            // NiLODNode : NiSwitchNode : NiNode. NiSwitchNode ergänzt switch_flags(u16) +
            // index(u32, aktuell aktiver Kind-Index), NiLODNode ergänzt lod_level_data_ref
            // (i32, zeigt meist auf eine NiRangeLODData). Rendering-seitig aktuell wie ein
            // normaler NiNode behandelt (alle Kinder werden platziert, keine echte
            // Entfernungs-basierte LOD-Umschaltung im Editor). Byte-exakt verifiziert an
            // tree05.nif: Name="LODGroup01", 3 Kinder (je ein LOD-Level), lod_level_data_ref
            // zeigt exakt auf die letzte NiRangeLODData der Datei.
            NiNodeBlock node = ParseNiNode(r);
            r.U16();  // switch_flags
            r.U32();  // index
            r.I32();  // lod_level_data_ref
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiSortAdjustNode") {
            NiNodeBlock node = ParseNiSortAdjustNode(r);
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiRoomGroup") {
            NiNodeBlock node = ParseNiRoomGroup(r);
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiRoom") {
            NiNodeBlock node = ParseNiRoom(r);
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiRangeLODData") {
            // KORRIGIERT: weicht von der öffentlichen Referenzstruktur ab (dort: Center-
            // Vector3 + num_lod_levels + Level-Paare) - in diesem Fork stattdessen: ein
            // einzelnes führendes u32-Feld (empirisch immer 0, evtl. eine vereinfachte/
            // weggelassene "Center"-Angabe) + num_lod_levels(u32) + je Level near(f32)+far(f32)
            // + ein abschließender 8-Byte-Trailer (Bedeutung ungeklärt, empirisch immer
            // 01 00 00 00 00 00 00 00). Byte-exakt an 2 echten Dateien verifiziert
            // (Adl_field_tree01.nif und Adl_field_tree02.nif, beide mit identischer
            // 3-Stufen-LOD-Konfiguration 0-1000/1000-2000/2000-100000000 Einheiten): die
            // berechnete Länge (40 Byte) trifft in BEIDEN Fällen exakt auf das jeweilige
            // Dateiende.
            r.U32(); // führendes Feld, empirisch immer 0
            const std::uint32_t numLodLevels = r.CountU32(64u);
            r.Skip(static_cast<std::size_t>(numLodLevels) * 8u); // je Level: near(f32)+far(f32)
            r.Skip(8); // abschließender Trailer, Bedeutung ungeklärt
        } else if (type == "NiStringExtraData") {
            SkipNiStringExtraData(r);
        } else if (type == "NiIntegerExtraData") {
            SkipNiIntegerExtraData(r);
        } else if (type == "NiTextKeyExtraData") {
            SkipNiTextKeyExtraData(r);
        } else if (type == "NiFloatExtraData") {
            SkipNiFloatExtraData(r);
        } else if (type == "NiColorExtraData") {
            SkipNiColorExtraData(r);
        } else if (type == "NiBooleanExtraData") {
            SkipNiBooleanExtraData(r);
        } else if (type == "NiIntegersExtraData") {
            SkipNiIntegersExtraData(r);
        } else if (type == "NiPalette") {
            SkipNiPalette(r);
        } else if (type == "NiVisController") {
            SkipNiVisController(r);
        } else if (type == "NiMultiTargetTransformController") {
            SkipNiMultiTargetTransformController(r);
        } else if (type == "NiGeomMorpherController") {
            SkipNiGeomMorpherController(r);
        } else if (type == "NiMorphData") {
            SkipNiMorphData(r);
        } else if (type == "NiPSysColliderManager") {
            SkipNiPSysColliderManager(r);
        } else if (type == "NiFogProperty") {
            SkipNiFogProperty(r);
            SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
        } else if (type == "NiDitherProperty") {
            SkipNiDitherProperty(r);
            SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
        } else if (type == "NiSourceCubeMap") {
            sourceTextureFilenames[blockIdx] = ParseNiSourceTexture(r).filename;
        } else if (type == "NiPortal") {
            SkipNiPortal(r);
        } else if (type == "NiCollisionData") {
            SkipNiCollisionData(r);
        } else if (type == "NiTransformController") {
            SkipNiTransformController(r);
        } else if (type == "NiTransformInterpolator") {
            SkipNiTransformInterpolator(r);
        } else if (type == "NiTransformData") {
            SkipNiTransformData(r);
        } else if (type == "NiAlphaController") {
            // NiAlphaController ist wie NiTransformController ein NiSingleInterpController
            // mit exakt derselben 30-Byte-Basis (next_controller/flags/frequency/phase/start/
            // stop/target/interpolator_ref), ohne weitere eigene Felder. Byte-exakt verifiziert
            // an AdlFH_field_burn_ground.nif: target=5 zeigt exakt auf die zugehörige
            // NiMaterialProperty, interpolator_ref=7 zeigt exakt auf die folgende
            // NiFloatInterpolator.
            SkipNiTransformController(r);
        } else if (type == "NiTextureTransformController") {
            // GELÖSTES RÄTSEL (siehe docs/MAP_FORMAT.md Abschnitt 7): dieselbe 30-Byte-
            // NiSingleInterpController-Basis + shader_map(u8) + texture_slot(u32) +
            // operation(u32) = 39 Byte, IMMER FEST, KEINE Variation zwischen Instanzen (die
            // frühere Beobachtung unterschiedlicher Längen war ein Artefakt eines fehlenden
            // Feldes in ParseNiTexturingProperty, siehe dort - nach dessen Behebung sind ALLE
            // Instanzen einheitlich 39 Byte). Byte-exakt verifiziert an
            // AdlFH_field_burn_ground.nif UND SD_Vale01_machine02.nif (2 bzw. 6 aufeinander-
            // folgende Instanzen je Datei): next_controller verkettet sauber (z.B.
            // 18→19→20→21→22→(-1)), target zeigt immer auf die gemeinsame
            // NiTexturingProperty, interpolator_ref trifft exakt auf die jeweils erwartete
            // NiFloatInterpolator.
            SkipNiTransformController(r);
            r.U8();  // shader_map
            r.U32(); // texture_slot
            r.U32(); // operation
        } else if (type == "NiFloatInterpolator") {
            SkipNiFloatInterpolator(r);
        } else if (type == "NiFloatData") {
            SkipNiFloatData(r);
        } else if (type == "NiMaterialColorController") {
            SkipNiMaterialColorController(r);
        } else if (type == "NiPoint3Interpolator") {
            SkipNiPoint3Interpolator(r);
        } else if (type == "NiPosData") {
            SkipNiPosData(r);
        } else if (type == "NiParticleSystem" || type == "NiMeshParticleSystem") {
            // NiMeshParticleSystem hat laut Referenz denselben NiParticleSystem-Kopf (nur die
            // referenzierte Daten-Klasse unterscheidet sich, NiMeshPSysData statt NiPSysData -
            // für unsere Zwecke, da wir keine Partikel rendern, ist nur die Kopf-Länge relevant).
            NiTriStripsBlock psys = SkipNiParticleSystem(r);
            // KORRIGIERT: currentMeshHasTexturing wurde bisher NUR bei NiTriStrips/NiTriShape
            // aktualisiert - bei einem NiParticleSystem blieb der Wert vom zuletzt gesehenen,
            // völlig unabhängigen Mesh stehen. Die folgende NiMaterialProperty des
            // Partikelsystems bekam dadurch den FALSCHEN meshHasTexturing-Wert übergeben,
            // wodurch das optionale 15. Float-Feld fälschlich gelesen/nicht gelesen wurde -
            // ein 4-Byte-Versatz, der sich erst viel später bei NiPSysData bemerkbar machte
            // (absurd hohe Vertex-Anzahl). Byte-exakt verifiziert an
            // BH_Karen_water_effect.nif: NiParticleSystem referenziert Property-Block 12
            // (NiTexturingProperty) - mit der Korrektur trifft die anschließende
            // NiMaterialProperty korrekt 14 statt 15 Felder, und NiPSysData bekommt eine
            // plausible Vertex-Anzahl.
            currentMeshHasTexturing = false;
            for (const auto propRef : psys.base.properties) {
                if (propRef < 0 || static_cast<std::uint32_t>(propRef) >= hdr.blockTypeIndex.size()) continue;
                const std::string& propType = hdr.blockTypes[hdr.blockTypeIndex[static_cast<std::size_t>(propRef)]];
                if (propType == "NiTexturingProperty") {
                    currentMeshHasTexturing = true;
                }
            }
        } else if (type == "NiPSysData" || type == "NiMeshPSysData") {
            SkipNiPSysData(r, hdr.version, type == "NiMeshPSysData");
        } else if (type == "NiPSysEmitterCtlr") {
            SkipNiPSysEmitterCtlr(r);
        } else if (type == "NiPSysModifierActiveCtlr") {
            SkipNiPSysModifierActiveCtlr(r);
        } else if (type == "NiPSysGravityStrengthCtlr") {
            // NiPSysGravityStrengthCtlr = NiPSysModifierFloatCtlr = NiPSysModifierCtlr, exakt
            // dieselbe Struktur wie NiPSysModifierActiveCtlr (30-Byte-Basis + modifier_name).
            SkipNiPSysModifierActiveCtlr(r);
        } else if (type == "NiPSysEmitterLifeSpanCtlr") {
            // Ebenfalls NiPSysModifierFloatCtlr - dieselbe Struktur.
            SkipNiPSysModifierActiveCtlr(r);
        } else if (type == "NiPSysPlanarCollider") {
            SkipNiPSysPlanarCollider(r);
        } else if (type == "NiFlipController") {
            SkipNiFlipController(r);
        } else if (type == "NiPSysUpdateCtlr") {
            SkipNiPSysUpdateCtlr(r);
        } else if (type == "NiBoolInterpolator" || type == "NiBoolTimelineInterpolator") {
            // NiBoolTimelineInterpolator ist laut Referenz identisch zu NiBoolInterpolator
            // (keine eigenen Zusatzfelder) - unterscheidet sich nur im Laufzeitverhalten
            // (verpasste Keys werden nachgeholt), nicht in der Byte-Struktur.
            SkipNiBoolInterpolator(r);
        } else if (type == "NiLookAtInterpolator") {
            SkipNiLookAtInterpolator(r);
        } else if (type == "NiBoolData") {
            SkipNiBoolData(r);
        } else if (type == "NiColorData") {
            SkipNiColorData(r);
        } else if (type == "NiPSysAgeDeathModifier") {
            SkipNiPSysAgeDeathModifier(r);
        } else if (type == "NiPSysBoxEmitter") {
            SkipNiPSysBoxEmitter(r);
        } else if (type == "NiPSysMeshEmitter") {
            SkipNiPSysMeshEmitter(r);
        } else if (type == "NiPSysSpawnModifier") {
            SkipNiPSysSpawnModifier(r);
        } else if (type == "NiPSysGrowFadeModifier") {
            SkipNiPSysGrowFadeModifier(r);
        } else if (type == "NiPSysColorModifier") {
            SkipNiPSysColorModifier(r);
        } else if (type == "NiPSysRotationModifier") {
            SkipNiPSysRotationModifier(r);
        } else if (type == "NiPSysGravityModifier") {
            SkipNiPSysGravityModifier(r);
        } else if (type == "NiPSysDragModifier") {
            SkipNiPSysDragModifier(r);
        } else if (type == "NiPSysPositionModifier") {
            SkipNiPSysPositionModifier(r);
        } else if (type == "NiPSysBoundUpdateModifier") {
            SkipNiPSysBoundUpdateModifier(r);
        } else if (type == "NiPSysMeshUpdateModifier") {
            SkipNiPSysMeshUpdateModifier(r);
        } else if (type == "NiSkinInstance") {
            SkipNiSkinInstance(r);
        } else if (type == "NiSkinData") {
            SkipNiSkinData(r);
        } else if (type == "NiSkinPartition") {
            SkipNiSkinPartition(r);
        } else if (type == "NiTextureEffect") {
            SkipNiTextureEffect(r);
        } else if (type == "NiDirectionalLight" || type == "NiAmbientLight") {
            // NiAmbientLight ist laut Referenz (PyFFI) ebenfalls reine NiLight-Basis ohne
            // eigene Zusatzfelder, exakt wie NiDirectionalLight.
            SkipNiDirectionalLight(r);
        } else if (type == "NiPointLight") {
            SkipNiPointLight(r);
        } else if (type == "NiZBufferProperty") {
            SkipNiZBufferProperty(r);
            // Siehe SkipExtraBytesIfFollowedByTriData - hier bewusst weiterhin mit
            // Versions-Gate belassen (siehe Abschnitt 38: ein unbedingter Test verursachte
            // eine Regression), auch wenn sich das bei NiAlphaProperty als unnötig
            // herausstellte - nicht risikofrei verallgemeinern ohne erneuten Test.
            if (hdr.version != 0x14000004u) {
                SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
            }
        } else if (type == "NiVertexColorProperty") {
            SkipNiVertexColorProperty(r);
            SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
        } else if (type == "NiAlphaProperty") {
            SkipNiAlphaProperty(r);
            SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
        } else if (type == "NiStencilProperty") {
            SkipNiStencilProperty(r);
        } else if (type == "NiSpecularProperty") {
            SkipNiSpecularProperty(r);
            SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
        } else if (type == "NiPathInterpolator") {
            SkipNiPathInterpolator(r);
        } else if (type == "NiTriStrips" || type == "NiTriShape") {
            NiTriStripsBlock strips = ParseNiTriStripsHeader(r);
            currentMeshHasTexturing = false;
            for (const auto propRef : strips.base.properties) {
                if (propRef < 0 || static_cast<std::uint32_t>(propRef) >= hdr.blockTypeIndex.size()) continue;
                const std::string& propType = hdr.blockTypes[hdr.blockTypeIndex[static_cast<std::size_t>(propRef)]];
                if (!kSupportedPropertyTypes.contains(propType)) {
                    return std::unexpected("Nicht unterst\u00fctzter Property-Typ '" + propType +
                                            "' - nur untexturierte Meshes werden aktuell unterst\u00fctzt (siehe docs/MAP_FORMAT.md)");
                }
                if (propType == "NiTexturingProperty") {
                    currentMeshHasTexturing = true;
                }
            }
        } else if (type == "NiMaterialProperty") {
            NifMaterial mat = ParseNiMaterialProperty(r, currentMeshHasTexturing);
            model.parts.emplace_back();
            model.parts.back().material = mat;
        } else if (type == "NiTexturingProperty") {
            // Ältere NIF-Versionen (10.1.0.0/10.2.0.0, vor 10.4.0.1) haben laut autoritativer
            // Referenz zwei zusätzliche PS2-Felder je Textur-Slot - bei 20.0.0.4 entfallen.
            const bool hasPS2Fields = hdr.version != 0x14000004u;
            const std::int32_t baseSourceRef = ParseNiTexturingProperty(r, hasPS2Fields);
            if (baseSourceRef >= 0 && !model.parts.empty()) {
                // NiTexturingProperty folgt in allen gepr\u00fcften Dateien direkt auf
                // NiMaterialProperty (siehe docs/MAP_FORMAT.md) - der zuletzt angelegte
                // Mesh-Teil ist also der richtige Empf\u00e4nger dieser Textur-Referenz.
                partBaseTextureRef[model.parts.size() - 1] = baseSourceRef;
            }
        } else if (type == "NiSourceTexture") {
            sourceTextures[blockIdx] = ParseNiSourceTexture(r);
        } else if (type == "NiPixelData") {
            auto embedded = ParseNiPixelData(r, hdr.version != 0x14000004u);
            if (embedded) embeddedPixelTextures[blockIdx] = std::move(embedded);
            // Der 8-Byte-Abschluss von NiPixelData ist bereits in der 17-Byte-Präambel von
            // NiSourceTexture enthalten (siehe ParseNiSourceTexture). Folgt danach KEINE
            // weitere NiSourceTexture, muss er hier separat konsumiert werden - sonst
            // verschiebt sich alles Folgende um 8 Byte.
            const bool nextIsSourceTexture =
                (blockIdx + 1 < hdr.blockTypeIndex.size()) &&
                hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]] == "NiSourceTexture";
            if (!nextIsSourceTexture) {
                // KORREKTUR (Partikel-Glow-Texturen, unkomprimiertes 8-Bit-Format): bei
                // mindestens einer echten Datei (BH_Karen_water_effect.nif) sind es nur 4
                // Byte statt der sonst üblichen 8 - der Pixelformat-Header war dabei
                // byte-identisch zu einer Datei, die echte 8 Byte braucht
                // (BH_Karen_fire.nif), die tatsächliche Ursache also weiterhin ungeklärt.
                // Per Peek robust behandelt: probiert 8 Byte (Standardfall, in der großen
                // Mehrheit der Dateien bereits verifiziert), fällt auf 4 Byte zurück NUR
                // wenn dort ein plausibler leerer ObjectNetBase-Anfang (namelen=0,
                // numExtra=0, controller=-1) erkennbar ist, den es bei 8 Byte nicht gibt -
                // byte-exakt an BH_Karen_water_effect.nif verifiziert (trifft danach exakt
                // auf NiAlphaProperty UND die folgende NiVertexColorProperty).
                // VERSUCHT UND VERWORFEN: LooksLikeFreshName() statt der strengen 3-Felder-
                // Prüfung (namelen=0+numExtra=0+controller=-1) zu verwenden, um auch benannte
                // Objekte (z.B. "24 - Defaulst" bei BeraM_Wood3.nif) zu erkennen, verursachte
                // einen KATASTROPHALEN Rückschritt (1818 → 363!) und wurde sofort
                // zurückgenommen. LooksLikeFreshName prüft NUR das Namensfeld selbst, nicht
                // numExtra/controller - das reicht hier offenbar bei weitem nicht aus, um
                // zuverlässig zwischen den beiden Fällen zu unterscheiden (vermutlich, weil an
                // dieser Stelle sehr oft zufällig plausibel aussehende kurze "Namen" in
                // Float-lastigen Pixeldaten vorkommen). Die strenge Prüfung bleibt Standard;
                // der BeraM_Wood3.nif-Fall (benannte NiMaterialProperty nach NiPixelData ohne
                // folgende NiSourceTexture) bleibt ein ungelöster Einzelfall.
                const bool looksEmptyAt8 =
                    r.PeekU32(8) == 0 && r.PeekU32(12) == 0 && r.PeekU32(16) == 0xFFFFFFFFu;
                const bool looksEmptyAt4 =
                    r.PeekU32(4) == 0 && r.PeekU32(8) == 0 && r.PeekU32(12) == 0xFFFFFFFFu;
                // WEITERER FUND (nur für ältere NIF-Versionen, siehe docs/MAP_FORMAT.md
                // Abschnitt 35): folgt auf NiPixelData direkt eine NiTriStripsData/
                // NiTriShapeData (kein Namensfeld, beginnt direkt mit num_vertices als u16 +
                // keep_flags(u8) + compress_flags(u8)), versagen die obigen ObjectNetBase-
                // Prüfungen strukturell (sie erwarten ein Namensfeld, das es hier gar nicht
                // gibt). Byte-exakt an skeleton_monolith_blood.nif verifiziert: bei +4 Byte
                // ergibt sich num_vertices=231 (plausibel), keep_flags=0, compress_flags=0,
                // has_vertices=1 - bei +0 oder +8 Byte dagegen ausschließlich unplausible
                // Werte. Bewusst nur für ältere Versionen aktiv, um das Risiko auf den bereits
                // separat behandelten, kleineren Dateibestand zu begrenzen.
                bool looksLikeTriDataAt4 = false;
                if (hdr.version != 0x14000004u && blockIdx + 1 < hdr.blockTypeIndex.size()) {
                    const std::string& nextType2 = hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]];
                    if (nextType2 == "NiTriStripsData" || nextType2 == "NiTriShapeData") {
                        const std::uint32_t numVertsCandidate = r.PeekU32(4) & 0xFFFFu;
                        const std::uint8_t keepCandidate = r.PeekU8(6);
                        const std::uint8_t compressCandidate = r.PeekU8(7);
                        const std::uint8_t hasVertsCandidate = r.PeekU8(8);
                        if (numVertsCandidate >= 3 && numVertsCandidate <= 20000 &&
                            keepCandidate <= 1 && compressCandidate <= 1 && hasVertsCandidate <= 1) {
                            looksLikeTriDataAt4 = true;
                        }
                    }
                }
                // WEITERER FUND (siehe docs/MAP_FORMAT.md Abschnitt 37): manchmal ist GAR KEIN
                // Trailer nötig (0 statt 4 oder 8 Byte) - byte-exakt an
                // skeleton_monolith_blood.nif verifiziert: direkt an der aktuellen Position
                // (0 Byte Versatz) steht bereits eine vollständige, gültige leere
                // ObjectNetBase (namelen=0, numExtra=0, controller=-1, anschließend ein
                // plausibler flags-Wert=1 für die folgende NiZBufferProperty). Wird VOR den
                // 4-und-8-Byte-Fällen geprüft, da es das strengste, eindeutigste Signal ist.
                const bool looksEmptyAt0 =
                    r.PeekU32(0) == 0 && r.PeekU32(4) == 0 && r.PeekU32(8) == 0xFFFFFFFFu;
                // WEITERER FUND (siehe docs/MAP_FORMAT.md Abschnitt 45): anders als der in
                // Abschnitt 20 gescheiterte Versuch (LooksLikeFreshName allein, nur das
                // Namensfeld geprüft) wird hier die GESAMTE ObjectNetBase-Struktur validiert:
                // ein plausibler Namenslänge (1-40, druckbare Zeichen) GEFOLGT von einem
                // plausiblen numExtra (0-10) UND einem plausiblen controller (-1 oder
                // 0..300). Diese Kombination aus mehreren unabhängigen Bedingungen ist viel
                // seltener zufällig erfüllt als eine bloße Namensform allein. Byte-exakt an
                // Tree01.nif verifiziert: bei +4 Byte ergibt sich ein Name der Länge 9,
                // numExtra=0, controller=-1, flags=16, plausible Weltkoordinaten
                // (88.3, 0.68, 32.9) - ohne diese Prüfung wären es weiterhin absurde Werte.
                bool looksLikeNamedAt4 = false;
                {
                    const std::uint32_t nameLenCandidate = r.PeekU32(4);
                    if (nameLenCandidate >= 1 && nameLenCandidate <= 40) {
                        bool allPrintable = true;
                        for (std::uint32_t i = 0; i < nameLenCandidate; ++i) {
                            const std::uint8_t b = r.PeekU8(8 + i);
                            if (b < 0x20 || b > 0x7E) { allPrintable = false; break; }
                        }
                        if (allPrintable) {
                            const std::size_t afterName = 8 + nameLenCandidate;
                            const std::uint32_t numExtraCandidate = r.PeekU32(afterName);
                            if (numExtraCandidate <= 10) {
                                const std::int32_t controllerCandidate =
                                    static_cast<std::int32_t>(r.PeekU32(afterName + 4 + 4 * numExtraCandidate));
                                if (controllerCandidate == -1 ||
                                    (controllerCandidate >= 0 && controllerCandidate < 300)) {
                                    looksLikeNamedAt4 = true;
                                }
                            }
                        }
                    }
                }
                if (looksEmptyAt0) {
                    // Kein Skip nötig - die aktuelle Position ist bereits korrekt.
                } else if ((!looksEmptyAt8 && looksEmptyAt4) || looksLikeTriDataAt4 || looksLikeNamedAt4) {
                    r.Skip(4);
                } else {
                    r.I32(); r.U32();
                }
            }
        } else if (type == "NiTriStripsData") {
            // KORRIGIERT: Trailer fehlt auch, wenn eine NiSkinInstance folgt (geskinnte
            // Meshes) - diese beginnt NICHT mit einem Namensfeld, weshalb der
            // LooksLikeFreshName-Peek in ParseNiTriStripsData sie nicht selbst erkennen kann.
            // Byte-exakt verifiziert an Tunnel02_Wood3.nif: mit dieser Ausnahme trifft die
            // Position exakt auf die folgende NiSkinInstance (data_ref=12 zeigt exakt auf die
            // nächste NiSkinData, skin_partition=13 exakt auf die übernächste
            // NiSkinPartition, plausible Knochenzahl 14 statt zuvor 126).
            //
            // WEITERER, GENERALISIERTER FUND (siehe docs/MAP_FORMAT.md Abschnitt 42): das
            // "+4-Byte-Muster" aus Abschnitt 39/40 tritt nicht nur bei bestimmten,
            // einzeln aufgezählten Eigenschaftstypen auf, sondern bei IRGENDEINEM Blocktyp,
            // der direkt vor einer NiTriStripsData/NiTriShapeData steht (byte-exakt auch für
            // NiFloatData bestätigt: numVerts=0/hasVerts=15 vor der Korrektur,
            // numVerts=15/hasVerts=1 danach). Statt jeden möglichen Vorgänger-Typ einzeln
            // aufzuzählen (riskant, siehe die gemischten Ergebnisse in Abschnitt 40), wird die
            // Plausibilität jetzt HIER, direkt am Zielblock, geprüft: sieht die aktuelle
            // Position NICHT nach einem gültigen num_vertices/keep_flags/compress_flags/
            // has_vertices-Muster aus, aber 4 Byte weiter schon, wird genau dort weitergelesen.
            if (!LooksLikeTriDataHeader(r, 0) && LooksLikeTriDataHeader(r, 4)) {
                r.Skip(4);
            }
            const bool nextIsTriStrips =
                (blockIdx + 1 < hdr.blockTypeIndex.size()) &&
                (hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]] == "NiTriStrips" ||
                 hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]] == "NiTriShape" ||
                 hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]] == "NiSkinInstance");
            RawTriStripsData raw = ParseNiTriStripsData(r, !nextIsTriStrips, hdr.version != 0x14000004u);
            if (model.parts.empty()) {
                return std::unexpected("NiTriStripsData ohne vorangehende NiMaterialProperty - unerwartete Blockreihenfolge");
            }
            NifMeshPart& part = model.parts.back();
            part.positions = std::move(raw.vertices);
            part.normals = std::move(raw.normals);
            part.uvs = std::move(raw.uvs);
            for (const auto& strip : raw.strips) {
                ExpandTriangleStrip(strip, part.triangleIndices);
            }
        } else if (type == "NiTriShapeData") {
            // Gleiche Korrektur wie bei NiTriStripsData (siehe dort, inkl. Abschnitt 42).
            if (!LooksLikeTriDataHeader(r, 0) && LooksLikeTriDataHeader(r, 4)) {
                r.Skip(4);
            }
            const bool nextIsTriMesh =
                (blockIdx + 1 < hdr.blockTypeIndex.size()) &&
                (hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]] == "NiTriStrips" ||
                 hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]] == "NiTriShape" ||
                 hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]] == "NiSkinInstance");
            RawTriShapeData raw = ParseNiTriShapeData(r, !nextIsTriMesh, hdr.version != 0x14000004u);
            if (model.parts.empty()) {
                return std::unexpected("NiTriShapeData ohne vorangehende NiMaterialProperty - unerwartete Blockreihenfolge");
            }
            NifMeshPart& part = model.parts.back();
            part.positions = std::move(raw.vertices);
            part.normals = std::move(raw.normals);
            part.uvs = std::move(raw.uvs);
            part.triangleIndices.reserve(raw.triangleIndices.size());
            for (const auto idx : raw.triangleIndices) {
                part.triangleIndices.push_back(idx); // bereits flache Dreiecksliste, keine Streifen-Expansion nötig
            }
        } else {
            return std::unexpected("Nicht unterst\u00fctzter Block-Typ '" + type +
                                    "' bei Block " + std::to_string(blockIdx) +
                                    " - nur einfache untexturierte Meshes werden aktuell unterst\u00fctzt");
        }

        if (traceBlocks) {
            std::fprintf(stderr, "[NifTrace] end   block=%u type=%s start=%zu end=%zu ok=%d\n",
                         blockIdx, type.c_str(), blockStart, r.Pos(), r.Ok() ? 1 : 0);
        }
        if (!r.Ok()) {
            return std::unexpected("Unerwartetes Dateiende beim Parsen von Block " +
                                   std::to_string(blockIdx) + " (" + type + "), start=" +
                                   std::to_string(blockStart) + ", pos=" + std::to_string(r.Pos()) +
                                   ", fileSize=" + std::to_string(data.size()));
        }
    }

    // KORRIGIERT: Dateien ganz ohne Mesh-Geometrie (nur NiNode/NiCollisionData/Properties,
    // z.B. reine Kollisions-/Ankerpunkt-Objekte wie die "BN" = "Bounding Node"-Familie,
    // siehe bera_BN01.nif/bera_BNset.nif/beraBN.nif) sind strukturell vollständig gültige
    // NIF-Dateien - nur eben ohne sichtbare Geometrie. Bisher wurde das fälschlich als Fehler
    // behandelt. model.parts bleibt einfach leer; NifMeshRenderer iteriert bereits sicher
    // über eine leere parts-Liste (kein Sonderfall nötig).

    // Zweistufige Textur-Auflösung abschließen (siehe Kommentar bei der Deklaration oben).
    for (const auto& [partIdx, blockIdx] : partBaseTextureRef) {
        if (blockIdx < 0 || partIdx >= model.parts.size()) continue;
        auto it = sourceTextures.find(static_cast<std::uint32_t>(blockIdx));
        if (it == sourceTextures.end()) continue;
        model.parts[partIdx].diffuseTexture = it->second.filename;
        if (it->second.useExternal == 0 && it->second.pixelDataRef >= 0) {
            auto pix = embeddedPixelTextures.find(static_cast<std::uint32_t>(it->second.pixelDataRef));
            if (pix != embeddedPixelTextures.end()) model.parts[partIdx].embeddedDiffuseTexture = pix->second;
            else std::fprintf(stderr, "[NifModel] NiSourceTexture '%s' verweist auf fehlendes NiPixelData %d\n",
                              it->second.filename.c_str(), it->second.pixelDataRef);
        }
    }

    return model;
}

} // namespace theseed::mapeditor::core
