#include "mapeditor/core/Autosave.hpp"

#include <fstream>
#include <sstream>

namespace theseed::mapeditor::core {

std::filesystem::path AutosaveMapDir(const std::filesystem::path& projectFolder, const std::string& mapStem) {
    return projectFolder / "Autosave" / "Client" / "resmap" / "field" / mapStem;
}

std::expected<void, std::string> WriteAutosaveMeta(const std::filesystem::path& mapDir, const AutosaveMeta& meta) {
    std::error_code ec;
    std::filesystem::create_directories(mapDir, ec);
    if (ec) return std::unexpected("Autosave-Ordner nicht anlegbar: " + mapDir.string());
    const auto tmp = mapDir / "autosave.txt.tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return std::unexpected("autosave.txt nicht schreibbar: " + mapDir.string());
        out << "source_ini=" << meta.sourceIni << "\n";
        out << "map=" << meta.mapStem << "\n";
        out << "saved_at=" << meta.savedAtUnix << "\n";
        if (!out) return std::unexpected("autosave.txt nicht schreibbar: " + mapDir.string());
    }
    std::filesystem::rename(tmp, mapDir / "autosave.txt", ec);
    if (ec) return std::unexpected("autosave.txt nicht ersetzbar: " + ec.message());
    return {};
}

std::optional<AutosaveMeta> ReadAutosaveMeta(const std::filesystem::path& mapDir) {
    std::ifstream in(mapDir / "autosave.txt", std::ios::binary);
    if (!in) return std::nullopt;
    AutosaveMeta meta;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "source_ini") meta.sourceIni = value;
        else if (key == "map") meta.mapStem = value;
        else if (key == "saved_at") meta.savedAtUnix = std::atoll(value.c_str());
    }
    if (meta.mapStem.empty()) return std::nullopt;
    return meta;
}

void RemoveAutosave(const std::filesystem::path& projectFolder, const std::string& mapStem) {
    if (projectFolder.empty() || mapStem.empty() || mapStem.find_first_of("/\\") != std::string::npos || mapStem == "..")
        return;
    std::error_code ec;
    std::filesystem::remove_all(AutosaveMapDir(projectFolder, mapStem), ec);
}

bool AutosaveDue(bool enabled, bool dirty, double nowSeconds, double lastSaveSeconds, double intervalSeconds) {
    return enabled && dirty && intervalSeconds > 0.0 && nowSeconds - lastSaveSeconds >= intervalSeconds;
}

} // namespace theseed::mapeditor::core
