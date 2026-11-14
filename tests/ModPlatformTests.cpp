#include "world/WorldBridge.h"
#include "game/GameUi.h"
#include "game/Equipment.h"
#include "game/InventorySystem.h"
#include "scripting/GameplayBridge.h"
#include "scripting/ExtensionBridge.h"
#include "scripting/DataConversion.h"
#include "mods/ModLoader.h"
#include "platform/WindowSystem.h"
#include <iostream>
#include <fstream>
#include <chrono>
using namespace game;
using namespace game::script;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool rejected=false;try { f(); }catch(const std::exception&) { rejected=true; }check(rejected,"Invalid operation accepted"); }
Value json(const std::string& text) { return fromData(decodeData(text)); }
Value text(const char* value) { return Value(std::string(value)); }
void runtimeTests() {
    Runtime r;r.bind("record",native([](const auto&,const auto&){return object();}));
    r.execute("qa",R"(@Globals
order = ""
elapsed = 0
func first(dt):
    order = order + "a"
func second(dt):
    order = order + "b"
func timer(dt):
    elapsed = elapsed + dt
register_system("qa:a", "fixed", first, 10)
register_system("qa:b", "fixed", second, -10)
register_system("qa:timer", "fixed", timer)
)");
    r.bind("options",json(R"({"after":["qa:a"],"disable_on_error":true})"));r.execute("qa","configure_system(\"qa:b\", options)");
    r.bind("options",json(R"({"interval":0.1})"));r.execute("qa","configure_system(\"qa:timer\", options)");
    for(int i=0;i<3;++i) check(r.emit("on_fixed_update",{Value(.04)}).empty(),"Scheduled system failed");
    check(r.global("qa","order").string()=="ababab" && std::abs(r.global("qa","elapsed").number()-.12)<1e-9,"Dependency ordering/timed delta incorrect");
    r.bind("options",json(R"({"after":["qa:b"]})"));rejects([&]{r.execute("qa","configure_system(\"qa:a\", options)");});
    r.execute("bad",R"(@Globals
func exhaust(dt):
    while true:
        dt = dt + 1
register_system("bad:exhaust", "fixed", exhaust, -100)
)");
    r.bind("options",json(R"({"disable_on_error":true})"));r.execute("bad","configure_system(\"bad:exhaust\", options)");
    auto errors=r.emit("on_fixed_update",{Value(.04)});check(errors.size()==1 && r.global("qa","order").string()=="abababab","One system exhausted other mods' budget");
    check(r.emit("on_fixed_update",{Value(.04)}).empty(),"Failed system was not disabled");
    r.execute("qa",R"(@Globals
visits = 0
pending = true
func pass(context):
    visits = visits + 1
    return "success"
func wait(context):
    if pending:
        return "running"
    return "success"
first_node = record()
first_node.kind = "action"
first_node.run = pass
wait_node = record()
wait_node.kind = "action"
wait_node.run = wait
tree = record()
tree.kind = "sequence"
tree.children = [first_node, wait_node]
memory = record()
result = behavior_tick(tree, null, memory)
result = behavior_tick(tree, null, result.memory)
pending = false
result = behavior_tick(tree, null, result.memory)
)");
    check(r.global("qa","visits").number()==1 && r.global("qa","result").member("status",{}).string()=="success","Behavior sequence did not resume its running child");
    r.execute("qa",R"(@Globals
phase_events = ""
func idle(context):
    return "rage"
func rage(context):
    return null
func leave(context):
    phase_events = phase_events + "x"
func enter(context):
    phase_events = phase_events + "e"
idle_state = record()
idle_state.tick = idle
idle_state.exit = leave
rage_state = record()
rage_state.tick = rage
rage_state.enter = enter
states = record()
states.idle = idle_state
states.rage = rage_state
phase = state_tick(states, "idle", null)
phase = state_tick(states, phase, null)
)");
    check(r.global("qa","phase").string()=="rage" && r.global("qa","phase_events").string()=="xe","State transition hooks incorrect");
}
}
int main() {
    try {
        runtimeTests();
        {
            World first,second;auto id=first.create();first.setCustom(id,AssetId("qa:index"),{1,Data{1.0}});
            first.setCustom(id,AssetId("qa:index"),{1,Data{2.0}});check(first.entitiesWithCustom("qa:index").size()==1,"Custom index duplicated entity");
            first.swapState(second);check(first.entitiesWithCustom("qa:index").empty() && second.entitiesWithCustom("qa:index").size()==1,"Custom index did not follow world swap");
            second.removeCustom(id,AssetId("qa:index"));check(second.entitiesWithCustom("qa:index").empty(),"Removed custom component retained index");
            second.setCustom(id,AssetId("qa:index"),{1,Data{1.0}});second.destroy(id);check(second.entitiesWithCustom("qa:index").empty(),"Destroyed entity retained custom index");
        }
        WindowSystem window({"Mod platform acceptance",1280,720,true});Renderer renderer(window.nativeHandle(),1280,720,"assets/core/shaders/forward.hlsl",{false,1});
        VirtualFileSystem vfs;AssetManager assets(vfs,renderer,"user/platform-cache");ModLoader loader(vfs,assets);const auto report=loader.load("content");check(report.diagnostics.empty(),"Content load failed");
        auto definition=WorldDefinition::load(report.mods);auto& settings=definition.generators.at("plains:gentle");settings.relief=0;settings.noise.clear();settings.structures.clear();
        auto volumeJson=definition.json();volumeJson["generators"]["plains:gentle"]["voxel_rules"]=nlohmann::json::parse(R"([
            {"id":"plains:cave","block":"","replace":["plains:stone","plains:dirt","plains:grass"],"min_y":1,"max_y":2,"threshold":-1,"protect_spawn":false},
            {"id":"plains:roof","block":"plains:stone","replace":[""],"min_y":8,"max_y":8,"threshold":-1,"protect_spawn":false}])");
        auto volume=WorldDefinition::parse(volumeJson);TerrainGenerator carved(volume,"plains:gentle","volume");
        check(carved.block(-1,1,-1).empty() && carved.block(-1,8,-1)=="plains:stone","Volumetric rules ignored coordinates/air");
        auto volumeChunk=buildChunk(carved,-1,-1);check(volumeChunk.boxes.size()==3,"Cave collision filled void or failed to merge identical occupancy");
        check(WorldDefinition::parse(volume.json()).json()==volume.json(),"Voxel rule snapshot not stable");
        volumeJson["generators"]["plains:gentle"]["voxel_rules"][0]["replace"]={"missing:block"};rejects([&]{WorldDefinition::parse(volumeJson);});
        World world;PhysicsSystem physics(world);PlayerController player(world,physics);InputSystem input;AudioSystem audio(true);Runtime runtime;
        GameUi ui(runtime,renderer,&input);bindRuntimeApi(runtime,world,assets);bindGameplayApi(runtime,world,physics,player,input,audio,assets);
        const auto directory=std::filesystem::path("user/platform-tests")/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        SaveGame saves(directory,world,physics,player,assets);Components components;ExtensionBridge extensions(runtime,world,components,saves);
        WorldRecord record;record.seed="platform";record.generator="plains:gentle";
        ChunkWorld terrain(renderer,assets,world,physics,player,TerrainGenerator(definition,record.generator,record.seed));bindWorldApi(runtime,terrain,record);bindWorldPersistence(saves,terrain);
        auto actor=world.create("actor");world.add(actor,TransformComponent{{{8,5,8}}});physics.attachBox(actor,{.3f,.9f,.3f},true);player.possess(actor,4);terrain.update(2,true);
        check(equal(entityValue(world,actor),entityValue(world,actor)),"Entity equality depends on wrapper allocation");
        runtime.bind("actor",entityValue(world,actor));runtime.execute("qa","@Items\nitem Blade:\n    .damage = 10\n@Globals\nready = true");
        const auto call=[&](const char* name,std::vector<Value> a) { return runtime.invoke(runtime.global("qa",name),a); };
        BlockEdits edits{{{15,7,1},"plains:stone"},{{16,7,1},"plains:stone"},{{15,0,1},""}};
        check(terrain.editBlocks(edits)==3,"Batch edit did not commit all cells");
        auto before=terrain.blockEdits();auto count=world.entities().size();
        rejects([&]{terrain.editBlocks({{{20,8,1},"plains:stone"},{{21,8,1},"unknown:block"}});});
        check(terrain.blockEdits()==before && world.entities().size()==count,"Invalid batch changed terrain/physics");
        check(terrain.undoEdits() && terrain.blockEdits().empty() && terrain.redoEdits() && terrain.blockEdits()==before,"Undo/redo did not restore atomic edits");
        // Exhaust Jolt capacity to exercise rollback after staging has begun.
        std::vector<EntityId> capacityBodies;
        for(unsigned i=0;i<4096;++i) {
            auto body=world.create("capacity_probe");world.add(body,TransformComponent{{{0,100,0}}});
            try { physics.attachBox(body,{.1f,.1f,.1f},false);capacityBodies.push_back(body); }
            catch(const std::exception&) { world.destroy(body);break; }
        }
        const auto fullCount=world.entities().size();rejects([&]{terrain.editBlocks({{{2,9,2},"plains:stone"}});});
        check(terrain.blockEdits()==before && world.entities().size()==fullCount && terrain.blockAt(2,9,2).empty(),"Physics capacity failure partially committed terrain");
        for(auto body:capacityBodies) { physics.detach(body);world.destroy(body); }
        const auto start=json(R"({"x":0,"y":4,"z":0})"),goal=json(R"({"x":8,"y":4,"z":0})"),options=json(R"({"max_nodes":512,"clearance":2,"step":0,"drop":0})");
        auto path=call("terrain_path",{start,goal,options});check(path.member("status",{}).string()=="found","Flat terrain path not found");
        BlockEdits wall;for(int z=-2;z<=2;++z) for(int y=4;y<7;++y) wall[{4,y,z}]="plains:stone";terrain.editBlocks(wall);
        path=call("terrain_path",{start,goal,options});check(path.member("status",{}).string()=="found","Navigation failed to route around edited obstacle");
        bool detour=false;for(const auto& point:std::get<std::shared_ptr<List>>(path.member("points",{}).data)->values) detour|=std::abs(point.member("z",{}).number()-.5)>2;
        check(detour,"Path passed through edited wall");
        auto limited=call("terrain_path",{start,goal,json(R"({"max_nodes":1,"clearance":2,"step":0,"drop":0})")});check(limited.member("status",{}).string()=="budget","Path budget not enforced");
        auto hit=call("physics_raycast",{json(R"({"x":0.5,"y":5,"z":0.5})"),json(R"({"x":9.5,"y":5,"z":0.5})"),list({})});
        check(hit.member("position",{}).member("x",{}).number()>3.9 && hit.member("position",{}).member("x",{}).number()<4.1,"Swept collision missed edited wall");
        int projectileHits=0,expired=0;
        runtime.subscribe("on_projectile_hit",native([&](const auto& args,const auto&) { ++projectileHits;check(args[3].member("x",{}).number()<4.1,"Projectile tunneled through wall");return Value{}; }));
        runtime.subscribe("on_projectile_expired",native([&](const auto&,const auto&) { ++expired;return Value{}; }));
        auto shot=world.create("shot");world.add(shot,TransformComponent{{{.5f,5,.5f}}});
        call("launch_projectile",{entityValue(world,shot),entityValue(world,actor),json(R"({"x":1000,"y":0,"z":0})"),Value(1.0),Value(0.0)});
        check(runtime.emit("on_fixed_update",{Value(1.0/60)}).empty() && projectileHits==1 && !world.alive(shot),"Projectile hit/lifetime cleanup failed");
        shot=world.create("expired");world.add(shot,TransformComponent{{{.5f,20,.5f}}});
        call("launch_projectile",{entityValue(world,shot),Value{},json(R"({"x":0,"y":0,"z":0})"),Value(.001),Value(0.0)});
        check(runtime.emit("on_fixed_update",{Value(1.0/60)}).empty() && expired==1 && !world.alive(shot),"Projectile expiry event missing");
        auto remoteHit=call("physics_raycast",{json(R"({"x":1000.5,"y":10,"z":1000.5})"),json(R"({"x":1000.5,"y":0,"z":1000.5})"),list({})});
        check(remoteHit.member("position",{}).member("y",{}).number()==4 && std::holds_alternative<std::monostate>(remoteHit.member("entity",{}).data),"Raycast ignored unstreamed procedural terrain");
        auto pool=json(R"([{"id":"qa:sharp","weight":1,"stats":{"damage":{"add_min":2,"add_max":8,"multiply_min":1,"multiply_max":2}}},{"id":"qa:sturdy","weight":2,"stats":{}}])");
        auto roll=call("roll_affixes",{text("loot-seed"),pool,Value(2.0)});
        check(equal(roll,call("roll_affixes",{text("loot-seed"),pool,Value(2.0)})) && !equal(roll,call("roll_affixes",{text("other-seed"),pool,Value(2.0)})),"Affix roll not seeded/deterministic");
        const auto actorValue=entityValue(world,actor);giveItem(world,actor,AssetId("qa:Blade"),2);
        auto item=call("create_item_instance",{actorValue,text("qa:Blade"),json(R"({"maximum":100,"stats":{"damage":{"add":5,"multiply":2}},"affixes":{"qa:sharp":{"damage":{"add":3,"multiply":1.5}}}})")});
        check(takeItem(world,actor,AssetId("qa:Blade"),1) && !takeItem(world,actor,AssetId("qa:Blade"),1),"Unique item copy not reserved");
        check(call("equip_instance",{actorValue,text("weapon"),item}).boolean(),"Instance not equipped");
        check(call("equipment_stat",{actorValue,text("damage"),Value(10.0)}).number()==54,"Equipment bonuses/affixes not combined");
        runtime.load("foundation","content/foundation","equipment_stats.sl");
        auto victim=world.create("victim");world.add(victim,HealthComponent{100,100});
        check(call("damage_from",{actorValue,entityValue(world,victim),Value(10.0)}).number()==54,"Foundation did not apply equipment bonuses to combat");
        world.destroy(victim);
        rejects([&]{call("equip_slot",{actorValue,text("weapon"),text("qa:Blade")});});
        call("item_durability",{actorValue,item,Value(0.0)});check(call("equipment_stat",{actorValue,text("damage"),Value(10.0)}).number()==10,"Broken item still contributes stats");
        call("item_durability",{actorValue,item,Value(90.0)});
        auto savedShot=world.create("saved_shot");world.add(savedShot,TransformComponent{{{.5f,30,.5f}}});
        call("launch_projectile",{entityValue(world,savedShot),entityValue(world,actor),json(R"({"x":2,"y":0,"z":0})"),Value(1.0),Value(9.8)});
        const auto shotKey=world.get<IdentityComponent>(savedShot).persistent;
        const auto actorKey=world.get<IdentityComponent>(actor).persistent;saves.save("checkpoint");check(saves.load("checkpoint").empty(),"Platform save/load failed");
        savedShot=world.findPersistent(shotKey);check(world.alive(savedShot) && world.entitiesWithCustom("core:projectile").size()==1,"Projectile save/index not restored");
        check(std::get<std::string>(std::get<Data::Object>(world.custom(savedShot).at("core:projectile").data.value).at("source").value)==actorKey,"Projectile source identity not persisted");
        call("stop_projectile",{entityValue(world,savedShot)});world.destroy(savedShot);
        actor=world.findPersistent(actorKey);check(readEquipment(world,actor).items.at(item.string()).durability==90 && terrain.blockEdits().size()==18,"Instances/terrain edits lost in save");
        check(!terrain.undoEdits(),"Save/load retained history from previous world state");
        auto saveJson=nlohmann::json::parse(encodeData(SaveGame::inspect(directory/"checkpoint.json")));
        for(auto& e:saveJson["entities"]) if(e["id"]==actorKey) e["inventory"].erase("qa:Blade");
        { std::ofstream out(directory/"invalid.json");out<<saveJson; }
        rejects([&]{saves.load("invalid");});check(world.alive(actor),"Invalid save replaced live world");
        // Socket follows both the bind pose and animation pose, including parent transforms.
        auto visual=world.create("socket_visual");world.add(visual,TransformComponent{});auto model=std::make_shared<ModelResource>();
        model->hierarchy.nodes.push_back({"hand","hand-id","",-1,transformMatrix({{2,0,0}})});world.add(actor,ModelComponent{AssetId("qa:body"),model});
        call("attach_socket",{entityValue(world,visual),entityValue(world,actor),text("hand"),json(R"({"x":1,"y":0,"z":0})")});
        check(std::abs(world.worldMatrix(visual).values[12]-11)<.001,"Socket offset ignored bind pose");
        world.get<ModelComponent>(actor).nodeWorld={transformMatrix({{3,0,0}})};check(std::abs(world.worldMatrix(visual).values[12]-12)<.001,"Socket ignored animated pose");
        runtime.bind("screen",json(R"({"layout":"column","modal":true,"x":0.1,"y":0.1,"w":0.5,"h":0.5,"gap":8,"widgets":[
            {"id":"button","kind":"button","text":"Aplicar"},
            {"id":"name","kind":"input","value":"olá"},
            {"id":"amount","kind":"slider","min":0,"max":10,"step":1,"value":5} ]})"));
        runtime.execute("qa",R"(@Globals
activations = 0
show_widgets("qa:settings", screen)
@Events
on_ui(screen, widget, event, value):
    if event == "activate":
        activations = activations + 1
)");
        auto event=[&](Event e) { input.process(e);ui.event(e); };
        input.beginFrame();event({EventType::MouseMove,Key::Unknown,MouseButton::Unknown,PadButton::Unknown,0,0,0,200,100});event({EventType::MouseDown,Key::Unknown,MouseButton::Left});event({EventType::KeyDown,Key::W});ui.updateInput();
        check(runtime.global("qa","activations").number()==1 && !input.pressed("Attack") && input.moveY()==0,"Widget click leaked to gameplay");
        ui.renderWidgets(renderer);auto firstTexture=ui.texture();ui.renderWidgets(renderer);check(ui.texture()==firstTexture,"Unchanged widget UI rebuilt its texture");
        renderer.render(Camera{},std::vector<RenderItem>{},ui.texture());
        { auto pixels=renderer.readback();std::ofstream image("user/platform-preview.ppm",std::ios::binary);image<<"P6\n1280 720\n255\n";for(size_t i=0;i<pixels.size();i+=4) image.write(reinterpret_cast<const char*>(pixels.data()+i),3); }
        runtime.bind("overlay",json(R"({"layout":"column","modal":false,"x":0.1,"y":0.1,"w":0.5,"h":0.5,"widgets":[{"id":"block","kind":"button","enabled":false,"text":"Disabled"}]})"));
        runtime.execute("qa","show_widgets(\"qa:overlay\", overlay)");
        input.beginFrame();event({EventType::MouseUp,Key::Unknown,MouseButton::Left});event({EventType::MouseDown,Key::Unknown,MouseButton::Left});ui.updateInput();
        check(runtime.global("qa","activations").number()==1,"Disabled upper widget clicked through to lower screen");runtime.execute("qa","hide_widgets(\"qa:overlay\")");

        runtime.execute("qa","focus_widget(\"qa:settings\", \"name\")");input.beginFrame();event({EventType::KeyDown,Key::Backspace});ui.updateInput();
        check(call("widget_value",{text("qa:settings"),text("name")}).string()=="ol","UTF-8 backspace corrupted input");
        runtime.execute("qa","focus_widget(\"qa:settings\", \"amount\")");input.beginFrame();event({EventType::KeyDown,Key::Right});ui.updateInput();
        check(call("widget_value",{text("qa:settings"),text("amount")}).number()==6,"Keyboard slider did not change");
        rejects([&]{runtime.execute("other","hide_widgets(\"qa:settings\")");});
        runtime.execute("qa","hide_widgets(\"qa:settings\")");input.beginFrame();ui.updateInput();check(!input.held("Attack"),"Held UI pointer escaped after closing screen");
        input.beginFrame();event({EventType::MouseUp,Key::Unknown,MouseButton::Left});ui.updateInput();input.beginFrame();ui.updateInput();check(input.moveY()==1,"Closing UI retained input capture");
        std::cout<<"Mod platform acceptance passed: systems, behaviors, volumes, edits, navigation, raycasts, equipment, saves, sockets, widgets\n";return 0;
    }catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
