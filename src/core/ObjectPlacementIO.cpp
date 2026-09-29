#include "mapeditor/core/ObjectPlacementIO.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <charconv>
#include <unordered_map>

namespace theseed::mapeditor::core::legacy {
namespace {
std::string CanonicalShmd(const ObjectPlacementSet& set);
bool ReadFloat(std::istream& in, float& value) {
    std::string token;
    if (!(in >> token)) return false;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    return error == std::errc{} && end == token.data() + token.size();
}
} // namespace

std::expected<ObjectPlacementSet, std::string> ParseLegacyShmd(const std::filesystem::path& file) {
    std::ifstream source(file, std::ios::binary);
    std::string original((std::istreambuf_iterator<char>(source)), {});
    std::istringstream in(original);
    in.imbue(std::locale::classic());
    if (!source) {
        return std::unexpected("Konnte .shmd nicht \u00f6ffnen: " + file.string());
    }

    ObjectPlacementSet set;

    std::string formatVersion;
    if (!(in >> formatVersion)) {
        return std::unexpected("Leere/ung\u00fcltige .shmd-Datei: " + file.string());
    }

    if (formatVersion != "shmd0_5") return std::unexpected("Unbekannte SHMD-Version: " + formatVersion);

    // Kategorie-Bl\u00f6cke (generisch, nicht auf "Sky"/"Water"/"GroundObject" hartkodiert) bis
    // zum Token "GlobalLight".
    std::string token;
    while (in >> token) {
        if (token == "GlobalLight") break;
        ObjectCategoryList category;
        category.name = token;
        std::size_t pathCount = 0;
        if (!(in >> pathCount)) {
            return std::unexpected("Fehler beim Lesen der Kategorie-Anzahl f\u00fcr '" + token + "': " + file.string());
        }
        for (std::size_t i = 0; i < pathCount; ++i) {
            std::string path;
            if (!(in >> path)) {
                return std::unexpected("Unerwartetes Dateiende in Kategorie '" + token + "': " + file.string());
            }
            category.modelPaths.push_back(std::move(path));
        }
        set.categories.push_back(std::move(category));
    }
    if (!in) {
        return std::unexpected("Token 'GlobalLight' nicht gefunden: " + file.string());
    }

    auto& env = set.environment;
    if (!(in >> env.globalLight[0] >> env.globalLight[1] >> env.globalLight[2])) {
        return std::unexpected("Fehler beim Lesen von GlobalLight: " + file.string());
    }
    std::string kw;
    if (!(in >> kw) || kw != "Fog" || !(in >> env.fog[0] >> env.fog[1] >> env.fog[2] >> env.fog[3])) {
        return std::unexpected("Fehler beim Lesen von Fog: " + file.string());
    }
    if (!(in >> kw) || kw != "BackGroundColor" ||
        !(in >> env.backgroundColor[0] >> env.backgroundColor[1] >> env.backgroundColor[2])) {
        return std::unexpected("Fehler beim Lesen von BackGroundColor: " + file.string());
    }
    if (!(in >> kw) || kw != "Frustum" || !(in >> env.frustumFar)) {
        return std::unexpected("Fehler beim Lesen von Frustum: " + file.string());
    }

    // Objekt-Bl\u00f6cke bis "DataObjectLoadingEnd".
    while (in >> token) {
        if (token == "DataObjectLoadingEnd") break;
        const std::string modelPath = token;
        std::size_t instanceCount = 0;
        if (!(in >> instanceCount)) {
            return std::unexpected("Fehler beim Lesen der Instanz-Anzahl f\u00fcr '" + modelPath + "': " + file.string());
        }
        for (std::size_t i = 0; i < instanceCount; ++i) {
            float legacyX = 0, legacyY = 0, legacyZ = 0;
            float legacyRotX = 0, legacyRotY = 0, legacyRotZ = 0, legacyRotW = 1;
            float scale = 1;
            if (!ReadFloat(in, legacyX) || !ReadFloat(in, legacyY) || !ReadFloat(in, legacyZ) ||
                !ReadFloat(in, legacyRotX) || !ReadFloat(in, legacyRotY) || !ReadFloat(in, legacyRotZ) ||
                !ReadFloat(in, legacyRotW) || !ReadFloat(in, scale)) {
                return std::unexpected("Unerwartetes Dateiende in Instanzdaten von '" + modelPath + "': " + file.string());
            }
            PlacedObject obj;
            obj.modelPath = modelPath;
            // Achsen-Remap: Legacy ist Z-up (X/Y = horizontale Ebene, Z = Höhe, per echtem
            // Heightmap-Abgleich verifiziert), intern durchgängig Y-up wie das Heightmap-Modul
            // (X/Z = horizontale Ebene, Y = Höhe). Reines Vertauschen, keine Berechnung -> beim
            // Export exakt umkehrbar, Byte-Exaktheit bleibt erhalten.
            obj.posX = legacyX;
            obj.posY = legacyZ;
            obj.posZ = legacyY;
            // Rotation: die Achsenvertauschung (x,y,z)->(x,z,y) ist eine SPIEGELUNG (det -1), keine
            // Drehung. Ein Quaternion muss dabei konjugiert werden: Vektoranteil (x,y,z) ->
            // -(x,z,y), w bleibt (P*R(a,theta)*P = R(-P*a, theta)). Ohne die Vorzeichen drehte sich
            // jedes Objekt in die falsche Richtung (Yaw +theta statt -theta) - "Objekte links/
            // rechts vertauscht", siehe CHANGELOG [0.44.27]. Reines Negieren ist exakt umkehrbar
            // (auch fuer -0.0), die Byte-Exaktheit des Exports bleibt erhalten.
            obj.rotX = -legacyRotX;
            obj.rotY = -legacyRotZ;
            obj.rotZ = -legacyRotY;
            obj.rotW = legacyRotW;
            obj.scale = scale;
            set.AddObject(std::move(obj));
        }
    }
    // Some supplied shmd0_5 maps end on a complete object boundary (or directly
    // after Frustum). Preserve this footer-less variant; an incomplete record
    // still fails above and is never treated as a complete map.
    if (in.eof() && token != "DataObjectLoadingEnd") {
        set.hasLightingFooter = false;
        set.originalText = std::move(original);
        set.originalCanonical = CanonicalShmd(set);
        return set;
    }
    if (!in) return std::unexpected("SHMD-Lesefehler: " + file.string());

    if (!(in >> kw) || kw != "DirectionLightAmbient" ||
        !(in >> env.directionLightAmbient[0] >> env.directionLightAmbient[1] >> env.directionLightAmbient[2])) {
        return std::unexpected("Fehler beim Lesen von DirectionLightAmbient: " + file.string());
    }
    if (!(in >> kw) || kw != "DirectionLightDiffuse" ||
        !(in >> env.directionLightDiffuse[0] >> env.directionLightDiffuse[1] >> env.directionLightDiffuse[2])) {
        return std::unexpected("Fehler beim Lesen von DirectionLightDiffuse: " + file.string());
    }

    set.originalText = std::move(original);
    set.originalCanonical = CanonicalShmd(set);
    return set;
}

namespace {

// %.6f wie im Original (verifiziert per Byte-Vergleich) - ABER mit manuellem
// Runden (round-half-away-from-zero via std::llround) statt snprintf("%.6f", ...):
// snprintf/glibc rundet exakte Ties an der 6. Nachkommastelle "round-half-to-even"
// (z.B. 482.3203125 -> "482.320312"), das Original-Tool aber offenbar "round-half-away-
// from-zero" (-> "482.320313", typisches MSVC-CRT-Verhalten). Ohne diese manuelle Rundung
// wich der Export in ca. 60 von 1580 Zeilen im letzten Nachkommadigit vom Original ab.
std::string FormatFloat6(float value) {
    if (std::isnan(value)) return std::signbit(value) ? "-nan(ind)" : "nan";
    if (std::isinf(value)) return std::signbit(value) ? "-inf" : "inf";
    if (std::abs(static_cast<double>(value)) > 9e12) {
        char text[64];
        const auto [end, ec] = std::to_chars(text, text + sizeof(text), value);
        return ec == std::errc{} ? std::string(text, end) : "0";
    }
    const double d = static_cast<double>(value);
    const bool negative = std::signbit(d);
    const double absVal = negative ? -d : d;
    const long long scaled = std::llround(absVal * 1000000.0);
    const long long intPart = scaled / 1000000;
    const long long fracPart = scaled % 1000000;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s%lld.%06lld", negative ? "-" : "", intPart, fracPart);
    return std::string(buf);
}

// Schreibt eine "Zeile" aus beliebig vielen bereits formatierten Tokens: jedes Token gefolgt von
// einem Leerzeichen, danach CRLF - exakt das im Original beobachtete Muster.
void WriteRecordLine(std::ostream& out, const std::vector<std::string>& tokens) {
    for (const auto& t : tokens) {
        out << t << " ";
    }
    out << "\r\n";
}

} // namespace

namespace {
std::string CanonicalShmd(const ObjectPlacementSet& set) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    WriteRecordLine(out, {"shmd0_5"});

    for (const auto& category : set.categories) {
        WriteRecordLine(out, {category.name, std::to_string(category.modelPaths.size())});
        for (const auto& path : category.modelPaths) {
            WriteRecordLine(out, {path});
        }
    }

    const auto& env = set.environment;
    WriteRecordLine(out, {"GlobalLight", FormatFloat6(env.globalLight[0]), FormatFloat6(env.globalLight[1]), FormatFloat6(env.globalLight[2])});
    WriteRecordLine(out, {"Fog", FormatFloat6(env.fog[0]), FormatFloat6(env.fog[1]), FormatFloat6(env.fog[2]), FormatFloat6(env.fog[3])});
    WriteRecordLine(out, {"BackGroundColor", FormatFloat6(env.backgroundColor[0]), FormatFloat6(env.backgroundColor[1]), FormatFloat6(env.backgroundColor[2])});
    WriteRecordLine(out, {"Frustum", FormatFloat6(env.frustumFar)});

    // Instanzen nach Modellpfad gruppieren, Reihenfolge = erstes Auftreten (rekonstruiert die
    // Original-Blockstruktur exakt, solange die Objektliste nicht umsortiert wurde).
    std::vector<std::string> blockOrder;
    std::vector<std::vector<const PlacedObject*>> blockObjects;
    std::unordered_map<std::string, std::size_t> groupIndex;
    for (const auto& obj : set.Objects()) {
        const auto [it, inserted] = groupIndex.try_emplace(obj.modelPath, blockOrder.size());
        if (inserted) { blockOrder.push_back(obj.modelPath); blockObjects.emplace_back(); }
        blockObjects[it->second].push_back(&obj);
    }

    for (std::size_t i = 0; i < blockOrder.size(); ++i) {
        WriteRecordLine(out, {blockOrder[i], std::to_string(blockObjects[i].size())});
        for (const auto* obj : blockObjects[i]) {
            // Inverses Achsen-Remap zu ParseLegacyShmd: intern Y-up -> Legacy Z-up.
            WriteRecordLine(out, {
                FormatFloat6(obj->posX), FormatFloat6(obj->posZ), FormatFloat6(obj->posY),
                FormatFloat6(-obj->rotX), FormatFloat6(-obj->rotZ), FormatFloat6(-obj->rotY), FormatFloat6(obj->rotW),
                FormatFloat6(obj->scale),
            });
        }
    }

    if (set.hasLightingFooter) {
    out << "DataObjectLoadingEnd\r\n"; // Sonderfall: KEIN trailing space (siehe Header-Kommentar)

    WriteRecordLine(out, {"DirectionLightAmbient", FormatFloat6(env.directionLightAmbient[0]), FormatFloat6(env.directionLightAmbient[1]), FormatFloat6(env.directionLightAmbient[2])});
    WriteRecordLine(out, {"DirectionLightDiffuse", FormatFloat6(env.directionLightDiffuse[0]), FormatFloat6(env.directionLightDiffuse[1]), FormatFloat6(env.directionLightDiffuse[2])});

    }
    return out.str();
}
} // namespace

std::expected<void, std::string> SerializeLegacyShmd(const ObjectPlacementSet& set, const std::filesystem::path& file) {
    const auto canonical = CanonicalShmd(set);
    const auto& text = !set.originalText.empty() && canonical == set.originalCanonical ? set.originalText : canonical;
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) return std::unexpected("Fehler beim Schreiben der .shmd: " + file.string());
    return {};
}

} // namespace theseed::mapeditor::core::legacy
