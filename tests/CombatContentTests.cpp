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
        WindowSystem window({"Combat content",960,640,true});
        Renderer renderer(window.nativeHandle(),960,640,"assets/core/shaders/forward.hlsl");
        VirtualFileSystem vfs; vfs.mount("core","assets/core"); AssetManager assets(vfs,renderer,"user/combat-content-cache"); registerCoreModels(assets);
        World world; PhysicsSystem physics(world); PlayerController player(world,physics); InputSystem input; AudioSystem audio(true);
        script::Runtime runtime; bindRuntimeApi(runtime,world,assets); bindGameplayApi(runtime,world,physics,player,input,audio,assets);
        Components components; SaveGame saves("user/combat-content-saves",world,physics,player,assets); ExtensionBridge extension(runtime,world,components,saves); GameUi ui(runtime,renderer);
        runtime.load("adventure","content/adventure","main.sl");
        saves.setMods({{"adventure","0.1.0"},{"combatmod","0.1.0"}});
        auto emit=[&](const std::string& name,std::vector<script::Value> args=std::vector<script::Value>{}) {
            auto errors=runtime.emit(name,args); if(!errors.empty()) throw DiagnosticError(errors.front());
        };
        auto run=[&](std::string_view code) { runtime.execute("adventure","@Globals\n"+std::string(code)); };
        auto state=[&]() { return runtime.global("adventure","state"); };
        auto value=[&](const char* key) { return runtime.global("adventure",key); };
        emit("on_start"); auto hero=player.entity();
        auto scout=world.find("adventure:Scout"), brute=world.find("adventure:Brute");
        require(scout && brute,"New encounters were not spawned");
        const auto scoutKey=world.get<IdentityComponent>(scout).persistent;
        require(world.get<HealthComponent>(scout).maximum==35 && world.get<HealthComponent>(brute).maximum==100,"Variants lost their health attributes");
        run("scout = get_entity(\"adventure:Scout\")\nbrute = get_entity(\"adventure:Brute\")\nscout_visual = get_entity_by_key(get_component(scout, Sentinel).visual)\nbrute_visual = get_entity_by_key(get_component(brute, Sentinel).visual)\nscout_model = entity_definition(scout_visual)");
        require(value("scout_model").string()=="adventure:ScoutVisual","Entity definition query returned another identity");
        auto visual=[&](const char* name) { return requireEntity(value(name),world,{}); };
        require(world.get<ModelComponent>(visual("scout_visual")).model->parts.size()==21 && world.get<ModelComponent>(visual("brute_visual")).model->parts.size()==24,"Blockbench enemy geometry was not imported completely");
        require(world.get<ModelComponent>(world.find("player_sword")).definition.str()=="core:iron_sword" && assets.model(AssetId("core:iron_sword"))->parts.size()==6,"Sword export lost the handle or guard");
        const auto count=world.entities().size(); run("sync_world_content()\nsync_world_content()");
        require(world.entities().size()==count,"Encounter reconciliation duplicates enemies");
        runtime.execute("combatmod", R"SL(
@Items
weapon LegacySword:
    .slot = "weapon"
    .tint = vec3(1, 1, 1)
    .name = "Old contract sword"
    .level = 1
    .rarity = "Comum"
    .damage = 15
    .reach = 2.2
    .arc = 0.2
    .windup = 0.15
    .recovery = 0.32
    .pattern = "slash"
)SL");
        run("give_item(hero, \"combatmod:LegacySword\", 1)\nsync_equipment()\nlegacy_equipped = equip_instance(list_at(state.items, 1))\nlegacy_model = weapon_definition().model");
        require(value("legacy_equipped").boolean() && value("legacy_model").string()=="core:iron_sword","Optional visual fields broke the previous weapon contract");
        run("equip_instance(list_at(state.items, 0))\nlegacy = list_at(state.items, 1)\nlegacy.attributes.model = \"missing:Blade\"");
        const auto equippedBeforeFailure=state().member("equipped_uid",{}).number();
        bool missingModel=false;
        try { run("equip_instance(legacy)"); } catch(const DiagnosticError& error) { missingModel=error.diagnostic.code=="ASSET306"; }
        require(missingModel && state().member("equipped_uid",{}).number()==equippedBeforeFailure && world.get<ModelComponent>(world.find("player_sword")).definition.str()=="core:iron_sword","Failed model load changed equipped identity or visual");
        run("take_item(hero, \"combatmod:LegacySword\", 1)\nensure_equipment()\nsync_equipment()");
        // Each type has its own windup and damage; the safe hub cancels both.
        run("teleport(hero, vec3(-5, 0.9, 16.9))\nenemy_tick(scout, 0.01)\nenemy_tick(scout, 0.39)");
        require(world.get<HealthComponent>(hero).current==100,"Scout hit before its windup");
        run("enemy_tick(scout, 0.01)"); require(world.get<HealthComponent>(hero).current==92,"Scout did not use its own damage");
        run("set_health(hero, 100)\nteleport(hero, vec3(5, 1.15, 26.1))\nenemy_tick(brute, 0.01)\nenemy_tick(brute, 1.09)");
        require(world.get<HealthComponent>(hero).current==100,"Brute hit before its longer windup");
        run("enemy_tick(brute, 0.01)"); require(world.get<HealthComponent>(hero).current==76,"Brute did not use its own damage");
        run("set_health(hero, 100)\nteleport(hero, vec3(0, 1, 0))\nteleport(brute, vec3(0, 1.15, 1))\nenemy_tick(brute, 2)");
        require(world.get<HealthComponent>(hero).current==100,"Variant attacked inside the safe hub");
        runtime.execute("combatmod", "@Entities\nenemy Raider from \"adventure:Brute\":\n    .max_health = 150\n    .damage = 30\n");
        run("raider = enemy_at(12, 38, \"combatmod:Raider\")\nteleport(hero, vec3(12, 1.15, 36.1))\nenemy_tick(raider, 0.01)\nenemy_tick(raider, 1.1)");
        require(world.get<HealthComponent>(world.find("combatmod:Raider")).maximum==150 && world.get<HealthComponent>(hero).current==70,"Dependent mod enemy did not use inherited AI and patched stats");
        run("set_health(hero, 100)");
        // Player attacks enumerate the Sentinel component, including derived types.
        run("teleport(scout, vec3(0, 0.9, 14))\nteleport(brute, vec3(8, 1.15, 28))\nteleport(hero, vec3(0, 1, 12.5))\nstate.swing = weapon_definition()\nstate.aim = vec3(0, 0, 1)\nresolve_attack()");
        require(world.get<HealthComponent>(scout).current==20,"Player strike ignored a derived enemy type");
        run("teleport(hero, vec3(0, 1, 0))\ndamage(scout, 1000)\nenemy_tick(scout, 0.016)\nenemy_tick(scout, 0.016)\nloot_count = list_count(get_entities_with(GroundLoot))");
        require(state().member("gold",{}).number()==9 && value("loot_count").number()==1,"Defeat duplicated or lost its reward/drop");
        require(itemCount(world,hero,AssetId("adventure:TrailKnife"))==0,"Distant loot was awarded remotely");
        run("unknown = drop_item(\"missing:Relic\", vec3(0, 0, 0))\ncollect_loot()\nsave_game(\"ground\")");
        require(extension.process().empty(),"Ground loot could not be saved");
        run("load_game(\"ground\")"); require(extension.process().empty(),"Ground loot could not be restored"); hero=player.entity(); scout=world.findPersistent(scoutKey);
        run("loot_count = list_count(get_entities_with(GroundLoot))\nscout = get_entity(\"adventure:Scout\")\nbrute = get_entity(\"adventure:Brute\")");
        require(value("loot_count").number()==2 && itemCount(world,hero,AssetId("adventure:TrailKnife"))==0,"Save lost or collected ground loot");
        // A rejected item hook cannot consume the world drop or grant an item count.
        run("func invalid_item(item):\n    item.level = 0\n    return item\ninvalid_token = add_hook(\"adventure:create_item\", invalid_item)\nteleport(hero, vec3(0, 1, 14))");
        bool rejected=false;
        try { run("collect_loot()"); } catch(const DiagnosticError& error) { rejected=error.diagnostic.code=="SCRIPT325"; }
        require(rejected && itemCount(world,hero,AssetId("adventure:TrailKnife"))==0,"Rejected hook granted or consumed an item");
        run("unsubscribe(invalid_token)\nloot_count = list_count(get_entities_with(GroundLoot))");
        require(value("loot_count").number()==2,"Rejected hook consumed ground loot");
        // Full capacity leaves a drop intact; freeing a slot permits one pickup.
        run("give_item(hero, IronSword, 255)\nsync_equipment()\nteleport(hero, vec3(0, 1, 14))\ncollect_loot()\nloot_count = list_count(get_entities_with(GroundLoot))");
        require(value("loot_count").number()==2 && itemCount(world,hero,AssetId("adventure:TrailKnife"))==0,"Full inventory consumed ground loot");
        run("take_item(hero, IronSword, 255)\nsync_equipment()\nsync_equipment()\ncollect_loot()\ncollect_loot()\nloot_count = list_count(get_entities_with(GroundLoot))");
        require(value("loot_count").number()==1 && itemCount(world,hero,AssetId("adventure:TrailKnife"))==1,"Pickup was lost or duplicated after freeing capacity");
        run("state.gold = 100\ndamage(hero, 1000)\nrespawn_if_dead()\nloot_count = list_count(get_entities_with(GroundLoot))");
        require(value("loot_count").number()==1 && itemCount(world,hero,AssetId("adventure:TrailKnife"))==1 && state().member("gold",{}).number()==85,"Death lost items or uncollected drops");
        runtime.execute("missing", "@Items\nweapon Relic from \"adventure:IronSword\":\n    .name = \"Relic restored\"\n");
        saves.setMods({{"adventure","0.1.0"},{"combatmod","0.1.0"},{"missing","0.1.0"}});
        run("collect_loot()\nloot_count = list_count(get_entities_with(GroundLoot))");
        require(value("loot_count").number()==0 && itemCount(world,hero,AssetId("missing:Relic"))==1,"Unavailable mod drop did not survive until its definition returned");
        // Old saves have no keyed encounters. Reconcile the newly installed content once.
        run("destroy(get_entity_by_key(get_component(scout, Sentinel).visual))\ndestroy(scout)\ndestroy(get_entity_by_key(get_component(brute, Sentinel).visual))\ndestroy(brute)\nsave_game(\"before_encounters\")");
        require(extension.process().empty(),"Migration fixture save failed");
        auto data=SaveGame::inspect("user/combat-content-saves/before_encounters.json");
        auto& saved=std::get<Data::Object>(std::get<Data::Object>(std::get<Data::Object>(data.value).at("state").value).at("adventure").value);
        saved.erase("encounters");
        { std::ofstream file("user/combat-content-saves/legacy.json"); file<<encodeData(data); }
        run("load_game(\"legacy\")"); require(extension.process().empty(),"Old save encounter reconciliation failed"); hero=player.entity();
        require(world.find("adventure:Scout") && world.find("adventure:Brute"),"Old save did not receive new encounters");
        const auto reconciled=world.entities().size(); run("sync_world_content()"); require(world.entities().size()==reconciled,"Migrated encounters duplicated");
        // Catalog snapshots refresh on successful registry changes, including replacement.
        run("teleport(hero, vec3(0, 1, 0))\nold_region = current_region().name");
        runtime.execute("adventure","@Regions\nmodify Hub {\n    .name = \"ABRIGO ATUALIZADO\"\n}\n");
        run("new_region = current_region().name"); require(value("new_region").string()=="ABRIGO ATUALIZADO","Cached region did not invalidate after modify");
        run("spec = get_definition(\"Waystones\", \"adventure:Hub\")\nspec.name = \"Abrigo atualizado\"\nreplace_definition(\"Waystones\", \"adventure:Hub\", spec)\nnew_destination = list_at(destination_list(), 0).data.name");
        require(value("new_destination").string()=="Abrigo atualizado","Cached destination did not invalidate after replacement");
        // The renderer reuses identical UI pixels, while visible marker changes invalidate.
        run("map_open = false\nrefresh_hud()"); auto panel=ui.texture();
        run("teleport(hero, vec3(1, 1, 0))\nrefresh_hud()"); require(ui.texture()==panel,"Hidden markers recreated an identical UI texture");
        run("map_open = true\nrefresh_hud()"); auto map=ui.texture();
        run("refresh_hud()"); require(ui.texture()==map,"Unchanged map recreated its UI texture");
        run("teleport(hero, vec3(2, 1, 0))\nrefresh_hud()"); require(ui.texture()!=map,"Visible player marker failed to invalidate the map");
        if(argc>1) {
            runtime.execute("adventure","@Regions\nmodify Hub {\n    .name = \"ABRIGO\"\n}\n");
            run("map_open = false\nnotice = \"Novos inimigos: leve e blindado\"\nteleport(hero, vec3(0, 1, -6))\nteleport(get_entity(\"adventure:Scout\"), vec3(-2, 0.9, -6))\nteleport(get_entity(\"adventure:Brute\"), vec3(2, 1.15, -6))\nstate.attack = 0\nanimate_hero(0.016)\nrefresh_hud()");
            renderer.render(Camera{{6,4,-14},{0,1,-6}},collectRenderItems(world),ui.texture()); auto pixels=renderer.readback();
            std::ofstream image("user/combat-content-preview.ppm",std::ios::binary); image<<"P6\n960 640\n255\n";
            for(size_t i=0;i<pixels.size();i+=4) image.write(reinterpret_cast<const char*>(pixels.data()+i),3);
            require(renderer.stats().draws>100,"New art failed to render");
            std::cout<<"Preview draws="<<renderer.stats().draws<<" render_cpu_ms="<<renderer.stats().cpuMilliseconds<<"\n";
        }
        std::cout<<"Combat variants, ground loot, saves, reconciliation and caches verified\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
