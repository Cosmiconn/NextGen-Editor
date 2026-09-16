#include "mapeditor/core/ObjectPlacement.hpp"

#include <utility>

namespace theseed::mapeditor::core {

std::size_t ObjectPlacementSet::AddObject(PlacedObject object) {
    objects_.push_back(std::move(object));
    return objects_.size() - 1;
}

void ObjectPlacementSet::RemoveObject(std::size_t index) {
    if (index >= objects_.size()) return;
    objects_.erase(objects_.begin() + static_cast<std::ptrdiff_t>(index));
}

} // namespace theseed::mapeditor::core
