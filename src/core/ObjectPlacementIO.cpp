#include "mapeditor/core/ObjectPlacementIO.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace theseed::mapeditor::core {

// ---------------------------------------------------------------------------------------------
// Natives Format ".tsobj" - einfaches, selbstbeschreibendes Text-Format.
// ---------------------------------------------------------------------------------------------

std::expected<ObjectPlacementSet, std::string> LoadTsObj(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        return std::unexpected("Konnte .tsobj nicht \u00f6ffnen: " + file.string());
    }

    std::string magic;
    int version = 0;
    in >> magic >> version;
    if (magic != "TSOBJ" || version != 1) {
        return std::unexpected("Ung\u00fcltige .tsobj-Datei (Magic/Version stimmt nicht): " + file.string());
    }

    ObjectPlacementSet set;

    std::string keyword;
    in >> keyword; // "CATEGORIES"
    std::size_t categoryCount = 0;
    in >> categoryCount;
    for (std::size_t i = 0; i < categoryCount; ++i) {
        in >> keyword; // "CATEGORY"
        ObjectCategoryList category;
        in >> std::quoted(category.name);
        std::size_t pathCount = 0;
        in >> pathCount;
        for (std::size_t p = 0; p < pathCount; ++p) {
            std::string path;
            in >> std::quoted(path);
            category.modelPaths.push_back(std::move(path));
        }
        set.categories.push_back(std::move(category));
    }

    in >> keyword; // "ENVIRONMENT"
    in >> keyword >> set.environment.globalLight[0] >> set.environment.globalLight[1] >> set.environment.globalLight[2];
    in >> keyword >> set.environment.fog[0] >> set.environment.fog[1] >> set.environment.fog[2] >> set.environment.fog[3];
    in >> keyword >> set.environment.backgroundColor[0] >> set.environment.backgroundColor[1] >> set.environment.backgroundColor[2];
    in >> keyword >> set.environment.frustumFar;
    in >> keyword >> set.environment.directionLightAmbient[0] >> set.environment.directionLightAmbient[1] >> set.environment.directionLightAmbient[2];
    in >> keyword >> set.environment.directionLightDiffuse[0] >> set.environment.directionLightDiffuse[1] >> set.environment.directionLightDiffuse[2];

    in >> keyword; // "OBJECTS"
    std::size_t objectCount = 0;
    in >> objectCount;
    for (std::size_t i = 0; i < objectCount; ++i) {
        in >> keyword; // "OBJ"
        PlacedObject obj;
        in >> std::quoted(obj.modelPath);
        in >> obj.posX >> obj.posY >> obj.posZ >> obj.rotX >> obj.rotY >> obj.rotZ >> obj.rotW >> obj.scale;
        set.AddObject(std::move(obj));
    }

    if (!in) {
        return std::unexpected("Unerwartetes Dateiende / Parse-Fehler in .tsobj: " + file.string());
    }
    return set;
}

std::expected<void, std::string> SaveTsObj(const ObjectPlacementSet& set, const std::filesystem::path& file) {
    std::ofstream out(file, std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte .tsobj nicht zum Schreiben \u00f6ffnen: " + file.string());
    }

    out << "TSOBJ 1\n";
    out << "CATEGORIES " << set.categories.size() << "\n";
    for (const auto& category : set.categories) {
        out << "CATEGORY " << std::quoted(category.name) << " " << category.modelPaths.size() << "\n";
        for (const auto& path : category.modelPaths) {
            out << std::quoted(path) << "\n";
        }
    }

    out << "ENVIRONMENT\n";
    const auto& env = set.environment;
    out << "GlobalLight " << env.globalLight[0] << " " << env.globalLight[1] << " " << env.globalLight[2] << "\n";
    out << "Fog " << env.fog[0] << " " << env.fog[1] << " " << env.fog[2] << " " << env.fog[3] << "\n";
    out << "BackgroundColor " << env.backgroundColor[0] << " " << env.backgroundColor[1] << " " << env.backgroundColor[2] << "\n";
    out << "Frustum " << env.frustumFar << "\n";
    out << "DirectionLightAmbient " << env.directionLightAmbient[0] << " " << env.directionLightAmbient[1] << " " << env.directionLightAmbient[2] << "\n";
    out << "DirectionLightDiffuse " << env.directionLightDiffuse[0] << " " << env.directionLightDiffuse[1] << " " << env.directionLightDiffuse[2] << "\n";

    out << "OBJECTS " << set.Count() << "\n";
    for (const auto& obj : set.Objects()) {
        out << "OBJ " << std::quoted(obj.modelPath) << " "
            << obj.posX << " " << obj.posY << " " << obj.posZ << " "
            << obj.rotX << " " << obj.rotY << " " << obj.rotZ << " " << obj.rotW << " "
            << obj.scale << "\n";
    }

    if (!out) {
        return std::unexpected("Fehler beim Schreiben der .tsobj: " + file.string());
    }
    return {};
}

} // namespace theseed::mapeditor::core

// ---------------------------------------------------------------------------------------------
// Legacy-Import/-Export ("Rou.shmd")
// ---------------------------------------------------------------------------------------------

namespace theseed::mapeditor::core::legacy {

std::expected<ObjectPlacementSet, std::string> ParseLegacyShmd(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte .shmd nicht \u00f6ffnen: " + file.string());
    }

    ObjectPlacementSet set;

    std::string formatVersion;
    if (!(in >> formatVersion)) {
        return std::unexpected("Leere/ung\u00fcltige .shmd-Datei: " + file.string());
    }

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
            if (!(in >> legacyX >> legacyY >> legacyZ >> legacyRotX >> legacyRotY >> legacyRotZ >> legacyRotW >> scale)) {
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
    if (!in) {
        return std::unexpected("Token 'DataObjectLoadingEnd' nicht gefunden: " + file.string());
    }

    if (!(in >> kw) || kw != "DirectionLightAmbient" ||
        !(in >> env.directionLightAmbient[0] >> env.directionLightAmbient[1] >> env.directionLightAmbient[2])) {
        return std::unexpected("Fehler beim Lesen von DirectionLightAmbient: " + file.string());
    }
    if (!(in >> kw) || kw != "DirectionLightDiffuse" ||
        !(in >> env.directionLightDiffuse[0] >> env.directionLightDiffuse[1] >> env.directionLightDiffuse[2])) {
        return std::unexpected("Fehler beim Lesen von DirectionLightDiffuse: " + file.string());
    }

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
void WriteRecordLine(std::ofstream& out, const std::vector<std::string>& tokens) {
    for (const auto& t : tokens) {
        out << t << " ";
    }
    out << "\r\n";
}

} // namespace

std::expected<void, std::string> SerializeLegacyShmd(const ObjectPlacementSet& set, const std::filesystem::path& file) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte .shmd nicht zum Schreiben \u00f6ffnen: " + file.string());
    }

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
    for (const auto& obj : set.Objects()) {
        bool found = false;
        for (std::size_t i = 0; i < blockOrder.size(); ++i) {
            if (blockOrder[i] == obj.modelPath) {
                blockObjects[i].push_back(&obj);
                found = true;
                break;
            }
        }
        if (!found) {
            blockOrder.push_back(obj.modelPath);
            blockObjects.push_back({&obj});
        }
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

    out << "DataObjectLoadingEnd\r\n"; // Sonderfall: KEIN trailing space (siehe Header-Kommentar)

    WriteRecordLine(out, {"DirectionLightAmbient", FormatFloat6(env.directionLightAmbient[0]), FormatFloat6(env.directionLightAmbient[1]), FormatFloat6(env.directionLightAmbient[2])});
    WriteRecordLine(out, {"DirectionLightDiffuse", FormatFloat6(env.directionLightDiffuse[0]), FormatFloat6(env.directionLightDiffuse[1]), FormatFloat6(env.directionLightDiffuse[2])});

    if (!out) {
        return std::unexpected("Fehler beim Schreiben der .shmd: " + file.string());
    }
    return {};
}

} // namespace theseed::mapeditor::core::legacy
