#pragma once

#include <span>
#include <string_view>

namespace theseed::mapeditor::core {

enum class DataChangeKind {
    NewMap,
    NewItem,
    NewEquipment,
    PlaceNpc,
    NewNpcIdentity,
    NewMob,
    NewQuest,
    NewActiveSkill,
    Shop,
    DropTable,
    Portal
};

enum class DataTree {
    Client,
    Server
};

enum class DataDependencyClass {
    Required,
    Conditional,
    Reference,
    Asset
};

struct DataDependencyRule {
    DataTree tree;
    DataDependencyClass dependencyClass;
    std::string_view relativePath;
    std::string_view reason;
};

struct DataDependencyProfile {
    DataChangeKind kind;
    std::string_view labelDe;
    std::string_view labelEn;
    std::span<const DataDependencyRule> rules;
};

std::span<const DataDependencyProfile> DataDependencyProfiles();
const DataDependencyProfile& DataDependencyProfileFor(DataChangeKind kind);

std::string_view DataTreeName(DataTree tree);
std::string_view DataDependencyClassNameDe(DataDependencyClass value);
std::string_view DataDependencyClassNameEn(DataDependencyClass value);

} // namespace theseed::mapeditor::core
