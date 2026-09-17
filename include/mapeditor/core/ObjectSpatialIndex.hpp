#pragma once
// ObjectSpatialIndex.hpp
// Kern-Datencontainer für den räumlichen Index aus "Rou.idm". Die genaue Zellzuordnung
// bleibt bewusst offen, solange sie nicht durch Referenzdaten verifiziert ist.

#include <cstdint>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct SpatialIndexGroup {
    std::vector<std::int32_t> indices;
};

struct ObjectSpatialIndex {
    std::string hash;
    std::int32_t headerValue = 0;
    std::vector<SpatialIndexGroup> groups;
};

} // namespace theseed::mapeditor::core
