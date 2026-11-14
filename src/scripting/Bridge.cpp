#include "scripting/Bridge.h"
#include "scripting/ShaderBridge.h"
#include "core/Log.h"
#include <cmath>
#include <limits>
namespace game {
namespace {
using namespace script;
void arity(const std::vector<Value>& args,size_t count,const Location& at) {
    if(args.size()!=count) fail(at,"BRIDGE101","Expected "+std::to_string(count)+" arguments");
}
float number(const Value& value,const Location& at) {
    const auto n=value.number(at);
    if(!std::isfinite(n) || std::abs(n)>std::numeric_limits<float>::max()) fail(at,"BRIDGE102","Invalid coordinate");
    return static_cast<float>(n);
}
Value vector(Vec3 v) {
    auto result=object(); auto& fields=std::get<std::shared_ptr<Object>>(result.data)->fields;
    fields={{"x",Value(double(v.x))},{"y",Value(double(v.y))},{"z",Value(double(v.z))}}; return result;
}
Vec3 vector(const Value& value,const Location& at) { return {number(value.member("x",at),at),number(value.member("y",at),at),number(value.member("z",at),at)}; }
class PositionRef final : public HostObject {
    World& world_; EntityId id_;
public:
    PositionRef(World& world,EntityId id) : world_(world),id_(id) {}
    Value get(const std::string& key,const Location& at) override {
        if(!world_.alive(id_)) fail(at,"BRIDGE103","Entity was destroyed");
        const auto& p=world_.get<TransformComponent>(id_).local.position;
        if(key=="x") return Value(double(p.x)); if(key=="y") return Value(double(p.y)); if(key=="z") return Value(double(p.z));
        fail(at,"BRIDGE104","Unknown position member: "+key);
    }
    void set(const std::string& key,Value value,const Location& at) override {
        if(!world_.alive(id_)) fail(at,"BRIDGE103","Entity was destroyed");
        auto& p=world_.get<TransformComponent>(id_).local.position; const auto v=number(value,at);
        if(key=="x") p.x=v; else if(key=="y") p.y=v; else if(key=="z") p.z=v;
        else fail(at,"BRIDGE104","Unknown position member: "+key);
    }
};
class EntityRef final : public HostObject {
public:
    World& world; EntityId id;
    EntityRef(World& w,EntityId value) : world(w),id(value) {}
    bool equals(const HostObject& other) const override {
        const auto* entity=dynamic_cast<const EntityRef*>(&other);return entity && &world==&entity->world && id==entity->id;
    }
    Value get(const std::string& key,const Location& at) override {
        if(!world.alive(id)) fail(at,"BRIDGE103","Entity was destroyed");
        if(key=="position") return Value(std::shared_ptr<HostObject>(std::make_shared<PositionRef>(world,id)));
        if(key=="name") return Value(world.get<NameComponent>(id).name);
        if(key=="active") return Value(world.get<ActiveComponent>(id).value);
        fail(at,"BRIDGE104","Unknown entity member: "+key);
    }
    void set(const std::string& key,Value value,const Location& at) override {
        if(!world.alive(id)) fail(at,"BRIDGE103","Entity was destroyed");
        if(key=="position") world.get<TransformComponent>(id).local.position=vector(value,at);
        else if(key=="name") world.get<NameComponent>(id).name=value.string(at);
        else if(key=="active") world.get<ActiveComponent>(id).value=value.boolean(at);
        else fail(at,"BRIDGE104","Unknown entity member: "+key);
    }
};
Value entity(World& world,EntityId id) { return id ? Value(std::shared_ptr<HostObject>(std::make_shared<EntityRef>(world,id))) : Value{}; }
const Value* field(const Value& value,const std::string& key) {
    if(auto p=std::get_if<std::shared_ptr<Object>>(&value.data)) { const auto it=(*p)->fields.find(key); if(it!=(*p)->fields.end()) return &it->second; }
    return nullptr;
}
}
script::Value entityValue(World& world,EntityId id) { return entity(world,id); }
EntityId requireEntity(const script::Value& value,World& world,const script::Location& at) {
    auto host=std::get_if<std::shared_ptr<script::HostObject>>(&value.data);
    auto ref=host ? std::dynamic_pointer_cast<EntityRef>(*host) : nullptr;
    if(!ref || &ref->world!=&world || !world.alive(ref->id)) script::fail(at,"BRIDGE103","Expected live entity");
    return ref->id;
}
void bindRuntimeApi(script::Runtime& scripting,World& world,AssetManager& assets) {
    bindShaderApi(scripting,world,assets);
    scripting.bind("is_studio_model",native([&](const auto& a,const auto& at){arity(a,1,at);auto e=requireEntity(a[0],world,at);return Value(world.has<ModelComponent>(e)&&world.get<ModelComponent>(e).model&&bool(world.get<ModelComponent>(e).model->studio));}));
    using namespace script;
    scripting.bind("vec3",native([](const auto& args,const auto& at) { arity(args,3,at); return vector({number(args[0],at),number(args[1],at),number(args[2],at)}); }));
    scripting.bind("spawn",native([&](const auto& args,const auto& at) {
        arity(args,2,at);
        const auto definition=std::get_if<Symbol>(&args[0].data);
        const auto id=definition ? definition->name : args[0].string(at);
        const auto& data=scripting.registry().get("Entities",id,at).data;
        const auto position=vector(args[1],at);
        ModelHandle model; std::string modelId;
        if(auto assetSection=field(data,"Assets")) if(auto source=field(*assetSection,"model")) {
            modelId=source->string(at); model=assets.model(AssetId(modelId));
        }
        auto created=world.create(id);
        try {
            world.get<IdentityComponent>(created).definition=id;
            world.get<IdentityComponent>(created).owner=AssetId(id).nameSpace();
            world.add(created,TransformComponent{{position}});
            if(model) world.add(created,ModelComponent{AssetId(modelId),model});
        } catch(...) { world.destroy(created); throw; }
        LOG_INFO("SCRIPT","Spawned "+id+" as #"+std::to_string(created.value));
        return entity(world,created);
    }));
    scripting.bind("get_entity",native([&](const auto& args,const auto& at) { arity(args,1,at); return entity(world,world.find(args[0].string(at))); }));
    scripting.bind("destroy",native([&](const auto& args,const auto& at) {
        arity(args,1,at);
        auto host=std::get_if<std::shared_ptr<HostObject>>(&args[0].data);
        auto ref=host ? std::dynamic_pointer_cast<EntityRef>(*host) : nullptr;
        if(!ref || &ref->world!=&world || !world.alive(ref->id)) fail(at,"BRIDGE103","Expected live entity");
        world.destroy(ref->id); return Value{};
    }));
    scripting.bind("log",native([](const auto& args,const auto& at) { arity(args,1,at); LOG_INFO("SCRIPT",args[0].string(at)); return Value{}; }));
}
}
