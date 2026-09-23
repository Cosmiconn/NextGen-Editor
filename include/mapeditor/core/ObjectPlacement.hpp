#pragma once
// ObjectPlacement.hpp
// Kern-Datencontainer des Objekt-Placement-Moduls. Struktur 1:1 aus der echten Rou.shmd
// hergeleitet (vollständig geparst, 0 Rest-Tokens - siehe docs/MAP_FORMAT.md):
//   - drei "Kategorie"-Listen (Sky/Water/GroundObject in der Referenzdatei, aber generisch
//     gehalten) mit reinen Modellpfad-Listen ohne Transform
//   - globale Szene-Parameter (Licht, Nebel, Hintergrundfarbe, Sichtweite)
//   - eine flache Liste platzierter Objekt-Instanzen (Modellpfad + Transform)

#include <cstdint>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct ObjectCategoryList {
    std::string name; // z.B. "Sky", "Water", "GroundObject" in der Referenzdatei
    std::vector<std::string> modelPaths;
};

struct SceneEnvironment {
    float globalLight[3] = {1.0f, 1.0f, 1.0f};
    float fog[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float backgroundColor[3] = {0.0f, 0.0f, 0.0f};
    float frustumFar = 5000.0f;
    float directionLightAmbient[3] = {0.0f, 0.0f, 0.0f};
    float directionLightDiffuse[3] = {1.0f, 1.0f, 1.0f};
};

struct PlacedObject {
    std::string modelPath; // z.B. "resmap\\field\\Rou\\GuildHall.nif"
    // Y-up (konsistent mit Heightmap: X/Z = horizontale Ebene, Y = Höhe). Legacy-Import/-Export
    // rechnet von/zu dessen Z-up-Konvention um, siehe ObjectPlacementIO.hpp.
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f, rotW = 1.0f; // Quaternion
    float scale = 1.0f;
};

class ObjectPlacementSet {
public:
    std::vector<ObjectCategoryList> categories;
    SceneEnvironment environment;

    std::size_t AddObject(PlacedObject object);
    void RemoveObject(std::size_t index);

    [[nodiscard]] std::size_t Count() const noexcept { return objects_.size(); }
    [[nodiscard]] const PlacedObject& At(std::size_t index) const { return objects_.at(index); }
    [[nodiscard]] PlacedObject& At(std::size_t index) { return objects_.at(index); }
    [[nodiscard]] const std::vector<PlacedObject>& Objects() const noexcept { return objects_; }

private:
    std::vector<PlacedObject> objects_;
};

} // namespace theseed::mapeditor::core
