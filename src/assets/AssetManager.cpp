#include "assets/AssetManager.h"
#include "assets/TextureCompiler.h"
#include "assets/ModelCompiler.h"
#include "core/Diagnostic.h"
#include "core/Log.h"
namespace game {
AssetManager::AssetManager(VirtualFileSystem& vfs, Renderer& renderer, std::filesystem::path cache)
    : vfs_(vfs),renderer_(renderer),cache_(std::move(cache)) {}
void AssetManager::registerTexture(TextureSource source) {
    const auto id = source.id.str();
    if (!textures_.emplace(id,std::move(source)).second) throw DiagnosticError({"ASSET301","Duplicate texture registration",{},{},id});
}
TextureHandle AssetManager::texture(const AssetId& id) {
    if (auto handle = loaded_[id.str()].lock()) return handle;
    const auto it = textures_.find(id.str());
    if (it == textures_.end()) throw DiagnosticError({"ASSET302","Texture not registered",{},id.nameSpace(),id.str()});
    const auto& entry = it->second;
    try {
        const auto source = vfs_.resolve(id.nameSpace(),entry.source);
        auto data = compileTexture(source,cache_ / id.nameSpace(),entry.srgb,entry.nearest);
        data.cacheHit ? ++hits_ : ++misses_;
        LOG_INFO("ASSETS",std::string(data.cacheHit ? "CACHE HIT " : "CACHE MISS ")+id.str());
        auto handle = renderer_.createTexture(data.width,data.height,data.pixels,entry.srgb,entry.nearest);
        loaded_[id.str()] = handle; return handle;
    } catch (const DiagnosticError& e) {
        auto diagnostic = e.diagnostic; diagnostic.mod = id.nameSpace(); diagnostic.resource = id.str(); throw DiagnosticError(std::move(diagnostic));
    }
}
void AssetManager::unmount(const std::string& nameSpace) {
    std::erase_if(models_,[&](const auto& pair) { return pair.second.id.nameSpace() == nameSpace; });
    std::erase_if(loadedModels_,[&](const auto& pair) { return AssetId(pair.first).nameSpace() == nameSpace; });
    std::erase_if(textures_,[&](const auto& pair) { return pair.second.id.nameSpace() == nameSpace; });
    std::erase_if(loaded_,[&](const auto& pair) { return AssetId(pair.first).nameSpace() == nameSpace; });
    vfs_.unmount(nameSpace);
}
void AssetManager::registerModel(ModelSource source) {
    const auto id=source.id.str();
    if (!models_.emplace(id,std::move(source)).second) throw DiagnosticError({"ASSET305","Duplicate model registration",{},{},id});
}
ModelHandle AssetManager::model(const AssetId& id) {
    if (auto result=loadedModels_[id.str()].lock()) return result;
    auto it=models_.find(id.str());
    if (it==models_.end()) throw DiagnosticError({"ASSET306","Model not registered",{},id.nameSpace(),id.str()});
    auto data=compileModel(vfs_.resolve(id.nameSpace(),it->second.source),cache_/id.nameSpace());
    data.cacheHit ? ++hits_ : ++misses_;
    LOG_INFO("ASSETS",std::string(data.cacheHit ? "CACHE HIT " : "CACHE MISS ")+id.str());
    auto result=std::make_shared<ModelResource>();
    for (const auto& part:data.parts) {
        RenderItem item; item.mesh=renderer_.createMesh(part.vertices,part.indices); item.world=part.transform; item.material.color=part.color;
        if (!part.texture.pixels.empty()) item.material.texture=renderer_.createTexture(part.texture.width,part.texture.height,part.texture.pixels);
        result->parts.push_back(std::move(item));
    }
    loadedModels_[id.str()]=result; return result;
}
}
