#include "scripting/ExtensionBridge.h"
#include "scripting/GameplayBridge.h"
#include "scripting/DataConversion.h"
#include "extensions/NativeMods.h"
#include "platform/WindowSystem.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace game;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
template<class F> void rejects(F function,const char* message) { bool rejected=false; try { function(); } catch(const std::exception&) { rejected=true; } require(rejected,message); }
int main() {
    try {
        WindowSystem window({"Extensions",128,128,true}); Renderer renderer(window.nativeHandle(),128,128,"assets/core/shaders/forward.hlsl");
        VirtualFileSystem vfs; AssetManager assets(vfs,renderer,"user/extensions/cache");
        World world; PhysicsSystem physics(world); PlayerController player(world,physics); Components components;
        SaveGame saves("user/extensions/saves",world,physics,player,assets); saves.setMods({{"test","1.0.0"},{"missing","1.0.0"}});
        InputSystem input; AudioSystem audio(true); script::Runtime runtime;
        bindRuntimeApi(runtime,world,assets); bindGameplayApi(runtime,world,physics,player,input,audio,assets);
        ExtensionBridge extension(runtime,world,components,saves);
        runtime.execute("test",R"SL(
@Components
component Counter:
    .version = 2
    Defaults:
        .count = 0
register_component(Counter)
func migrate(old):
    old.count = old.count + 10
    return old
register_migration(Counter, 1, migrate)
@Entities
entity Actor:
    .health = 100
@Globals
actor = spawn(Actor, vec3(0, 2, 0))
events = 0
loaded = 0
hook_token = 0
func halve(context):
    context.amount = context.amount / 2
    return context
hook_token = add_hook("core:damage", halve)
add_component(actor, Counter)
set_health(actor, 100)
add_box(actor, vec3(0.5, 1, 0.5), true)
set_player(actor, 4)
damage(actor, 20)
@Events
on_damage(target, amount):
    events = events + 1
on_loaded():
    actor = get_entity_by_key(get_save_state().actor)
    loaded = loaded + 1
on_before_save():
    data = record()
    data.checkpoint = 7
    data.actor = entity_key(actor)
    set_save_state(data)
)SL");
        const auto actor=world.find("test:Actor"); require(world.get<HealthComponent>(actor).current==90,"Damage hook failed");
        world.setCustom(actor,AssetId("test:Counter"),{1,Data{Data::Object{{"count",Data{2.0}}}}});
        require(std::get<double>(std::get<Data::Object>(components.get(world,actor,AssetId("test:Counter")).value).at("count").value)==12,"Migration failed");
        runtime.execute("test","damage(actor, 10)\n"); require(runtime.global("test","events").number()==1,"Damage event missing");
        runtime.execute("test","unsubscribe(hook_token)\ndamage(actor, 10)\n"); require(world.get<HealthComponent>(actor).current==75,"Hook unsubscribe failed");
        world.setCustom(actor,AssetId("missing:opaque"),{9,Data{Data::Object{{"preserve",Data{true}}}}});
        const auto orphan=world.create("missing entity"); world.get<IdentityComponent>(orphan).definition="missing:enemy";
        auto child=world.create("orphan child"); world.setParent(child,orphan);
        const auto orphanKey=world.get<IdentityComponent>(orphan).persistent;
        saves.setState("missing",Data{Data::Object{{"secret",Data{42.0}}}});
        runtime.execute("test","save_game(\"roundtrip\")\n"); require(extension.process().empty(),"Queued save failed");
        require(std::get<double>(std::get<Data::Object>(saves.state("test").value).at("checkpoint").value)==7,"Save hook did not write module state");
        saves.save("roundtrip"); saves.save("roundtrip"); require(std::filesystem::exists("user/extensions/saves/roundtrip.json.bak"),"Backup not retained");
        saves.setMods({{"test","2.0.0"}});
        runtime.execute("test","load_game(\"roundtrip\")\n"); auto diagnostics=extension.process();
        require(!diagnostics.empty() && runtime.global("test","loaded").number()==1,"Missing mod warning or load event absent");
        require(!world.alive(actor) && world.entities().size()==1,"Orphan subtree was not isolated / stale entity remained alive");
        auto restored=world.find("test:Actor"); require(world.get<HealthComponent>(restored).current==75 && physics.contains(restored) && player.entity()==restored,"Built-in state did not restore");
        require(world.custom(restored).at("missing:opaque").version==9,"Unknown custom component was discarded");
        saves.save("without-mod"); saves.setMods({{"test","2.0.0"},{"missing","1.0.0"}}); saves.load("without-mod");
        require(world.entities().size()==3 && world.get<IdentityComponent>(world.find("missing entity")).persistent==orphanKey,"Reinstalled mod lost orphan entities");
        require(std::get<double>(std::get<Data::Object>(saves.state("missing").value).at("secret").value)==42,"Missing mod state lost");
        const auto count=world.entities().size();
        { std::ofstream out("user/extensions/saves/broken.json"); out<<"{broken"; }
        rejects([&] { saves.load("broken"); },"Corrupt save accepted"); require(world.entities().size()==count,"Corrupt load changed live world");
        auto malformed=SaveGame::inspect("user/extensions/saves/roundtrip.json");
        auto& records=std::get<Data::Array>(std::get<Data::Object>(malformed.value).at("entities").value);
        auto& first=std::get<Data::Object>(records.front().value); first["parent"]=first.at("id");
        { std::ofstream out("user/extensions/saves/cycle.json"); out<<encodeData(malformed); }
        rejects([&] { saves.load("cycle"); },"Cyclic saved hierarchy accepted"); require(world.entities().size()==count,"Failed transactional restore altered world");
        rejects([&] { saves.save("../escape"); },"Save path traversal accepted");
        components.define(AssetId("test:fail"),2,Data{Data::Object{}}); world.setCustom(world.find("test:Actor"),AssetId("test:fail"),{1,Data{Data::Object{{"x",Data{1.0}}}}});
        components.migration(AssetId("test:fail"),1,[](const Data&)->Data { throw std::runtime_error("migration failed"); });
        rejects([&] { components.get(world,world.find("test:Actor"),AssetId("test:fail")); },"Failed migration accepted");
        require(world.custom(world.find("test:Actor")).at("test:fail").version==1,"Failed migration altered stored data");
        world.setCustom(world.find("test:Actor"),AssetId("test:Counter"),{99,Data{Data::Object{{"keep",Data{true}}}}});
        rejects([&] { components.get(world,world.find("test:Actor"),AssetId("test:Counter")); },"Future component version accepted");
        require(world.custom(world.find("test:Actor")).at("test:Counter").version==99,"Future component data changed");
        rejects([&] { runtime.execute("test","@Globals\ncycle = record()\ncycle.self = cycle\n"); },"Cyclic script record accepted");
        rejects([&] { runtime.execute("test","set_save_state(get_entity(\"test:Actor\"))\n"); },"Runtime entity reference persisted as data");
        runtime.execute("test",R"SL(
@Globals
replacement = get_definition("Entities", Actor)
replacement.health = 200
replace_definition("Entities", Actor, replacement)
)SL");
        rejects([&] { runtime.execute("other","replace_definition(\"Entities\", \"test:Actor\", record())\n"); },"Unrelated replacement conflict accepted");
        {
            NativeMods native(runtime,world,components,saves);
            native.load("native_sample","examples/native/native_sample","sample.dll");
            require(runtime.emit("on_start").empty(),"Native event failed");
            require(std::get<bool>(std::get<Data::Object>(saves.state("native_sample").value).at("native_started").value),"Native ABI did not modify runtime state");
            require(world.custom(world.find("native-counter")).contains("native_sample:counter"),"Native ABI did not create entity/component");
            rejects([&] { native.load("bad",".","bad_native.dll"); },"Bad native ABI accepted");
            require(runtime.emit("test:bad").empty(),"Rejected DLL retained callback");
        }
        saves.setState("native_sample",Data{}); require(runtime.emit("on_start").empty(),"Unloaded DLL retained callbacks");
        require(std::holds_alternative<std::monostate>(saves.state("native_sample").value),"Native handler remained after unload");
        saves.setMods({{"test","2.0.0"},{"missing","1.0.0"},{"native_sample","1.0.0"}}); saves.save("native-owned");
        saves.setMods({{"test","2.0.0"},{"missing","1.0.0"}}); saves.load("native-owned");
        require(!world.find("native-counter"),"Removed native mod entity remained active");
        saves.save("native-removed"); saves.setMods({{"test","2.0.0"},{"missing","1.0.0"},{"native_sample","1.0.0"}}); saves.load("native-removed");
        require(bool(world.find("native-counter")),"Native mod orphan did not restore");
        std::cout<<"Stage 12 integration passed\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
