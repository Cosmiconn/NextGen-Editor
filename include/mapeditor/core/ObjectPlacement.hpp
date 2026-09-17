#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct ObjectCategoryList { std::string name; std::vector<std::string> modelPaths; };
struct SceneEnvironment {
    float globalLight[3] = {1.0f, 1.0f, 1.0f};
    float fog[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float backgroundColor[3] = {0.0f, 0.0f, 0.0f};
    float frustumFar = 5000.0f;
    float directionLightAmbient[3] = {0.0f, 0.0f, 0.0f};
    float directionLightDiffuse[3] = {1.0f, 1.0f, 1.0f};
};
struct PlacedObject {
    std::string modelPath;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f, rotW = 1.0f;
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
