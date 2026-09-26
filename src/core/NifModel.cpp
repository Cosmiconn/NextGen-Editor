#include "mapeditor/core/NifModel.hpp"
#include "mapeditor/core/DdsImage.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <cmath>
#include <cstring>
#include <cstdio>
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
// Bei der Variantensuche wird nur die STRUKTUR geprueft - das aufwendige Dekodieren eingebetteter
// Texturen entfaellt (der erfolgreiche Lauf wird danach noch einmal vollstaendig ausgefuehrt).
thread_local bool g_probeNoDecode = false;

class ByteReader {
public:
    explicit ByteReader(const std::vector<std::uint8_t>& data, bool legacyLayout = false)
        : data_(data), legacyLayout_(legacyLayout) {}
    bool LegacyLayout() const { return legacyLayout_; }
    std::uint32_t Version() const { return version_; }
    void SetVersion(std::uint32_t version) { version_ = version; }

    [[nodiscard]] bool Ok() const noexcept { return ok_; }
    [[nodiscard]] std::size_t Pos() const noexcept { return pos_; }
    void SetPos(std::size_t p) { pos_ = p; }
    [[nodiscard]] std::size_t Remaining() const noexcept { return pos_ <= data_.size() ? data_.size() - pos_ : 0; }
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

    std::span<const std::uint8_t> BytesView(std::size_t n) {
        if (!ok_ || n > Remaining()) { ok_ = false; return {}; }
        auto out = std::span<const std::uint8_t>(data_).subspan(pos_, n);
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
    bool legacyLayout_ = false;
    std::uint32_t version_ = 0x14000004u;
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
    r.SetVersion(hdr.version);
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
    if (r.LegacyLayout() && hdr.version == 0x0a010000u) {
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
    if (!r.LegacyLayout()) {
        const auto count = r.CountU32();
        r.Skip(static_cast<std::size_t>(count) * 4u);
        base.controller = r.I32();
        return base;
    }
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
    std::vector<std::int32_t> effects;
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
    node.effects.reserve(numEffects);
    for (std::uint32_t i = 0; i < numEffects; ++i) {
        node.effects.push_back(r.I32());
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

struct NifZBufferState {
    bool test = true;
    bool write = true;
    std::uint32_t function = 3; // ZCOMP_LESS_EQUAL
};

NifZBufferState ParseNiZBufferProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    const std::uint16_t flags = r.U16();
    const std::uint32_t function = r.U32();
    return NifZBufferState{
        (flags & 0x0001u) != 0,
        (flags & 0x0002u) != 0,
        function
    };
}

struct NifVertexColorState {
    std::uint16_t flags = 0;
    std::uint32_t vertexMode = 2;    // SRC_AMB_DIF
    std::uint32_t lightingMode = 1;  // EMI_AMB_DIF
};

NifVertexColorState ParseNiVertexColorProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    NifVertexColorState out;
    // Fiesta's supported NIF layouts store the 10-byte NiVertexColorProperty payload
    // explicitly as flags(u16), vertex_mode(u32), lighting_mode(u32). This is the same
    // layout consumed by the previous skip path; only the already-read semantics are kept.
    out.flags = r.U16();
    out.vertexMode = r.U32();
    out.lightingMode = r.U32();
    return out;
}

// NiAlphaProperty: ObjectNetBase (bei allen geprüften Dateien leer -> 8 Byte) + flags(u16) +
// threshold(u8) + 4 weitere Byte = 15 Byte gesamt. Länge empirisch bestimmt durch Subtraktion
// der bekannten Nachbarblock-Längen zwischen zwei String-Landmarken (siehe docs/MAP_FORMAT.md).
// Häufigster Blocker im Massentest (1989 von 3436 echten Dateien enthalten diesen Typ).
struct NifAlphaState {
    bool blend = false;
    bool test = false;
    std::uint8_t threshold = 0;
    std::uint8_t srcBlend = 6;
    std::uint8_t dstBlend = 7;
    std::uint8_t testFunc = 4;
};

NifAlphaState ParseNiAlphaProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    const std::uint16_t flags = r.U16();
    const std::uint8_t threshold = r.U8();
    // NIF alpha flags: bit 0 = blending, bit 9 = alpha test. The remaining
    // bits encode source/destination blend and test functions. We preserve
    // the two visibility-critical switches here; blend functions can be
    // expanded later without changing the file parser.
    return NifAlphaState{
        (flags & 0x0001u) != 0,
        (flags & 0x0200u) != 0,
        threshold,
        static_cast<std::uint8_t>((flags >> 1u) & 0x0Fu),
        static_cast<std::uint8_t>((flags >> 5u) & 0x0Fu),
        static_cast<std::uint8_t>((flags >> 10u) & 0x07u)
    };
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
struct NifStencilState {
    bool enabled = false;
    std::uint32_t function = 0;
    std::uint32_t reference = 0;
    std::uint32_t mask = 0xFFFFFFFFu;
    std::uint32_t failAction = 0;
    std::uint32_t zFailAction = 0;
    std::uint32_t passAction = 0;
    std::uint32_t drawMode = 3;
};

NifStencilState ParseNiStencilProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    NifStencilState out;
    // NIF/Gamebryo layout: enabled byte followed by the seven uint32 fields.
    // The old skip routine consumed the same byte count but in the wrong semantic order.
    out.enabled = r.U8() != 0;
    out.function = r.U32();
    out.reference = r.U32();
    out.mask = r.U32();
    out.failAction = r.U32();
    out.zFailAction = r.U32();
    out.passAction = r.U32();
    out.drawMode = r.U32();
    return out;
}

// NiSpecularProperty: laut Referenzimplementierung denkbar einfach - NiObjectNET-Basis +
// EIN einzelnes flags(u16)-Feld. Deutlich kürzer als der zuvor bei NiStencilProperty
// vermutete "8-unbekannte-Felder"-Aufbau.
bool ParseNiSpecularProperty(ByteReader& r) {
    ParseObjectNetBase(r);
    return r.U16() != 0; // NifSkope: flags==0 disables material specular
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
    if (r.LegacyLayout() && r.PeekU32(0) == 0xFFFFFFFFu) {
        r.I32(); // seltenes führendes Ketten-/Controller-Feld, siehe oben
    }
    r.SizedString(); // name
}

// NiPalette: KEINE NiObjectNET-Basis (reines NiObject, kein Name/ExtraData/Controller!) -
// has_alpha(u8) + palette(ByteColor4-Liste, IMMER exakt 256 Einträge = 1024 Byte). KORRIGIERT
// (diese Sitzung): die autoritative nif.xml listet zusätzlich ein "Num Entries"(u32)-Feld
// zwischen has_alpha und der Palette - dieser Fork hat es NICHT. Byte-exakt an 2 unabhängigen
// Dateien verifiziert (filddoll.nif, Sign01.nif, unterschiedliche Größe/Inhalt): in BEIDEN
// endet eine lange Nullen-Sequenz exakt bei has_alpha+1024 Byte, danach beginnt sofort ein
// neues, plausibles Blockmuster. Die von der Referenz vorgesehene "kann auch 16 Einträge
// sein"-Variante wurde an diesen 2 Belegen nicht beobachtet - falls künftig eine Datei mit
// erkennbar anderer Struktur auftaucht, bräuchte es einen neuen Diskriminator.
struct NifPaletteState {
    bool hasAlpha = false;
    std::array<std::uint8_t, 256u * 4u> rgba{};
};

NifPaletteState ParseNiPalette(ByteReader& r) {
    NifPaletteState out;
    out.hasAlpha = r.U8() != 0;
    const auto count = r.LegacyLayout() ? 256u : r.CountU32(256);
    for (std::size_t i = 0; i < count; ++i) {
        out.rgba[i * 4 + 0] = r.U8();
        out.rgba[i * 4 + 1] = r.U8();
        out.rgba[i * 4 + 2] = r.U8();
        out.rgba[i * 4 + 3] = r.U8();
        if (!out.hasAlpha) out.rgba[i * 4 + 3] = 255;
    }
    return out;
}

void SkipNiPalette(ByteReader& r) {
    (void)ParseNiPalette(r);
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

// Skinning data used by NiSkinInstance/NiSkinData/NiSkinPartition.  The editor has no
// animation timeline yet, but still evaluates the complete weighted bind-pose deformation so
// skinned map props are rendered from their skeleton instead of being treated as rigid meshes.
struct SkinTransform {
    std::array<float, 9> rotation{1.0f, 0.0f, 0.0f,
                                  0.0f, 1.0f, 0.0f,
                                  0.0f, 0.0f, 1.0f};
    NifVec3 translation{};
    float scale = 1.0f;
};

SkinTransform ParseSkinTransform(ByteReader& r) {
    SkinTransform t;
    for (float& v : t.rotation) v = r.F32();
    t.translation = {r.F32(), r.F32(), r.F32()};
    t.scale = r.F32();
    return t;
}

struct SkinVertexWeight {
    std::uint16_t vertex = 0;
    float weight = 0.0f;
};

struct SkinBoneData {
    SkinTransform transform;
    NifVec3 boundCenter{};
    float boundRadius = 0.0f;
    std::vector<SkinVertexWeight> weights;
};

struct SkinDataBlock {
    SkinTransform skinTransform;
    bool hasVertexWeights = false;
    std::vector<SkinBoneData> bones;
};

struct SkinInstanceBlock {
    std::int32_t dataRef = -1;
    std::int32_t partitionRef = -1;
    std::int32_t skeletonRoot = -1;
    std::vector<std::int32_t> bones;
};

struct SkinPartitionEntry {
    std::uint16_t numWeightsPerVertex = 0;
    std::vector<std::uint16_t> boneMap;
    std::vector<std::uint16_t> vertexMap;
    std::vector<float> weights;
    std::vector<std::uint8_t> boneIndices;
};

struct SkinPartitionBlock {
    std::vector<SkinPartitionEntry> partitions;
};

// NiSkinInstance: data_ref(i32) + skin_partition(i32) + skeleton_root(i32) + num_bones(u32) +
// one Ref(i32) per bone.  The block is linked from NiTriShape/NiTriStrips, so parsing this
// reference is what lets the geometry be matched back to its skeleton after all blocks exist.
SkinInstanceBlock ParseNiSkinInstance(ByteReader& r) {
    SkinInstanceBlock out;
    out.dataRef = r.I32();
    out.partitionRef = r.I32();
    out.skeletonRoot = r.I32();
    const std::uint32_t numBones = r.CountU32(256u);
    out.bones.reserve(numBones);
    for (std::uint32_t i = 0; i < numBones; ++i) out.bones.push_back(r.I32());
    return out;
}

// NiSkinData: skin transform + bone bind transforms/bounds and, when present, the sparse
// per-bone vertex-weight lists.  Partitioned meshes still need the bone transforms from this
// block even when their actual weights live in NiSkinPartition.
SkinDataBlock ParseNiSkinData(ByteReader& r) {
    SkinDataBlock out;
    out.skinTransform = ParseSkinTransform(r);
    const std::uint32_t numBones = r.CountU32(256u);
    out.hasVertexWeights = r.U8() != 0;
    out.bones.reserve(numBones);
    for (std::uint32_t b = 0; b < numBones; ++b) {
        SkinBoneData bone;
        bone.transform = ParseSkinTransform(r);
        bone.boundCenter = {r.F32(), r.F32(), r.F32()};
        bone.boundRadius = r.F32();
        const std::uint32_t numVerts = r.CountU16(65535u);
        if (out.hasVertexWeights) {
            bone.weights.reserve(numVerts);
            for (std::uint32_t v = 0; v < numVerts; ++v) {
                SkinVertexWeight w;
                w.vertex = r.U16();
                w.weight = r.F32();
                bone.weights.push_back(w);
            }
        }
        out.bones.push_back(std::move(bone));
    }
    return out;
}

// NiSkinPartition: hardware-skinning partitions.  The partition-local bone indices refer into
// boneMap, while vertexMap maps partition vertices back to the original geometry.  Faces are
// consumed as before; weighted deformation only needs maps, weights and bone indices.
SkinPartitionBlock ParseNiSkinPartition(ByteReader& r) {
    SkinPartitionBlock out;
    const std::uint32_t numPartitions = r.CountU32(256u);
    out.partitions.reserve(numPartitions);
    for (std::uint32_t p = 0; p < numPartitions; ++p) {
        const std::uint32_t numVerts = r.CountU16(65535u);
        const std::uint32_t numTriangles = r.CountU16(65535u);
        const std::uint32_t numBones = r.CountU16(2000u);
        const std::uint32_t numStrips = r.CountU16(65535u);
        const std::uint32_t numWeightsPerVertex = r.CountU16(16u);

        SkinPartitionEntry part;
        part.numWeightsPerVertex = static_cast<std::uint16_t>(numWeightsPerVertex);
        part.boneMap.reserve(numBones);
        for (std::uint32_t i = 0; i < numBones; ++i) part.boneMap.push_back(r.U16());

        const bool hasVertexMap = r.U8() != 0;
        if (hasVertexMap) {
            part.vertexMap.reserve(numVerts);
            for (std::uint32_t i = 0; i < numVerts; ++i) part.vertexMap.push_back(r.U16());
        } else {
            part.vertexMap.reserve(numVerts);
            for (std::uint32_t i = 0; i < numVerts; ++i) part.vertexMap.push_back(static_cast<std::uint16_t>(i));
        }

        const bool hasVertexWeights = r.U8() != 0;
        if (hasVertexWeights) {
            const std::size_t count = static_cast<std::size_t>(numVerts) * numWeightsPerVertex;
            part.weights.reserve(count);
            for (std::size_t i = 0; i < count; ++i) part.weights.push_back(r.F32());
        }

        std::uint32_t stripLengthSum = 0;
        for (std::uint32_t s = 0; s < numStrips; ++s) stripLengthSum += r.CountU16(65535u);
        const bool hasFaces = r.U8() != 0;
        if (hasFaces && numStrips != 0) {
            r.Skip(static_cast<std::size_t>(stripLengthSum) * 2u);
        } else if (hasFaces) {
            r.Skip(static_cast<std::size_t>(numTriangles) * 6u);
        }

        const bool hasBoneIndices = r.U8() != 0;
        if (hasBoneIndices) {
            const std::size_t count = static_cast<std::size_t>(numVerts) * numWeightsPerVertex;
            part.boneIndices.reserve(count);
            for (std::size_t i = 0; i < count; ++i) part.boneIndices.push_back(r.U8());
        }
        out.partitions.push_back(std::move(part));
    }
    return out;
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
std::uint32_t SkipNiGeometryDataHeader(ByteReader& r, std::uint32_t version) {
    // NiGeometryData::unknownInt precedes numVertices since 10.2.0.0.
    // The supported versions are restricted in ParseHeader (no Bethesda layouts).
    if (version >= 0x0A020000u) r.U32();
    const std::uint32_t numVerts = r.CountU16(65535u);
    r.U8(); r.U8(); // keep_flags, compress_flags
    const std::uint8_t hasVerts = r.U8();
    if (hasVerts) r.Skip(static_cast<std::size_t>(numVerts) * 12u);
    const std::uint16_t dataFlags = r.CountU16(0xFFFFu);
    const std::uint32_t numUvSets = dataFlags & 0x3Fu;
    const bool hasTangentSpace = (dataFlags & 0xF000u) != 0; // KORRIGIERT [0.44.35]: alle 4 oberen Bit von tspace_flag (0xF0), nicht nur Bit 4 (0x10) - siehe SkipNiGeometryDataHeader
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
    r.U16(); // consistency_flags
    if (version >= 0x14000004u) r.I32(); // additional_data ref
    return numVerts;
}

// NiParticlesData : NiGeometryData + has_radii+Radii + num_active(u16) + has_sizes+Sizes +
// has_rotations+Rotations(Quaternion je 16 Byte) + has_rotation_angles+Angles +
// has_rotation_axes+Achsen(Vector3 je 12 Byte).
std::uint32_t SkipNiParticlesData(ByteReader& r, std::uint32_t version) {
    const std::uint32_t numVerts = SkipNiGeometryDataHeader(r, version);
    if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 4u); // has_radii
    r.U16(); // num_active
    if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 4u); // has_sizes
    if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 16u); // has_rotations (Quaternion)
    if (version >= 0x14000004u) {
        if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 4u); // rotation_angles
        if (r.U8()) r.Skip(static_cast<std::size_t>(numVerts) * 12u); // rotation_axes
    }
    // NiRotatingParticlesData adds fields only through 4.2.2.0, outside our versions.
    return numVerts;
}

// NiPSysData : NiParticlesData + je Vertex ein NiParticleInfo (Velocity-Vector3(12) +
// age/life_span/last_update(je f32=4) + spawn_generation/code(je u16=2) = 28 Byte) +
// has_unknown_floats+Floats + 2 abschließende u16-Felder.
// NiMeshPSysData has a counted uint array, not a fixed trailer (Niflib).
void SkipNiPSysData(ByteReader& r, std::uint32_t version, bool isMeshVariant = false) {
    const std::uint32_t numVerts = SkipNiParticlesData(r, version);
    // ParticleDesc: Vector3 + [3 legacy floats] + 3 floats + uint.
    const std::size_t particleBytes = version <= 0x0A040001u ? 40u : 28u;
    r.Skip(static_cast<std::size_t>(numVerts) * particleBytes);
    if (version >= 0x14000004u && r.U8())
        r.Skip(static_cast<std::size_t>(numVerts) * 4u); // unknown_floats3
    r.U16(); r.U16(); // unknown_short_1, unknown_short_2
    if (isMeshVariant) {
        if (version >= 0x0A020000u) {
            r.U32(); // unknownInt2
            r.U8();  // unknownByte3
            const auto count = r.CountU32(); // numUnknownInts1
            r.Skip(static_cast<std::size_t>(count) * 4u); // unknownInts1
        }
        r.I32(); // particleMeshes link
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
    r.F32(); // initial radius
    if (r.LegacyLayout() || r.Version() >= 0x0A040001u) r.F32(); // radius variation
    r.F32(); r.F32(); // lifespan, lifespan variation
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

void SkipNiPSysCylinderEmitter(ByteReader& r) {
    SkipNiPSysVolumeEmitterBase(r);
    r.F32(); // radius
    r.F32(); // height
}

void SkipNiPSysSphereEmitter(ByteReader& r) {
    SkipNiPSysVolumeEmitterBase(r);
    r.F32(); // radius
}

void SkipNiPSysBombModifier(ByteReader& r) {
    SkipNiPSysModifierBase(r);
    r.I32(); // bomb object
    r.F32(); r.F32(); r.F32(); // bomb axis
    r.F32(); // decay
    r.F32(); // deltaV
    r.U32(); // decay type
    r.U32(); // symmetry type
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
    r.F32(); // rotation speed
    if (r.LegacyLayout() || r.Version() >= 0x14000002u) {
        r.F32(); r.F32(); r.F32(); // speed variation, angle, angle variation
        r.U8(); // random speed sign
    }
    r.U8(); // random axis
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
struct NifDynamicEffectState {
    AVObjectBase base;
    bool switchState = true;
    std::vector<std::int32_t> affectedNodes;
};

NifDynamicEffectState ParseNiDynamicEffectBase(ByteReader& r) {
    NifDynamicEffectState out;
    out.base = ParseAVObjectBase(r);
    if (r.LegacyLayout() || r.Version() >= 0x0A01006Au) out.switchState = r.U8() != 0;
    const std::uint32_t numAffected = r.CountU32(256u);
    out.affectedNodes.reserve(numAffected);
    for (std::uint32_t i = 0; i < numAffected; ++i) out.affectedNodes.push_back(r.I32());
    return out;
}

void SkipNiDynamicEffectBase(ByteReader& r) {
    (void)ParseNiDynamicEffectBase(r);
}

// NiTextureEffect: NiDynamicEffect + model_projection_matrix(Matrix33=9 Floats) +
// model_projection_translation(Vector3) + texture_filtering(u32) + texture_clamping(u32) +
// texture_type(u32) + coordinate_generation_type(u32) + source_texture_ref(i32) +
// enable_plane(u8) + plane(NiPlane: normal(Vector3)+constant(f32) = 16 Byte).
struct NifTextureEffectState {
    NifDynamicEffectState dynamic;
    std::array<float, 9> projectionRotation{};
    NifVec3 projectionPosition{};
    std::uint32_t filterMode = 0;
    std::uint32_t clampMode = 0;
    std::uint32_t textureType = 0;
    std::uint32_t coordGenType = 0;
    std::int32_t sourceTextureRef = -1;
    bool enablePlane = false;
    std::array<float, 4> clipPlane{};
};

NifTextureEffectState ParseNiTextureEffect(ByteReader& r) {
    NifTextureEffectState out;
    out.dynamic = ParseNiDynamicEffectBase(r);
    for (float& v : out.projectionRotation) v = r.F32();
    out.projectionPosition = {r.F32(), r.F32(), r.F32()};
    out.filterMode = r.U32();
    out.clampMode = r.U32();
    out.textureType = r.U32();
    out.coordGenType = r.U32();
    out.sourceTextureRef = r.I32();
    out.enablePlane = r.U8() != 0;
    for (float& v : out.clipPlane) v = r.F32();
    if (!r.LegacyLayout() && r.Version() <= 0x0A020000u) { r.I16(); r.I16(); } // PS2 L/K
    return out;
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
            if (!r.LegacyLayout() && r.Version() <= 0x0A010000u) r.F32(); // legacy Euler order
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
struct NifSingleControllerState {
    std::int32_t nextRef = -1;
    std::uint16_t flags = 0;
    float frequency = 1.0f;
    float phase = 0.0f;
    float startTime = 0.0f;
    float stopTime = 0.0f;
    std::int32_t targetRef = -1;
    std::int32_t interpolatorRef = -1;
};

NifSingleControllerState ParseNiSingleController(ByteReader& r) {
    NifSingleControllerState c;
    c.nextRef = r.I32();
    c.flags = r.U16();
    c.frequency = r.F32();
    c.phase = r.F32();
    c.startTime = r.F32();
    c.stopTime = r.F32();
    c.targetRef = r.I32();
    c.interpolatorRef = r.I32();
    return c;
}

void SkipNiTransformController(ByteReader& r) {
    (void)ParseNiSingleController(r);
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

// NiBoneLODController (Charakter-NIFs in reschar/): 26-Byte-NiTimeController-Basis + LOD(u32) +
// Num LODs(u32) + Num Node Groups(u32) + je LOD ein NodeSet (Num Nodes u32 + Refs) + Num Shape Groups
// (u32) + je Gruppe (Num Skin Info u32 + je 8 Byte: NiSkinData-Ref + NiSkinInstance-Ref) + Num Shape
// Groups 2 (u32) + Refs (nif.xml, seit 4.2.2.0).
void SkipNiBoneLODController(ByteReader& r) {
    r.Skip(26);
    r.U32();                                   // LOD
    const std::uint32_t numLods = r.CountU32(64u);
    r.U32();                                   // Num Node Groups
    for (std::uint32_t i = 0; i < numLods; ++i) {
        const std::uint32_t nodes = r.CountU32(2000u);
        r.Skip(static_cast<std::size_t>(nodes) * 4u);
    }
    const std::uint32_t shapeGroups = r.CountU32(64u);
    for (std::uint32_t i = 0; i < shapeGroups; ++i) {
        const std::uint32_t infos = r.CountU32(2000u);
        r.Skip(static_cast<std::size_t>(infos) * 8u);
    }
    const std::uint32_t shapeGroups2 = r.CountU32(2000u);
    r.Skip(static_cast<std::size_t>(shapeGroups2) * 4u);
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

void SkipNiControllerManager(ByteReader& r) {
    SkipNiPSysUpdateCtlr(r); // NiTimeController
    r.U8(); // cumulative
    const auto count = r.CountU32();
    r.Skip(static_cast<std::size_t>(count) * 4); // sequence references
    r.I32(); // object palette
}

void SkipNiControllerSequence(ByteReader& r) {
    r.SizedString(); // sequence name
    if (r.Version() < 0x0A020000u) { r.Invalidate(); return; }
    const auto count = r.CountU32();
    r.U32(); // array grow by
    for (std::uint32_t i = 0; i < count; ++i) {
        r.I32(); r.I32(); // interpolator and controller
        r.I32(); // string palette
        for (int j = 0; j < 5; ++j) r.U32(); // string offsets
    }
    r.F32(); // weight
    r.I32(); // text keys
    r.U32(); // cycle type
    r.F32(); // frequency
    if (r.Version() <= 0x0A040001u) r.F32(); // phase
    r.F32(); r.F32(); // start, stop
    r.I32(); // manager
    r.SizedString(); // accumulation root
    r.I32(); // string palette
}

void SkipNiBlendInterpolator(ByteReader& r) {
    // Versions currently encountered with these blocks are 10.2 and 20.0.
    if (r.Version() < 0x0A020000u) { r.Invalidate(); return; }
    const auto flags = r.U8();
    const auto size = r.U8();
    r.F32(); // weight threshold
    if ((flags & 1u) == 0) {
        r.U8(); r.U8(); r.U8(); r.U8(); // count, single index, two priorities
        for (int i = 0; i < 4; ++i) r.F32(); // time and weight sums/ease spinner
        for (unsigned i = 0; i < size; ++i) {
            r.I32(); r.F32(); r.F32(); r.U8(); r.F32(); // InterpBlendItem
        }
    }
}

void ParseFiestaAccumulationState(ByteReader& r) {
    // Fiesta 20.0.0.4 extension, field layout established from EglackMad, Helga
    // and M_MajesticLion. Two groups of two 8-float transform states plus a
    // 3x3 basis, followed by one final 8-float state. The -FLT_MAX sentinels
    // denote unset components. Playback semantics are not inferred from them.
    if (r.Version() != 0x14000004u) { r.Invalidate(); return; }
    SkipNiBlendInterpolator(r);
    const auto readFloats = [&](auto& values) {
        for (auto& value : values) {
            value = r.F32();
            if (!std::isfinite(value)) r.Invalidate();
        }
    };
    for (int group = 0; group < 2; ++group) {
        std::array<float, 8> first{}, second{};
        std::array<float, 9> basis{};
        readFloats(first); readFloats(second); readFloats(basis);
    }
    std::array<float, 8> finalState{};
    readFloats(finalState);
}

void ParseFiestaShaderReference(ByteReader& r) {
    // This is a named shader reference, not NiObjectNET. Validate the on-disk
    // vendor/version signature; do not search for a plausible next block.
    if (r.Version() != 0x14000004u || r.SizedString() != "NPTR_IS" ||
        r.SizedString() != "PTSEV2" || r.U8() > 1) r.Invalidate();
}

void ParseFiestaToonExtraData(ByteReader& r) {
    // Fiesta-specific NiExtraData payload. Names can include a terminating NUL.
    // Keep field order explicit; the u32 is opaque vendor state (varies among
    // otherwise identical hat files), not a block link or an array length.
    if (r.Version() != 0x14000004u) { r.Invalidate(); return; }
    r.SizedString();
    for (int i = 0; i < 4; ++i) r.F32();
    r.U32();
    r.F32();
    if (r.U8() > 1 || r.U8() > 1) r.Invalidate();
    r.F32(); r.F32();
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
struct NifFlipControllerState {
    NifSingleControllerState base;
    std::uint32_t textureSlot = 0;
    std::vector<std::int32_t> sourceRefs;
};

NifFlipControllerState ParseNiFlipController(ByteReader& r) {
    NifFlipControllerState out;
    out.base = ParseNiSingleController(r);
    out.textureSlot = r.U32();
    const std::uint32_t numSources = r.CountU32(256u);
    out.sourceRefs.reserve(numSources);
    for (std::uint32_t i = 0; i < numSources; ++i) out.sourceRefs.push_back(r.I32());
    return out;
}

void SkipNiFlipController(ByteReader& r) {
    (void)ParseNiFlipController(r);
}

struct NifTextureTransformControllerState {
    NifSingleControllerState base;
    std::uint8_t shaderMap = 0;
    std::uint32_t textureSlot = 0;
    std::uint32_t operation = 0;
};

NifTextureTransformControllerState ParseNiTextureTransformController(ByteReader& r) {
    NifTextureTransformControllerState out;
    out.base = ParseNiSingleController(r);
    out.shaderMap = r.U8();
    out.textureSlot = r.U32();
    out.operation = r.U32();
    return out;
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
struct NifFloatInterpolatorState {
    float value = 0.0f;
    std::int32_t dataRef = -1;
};

NifFloatInterpolatorState ParseNiFloatInterpolator(ByteReader& r) {
    return {r.F32(), r.I32()};
}

void SkipNiFloatInterpolator(ByteReader& r) {
    (void)ParseNiFloatInterpolator(r);
}

struct NifFloatDataState {
    std::uint32_t interpolation = 1;
    std::vector<NifFloatKey> keys;
};

NifFloatDataState ParseNiFloatData(ByteReader& r) {
    NifFloatDataState out;
    const std::uint32_t numKeys = r.CountU32(200000u);
    if (numKeys == 0) return out;
    out.interpolation = r.U32();
    if (out.interpolation != 1u && out.interpolation != 2u &&
        out.interpolation != 3u && out.interpolation != 5u) {
        r.Invalidate();
        return out;
    }
    out.keys.reserve(numKeys);
    for (std::uint32_t i = 0; i < numKeys; ++i) {
        NifFloatKey k;
        k.time = r.F32();
        k.value = r.F32();
        if (out.interpolation == 2u) {
            k.forwardTangent = r.F32();
            k.backwardTangent = r.F32();
        } else if (out.interpolation == 3u) {
            // NifSkope rendert TBC-Floatspuren derzeit linear; die drei TBC-Parameter
            // muessen fuer Byte-Exaktheit trotzdem konsumiert werden.
            r.F32(); r.F32(); r.F32();
        }
        out.keys.push_back(k);
    }
    return out;
}

// NiFloatData: eine einzelne KeyGroup<float> (Skalar-Keyframes, z.B. Alpha- oder
// Textur-Transform-Wert über die Zeit). Byte-exakt verifiziert an
// AdlFH_field_burn_ground.nif: 3 QUADRATIC-Keys (Alpha oszilliert 0.5→1.0→0.5 über 3.33s,
// ein flackernder Brand-Boden-Effekt) - die berechnete Länge landet exakt auf dem
// Namensfeld-Beginn der folgenden NiVertexColorProperty (volle ObjectNetBase, leerer Name).
void SkipNiFloatData(ByteReader& r) {
    (void)ParseNiFloatData(r);
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
    std::int32_t skinInstanceRef = -1;
    std::string shaderName;
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
    block.skinInstanceRef = r.I32();
    const std::uint8_t hasShader = r.U8();
    if (!r.LegacyLayout()) {
        if (hasShader) { block.shaderName = r.SizedString(); r.I32(); }
        return block;
    }
    if (hasShader == 1) {
        // Geskinnte Charakter-Meshes ("FxSkinningBaseMap" in reschar/): Shader-Name (SizedString) +
        // "Shader Extra Data" (i32, -1) laut nif.xml (CHANGELOG [0.44.30]).
        // Bei LEEREM Namen (z.B. Cypian/Bark02.nif, Teva/Pillar_B.nif) fehlt das Extra-Data-Feld.
        block.shaderName = r.SizedString();
        if (!block.shaderName.empty()) r.I32();
    } else {
        // has_shader=0: es folgt in vielen Dateien ein ECHTER String (Laenge > 0, z.B. Gruppe der
        // AdlF-Tore) und in den meisten der leere String (Laenge 0). Steht dort aber 0xFFFFFFFF
        // (z.B. Urg_AlruinTW.nif: "Active Material" = -1), ist es KEINE Stringlaenge, sondern ein
        // i32 - nur dieser Fall wird gesondert gelesen (CHANGELOG [0.44.31]). (Ein Versuch, die 4 Byte
        // IMMER als i32 zu lesen, verlor 638 vorher ladbare Dateien - zurueckgenommen.)
        if (r.PeekU32(0) == 0xFFFFFFFFu) r.I32();
        else r.SizedString();
    }
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

NifMaterial ParseNiMaterialProperty(ByteReader& r, bool fifteenFloats) {
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
    if (r.LegacyLayout() && fifteenFloats) {
        r.F32();
    }
    return mat;
}

struct RawTriStripsData {
    std::vector<NifVec3> vertices;
    std::vector<NifVec3> normals;
    std::vector<NifColor4> vertexColors;
    std::vector<NifVec2> uvs;
    std::vector<std::vector<NifVec2>> uvSets;
    std::vector<NifUvSetDiagnostic> uvSetDiagnostics;
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
struct NifTextureSlotState {
    bool present = false;
    std::int32_t sourceRef = -1;
    std::uint32_t clampMode = 3;
    std::uint32_t filterMode = 2;
    std::uint32_t uvSet = 0;
    bool hasTransform = false;
    NifVec2 translation{};
    NifVec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
    std::uint32_t transformType = 0;
    NifVec2 center{0.5f, 0.5f};
};

struct NifTextureState {
    std::int32_t controllerRef = -1;
    std::uint32_t applyMode = 2; // APPLY_MODULATE
    std::array<NifTextureSlotState, 10> slots{};
    std::vector<std::pair<std::uint32_t, NifTextureSlotState>> shaderSlots;
    float bumpMapLumaScale = 1.0f;
    float bumpMapLumaOffset = 0.0f;
    std::array<float, 4> bumpMapMatrix{1.0f, 0.0f, 0.0f, 1.0f};
};

NifTextureState ParseNiTexturingProperty(ByteReader& r, bool hasPS2Fields) {
    // Die ObjectNetBase-Heuristik ist bei manchen Fiesta-NIFs mehrdeutig. Wie im bisherigen
    // Parser wird nur bei einem eindeutig unplausiblen TextureCount auf die alternative
    // Interpretation (Name + Controller ohne Extra-Data-Count) zurueckgefallen.
    const std::size_t startPos = r.Pos();
    const ObjectNetBase base = ParseObjectNetBase(r);
    std::int32_t controllerRef = base.controller;

    NifTextureState state;
    state.controllerRef = controllerRef;
    state.applyMode = r.U32();
    std::uint32_t textureCount = r.CountU32(64);
    if (r.LegacyLayout() && !r.Ok()) {
        r.SetOk(true);
        r.SetPos(startPos);
        r.SizedString();
        controllerRef = r.I32();
        state.controllerRef = controllerRef;
        state.applyMode = r.U32();
        textureCount = r.CountU32(64);
    }

    // Fuer NIF 20.0.0.4 entsprechen die klassischen Eintraege direkt den Slots
    // Base/Dark/Detail/Gloss/Glow/Bump/Decal0..3. Aeltere 10.x-Dateien verwenden dieselbe
    // Reihenfolge, besitzen aber die beiden PS2-Shorts in jeder TexDesc.
    for (std::uint32_t i = 0; i < textureCount; ++i) {
        const std::uint8_t hasTexture = r.U8();
        if (!hasTexture) continue;

        NifTextureSlotState slot;
        slot.present = true;
        slot.sourceRef = r.I32();
        slot.clampMode = r.U32();
        slot.filterMode = r.U32();
        slot.uvSet = r.U32();
        if (hasPS2Fields) {
            r.I16(); // PS2 L
            r.I16(); // PS2 K
        }
        slot.hasTransform = r.U8() != 0;
        if (slot.hasTransform) {
            slot.translation = {r.F32(), r.F32()};
            slot.scale = {r.F32(), r.F32()};
            slot.rotation = r.F32();
            slot.transformType = r.U32();
            slot.center = {r.F32(), r.F32()};
        }
        if (i < state.slots.size()) state.slots[i] = slot;

        // NiTexturingProperty fuegt NUR fuer den Bump-Slot diese 24 Byte an:
        // Luma Scale, Luma Offset und Matrix22. Die alte Implementierung uebersprang diesen
        // Block versehentlich doppelt; das machte Decal-/Shader-Slots hinter einem Bump-Map
        // unbrauchbar. Die Laenge folgt niflib/nif.xml und wird unten per Korpus-Test geprueft.
        if (i == 5) {
            state.bumpMapLumaScale = r.F32();
            state.bumpMapLumaOffset = r.F32();
            state.bumpMapMatrix = {r.F32(), r.F32(), r.F32(), r.F32()};
        }
    }

    // ShaderTexDesc ist shader-spezifisch. Den TexDesc deshalb vollständig bewahren und
    // erst zusammen mit dem Shadernamen der Geometrie zuordnen.
    const std::uint32_t numShaderTextures = r.CountU32(64);
    for (std::uint32_t i = 0; i < numShaderTextures; ++i) {
        const std::uint8_t hasMap = r.U8();
        if (!hasMap) continue;
        NifTextureSlotState slot;
        slot.present = true;
        slot.sourceRef = r.I32();
        slot.clampMode = r.U32();
        slot.filterMode = r.U32();
        slot.uvSet = r.U32();
        if (hasPS2Fields) { r.I16(); r.I16(); }
        slot.hasTransform = r.U8() != 0;
        if (slot.hasTransform) {
            slot.translation = {r.F32(), r.F32()};
            slot.scale = {r.F32(), r.F32()};
            slot.rotation = r.F32();
            slot.transformType = r.U32();
            slot.center = {r.F32(), r.F32()};
        }
        state.shaderSlots.emplace_back(r.U32(), slot);
    }
    return state;
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
    if (!r.LegacyLayout()) {
        ParseObjectNetBase(r);
        NifTextureSource source;
        source.useExternal = r.U8();
        source.filename = r.SizedString();
        source.pixelDataRef = r.I32();
        r.U32(); // pixel layout
        r.U32(); // mipmap format
        r.U32(); // alpha format
        r.U8();  // is static
        if (r.Version() >= 0x0A01006Au) r.U8(); // direct render
        return source;
    }
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

struct PendingPalettedTexture {
    std::int32_t paletteRef = -1;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> indices;
};

std::shared_ptr<const NifEmbeddedTexture> ParseNiPixelData(
    ByteReader& r, bool isOlderVersion,
    const std::unordered_map<std::uint32_t, NifPaletteState>& palettes,
    PendingPalettedTexture* pendingPalette = nullptr) {
    const std::uint32_t pixelFormat = r.PeekU32(0);
    std::array<std::uint32_t, 4> colorMasks{};
    std::uint32_t bitsPerPixel = 0;
    bool packedFormatSupported = true;
    if (r.LegacyLayout()) {
        r.Skip(isOlderVersion ? 50 : 72);
    } else {
        r.U32(); // pixel format
        if (isOlderVersion) {
            for (auto& mask : colorMasks) mask = r.U32();
            bitsPerPixel = r.U32();
            r.Skip(8); // old fast-compare byte array
            packedFormatSupported = r.U32() == 0; // no tiled raw images
        } else {
            bitsPerPixel = r.U8();
            r.U32(); r.U32(); // renderer hint, extra data
            r.U8(); // flags
            packedFormatSupported = r.U32() == 0;
            unsigned shift = 0;
            for (int i = 0; i < 4; ++i) {
                const auto component = r.U32(), representation = r.U32();
                const auto bits = r.U8(), isSigned = r.U8();
                if (bits > 32 || shift + bits > 32) packedFormatSupported = false;
                // Fiesta exporters set the trailing channel flag to 1 even for
                // unsigned RGB8/RGB5A1 colors (including one-bit alpha). Match
                // the existing RGB8 interpretation; do not sign-extend colors.
                else if (bits && component < 4 && representation == 0 && isSigned <= 1) {
                    if (colorMasks[component]) packedFormatSupported = false;
                    colorMasks[component] = static_cast<std::uint32_t>(((std::uint64_t{1} << bits) - 1) << shift);
                } else if (bits && component != 14 && component != 19) packedFormatSupported = false;
                shift += bits;
            }
            if (bitsPerPixel && shift != bitsPerPixel) packedFormatSupported = false;
        }
    }
    const std::int32_t paletteRef = r.I32();
    const std::uint32_t numMipmaps = r.CountU32(32);
    // "Bytes Per Pixel": 0 bei DXT-komprimierten Texturen, 3/4 bei unkomprimiertem RGB/RGBA
    // (an 1500 echten NIF-Texturen geprueft: 0 -> DXT, 3/4 -> Rohpixel).
    const std::uint32_t bytesPerPixel = r.U32();
    struct Mip { std::uint32_t width, height, offset; };
    std::vector<Mip> mips;
    mips.reserve(numMipmaps);
    for (std::uint32_t i = 0; i < numMipmaps; ++i) {
        mips.push_back({r.U32(), r.U32(), r.U32()});
    }
    const std::uint32_t dataSize = r.CountU32(64u * 1024u * 1024u);
    const auto faces = !r.LegacyLayout() && !isOlderVersion ? r.CountU32(6) : 1u;
    if (faces == 0) r.Invalidate();
    auto allPixels = r.BytesView(dataSize);
    if (faces > 1) r.Skip(static_cast<std::size_t>(faces - 1) * dataSize);
    if (!r.Ok() || mips.empty()) return {};
    // KORREKTUR (CHANGELOG [0.44.27], nif.xml-Referenz): Ab NIF 10.4.0.2 folgt auf "Num Pixels"
    // das Feld "Num Faces" (u32, hier immer 1), ERST DANACH beginnen die Pixeldaten. Die
    // Leseposition bleibt bewusst UNVERAENDERT (der Block-Parser darum herum ist heikel, siehe
    // HANDOFF) - stattdessen werden die 4 Bytes "Num Faces" am Anfang des gelesenen Puffers
    // uebersprungen und die 4 am Ende fehlenden Bytes (die noch zu den Pixeldaten gehoeren) per
    // Peek angehaengt. Vorher lag jede DXT-Textur 4 Byte verschoben und wurde als bunter Rauschteppich
    // dekodiert ("Objekt-Texturen nicht bunt"). Empirisch belegt: nur bei Versatz 4 sind die
    // DXT-Endpunkte benachbarter Bloecke glatt (Differenz ~10 statt ~80).
    std::size_t faceShift = 0;
    std::vector<std::uint8_t> compatibilityPixels;
    if (r.LegacyLayout() && !isOlderVersion) {
        faceShift = 4;
        const std::uint32_t tail = r.PeekU32(0);
        compatibilityPixels.assign(allPixels.begin(), allPixels.end());
        for (int i = 0; i < 4; ++i) compatibilityPixels.push_back(static_cast<std::uint8_t>((tail >> (8 * i)) & 0xFFu));
        allPixels = compatibilityPixels;
    }
    const auto& top = mips.front();
    if (top.width == 0 || top.height == 0 || top.offset + faceShift >= allPixels.size()) return {};
    std::size_t topSize = allPixels.size() - (top.offset + faceShift);
    if (mips.size() > 1 && mips[1].offset > top.offset)
        topSize = std::min(topSize, static_cast<std::size_t>(mips[1].offset - top.offset));
    const auto topData = allPixels.subspan(top.offset + faceShift, topSize);
    if (g_probeNoDecode) return nullptr;
    std::expected<DdsImage, std::string> decoded = std::unexpected(std::string("nicht gesetzt"));
    if (bytesPerPixel == 1 && paletteRef >= 0) {
        // Fiesta-Sonderfall: echte Dateien (z.B. filddoll.nif) deklarieren PixelFormat=6
        // (normalerweise DXT5_ALT), speichern aber 1 Byte Palettenindex pro Pixel und zeigen
        // auf einen NiPalette-Block. Deshalb entscheidet hier die reale Struktur
        // (bpp=1 + gueltige Palette-Ref), nicht allein die PixelFormat-Enum.
        const std::size_t pixelCount = static_cast<std::size_t>(top.width) * top.height;
        if (topData.size() < pixelCount) {
            decoded = std::unexpected(std::string("Palettenindizes kuerzer als das angegebene Top-Mip"));
        } else {
            const auto pit = palettes.find(static_cast<std::uint32_t>(paletteRef));
            if (pit == palettes.end()) {
                if (pendingPalette != nullptr) {
                    pendingPalette->paletteRef = paletteRef;
                    pendingPalette->width = top.width;
                    pendingPalette->height = top.height;
                    pendingPalette->indices.assign(topData.begin(), topData.begin() + static_cast<std::ptrdiff_t>(pixelCount));
                    return {};
                }
                decoded = std::unexpected(std::string("referenzierte NiPalette wurde noch nicht gelesen"));
            } else {
                DdsImage img;
                img.width = top.width;
                img.height = top.height;
                img.rgba.resize(pixelCount * 4);
                for (std::size_t px = 0; px < pixelCount; ++px) {
                    const std::size_t pi = static_cast<std::size_t>(topData[px]) * 4u;
                    img.rgba[px * 4 + 0] = pit->second.rgba[pi + 0];
                    img.rgba[px * 4 + 1] = pit->second.rgba[pi + 1];
                    img.rgba[px * 4 + 2] = pit->second.rgba[pi + 2];
                    img.rgba[px * 4 + 3] = pit->second.rgba[pi + 3];
                }
                decoded = std::move(img);
            }
        }
    } else if (!r.LegacyLayout() && bytesPerPixel >= 1 && bytesPerPixel <= 4) {
        if (!packedFormatSupported || bitsPerPixel != bytesPerPixel * 8)
            decoded = std::unexpected(std::string("Nicht unterstuetzte Rohpixel-Kanalbeschreibung"));
        else decoded = DecodePackedImage(top.width, top.height, bitsPerPixel, colorMasks, topData);
    } else if (bytesPerPixel == 3 || bytesPerPixel == 4) {
        // Unkomprimierte Pixel (Reihenfolge R,G,B[,A]). Fuer diese steht in "pixelFormat" NICHT
        // 4/5/6 - die Groesse des Top-Mips entscheidet (siehe CHANGELOG [0.44.27]).
        const std::size_t pixelCount = static_cast<std::size_t>(top.width) * top.height;
        if (topData.size() < pixelCount * bytesPerPixel) {
            decoded = std::unexpected(std::string("Rohpixel kuerzer als das angegebene Top-Mip"));
        } else {
            DdsImage img;
            img.width = top.width;
            img.height = top.height;
            img.rgba.resize(pixelCount * 4);
            for (std::size_t px = 0; px < pixelCount; ++px) {
                const std::uint8_t* src = topData.data() + px * bytesPerPixel;
                img.rgba[px * 4 + 0] = src[0];
                img.rgba[px * 4 + 1] = src[1];
                img.rgba[px * 4 + 2] = src[2];
                img.rgba[px * 4 + 3] = bytesPerPixel == 4 ? src[3] : 255;
            }
            decoded = std::move(img);
        }
    } else {
        decoded = DecodeBcImage(top.width, top.height, pixelFormat, topData);
    }
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

// UV-Sicherheitsnetz:
// Der frühere Befund "praktisch alle UVs unbrauchbar" war eine Folge des damals falsch
// positionierten 2-Byte-Felds vor den UV-Daten. Nach der Korrektur (Feld liegt hinter den
// UV-Sets) liefern die verifizierten Referenzdateien plausible authored UVs.
// Sanitize bleibt trotzdem als harte Schutzschicht gegen Recovery-/Sondervarianten erhalten.
// Wichtig seit Multi-Texture: NICHT nur der alte Base-Alias, sondern jedes einzelne UV-Set
// muss geprüft werden, weil Base/Dark/Detail/Gloss/Glow/Bump/Decals unterschiedliche Sets
// referenzieren können. Ein verworfenes sekundäres Set erlaubt dem Renderer den bestehenden
// deterministischen UV0-Fallback, statt mit NaN/extremen Koordinaten eine korrekte (auch
// eingebettete) Textur scheinbar verschwinden zu lassen.
NifUvSetDiagnostic SanitizeUvs(std::vector<NifVec2>& uvs) {
    NifUvSetDiagnostic diagnostic;
    diagnostic.originalCount = static_cast<std::uint32_t>(
        std::min<std::size_t>(uvs.size(), std::numeric_limits<std::uint32_t>::max()));
    for (std::size_t i = 0; i < uvs.size(); ++i) {
        const auto& uv = uvs[i];
        const bool finite = std::isfinite(uv.u) && std::isfinite(uv.v);
        if (finite) {
            if (!diagnostic.hasFinite) {
                diagnostic.hasFinite = true;
                diagnostic.minFiniteU = diagnostic.maxFiniteU = uv.u;
                diagnostic.minFiniteV = diagnostic.maxFiniteV = uv.v;
            } else {
                diagnostic.minFiniteU = std::min(diagnostic.minFiniteU, uv.u);
                diagnostic.maxFiniteU = std::max(diagnostic.maxFiniteU, uv.u);
                diagnostic.minFiniteV = std::min(diagnostic.minFiniteV, uv.v);
                diagnostic.maxFiniteV = std::max(diagnostic.maxFiniteV, uv.v);
            }
            diagnostic.maxFiniteAbs =
                std::max({diagnostic.maxFiniteAbs, std::abs(uv.u), std::abs(uv.v)});
            if (std::abs(uv.u) > 1000.0f || std::abs(uv.v) > 1000.0f)
                ++diagnostic.extremeCount;
        }
        // UVs are not range-limited by the NIF format. Large finite coordinates are valid
        // input for repeat/mirror-style sampling and occur in real ResMap assets (ship.nif).
        // Sanitization therefore rejects only values that cannot participate in arithmetic.
        if (!finite && !diagnostic.discarded) {
            diagnostic.discarded = true;
            diagnostic.firstBadIndex = static_cast<std::uint32_t>(
                std::min<std::size_t>(i, std::numeric_limits<std::uint32_t>::max()));
            diagnostic.firstBadValue = uv;
            diagnostic.nonFinite = true;
        }
    }
    if (diagnostic.discarded) uvs.clear();
    return diagnostic;
}

void SanitizeUvSets(std::vector<std::vector<NifVec2>>& uvSets, std::vector<NifVec2>& baseUvs,
                    std::vector<NifUvSetDiagnostic>& diagnostics) {
    diagnostics.clear();
    diagnostics.reserve(uvSets.size());
    for (auto& uvSet : uvSets) diagnostics.push_back(SanitizeUvs(uvSet));
    baseUvs = uvSets.empty() ? std::vector<NifVec2>{} : uvSets.front();
}

RawTriStripsData ParseNiTriStripsData(ByteReader& r, bool hasTrailer, bool isOlderVersion) {
    if (!r.LegacyLayout() && r.Version() >= 0x0A020000u) r.U32(); // group ID
    RawTriStripsData d;
    // KORRIGIERT: "Num Vertices" ist laut Format ein uint16, gefolgt von Keep-Flags(u8) +
    // Compress-Flags(u8) - NICHT ein einzelnes uint32 wie zuvor angenommen. Der u32-Read war
    // nur deshalb "byte-exakt verifiziert", weil Keep-/Compress-Flags in allen bisher
    // getesteten Dateien zufällig 0 waren (macht den u32-Wert zahlengleich zum echten u16-Wert).
    // Bei mindestens einer echten Datei sind sie ungleich 0 (siehe ParseNiTriShapeData für die
    // Herleitung mit plausiblen Vertex-Koordinaten) - dort führte der alte u32-Read zu einer
    // absurd großen "Vertex-Anzahl". Reine Präzisierung, kein Verhalten für die weit
    // überwiegende Mehrheit der Dateien geändert (dort bleiben die Byte-Positionen identisch).
    const std::uint32_t numVerts = r.CountU16(65535u);
    r.U8(); // keep_flags
    r.U8(); // compress_flags
    const std::uint8_t hasVerts = r.U8();
    // Billige Plausibilitaet gegen die restliche Dateigroesse: bei falsch ausgerichteten Probelaeufen
    // (Varianten-/Verschiebungssuche) stehen hier oft riesige Zaehler - ohne diese Pruefung parste jeder
    // Probelauf bis zu 65535 Vertices durch (8x langsamer nach Anhebung der u16-Grenzen).
    if (hasVerts && static_cast<std::size_t>(numVerts) * 12u > r.Remaining()) { r.Invalidate(); return d; }
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
    const bool hasTangentSpace = (dataFlags & 0xF000u) != 0; // KORRIGIERT [0.44.35]: alle 4 oberen Bit von tspace_flag (0xF0), nicht nur Bit 4 (0x10) - siehe SkipNiGeometryDataHeader
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
        d.vertexColors.reserve(numVerts);
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            d.vertexColors.push_back({r.F32(), r.F32(), r.F32(), r.F32()});
        }
    }
    // HAUPTFUND DIESER KORREKTUR: das früher hier (VOR den UV-Daten) gelesene "uv_flags"-u16
    // existiert an dieser Stelle laut Referenzimplementierung GAR NICHT - auf die Vertexfarben
    // folgen die UV-Sets direkt. Das bisherige Skippen dieser 2 Byte hat JEDE UV-Koordinate um
    // 2 Byte fehlausgerichtet gelesen (führte zu astronomisch großen/winzigen Werten, siehe
    // docs/MAP_FORMAT.md) - obwohl die GESAMTLÄNGE zufällig trotzdem exakt bis zum Dateiende
    // aufging, weil ein bislang unidentifiziertes 2-Byte-Feld tatsächlich HINTER den UV-Daten
    // liegt (statt davor) und die 2 Byte dort weiterhin konsumiert werden müssen (siehe unten).
    d.uvSets.resize(numUvSets);
    for (std::uint32_t set = 0; set < numUvSets; ++set) {
        auto& uvSet = d.uvSets[set];
        uvSet.reserve(numVerts);
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            const float u = r.F32();
            const float v = r.F32();
            uvSet.push_back({u, v});
        }
    }
    if (!d.uvSets.empty()) d.uvs = d.uvSets.front();
    // Das mysteriöse 2-Byte-Feld (Bedeutung weiterhin ungeklärt) gehört HIER hin, nicht vor die
    // UV-Daten. Byte-exakt verifiziert an santuary.nif: mit dieser Anordnung sind ALLE 86 UV-
    // Paare plausible, normalisierte Texturkoordinaten (0..1-Bereich, z.B. (0.213, 0.015)) UND
    // alle nachfolgenden Felder (consistency_flags=-1, num_triangles=284 - stimmt exakt mit der
    // Streifenlänge 286 überein, der bekannte 8-Byte-Trailer) treffen weiterhin exakt bis zum
    // letzten Byte der Datei.
    // Bytes von hier bis "num_triangles": bisher 4 (Version 10.1/10.2: mysteriöses Feld + 2 Byte
    // consistency) bzw. 6 (>= 20.0.0.4: mysteriöses Feld + 4 Byte consistency/additional data).
    // Bei NIF 10.1/10.2 gibt es aber zwei Varianten: manche Dateien haben (wie nif.xml sagt) NUR
    // 2 Byte (Consistency Flags) - z.B. Rou-Gras/-Blumen/-Pfaehle (grass3, flowers1, ship_post):
    // dort wurde alles um 2 Byte verschoben gelesen, das Ergebnis waren Vertices OHNE Dreiecke
    // und damit unsichtbare Objekte (CHANGELOG [0.44.28]). SELBSTPRUEFEND: die Dreiecksanzahl muss
    // zu den Streifen passen (Summe Streifenlaengen - 2*Streifenzahl == num_triangles); die
    // bisherige Ausrichtung hat Vorrang, nur wenn sie NICHT plausibel ist, die kuerzere.
    std::size_t bytesToNumTriangles = isOlderVersion ? (r.LegacyLayout() ? 4u : 2u) : 6u;
    if (r.LegacyLayout() && isOlderVersion) {
        auto peek16 = [&](std::size_t off) {
            return static_cast<std::uint32_t>(r.PeekU8(off)) | (static_cast<std::uint32_t>(r.PeekU8(off + 1)) << 8);
        };
        auto plausible = [&](std::size_t off) {
            const std::uint32_t triangles = peek16(off);
            const std::uint32_t strips = peek16(off + 2);
            if (strips == 0 || strips > 2000) return false;
            std::uint64_t sum = 0;
            for (std::uint32_t si = 0; si < strips; ++si) sum += peek16(off + 4 + 2 * si);
            if (sum < 2ull * strips || sum - 2ull * strips != triangles) return false;
            return r.PeekU8(off + 4 + 2 * strips) <= 1; // has_points
        };
        if (!plausible(4) && plausible(2)) bytesToNumTriangles = 2u;
    }
    r.Skip(bytesToNumTriangles);
    r.U16(); // num_triangles
    const std::uint16_t numStrips = r.CountU16(65535u);
    if (static_cast<std::size_t>(numStrips) * 2u > r.Remaining()) { r.Invalidate(); return d; }
    std::vector<std::uint16_t> stripLengths(numStrips);
    for (auto& sl : stripLengths) sl = r.CountU16(65535u);
    {
        std::size_t total = 0;
        for (const auto sl : stripLengths) total += sl;
        if (total * 2u > r.Remaining()) { r.Invalidate(); return d; }
    }
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
    if (r.LegacyLayout() && hasTrailer) {
        const bool freshWithTrailer = LooksLikeFreshName(r, 8);
        const bool freshWithoutTrailer = LooksLikeFreshName(r, 0);
        if (!freshWithTrailer && freshWithoutTrailer) {
            // Trailer wie vermutet nicht vorhanden - nichts weiter überspringen.
        } else {
            r.Skip(8);
        }
    }
    SanitizeUvSets(d.uvSets, d.uvs, d.uvSetDiagnostics);
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
    std::vector<NifColor4> vertexColors;
    std::vector<NifVec2> uvs;
    std::vector<std::vector<NifVec2>> uvSets;
    std::vector<NifUvSetDiagnostic> uvSetDiagnostics;
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
    if (!r.LegacyLayout() && r.Version() >= 0x0A020000u) r.U32(); // group ID
    RawTriShapeData d;
    // "Num Vertices"(u16) + Keep-Flags(u8) + Compress-Flags(u8) - siehe ausführliche Herleitung
    // bei ParseNiTriStripsData. Byte-exakt verifiziert an BerFrz01_IceSmog.nif: mit dem alten
    // u32-Read ergab sich eine absurde Vertex-Anzahl (3.342.408); mit u16(72)+keep(0x33=51,
    // Bedeutung ungeklärt)+compress ergeben sich 72 plausible Vertex-Koordinaten (z.B. eine
    // radialsymmetrische Anordnung: v1.y ≈ v4.x ≈ 492.24 - passend zu einem kegel-/
    // ringförmigen Partikel-Mesh).
    const std::uint32_t numVerts = r.CountU16(65535u);
    r.U8(); // keep_flags
    r.U8(); // compress_flags
    const std::uint8_t hasVerts = r.U8();
    // Billige Plausibilitaet gegen die restliche Dateigroesse: bei falsch ausgerichteten Probelaeufen
    // (Varianten-/Verschiebungssuche) stehen hier oft riesige Zaehler - ohne diese Pruefung parste jeder
    // Probelauf bis zu 65535 Vertices durch (8x langsamer nach Anhebung der u16-Grenzen).
    if (hasVerts && static_cast<std::size_t>(numVerts) * 12u > r.Remaining()) { r.Invalidate(); return d; }
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
    const bool hasTangentSpace = (dataFlags & 0xF000u) != 0; // KORRIGIERT [0.44.35]: alle 4 oberen Bit von tspace_flag (0xF0), nicht nur Bit 4 (0x10) - siehe SkipNiGeometryDataHeader
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
        d.vertexColors.reserve(numVerts);
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            d.vertexColors.push_back({r.F32(), r.F32(), r.F32(), r.F32()});
        }
    }
    // Kein "uv_flags" vor den UV-Daten - siehe ParseNiTriStripsData für die vollständige
    // Herleitung dieser Korrektur.
    d.uvSets.resize(numUvSets);
    for (std::uint32_t set = 0; set < numUvSets; ++set) {
        auto& uvSet = d.uvSets[set];
        uvSet.reserve(numVerts);
        for (std::uint32_t i = 0; i < numVerts; ++i) {
            const float u = r.F32();
            const float v = r.F32();
            uvSet.push_back({u, v});
        }
    }
    if (!d.uvSets.empty()) d.uvs = d.uvSets.front();
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
    const std::uint16_t numMatchGroups = r.CountU16(65535u);
    for (std::uint16_t g = 0; g < numMatchGroups; ++g) {
        const std::uint16_t numVertsInGroup = r.CountU16(65535u);
        for (std::uint16_t i = 0; i < numVertsInGroup; ++i) r.U16();
    }
    // Gleicher konditionaler 8-Byte-Trailer wie bei NiTriStripsData (siehe dort) - fehlt, wenn
    // direkt ein weiteres NiTriShape/NiTriStrips oder eine NiSkinInstance folgt
    // (Mehrfach-Mesh-Objekt bzw. geskinntes Mesh). Gleiche Peek-Absicherung wie dort (siehe
    // LooksLikeFreshName) NUR im Standardfall (hasTrailer=true) - der explizite Ausschluss wird
    // nicht durch den Peek in Frage gestellt (siehe ausführliche Begründung dort).
    if (r.LegacyLayout() && hasTrailer) {
        const bool freshWithTrailer = LooksLikeFreshName(r, 8);
        const bool freshWithoutTrailer = LooksLikeFreshName(r, 0);
        if (!freshWithTrailer && freshWithoutTrailer) {
            // Trailer wie vermutet nicht vorhanden.
        } else {
            r.Skip(8);
        }
    }
    SanitizeUvSets(d.uvSets, d.uvs, d.uvSetDiagnostics);
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
    if (!r.LegacyLayout()) return;
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
    if (numVerts < 3 || numVerts > 65535) return false;
    const std::uint8_t keep = r.PeekU8(offset + 2);
    const std::uint8_t compress = r.PeekU8(offset + 3);
    const std::uint8_t hasVerts = r.PeekU8(offset + 4);
    // keep_flags ist ein Bitfeld: neben 0/1 kommt 0x33 vor (X_Adl/adl_town_teras_ground.nif,
    // FireDn01/bail80_DG.nif) - nur compress und has_vertices muessen 0/1 sein (CHANGELOG [0.44.31]).
    (void)keep;
    return compress <= 1 && hasVerts <= 1;
}

} // namespace

namespace {

// Blocktypen, die mit einem Namen (SizedString) beginnen (NiObjectNET-Ableitungen) - nur bei diesen ist
// eine Namens-Resynchronisation an der Blockgrenze sinnvoll (Daten-Bloecke wie NiTriStripsData nicht).
bool BlockStartsWithName(const std::string& type) {
    static const char* const kTypes[] = {"NiNode", "NiBillboardNode", "NiLODNode", "NiSwitchNode", "NiSortAdjustNode", "NiTriStrips", "NiTriShape",
                                         "NiMaterialProperty", "NiTexturingProperty", "NiAlphaProperty", "NiZBufferProperty",
                                         "NiVertexColorProperty", "NiStencilProperty", "NiSpecularProperty", "NiStringExtraData",
                                         "NiIntegerExtraData", "NiSourceTexture", "NiParticleSystem", "NiCamera", "NiAmbientLight"};
    for (const char* t : kTypes) if (type == t) return true;
    return false;
}

// Sieht die Position (relativ zur aktuellen um `delta` Byte verschoben) wie ein Blockanfang mit
// NICHT leerem, druckbarem Namen aus? (Laenge 1..64, nur ASCII 0x20..0x7E, danach ein kleiner u32.)
bool PlausibleNamedStart(const ByteReader& r, long long delta) {
    const long long target = static_cast<long long>(r.Pos()) + delta;
    if (target < 0) return false;
    ByteReader t = r;
    t.SetPos(static_cast<std::size_t>(target));
    const std::uint32_t len = t.PeekU32(0);
    if (len < 1u || len > 64u) return false;
    for (std::uint32_t i = 0; i < len; ++i) {
        const std::uint8_t b = t.PeekU8(4 + i);
        if (b < 0x20 || b > 0x7E) return false;
    }
    return t.PeekU32(4 + len) <= 8u;
}

// Blocktypen am DATEIENDE (Animation, Partikel, Kollision), deren Parse-Fehler die schon geladene
// Geometrie nicht entwerten: wird ein Modell dort abgebrochen, kann das bisher Geladene als
// Teilmodell verwendet werden (Objekt statt Platzhalter, siehe LoadNifMesh / CHANGELOG [0.44.29]).
bool IsToleratedTailBlock(const std::string& type) {
    static const char* const kPrefixes[] = {"NiPSys", "NiMeshPSys", "NiParticle", "NiPSMesh", "NiBool", "NiFloat", "NiPoint3", "NiColor",
                                            "NiTransformController", "NiTransformInterpolator", "NiTransformData", "NiFlipController",
                                            "NiControllerManager", "NiControllerSequence", "NiTextKey", "NiVisController", "NiAlphaController",
                                            "NiUVController", "NiKeyframe", "NiStringExtra", "NiCollision", "NiBoneLOD", "NiMultiTarget",
                                            "NiLookAt", "NiPathController", "NiLight", "NiDirectionalLight", "NiPointLight", "NiAmbientLight",
                                            "NiMorph", "NiGeomMorpher", "NiRotatingParticles", "NiUVData", "NiRollController"};
    for (const char* prefix : kPrefixes) if (type.rfind(prefix, 0) == 0) return true;
    return false;
}

// Kern des Ladens. `materialFlipMask`: Bit k dreht die 14/15-Floats-Regel der k-ten
// NiMaterialProperty der Datei um (Variantensuche, siehe LoadNifMesh). `strictEnd`: nach dem
// letzten Block muss (fast) nur noch die Fusszeile uebrig sein - dient als Pruefsumme, wenn eine
// Variante ausprobiert wird, damit keine falsche Variante "erfolgreich" durchrutscht.
std::expected<NifModel, std::string> LoadNifMeshData(const std::vector<std::uint8_t>& data,
                                                     std::uint32_t materialFlipMask, std::uint32_t pixelVariant,
                                                     bool strictEnd, std::uint32_t* materialCountOut,
                                                     std::uint32_t* pixelCountOut,
                                                     const std::vector<std::pair<std::uint32_t, int>>* shifts = nullptr,
                                                     bool allowPartial = false, bool nameResync = false,
                                                     bool legacyLayout = true) {
    ByteReader r(data, legacyLayout);
    auto headerResult = ParseHeader(r, data);
    if (!headerResult) {
        return std::unexpected(headerResult.error());
    }
    const NifHeader& hdr = *headerResult;
    const bool traceBlocks = std::getenv("NEXTGEN_NIF_TRACE") != nullptr;
    {
        std::uint32_t materials = 0, pixels = 0;
        for (const auto ti : hdr.blockTypeIndex) {
            if (ti >= hdr.blockTypes.size()) continue;
            if (hdr.blockTypes[ti] == "NiMaterialProperty") ++materials;
            else if (hdr.blockTypes[ti] == "NiPixelData") ++pixels;
        }
        if (materialCountOut != nullptr) *materialCountOut = materials;
        if (pixelCountOut != nullptr) *pixelCountOut = pixels;
    }

    static const std::unordered_set<std::string> kSupportedPropertyTypes = {
        "NiZBufferProperty", "NiVertexColorProperty", "NiMaterialProperty", "NiTexturingProperty",
        "NiAlphaProperty", "NiStencilProperty", "NiSpecularProperty", "NiFogProperty",
        "NiDitherProperty", "NiShadeProperty",
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
    std::unordered_map<std::uint32_t, NifPaletteState> palettesByBlock;
    std::unordered_map<std::uint32_t, std::shared_ptr<const NifEmbeddedTexture>> embeddedPixelTextures;
    std::unordered_map<std::uint32_t, PendingPalettedTexture> pendingPalettedPixelTextures;
    std::unordered_map<std::uint32_t, NifFloatInterpolatorState> floatInterpolatorsByBlock;
    std::unordered_map<std::uint32_t, NifFloatDataState> floatDataByBlock;
    std::unordered_map<std::uint32_t, NifTextureTransformControllerState> texTransformControllersByBlock;
    std::unordered_map<std::uint32_t, NifFlipControllerState> flipControllersByBlock;
    std::unordered_map<std::size_t, std::int32_t> partBaseTextureRef;      // Legacy-Alias fuer Slot 0
    std::unordered_map<std::size_t, std::array<std::int32_t, 10>> partTextureRefs; // Part -> SourceTexture-Refs je Slot
    std::unordered_map<std::size_t, std::vector<std::vector<std::int32_t>>> partFlipSourceRefs;

    // Szenengraph (CHANGELOG [0.44.28]): Translation/Rotation/Skalierung jedes NiNode/NiTriStrips/
    // NiTriShape wurden bisher gelesen, aber NIRGENDS angewendet - Vertices blieben im lokalen
    // Geometrie-Raum (tree05: Y-Bereich -1104..-10 -> Baum steckte 2500 Einheiten im Boden, viele
    // Objekte "auf falscher Hoehe"). Hier wird pro Block gemerkt, was fuer die Weltmatrix noetig ist.
    struct SceneNode {
        bool present = false;
        NifVec3 translation;
        std::array<float, 9> rotation{};
        float scale = 1.0f;
        std::vector<std::int32_t> children;
        std::vector<std::int32_t> properties;
        std::vector<std::int32_t> effects;
        std::string name;
        bool billboard = false;
        std::uint16_t billboardMode = 0;
        std::int32_t lodDataRef = -1;
    };
    std::uint32_t materialIndex = 0;
    std::uint32_t pixelIndex = 0;
    bool partialStop = false;
    std::vector<SceneNode> scene(hdr.numBlocks);
    std::unordered_map<std::int32_t, std::uint32_t> dataToGeometry; // Datenblock -> Geometrieblock
    std::vector<std::int32_t> partDataBlock;                        // Part-Index -> Datenblock
    // Geometrie-getriebener Neuaufbau der Parts (CHANGELOG [0.44.30]): das urspruengliche Verfahren
    // legt EINEN Part je NiMaterialProperty an und fuellt ihn mit dem naechsten Datenblock - bei
    // Modellen mit mehreren Detailstufen (NiLODNode) oder geteilten Properties (Charakter-NIFs) gibt es
    // aber MEHR Datenbloecke als Materialien: Positionen des letzten Blocks + Indizes ALLER Bloecke
    // (z.B. 24 Vertices mit 218 Dreiecken). Deshalb wird alles Noetige pro Block mitgeschrieben und
    // bei erkannter Inkonsistenz aus den Geometrie-Knoten neu aufgebaut.
    struct RawGeom {
        std::vector<NifVec3> positions, normals;
        std::vector<NifColor4> vertexColors;
        std::vector<NifVec2> uvs;
        std::vector<std::vector<NifVec2>> uvSets;
        std::vector<NifUvSetDiagnostic> uvSetDiagnostics;
        std::vector<std::uint32_t> triangleIndices;
    };
    struct GeomNode {
        std::uint32_t block = 0;
        std::int32_t dataRef = -1;
        std::int32_t skinInstanceRef = -1;
        std::vector<std::int32_t> properties;
        std::string shaderName;
    };
    std::unordered_map<std::int32_t, RawGeom> rawByData;
    std::vector<GeomNode> geomNodes;
    std::unordered_map<std::uint32_t, NifMaterial> materialByBlock;
    std::unordered_map<std::uint32_t, NifAlphaState> alphaByBlock;
    std::unordered_map<std::uint32_t, NifZBufferState> zBufferByBlock;
    std::unordered_map<std::uint32_t, NifStencilState> stencilByBlock;
    std::unordered_map<std::uint32_t, NifVertexColorState> vertexColorByBlock;
    std::unordered_map<std::uint32_t, bool> specularByBlock;
    std::unordered_map<std::uint32_t, NifTextureState> texStateByBlock;
    std::unordered_map<std::uint32_t, NifTextureEffectState> textureEffectByBlock;
    std::unordered_map<std::uint32_t, SkinInstanceBlock> skinInstanceByBlock;
    std::unordered_map<std::uint32_t, SkinDataBlock> skinDataByBlock;
    std::unordered_map<std::uint32_t, SkinPartitionBlock> skinPartitionByBlock;
    struct LodRangeData {
        NifVec3 center{};
        std::vector<std::pair<float, float>> ranges;
    };
    std::unordered_map<std::uint32_t, LodRangeData> lodRangeByBlock;
    auto recordNode = [&](std::uint32_t idx, const NiNodeBlock& n) {
        if (idx >= scene.size()) return;
        SceneNode& sn = scene[idx];
        sn.present = true;
        sn.translation = n.base.translation;
        sn.rotation = n.base.rotation;
        sn.scale = n.base.scale;
        sn.children = n.children;
        sn.properties = n.base.properties;
        sn.effects = n.effects;
        sn.name = n.base.net.name;
    };

    for (std::uint32_t blockIdx = 0; blockIdx < hdr.numBlocks; ++blockIdx) {
        if (blockIdx >= hdr.blockTypeIndex.size()) break;
        const std::string& type = hdr.blockTypes[hdr.blockTypeIndex[blockIdx]];
        if (nameResync && blockIdx > 0 && BlockStartsWithName(type) && !PlausibleNamedStart(r, 0)) {
            // Namens-Resynchronisation (nur als eigene Stufe NACH einem gescheiterten Standardlauf):
            // steht die Position nicht auf einem plausiblen Blockanfang, den naechsten plausiblen
            // Anfang mit nicht leerem Namen im Umkreis von -16..+16 Byte suchen (kleinste Verschiebung
            // zuerst) - z.B. wenn der Vorgaenger die Namenslaenge des Nachfolgers mitgelesen hat
            // (NiSourceTexture -> benannte NiMaterialProperty) oder zu wenige Byte (NiVertexColorProperty).
            for (long long dist = 1; dist <= 16; ++dist) {
                bool moved = false;
                for (const long long delta : {-dist, dist}) {
                    if (PlausibleNamedStart(r, delta)) { r.SetPos(static_cast<std::size_t>(static_cast<long long>(r.Pos()) + delta)); moved = true; break; }
                }
                if (moved) break;
            }
        }
        if (shifts != nullptr) {
            // Variantensuche: Leseposition vor diesem Block um wenige Byte verschieben.
            for (const auto& [shiftBlock, shiftBytes] : *shifts) {
                if (shiftBlock != blockIdx) continue;
                const long long target = static_cast<long long>(r.Pos()) + shiftBytes;
                if (target < 0 || static_cast<std::size_t>(target) >= data.size()) return std::unexpected("Verschiebung ausserhalb der Datei");
                r.SetPos(static_cast<std::size_t>(target));
            }
        }

        // Niflib's object stream has a zero uint BEFORE EACH block through 10.1.0.106.
        // It is not a one-off header field or padding in the preceding object.
        if (!r.LegacyLayout() && hdr.version <= 0x0A01006Au && r.U32() != 0)
            return std::unexpected("Ungueltiger Block-Marker bei Block " + std::to_string(blockIdx) + " (" + type + ")");
        if (traceBlocks) std::fprintf(stderr, "block %u %s offset %zu\n", blockIdx, type.c_str(), r.Pos());
        if (type == "NiNode") {
            NiNodeBlock node = ParseNiNode(r);
            recordNode(blockIdx, node);
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiBillboardNode") {
            // NiBillboardNode : NiNode + BillboardMode(u16 bei diesen NIF-Versionen).
            // NifSkope ersetzt beim Transformieren die Rotation des Billboard-Knotens durch
            // die Kameraorientierung. Wir bewahren den Modus und die Knotenhierarchie deshalb
            // bis zum Renderer auf, statt ihn dauerhaft wie einen normalen NiNode einzubacken.
            NiNodeBlock node = ParseNiNode(r);
            recordNode(blockIdx, node);
            if (blockIdx < scene.size()) {
                scene[blockIdx].billboard = true;
                scene[blockIdx].billboardMode = r.U16();
            } else {
                r.U16();
            }
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiLODNode") {
            // NiLODNode : NiSwitchNode : NiNode. Das gespeicherte Switch-Index-Feld ist nur
            // der statische Zustand; fuer die Vorschau werden die Kindbereiche aus
            // NiRangeLODData zur Laufzeit anhand der Kameraentfernung ausgewertet.
            NiNodeBlock node = ParseNiNode(r);
            r.U16();  // switch_flags
            r.U32();  // index (statisches/gespeichertes Kind; Runtime-LOD hat Vorrang)
            const std::int32_t lodDataRef = r.I32();
            recordNode(blockIdx, node);
            if (blockIdx < scene.size()) scene[blockIdx].lodDataRef = lodDataRef;
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiSortAdjustNode") {
            NiNodeBlock node = ParseNiSortAdjustNode(r);
            recordNode(blockIdx, node);
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiRoomGroup") {
            NiNodeBlock node = ParseNiRoomGroup(r);
            recordNode(blockIdx, node);
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiRoom") {
            NiNodeBlock node = ParseNiRoom(r);
            recordNode(blockIdx, node);
            if (!rootNameSet) {
                model.rootName = node.base.net.name;
                rootNameSet = true;
            }
        } else if (type == "NiRangeLODData") {
            if (!r.LegacyLayout()) {
                LodRangeData lodData;
                const float x = r.F32(), y = r.F32(), z = r.F32();
                lodData.center = {x, y, z}; // scene transforms remap to editor space later
                const auto count = r.CountU32(10000);
                for (std::uint32_t i = 0; i < count; ++i) {
                    const float nearExtent = r.F32(), farExtent = r.F32();
                    lodData.ranges.emplace_back(nearExtent, farExtent);
                }
                lodRangeByBlock[blockIdx] = std::move(lodData);
            } else {
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
            LodRangeData lodData;
            lodData.ranges.reserve(numLodLevels);
            for (std::uint32_t i = 0; i < numLodLevels; ++i) {
                const float nearExtent = r.F32();
                const float farExtent = r.F32();
                lodData.ranges.emplace_back(nearExtent, farExtent);
            }
            // Dieser Fiesta-Fork speichert in den verifizierten 20.0.0.4-Dateien keinen
            // Vector3-Center an dieser Stelle. Center=(0,0,0) bedeutet daher den Ursprung
            // des NiLODNode; dessen Weltposition wird nach der Scene-Graph-Aufloesung berechnet.
            lodRangeByBlock[blockIdx] = std::move(lodData);
            r.Skip(8); // abschließender Trailer, Bedeutung ungeklärt
            // Manche Dateien (Item-NIFs wie JackO_CBow00/PierrotWand, Feld-Baeume) haben hier
            // MEHR Bytes (bis zu 24 zusaetzlich: weitere near/far-Paare) - das folgende NiNode
            // beginnt dann erst spaeter. Nur wenn die Position NICHT wie ein Blockanfang
            // (Name + kleine Extra-Data-Zahl) aussieht, wird bis zum naechsten plausiblen
            // Blockanfang weitergesprungen (max. 64 Byte), sonst bleibt alles wie bisher.
            if (blockIdx + 1 < hdr.blockTypeIndex.size()) {
                auto plausibleBlockStart = [&](std::size_t off) {
                    const std::uint32_t len = r.PeekU32(off);
                    if (len == 0) return r.PeekU32(off + 4) <= 8u;
                    if (len > 64u) return false;
                    for (std::uint32_t i = 0; i < len; ++i) {
                        const std::uint8_t b = r.PeekU8(off + 4 + i);
                        if (b < 0x20 || b > 0x7E) return false;
                    }
                    return r.PeekU32(off + 4 + len) <= 8u;
                };
                if (!plausibleBlockStart(0)) {
                    for (std::size_t k = 4; k <= 64; k += 4) {
                        if (plausibleBlockStart(k)) { r.Skip(k); break; }
                    }
                }
                // (Selten: der Trailer war kuerzer - dann liegt der Blockanfang VOR der Position.)
                if (!plausibleBlockStart(0)) {
                    for (std::size_t back = 4; back <= 24 && back <= r.Pos(); back += 4) {
                        const std::size_t saved = r.Pos();
                        r.SetPos(saved - back);
                        if (plausibleBlockStart(0)) break;
                        r.SetPos(saved);
                    }
                }
            }
            }
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
            palettesByBlock[blockIdx] = ParseNiPalette(r);
        } else if (type == "NiVisController") {
            SkipNiVisController(r);
        } else if (type == "NiBoneLODController") {
            SkipNiBoneLODController(r);
        } else if (type == "NiMultiTargetTransformController") {
            SkipNiMultiTargetTransformController(r);
        } else if (type == "NiControllerManager") {
            SkipNiControllerManager(r);
        } else if (type == "NiControllerSequence") {
            SkipNiControllerSequence(r);
        } else if (type == "NiBSplineTransformInterpolator" || type == "NiBSplineCompTransformInterpolator") {
            r.F32(); r.F32(); // start and stop time
            r.I32(); r.I32(); // spline and basis data
            for (int i = 0; i < 8; ++i) r.F32(); // translation, quaternion, scale
            if (hdr.version <= 0x0A01006Du) { r.U8(); r.U8(); r.U8(); } // TRS valid
            r.U32(); r.U32(); r.U32(); // translation, rotation and scale handles
            if (type == "NiBSplineCompTransformInterpolator")
                for (int i = 0; i < 6; ++i) r.F32(); // offset and half-range per channel
        } else if (type == "NiBSplineFloatInterpolator" || type == "NiBSplineCompFloatInterpolator" ||
                   type == "NiBSplinePoint3Interpolator" || type == "NiBSplineCompPoint3Interpolator") {
            r.F32(); r.F32(); // start and stop time (NiBSplineInterpolator)
            r.I32(); r.I32(); // spline and basis references
            const bool point = type.find("Point3") != std::string::npos;
            for (int i = 0; i < (point ? 3 : 1); ++i) r.F32(); // base value
            r.U32(); // data handle
            if (type.find("Comp") != std::string::npos) { r.F32(); r.F32(); } // offset, half-range
        } else if (type == "NiBSplineData") {
            const auto floats = r.U32(); // Size is bounded by the remaining file, not an arbitrary key count.
            r.Skip(static_cast<std::size_t>(floats) * 4);
            const auto shorts = r.U32();
            r.Skip(static_cast<std::size_t>(shorts) * 2);
        } else if (type == "NiBSplineBasisData") {
            r.U32(); // number of control points
        } else if (type == "NiDefaultAVObjectPalette") {
            r.I32(); // scene
            const auto count = r.CountU32();
            for (std::uint32_t i = 0; i < count; ++i) { r.SizedString(); r.I32(); }
        } else if (type == "NiStringPalette") {
            r.SizedString(); // binary string table, includes NUL separators
            r.U32(); // repeated length
        } else if (type == "NiBlendAccumTransformInterpolator") {
            ParseFiestaAccumulationState(r);
        } else if (type == "NPTR_ISShader_v2") {
            ParseFiestaShaderReference(r);
        } else if (type == "NsPgToonExtraData") {
            ParseFiestaToonExtraData(r);
        } else if (type == "NiBlendFloatInterpolator" || type == "NiBlendBoolInterpolator" ||
                   type == "NiBlendTransformInterpolator" || type == "NiBlendPoint3Interpolator") {
            SkipNiBlendInterpolator(r);
            if (type == "NiBlendFloatInterpolator") r.F32();
            else if (type == "NiBlendBoolInterpolator") r.U8();
            else if (type == "NiBlendPoint3Interpolator") { r.F32(); r.F32(); r.F32(); }
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
        } else if (type == "NiShadeProperty") {
            ParseObjectNetBase(r);
            r.U16(); // shade flags
        } else if (type == "NiCamera") {
            ParseAVObjectBase(r);
            r.U16(); // camera flags
            for (int i = 0; i < 6; ++i) r.F32(); // frustum
            r.U8(); // orthographic
            for (int i = 0; i < 5; ++i) r.F32(); // viewport and LOD adjust
            r.I32(); // scene
            // These deprecated arrays must be zero on disk (NifXML).
            if (r.U32() != 0 || r.U32() != 0) r.Invalidate();
        } else if (type == "NiSourceCubeMap") {
            sourceTextureFilenames[blockIdx] = ParseNiSourceTexture(r).filename;
        } else if (type == "NiPortal") {
            SkipNiPortal(r);
        } else if (type == "NiCollisionData") {
            SkipNiCollisionData(r);
        } else if (type == "NiTransformController" || type == "NiKeyframeController") {
            SkipNiTransformController(r);
        } else if (type == "NiTransformInterpolator") {
            SkipNiTransformInterpolator(r);
        } else if (type == "NiTransformData" || type == "NiKeyframeData") {
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
            // 30-Byte-NiSingleInterpController-Basis + shader_map(u8) + texture_slot(u32) +
            // operation(u32) = 39 Byte. Ab v11 werden diese Werte nicht nur byte-exakt
            // konsumiert, sondern als echte UV-Animation an das referenzierte
            // NiTexturingProperty weitergereicht.
            texTransformControllersByBlock[blockIdx] = ParseNiTextureTransformController(r);
        } else if (type == "NiFloatInterpolator") {
            floatInterpolatorsByBlock[blockIdx] = ParseNiFloatInterpolator(r);
        } else if (type == "NiFloatData") {
            floatDataByBlock[blockIdx] = ParseNiFloatData(r);
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
        } else if (type == "NiParticlesData" || type == "NiRotatingParticlesData") {
            SkipNiParticlesData(r, hdr.version);
        } else if (type == "NiPSysEmitterCtlr") {
            SkipNiPSysEmitterCtlr(r);
        } else if (type == "NiPSysModifierActiveCtlr") {
            SkipNiPSysModifierActiveCtlr(r);
        } else if (type == "NiPSysGravityStrengthCtlr") {
            // NiPSysGravityStrengthCtlr = NiPSysModifierFloatCtlr = NiPSysModifierCtlr, exakt
            // dieselbe Struktur wie NiPSysModifierActiveCtlr (30-Byte-Basis + modifier_name).
            SkipNiPSysModifierActiveCtlr(r);
        } else if (type == "NiPSysEmitterLifeSpanCtlr" ||
                   type == "NiPSysEmitterInitialRadiusCtlr" || type == "NiPSysEmitterSpeedCtlr" ||
                   type == "NiPSysEmitterPlanarAngleCtlr" || type == "NiPSysEmitterPlanarAngleVarCtlr" ||
                   type == "NiPSysEmitterDeclinationCtlr" || type == "NiPSysEmitterDeclinationVarCtlr" ||
                   type == "NiPSysInitialRotSpeedCtlr" || type == "NiPSysInitialRotSpeedVarCtlr" ||
                   type == "NiPSysInitialRotAngleCtlr" || type == "NiPSysInitialRotAngleVarCtlr") {
            // Ebenfalls NiPSysModifierFloatCtlr - dieselbe Struktur.
            SkipNiPSysModifierActiveCtlr(r);
        } else if (type == "NiPSysPlanarCollider") {
            SkipNiPSysPlanarCollider(r);
        } else if (type == "NiPSysSphericalCollider") {
            SkipNiPSysColliderBase(r);
            r.F32(); // radius
        } else if (type == "NiFlipController") {
            flipControllersByBlock[blockIdx] = ParseNiFlipController(r);
        } else if (type == "NiPSysUpdateCtlr" || type == "NiPSysResetOnLoopCtlr") {
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
        } else if (type == "NiPSysCylinderEmitter") {
            SkipNiPSysCylinderEmitter(r);
        } else if (type == "NiPSysSphereEmitter") {
            SkipNiPSysSphereEmitter(r);
        } else if (type == "NiPSysBombModifier") {
            SkipNiPSysBombModifier(r);
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
            skinInstanceByBlock[blockIdx] = ParseNiSkinInstance(r);
        } else if (type == "NiSkinData") {
            skinDataByBlock[blockIdx] = ParseNiSkinData(r);
        } else if (type == "NiSkinPartition") {
            skinPartitionByBlock[blockIdx] = ParseNiSkinPartition(r);
        } else if (type == "NiTextureEffect") {
            ++model.textureEffectBlocks;
            auto effect = ParseNiTextureEffect(r);
            if (effect.dynamic.switchState &&
                effect.textureType == 2u && effect.coordGenType == 2u &&
                effect.sourceTextureRef >= 0) {
                ++model.textureEffectEnvironmentSphereBlocks;
            } else {
                ++model.textureEffectUnsupportedBlocks;
            }
            textureEffectByBlock[blockIdx] = std::move(effect);
        } else if (type == "NiDirectionalLight" || type == "NiAmbientLight") {
            // NiAmbientLight ist laut Referenz (PyFFI) ebenfalls reine NiLight-Basis ohne
            // eigene Zusatzfelder, exakt wie NiDirectionalLight.
            SkipNiDirectionalLight(r);
        } else if (type == "NiPointLight") {
            SkipNiPointLight(r);
        } else if (type == "NiZBufferProperty") {
            ++model.zBufferPropertyBlocks;
            zBufferByBlock[blockIdx] = ParseNiZBufferProperty(r);
            // Siehe SkipExtraBytesIfFollowedByTriData - hier bewusst weiterhin mit
            // Versions-Gate belassen (siehe Abschnitt 38: ein unbedingter Test verursachte
            // eine Regression), auch wenn sich das bei NiAlphaProperty als unnötig
            // herausstellte - nicht risikofrei verallgemeinern ohne erneuten Test.
            if (hdr.version != 0x14000004u) {
                SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
            }
        } else if (type == "NiVertexColorProperty") {
            ++model.vertexColorPropertyBlocks;
            vertexColorByBlock[blockIdx] = ParseNiVertexColorProperty(r);
            SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
        } else if (type == "NiAlphaProperty") {
            alphaByBlock[blockIdx] = ParseNiAlphaProperty(r);
            SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
        } else if (type == "NiStencilProperty") {
            stencilByBlock[blockIdx] = ParseNiStencilProperty(r);
        } else if (type == "NiSpecularProperty") {
            specularByBlock[blockIdx] = ParseNiSpecularProperty(r);
            SkipExtraBytesIfFollowedByTriData(r, hdr, blockIdx);
        } else if (type == "NiPathInterpolator") {
            SkipNiPathInterpolator(r);
        } else if (type == "NiTriStrips" || type == "NiTriShape") {
            NiTriStripsBlock strips = ParseNiTriStripsHeader(r);
            if (blockIdx < scene.size()) {
                SceneNode& sn = scene[blockIdx];
                sn.present = true;
                sn.translation = strips.base.translation;
                sn.rotation = strips.base.rotation;
                sn.scale = strips.base.scale;
                sn.properties = strips.base.properties;
                if (strips.dataRef >= 0) dataToGeometry[strips.dataRef] = blockIdx;
            }
            geomNodes.push_back({blockIdx, strips.dataRef, strips.skinInstanceRef, strips.base.properties, strips.shaderName});
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
            bool fifteen = !currentMeshHasTexturing;
            // The legacy mesh path sometimes consumes GeometryData::unknownInt
            // as a fifteenth material float. Particle data now owns that field.
            if (blockIdx + 1 < hdr.blockTypeIndex.size()) {
                const auto& next = hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]];
                if (next == "NiPSysData" || next == "NiMeshPSysData" ||
                    next == "NiParticlesData" || next == "NiRotatingParticlesData") fifteen = false;
            }
            if (materialIndex < 32 && ((materialFlipMask >> materialIndex) & 1u) != 0) fifteen = !fifteen;
            ++materialIndex;
            NifMaterial mat = ParseNiMaterialProperty(r, fifteen);
            materialByBlock[blockIdx] = mat;
            model.parts.emplace_back();
            model.parts.back().material = mat;
        } else if (type == "NiTexturingProperty") {
            // Ältere NIF-Versionen (10.1.0.0/10.2.0.0, vor 10.4.0.1) haben laut autoritativer
            // Referenz zwei zusätzliche PS2-Felder je Textur-Slot - bei 20.0.0.4 entfallen.
            const bool hasPS2Fields = hdr.version != 0x14000004u;
            const NifTextureState texState = ParseNiTexturingProperty(r, hasPS2Fields);
            texStateByBlock[blockIdx] = texState;
            const auto& baseSlot = texState.slots[0];
            if (baseSlot.present && baseSlot.sourceRef >= 0 && !model.parts.empty()) {
                // Legacy-Zuordnung fuer den normalen Blockreihenfolge-Pfad; die vollstaendige
                // Slot-Zuordnung wird nach dem Parsen ueber die Geometry-Property-Refs geloest.
                partBaseTextureRef[model.parts.size() - 1] = baseSlot.sourceRef;
                model.parts.back().baseUvSet = baseSlot.uvSet;
                model.parts.back().textureClampMode = baseSlot.clampMode;
                model.parts.back().textureFilterMode = baseSlot.filterMode;
            }
        } else if (type == "NiSourceTexture") {
            sourceTextures[blockIdx] = ParseNiSourceTexture(r);
        } else if (type == "NiPixelData") {
            // Variante fuer die Trailer-Behandlung dieses NiPixelData (0 = Standardlogik, 1 = genau
            // 4 Byte ueberspringen, 2 = nichts ueberspringen) - nur bei der Variantensuche != 0.
            std::uint32_t pixelDigit = 0;
            {
                std::uint32_t pv = pixelVariant;
                for (std::uint32_t k = 0; k < pixelIndex; ++k) pv /= 3u;
                pixelDigit = pv % 3u;
                ++pixelIndex;
            }
            PendingPalettedTexture pendingPalette;
            auto embedded = ParseNiPixelData(r, hdr.version != 0x14000004u, palettesByBlock, &pendingPalette);
            if (embedded) embeddedPixelTextures[blockIdx] = std::move(embedded);
            else if (!pendingPalette.indices.empty()) pendingPalettedPixelTextures[blockIdx] = std::move(pendingPalette);
            // Der 8-Byte-Abschluss von NiPixelData ist bereits in der 17-Byte-Präambel von
            // NiSourceTexture enthalten (siehe ParseNiSourceTexture). Folgt danach KEINE
            // weitere NiSourceTexture, muss er hier separat konsumiert werden - sonst
            // verschiebt sich alles Folgende um 8 Byte.
            const bool nextIsSourceTexture =
                (blockIdx + 1 < hdr.blockTypeIndex.size()) &&
                hdr.blockTypes[hdr.blockTypeIndex[blockIdx + 1]] == "NiSourceTexture";
            if (r.LegacyLayout() && !nextIsSourceTexture) {
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
                if (pixelDigit == 1) {
                    r.Skip(4);
                } else if (pixelDigit == 2) {
                    // Variante: nichts ueberspringen
                } else if (looksEmptyAt0) {
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
            if (r.LegacyLayout() && !LooksLikeTriDataHeader(r, 0) && LooksLikeTriDataHeader(r, 4)) {
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
            {
                RawGeom rg;
                rg.positions = raw.vertices;
                rg.normals = raw.normals;
                rg.vertexColors = raw.vertexColors;
                rg.uvs = raw.uvs;
                rg.uvSets = raw.uvSets;
                rg.uvSetDiagnostics = raw.uvSetDiagnostics;
                for (const auto& strip : raw.strips) ExpandTriangleStrip(strip, rg.triangleIndices);
                rawByData[static_cast<std::int32_t>(blockIdx)] = std::move(rg);
            }
            partDataBlock.resize(model.parts.size(), -1);
            partDataBlock[model.parts.size() - 1] = static_cast<std::int32_t>(blockIdx);
            NifMeshPart& part = model.parts.back();
            part.positions = std::move(raw.vertices);
            part.normals = std::move(raw.normals);
            part.vertexColors = std::move(raw.vertexColors);
            part.uvs = std::move(raw.uvs);
            part.uvSets = std::move(raw.uvSets);
            part.uvSetDiagnostics = std::move(raw.uvSetDiagnostics);
            for (const auto& strip : raw.strips) {
                ExpandTriangleStrip(strip, part.triangleIndices);
            }
        } else if (type == "NiTriShapeData") {
            // Gleiche Korrektur wie bei NiTriStripsData (siehe dort, inkl. Abschnitt 42).
            if (r.LegacyLayout() && !LooksLikeTriDataHeader(r, 0) && LooksLikeTriDataHeader(r, 4)) {
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
            {
                RawGeom rg;
                rg.positions = raw.vertices;
                rg.normals = raw.normals;
                rg.vertexColors = raw.vertexColors;
                rg.uvs = raw.uvs;
                rg.uvSets = raw.uvSets;
                rg.uvSetDiagnostics = raw.uvSetDiagnostics;
                rg.triangleIndices.assign(raw.triangleIndices.begin(), raw.triangleIndices.end());
                rawByData[static_cast<std::int32_t>(blockIdx)] = std::move(rg);
            }
            partDataBlock.resize(model.parts.size(), -1);
            partDataBlock[model.parts.size() - 1] = static_cast<std::int32_t>(blockIdx);
            NifMeshPart& part = model.parts.back();
            part.positions = std::move(raw.vertices);
            part.normals = std::move(raw.normals);
            part.vertexColors = std::move(raw.vertexColors);
            part.uvs = std::move(raw.uvs);
            part.uvSets = std::move(raw.uvSets);
            part.uvSetDiagnostics = std::move(raw.uvSetDiagnostics);
            part.triangleIndices.reserve(raw.triangleIndices.size());
            for (const auto idx : raw.triangleIndices) {
                part.triangleIndices.push_back(idx); // bereits flache Dreiecksliste, keine Streifen-Expansion nötig
            }
        } else {
            // Letzte Fallback-Stufe: LoadNifMesh ruft allowPartial erst NACH allen exakten
            // Varianten-/Resync-Versuchen auf. Wenn bereits echte Dreiecksgeometrie gelesen
            // wurde, darf ein spaeter unbekannter Effekt-/Animationsblock nicht mehr das
            // komplette Objekt unsichtbar machen. Der bis hierhin verifizierte Mesh-Anteil
            // wird behalten; der normale (nicht-partielle) Parser bleibt weiterhin strikt.
            const bool haveRenderableGeometry = std::any_of(model.parts.begin(), model.parts.end(),
                [](const NifMeshPart& p) { return !p.positions.empty() && p.triangleIndices.size() >= 3; });
            if (allowPartial && (IsToleratedTailBlock(type) || haveRenderableGeometry)) { partialStop = true; break; }
            return std::unexpected("Nicht unterst\u00fctzter Block-Typ '" + type +
                                    "' bei Block " + std::to_string(blockIdx) +
                                    " - nur einfache untexturierte Meshes werden aktuell unterst\u00fctzt");
        }

        if (!r.Ok()) {
            const bool haveRenderableGeometry = std::any_of(model.parts.begin(), model.parts.end(),
                [](const NifMeshPart& p) { return !p.positions.empty() && p.triangleIndices.size() >= 3; });
            if (allowPartial && (IsToleratedTailBlock(type) || haveRenderableGeometry)) { partialStop = true; break; }
            return std::unexpected("Unerwartetes Dateiende beim Parsen von Block " + std::to_string(blockIdx) + " (" + type + ")");
        }
    }

    if (!r.LegacyLayout() && !partialStop) {
        const auto roots = r.CountU32(hdr.numBlocks);
        for (std::uint32_t i = 0; i < roots; ++i) {
            const auto ref = r.I32();
            if (ref < -1 || (ref >= 0 && static_cast<std::uint32_t>(ref) >= hdr.numBlocks)) r.Invalidate();
        }
        if (!r.Ok() || r.Remaining() != 0)
            return std::unexpected("Ungueltiger NIF-Footer nach Block " + std::to_string(hdr.numBlocks) +
                                   ": " + std::to_string(r.Remaining()) + " Restbytes");
    }
    if (strictEnd && !partialStop && data.size() > r.Pos() && data.size() - r.Pos() > 4u + 4u * 16u) {
        return std::unexpected("Variante verworfen: nach dem letzten Block bleiben " + std::to_string(data.size() - r.Pos()) + " Byte uebrig");
    }
    // KORRIGIERT: Dateien ganz ohne Mesh-Geometrie (nur NiNode/NiCollisionData/Properties,
    // z.B. reine Kollisions-/Ankerpunkt-Objekte wie die "BN" = "Bounding Node"-Familie,
    // siehe bera_BN01.nif/bera_BNset.nif/beraBN.nif) sind strukturell vollständig gültige
    // NIF-Dateien - nur eben ohne sichtbare Geometrie. Bisher wurde das fälschlich als Fehler
    // behandelt. model.parts bleibt einfach leer; NifMeshRenderer iteriert bereits sicher
    // über eine leere parts-Liste (kein Sonderfall nötig).

    // NiTextureEffect is not a NiProperty. It is referenced by NiNode::effects and applies
    // to that node's direct subgraph in the verified classic runtime behavior. Preserve the
    // real node-to-effect bindings here; rendering remains a separate, explicitly gated step.
    {
        for (const auto& node : scene) {
            if (!node.present) continue;
            for (const auto ref : node.effects) {
                if (ref < 0) continue;
                if (textureEffectByBlock.contains(static_cast<std::uint32_t>(ref)))
                    ++model.textureEffectNodeBindings;
            }
        }
    }

    // NiAVObject-Properties sind im NIF-Szenengraph vererbbar. Bisher wurden nur die
    // direkten Property-Refs von NiTriShape/NiTriStrips ausgewertet; dadurch gingen z.B.
    // Texturing/Material/Alpha/Z-Properties verloren, wenn sie auf einem übergeordneten
    // NiNode lagen. Die effektive Liste bleibt bewusst child-first: ein direkt am Mesh
    // gesetzter Property-Typ überschreibt denselben Typ eines Elternknotens.
    {
        std::vector<int> parentOf(scene.size(), -1);
        for (std::size_t i = 0; i < scene.size(); ++i) {
            for (const auto child : scene[i].children) {
                if (child < 0 || static_cast<std::size_t>(child) >= scene.size()) continue;
                if (parentOf[static_cast<std::size_t>(child)] < 0)
                    parentOf[static_cast<std::size_t>(child)] = static_cast<int>(i);
            }
        }

        for (auto& g : geomNodes) {
            std::unordered_set<std::int32_t> seen(g.properties.begin(), g.properties.end());
            int parent = g.block < parentOf.size() ? parentOf[g.block] : -1;
            for (int guard = 0; parent >= 0 && guard < 64; ++guard) {
                const auto parentIndex = static_cast<std::size_t>(parent);
                if (parentIndex >= scene.size()) break;
                for (const auto ref : scene[parentIndex].properties) {
                    if (ref >= 0 && seen.insert(ref).second) {
                        g.properties.push_back(ref);
                        ++model.inheritedPropertyBindings;
                    }
                }
                parent = parentOf[parentIndex];
            }
        }
    }

    // Konsistenzpruefung der Parts; bei Widerspruch aus den Geometrie-Knoten neu aufbauen.
    {
        bool consistent = rawByData.size() == model.parts.size();
        for (const auto& part : model.parts) {
            if (!consistent) break;
            for (const auto idx : part.triangleIndices) {
                if (idx >= part.positions.size()) { consistent = false; break; }
            }
        }
        if (!consistent && !geomNodes.empty()) {
            std::vector<NifMeshPart> rebuilt;
            std::unordered_map<std::size_t, std::int32_t> newTexRef;
            std::vector<std::int32_t> newPartData;
            for (const auto& g : geomNodes) {
                const auto it = rawByData.find(g.dataRef);
                if (it == rawByData.end()) continue;
                NifMeshPart part;
                part.positions = it->second.positions;
                part.normals = it->second.normals;
                part.vertexColors = it->second.vertexColors;
                part.uvs = it->second.uvs;
                part.uvSets = it->second.uvSets;
                part.uvSetDiagnostics = it->second.uvSetDiagnostics;
                part.shaderName = g.shaderName;
                part.triangleIndices = it->second.triangleIndices;
                for (const auto ref : g.properties) {
                    if (ref < 0) continue;
                    const auto mit = materialByBlock.find(static_cast<std::uint32_t>(ref));
                    if (mit != materialByBlock.end()) part.material = mit->second;
                    const auto tit = texStateByBlock.find(static_cast<std::uint32_t>(ref));
                    if (tit != texStateByBlock.end() && tit->second.slots[0].present && tit->second.slots[0].sourceRef >= 0) {
                        const auto& baseSlot = tit->second.slots[0];
                        newTexRef[rebuilt.size()] = baseSlot.sourceRef;
                        part.baseUvSet = baseSlot.uvSet;
                        part.textureClampMode = baseSlot.clampMode;
                        part.textureFilterMode = baseSlot.filterMode;
                        if (part.baseUvSet < part.uvSets.size()) part.uvs = part.uvSets[part.baseUvSet];
                    }
                }
                rebuilt.push_back(std::move(part));
                newPartData.push_back(g.dataRef);
            }
            if (!rebuilt.empty()) {
                model.parts = std::move(rebuilt);
                partBaseTextureRef = std::move(newTexRef);
                partDataBlock = std::move(newPartData);
            }
        }
    }

    // Material follows the same scene-graph inheritance as the other NiProperties.
    // Resolve it authoritatively after any geometry-driven part rebuild, so shared or parent
    // materials do not depend on block adjacency.
    {
        std::unordered_map<std::int32_t, NifMaterial> materialByData;
        for (const auto& g : geomNodes) {
            for (const auto ref : g.properties) {
                if (ref < 0) continue;
                const auto it = materialByBlock.find(static_cast<std::uint32_t>(ref));
                if (it != materialByBlock.end()) {
                    materialByData[g.dataRef] = it->second;
                    break;
                }
            }
        }
        for (std::size_t p = 0; p < model.parts.size() && p < partDataBlock.size(); ++p) {
            const auto it = materialByData.find(partDataBlock[p]);
            if (it != materialByData.end()) model.parts[p].material = it->second;
        }
    }

    // Resolve NiVertexColorProperty through the same effective child-first property chain.
    // Direct geometry state wins over inherited parent state because g.properties keeps that order.
    {
        std::unordered_map<std::int32_t, NifVertexColorState> vertexColorByData;
        for (const auto& g : geomNodes) {
            for (const auto ref : g.properties) {
                if (ref < 0) continue;
                const auto it = vertexColorByBlock.find(static_cast<std::uint32_t>(ref));
                if (it != vertexColorByBlock.end()) {
                    vertexColorByData[g.dataRef] = it->second;
                    break;
                }
            }
        }
        for (std::size_t p = 0; p < model.parts.size() && p < partDataBlock.size(); ++p) {
            const auto it = vertexColorByData.find(partDataBlock[p]);
            if (it == vertexColorByData.end()) continue;
            model.parts[p].hasVertexColorProperty = true;
            model.parts[p].vertexColorMode = it->second.vertexMode;
            model.parts[p].vertexLightingMode = it->second.lightingMode;
        }
    }

    // NiAlphaProperty belongs to geometry through the geometry node's property references,
    // not through block adjacency. Resolve it after all blocks are known so both normal and
    // rebuilt part paths get identical alpha semantics.
    {
        std::unordered_map<std::int32_t, NifAlphaState> alphaByData;
        for (const auto& g : geomNodes) {
            for (const auto ref : g.properties) {
                if (ref < 0) continue;
                const auto ait = alphaByBlock.find(static_cast<std::uint32_t>(ref));
                if (ait != alphaByBlock.end()) { alphaByData[g.dataRef] = ait->second; break; }
            }
        }
        for (std::size_t p = 0; p < model.parts.size() && p < partDataBlock.size(); ++p) {
            const auto ait = alphaByData.find(partDataBlock[p]);
            if (ait == alphaByData.end()) continue;
            model.parts[p].alphaBlend = ait->second.blend;
            model.parts[p].alphaTest = ait->second.test;
            model.parts[p].alphaThreshold = ait->second.threshold;
            model.parts[p].alphaSrcBlend = ait->second.srcBlend;
            model.parts[p].alphaDstBlend = ait->second.dstBlend;
            model.parts[p].alphaTestFunc = ait->second.testFunc;
        }
    }

    // Resolve NiZBufferProperty through the geometry property references. This is not
    // cosmetic state: sky/water/effect meshes frequently rely on read-only depth or a
    // disabled Z test. Applying it per mesh prevents otherwise correctly resolved textures
    // from disappearing behind depth writes that the original NIF explicitly disabled.
    {
        std::unordered_map<std::int32_t, NifZBufferState> zBufferByData;
        for (const auto& g : geomNodes) {
            for (const auto ref : g.properties) {
                if (ref < 0) continue;
                const auto it = zBufferByBlock.find(static_cast<std::uint32_t>(ref));
                if (it != zBufferByBlock.end()) {
                    zBufferByData[g.dataRef] = it->second;
                    break;
                }
            }
        }
        for (std::size_t p = 0; p < model.parts.size() && p < partDataBlock.size(); ++p) {
            const auto it = zBufferByData.find(partDataBlock[p]);
            if (it == zBufferByData.end()) continue;
            model.parts[p].depthTest = it->second.test;
            model.parts[p].depthWrite = it->second.write;
            model.parts[p].depthFunction = it->second.function;
        }
    }

    // Resolve the complete NiStencilProperty through the same effective child-first property
    // chain as the other render states. Direct geometry state therefore wins over inherited
    // parent state, while authored disabled properties remain distinguishable from no property.
    {
        std::unordered_map<std::int32_t, NifStencilState> stencilByData;
        for (const auto& g : geomNodes) {
            for (const auto ref : g.properties) {
                if (ref < 0) continue;
                const auto it = stencilByBlock.find(static_cast<std::uint32_t>(ref));
                if (it != stencilByBlock.end()) { stencilByData[g.dataRef] = it->second; break; }
            }
        }
        for (std::size_t p = 0; p < model.parts.size() && p < partDataBlock.size(); ++p) {
            const auto it = stencilByData.find(partDataBlock[p]);
            if (it == stencilByData.end()) continue;
            auto& part = model.parts[p];
            part.hasStencilProperty = true;
            part.stencilEnabled = it->second.enabled;
            part.stencilFunction = it->second.function;
            part.stencilReference = it->second.reference;
            part.stencilMask = it->second.mask;
            part.stencilFailAction = it->second.failAction;
            part.stencilZFailAction = it->second.zFailAction;
            part.stencilPassAction = it->second.passAction;
            part.faceDrawMode = it->second.drawMode;
        }
    }

    {
        std::unordered_map<std::int32_t, std::string> shaderByData;
        for (const auto& g : geomNodes) shaderByData[g.dataRef] = g.shaderName;
        for (std::size_t p = 0; p < model.parts.size() && p < partDataBlock.size(); ++p) {
            const auto it = shaderByData.find(partDataBlock[p]);
            if (it != shaderByData.end()) model.parts[p].shaderName = it->second;
        }
    }

    // Resolve NiTexturingProperty over the geometry property references. This is the authoritative
    // mapping; adjacency is insufficient for shared properties, LOD and character meshes.
    {
        struct TexBindingState { std::uint32_t block = 0; const NifTextureState* state = nullptr; };
        std::unordered_map<std::int32_t, TexBindingState> texByData;
        for (const auto& g : geomNodes) {
            for (const auto ref : g.properties) {
                if (ref < 0) continue;
                const auto it = texStateByBlock.find(static_cast<std::uint32_t>(ref));
                if (it != texStateByBlock.end()) {
                    texByData[g.dataRef] = {static_cast<std::uint32_t>(ref), &it->second};
                    break;
                }
            }
        }

        const auto makeTrack = [&](const NifSingleControllerState& c) {
            NifFloatTrack t;
            t.active = (c.flags & 0x0008u) != 0;
            t.extrapolation = static_cast<std::uint8_t>((c.flags & 0x0006u) >> 1u);
            t.frequency = c.frequency;
            t.phase = c.phase;
            t.startTime = c.startTime;
            t.stopTime = c.stopTime;
            if (c.interpolatorRef >= 0) {
                const auto ii = floatInterpolatorsByBlock.find(static_cast<std::uint32_t>(c.interpolatorRef));
                if (ii != floatInterpolatorsByBlock.end()) {
                    t.currentValue = ii->second.value;
                    if (ii->second.dataRef >= 0) {
                        const auto di = floatDataByBlock.find(static_cast<std::uint32_t>(ii->second.dataRef));
                        if (di != floatDataByBlock.end()) {
                            t.interpolation = di->second.interpolation;
                            t.keys = di->second.keys;
                        }
                    }
                }
            }
            return t;
        };

        for (std::size_t p = 0; p < model.parts.size() && p < partDataBlock.size(); ++p) {
            const auto it = texByData.find(partDataBlock[p]);
            if (it == texByData.end() || it->second.state == nullptr) continue;
            const auto& ts = *it->second.state;
            const std::uint32_t texPropertyBlock = it->second.block;
            auto& part = model.parts[p];
            part.textureApplyMode = ts.applyMode;
            part.bumpMapLumaScale = ts.bumpMapLumaScale;
            part.bumpMapLumaOffset = ts.bumpMapLumaOffset;
            part.bumpMapMatrix = ts.bumpMapMatrix;

            std::array<std::int32_t, 10> refs{};
            refs.fill(-1);
            for (std::size_t slotIndex = 0; slotIndex < ts.slots.size(); ++slotIndex) {
                const auto& src = ts.slots[slotIndex];
                auto& dst = part.textureSlots[slotIndex];
                dst.present = src.present;
                dst.uvSet = src.uvSet;
                dst.clampMode = src.clampMode;
                dst.filterMode = src.filterMode;
                dst.hasTransform = src.hasTransform;
                dst.translation = src.translation;
                dst.scale = src.scale;
                dst.rotation = src.rotation;
                dst.transformType = src.transformType;
                dst.center = src.center;
                refs[slotIndex] = src.sourceRef;
            }
            // ShaderTexDesc immer verlustfrei am Part erhalten. mapId bleibt shader-spezifisch
            // und wird erst in einem nachweislich bekannten Shaderpfad auf feste Renderer-Slots
            // abgebildet. Dadurch kann der Material-Auditor unbekannte Shaderdaten inventarisieren,
            // ohne ihnen plausible, aber unbewiesene Semantik zu geben.
            part.shaderTextureSlots.clear();
            part.shaderTextureSlots.reserve(ts.shaderSlots.size());
            for (const auto& [mapId, src] : ts.shaderSlots) {
                NifShaderTextureSlot shaderSlot;
                shaderSlot.mapId = mapId;
                shaderSlot.sourceTextureRef = src.sourceRef;
                auto& dst = shaderSlot.texture;
                dst.present = src.present;
                dst.uvSet = src.uvSet;
                dst.clampMode = src.clampMode;
                dst.filterMode = src.filterMode;
                dst.hasTransform = src.hasTransform;
                dst.translation = src.translation;
                dst.scale = src.scale;
                dst.rotation = src.rotation;
                dst.transformType = src.transformType;
                dst.center = src.center;
                part.shaderTextureSlots.push_back(std::move(shaderSlot));
            }

            // Original Gamebryo VCAlphaTextureBlender shader: artist maps
            // 0=Texture1, 1=Texture2, 2=Detail.
            if (part.shaderName == "VCAlphaTextureBlender") {
                for (const auto& [mapId, src] : ts.shaderSlots) {
                    if (mapId > 2u) continue;
                    auto& dst = part.textureSlots[mapId];
                    dst.present = src.present;
                    dst.uvSet = src.uvSet;
                    dst.clampMode = src.clampMode;
                    dst.filterMode = src.filterMode;
                    dst.hasTransform = src.hasTransform;
                    dst.translation = src.translation;
                    dst.scale = src.scale;
                    dst.rotation = src.rotation;
                    dst.transformType = src.transformType;
                    dst.center = src.center;
                    refs[mapId] = src.sourceRef;
                }
            }
            partTextureRefs[p] = refs;
            const auto& base = part.textureSlots[0];
            if (base.present) {
                part.baseUvSet = base.uvSet;
                part.textureClampMode = base.clampMode;
                part.textureFilterMode = base.filterMode;
            }

            // ObjectNET.controller ist der Kopf einer verketteten NiTimeController-Liste.
            // Nur Controller, deren Target wirklich auf diese NiTexturingProperty zeigt,
            // duerfen die Slots dieses Parts beeinflussen.
            std::int32_t controllerRef = ts.controllerRef;
            std::unordered_set<std::int32_t> seenControllers;
            for (int guard = 0; controllerRef >= 0 && guard < 128; ++guard) {
                if (!seenControllers.insert(controllerRef).second) break;
                std::int32_t nextRef = -1;
                const auto tt = texTransformControllersByBlock.find(static_cast<std::uint32_t>(controllerRef));
                if (tt != texTransformControllersByBlock.end()) {
                    nextRef = tt->second.base.nextRef;
                    if (tt->second.base.targetRef == static_cast<std::int32_t>(texPropertyBlock) &&
                        tt->second.operation <= 4u) {
                        NifTextureTransformAnimation a;
                        // NifSkope verwendet texSlot & 7 fuer diese Legacy-Controller.
                        a.slot = tt->second.textureSlot & 7u;
                        a.operation = tt->second.operation;
                        a.track = makeTrack(tt->second.base);
                        part.textureTransformAnimations.push_back(std::move(a));
                    }
                } else {
                    const auto ff = flipControllersByBlock.find(static_cast<std::uint32_t>(controllerRef));
                    if (ff != flipControllersByBlock.end()) {
                        nextRef = ff->second.base.nextRef;
                        if (ff->second.base.targetRef == static_cast<std::int32_t>(texPropertyBlock)) {
                            NifTextureFlipAnimation a;
                            a.slot = ff->second.textureSlot & 7u;
                            a.track = makeTrack(ff->second.base);
                            part.textureFlipAnimations.push_back(std::move(a));
                            partFlipSourceRefs[p].push_back(ff->second.sourceRefs);
                        }
                    } else {
                        // Unbekannter Controller in der Kette: ohne seine Basisstruktur koennen
                        // wir den next_ref nicht sicher ermitteln; sauber abbrechen statt raten.
                        break;
                    }
                }
                controllerRef = nextRef;
            }
        }
    }

    // NiSpecularProperty gates the material's specular term. Missing property means enabled,
    // matching NifSkope's fixed-function material behavior.
    {
        std::unordered_map<std::int32_t, bool> specByData;
        for (const auto& g : geomNodes) {
            for (const auto ref : g.properties) {
                if (ref < 0) continue;
                const auto it = specularByBlock.find(static_cast<std::uint32_t>(ref));
                if (it != specularByBlock.end()) { specByData[g.dataRef] = it->second; break; }
            }
        }
        for (std::size_t p = 0; p < model.parts.size() && p < partDataBlock.size(); ++p) {
            const auto it = specByData.find(partDataBlock[p]);
            if (it != specByData.end()) model.parts[p].specularEnabled = it->second;
        }
    }

    // Base TexDesc may select a UV set other than set 0. Keep all sets in the parsed
    // model and expose the selected set through `uvs` for the current single-texture renderer.
    for (auto& part : model.parts) {
        if (part.baseUvSet < part.uvSets.size()) part.uvs = part.uvSets[part.baseUvSet];
    }

    // Skin-Bone-Refs sind an dieser Stelle noch NIF-Blockindizes. Nach dem Export der
    // Node-Hierarchie werden sie stabil auf NifModel::nodes remapped.
    std::vector<std::vector<std::int32_t>> partSkinBoneSceneRefs(model.parts.size());
    std::vector<std::int32_t> partSkinRootSceneRef(model.parts.size(), -1);

    // Szenengraph anwenden: Weltmatrix je Geometrie = Produkt der lokalen Transformationen von der
    // Wurzel bis zum Geometrieblock. Positionen/Normalen liegen hier schon im Editor-Rahmen
    // (x, legacyZ, legacyY) - die Matrizen gelten im Legacy-Rahmen, daher wird zurueckgetauscht,
    // transformiert und wieder getauscht. Dynamische LOD-/Billboard-Semantik wird parallel als
    // Metadaten am Part erhalten und erst im Renderer ausgewertet.
    {
        std::vector<int> parentOf(scene.size(), -1);
        for (std::size_t i = 0; i < scene.size(); ++i) {
            for (const auto c : scene[i].children) {
                if (c >= 0 && static_cast<std::size_t>(c) < scene.size() && parentOf[static_cast<std::size_t>(c)] < 0) {
                    parentOf[static_cast<std::size_t>(c)] = static_cast<int>(i);
                }
            }
        }

        // Rotationsmatrix zeilenweise (R[0..2] = erste Zeile): empirisch an den Objekt-Hoehen der
        // Karte Adl entschieden (95 % der Objekte stehen dann auf dem Gelaende, transponiert 60 %).
        constexpr bool transposed = false;
        auto applySceneTransform = [&](const SceneNode& sn, NifVec3& v, bool isPoint) {
            const float x = v.x, y = v.z, z = v.y; // zurueck in den Legacy-Rahmen
            const auto& R = sn.rotation;
            float nx, ny, nz;
            if (!transposed) {
                nx = R[0] * x + R[1] * y + R[2] * z;
                ny = R[3] * x + R[4] * y + R[5] * z;
                nz = R[6] * x + R[7] * y + R[8] * z;
            } else {
                nx = R[0] * x + R[3] * y + R[6] * z;
                ny = R[1] * x + R[4] * y + R[7] * z;
                nz = R[2] * x + R[5] * y + R[8] * z;
            }
            if (isPoint) {
                nx = nx * sn.scale + sn.translation.x;
                ny = ny * sn.scale + sn.translation.y;
                nz = nz * sn.scale + sn.translation.z;
            }
            v = {nx, nz, ny};
        };

        auto transformFromNodeToRoot = [&](int nodeIndex, NifVec3 v, bool isPoint) {
            int guard = 0;
            for (int b = nodeIndex; b >= 0 && guard < 64; b = parentOf[static_cast<std::size_t>(b)], ++guard) {
                const SceneNode& sn = scene[static_cast<std::size_t>(b)];
                if (sn.present) applySceneTransform(sn, v, isPoint);
            }
            return v;
        };

        auto billboardInverseRotation = [&](int nodeIndex) {
            NifVec3 ex{1.0f, 0.0f, 0.0f};
            NifVec3 ey{0.0f, 1.0f, 0.0f};
            NifVec3 ez{0.0f, 0.0f, 1.0f};
            int guard = 0;
            for (int b = nodeIndex; b >= 0 && guard < 64; b = parentOf[static_cast<std::size_t>(b)], ++guard) {
                const SceneNode& sn = scene[static_cast<std::size_t>(b)];
                if (!sn.present) continue;
                applySceneTransform(sn, ex, false);
                applySceneTransform(sn, ey, false);
                applySceneTransform(sn, ez, false);
            }
            auto normalize = [](NifVec3 v) {
                const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
                if (len > 1.0e-6f) { v.x /= len; v.y /= len; v.z /= len; }
                return v;
            };
            ex = normalize(ex); ey = normalize(ey); ez = normalize(ez);
            // R hat ex/ey/ez als Spalten. Fuer eine orthonormale Rotation ist R^-1 = R^T.
            return std::array<float, 9>{
                ex.x, ey.x, ez.x,
                ex.y, ey.y, ez.y,
                ex.z, ey.z, ez.z,
            };
        };

        // CPU bind-pose skinning, following NifSkope's NiTriBasedGeom path.  Partitioned
        // meshes use partition-local bone indices -> boneMap -> NiSkinInstance bones; meshes
        // without a partition use the sparse per-bone weights stored in NiSkinData.
        auto multiplyTransform = [](const SkinTransform& a, const SkinTransform& b) {
            SkinTransform out;
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    out.rotation[static_cast<std::size_t>(row * 3 + col)] =
                        a.rotation[static_cast<std::size_t>(row * 3 + 0)] * b.rotation[static_cast<std::size_t>(0 * 3 + col)] +
                        a.rotation[static_cast<std::size_t>(row * 3 + 1)] * b.rotation[static_cast<std::size_t>(1 * 3 + col)] +
                        a.rotation[static_cast<std::size_t>(row * 3 + 2)] * b.rotation[static_cast<std::size_t>(2 * 3 + col)];
                }
            }
            const float bx = b.translation.x, by = b.translation.y, bz = b.translation.z;
            out.translation.x = a.translation.x + a.scale * (a.rotation[0] * bx + a.rotation[1] * by + a.rotation[2] * bz);
            out.translation.y = a.translation.y + a.scale * (a.rotation[3] * bx + a.rotation[4] * by + a.rotation[5] * bz);
            out.translation.z = a.translation.z + a.scale * (a.rotation[6] * bx + a.rotation[7] * by + a.rotation[8] * bz);
            out.scale = a.scale * b.scale;
            return out;
        };
        auto sceneTransform = [](const SceneNode& sn) {
            SkinTransform out;
            out.rotation = sn.rotation;
            out.translation = sn.translation;
            out.scale = sn.scale;
            return out;
        };
        auto publicTransform = [](const SkinTransform& in) {
            NifTransform out;
            out.rotation = in.rotation;
            out.translation = in.translation;
            out.scale = in.scale;
            return out;
        };
        auto relativeBoneTransform = [&](int boneBlock, int skeletonRoot, SkinTransform& out) {
            out = SkinTransform{};
            if (boneBlock < 0 || static_cast<std::size_t>(boneBlock) >= scene.size()) return false;
            int node = boneBlock;
            int guard = 0;
            while (node >= 0 && node != skeletonRoot && guard++ < 128) {
                if (static_cast<std::size_t>(node) >= scene.size() || !scene[static_cast<std::size_t>(node)].present) return false;
                out = multiplyTransform(sceneTransform(scene[static_cast<std::size_t>(node)]), out);
                node = parentOf[static_cast<std::size_t>(node)];
            }
            return skeletonRoot < 0 || node == skeletonRoot;
        };
        auto transformSkinPoint = [](const SkinTransform& t, const NifVec3& editorPoint) {
            // Geometry arrays are already Y-up (x, legacyZ, legacyY); skin transforms are in
            // the original NIF coordinate system.
            const float x = editorPoint.x, y = editorPoint.z, z = editorPoint.y;
            const float nx = (t.rotation[0] * x + t.rotation[1] * y + t.rotation[2] * z) * t.scale + t.translation.x;
            const float ny = (t.rotation[3] * x + t.rotation[4] * y + t.rotation[5] * z) * t.scale + t.translation.y;
            const float nz = (t.rotation[6] * x + t.rotation[7] * y + t.rotation[8] * z) * t.scale + t.translation.z;
            return NifVec3{nx, nz, ny};
        };
        auto transformSkinNormal = [](const SkinTransform& t, const NifVec3& editorNormal) {
            const float x = editorNormal.x, y = editorNormal.z, z = editorNormal.y;
            const float nx = t.rotation[0] * x + t.rotation[1] * y + t.rotation[2] * z;
            const float ny = t.rotation[3] * x + t.rotation[4] * y + t.rotation[5] * z;
            const float nz = t.rotation[6] * x + t.rotation[7] * y + t.rotation[8] * z;
            return NifVec3{nx, nz, ny};
        };
        auto normalizeVec = [](NifVec3& v) {
            const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            if (len > 1.0e-8f) { v.x /= len; v.y /= len; v.z /= len; }
        };

        std::unordered_map<std::int32_t, std::int32_t> skinRefByData;
        for (const auto& g : geomNodes) if (g.dataRef >= 0 && g.skinInstanceRef >= 0) skinRefByData[g.dataRef] = g.skinInstanceRef;

        auto applySkinning = [&](NifMeshPart& part, std::int32_t dataRef, std::size_t partIndex) {
            const auto sr = skinRefByData.find(dataRef);
            if (sr == skinRefByData.end() || sr->second < 0) return;
            const auto siIt = skinInstanceByBlock.find(static_cast<std::uint32_t>(sr->second));
            if (siIt == skinInstanceByBlock.end()) return;
            const SkinInstanceBlock& inst = siIt->second;
            if (inst.dataRef < 0) return;
            const auto sdIt = skinDataByBlock.find(static_cast<std::uint32_t>(inst.dataRef));
            if (sdIt == skinDataByBlock.end()) return;
            const SkinDataBlock& skin = sdIt->second;

            part.skinned = true;
            part.skinBoneCount = static_cast<std::uint16_t>(std::min<std::size_t>(inst.bones.size(), 65535));

            const std::vector<NifVec3> sourcePos = part.positions;
            const std::vector<NifVec3> sourceNorm = part.normals;

            // Preserve exactly the already-verified skin inputs before the bind-pose bake. The
            // normal Map renderer keeps using the baked vertices, while KFM preview can replay
            // the same formula with animated local NIF-node transforms.
            part.skinBinding.emplace();
            auto& exportedSkin = *part.skinBinding;
            exportedSkin.sourcePositions = sourcePos;
            exportedSkin.sourceNormals = sourceNorm;
            exportedSkin.skinTransform = publicTransform(skin.skinTransform);
            exportedSkin.vertexInfluences.resize(sourcePos.size());
            const std::size_t exportedBoneCount = std::min(skin.bones.size(), inst.bones.size());
            exportedSkin.bones.resize(exportedBoneCount);
            partSkinBoneSceneRefs[partIndex].assign(inst.bones.begin(),
                inst.bones.begin() + static_cast<std::ptrdiff_t>(exportedBoneCount));
            partSkinRootSceneRef[partIndex] = inst.skeletonRoot;
            for (std::size_t b = 0; b < exportedBoneCount; ++b)
                exportedSkin.bones[b].bindTransform = publicTransform(skin.bones[b].transform);

            std::vector<NifVec3> accumPos(sourcePos.size());
            std::vector<NifVec3> accumNorm(sourceNorm.size());
            std::vector<float> accumulatedWeight(sourcePos.size(), 0.0f);
            bool usedPartitionWeights = false;

            if (inst.partitionRef >= 0) {
                const auto spIt = skinPartitionByBlock.find(static_cast<std::uint32_t>(inst.partitionRef));
                if (spIt != skinPartitionByBlock.end()) {
                    std::vector<bool> processed(sourcePos.size(), false);
                    for (const auto& partition : spIt->second.partitions) {
                        const std::size_t nw = partition.numWeightsPerVertex;
                        if (nw == 0 || partition.vertexMap.empty()) continue;
                        if (partition.weights.size() < partition.vertexMap.size() * nw ||
                            partition.boneIndices.size() < partition.vertexMap.size() * nw) continue;
                        part.maxSkinInfluences = static_cast<std::uint8_t>(std::max<std::size_t>(part.maxSkinInfluences, std::min<std::size_t>(nw, 255)));
                        for (std::size_t lv = 0; lv < partition.vertexMap.size(); ++lv) {
                            const std::size_t vi = partition.vertexMap[lv];
                            if (vi >= sourcePos.size() || processed[vi]) continue;
                            bool hadInfluence = false;
                            for (std::size_t w = 0; w < nw; ++w) {
                                const std::size_t wi = lv * nw + w;
                                const float weight = partition.weights[wi];
                                if (weight == 0.0f) continue;
                                const std::size_t localBone = partition.boneIndices[wi];
                                if (localBone >= partition.boneMap.size()) continue;
                                const std::size_t globalBone = partition.boneMap[localBone];
                                if (globalBone >= skin.bones.size() || globalBone >= inst.bones.size()) continue;

                                SkinTransform rel;
                                SkinTransform trans; // identity if the bone cannot be resolved, as in NifSkope's partition path
                                if (relativeBoneTransform(inst.bones[globalBone], inst.skeletonRoot, rel)) {
                                    trans = multiplyTransform(rel, skin.bones[globalBone].transform);
                                }
                                if (globalBone < exportedSkin.bones.size())
                                    exportedSkin.vertexInfluences[vi].push_back(
                                        {static_cast<std::uint16_t>(globalBone), weight});
                                const NifVec3 p = transformSkinPoint(trans, sourcePos[vi]);
                                accumPos[vi].x += p.x * weight; accumPos[vi].y += p.y * weight; accumPos[vi].z += p.z * weight;
                                if (vi < sourceNorm.size()) {
                                    const NifVec3 n = transformSkinNormal(trans, sourceNorm[vi]);
                                    accumNorm[vi].x += n.x * weight; accumNorm[vi].y += n.y * weight; accumNorm[vi].z += n.z * weight;
                                }
                                accumulatedWeight[vi] += weight;
                                hadInfluence = true;
                            }
                            if (hadInfluence) { processed[vi] = true; usedPartitionWeights = true; }
                        }
                    }
                }
            }

            // Older/non-partitioned geometry stores sparse weights directly in NiSkinData.
            exportedSkin.partitionWeights = usedPartitionWeights;
            if (!usedPartitionWeights && skin.hasVertexWeights) {
                std::vector<std::uint8_t> influenceCount(sourcePos.size(), 0);
                const std::size_t boneCount = std::min(skin.bones.size(), inst.bones.size());
                for (std::size_t b = 0; b < boneCount; ++b) {
                    SkinTransform trans = skin.skinTransform;
                    SkinTransform rel;
                    if (relativeBoneTransform(inst.bones[b], inst.skeletonRoot, rel)) {
                        trans = multiplyTransform(multiplyTransform(skin.skinTransform, rel), skin.bones[b].transform);
                    }
                    for (const auto& vw : skin.bones[b].weights) {
                        const std::size_t vi = vw.vertex;
                        if (vi >= sourcePos.size() || vw.weight == 0.0f) continue;
                        if (b < exportedSkin.bones.size())
                            exportedSkin.vertexInfluences[vi].push_back(
                                {static_cast<std::uint16_t>(b), vw.weight});
                        const NifVec3 p = transformSkinPoint(trans, sourcePos[vi]);
                        accumPos[vi].x += p.x * vw.weight; accumPos[vi].y += p.y * vw.weight; accumPos[vi].z += p.z * vw.weight;
                        if (vi < sourceNorm.size()) {
                            const NifVec3 n = transformSkinNormal(trans, sourceNorm[vi]);
                            accumNorm[vi].x += n.x * vw.weight; accumNorm[vi].y += n.y * vw.weight; accumNorm[vi].z += n.z * vw.weight;
                        }
                        accumulatedWeight[vi] += vw.weight;
                        if (influenceCount[vi] < 255) ++influenceCount[vi];
                    }
                }
                for (const auto c : influenceCount) part.maxSkinInfluences = std::max(part.maxSkinInfluences, c);
            }

            for (std::size_t i = 0; i < sourcePos.size(); ++i) {
                if (accumulatedWeight[i] > 1.0e-8f) part.positions[i] = accumPos[i];
            }
            for (std::size_t i = 0; i < sourceNorm.size(); ++i) {
                if (i < accumulatedWeight.size() && accumulatedWeight[i] > 1.0e-8f) {
                    normalizeVec(accumNorm[i]);
                    part.normals[i] = accumNorm[i];
                }
            }
        };

        for (std::size_t p = 0; p < model.parts.size(); ++p) {
            if (p >= partDataBlock.size() || partDataBlock[p] < 0) continue;
            const auto geomIt = dataToGeometry.find(partDataBlock[p]);
            if (geomIt == dataToGeometry.end()) continue;
            std::vector<int> chain;
            for (int b = static_cast<int>(geomIt->second); b >= 0 && chain.size() < 64; b = parentOf[static_cast<std::size_t>(b)]) chain.push_back(b);

            NifMeshPart& part = model.parts[p];

            // NiTextureEffect is attached through NiNode::effects rather than the property
            // list. Resolve the effective child->parent chain for this geometry and preserve
            // every distinct authored effect. Rendering may support only a verified subset,
            // but unsupported combinations must remain visible to diagnostics instead of
            // being silently discarded.
            part.textureEffects.clear();
            std::unordered_set<std::int32_t> seenTextureEffects;
            for (const int b : chain) {
                const SceneNode& effectNode = scene[static_cast<std::size_t>(b)];
                for (const auto ref : effectNode.effects) {
                    if (ref < 0 || !seenTextureEffects.insert(ref).second) continue;
                    const auto effectIt = textureEffectByBlock.find(static_cast<std::uint32_t>(ref));
                    if (effectIt == textureEffectByBlock.end()) continue;
                    const auto& src = effectIt->second;
                    NifTextureEffectBinding binding;
                    binding.enabled = src.dynamic.switchState;
                    binding.projectionRotation = src.projectionRotation;
                    binding.projectionPosition = src.projectionPosition;
                    binding.filterMode = src.filterMode;
                    binding.clampMode = src.clampMode;
                    binding.textureType = src.textureType;
                    binding.coordGenType = src.coordGenType;
                    binding.sourceTextureRef = src.sourceTextureRef;
                    binding.clippingPlaneEnabled = src.enablePlane;
                    binding.clippingPlane = src.clipPlane;
                    part.textureEffects.push_back(std::move(binding));
                }
            }

            applySkinning(part, partDataBlock[p], p);

            if (part.skinBinding) {
                SkinTransform meshToModel;
                for (const int b : chain) {
                    const SceneNode& sn = scene[static_cast<std::size_t>(b)];
                    if (!sn.present) continue;
                    meshToModel = multiplyTransform(sceneTransform(sn), meshToModel);
                }
                part.skinBinding->meshToModelTransform = publicTransform(meshToModel);
            }

            // Laufzeit-LOD: das direkte Kind unter dem NiLODNode entspricht demselben Index im
            // Range-Array. NifSkope verwendet near <= distance < far; ohne Range-Daten wird nur
            // das erste Kind sichtbar gehalten.
            for (std::size_t ci = 0; ci + 1 < chain.size(); ++ci) {
                const int childBlock = chain[ci];
                const int parentBlock = chain[ci + 1];
                const SceneNode& parentNode = scene[static_cast<std::size_t>(parentBlock)];
                if (parentNode.lodDataRef < 0) continue;

                std::size_t childIndex = parentNode.children.size();
                for (std::size_t k = 0; k < parentNode.children.size(); ++k) {
                    if (parentNode.children[k] == childBlock) { childIndex = k; break; }
                }
                if (childIndex >= parentNode.children.size()) continue;

                part.lodControlled = true;
                part.lodCenter = transformFromNodeToRoot(parentBlock, {0.0f, 0.0f, 0.0f}, true);
                const auto lodIt = lodRangeByBlock.find(static_cast<std::uint32_t>(parentNode.lodDataRef));
                if (lodIt != lodRangeByBlock.end()) {
                    part.lodCenter = transformFromNodeToRoot(parentBlock, lodIt->second.center, true);
                    if (childIndex < lodIt->second.ranges.size()) {
                        part.lodNear = lodIt->second.ranges[childIndex].first;
                        part.lodFar = lodIt->second.ranges[childIndex].second;
                    } else {
                        part.lodNear = 1.0f;
                        part.lodFar = 0.0f; // bewusst nie sichtbar
                    }
                } else if (childIndex == 0) {
                    part.lodNear = 0.0f;
                    part.lodFar = std::numeric_limits<float>::max();
                } else {
                    part.lodNear = 1.0f;
                    part.lodFar = 0.0f;
                }
                break;
            }

            // Naechster Billboard-Vorfahre gewinnt. Die Vertexdaten werden wie bisher vollstaendig
            // transformiert; Pivot + inverse Billboard-Weltrotation erlauben dem Renderer, genau
            // diesen Orientierungsanteil pro Frame durch die Kameraausrichtung zu ersetzen.
            for (const int b : chain) {
                const SceneNode& sn = scene[static_cast<std::size_t>(b)];
                if (!sn.billboard) continue;
                part.billboard = true;
                part.billboardMode = sn.billboardMode;
                part.billboardPivot = transformFromNodeToRoot(b, {0.0f, 0.0f, 0.0f}, true);
                part.billboardInverseRotation = billboardInverseRotation(b);
                break;
            }

            for (const int b : chain) {
                const SceneNode& sn = scene[static_cast<std::size_t>(b)];
                if (!sn.present) continue;
                for (auto& v : part.positions) applySceneTransform(sn, v, true);
                for (auto& n : part.normals) applySceneTransform(sn, n, false);
            }
        }
    }

    // NIF-Node-Hierarchie für Attachments UND KFM/KF-Skeleton-Preview. Frühere Versionen
    // exportierten nur benannte Nodes mit Kindern und verloren dadurch Parent-/Bind-Information.
    // Jetzt werden alle echten NiNode-artigen Scene-Einträge erhalten (Name ODER Kinder);
    // Geometrieeinträge bleiben draußen, da recordNode() deren Name/Kinder nicht setzt.
    {
        std::vector<int> parentOf2(scene.size(), -1);
        for (std::size_t i = 0; i < scene.size(); ++i)
            for (const auto child : scene[i].children)
                if (child >= 0 && static_cast<std::size_t>(child) < scene.size() &&
                    parentOf2[static_cast<std::size_t>(child)] < 0)
                    parentOf2[static_cast<std::size_t>(child)] = static_cast<int>(i);

        std::vector<int> sceneToModel(scene.size(), -1);
        for (std::size_t i = 0; i < scene.size(); ++i) {
            if (!scene[i].present || (scene[i].name.empty() && scene[i].children.empty())) continue;

            // Weltposition = lokaler Ursprung durch die Kette Knoten -> Wurzel.
            float px = 0.0f, py = 0.0f, pz = 0.0f;
            std::array<float, 9> Rw = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            int guard = 0;
            for (int b = static_cast<int>(i); b >= 0 && guard < 64;
                 b = parentOf2[static_cast<std::size_t>(b)], ++guard) {
                const SceneNode& sn = scene[static_cast<std::size_t>(b)];
                if (!sn.present) continue;
                const auto& R = sn.rotation;
                const float nx = R[0] * px + R[1] * py + R[2] * pz;
                const float ny = R[3] * px + R[4] * py + R[5] * pz;
                const float nz = R[6] * px + R[7] * py + R[8] * pz;
                px = nx * sn.scale + sn.translation.x;
                py = ny * sn.scale + sn.translation.y;
                pz = nz * sn.scale + sn.translation.z;

                std::array<float, 9> M{};
                for (int r2 = 0; r2 < 3; ++r2)
                    for (int c2 = 0; c2 < 3; ++c2)
                        M[static_cast<std::size_t>(r2 * 3 + c2)] =
                            R[static_cast<std::size_t>(r2 * 3)] * Rw[static_cast<std::size_t>(c2)] +
                            R[static_cast<std::size_t>(r2 * 3 + 1)] * Rw[static_cast<std::size_t>(3 + c2)] +
                            R[static_cast<std::size_t>(r2 * 3 + 2)] * Rw[static_cast<std::size_t>(6 + c2)];
                Rw = M;
            }

            NifNodeInfo info;
            info.name = scene[i].name;
            info.localTranslation = scene[i].translation;
            info.localRotation = scene[i].rotation;
            info.localScale = scene[i].scale;
            info.position = {px, pz, py}; // Legacy (x,y,z) -> Editor-Rahmen (x,z,y)
            info.rotation = Rw;
            sceneToModel[i] = static_cast<int>(model.nodes.size());
            model.nodes.push_back(std::move(info));
        }

        // Parent-Index erst nach dem Aufbau setzen. Falls zwischen zwei erfassten Nodes ein
        // nicht erfasster Scene-Eintrag liegt, bis zum nächsten erfassten Vorfahren hochlaufen.
        for (std::size_t sceneIndex = 0; sceneIndex < scene.size(); ++sceneIndex) {
            const int modelIndex = sceneToModel[sceneIndex];
            if (modelIndex < 0) continue;
            int parent = parentOf2[sceneIndex];
            int guard = 0;
            while (parent >= 0 && guard++ < 64) {
                if (static_cast<std::size_t>(parent) < sceneToModel.size() &&
                    sceneToModel[static_cast<std::size_t>(parent)] >= 0) {
                    model.nodes[static_cast<std::size_t>(modelIndex)].parentIndex =
                        sceneToModel[static_cast<std::size_t>(parent)];
                    break;
                }
                parent = static_cast<std::size_t>(parent) < parentOf2.size()
                    ? parentOf2[static_cast<std::size_t>(parent)] : -1;
            }
        }

        // Skin bindings now receive stable exported node indices. A missing mapping is kept as
        // -1 and will make only that influence remain in bind pose during animation preview.
        for (std::size_t partIndex = 0; partIndex < model.parts.size(); ++partIndex) {
            auto& part = model.parts[partIndex];
            if (!part.skinBinding) continue;
            auto& skin = *part.skinBinding;

            const auto rootScene = partSkinRootSceneRef[partIndex];
            if (rootScene >= 0 && static_cast<std::size_t>(rootScene) < sceneToModel.size())
                skin.skeletonRootNodeIndex = sceneToModel[static_cast<std::size_t>(rootScene)];

            const auto& refs = partSkinBoneSceneRefs[partIndex];
            for (std::size_t b = 0; b < skin.bones.size() && b < refs.size(); ++b) {
                const auto sceneRef = refs[b];
                if (sceneRef >= 0 && static_cast<std::size_t>(sceneRef) < sceneToModel.size())
                    skin.bones[b].nodeIndex = sceneToModel[static_cast<std::size_t>(sceneRef)];
            }
        }
    }

    // Palettierte NiPixelData koennen vor ihrem NiPalette-Block stehen. Jetzt, nachdem alle
    // Blocks gelesen sind, werden diese Pending-Texturen aufgeloest und wie alle anderen
    // Embedded-Texturen vertikal in die Renderer-Konvention gedreht.
    for (auto& [pixelBlock, pending] : pendingPalettedPixelTextures) {
        if (pending.paletteRef < 0 || pending.width == 0 || pending.height == 0) continue;
        const auto pit = palettesByBlock.find(static_cast<std::uint32_t>(pending.paletteRef));
        if (pit == palettesByBlock.end()) continue;
        const std::size_t pixelCount = static_cast<std::size_t>(pending.width) * pending.height;
        if (pending.indices.size() < pixelCount) continue;
        auto out = std::make_shared<NifEmbeddedTexture>();
        out->width = pending.width;
        out->height = pending.height;
        out->rgba.resize(pixelCount * 4u);
        for (std::size_t px = 0; px < pixelCount; ++px) {
            const std::size_t pi = static_cast<std::size_t>(pending.indices[px]) * 4u;
            out->rgba[px * 4 + 0] = pit->second.rgba[pi + 0];
            out->rgba[px * 4 + 1] = pit->second.rgba[pi + 1];
            out->rgba[px * 4 + 2] = pit->second.rgba[pi + 2];
            out->rgba[px * 4 + 3] = pit->second.rgba[pi + 3];
        }
        const std::size_t rowBytes = static_cast<std::size_t>(out->width) * 4u;
        std::vector<std::uint8_t> row(rowBytes);
        for (std::uint32_t y = 0; y < out->height / 2; ++y) {
            auto* a = out->rgba.data() + static_cast<std::size_t>(y) * rowBytes;
            auto* b = out->rgba.data() + static_cast<std::size_t>(out->height - 1 - y) * rowBytes;
            std::memcpy(row.data(), a, rowBytes);
            std::memcpy(a, b, rowBytes);
            std::memcpy(b, row.data(), rowBytes);
        }
        embeddedPixelTextures[pixelBlock] = std::move(out);
    }

    // Zweistufige Textur-Aufloesung fuer alle klassischen Slots abschliessen. SourceTexture-
    // Bloecke stehen haeufig erst hinter der Property, daher wird der Dateiname/embedded PixelData
    // bewusst erst jetzt eingetragen.
    for (const auto& [partIdx, refs] : partTextureRefs) {
        if (partIdx >= model.parts.size()) continue;
        auto& part = model.parts[partIdx];
        for (std::size_t slotIndex = 0; slotIndex < refs.size(); ++slotIndex) {
            const auto blockIdx = refs[slotIndex];
            if (blockIdx < 0) continue;
            auto it = sourceTextures.find(static_cast<std::uint32_t>(blockIdx));
            if (it == sourceTextures.end()) continue;
            auto& slot = part.textureSlots[slotIndex];
            slot.texture = it->second.filename;
            slot.sourceUsesEmbeddedPixelData = it->second.useExternal == 0;
            slot.sourcePixelDataRef = it->second.pixelDataRef;
            if (it->second.useExternal == 0 && it->second.pixelDataRef >= 0) {
                auto pix = embeddedPixelTextures.find(static_cast<std::uint32_t>(it->second.pixelDataRef));
                if (pix != embeddedPixelTextures.end()) slot.embeddedTexture = pix->second;
                else std::fprintf(stderr, "[NifModel] NiSourceTexture '%s' verweist auf fehlendes NiPixelData %d\n",
                                  it->second.filename.c_str(), it->second.pixelDataRef);
            }
        }
        // ShaderTexDesc-Quellen separat auflösen. Sie dürfen nicht automatisch in die
        // klassischen Slots gespiegelt werden; nur verifizierte Shaderpfade wie
        // VCAlphaTextureBlender tun das oben explizit.
        for (auto& shaderSlot : part.shaderTextureSlots) {
            if (shaderSlot.sourceTextureRef < 0) continue;
            const auto it = sourceTextures.find(static_cast<std::uint32_t>(shaderSlot.sourceTextureRef));
            if (it == sourceTextures.end()) continue;
            auto& slot = shaderSlot.texture;
            slot.texture = it->second.filename;
            slot.sourceUsesEmbeddedPixelData = it->second.useExternal == 0;
            slot.sourcePixelDataRef = it->second.pixelDataRef;
            if (it->second.useExternal == 0 && it->second.pixelDataRef >= 0) {
                const auto pix = embeddedPixelTextures.find(static_cast<std::uint32_t>(it->second.pixelDataRef));
                if (pix != embeddedPixelTextures.end()) slot.embeddedTexture = pix->second;
                else std::fprintf(stderr,
                                  "[NifModel] ShaderTexDesc map %u source '%s' verweist auf nicht dekodierte NiPixelData %d\n",
                                  shaderSlot.mapId, it->second.filename.c_str(), it->second.pixelDataRef);
            }
        }

        // Kompatibilitaets-Aliase fuer bestehende Aufrufer/Diagnose.
        if (part.textureSlots[0].present) {
            part.diffuseTexture = part.textureSlots[0].texture;
            part.embeddedDiffuseTexture = part.textureSlots[0].embeddedTexture;
        }
    }

    // NiTextureEffect sources are resolved only after every NiSourceTexture/NiPixelData
    // block is known. Embedded effects deliberately never fall back to an external file when
    // their PixelData cannot be decoded; doing so would change the authored NIF semantics.
    for (auto& part : model.parts) {
        for (auto& effect : part.textureEffects) {
            if (effect.sourceTextureRef < 0) continue;
            const auto st = sourceTextures.find(static_cast<std::uint32_t>(effect.sourceTextureRef));
            if (st == sourceTextures.end()) continue;
            effect.texture = st->second.filename;
            effect.sourceUsesEmbeddedPixelData = st->second.useExternal == 0;
            effect.sourcePixelDataRef = st->second.pixelDataRef;
            if (st->second.useExternal == 0 && st->second.pixelDataRef >= 0) {
                const auto pix = embeddedPixelTextures.find(static_cast<std::uint32_t>(st->second.pixelDataRef));
                if (pix != embeddedPixelTextures.end()) effect.embeddedTexture = pix->second;
                else std::fprintf(stderr,
                                  "[NifModel] NiTextureEffect source '%s' verweist auf nicht dekodierte NiPixelData %d\n",
                                  st->second.filename.c_str(), st->second.pixelDataRef);
            }
        }
    }

    // Flipbook-Frames nach Aufloesung aller NiSourceTexture/NiPixelData-Blocks anhaengen.
    // Die Reihenfolge der Source-Refs ist die Animationsreihenfolge des NiFlipController.
    for (const auto& [partIdx, refLists] : partFlipSourceRefs) {
        if (partIdx >= model.parts.size()) continue;
        auto& part = model.parts[partIdx];
        const std::size_t count = std::min(refLists.size(), part.textureFlipAnimations.size());
        for (std::size_t ai = 0; ai < count; ++ai) {
            auto& anim = part.textureFlipAnimations[ai];
            anim.frames.reserve(refLists[ai].size());
            for (const auto ref : refLists[ai]) {
                if (ref < 0) continue;
                const auto st = sourceTextures.find(static_cast<std::uint32_t>(ref));
                if (st == sourceTextures.end()) continue;
                NifTextureFlipFrame frame;
                frame.texture = st->second.filename;
                frame.sourceUsesEmbeddedPixelData = st->second.useExternal == 0;
                frame.sourcePixelDataRef = st->second.pixelDataRef;
                if (st->second.useExternal == 0 && st->second.pixelDataRef >= 0) {
                    const auto pix = embeddedPixelTextures.find(static_cast<std::uint32_t>(st->second.pixelDataRef));
                    if (pix != embeddedPixelTextures.end()) frame.embeddedTexture = pix->second;
                }
                anim.frames.push_back(std::move(frame));
            }
        }
    }

    // Falls ein sehr alter/teilweise rekonstruierter Pfad keine Geometry-Property-Zuordnung
    // lieferte, den bisherigen Base-only-Fallback beibehalten.
    for (const auto& [partIdx, blockIdx] : partBaseTextureRef) {
        if (partIdx >= model.parts.size() || !model.parts[partIdx].diffuseTexture.empty() || blockIdx < 0) continue;
        auto it = sourceTextures.find(static_cast<std::uint32_t>(blockIdx));
        if (it == sourceTextures.end()) continue;
        auto& part = model.parts[partIdx];
        part.diffuseTexture = it->second.filename;
        part.textureSlots[0].present = true;
        part.textureSlots[0].texture = it->second.filename;
        part.textureSlots[0].sourceUsesEmbeddedPixelData = it->second.useExternal == 0;
        part.textureSlots[0].sourcePixelDataRef = it->second.pixelDataRef;
        part.textureSlots[0].uvSet = part.baseUvSet;
        part.textureSlots[0].clampMode = part.textureClampMode;
        part.textureSlots[0].filterMode = part.textureFilterMode;
        if (it->second.useExternal == 0 && it->second.pixelDataRef >= 0) {
            auto pix = embeddedPixelTextures.find(static_cast<std::uint32_t>(it->second.pixelDataRef));
            if (pix != embeddedPixelTextures.end()) {
                part.embeddedDiffuseTexture = pix->second;
                part.textureSlots[0].embeddedTexture = pix->second;
            }
        }
    }

    model.partial = partialStop;
    model.recovered = r.LegacyLayout();
    model.decodedEmbeddedTextures = static_cast<std::uint32_t>(embeddedPixelTextures.size());
    for (const auto typeIndex : hdr.blockTypeIndex)
        if (typeIndex < hdr.blockTypes.size() && hdr.blockTypes[typeIndex] == "NiPixelData") ++model.undecodedEmbeddedTextures;
    model.undecodedEmbeddedTextures -= std::min(model.undecodedEmbeddedTextures, model.decodedEmbeddedTextures);
    return model;
}

} // namespace

NifVec2 ApplyNifTextureTransform(const NifTextureSlot& slot, NifVec2 uv) {
    if (!slot.hasTransform) return uv;

    const float c = std::cos(slot.rotation);
    const float sn = std::sin(slot.rotation);
    const auto rotate = [&](NifVec2 p) {
        return NifVec2{c * p.u - sn * p.v, sn * p.u + c * p.v};
    };
    const auto scale = [&](NifVec2 p) {
        return NifVec2{p.u * slot.scale.u, p.v * slot.scale.v};
    };
    const auto translate = [&](NifVec2 p) {
        return NifVec2{p.u + slot.translation.u, p.v + slot.translation.v};
    };
    const auto back = [&](NifVec2 p) {
        return NifVec2{p.u - slot.center.u, p.v - slot.center.v};
    };
    const auto center = [&](NifVec2 p) {
        return NifVec2{p.u + slot.center.u, p.v + slot.center.v};
    };

    // nif.xml TransformMethod matrix order (column-vector convention):
    // 0 TM_Maya Deprecated: Center * Rotation * Back * Translate * Scale
    // 1 TM_Max:             Center * Scale * Rotation * Translate * Back
    // 2 TM_Maya:            Center * Rotation * Back * FromMaya * Translate * Scale
    // FromMaya flips V and translates it by +1 => (u, 1-v).
    switch (slot.transformType) {
        case kNifTextureTransformMax: {
            NifVec2 p = back(uv);
            p = translate(p);
            p = rotate(p);
            p = scale(p);
            return center(p);
        }
        case kNifTextureTransformMaya: {
            NifVec2 p = scale(uv);
            p = translate(p);
            p.v = 1.0f - p.v;
            p = back(p);
            p = rotate(p);
            return center(p);
        }
        case kNifTextureTransformMayaDeprecated:
        default: {
            NifVec2 p = scale(uv);
            p = translate(p);
            p = back(p);
            p = rotate(p);
            return center(p);
        }
    }
}

std::expected<NifModel, std::string> LoadNifMesh(const std::filesystem::path& file, bool allowRecovery) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) {
        return std::unexpected("Konnte NIF-Datei nicht \u00f6ffnen: " + file.string());
    }
    const auto size = in.tellg();
    if (size < 0 || size > 256 * 1024 * 1024)
        return std::unexpected("NIF-Dateigroesse ungueltig oder groesser als 256 MiB");
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
        return std::unexpected("NIF-Datei konnte nicht vollstaendig gelesen werden");

    std::uint32_t materialCount = 0, pixelCount = 0;
    auto result = LoadNifMeshData(data, 0, 0, false, &materialCount, &pixelCount, nullptr, false, false, false);
    if (result || !allowRecovery) return result;
    // Standardregeln scheitern: zwei Stellen sind bei diesen Dateien mehrdeutig -
    //  (a) NiMaterialProperty hat mal 14, mal 15 Floats (Regel "15 nur ohne NiTexturingProperty"
    //      gilt nicht fuer alle Dateien),
    //  (b) nach NiPixelData fehlen/ueberzaehlen 4 Byte, wenn keine NiSourceTexture folgt.
    // Beide Varianten werden durchprobiert (erst je Dimension, dann kombiniert mit wenigen
    // Abweichungen); akzeptiert wird die erste, die fehlerfrei bis (fast) zum Dateiende parst -
    // das ist eine harte Pruefsumme. Die Suche laeuft ohne Textur-Dekodierung, der Treffer wird
    // anschliessend vollstaendig neu geladen (CHANGELOG [0.44.28]).
    const std::string firstError = result.error();
    // v11 compatibility belongs to recovery, never to the standard-format scan.
    auto legacy = LoadNifMeshData(data, 0, 0, false, &materialCount, &pixelCount);
    if (legacy) return legacy;
    {
        // Stufe 0: Namens-Resynchronisation (ein einziger Lauf, ohne Suche).
        auto resynced = LoadNifMeshData(data, 0, 0, true, nullptr, nullptr, nullptr, false, true);
        if (resynced) return resynced;
    }
    const std::uint32_t mBits = std::min<std::uint32_t>(materialCount, 6u);
    const std::uint32_t pDigits = std::min<std::uint32_t>(pixelCount, 4u);
    std::uint32_t pixelCombos = 1;
    for (std::uint32_t k = 0; k < pDigits; ++k) pixelCombos *= 3u;
    auto popcount = [](std::uint32_t v) { std::uint32_t c = 0; while (v) { c += v & 1u; v >>= 1u; } return c; };
    auto changedDigits = [&](std::uint32_t pv) { std::uint32_t c = 0; while (pv) { if (pv % 3u) ++c; pv /= 3u; } return c; };
    struct ProbeStateGuard {
        bool previous = g_probeNoDecode;
        ~ProbeStateGuard() { g_probeNoDecode = previous; }
    } probeStateGuard;
    g_probeNoDecode = true;
    // Such-Budget: jeder Probelauf parst die GANZE Datei - bei grossen Dateien (Mauern/Tuerme bis 10 MB)
    // muss die Zahl der Laeufe klein bleiben, sonst dauert das Laden einer einzigen Datei Minuten.
    // ~150 MB Parse-Volumen je Datei, mindestens 12 und hoechstens 600 Probelaeufe.
    int budget = static_cast<int>(std::clamp<std::size_t>(1'200'000'000u / std::max<std::size_t>(data.size(), 1u), 200u, 12000u));
    std::optional<std::pair<std::uint32_t, std::uint32_t>> found;
    // Jede Stufe bekommt ein EIGENES Budget - die Varianten-Stufe (Material x Pixel-Trailer) hat bis zu
    // ~1500 Kombinationen und darf der Verschiebungs-Stufe nichts wegnehmen (Teva/Pillar_B.nif).
    int variantBudget = budget / 2;
    // Zusaetzlich ein Zeitlimit fuer die GESAMTE Suche (grosse Dateien mit teuren Probelaeufen).
    const auto searchDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    auto timeUp = [&]() { return std::chrono::steady_clock::now() > searchDeadline; };
    for (std::uint32_t pv = 0; pv < pixelCombos && !found && variantBudget > 0 && !timeUp(); ++pv) {
        for (std::uint32_t mask = 0; mask < (1u << mBits) && !found && variantBudget > 0 && !timeUp(); ++mask) {
            if (pv == 0 && mask == 0) continue;               // Standardlauf ist bereits gescheitert
            if (popcount(mask) + changedDigits(pv) > 3) continue; // hoechstens 3 Abweichungen vom Standard
            --variantBudget;
            auto probe = LoadNifMeshData(data, mask, pv, true, nullptr, nullptr);
            if (probe) found = std::make_pair(mask, pv);
        }
    }
    // (c) Positions-Verschiebungen an Blockgrenzen kurz VOR der Fehlerstelle (allgemeiner Fall:
    //     "ein vorheriger Block hat 4 Byte zu viel/zu wenig gelesen", z.B. benannte
    //     NiMaterialProperty nach NiSourceTexture). Gesucht wird mit der Fehlerposition als
    //     Fortschrittsmass (Tiefensuche, hoechstens 3 Verschiebungen).
    std::vector<std::pair<std::uint32_t, int>> shiftFound;
    bool shiftOk = false;
    if (!found) {
        auto failIndexOf = [](const std::string& err) -> long long {
            const auto pos = err.find("Block ");
            if (pos == std::string::npos) return -1;
            return std::atoll(err.c_str() + pos + 6);
        };
        std::vector<std::pair<std::uint32_t, int>> current;
        // Tiefensuche wie im urspruenglichen Verfahren (findet die Loesung meist schon nach wenigen
        // Probelaeufen - eine Breitensuche kostete ein Vielfaches), aber mit Probe-Budget und Zeitlimit.
        std::function<bool(long long, int)> dfs = [&](long long failAt, int depth) -> bool {
            if (depth >= 3 || failAt < 0) return false;
            const long long lo = std::max<long long>(1, failAt - 5);
            for (long long b = failAt; b >= lo; --b) {
                for (const int shift : {-4, 4, -8, 8, -12, 12, -16, 16}) {
                    if (budget <= 0 || timeUp()) return false;
                    --budget;
                    current.emplace_back(static_cast<std::uint32_t>(b), shift);
                    auto probe = LoadNifMeshData(data, 0, 0, true, nullptr, nullptr, &current);
                    if (probe) return true; // `current` bleibt als Loesung stehen
                    const long long nextFail = failIndexOf(probe.error());
                    if (nextFail > failAt && dfs(nextFail, depth + 1)) return true;
                    current.pop_back();
                }
            }
            return false;
        };
        shiftOk = dfs(failIndexOf(firstError), 0);
        if (shiftOk) shiftFound = current;
    }
    g_probeNoDecode = false;
    if (found) {
        auto full = LoadNifMeshData(data, found->first, found->second, true, nullptr, nullptr);
        if (full) return full;
    }
    if (shiftOk) {
        auto full = LoadNifMeshData(data, 0, 0, true, nullptr, nullptr, &shiftFound);
        if (full) return full;
    }
    // Letzte Stufe: Teilmodell (Fehler nur in Animation/Partikel/Kollision NACH der Geometrie).
    {
        auto partial = LoadNifMeshData(data, 0, 0, false, nullptr, nullptr, nullptr, true);
        if (partial) {
            bool hasTriangles = false;
            for (const auto& part : partial->parts) if (!part.triangleIndices.empty()) hasTriangles = true;
            if (hasTriangles) return partial;
        }
    }
    return std::unexpected(firstError);
}


std::vector<NifGroundContactSegment> ComputeGroundContactSegments(const NifModel& model) {
    float minY = 0.0f, maxY = 0.0f;
    bool any = false;
    for (const auto& part : model.parts) {
        for (const auto& v : part.positions) {
            if (!any) {
                minY = maxY = v.y;
                any = true;
            } else {
                minY = std::min(minY, v.y);
                maxY = std::max(maxY, v.y);
            }
        }
    }
    if (!any) return {};

    // Placement-NIFs are authored around their local origin. When the geometry spans y=0,
    // use that plane: it correctly ignores below-ground foundations/decorations and matches
    // the plane that is placed on the terrain. Models whose geometry does not span y=0 use
    // their actual lowest plane instead of assuming a pivot convention they do not follow.
    const float height = std::max(0.0f, maxY - minY);
    const float eps = std::clamp(height * 0.0015f, 0.05f, 2.0f);
    const float planeY = (minY <= eps && maxY >= -eps) ? 0.0f : minY;

    struct Point2 {
        float x = 0.0f;
        float z = 0.0f;
    };
    struct PointKey {
        std::int64_t x = 0;
        std::int64_t z = 0;
        bool operator==(const PointKey&) const = default;
    };
    struct EdgeKey {
        PointKey a{};
        PointKey b{};
        bool operator==(const EdgeKey&) const = default;
    };
    struct EdgeHash {
        std::size_t operator()(const EdgeKey& e) const noexcept {
            auto mix = [](std::uint64_t v) {
                v ^= v >> 33;
                v *= 0xff51afd7ed558ccdULL;
                v ^= v >> 33;
                v *= 0xc4ceb9fe1a85ec53ULL;
                v ^= v >> 33;
                return v;
            };
            const auto ax = mix(static_cast<std::uint64_t>(e.a.x));
            const auto az = mix(static_cast<std::uint64_t>(e.a.z));
            const auto bx = mix(static_cast<std::uint64_t>(e.b.x));
            const auto bz = mix(static_cast<std::uint64_t>(e.b.z));
            return static_cast<std::size_t>(ax ^ (az << 1) ^ (bx << 2) ^ (bz << 3));
        }
    };

    // Millimetre-ish quantization in Fiesta model units. It is only used to identify the
    // same authored edge across adjacent triangles; returned coordinates remain untouched.
    constexpr double kQuantize = 1000.0;
    auto pointKey = [](const Point2& p) {
        return PointKey{
            static_cast<std::int64_t>(std::llround(static_cast<double>(p.x) * kQuantize)),
            static_cast<std::int64_t>(std::llround(static_cast<double>(p.z) * kQuantize))
        };
    };
    auto edgeKey = [&](Point2 a, Point2 b) {
        PointKey ka = pointKey(a), kb = pointKey(b);
        if (kb.x < ka.x || (kb.x == ka.x && kb.z < ka.z)) std::swap(ka, kb);
        return EdgeKey{ka, kb};
    };
    auto validSegment = [](const Point2& a, const Point2& b) {
        const float dx = b.x - a.x;
        const float dz = b.z - a.z;
        return dx * dx + dz * dz > 1.0e-8f;
    };

    std::unordered_map<EdgeKey, NifGroundContactSegment, EdgeHash> uniqueSegments;
    auto keepSegment = [&](const Point2& a, const Point2& b) {
        if (!validSegment(a, b)) return;
        const EdgeKey key = edgeKey(a, b);
        uniqueSegments.try_emplace(key, NifGroundContactSegment{a.x, a.z, b.x, b.z});
    };

    auto side = [&](float y) {
        const float d = y - planeY;
        if (d > eps) return 1;
        if (d < -eps) return -1;
        return 0;
    };

    for (const auto& part : model.parts) {
        if (part.positions.empty() || part.triangleIndices.size() < 3) continue;

        // Coplanar floor triangles need special treatment: count their edges within the mesh
        // part and keep only boundary edges. Otherwise every triangulation diagonal would be
        // visible in the 2D editor.
        struct CountedEdge {
            Point2 a{};
            Point2 b{};
            std::uint32_t count = 0;
        };
        std::unordered_map<EdgeKey, CountedEdge, EdgeHash> coplanarEdges;
        auto countCoplanarEdge = [&](const Point2& a, const Point2& b) {
            if (!validSegment(a, b)) return;
            const EdgeKey key = edgeKey(a, b);
            auto [it, inserted] = coplanarEdges.try_emplace(key, CountedEdge{a, b, 0});
            ++it->second.count;
        };

        for (std::size_t ti = 0; ti + 2 < part.triangleIndices.size(); ti += 3) {
            const auto ia = part.triangleIndices[ti + 0];
            const auto ib = part.triangleIndices[ti + 1];
            const auto ic = part.triangleIndices[ti + 2];
            if (ia >= part.positions.size() || ib >= part.positions.size() || ic >= part.positions.size())
                continue;

            const auto& a3 = part.positions[ia];
            const auto& b3 = part.positions[ib];
            const auto& c3 = part.positions[ic];
            const int sa = side(a3.y), sb = side(b3.y), sc = side(c3.y);
            const Point2 a{a3.x, a3.z}, b{b3.x, b3.z}, c{c3.x, c3.z};

            if (sa == 0 && sb == 0 && sc == 0) {
                countCoplanarEdge(a, b);
                countCoplanarEdge(b, c);
                countCoplanarEdge(c, a);
                continue;
            }

            std::vector<Point2> hits;
            hits.reserve(4);
            auto appendUnique = [&](const Point2& p) {
                const PointKey key = pointKey(p);
                for (const auto& existing : hits)
                    if (pointKey(existing) == key) return;
                hits.push_back(p);
            };
            auto intersectEdge = [&](const core::NifVec3& p0, int s0,
                                     const core::NifVec3& p1, int s1) {
                const Point2 q0{p0.x, p0.z};
                const Point2 q1{p1.x, p1.z};
                if (s0 == 0 && s1 == 0) {
                    keepSegment(q0, q1);
                    appendUnique(q0);
                    appendUnique(q1);
                    return;
                }
                if (s0 == 0) {
                    appendUnique(q0);
                    return;
                }
                if (s1 == 0) {
                    appendUnique(q1);
                    return;
                }
                if (s0 == s1) return;
                const float denom = p1.y - p0.y;
                if (std::abs(denom) <= 1.0e-8f) return;
                const float t = std::clamp((planeY - p0.y) / denom, 0.0f, 1.0f);
                appendUnique(Point2{
                    p0.x + (p1.x - p0.x) * t,
                    p0.z + (p1.z - p0.z) * t
                });
            };

            intersectEdge(a3, sa, b3, sb);
            intersectEdge(b3, sb, c3, sc);
            intersectEdge(c3, sc, a3, sa);

            if (hits.size() >= 2) {
                // Tolerance can occasionally classify all three edges as touching. Use the
                // farthest pair; this avoids a tiny spurious segment around a near-coplanar
                // vertex while preserving the actual plane/triangle intersection.
                std::size_t bestA = 0, bestB = 1;
                float bestD2 = -1.0f;
                for (std::size_t i = 0; i < hits.size(); ++i) {
                    for (std::size_t j = i + 1; j < hits.size(); ++j) {
                        const float dx = hits[j].x - hits[i].x;
                        const float dz = hits[j].z - hits[i].z;
                        const float d2 = dx * dx + dz * dz;
                        if (d2 > bestD2) {
                            bestD2 = d2;
                            bestA = i;
                            bestB = j;
                        }
                    }
                }
                keepSegment(hits[bestA], hits[bestB]);
            }
        }

        for (const auto& [key, edge] : coplanarEdges) {
            (void)key;
            // Two coplanar triangles share an interior edge. Odd count is retained instead
            // of requiring exactly one so duplicated triangles cannot erase a real boundary.
            if ((edge.count & 1u) != 0u) keepSegment(edge.a, edge.b);
        }
    }

    std::vector<NifGroundContactSegment> result;
    result.reserve(uniqueSegments.size());
    for (const auto& [key, segment] : uniqueSegments) {
        (void)key;
        result.push_back(segment);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.x0 != b.x0) return a.x0 < b.x0;
        if (a.z0 != b.z0) return a.z0 < b.z0;
        if (a.x1 != b.x1) return a.x1 < b.x1;
        return a.z1 < b.z1;
    });
    return result;
}

std::vector<std::pair<float, float>> ComputeFootprintHull(const NifModel& model) {
    float minY = 0.0f, maxY = 0.0f;
    bool any = false;
    for (const auto& part : model.parts) {
        for (const auto& v : part.positions) {
            if (!any) { minY = maxY = v.y; any = true; }
            else { minY = std::min(minY, v.y); maxY = std::max(maxY, v.y); }
        }
    }
    if (!any) return {};
    const float band = std::clamp((maxY - minY) * 0.12f, 20.0f, 120.0f);
    std::vector<std::pair<float, float>> pts;
    for (const auto& part : model.parts) {
        for (const auto& v : part.positions) {
            if (v.y <= minY + band) pts.emplace_back(v.x, v.z);
        }
    }
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    if (pts.size() < 3) return pts;
    // Andrew's monotone chain (gegen den Uhrzeigersinn, im x/z-Koordinatensystem).
    auto cross = [](const std::pair<float, float>& o, const std::pair<float, float>& a, const std::pair<float, float>& b) {
        return (a.first - o.first) * (b.second - o.second) - (a.second - o.second) * (b.first - o.first);
    };
    std::vector<std::pair<float, float>> hull(2 * pts.size());
    std::size_t k = 0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) --k;
        hull[k++] = pts[i];
    }
    for (std::size_t i = pts.size() - 1, t = k + 1; i > 0; --i) {
        while (k >= t && cross(hull[k - 2], hull[k - 1], pts[i - 1]) <= 0.0f) --k;
        hull[k++] = pts[i - 1];
    }
    hull.resize(k - 1);
    return hull;
}

} // namespace theseed::mapeditor::core
