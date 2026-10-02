#include "mapeditor/core/DataDependency.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string_view>

using namespace theseed::mapeditor::core;

namespace {

bool Has(DataChangeKind kind, DataTree tree, DataDependencyClass cls, std::string_view path) {
    const auto& profile = DataDependencyProfileFor(kind);
    return std::any_of(profile.rules.begin(), profile.rules.end(), [&](const DataDependencyRule& r) {
        return r.tree == tree && r.dependencyClass == cls && r.relativePath == path;
    });
}

} // namespace

int main() {
    const auto profiles = DataDependencyProfiles();
    assert(profiles.size() == 11);

    assert(Has(DataChangeKind::NewMap, DataTree::Client, DataDependencyClass::Required,
               "ressystem/MapInfo.shn"));
    assert(Has(DataChangeKind::NewMap, DataTree::Server, DataDependencyClass::Conditional,
               "9Data/Shine/MobRegen/<Map>.txt"));

    for (auto kind : {DataChangeKind::NewItem, DataChangeKind::NewEquipment}) {
        assert(Has(kind, DataTree::Client, DataDependencyClass::Required,
                   "ressystem/ItemInfo.shn"));
        assert(Has(kind, DataTree::Server, DataDependencyClass::Required,
                   "9Data/Shine/ItemInfoServer.shn"));
        assert(Has(kind, DataTree::Client, DataDependencyClass::Required,
                   "ressystem/ItemViewInfo.shn"));
    }
    assert(Has(DataChangeKind::NewEquipment, DataTree::Client, DataDependencyClass::Conditional,
               "ressystem/WeaponAttrib.shn"));

    assert(Has(DataChangeKind::PlaceNpc, DataTree::Server, DataDependencyClass::Required,
               "9Data/Shine/World/NPC.txt"));
    assert(!Has(DataChangeKind::PlaceNpc, DataTree::Client, DataDependencyClass::Required,
                "ressystem/MobInfo.shn"));

    assert(Has(DataChangeKind::NewNpcIdentity, DataTree::Server, DataDependencyClass::Required,
               "9Data/Shine/MobInfoServer.shn"));
    assert(Has(DataChangeKind::NewMob, DataTree::Client, DataDependencyClass::Required,
               "ressystem/MobViewInfo.shn"));
    assert(Has(DataChangeKind::NewMob, DataTree::Server, DataDependencyClass::Conditional,
               "9Data/Shine/MobRoam/<Mob>.txt"));

    assert(Has(DataChangeKind::NewQuest, DataTree::Server, DataDependencyClass::Required,
               "9Data/Shine/QuestData.shn"));
    assert(Has(DataChangeKind::NewQuest, DataTree::Client, DataDependencyClass::Reference,
               "ressystem/ItemInfo.shn"));
    assert(!Has(DataChangeKind::NewQuest, DataTree::Client, DataDependencyClass::Required,
                "ressystem/ItemInfo.shn"));

    assert(Has(DataChangeKind::NewActiveSkill, DataTree::Server, DataDependencyClass::Required,
               "9Data/Shine/ActiveSkillInfoServer.shn"));

    std::cout << "DATA DEPENDENCY OK: " << profiles.size() << " profiles\n";
    return 0;
}
