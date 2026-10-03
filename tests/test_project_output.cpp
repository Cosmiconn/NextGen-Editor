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

    // Choosing a deeper read-only source root must never shorten the project layout.
    auto clientFromRessystem = ProjectOutputForSource(
        project, client / "ressystem", clientFile, ProjectOutputSide::Client);
    assert(clientFromRessystem);
    assert(*clientFromRessystem ==
           (project / "Client" / "ressystem" / "ItemInfo.shn").lexically_normal());

    auto serverFrom9Data = ProjectOutputForSource(
        project, server / "9Data", serverFile, ProjectOutputSide::Server);
    assert(serverFrom9Data);
    assert(*serverFrom9Data ==
           (project / "Server" / "9Data" / "Shine" / "QuestData.shn").lexically_normal());

    auto serverFromShine = ProjectOutputForSource(
        project, server / "9Data" / "Shine", serverFile, ProjectOutputSide::Server);
    assert(serverFromShine);
    assert(*serverFromShine ==
           (project / "Server" / "9Data" / "Shine" / "QuestData.shn").lexically_normal());

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

    // The writable project tree and either read-only source must be physically disjoint in
    // BOTH directions. Otherwise <Project>/Client could itself be the configured source.
    assert(ValidateProjectOutputRoots(project, client, server));
    assert(!ValidateProjectOutputRoots(client, client, server));
    assert(!ValidateProjectOutputRoots(client / "EditorProject", client, server));
    assert(!ValidateProjectOutputRoots(server / "EditorProject", client, server));
    assert(!ValidateProjectOutputRoots(project, project / "Client", server));
    assert(!ValidateProjectOutputRoots(project, client, project / "Server"));

    auto nestedProjectSource = ProjectOutputForSource(
        client / "EditorProject", client, clientFile, ProjectOutputSide::Client);
    assert(!nestedProjectSource);

    const auto nestedSourceRoot = project / "NestedClient";
    const auto nestedSourceFile = nestedSourceRoot / "ressystem" / "ItemInfo.shn";
    std::filesystem::create_directories(nestedSourceFile.parent_path());
    std::ofstream(nestedSourceFile).put('n');
    auto sourceInsideProject = ProjectOutputForSource(
        project, nestedSourceRoot, nestedSourceFile, ProjectOutputSide::Client);
    assert(!sourceInsideProject);

    auto parent = EnsureProjectOutputParent(*npcOut);
    assert(parent);
    assert(std::filesystem::is_directory(npcOut->parent_path()));

    // Existing symlinks/junction-like paths must not let the project Client tree physically
    // escape the project. Symlink creation can be unavailable on Windows without privileges;
    // in that environment the portable root-overlap checks above still run.
    const auto outside = base / "OutsideWritable";
    const auto symlinkProject = base / "SymlinkProject";
    std::filesystem::create_directories(outside);
    std::filesystem::create_directories(symlinkProject);
    ec.clear();
    std::filesystem::create_directory_symlink(outside, symlinkProject / "Client", ec);
    if (!ec) {
        auto escapedBySymlink = ProjectOutputForRelative(
            symlinkProject, ProjectOutputSide::Client,
            std::filesystem::path("ressystem") / "ItemInfo.shn");
        assert(!escapedBySymlink);
    }

    std::filesystem::remove_all(base, ec);
    std::cout << "PROJECT OUTPUT OK\n";
    return 0;
}
