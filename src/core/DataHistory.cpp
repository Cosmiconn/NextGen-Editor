#include "mapeditor/core/DataHistory.hpp"

#include <utility>

namespace theseed::mapeditor::core {

void DataHistory::Begin(std::string label) {
    if (depth_++ == 0) {
        open_ = DataTransaction{};
        open_.label = std::move(label);
    }
}

bool DataHistory::Commit() {
    if (depth_ == 0) return false;
    if (--depth_ > 0) return false;
    if (open_.operations.empty()) {
        open_ = DataTransaction{};
        return false;
    }
    undo_.push_back(std::move(open_));
    open_ = DataTransaction{};
    while (undo_.size() > limit_) undo_.pop_front();
    redo_.clear();
    ++revision_;
    return true;
}

void DataHistory::Abort() {
    if (depth_ == 0) return;
    for (auto it = open_.operations.rbegin(); it != open_.operations.rend(); ++it)
        if (it->undo) it->undo();
    open_ = DataTransaction{};
    depth_ = 0;
    ++revision_;
}

void DataHistory::Record(DataOperation operation, std::string labelIfStandalone) {
    if (depth_ > 0) {
        open_.operations.push_back(std::move(operation));
        return;
    }
    DataTransaction t;
    t.label = std::move(labelIfStandalone);
    t.operations.push_back(std::move(operation));
    undo_.push_back(std::move(t));
    while (undo_.size() > limit_) undo_.pop_front();
    redo_.clear();
    ++revision_;
}

bool DataHistory::Undo() {
    if (depth_ > 0 || undo_.empty()) return false;
    DataTransaction t = std::move(undo_.back());
    undo_.pop_back();
    for (auto it = t.operations.rbegin(); it != t.operations.rend(); ++it)
        if (it->undo) it->undo();
    redo_.push_back(std::move(t));
    ++revision_;
    return true;
}

bool DataHistory::Redo() {
    if (depth_ > 0 || redo_.empty()) return false;
    DataTransaction t = std::move(redo_.back());
    redo_.pop_back();
    for (auto& op : t.operations)
        if (op.redo) op.redo();
    undo_.push_back(std::move(t));
    ++revision_;
    return true;
}

void DataHistory::Clear() {
    undo_.clear();
    redo_.clear();
    open_ = DataTransaction{};
    depth_ = 0;
    ++revision_;
}

} // namespace theseed::mapeditor::core
