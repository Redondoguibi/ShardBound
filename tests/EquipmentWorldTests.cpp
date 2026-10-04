#include "assets/CoreAssets.h"
#include "scripting/GameplayBridge.h"
#include "scripting/ExtensionBridge.h"
#include "game/GameUi.h"
#include "game/HealthSystem.h"
#include "game/InventorySystem.h"
#include "ecs/RenderSystem.h"
#include "platform/WindowSystem.h"
#include <fstream>
#include <iostream>
#include <cmath>
using namespace game;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char**) {
    try {
        WindowSystem window({"Equipment and world",960,640,true});
        Renderer renderer(window.nativeHandle(),960,640,"assets/core/shaders/forward.hlsl");
        VirtualFileSystem vfs; vfs.mount("core","assets/core"); AssetManager assets(vfs,renderer,"user/equipment-cache"); registerCoreModels(assets);
        World world; PhysicsSystem physics(world); PlayerController player(world,physics); InputSystem input; AudioSystem audio(true);
        script::Runtime runtime; bindRuntimeApi(runtime,world,assets); bindGameplayApi(runtime,world,physics,player,input,audio,assets);
        Components components; SaveGame saves("user/equipment-saves",world,physics,player,assets); ExtensionBridge extension(runtime,world,components,saves); GameUi ui(runtime,renderer);
        runtime.load("adventure","content/adventure","main.sl");
        runtime.load("trailcraft","examples/adventure-extension/trailcraft","main.sl",{"adventure"});
        saves.setMods({{"adventure","0.1.0"},{"trailcraft","0.1.0"}});
        auto emit=[&](const std::string& name,std::vector<script::Value> args=std::vector<script::Value>{}) {
            auto errors=runtime.emit(name,args); if(!errors.empty()) throw DiagnosticError(errors.front());
        };
        auto run=[&](std::string_view code) { runtime.execute("adventure","@Globals\n"+std::string(code)); };
        auto state=[&]() { return runtime.global("adventure","state"); };
        auto items=[&]() { return std::get<std::shared_ptr<script::List>>(state().member("items",{}).data)->values; };
        emit("on_start"); auto hero=player.entity();
        for(int i=0;i<90;++i) { player.move(0,0); physics.step(1.0f/60); }
        require(world.find("trailcraft_camp") && physics.grounded(hero),"Mod content was not built at startup");
        run("teleport(hero, vec3(0, 1, 55))\nexplore_tick(0)\nregion_result = current_region().name");
        require(runtime.global("adventure","region_result").string()=="TRILHA DO NORTE","Mod region not recognized");
        require(physics.grounded(hero),"Mod region has no physical terrain");
        run("state.combat = 0\ntravel_destination(\"adventure:Hub\")");
        require(world.get<TransformComponent>(hero).local.position.z==55,"Undiscovered target bypassed discovery");
        run("teleport(hero, vec3(3, 1.1, 0))\nexplore_tick(0)\ntravel_destination(\"trailcraft:NorthCamp\")");
        require(world.get<TransformComponent>(hero).local.position.z==53,"Mod fast travel failed");
        require(itemCount(world,hero,AssetId("trailcraft:TrailBlade"))==1,"Mod startup gift missing");
        run("give_item(hero, \"trailcraft:TrailBlade\", 1)\nsync_equipment()");
        require(items().size()==3,"Duplicate equipment copies collapsed");
        const auto first=items()[1],second=items()[2];
        require(first.member("uid",{}).number()!=second.member("uid",{}).number() && first.member("level",{}).number()==4 && first.member("rarity",{}).string()=="Rara","Instance identity or hook attributes missing");
        run("chosen = list_at(state.items, 1)\nchosen.attributes.damage = 37\nequip_instance(chosen)");
        require(items()[2].member("attributes",{}).member("damage",{}).number()==31,"Copies share mutable attributes");
        auto foe=world.find("adventure:Enemy"); setHealth(world,foe,200); physics.teleport(foe,{2,1,14}); physics.teleport(hero,{2,1,12.2f});
        run("state.swing = weapon_definition()\nstate.aim = vec3(0, 0, 1)\nresolve_attack()");
        require(std::abs(world.get<HealthComponent>(foe).current-161)<.01f,"Instance attributes did not affect real combat and mod hook");
        run("state.attack = 0\nfacing = vec3(0, 0, 1)\nbegin_attack()\nanimate_combat_visuals(0)");
        require(world.get<ActiveComponent>(world.find("attack_reach_0")).value,"Attack windup has no reach feedback");
        run("state.winding = false\nstate.attack = 0\nanimate_combat_visuals(0.2)");
        require(!world.get<ActiveComponent>(world.find("attack_reach_0")).value,"Reach feedback remains after attack");
        // Preserve data for an unavailable mod without granting equip access.
        giveItem(world,hero,AssetId("missing:Relic"),1);
        run(R"SL(
orphan = record()
orphan.uid = state.next_item_uid
state.next_item_uid = state.next_item_uid + 1
orphan.definition = "missing:Relic"
orphan.level = 7
orphan.rarity = "Rara"
orphan.attributes = record()
orphan.attributes.damage = 99
orphan.owned = true
list_append(state.items, orphan)
discovery("missing:Camp").active = true
save_game("instances")
)SL");
        require(extension.process().empty(),"Instance save failed");
        run("list_at(state.items, 1).attributes.damage = 1\nload_game(\"instances\")");
        require(extension.process().empty(),"Instance load failed"); hero=player.entity();
        require(items()[1].member("attributes",{}).member("damage",{}).number()==37 && state().member("equipped_uid",{}).number()==first.member("uid",{}).number(),"Equipped copy or attributes not persisted");
        require(items().back().member("attributes",{}).member("damage",{}).number()==99,"Unavailable mod item lost attributes");
        run("orphan_equipped = equip_instance(list_at(state.items, 3))\nold_discovery = discovered(\"missing:Camp\")");
        require(!runtime.global("adventure","orphan_equipped").boolean() && runtime.global("adventure","old_discovery").boolean(),"Unavailable mod equipped or discovery lost");
        // Reject malformed instance IDs before replacing the live world.
        auto data=SaveGame::inspect("user/equipment-saves/instances.json");
        auto& modules=std::get<Data::Object>(std::get<Data::Object>(data.value).at("state").value);
        auto& saved=std::get<Data::Object>(modules.at("adventure").value);
        auto& copies=std::get<Data::Array>(saved.at("items").value);
        std::get<Data::Object>(copies[1].value)["uid"]=Data{1.0};
        { std::ofstream file("user/equipment-saves/duplicate.json"); file<<encodeData(data); }
        run("load_game(\"duplicate\")"); require(!extension.process().empty() && player.entity()==hero,"Duplicate identity replaced live world");
        // Remove only additive fields to emulate the original stage-13 save.
        data=SaveGame::inspect("user/equipment-saves/instances.json");
        auto& old=std::get<Data::Object>(std::get<Data::Object>(std::get<Data::Object>(data.value).at("state").value).at("adventure").value);
        for(const auto* key:{"equipment_schema","items","next_item_uid","equipped_uid","discoveries"}) old.erase(key);
        old["forest"]=Data{true}; old["equipped"]=Data{std::string("adventure:IronSword")};
        { std::ofstream file("user/equipment-saves/legacy.json"); file<<encodeData(data); }
        run("load_game(\"legacy\")"); require(extension.process().empty(),"Legacy migration failed");
        require(items().size()==3 && state().member("equipped_uid",{}).number()>0,"Legacy counts did not become instances");
        run("migrated_forest = discovered(\"adventure:Forest\")"); require(runtime.global("adventure","migrated_forest").boolean(),"Legacy discovery not migrated");
        // Content installed after a save must reconcile without rebuilding existing regions.
        runtime.execute("trailcraft",R"SL(
@Regions
region EastTrail from NorthTrail:
    .name = "TRILHA LESTE"
    .min = vec3(18, 0, 42)
    .max = vec3(36, 0, 62)
    .trees = []
@Waystones
waystone EastCamp from NorthCamp:
    .label = "trailcraft_east_camp"
    .name = "Acampamento leste"
    .region = "trailcraft:EastTrail"
    .position = vec3(27, 0.06, 55)
    .destination = vec3(27, 1.1, 53)
)SL");
        run("load_game(\"legacy\")"); require(extension.process().empty(),"New mod content failed to reconcile with save");
        require(bool(world.find("trailcraft_east_camp")),"New mod waystone missing from old save");
        hero=player.entity(); physics.teleport(hero,{27,1,55}); require(physics.grounded(hero),"New mod region terrain missing from old save");
        const auto entities=world.entities().size(); run("sync_world_content()"); require(world.entities().size()==entities,"World reconciliation duplicated entities");
        // Drive the real UI selection path with an item added by a mod.
        auto key=[&](Key value) { Event event; event.type=EventType::KeyDown; event.key=value; input.process(event); emit("on_update",{script::Value(.016)}); input.beginFrame(); event.type=EventType::KeyUp; input.process(event); input.beginFrame(); };
        key(Key::I); key(Key::Q); key(Key::E);
        require(state().member("equipped",{}).string()=="trailcraft:TrailBlade","Inventory cannot equip a modded weapon");
        run("give_item(hero, \"trailcraft:TrailBlade\", 6)\nensure_equipment()");
        for(int i=0;i<5;++i) key(Key::Q);
        key(Key::E);
        require(state().member("equipped_uid",{}).number()==items()[6].member("uid",{}).number(),"Equipment beyond first inventory page cannot be selected");
        run("refresh_hud()");
        renderer.render(player.camera(),collectRenderItems(world),ui.texture()); auto pixels=renderer.readback();
        require(ui.texture() && renderer.stats().draws>100,"Expanded inventory failed to render");
        if(argc>1) {
            auto image=[&](const char* path) { std::ofstream file(path,std::ios::binary); file<<"P6\n960 640\n255\n"; for(size_t i=0;i<pixels.size();i+=4) file.write(reinterpret_cast<const char*>(pixels.data()+i),3); };
            image("user/equipment-preview.ppm");
            key(Key::M); run("refresh_hud()"); renderer.render(player.camera(),collectRenderItems(world),ui.texture()); pixels=renderer.readback(); image("user/world-preview.ppm");
            run("map_open = false\ninventory_open = false\nteleport(hero, vec3(0, 1, 0))\nfacing = vec3(0, 0, -1)\nstate.attack = 0\nbegin_attack()\ncombat_tick(0.08)\nanimate_hero(0.016)\nanimate_combat_visuals(0.016)\nrefresh_hud()");
            renderer.render(player.camera(),collectRenderItems(world),ui.texture()); pixels=renderer.readback(); image("user/combat-preview.ppm");
        }
        std::cout<<"Equipment instances, migration, UI and mod world verified\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
