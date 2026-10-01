#include "mapeditor/core/ProjectOutput.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace theseed::mapeditor::core;

int main() {
    const auto base = std::filesystem::temp_directory_path() / "nextgen_project_output_test";
    const auto project = base / "Project";
    const auto client = base / "OriginalClient";
    const auto server = base / "OriginalServer";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(client / "ressystem");
    std::filesystem::create_directories(server / "9Data" / "Shine" / "World");

    const auto clientFile = client / "ressystem" / "ItemInfo.shn";
    const auto serverFile = server / "9Data" / "Shine" / "QuestData.shn";
    std::ofstream(clientFile).put('c');
    std::ofstream(serverFile).put('s');

    auto clientOut = ProjectOutputForSource(
        project, client, clientFile, ProjectOutputSide::Client);
    assert(clientOut);
    assert(*clientOut == (project / "Client" / "ressystem" / "ItemInfo.shn").lexically_normal());

    auto serverOut = ProjectOutputForSource(
        project, server, serverFile, ProjectOutputSide::Server);
    assert(serverOut);
    assert(*serverOut == (project / "Server" / "9Data" / "Shine" / "QuestData.shn").lexically_normal());

    auto npcOut = ProjectOutputForRelative(
        project, ProjectOutputSide::Server,
        std::filesystem::path("9Data") / "Shine" / "World" / "NPC.txt");
    assert(npcOut);
    assert(*npcOut == (project / "Server" / "9Data" / "Shine" / "World" / "NPC.txt").lexically_normal());

    auto escapedRelative = ProjectOutputForRelative(
        project, ProjectOutputSide::Client, "../OriginalClient/ressystem/ItemInfo.shn");
    assert(!escapedRelative);

    auto escapedSource = ProjectOutputForSource(
        project, client, serverFile, ProjectOutputSide::Client);
    assert(!escapedSource);

    auto parent = EnsureProjectOutputParent(*npcOut);
    assert(parent);
    assert(std::filesystem::is_directory(npcOut->parent_path()));

    std::filesystem::remove_all(base, ec);
    std::cout << "PROJECT OUTPUT OK\n";
    return 0;
}
