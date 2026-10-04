#pragma once
#include "assets/AssetId.h"
#include "assets/VirtualFileSystem.h"
#include "renderer/Renderer.h"
#include <map>
namespace game {
struct TextureSource { AssetId id; std::filesystem::path source; bool srgb = true, nearest = true; };
struct ModelSource { AssetId id; std::filesystem::path source; };
struct SoundSource { AssetId id; std::filesystem::path source; bool streaming = false; };
struct ModelResource { std::vector<RenderItem> parts; std::vector<std::string> names; };
using ModelHandle = std::shared_ptr<const ModelResource>;
class AssetManager {
public:
    AssetManager(VirtualFileSystem& vfs, Renderer& renderer, std::filesystem::path cache);
    void registerTexture(TextureSource source);
    TextureHandle texture(const AssetId& id);
    void registerModel(ModelSource source);
    ModelHandle model(const AssetId& id);
    void registerSound(SoundSource source);
    SoundSource sound(const AssetId& id) const;
    void unmount(const std::string& nameSpace);
    unsigned cacheHits() const { return hits_; }
    unsigned cacheMisses() const { return misses_; }
private:
    VirtualFileSystem& vfs_;
    Renderer& renderer_;
    std::filesystem::path cache_;
    std::map<std::string,TextureSource> textures_;
    std::map<std::string,std::weak_ptr<const TextureResource>> loaded_;
    unsigned hits_ = 0, misses_ = 0;
    std::map<std::string,ModelSource> models_;
    std::map<std::string,SoundSource> sounds_;
    std::map<std::string,std::weak_ptr<const ModelResource>> loadedModels_;
};
}
