#pragma once
#include "mods/Manifest.h"
#include "assets/AssetManager.h"
#include <functional>
namespace game {
struct LoadReport { std::vector<ModManifest> mods; std::vector<Diagnostic> diagnostics; size_t registeredAssets=0; };
using PreparationProgress=std::function<void(std::string_view,size_t,size_t)>;
class ModLoader {
public:
    ModLoader(VirtualFileSystem& vfs,AssetManager& assets) : vfs_(vfs),assets_(assets) {}
    LoadReport load(const std::filesystem::path& directory,PreparationProgress progress = {});
private:
    VirtualFileSystem& vfs_;
    AssetManager& assets_;
};
}
