#include "scripting/ShaderBridge.h"
#include <cmath>
#include <limits>
namespace game {
namespace {
using namespace script;
struct Options { ShaderParameters parameters{};std::string texture;int priority=0; };
Options options(const std::vector<Value>& args,const Location& at,bool post=false) {
    if(args.size()<2 || args.size()>3) fail(at,"SHADER201","Expected target, shader ID (or null), and optional options object");
    Options out;if(args.size()==2) return out;
    const auto data=std::get_if<std::shared_ptr<Object>>(&args[2].data);
    if(!data) fail(at,"SHADER201","Shader options must be an object");
    for(const auto& [key,value]:(*data)->fields) {
        if(key=="parameters") {
            const auto values=std::get_if<std::shared_ptr<List>>(&value.data);
            if(!values || (*values)->values.size()>16) fail(at,"SHADER202","Shader parameters must be a list of at most 16 numbers");
            for(size_t i=0;i<(*values)->values.size();++i) {
                const auto n=(*values)->values[i].number(at);
                if(!std::isfinite(n) || std::abs(n)>std::numeric_limits<float>::max()) fail(at,"SHADER202","Shader parameter must be a finite float");
                out.parameters[i]=float(n);
            }
        } else if(key=="texture") out.texture=value.string(at);
        else if(key=="priority" && post) {
            const auto n=value.number(at);
            if(!std::isfinite(n) || n!=std::floor(n) || n < -10000 || n>10000) fail(at,"SHADER203","Postprocess priority must be an integer in -10000..10000");
            out.priority=int(n);
        } else fail(at,"SHADER201","Unknown shader option: "+key);
    }
    return out;
}
ShaderInstanceHandle instance(AssetManager& assets,const Value& value,const Options& opts,ShaderKind kind,const Location& at) {
    if(std::holds_alternative<std::monostate>(value.data)) return {};
    return assets.shaderInstance(value.string(at),opts.parameters,opts.texture,kind);
}
}
void bindShaderApi(script::Runtime& runtime,World& world,AssetManager& assets) {
    using namespace script;
    runtime.bind("set_entity_shader",native([&](const auto& args,const auto& at) {
        const auto opts=options(args,at);const auto entity=requireEntity(args[0],world,at);
        if(!world.has<ModelComponent>(entity)) fail(at,"SHADER204","Entity has no model");
        auto shader=instance(assets,args[1],opts,ShaderKind::Surface,at);
        world.get<ModelComponent>(entity).shader=std::move(shader);return Value{};
    }));
    runtime.bind("entity_shader",native([&](const auto& args,const auto& at) {
        if(args.size()!=1) fail(at,"SHADER201","entity_shader expects one entity");
        const auto entity=requireEntity(args[0],world,at);
        if(!world.has<ModelComponent>(entity) || !world.get<ModelComponent>(entity).shader) return Value{};
        const auto& shader=*world.get<ModelComponent>(entity).shader;auto value=object();
        value.setMember("shader",Value(shader.shaderId),at);value.setMember("texture",Value(shader.textureId),at);value.setMember("available",Value(bool(shader.shader)),at);
        std::vector<Value> params;for(float n:shader.parameters) params.emplace_back(double(n));value.setMember("parameters",list(std::move(params)),at);return value;
    }));
    runtime.bind("set_texture_shader",native([&](const auto& args,const auto& at) {
        const auto opts=options(args,at);auto texture=assets.texture(AssetId(args[0].string(at)));
        auto shader=instance(assets,args[1],opts,ShaderKind::Surface,at);
        assets.renderer().setTextureShader(std::move(texture),std::move(shader),runtime.currentMod());return Value{};
    }));
    runtime.bind("set_post_effect",native([&](const auto& args,const auto& at) {
        const auto opts=options(args,at,true);const AssetId id(args[0].string(at));
        if(id.nameSpace()!=runtime.currentMod()) fail(at,"SHADER205","Postprocess pass must use its owner's namespace");
        auto shader=instance(assets,args[1],opts,ShaderKind::PostProcess,at);
        if(shader) assets.renderer().setPostEffect(id.str(),std::move(shader),opts.priority);
        else assets.renderer().removePostEffect(id.str());
        return Value{};
    }));
}
}
