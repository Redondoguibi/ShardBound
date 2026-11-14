#include "scripting/ExtensionBridge.h"
#include "scripting/DataConversion.h"
#include <cmath>
#include "scripting/ScriptCheck.h"
namespace game {
namespace {
using namespace script;
void arity(const std::vector<Value>& a,size_t n,const Location& at) { if(a.size()!=n) fail(at,"BRIDGE101","Expected "+std::to_string(n)+" arguments"); }
AssetId id(const Value& v,const Location& at) { const auto* symbol=std::get_if<Symbol>(&v.data); return AssetId(symbol ? symbol->name : v.string(at)); }
unsigned version(const Value& v,const Location& at) { auto n=v.number(at); if(!std::isfinite(n) || n<1 || n>1000000 || n!=std::floor(n)) fail(at,"EXT102","Version must be an integer from 1 to 1000000"); return static_cast<unsigned>(n); }
void own(Runtime& runtime,const AssetId& type,const Location& at) { if(type.nameSpace()!=runtime.currentMod()) fail(at,"EXT111","Registration requires own mod namespace"); }
}
ExtensionBridge::ExtensionBridge(script::Runtime& runtime,World& world,Components& components,SaveGame& saves):runtime_(runtime),saves_(saves) {
    using namespace script;
    saves.validateState=[&runtime](const Data& state) {
        auto errors=runtime.emit("on_validate_load",{fromData(state)});
        if(!errors.empty()) throw DiagnosticError(errors.front());
    };
    const auto inspect=[&world](EntityId entity) {
        auto row=object();const auto& identity=world.get<IdentityComponent>(entity);
        row.setMember("key",Value(identity.persistent),{});row.setMember("name",Value(world.get<NameComponent>(entity).name),{});row.setMember("definition",Value(identity.definition),{});row.setMember("owner",Value(identity.owner),{});row.setMember("active",Value(world.get<ActiveComponent>(entity).value),{});
        auto custom=object();for(const auto& [type,value]:world.custom(entity)) { auto item=object();item.setMember("version",Value(double(value.version)),{});item.setMember("data",fromData(value.data),{});custom.setMember(type,item,{}); }row.setMember("components",custom,{});
        auto inventory=object();if(world.has<InventoryComponent>(entity)) for(const auto& [id,count]:world.get<InventoryComponent>(entity).items) inventory.setMember(id,Value(double(count)),{});row.setMember("inventory",inventory,{});
        if(world.has<TransformComponent>(entity)) { const auto p=world.get<TransformComponent>(entity).local.position;auto position=object();position.setMember("x",Value(double(p.x)),{});position.setMember("y",Value(double(p.y)),{});position.setMember("z",Value(double(p.z)),{});row.setMember("position",position,{}); }
        if(world.has<HealthComponent>(entity)) { const auto health=world.get<HealthComponent>(entity);auto value=object();value.setMember("current",Value(double(health.current)),{});value.setMember("maximum",Value(double(health.maximum)),{});row.setMember("health",value,{}); }
        if(auto parent=world.parent(entity)) row.setMember("parent",Value(world.get<IdentityComponent>(parent).persistent),{});
        if(world.has<ModelComponent>(entity)) row.setMember("model",Value(world.get<ModelComponent>(entity).definition.str()),{});
        if(world.has<InteractableComponent>(entity)) row.setMember("interactable",Value(world.get<InteractableComponent>(entity).enabled),{});
        return row;
    };
    runtime.bind("inspect_entity",native([&world,inspect](const auto& a,const auto& at) { arity(a,1,at);return inspect(requireEntity(a[0],world,at)); }));
    runtime.bind("write_debug_report",native([&runtime,&world,&saves,inspect](const auto& a,const auto& at) {
        arity(a,0,at);auto report=fromData(runtime.debugSnapshot());std::vector<Value> entities;
        for(auto entity:world.entities()) if(!saves.includeEntity || saves.includeEntity(entity)) entities.push_back(inspect(entity));report.setMember("entities",list(std::move(entities)),at);
        return Value(saves.writeDebugReport(toData(report)).string());
    }));
    runtime.bind("script_error",native([](const auto& a,const auto& at)->Value { arity(a,1,at); fail(at,"SCRIPT325",a[0].string(at)); }));
    runtime.bind("register_save_migration",native([&](const auto& a,const auto& at) {
        arity(a,2,at);const auto from=version(a[0],at);const auto owner=runtime.currentMod();const auto file=a[1].string(at);const auto root=runtime.moduleRoot(owner);
        const auto checked=checkScripts(root,file);if(!checked.diagnostics.empty()) throw DiagnosticError(checked.diagnostics.front());
        saves.registerMigration(owner,from,[owner,root,file](const Data& data){
            Runtime isolated;isolated.bind("record",native([](const auto& args,const auto& loc){arity(args,0,loc);return object();}));
            isolated.bind("script_error",native([](const auto& args,const auto& loc)->Value {arity(args,1,loc);fail(loc,"SCRIPT325",args[0].string(loc));}));
            isolated.load(owner,root,file);return toData(isolated.invoke(isolated.global(owner,"migrate"),{fromData(data)}));
        });return Value{};
    }));
    runtime.bind("record",native([](const auto& a,const auto& at) { arity(a,0,at); return object(); }));
    runtime.bind("entity_key",native([&](const auto& a,const auto& at) { arity(a,1,at); return Value(world.get<IdentityComponent>(requireEntity(a[0],world,at)).persistent); }));
    runtime.bind("get_entity_by_key",native([&](const auto& a,const auto& at) { arity(a,1,at); return entityValue(world,world.findPersistent(a[0].string(at))); }));
    runtime.bind("register_component",native([&](const auto& a,const auto& at) {
        arity(a,1,at); const auto type=id(a[0],at); own(runtime,type,at);
        const auto& data=runtime.registry().get("Components",type.str(),at).data;
        components.define(type,version(data.member("version",at),at),toData(data.member("Defaults",at))); return Value{};
    }));
    runtime.bind("register_migration",native([&](const auto& a,const auto& at) {
        arity(a,3,at); const auto type=id(a[0],at); own(runtime,type,at); const auto function=a[2];
        if(!std::holds_alternative<std::shared_ptr<Callable>>(function.data)) fail(at,"SCRIPT323","Expected migration function");
        components.migration(type,version(a[1],at),[&runtime,function](const Data& data) { return toData(runtime.invoke(function,{fromData(data)})); }); return Value{};
    }));
    runtime.bind("add_component",native([&](const auto& a,const auto& at) { arity(a,2,at); components.add(world,requireEntity(a[0],world,at),id(a[1],at)); return Value{}; }));
    runtime.bind("get_component",native([&](const auto& a,const auto& at) { arity(a,2,at); return fromData(components.get(world,requireEntity(a[0],world,at),id(a[1],at))); }));
    runtime.bind("set_component",native([&](const auto& a,const auto& at) { arity(a,3,at); components.set(world,requireEntity(a[0],world,at),id(a[1],at),toData(a[2])); return Value{}; }));
    runtime.bind("has_component",native([&](const auto& a,const auto& at) { arity(a,2,at); return Value(world.custom(requireEntity(a[0],world,at)).contains(id(a[1],at).str())); }));
    runtime.bind("get_entities_with",native([&](const auto& a,const auto& at) {
        arity(a,1,at); const auto type=id(a[0],at);
        if(!components.known(type)) fail(at,"EXT102","Component type is not registered");
        std::vector<Value> result;
        for(auto entity:world.entitiesWithCustom(type.str())) result.push_back(entityValue(world,entity));
        return list(std::move(result));
    }));
    runtime.bind("remove_component",native([&](const auto& a,const auto& at) { arity(a,2,at); return Value(world.removeCustom(requireEntity(a[0],world,at),id(a[1],at))); }));
    runtime.bind("on_event",native([&](const auto& a,const auto& at) { arity(a,2,at); return Value(double(runtime.subscribe(a[0].string(at),a[1]))); }));
    runtime.bind("add_hook",native([&](const auto& a,const auto& at) { arity(a,2,at); return Value(double(runtime.subscribe(a[0].string(at),a[1],true))); }));
    runtime.bind("unsubscribe",native([&](const auto& a,const auto& at) {
        arity(a,1,at); const auto n=a[0].number(at); if(!std::isfinite(n) || n<1 || n>9007199254740991.0 || n!=std::floor(n)) fail(at,"EXT112","Invalid subscription token");
        return Value(runtime.unsubscribe(static_cast<std::uint64_t>(n)));
    }));
    runtime.bind("emit_event",native([&](const auto& a,const auto& at) {
        arity(a,2,at); const auto* values=std::get_if<std::shared_ptr<List>>(&a[1].data); if(!values) fail(at,"SCRIPT301","Event arguments must be a list");
        auto errors=runtime.emit(a[0].string(at),(*values)->values); if(!errors.empty()) throw DiagnosticError(errors.front()); return Value{};
    }));
    runtime.bind("apply_hook",native([&](const auto& a,const auto& at) { arity(a,2,at); return runtime.filter(a[0].string(at),a[1]); }));
    runtime.bind("get_definition",native([&](const auto& a,const auto& at) { arity(a,2,at); return clone(runtime.registry().get(a[0].string(at),id(a[1],at).str(),at).data); }));
    runtime.bind("replace_definition",native([&](const auto& a,const auto& at) { arity(a,3,at); (void)toData(a[2]); runtime.registry().replace(a[0].string(at),id(a[1],at).str(),a[2],runtime.currentMod(),at); return Value{}; }));
    runtime.bind("set_save_state",native([&](const auto& a,const auto& at) { arity(a,1,at); saves.setState(runtime.currentMod(),toData(a[0])); return Value{}; }));
    runtime.bind("get_save_state",native([&](const auto& a,const auto& at) { arity(a,0,at); return fromData(saves.state(runtime.currentMod())); }));
    runtime.bind("save_game",native([this](const auto& a,const auto& at) { arity(a,1,at); if(pending_) fail(at,"SAVE105","Save/load operation already pending"); pending_={{false,a[0].string(at)}}; return Value{}; }));
    runtime.bind("load_game",native([this](const auto& a,const auto& at) { arity(a,1,at); if(pending_) fail(at,"SAVE105","Save/load operation already pending"); pending_={{true,a[0].string(at)}}; return Value{}; }));
}
std::vector<Diagnostic> ExtensionBridge::process() {
    if(!pending_) return {};
    const auto request=*pending_; std::vector<Diagnostic> diagnostics;
    try {
        if(request.first) {
            diagnostics=saves_.load(request.second);
            auto events=runtime_.emit("on_loaded"); diagnostics.insert(diagnostics.end(),events.begin(),events.end());
        } else {
            diagnostics=runtime_.emit("on_before_save");
            if(diagnostics.empty()) { saves_.save(request.second); diagnostics=runtime_.emit("on_saved"); }
        }
    } catch(const DiagnosticError& error) { diagnostics.push_back(error.diagnostic); }
    catch(const std::exception& error) { diagnostics.push_back({"SAVE106",error.what()}); }
    if(!diagnostics.empty()) {
        auto errors=runtime_.emit("on_save_diagnostic",{script::Value(request.first),script::Value(diagnostics.front().message)});
        diagnostics.insert(diagnostics.end(),errors.begin(),errors.end());
    }
    pending_.reset(); return diagnostics;
}
}
