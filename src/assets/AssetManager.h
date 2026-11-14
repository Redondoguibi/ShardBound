#pragma once
#include "assets/AssetId.h"
#include "assets/ShaderCompiler.h"
#include "assets/VirtualFileSystem.h"
#include "renderer/Renderer.h"
#include "assets/ModelAnimation.h"
#include <map>
namespace game {
struct TextureSource { AssetId id; std::filesystem::path source; bool srgb = true, nearest = true; };
struct ModelSource { AssetId id; std::filesystem::path source; };
struct SoundSource { AssetId id; std::filesystem::path source; bool streaming = false; };
struct StudioPackage;
struct ModelResource { std::vector<RenderItem> parts; std::vector<std::string> names; ModelHierarchy hierarchy; std::string studioModelId; std::vector<size_t> partNodes; std::shared_ptr<const StudioPackage> studio; };
using ModelHandle = std::shared_ptr<const ModelResource>;
class AssetManager {
public:
    AssetManager(VirtualFileSystem& vfs, Renderer& renderer, std::filesystem::path cache);
    std::unique_ptr<AssetManager> stage(VirtualFileSystem& files) const;
    void publish(AssetManager& staged) noexcept;
    void registerTexture(TextureSource source);
    TextureHandle texture(const AssetId& id);
    void registerModel(ModelSource source);
    ModelHandle model(const AssetId& id);
    void registerSound(SoundSource source);
    SoundSource sound(const AssetId& id) const;
    void registerStudioPackage(const AssetId& id,const std::filesystem::path& manifest);
    void registerShader(ShaderSource source);
    ShaderInstanceHandle shaderInstance(const std::string& id,ShaderParameters parameters={},const std::string& texture={},ShaderKind kind=ShaderKind::Surface);
    Renderer& renderer() { return renderer_; }
    void unmount(const std::string& nameSpace);
    unsigned cacheHits() const { return hits_; }
    unsigned cacheMisses() const { return misses_; }
private:
    VirtualFileSystem& vfs_;
    Renderer& renderer_;
    std::filesystem::path cache_;
    struct LoadedShader { ShaderSource source;ShaderHandle shader; };
    std::map<std::string,LoadedShader> shaders_;
    std::map<std::string,TextureSource> textures_;
    std::map<std::string,std::weak_ptr<const TextureResource>> loaded_;
    unsigned hits_ = 0, misses_ = 0;
    std::map<std::string,ModelSource> models_;
    std::map<std::string,SoundSource> sounds_;
    std::map<std::string,std::weak_ptr<const ModelResource>> loadedModels_;
    std::map<std::string,std::shared_ptr<const StudioPackage>> studioModels_;
};
}
