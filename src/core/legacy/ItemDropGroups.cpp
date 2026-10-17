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

std::vector<std::string> DropGroupCatalog::ItemInfoDropGroupNames() const {
    std::vector<std::string> names;
    names.reserve(membersByDropGroup_.size());
    for (const auto& [name, members] : membersByDropGroup_) names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}

DropGroupColumns FindDropGroupColumns(const ShineTable& itemDropGroup) {
    DropGroupColumns c;
    c.group = ColumnIndex(itemDropGroup, "ItemGroupIdx");
    c.item = ColumnIndex(itemDropGroup, "ItemID");
    c.minQty = ColumnIndex(itemDropGroup, "MinQtty");
    c.maxQty = ColumnIndex(itemDropGroup, "MaxQtty");
    c.checksum = ColumnIndex(itemDropGroup, "CheckSum");
    for (int i = 0; i < 16; ++i) {
        const std::string name = std::string(i < 10 ? "Upgrade0" : "Upgrade") + std::to_string(i);
        if (const int col = ColumnIndex(itemDropGroup, name); col >= 0) c.upgrades.push_back(col);
    }
    return c;
}

namespace {
std::optional<long long> Number(const ShineRecord& record, int column) {
    const std::string v = Value(record, column);
    if (v.empty() || v.size() > 12 || !std::all_of(v.begin(), v.end(), [](unsigned char ch) { return ch >= '0' && ch <= '9'; }))
        return std::nullopt;
    return std::stoll(v);
}
} // namespace

std::optional<long long> DropGroupUpgradeSum(const DropGroupColumns& columns, const ShineRecord& record) {
    long long sum = 0;
    for (const int col : columns.upgrades) {
        const auto n = Number(record, col);
        if (!n) return std::nullopt;
        sum += *n;
    }
    return sum;
}

std::optional<long long> DropGroupChecksum(const DropGroupColumns& columns, const ShineRecord& record) {
    if (columns.minQty < 0 || columns.maxQty < 0 || columns.checksum < 0) return std::nullopt;
    const auto minQty = Number(record, columns.minQty);
    const auto maxQty = Number(record, columns.maxQty);
    const auto upgrades = DropGroupUpgradeSum(columns, record);
    if (!minQty || !maxQty || !upgrades) return std::nullopt;
    return *minQty + *maxQty + *upgrades;
}

bool UpdateDropGroupChecksum(const DropGroupColumns& columns, ShineRecord& record) {
    const auto sum = DropGroupChecksum(columns, record);
    if (!sum || static_cast<std::size_t>(columns.checksum) >= record.values.size()) return false;
    record.values[static_cast<std::size_t>(columns.checksum)] = std::to_string(*sum);
    return true;
}

ShineRecord MakeDropGroupRecord(const ShineTable& itemDropGroup, const std::string& group, const std::string& itemId,
                                int minQty, int maxQty) {
    const auto columns = FindDropGroupColumns(itemDropGroup);
    ShineRecord record;
    record.values.assign(itemDropGroup.columns.size(), "0");
    auto set = [&](int col, std::string v) {
        if (col >= 0 && static_cast<std::size_t>(col) < record.values.size()) record.values[static_cast<std::size_t>(col)] = std::move(v);
    };
    set(columns.group, group);
    set(columns.item, itemId);
    set(columns.minQty, std::to_string(minQty));
    set(columns.maxQty, std::to_string(maxQty));
    if (!columns.upgrades.empty()) set(columns.upgrades.front(), "1000");
    UpdateDropGroupChecksum(columns, record);
    return record;
}

std::vector<std::size_t> DropTableRecordsUsingGroup(const ShineTable& itemGroupTable, const std::string& group) {
    std::vector<int> slotColumns;
    for (std::size_t i = 0; i < itemGroupTable.columns.size(); ++i) {
        const std::string& name = itemGroupTable.columns[i].name;
        if (name.size() > 6 && name.compare(0, 6, "DrItem") == 0 &&
            std::all_of(name.begin() + 6, name.end(), [](unsigned char ch) { return ch >= '0' && ch <= '9'; }))
            slotColumns.push_back(static_cast<int>(i));
    }
    std::vector<std::size_t> out;
    if (group.empty()) return out;
    for (std::size_t r = 0; r < itemGroupTable.records.size(); ++r)
        for (const int col : slotColumns)
            if (Value(itemGroupTable.records[r], col) == group) { out.push_back(r); break; }
    return out;
}

} // namespace theseed::mapeditor::core::legacy
