#include "mapeditor/core/legacy/ItemDropGroups.hpp"

#include <algorithm>
#include <set>

namespace theseed::mapeditor::core::legacy {

namespace {
int ColumnIndex(const ShineTable& table, const std::string& name) {
    for (std::size_t i = 0; i < table.columns.size(); ++i)
        if (table.columns[i].name == name) return static_cast<int>(i);
    return -1;
}
std::string Value(const ShineRecord& record, int column) {
    if (column < 0 || static_cast<std::size_t>(column) >= record.values.size()) return {};
    return record.values[static_cast<std::size_t>(column)];
}
} // namespace

bool DropGroupCatalog::SetGroups(const ShineTable& itemDropGroup) {
    const int cGroup = ColumnIndex(itemDropGroup, "ItemGroupIdx");
    const int cItem = ColumnIndex(itemDropGroup, "ItemID");
    const int cMin = ColumnIndex(itemDropGroup, "MinQtty");
    const int cMax = ColumnIndex(itemDropGroup, "MaxQtty");
    if (cGroup < 0 || cItem < 0) return false;
    groups_.clear();
    for (const auto& record : itemDropGroup.records) {
        const std::string group = Value(record, cGroup);
        if (group.empty()) continue;
        groups_[group].push_back({Value(record, cItem), Value(record, cMin), Value(record, cMax)});
    }
    hasGroups_ = true;
    return true;
}

void DropGroupCatalog::AddItemInfoServer(const ShnFile& itemInfoServer) {
    int cInx = -1, cA = -1, cB = -1;
    for (std::size_t i = 0; i < itemInfoServer.columns.size(); ++i) {
        const auto& name = itemInfoServer.columns[i].name;
        if (name == "InxName") cInx = static_cast<int>(i);
        else if (name == "DropGroupA") cA = static_cast<int>(i);
        else if (name == "DropGroupB") cB = static_cast<int>(i);
    }
    if (cInx < 0) return;
    auto cell = [](const ShnRow& row, int c) {
        return c >= 0 && static_cast<std::size_t>(c) < row.values.size() ? ShnValueToString(row.values[static_cast<std::size_t>(c)]) : std::string();
    };
    for (const auto& row : itemInfoServer.rows) {
        const std::string inx = cell(row, cInx);
        if (inx.empty()) continue;
        for (const int c : {cA, cB}) {
            const std::string group = cell(row, c);
            if (!IsEmptySlot(group)) membersByDropGroup_[group].push_back(inx);
        }
    }
}

void DropGroupCatalog::AddItem(const std::string& inxName, const std::string& displayName) {
    if (!inxName.empty()) itemNames_[inxName] = displayName;
}

const std::vector<DropGroupRow>* DropGroupCatalog::Group(const std::string& name) const {
    const auto it = groups_.find(name);
    return it == groups_.end() ? nullptr : &it->second;
}

std::vector<std::string> DropGroupCatalog::ItemsForGroupItemId(const std::string& itemId) const {
    if (const auto it = membersByDropGroup_.find(itemId); it != membersByDropGroup_.end()) return it->second;
    if (itemNames_.contains(itemId)) return {itemId};
    return {};
}

std::vector<std::string> DropGroupCatalog::ItemsForGroup(const std::string& name) const {
    std::set<std::string> items;
    if (const auto* rows = Group(name))
        for (const auto& row : *rows)
            for (auto& inx : ItemsForGroupItemId(row.itemId)) items.insert(std::move(inx));
    return {items.begin(), items.end()};
}

DropSlotStatus DropGroupCatalog::Check(const std::string& slotValue) const {
    if (IsEmptySlot(slotValue)) return DropSlotStatus::Empty;
    if (!hasGroups_) return DropSlotStatus::Unknown;
    const auto* rows = Group(slotValue);
    if (!rows) return DropSlotStatus::MissingGroup;
    for (const auto& row : *rows)
        if (!ItemsForGroupItemId(row.itemId).empty()) return DropSlotStatus::Resolved;
    return DropSlotStatus::GroupWithoutItems;
}

std::string DropGroupCatalog::DisplayName(const std::string& inxName) const {
    const auto it = itemNames_.find(inxName);
    return it == itemNames_.end() || it->second.empty() ? inxName : it->second;
}

} // namespace theseed::mapeditor::core::legacy
