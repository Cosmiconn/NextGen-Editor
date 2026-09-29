#pragma once
// Vorschau eines Spieler-Avatars (NPC "Spieler mit Ruestung", siehe NPCViewInfo.shn): setzt aus den
// Client-Dateien reschar/<Klasse>-<m|f>/ (Koerper, Gesicht, Haare, Ruestungs-Sets, Texturen) und
// resitem/ (Waffen) ein statisches Modell in Bindepose zusammen und rendert es per CPU.
//
// Zusammensetzung (an Fighter-m/-f, Cleric-f geprueft, CHANGELOG [0.44.30]):
//  * Koerper: <Klasse>-<m|f>.nif - je Textur ein Teil (Body/Legs/Shoes), drei Detailstufen -> groesste.
//  * Ruestung: ItemViewInfo.MSetNo/FSetNo -> setNNN.nif liefert die Geometrie des Slots
//    (Koerper/Beine/Schuhe erkennt man an der Hoehe), ItemViewInfo.TextureFile -> <Textur>.dds im
//    Klassenordner. Zusaetzliche Teile (_BR, _BELT, ...) werden NICHT dargestellt.
//  * Kopf: Face%03d.nif (Haut + Augen) am Knoten "Bip01 Head"; Haare: HairInfo -> Hair..._Front/Bottom/Top.
//  * Waffen: ItemViewInfo.LinkFile -> resitem/<LinkFile>.nif, Handposition GESCHAETZT (Bindepose).
#include "mapeditor/core/NifModel.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct AvatarItem {
    std::string slot;        // "Body", "Leg", "Shoes", "RightHand", "LeftHand"
    std::string textureFile; // ItemViewInfo.TextureFile ("-" = keine)
    int setNo = 0;           // MSetNo (maennlich) bzw. FSetNo (weiblich)
    std::string linkFile;    // ItemViewInfo.LinkFile (Waffen/Schilde)
};

struct AvatarRequest {
    std::filesystem::path charRoot;  // .../reschar
    std::filesystem::path itemRoot;  // .../resitem (Waffen) - darf leer sein
    int classIdx = 0;                // 0 Fighter, 1 Archer (keine Modelle), 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel
    bool male = true;
    int faceShape = 1;
    std::string hairFront, hairBottom, hairTop, hairTexture; // Modellnamen aus HairInfo (ohne .nif)
    std::vector<AvatarItem> items;
};

struct AvatarTexture {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};

struct AvatarPart {
    std::vector<NifVec3> positions, normals;
    std::vector<NifVec2> uvs;
    std::vector<std::uint32_t> triangleIndices;
    std::shared_ptr<const AvatarTexture> texture; // nullptr = Einheitsfarbe
    std::array<float, 3> color{0.62f, 0.62f, 0.68f};
    std::string label;
};

struct AvatarModel {
    std::vector<AvatarPart> parts;
    std::vector<std::string> notes; // Hinweise (fehlende Dateien/Texturen) fuer die UI
};

// Ordnername der Klasse ("Fighter", ...); leer fuer Klassen ohne Modelle (Archer).
std::string AvatarClassFolder(int classIdx);

std::expected<AvatarModel, std::string> BuildAvatarModel(const AvatarRequest& request);

// Bereitet ein geladenes Charakter-NIF fuer die Anzeige auf (NPC-Modelle aus reschar/<Name>/<Name>.nif):
// untexturierte Teile (unsichtbare Knochen-Huellkoerper) entfallen, von mehreren Detailstufen desselben
// Teils (gleiche Textur, ueberlappende Bounding-Box) bleibt nur die mit den meisten Vertices.
void SimplifyCharacterModel(NifModel& model);

// Wandelt ein zusammengesetztes Avatar-Modell in ein NifModel (fuer NifMeshRenderer: Teile mit
// eingebetteten Texturen, Farben als Material) - damit Spieler-Avatar-NPCs im 3D-View gezeichnet werden.
NifModel AvatarToNifModel(const AvatarModel& model);

// Rendert das Modell (Drehung um die Hochachse) in einen RGBA-Puffer width*height*4.
std::vector<std::uint8_t> RenderAvatarModel(const AvatarModel& model, float yawRadians, int width, int height);

} // namespace theseed::mapeditor::core
