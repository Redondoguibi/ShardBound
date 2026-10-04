#include "assets/CoreAssets.h"
#include "scripting/GameplayBridge.h"
#include "scripting/ExtensionBridge.h"
#include "game/GameUi.h"
#include "game/HealthSystem.h"
#include "game/InventorySystem.h"
#include "mods/ModLoader.h"
#include "ecs/RenderSystem.h"
#include "platform/WindowSystem.h"
#include <cmath>
#include <fstream>
#include <iostream>
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char**) {
    using namespace game;
    try {
        WindowSystem window({"Adventure integration",960,640,true});
        Renderer renderer(window.nativeHandle(),960,640,"assets/core/shaders/forward.hlsl");
        VirtualFileSystem vfs; vfs.mount("core","assets/core"); AssetManager assets(vfs,renderer,"user/adventure-cache"); registerCoreModels(assets);
        World world; PhysicsSystem physics(world); PlayerController player(world,physics); InputSystem input; AudioSystem audio(true);
        script::Runtime runtime; bindRuntimeApi(runtime,world,assets); bindGameplayApi(runtime,world,physics,player,input,audio,assets);
        Components components; SaveGame saves("user/adventure-test-saves",world,physics,player,assets); ExtensionBridge extension(runtime,world,components,saves); GameUi ui(runtime,renderer);
        ModLoader loader(vfs,assets); auto report=loader.load("content"); require(report.diagnostics.empty(),"Content discovery failed");
        for(const auto& mod:report.mods) runtime.load(mod.id,mod.root,mod.entry);
        saves.setMods({{"adventure","0.1.0"}});
        auto emit=[&](const std::string& event,std::vector<script::Value> args=std::vector<script::Value>{}) {
            const auto errors=runtime.emit(event,args); if(!errors.empty()) throw DiagnosticError(errors.front());
        };
        auto run=[&](std::string_view code) { runtime.execute("adventure",code); };
        auto state=[&]() { return runtime.global("adventure","state"); };
        emit("on_start");
        for(int i=0;i<90;++i) { emit("on_fixed_update",{script::Value(1.0/60)}); player.move(0,0); physics.step(1.0f/60); }
        auto hero=player.entity(); require(bool(hero) && physics.grounded(hero),"Hero failed to stand in hub");
        auto stateNumber=[&](const char* field) { return state().member(field,{}).number(); };
        require(!state().member("forest",{}).boolean(),"Forest discovered remotely");
        run("travel(\"forest_waystone\", state.forest)"); require(world.get<TransformComponent>(hero).local.position.z<1,"Undiscovered travel accepted");
        physics.teleport(hero,{3,1.1f,0}); emit("on_fixed_update",{script::Value(1.0/60)});
        require(state().member("hub",{}).boolean(),"Nearby hub did not activate");
        physics.teleport(hero,{-3,1.1f,20}); emit("on_fixed_update",{script::Value(1.0/60)});
        require(state().member("forest",{}).boolean(),"Nearby forest did not activate");
        run("damage(hero, 1)"); require(stateNumber("combat")==6,"Received damage did not block travel");
        run("travel(\"hub_waystone\", state.hub)"); require(world.get<TransformComponent>(hero).local.position.z>10,"Travel during combat accepted");
        run("explore_tick(5.99)"); require(stateNumber("combat")>0,"Travel unlocked before six seconds");
        run("damage(hero, 1)"); require(stateNumber("combat")==6,"Second damage did not reset timer");
        run("damage(hero, 0)"); require(stateNumber("combat")==6,"Zero damage changed timer");
        run("explore_tick(6)\ntravel(\"hub_waystone\", state.hub)"); require(world.get<TransformComponent>(hero).local.position.z<1,"Travel did not unlock at six seconds");
        auto foe=world.find("adventure:Enemy"); require(bool(foe),"Enemy missing");
        const auto foeKey=world.get<IdentityComponent>(foe).persistent;
        physics.teleport(hero,{2,1.0f,12.2f}); physics.teleport(foe,{2,1,14});
        physics.setVelocity(hero,{0,-2,0});
        run("state.swing = get_definition(\"Items\", IronSword)\nstate.aim = vec3(0, 0, 1)\nresolve_attack()");
        // At ground level this is a normal hit despite negative Y velocity.
        require(std::abs(world.get<HealthComponent>(foe).current-30)<.01f,"Grounded damage or arc wrong");
        require(stateNumber("combat")==6,"Dealt damage did not reset combat timer");
        setHealth(world,foe,100); physics.teleport(hero,{2,2.2f,12.2f}); physics.setVelocity(hero,{0,-2,0});
        run("resolve_attack()"); require(std::abs(world.get<HealthComponent>(foe).current-74.5f)<.01f,"Falling crit is not 1.7x");
        setHealth(world,foe,100); physics.setVelocity(hero,{0,2,0}); run("resolve_attack()"); require(std::abs(world.get<HealthComponent>(foe).current-85)<.01f,"Rising attack wrongly critical");
        run("state.gold = 101\ndamage(hero, 10000)\nrespawn_if_dead()\nrespawn_if_dead()");
        require(stateNumber("gold")==86 && world.get<HealthComponent>(hero).current==100,"Death penalty/revival incorrect");
        require(state().member("forest",{}).boolean() && itemCount(world,hero,AssetId("adventure:IronSword"))==1,"Death lost discoveries or gear");
        run("save_game(\"adventure_test\")"); require(extension.process().empty(),"Save failed");
        run("state.gold = 0\nstate.forest = false\nteleport(hero, vec3(8, 1, 28))\nload_game(\"adventure_test\")");
        require(extension.process().empty(),"Load failed"); hero=player.entity();
        require(stateNumber("gold")==86 && state().member("forest",{}).boolean() && stateNumber("combat")==6,"Save/load lost game state");
        require(world.get<TransformComponent>(hero).local.position.z==0,"Save did not restore location");
        require(player.viewOffset().x==7 && player.viewOffset().y==10,"Camera offset was not restored");
        require(std::abs(world.get<ModelComponent>(world.find("hub_waystone")).tint.g-1)<.001f,"Tint was not persisted");
        // Full fixed-step cooldown, including the exact 360th tick.
        for(int i=0;i<359;++i) run("explore_tick(1 / 60)");
        require(stateNumber("combat")>0,"Combat lock expired before 360 fixed ticks");
        run("explore_tick(1 / 60)"); require(stateNumber("combat")==0,"Combat lock retained floating-point residue");
        // Trigger the same input path as the application and reject attack repetition.
        foe=world.findPersistent(foeKey); require(bool(foe),"Saved enemy identity missing"); setHealth(world,foe,100);
        physics.teleport(hero,{2,1,12.2f}); physics.teleport(foe,{2,1,14});
        run("facing = vec3(0, 0, 1)\nstate.attack = 0");
        Event click; click.type=EventType::MouseDown; click.mouse=MouseButton::Left; input.process(click);
        emit("on_update",{script::Value(.016)}); input.beginFrame();
        require(world.get<HealthComponent>(foe).current==100,"Damage occurred before windup");
        for(int i=0;i<30;++i) emit("on_fixed_update",{script::Value(1.0/60)});
        require(world.get<HealthComponent>(foe).current==85,"One attack hit repeatedly or missed");
        click.type=EventType::MouseUp; input.process(click); input.beginFrame();
        // Actual dependent package: registry patch plus public melee hook.
        runtime.load("trailcraft","examples/adventure-extension/trailcraft","main.sl",{"adventure"});
        require(runtime.registry().get("Items","adventure:IronSword").data.member("damage",{}).number()==18,"Mod patch did not apply");
        setHealth(world,foe,100); physics.teleport(hero,{2,1,12.2f}); physics.teleport(foe,{2,1,14});
        run("state.swing = get_definition(\"Items\", IronSword)\nresolve_attack()");
        require(world.get<HealthComponent>(foe).current==80,"Mod hook did not change real hit");
        // Defeat rewards happen once; the new item can actually be equipped.
        auto goldBefore=stateNumber("gold"); damage(world,foe,10000);
        run("combat_tick(0.016)\ncombat_tick(0.016)");
        require(stateNumber("gold")==goldBefore+12 && itemCount(world,hero,AssetId("adventure:ForestSword"))==1,"Enemy reward repeated or loot missing");
        run("state.attack = 0"); Event equip; equip.type=EventType::KeyDown; equip.key=Key::Q; input.process(equip);
        emit("on_update",{script::Value(.016)}); input.beginFrame();
        require(state().member("equipped",{}).string()=="adventure:ForestSword","Loot cannot be equipped");
        equip.type=EventType::KeyUp; input.process(equip); input.beginFrame();
        // Enemy windup is harmless until impact; the safe hub cancels pursuit.
        run("combat_tick(25)"); physics.teleport(foe,{2,1,14}); physics.teleport(hero,{2,1,12.6f}); setHealth(world,hero,100);
        auto npc=entityValue(world,foe);
        runtime.invoke(runtime.global("adventure","enemy_tick"),{npc,script::Value(.016)});
        require(world.get<HealthComponent>(hero).current==100,"Enemy damaged during windup");
        runtime.invoke(runtime.global("adventure","enemy_tick"),{npc,script::Value(.65)});
        require(world.get<HealthComponent>(hero).current==88,"Enemy did not damage at impact");
        physics.teleport(hero,{2,1,6}); setHealth(world,hero,100);
        runtime.invoke(runtime.global("adventure","enemy_tick"),{npc,script::Value(2.0)});
        require(world.get<HealthComponent>(hero).current==100,"Enemy attacked inside safe hub");
        // Future package schema must fail before replacing live entities.
        auto malformed=SaveGame::inspect("user/adventure-test-saves/adventure_test.json");
        auto& savedState=std::get<Data::Object>(std::get<Data::Object>(malformed.value).at("state").value);
        std::get<Data::Object>(savedState.at("adventure").value)["schema"]=Data{2.0};
        { std::ofstream file("user/adventure-test-saves/future.json"); file<<encodeData(malformed); }
        run("load_game(\"future\")"); require(!extension.process().empty() && player.entity()==hero,"Future schema changed live world");
        physics.teleport(hero,{0,1.1f,0});
        run("notice = \"Explore a floresta ao norte.\"\nfacing = vec3(0, 0, -1)\nrefresh_hud()");
        emit("on_update",{script::Value(.016)});
        renderer.render(player.camera(),collectRenderItems(world),ui.texture()); auto pixels=renderer.readback();
        require(renderer.stats().draws>100 && ui.texture(),"World/UI not rendered");
        if(argc>1) {
            std::ofstream image("user/adventure-preview.ppm",std::ios::binary); image<<"P6\n960 640\n255\n";
            for(size_t i=0;i<pixels.size();i+=4) image.write(reinterpret_cast<const char*>(pixels.data()+i),3);
            run("map_open = true\nrefresh_hud()"); renderer.render(player.camera(),collectRenderItems(world),ui.texture());
            pixels=renderer.readback(); std::ofstream map("user/adventure-map.ppm",std::ios::binary); map<<"P6\n960 640\n255\n";
            for(size_t i=0;i<pixels.size();i+=4) map.write(reinterpret_cast<const char*>(pixels.data()+i),3);
        }
        std::cout<<"Adventure cycle verified\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
