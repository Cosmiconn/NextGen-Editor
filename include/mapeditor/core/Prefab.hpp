#pragma once
// Prefab.hpp
// Objektgruppen (z.B. Marktstand mit Kisten und Lampe) speichern und auf jeder Karte wieder
// einsetzen. Reines Editor-Format in <Projekt>/Prefabs/<Name>.prefab, wird nie ausgeliefert:
//   #NextGenPrefab 1
//   <Modellpfad>\t<dx>\t<dHöheÜberBoden>\t<dz>\t<qx>\t<qy>\t<qz>\t<qw>\t<Skalierung>
// Positionen relativ zum Gruppenmittelpunkt; die Höhe relativ zum Terrain unter dem jeweiligen
// Objekt, damit die Gruppe sich beim Einsetzen an Hänge anpasst.

#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/ObjectPlacement.hpp"

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct PrefabItem {
    std::string modelPath;
    float dx = 0.0f, heightAboveGround = 0.0f, dz = 0.0f;
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f, rotW = 1.0f;
    float scale = 1.0f;
};

struct Prefab {
    std::string name;
    std::vector<PrefabItem> items;
};

[[nodiscard]] Prefab MakePrefab(const std::string& name, std::span<const PlacedObject> objects, const Heightmap& heightmap);

// Setzt die Gruppe an (x, z) mit Drehung um die Hochachse (Grad).
[[nodiscard]] std::vector<PlacedObject> InstantiatePrefab(const Prefab& prefab, const Heightmap& heightmap, float x, float z,
                                                          float yawDegrees);

std::expected<void, std::string> SavePrefab(const Prefab& prefab, const std::filesystem::path& file);
[[nodiscard]] std::expected<Prefab, std::string> LoadPrefab(const std::filesystem::path& file);
// Alle .prefab-Dateien eines Ordners (nach Name sortiert).
[[nodiscard]] std::vector<std::filesystem::path> ListPrefabs(const std::filesystem::path& dir);

} // namespace theseed::mapeditor::core
