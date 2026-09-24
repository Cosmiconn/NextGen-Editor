#pragma once
#include "mapeditor/core/KfmFile.hpp"
#include <functional>

namespace theseed::mapeditor::app {
class KfmPanel {
public:
    void Draw(const std::function<std::optional<std::string>()>& browse);
    bool Open(const std::filesystem::path& path);
private:
    void Filter();
    char path_[4096]{}, exportPath_[4096]{}, filter_[256]{};
    std::filesystem::path source_;
    std::optional<core::KfmFile> file_;
    std::optional<core::KfmReferences> references_;
    std::vector<std::size_t> visible_;
    std::size_t selected_ = 0, transitionCount_ = 0;
    std::string message_;
};
}
