#pragma once
// NifModel.hpp
// GUI-freier Parser für das Gamebryo/NetImmerse-Format (NIF), Version 20.0.0.4 - verwendet von
// allen .nif-Objektdateien in den echten Kartensets (bestätigt per Header-Signatur an
// mehreren hundert echten Dateien).
//
// STATUS (siehe docs/MAP_FORMAT.md für die vollständige Herleitung):
//   - Datei-Header (Block-Typen-Liste, Block-Type-Index): VERIFIZIERT
//   - NiNode-Szenengraph (Name, Transform, Kinder, Properties): VERIFIZIERT
//   - NiMaterialProperty: VERIFIZIERT
//   - NiTriStripsData (Vertices/Normalen/Farben/UVs/Dreiecksstreifen): VERIFIZIERT BYTE-EXAKT
//   - NiTexturingProperty/NiSourceTexture/NiPixelData: VERIFIZIERT; klassische Slots
//     Base/Dark/Detail/Gloss/Glow/Bump/Decal0..3 inklusive UV-Set, Clamp/Filter und optionaler
//     Texture-Transform werden erhalten. Use External=0 dekodiert eingebettete NiPixelData,
//     inklusive Fiesta-Palettenvarianten mit nachgelagertem NiPalette-Block.
//   - NiTextureTransformController/NiFlipController: Float-Key-Spuren und Flipbook-Quellen
//     werden erhalten und vom OpenGL-Renderer zeitabhaengig ausgewertet.
//   - NiSkinInstance/NiSkinData/NiSkinPartition: gewichtetes CPU-Skinning der aktuellen
//     Bind-/Skeleton-Pose; Partition- und Sparse-Weight-Pfade werden unterstützt.
//
// Für nicht unterstützte/nicht parsbare Objekte liefert LoadNifMesh einen Fehler zurück - der
// Aufrufer sollte in diesem Fall auf den Platzhalter-Marker zurückfallen (siehe
// ObjectMarkerRenderer), nicht abstürzen.

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace theseed::mapeditor::core {

struct NifVec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct NifVec2 {
    float u = 0.0f, v = 0.0f;
};

struct NifColor4 {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

struct NifMaterial {
    std::array<float, 3> ambient{1.0f, 1.0f, 1.0f};
    std::array<float, 3> diffuse{1.0f, 1.0f, 1.0f};
    std::array<float, 3> specular{1.0f, 1.0f, 1.0f};
    std::array<float, 3> emissive{0.0f, 0.0f, 0.0f};
    float glossiness = 10.0f;
    float alpha = 1.0f;
};

// Ein einzelnes NiTriStrips/NiTriStripsData-Paar, in Dreiecke aufgelöst (aus den
// Dreiecksstreifen mit Degenerate-Triangle-Entfernung - siehe ExpandTriangleStrip).
struct NifEmbeddedTexture {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Top-Mip als RGBA8; vor dem Upload wird die gleiche V-Ausrichtung wie bei DDS hergestellt.
    std::vector<std::uint8_t> rgba;
};

// Ein Slot aus NiTexturingProperty. Die klassischen Slots sind:
// 0 Base, 1 Dark, 2 Detail, 3 Gloss, 4 Glow, 5 Bump, 6..9 Decal 0..3.
// Die Struktur bleibt absichtlich generisch, damit jeder Slot sein eigenes UV-Set, Sampling
// und seine optionale NIF-Texturmatrix behalten kann.
inline constexpr std::uint32_t kNifTextureTransformMayaDeprecated = 0;
inline constexpr std::uint32_t kNifTextureTransformMax = 1;
inline constexpr std::uint32_t kNifTextureTransformMaya = 2;

struct NifTextureSlot {
    bool present = false;
    std::string texture;
    // Herkunft des NiSourceTexture-Slots explizit erhalten. Bei useExternal=0 ist ein leerer
    // Dateiname normal; wenn die referenzierte NiPixelData nicht dekodiert werden konnte,
    // darf dieser Fall nicht als "keine Textur" oder fehlender externer Pfad verschwinden.
    bool sourceUsesEmbeddedPixelData = false;
    std::int32_t sourcePixelDataRef = -1;
    std::shared_ptr<const NifEmbeddedTexture> embeddedTexture;
    std::uint32_t uvSet = 0;
    std::uint32_t clampMode = 3;
    std::uint32_t filterMode = 2;
    bool hasTransform = false;
    NifVec2 translation{};
    NifVec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
    // nif.xml TransformMethod: 0=TM_Maya Deprecated, 1=TM_Max, 2=TM_Maya.
    // Die Reihenfolge von Scale/Rotation/Translation unterscheidet sich sichtbar.
    std::uint32_t transformType = kNifTextureTransformMayaDeprecated;
    NifVec2 center{0.5f, 0.5f};
};

// Reine CPU-Referenz derselben TexDesc-UV-Matrix, die der OpenGL-Renderer im Shader
// auswertet. Sie hält die Gamebryo-TransformMethod-Semantik testbar, ohne GL-Kontext.
[[nodiscard]] NifVec2 ApplyNifTextureTransform(const NifTextureSlot& slot, NifVec2 uv);

// Zeitabhaengige Float-Spur fuer NiTextureTransformController/NiFlipController.
// extrapolation: 0=cycle, 1=reverse, 2=constant/clamp (NiTimeController flags bits 1..2).
struct NifFloatKey {
    float time = 0.0f;
    float value = 0.0f;
    float forwardTangent = 0.0f;
    float backwardTangent = 0.0f;
};

struct NifFloatTrack {
    bool active = false;
    std::uint8_t extrapolation = 2;
    float frequency = 1.0f;
    float phase = 0.0f;
    float startTime = 0.0f;
    float stopTime = 0.0f;
    float currentValue = 0.0f;
    std::uint32_t interpolation = 1; // 1 linear, 2 quadratic, 3 TBC, 5 constant
    std::vector<NifFloatKey> keys;
};

// Verifizierte Transformdarstellung für NIF-Szene/Skinning im ursprünglichen
// Gamebryo-Koordinatenrahmen. Sie wird zusätzlich zur bereits gerenderten Bind-Pose erhalten,
// damit KF-Playback dieselben Bone-/Skin-Matrizen erneut auswerten kann.
struct NifTransform {
    std::array<float, 9> rotation{1.0f, 0.0f, 0.0f,
                                  0.0f, 1.0f, 0.0f,
                                  0.0f, 0.0f, 1.0f};
    NifVec3 translation{};
    float scale = 1.0f;
};

struct NifSkinInfluence {
    std::uint16_t boneIndex = 0; // Index in NifSkinBinding::bones
    float weight = 0.0f;
};

struct NifSkinBoneBinding {
    std::int32_t nodeIndex = -1; // Index in NifModel::nodes
    NifTransform bindTransform{}; // NiSkinData bone transform, bytegetreu gelesen
};

struct NifSkinBinding {
    std::int32_t skeletonRootNodeIndex = -1;
    bool partitionWeights = false;
    NifTransform skinTransform{};        // NiSkinData::Skin Transform
    NifTransform meshToModelTransform{}; // Geometrie-/Parent-Kette nach dem Skinning
    std::vector<NifVec3> sourcePositions;
    std::vector<NifVec3> sourceNormals;
    std::vector<std::vector<NifSkinInfluence>> vertexInfluences;
    std::vector<NifSkinBoneBinding> bones;
};

struct NifTextureTransformAnimation {
    std::uint32_t slot = 0;
    std::uint32_t operation = 0; // 0 U offset, 1 V offset, 2 rotation, 3 U scale, 4 V scale
    NifFloatTrack track;
};

struct NifTextureFlipFrame {
    std::string texture;
    bool sourceUsesEmbeddedPixelData = false;
    std::int32_t sourcePixelDataRef = -1;
    std::shared_ptr<const NifEmbeddedTexture> embeddedTexture;
};

struct NifTextureFlipAnimation {
    std::uint32_t slot = 0;
    NifFloatTrack track;
    std::vector<NifTextureFlipFrame> frames;
};

// NiTextureEffect ist ein NiDynamicEffect und kein NiProperty. Fiesta verwendet im echten
// Corpus vor allem ENVIRONMENT_MAP + SPHERE_MAP. Die vollständigen Wire-Felder bleiben hier
// trotzdem erhalten, damit andere Effect-/CoordGen-Kombinationen nicht still umgedeutet werden.
struct NifTextureEffectBinding {
    bool enabled = true;
    std::array<float, 9> projectionRotation{1.0f,0.0f,0.0f,
                                            0.0f,1.0f,0.0f,
                                            0.0f,0.0f,1.0f};
    NifVec3 projectionPosition{};
    std::uint32_t filterMode = 2;
    std::uint32_t clampMode = 3;
    std::uint32_t textureType = 0;
    std::uint32_t coordGenType = 0;
    std::string texture;
    bool sourceUsesEmbeddedPixelData = false;
    std::int32_t sourcePixelDataRef = -1;
    std::shared_ptr<const NifEmbeddedTexture> embeddedTexture;
    bool clippingPlaneEnabled = false;
    std::array<float, 4> clippingPlane{};
};

struct NifMeshPart {
    std::string name;
    std::string shaderName;            // z.B. VCAlphaTextureBlender
    std::vector<NifVec3> positions;
    std::vector<NifVec3> normals;      // leer, falls keine Normalen vorhanden
    std::vector<NifColor4> vertexColors; // leer => weiss/alpha 1 im Renderer
    std::vector<NifVec2> uvs;          // fuer die Base-Textur ausgewaehltes UV-Set
    std::vector<std::vector<NifVec2>> uvSets; // alle im Geometrieblock vorhandenen UV-Sets
    std::vector<std::uint32_t> triangleIndices; // 3 Indizes pro Dreieck, in `positions` indiziert
    NifMaterial material;
    // Vollstaendige klassische NiTexturingProperty-Slots. Die alten Base-Felder bleiben als
    // Kompatibilitaets-/Diagnose-Alias fuer Slot 0 erhalten.
    std::array<NifTextureSlot, 10> textureSlots{};
    std::vector<NifTextureTransformAnimation> textureTransformAnimations;
    std::vector<NifTextureFlipAnimation> textureFlipAnimations;
    std::vector<NifTextureEffectBinding> textureEffects;
    std::uint32_t textureApplyMode = 2; // APPLY_MODULATE
    float bumpMapLumaScale = 1.0f;
    float bumpMapLumaOffset = 0.0f;
    std::array<float, 4> bumpMapMatrix{1.0f, 0.0f, 0.0f, 1.0f};
    std::string diffuseTexture; // Alias fuer textureSlots[0].texture
    std::shared_ptr<const NifEmbeddedTexture> embeddedDiffuseTexture; // Alias fuer Slot 0
    bool specularEnabled = true; // NiSpecularProperty fehlt => NifSkope nutzt Material-Specular
    // NiVertexColorProperty. Ohne explizite Property entspricht der klassische NIF-Pfad
    // bei vorhandenen Vertexfarben SRC_AMB_DIF + EMI_AMB_DIF; der Renderer entscheidet den
    // Default anhand davon, ob für diesen Part tatsächlich Vertexfarben vorhanden sind.
    bool hasVertexColorProperty = false;
    std::uint32_t vertexColorMode = 2;    // 0 SRC_IGNORE, 1 SRC_EMISSIVE, 2 SRC_AMB_DIF
    std::uint32_t vertexLightingMode = 1; // 0 EMISSIVE, 1 EMI_AMB_DIF
    // NiAlphaProperty render state. Flags follow the Gamebryo/NIF bit layout.
    bool alphaBlend = false;
    bool alphaTest = false;
    std::uint8_t alphaThreshold = 0;
    // Raw NiAlphaProperty blend/test function selectors (Gamebryo bit fields).
    std::uint8_t alphaSrcBlend = 6; // SRC_ALPHA
    std::uint8_t alphaDstBlend = 7; // INV_SRC_ALPHA
    std::uint8_t alphaTestFunc = 4; // GREATER
    // NiZBufferProperty. Defaults match the normal fixed-function editor path when
    // no explicit Z property is attached: test + write, LESS_EQUAL comparison.
    bool depthTest = true;
    bool depthWrite = true;
    std::uint32_t depthFunction = 3; // ZCOMP_LESS_EQUAL
    std::uint32_t baseUvSet = 0;
    std::uint32_t textureClampMode = 3;
    std::uint32_t textureFilterMode = 2;
    // NiStencilProperty FaceDrawMode: 0=application default, 1=CCW, 2=CW, 3=both.
    std::uint32_t faceDrawMode = 3;

    // Skinning wird beim Laden in der aktuellen Bind-/Skeleton-Pose CPU-seitig ausgewertet.
    // Die Metadaten bleiben fuer Diagnose/UI erhalten; Animationen selbst sind noch kein Teil
    // des Map-Editor-Renderloops.
    bool skinned = false;
    std::uint16_t skinBoneCount = 0;
    std::uint8_t maxSkinInfluences = 0;
    // Originale Skin-Quelle/Weights/Bind-Matrizen bleiben neben der fertig berechneten Bind-Pose
    // erhalten. Der normale Map-Renderer nutzt weiterhin positions/normals; nur der
    // KFM-Preview-Pfad wertet skinBinding zeitabhängig aus.
    std::optional<NifSkinBinding> skinBinding;

    // Dynamische Scene-Graph-Semantik, die nicht dauerhaft in die Vertexdaten eingebrannt
    // werden darf. Die Geometrie selbst bleibt weiterhin in Modellkoordinaten.
    bool billboard = false;
    std::uint16_t billboardMode = 0;
    NifVec3 billboardPivot{};
    // Inverse der beim Laden bereits eingebrannten Billboard-Weltrotation (Editor-Rahmen,
    // column-major 3x3). Der Renderer ersetzt damit nur die Orientierung durch die Kamera-
    // Orientierung, ohne Kind-Transforms/Translation/Skalierung zu verlieren.
    std::array<float, 9> billboardInverseRotation{1.0f, 0.0f, 0.0f,
                                                  0.0f, 1.0f, 0.0f,
                                                  0.0f, 0.0f, 1.0f};

    bool lodControlled = false;
    float lodNear = 0.0f;
    float lodFar = 0.0f;
    NifVec3 lodCenter{};
};

// Scene-Graph-Knoten (z.B. Skelett-Knochen "Bip01 Head"). Neben der bisherigen
// Weltposition/-rotation bleiben jetzt auch Parent und lokaler Bind-Transform erhalten. Das ist
// reine, aus dem NIF gelesene Strukturinformation und erlaubt dem KFM-Preview, verifizierte
// KF-Transformtracks auf die echte NIF-Hierarchie anzuwenden, ohne eine Bone-Hierarchie zu raten.
//
// Coordinate conventions:
// - position: Weltposition im Editor-Rahmen (x, z, y), wie die gerenderten Vertices.
// - rotation: bisherige Weltrotation im Legacy/Gamebryo-Rahmen, row-major.
// - localTranslation/localRotation/localScale: unveränderter lokaler NIF-Transform im
//   Legacy/Gamebryo-Rahmen. KF-Transforms verwenden denselben lokalen Rahmen.
struct NifNodeInfo {
    std::string name;
    std::int32_t parentIndex = -1; // Index in NifModel::nodes, -1 = keine erfasste Node-Parent.
    NifVec3 localTranslation{};
    std::array<float, 9> localRotation{1.0f, 0.0f, 0.0f,
                                       0.0f, 1.0f, 0.0f,
                                       0.0f, 0.0f, 1.0f};
    float localScale = 1.0f;
    NifVec3 position;
    std::array<float, 9> rotation{};
};

struct NifModel {
    bool recovered = false; // compatibility parsing was required
    bool partial = false;   // stopped before all declared blocks were consumed
    std::uint32_t decodedEmbeddedTextures = 0;
    std::uint32_t undecodedEmbeddedTextures = 0;
    // Render-Diagnose für im NIF vorkommende Property-/Effect-Familien.
    // NiTextureEffect wird strukturell erhalten und klassifiziert; Rendering bleibt bis zur
    // verifizierten EnvironmentMap/SphereMap-Anbindung bewusst separat. NiVertexColorProperty
    // und NiZBufferProperty werden bereits vollständig in den Mesh-Renderstate übernommen.
    std::uint32_t textureEffectBlocks = 0;
    std::uint32_t textureEffectEnvironmentSphereBlocks = 0;
    std::uint32_t textureEffectUnsupportedBlocks = 0;
    std::uint32_t textureEffectNodeBindings = 0;
    std::uint32_t vertexColorPropertyBlocks = 0;
    std::uint32_t zBufferPropertyBlocks = 0;
    // Anzahl effektiver NiProperty-Refs, die nicht direkt am Mesh hängen, sondern über
    // die NiNode-Parentkette geerbt und deshalb zusätzlich in den Renderstate übernommen werden.
    std::uint32_t inheritedPropertyBindings = 0;
    std::string rootName;
    std::vector<NifMeshPart> parts;
    std::vector<NifNodeInfo> nodes; // NiNode-Hierarchie inkl. lokaler Bind-Transforms; Namen können bei reinen Hierarchie-Knoten leer sein.
};

// Disable recovery for deterministic corpus validation of the standard parser.
std::expected<NifModel, std::string> LoadNifMesh(const std::filesystem::path& file,
                                              bool allowRecovery = true);

struct NifGroundContactSegment {
    float x0 = 0.0f;
    float z0 = 0.0f;
    float x1 = 0.0f;
    float z1 = 0.0f;
};

// Exakte 2D-Kontaktkontur fuer den Editor: Schnitt der echten Mesh-Dreiecke mit der lokalen
// Bodenebene des platzierten Modells. Wenn y=0 innerhalb der Modellhoehe liegt, wird diese
// authored Pivot-/Placement-Ebene verwendet; andernfalls die tiefste Modellhoehe. Koplanare
// Bodenflaechen werden auf ihre Randkanten reduziert, interne Triangulationskanten entfernt.
// Dadurch bleiben konkave und getrennte Standflaechen erhalten statt zu einer konvexen Huelle
// zusammengeschmolzen zu werden. Leer, wenn keine belastbare Kontaktlinie ableitbar ist.
std::vector<NifGroundContactSegment> ComputeGroundContactSegments(const NifModel& model);

// Legacy-/Walk-Fallback: konvexe Huelle (x,z) der Vertices in der untersten Hoehenschicht.
// Fuer die sichtbare 2D-Objektkontur NICHT verwenden; konkave Grundrisse werden hier bewusst
// ueberdeckt. Der Pfad bleibt vorerst fuer bestehende Walk/Block-Polygonoperationen erhalten.
std::vector<std::pair<float, float>> ComputeFootprintHull(const NifModel& model);

} // namespace theseed::mapeditor::core
