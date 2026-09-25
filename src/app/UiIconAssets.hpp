#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace theseed::mapeditor::app {

// Runtime bridge between stable semantic icon IDs (docs/ui-vision/ICON_INVENTORY.md)
// and the approved small-size PNG exports under assets/ui/icons/png.
//
// Missing files are intentionally non-fatal: callers keep their existing DrawList fallback
// until the corresponding approved package asset has been committed. This lets the migration
// happen incrementally without ever presenting a dead button.
class UiIconAssets {
public:
    ~UiIconAssets();

    // Locates assets/ui/icons relative to the executable/current checkout.
    // Returns false when no asset root exists; Texture() will then simply return 0.
    bool Init();
    void Shutdown();

    [[nodiscard]] std::uint32_t Texture(std::string_view semanticId, int requestedSize);
    [[nodiscard]] bool HasAssetRoot() const { return !assetRoot_.empty(); }
    [[nodiscard]] const std::filesystem::path& AssetRoot() const { return assetRoot_; }

private:
    [[nodiscard]] static std::filesystem::path FindAssetRoot();
    [[nodiscard]] static std::filesystem::path RelativePath(std::string_view semanticId, int size);
    [[nodiscard]] static int NormalizeSize(int requestedSize);

    std::filesystem::path assetRoot_;
    std::unordered_map<std::string, std::uint32_t> textures_;
};

} // namespace theseed::mapeditor::app
