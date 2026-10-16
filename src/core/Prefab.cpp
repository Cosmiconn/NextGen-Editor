#include "mapeditor/core/Prefab.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace theseed::mapeditor::core {

namespace {
float TerrainAt(const Heightmap& h, float x, float z) {
    return h.Width() > 1 && h.Height() > 1 ? h.SampleWorld(x, z) : 0.0f;
}
} // namespace

Prefab MakePrefab(const std::string& name, std::span<const PlacedObject> objects, const Heightmap& heightmap) {
    Prefab prefab;
    prefab.name = name;
    if (objects.empty()) return prefab;
    float cx = 0.0f, cz = 0.0f;
    for (const auto& o : objects) {
        cx += o.posX;
        cz += o.posZ;
    }
    cx /= static_cast<float>(objects.size());
    cz /= static_cast<float>(objects.size());
    for (const auto& o : objects) {
        PrefabItem item;
        item.modelPath = o.modelPath;
        item.dx = o.posX - cx;
        item.dz = o.posZ - cz;
        item.heightAboveGround = o.posY - TerrainAt(heightmap, o.posX, o.posZ);
        item.rotX = o.rotX;
        item.rotY = o.rotY;
        item.rotZ = o.rotZ;
        item.rotW = o.rotW;
        item.scale = o.scale;
        prefab.items.push_back(std::move(item));
    }
    return prefab;
}

std::vector<PlacedObject> InstantiatePrefab(const Prefab& prefab, const Heightmap& heightmap, float x, float z, float yawDegrees) {
    std::vector<PlacedObject> out;
    const float yaw = yawDegrees * 3.14159265f / 180.0f;
    const float c = std::cos(yaw), s = std::sin(yaw);
    // Drehung um +Y: x' = x*c + z*s, z' = -x*s + z*c (rechtshändig, wie das Quaternion (0, sin, 0, cos)).
    const float qy = std::sin(yaw * 0.5f), qw = std::cos(yaw * 0.5f);
    for (const auto& item : prefab.items) {
        PlacedObject o;
        o.modelPath = item.modelPath;
        o.posX = x + item.dx * c + item.dz * s;
        o.posZ = z - item.dx * s + item.dz * c;
        o.posY = TerrainAt(heightmap, o.posX, o.posZ) + item.heightAboveGround;
        // q = yaw * itemRot
        o.rotW = qw * item.rotW - qy * item.rotY;
        o.rotX = qw * item.rotX + qy * item.rotZ;
        o.rotY = qw * item.rotY + qy * item.rotW;
        o.rotZ = qw * item.rotZ - qy * item.rotX;
        o.scale = item.scale;
        out.push_back(std::move(o));
    }
    return out;
}

std::expected<void, std::string> SavePrefab(const Prefab& prefab, const std::filesystem::path& file) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return std::unexpected("Prefab nicht schreibbar: " + file.string());
    out << "#NextGenPrefab 1\n";
    char buf[256];
    for (const auto& it : prefab.items) {
        if (it.modelPath.find_first_of("\t\n\r") != std::string::npos) return std::unexpected("Ungültiger Modellpfad im Prefab");
        std::snprintf(buf, sizeof(buf), "\t%.3f\t%.3f\t%.3f\t%.6f\t%.6f\t%.6f\t%.6f\t%.4f\n", it.dx, it.heightAboveGround, it.dz,
                      it.rotX, it.rotY, it.rotZ, it.rotW, it.scale);
        out << it.modelPath << buf;
    }
    if (!out) return std::unexpected("Prefab nicht schreibbar: " + file.string());
    return {};
}

std::expected<Prefab, std::string> LoadPrefab(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected("Prefab nicht lesbar: " + file.string());
    Prefab prefab;
    prefab.name = file.stem().string();
    std::string line;
    if (!std::getline(in, line) || line.rfind("#NextGenPrefab", 0) != 0) return std::unexpected("Keine Prefab-Datei: " + file.string());
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        std::istringstream fields(line);
        PrefabItem it;
        std::string value;
        std::getline(fields, it.modelPath, '\t');
        float v[8];
        int n = 0;
        while (n < 8 && std::getline(fields, value, '\t')) v[n++] = std::strtof(value.c_str(), nullptr);
        if (it.modelPath.empty() || n != 8) return std::unexpected("Prefab-Zeile unvollständig: " + line);
        it.dx = v[0]; it.heightAboveGround = v[1]; it.dz = v[2];
        it.rotX = v[3]; it.rotY = v[4]; it.rotZ = v[5]; it.rotW = v[6]; it.scale = v[7];
        prefab.items.push_back(std::move(it));
    }
    return prefab;
}

std::vector<std::filesystem::path> ListPrefabs(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (e.is_regular_file(ec) && e.path().extension() == ".prefab") out.push_back(e.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace theseed::mapeditor::core
