#include "assets/AssetManager.h"
#include "assets/TextureCompiler.h"
#include "assets/ModelCompiler.h"
#include "core/Diagnostic.h"
#include "core/Log.h"
#include <cmath>
namespace game {
AssetManager::AssetManager(VirtualFileSystem& vfs, Renderer& renderer, std::filesystem::path cache)
    : vfs_(vfs),renderer_(renderer),cache_(std::move(cache)) {}
std::unique_ptr<AssetManager> AssetManager::stage(VirtualFileSystem& files) const {
    auto result=std::make_unique<AssetManager>(files,renderer_,cache_);
    result->shaders_=shaders_;result->textures_=textures_;result->loaded_=loaded_;result->models_=models_;result->sounds_=sounds_;
    result->loadedModels_=loadedModels_;result->studioModels_=studioModels_;result->hits_=hits_;result->misses_=misses_;return result;
}
void AssetManager::publish(AssetManager& staged) noexcept {
    shaders_.swap(staged.shaders_);textures_.swap(staged.textures_);loaded_.swap(staged.loaded_);models_.swap(staged.models_);sounds_.swap(staged.sounds_);
    loadedModels_.swap(staged.loadedModels_);studioModels_.swap(staged.studioModels_);hits_=staged.hits_;misses_=staged.misses_;
}
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
void AssetManager::registerShader(ShaderSource source) {
    const auto id=source.id;(void)AssetId(id);
    if(shaders_.contains(id)) throw DiagnosticError({"SHADER103","Duplicate shader registration",source.source.string(),AssetId(id).nameSpace(),id});
    if(shaders_.size()>=256) throw std::runtime_error("Shader asset limit exceeded (256)");
    auto shader=renderer_.createShader(compileShader(vfs_,source,renderer_.shaderLibrary()));
    shaders_.emplace(id,LoadedShader{std::move(source),std::move(shader)});
}
ShaderInstanceHandle AssetManager::shaderInstance(const std::string& id,ShaderParameters parameters,const std::string& textureId,ShaderKind kind) {
    const auto it=shaders_.find(id);
    if(it==shaders_.end()) throw DiagnosticError({"SHADER104","Shader not registered",{},AssetId(id).nameSpace(),id});
    if(it->second.source.kind!=kind) throw DiagnosticError({"SHADER105","Shader kind mismatch",{},AssetId(id).nameSpace(),id});
    for(float value:parameters) if(!std::isfinite(value)) throw std::runtime_error("Non-finite shader parameter");
    auto result=std::make_shared<ShaderInstance>();result->shader=it->second.shader;result->parameters=parameters;result->shaderId=id;result->textureId=textureId;
    if(!textureId.empty()) result->texture=texture(AssetId(textureId));
    return result;
}
void AssetManager::unmount(const std::string& nameSpace) {
    std::erase_if(shaders_,[&](const auto& p){return AssetId(p.first).nameSpace()==nameSpace;});
    std::erase_if(studioModels_,[&](const auto& p){return AssetId(p.first).nameSpace()==nameSpace;});
    std::erase_if(sounds_,[&](const auto& pair) { return pair.second.id.nameSpace() == nameSpace; });
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
void AssetManager::registerSound(SoundSource source) {
    const auto id=source.id.str(); (void)vfs_.resolve(source.id.nameSpace(),source.source);
    if(!sounds_.emplace(id,std::move(source)).second) throw DiagnosticError({"ASSET307","Duplicate sound registration",{},{},id});
}
SoundSource AssetManager::sound(const AssetId& id) const {
    const auto found=sounds_.find(id.str());
    if(found==sounds_.end()) throw DiagnosticError({"ASSET308","Sound not registered",{},id.nameSpace(),id.str()});
    auto result=found->second; result.source=vfs_.resolve(id.nameSpace(),result.source); return result;
}
ModelHandle AssetManager::model(const AssetId& id) {
    if (auto result=loadedModels_[id.str()].lock()) return result;
    auto it=models_.find(id.str());
    if (it==models_.end()) throw DiagnosticError({"ASSET306","Model not registered",{},id.nameSpace(),id.str()});
    auto data=compileModel(vfs_.resolve(id.nameSpace(),it->second.source),cache_/id.nameSpace());
    data.cacheHit ? ++hits_ : ++misses_;
    LOG_INFO("ASSETS",std::string(data.cacheHit ? "CACHE HIT " : "CACHE MISS ")+id.str());
    auto result=std::make_shared<ModelResource>();
    result->hierarchy=std::move(data.hierarchy);
    if(auto package=studioModels_.find(id.str());package!=studioModels_.end()) {result->studio=package->second;result->studioModelId=id.str();}
    for (const auto& part:data.parts) {
        RenderItem item; item.mesh=renderer_.createMesh(part.vertices,part.indices); item.world=part.transform; item.material.color=part.color;
        item.material.alphaMode=part.alphaMode; item.material.alphaCutoff=part.alphaCutoff; item.material.doubleSided=part.doubleSided;
        if (!part.texture.pixels.empty()) item.material.texture=renderer_.createTexture(part.texture.width,part.texture.height,part.texture.pixels);
        result->parts.push_back(std::move(item));
        result->names.push_back(part.name); result->partNodes.push_back(part.node);
    }
    loadedModels_[id.str()]=result; return result;
}
}
