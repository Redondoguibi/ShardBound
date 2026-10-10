#include "assets/CoreAssets.h"
#include "scripting/GameplayBridge.h"
#include "scripting/ExtensionBridge.h"
#include "game/GameUi.h"
#include "ecs/RenderSystem.h"
#include "platform/WindowSystem.h"
#include "platform/TextImage.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace game;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char**) {
    try {
        WindowSystem window({"Presentation acceptance",1280,720,true});
        Renderer renderer(window.nativeHandle(),1280,720,"assets/core/shaders/forward.hlsl",{false,1});
        VirtualFileSystem vfs; vfs.mount("core","assets/core"); AssetManager assets(vfs,renderer,"user/presentation-cache"); registerCoreModels(assets);
        World world; PhysicsSystem physics(world); PlayerController player(world,physics); InputSystem input; AudioSystem audio(true);
        script::Runtime runtime; bindRuntimeApi(runtime,world,assets); bindGameplayApi(runtime,world,physics,player,input,audio,assets);
        Components components; SaveGame saves("user/presentation-saves",world,physics,player,assets); ExtensionBridge extension(runtime,world,components,saves); GameUi ui(runtime,renderer);
        saves.setMods({{"adventure","0.1.0"}}); runtime.load("adventure","content/adventure","main.sl");
        auto emit=[&](const char* name,std::vector<script::Value> args=std::vector<script::Value>{}) { const auto errors=runtime.emit(name,args); if(!errors.empty()) throw DiagnosticError(errors.front()); };
        auto run=[&](std::string_view code) { runtime.execute("adventure","@Globals\n"+std::string(code)); };
        auto value=[&](const char* name) { return runtime.global("adventure",name); };
        auto key=[&](Key key) { input.beginFrame(); Event e; e.type=EventType::KeyDown; e.key=key; input.process(e); run("handle_input()"); e.type=EventType::KeyUp; input.process(e); input.beginFrame(); };
        auto pointer=[&](float x,float y) { Event e; e.type=EventType::MouseMove; e.x=x; e.y=y; input.process(e); };
        auto click=[&](float x,float y) { input.beginFrame(); pointer(x,y); Event e; e.type=EventType::MouseDown; e.mouse=MouseButton::Left; input.process(e); run("handle_input()"); e.type=EventType::MouseUp; input.process(e); input.beginFrame(); };
        auto capture=[&](const char* name) {
            run("refresh_hud()"); renderer.render(player.camera(),collectRenderItems(world),ui.texture()); const auto pixels=renderer.readback();
            require(renderer.stats().instances>100 && renderer.stats().shadowDraws>0,"Scenery or shadows missing");
            if(argc>1) { std::filesystem::create_directories("user/previews"); std::ofstream f(std::string("user/previews/")+name+".ppm",std::ios::binary); f<<"P6\n1280 720\n255\n"; for(size_t i=0;i<pixels.size();i+=4) f.write(reinterpret_cast<const char*>(pixels.data()+i),3); }
            std::cout<<name<<": "<<renderer.stats().draws<<" draws / "<<renderer.stats().instances<<" instances, "<<renderer.stats().cpuMilliseconds<<" ms render CPU\n";
        };
        auto source=[&](const char* path) { std::ifstream file(path); require(bool(file),"Missing presentation fixture"); std::ostringstream out; out<<file.rdbuf(); return out.str(); };
        // Reproduce a real v1 world, then load it through the current content migration.
        const auto legacyRoot=std::filesystem::path("user/presentation-v1");
        std::filesystem::create_directories(legacyRoot);
        std::filesystem::copy("content/adventure",legacyRoot,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        { std::ofstream file(legacyRoot/"world.sl"); file<<source("tests/fixtures/world-v1.sl"); }
        { std::ofstream file(legacyRoot/"scenery.sl"); file<<source("tests/fixtures/scenery-v1.sl"); }
        {
            World legacyWorld; PhysicsSystem legacyPhysics(legacyWorld); PlayerController legacyPlayer(legacyWorld,legacyPhysics);
            script::Runtime legacy; bindRuntimeApi(legacy,legacyWorld,assets); bindGameplayApi(legacy,legacyWorld,legacyPhysics,legacyPlayer,input,audio,assets);
            Components legacyComponents; SaveGame legacySaves("user/presentation-saves",legacyWorld,legacyPhysics,legacyPlayer,assets);
            legacySaves.setMods({{"adventure","0.1.0"}});
            ExtensionBridge legacyExtension(legacy,legacyWorld,legacyComponents,legacySaves); GameUi legacyUi(legacy,renderer);
            legacy.load("adventure",legacyRoot,"main.sl"); const auto errors=legacy.emit("on_start");
            if(!errors.empty()) throw DiagnosticError(errors.front());
            legacy.execute("adventure", "@Globals\nstate.gold = 37\ndiscovery(\"adventure:Forest\").active = true\nlist_at(state.items, 0).level = 7\npreserved = block_at(vec3(-7, 0.16, -3), vec3(2.13, 0.16, 1.83), vec3(0.25, 0.29, 0.29), false)\npreserved.name = \"mod_decoration\"\npreserved.active = false\nsave_game(\"scenery_v1\")");
            require(legacyExtension.process().empty(),"Legacy presentation save failed");
        }
        run("load_game(\"scenery_v1\")");
        const auto migrationErrors=extension.process(); if(!migrationErrors.empty()) throw DiagnosticError(migrationErrors.front());
        auto hero=player.entity();
        require(world.find("adventure_scenery_v2") && !world.find("adventure_scenery_v1"),"Scenery marker did not migrate");
        require(bool(world.find("mod_decoration")),"Visual migration removed named extension decoration");
        require(value("state").member("gold",{}).number()==37,"Visual migration changed currency");
        run("migration_discovery = discovered(\"adventure:Forest\")\nmigration_level = list_at(state.items, 0).level");
        require(value("migration_discovery").boolean() && value("migration_level").number()==7,"Visual migration changed discoveries or equipment");
        run("list_at(state.items, 0).level = 1\nstate.gold = 0");
        for(auto id:world.entities()) {
            const auto& definition=world.get<IdentityComponent>(id).definition;
            const auto& name=world.get<NameComponent>(id).name;
            require(!((definition=="adventure:Block" || definition=="adventure:Pine") && name==definition && !physics.contains(id)),"Obsolete official decoration survived visual migration");
            if(world.get<NameComponent>(id).name!="adventure_scenery_block") continue;
            const auto& t=world.get<TransformComponent>(id).local;
            require(t.rotation.x==0 && t.rotation.y==0 && t.rotation.z==0,"Official blocks have arbitrary rotations");
            if(t.position.y==-1) require(t.scale.x==1 && t.scale.y==1 && (t.scale.z==1 || t.scale.z==.5f),"Terrain is not made of regular blocks");
        }
        physics.teleport(hero,{0,1,17}); for(int i=0;i<30;++i) { player.move(0,0); physics.step(1.0f/60); }
        require(physics.grounded(hero),"Migrated visual terrain lost its physical ground"); physics.teleport(hero,{0,1.1f,0});
        // Real keyboard input, package tuning and recreated save body must give a jump of two author tree blocks.
        for(int i=0;i<30;++i) physics.step(1.0f/60);
        const auto takeoff=world.get<TransformComponent>(hero).local.position.y;
        key(Key::Space);
        require(physics.velocity(hero).y>0,"Space did not initiate the configured jump");
        float jumpHeight=0; int landedFrame=0;
        for(int i=1;i<=90;++i) {
            player.move(0,0); physics.step(1.0f/60);
            jumpHeight=std::max(jumpHeight,world.get<TransformComponent>(hero).local.position.y-takeoff);
            if(physics.grounded(hero)) { landedFrame=i; break; }
        }
        require(std::abs(jumpHeight-2*16*.038f)<.025f,"Player jump does not reach two author tree blocks");
        require(landedFrame>=40 && landedFrame<=45,"Player jump is too floaty or falls too quickly");
        std::cout<<"Jump: "<<jumpHeight<<" units, "<<landedFrame<<" frames airborne at 60 Hz\n";
        // Walk the existing route and collide with a house using the actual controller and physics.
        for(int i=0;i<540;++i) { player.move(0.573462f,0.819232f); physics.step(1.0f/60); }
        require(world.get<TransformComponent>(hero).local.position.z>35 && physics.grounded(hero),"Hub/forest route has a collision gap or obstruction");
        physics.teleport(hero,{-7,1.1f,-6.5f});
        for(int i=0;i<90;++i) { player.move(0.573462f,0.819232f); physics.step(1.0f/60); }
        require(world.get<TransformComponent>(hero).local.position.z< -5.25f,"House collision does not match its front wall");
        physics.teleport(hero,{0,1.1f,0}); player.move(0,0);
        require(world.entities().size()<3000,"Official scenery exceeds the save/mod headroom budget");
        const auto count=world.entities().size(); run("configure_presentation()\nconfigure_presentation()"); require(world.entities().size()==count,"Scenery migration is not idempotent");
        run("scenery_root.name = \"adventure_scenery_building\"\nconfigure_presentation()");
        require(world.entities().size()==count && world.find("adventure_scenery_v2") && !world.find("adventure_scenery_building"),"Interrupted scenery build could not recover without duplication");
        emit("on_ready"); require(ui.paused(),"Title screen failed to pause the simulation"); capture("title");
        click(640,310); require(!ui.paused() && !value("title_open").boolean(),"Title mouse button failed");
        run("notice_time = 0\nnotice = \"\"\nlast_notice = \"\"\nanimate_hero(0)"); capture("hub");
        key(Key::Escape); require(ui.paused(),"Escape did not pause");
        run("state.combat = 5"); emit("on_fixed_update",{script::Value(1.0)});
        require(value("state").member("combat",{}).number()==5,"Paused scripts advanced combat timers"); capture("pause");
        key(Key::Escape); require(!ui.paused(),"Escape did not resume");
        run("state.attack = 0.04\nstate.winding = false\nattack_buffer = 0.14");
        key(Key::I); require(value("inventory_open").boolean(),"Inventory did not open");
        emit("on_fixed_update",{script::Value(0.05)});
        require(value("state").member("attack",{}).number()==0 && !value("state").member("winding",{}).boolean(),"Buffered attack started behind inventory");
        run("give_item(hero, ForestSword, 1)\ngive_item(hero, HeavyBlade, 1)\ngive_item(hero, TrailKnife, 2)\ngive_item(hero, IronSword, 12)\nensure_equipment()");
        const auto originalEquipped=value("state").member("equipped_uid",{}).number();
        click(290,240); require(value("selected_item").number()==1 && value("state").member("equipped_uid",{}).number()==originalEquipped,"Selecting an inventory card equipped it prematurely");
        run("refresh_hud()\ncomparison_visible = false\nfor command in canvas:\n    if command.kind == \"text\" and command.text == \"+8\":\n        comparison_visible = true");
        require(value("comparison_visible").boolean(),"Selected weapon does not compare its damage to the equipped copy"); capture("inventory");
        click(900,580); require(value("state").member("equipped",{}).string()=="adventure:ForestSword","Inventory equip button failed");
        click(565,553); require(value("selected_item").number()==12,"Next inventory page failed");
        click(580,450); require(value("selected_item").number()==12,"Empty inventory cell changed selection");
        click(290,240); key(Key::E); require(value("state").member("equipped_uid",{}).number()==14,"Second-page copy could not be equipped");
        click(550,160); require(value("inventory_filter").number()==2,"Inventory filter did not change");
        run("refresh_hud()"); key(Key::E); require(value("state").member("equipped_uid",{}).number()==14,"Empty category changed equipment");
        click(130,160); run("take_item(hero, ForestSword, 1)\ntake_item(hero, HeavyBlade, 1)\ntake_item(hero, TrailKnife, 2)\ntake_item(hero, IronSword, 12)\nensure_equipment()\nsync_equipment()\nselected_item = 0");
        input.beginFrame(); Event close; close.type=EventType::KeyDown; close.key=Key::Escape; input.process(close);
        Event mouse; mouse.type=EventType::MouseDown; mouse.mouse=MouseButton::Left; input.process(mouse); run("handle_input()");
        close.type=EventType::KeyUp; input.process(close); mouse.type=EventType::MouseUp; input.process(mouse); input.beginFrame();
        require(!value("inventory_open").boolean() && !ui.paused(),"Escape should close the panel first");
        require(value("state").member("attack",{}).number()==0,"Clicking while closing inventory leaked into an attack");
        run("state.combat = 0\ndiscovery(\"adventure:Hub\").active = true\ndiscovery(\"adventure:Forest\").active = true\ndiscovery(\"adventure:Grove\").active = true\nmap_open = true"); capture("map");
        click(860,291); require(!value("map_open").boolean(),"Map click failed to travel"); require(value("state").member("attack",{}).number()==0,"UI click leaked into a melee attack");
        run("teleport(hero, vec3(0, 1, 17))\nnotice = \"\"\nnotice_time = 0\nstate.attack = 0\nanimate_hero(0)"); capture("forest");
        // Projection and unprojection must agree, including the canvas letterbox on a square viewport.
        for(const auto width : {1280,720}) {
            input.setViewport(width,720); run("point = screen_position(vec3(hero.position.x + 3, hero.position.y, hero.position.z))");
            const auto p=value("point"); const float scale=std::min(width/1280.0f,1.0f);
            pointer(float(p.member("x",{}).number())*scale+(width-1280*scale)/2,float(p.member("y",{}).number())*scale+(720-720*scale)/2);
            run("aim = aim_position()\nupdate_aim()"); const auto aim=value("aim");
            require(std::abs(aim.member("x",{}).number()-3)<.02 && std::abs(aim.member("z",{}).number()-17)<.02,"Cursor projection did not round trip");
            require(value("facing").member("x",{}).number()>.99,"Cursor did not aim the weapon");
        }
        input.setViewport(1280,720);
        run("point = screen_position(vec3(hero.position.x + 3, hero.position.y, hero.position.z))");
        pointer(float(value("point").member("x",{}).number()),float(value("point").member("y",{}).number()));
        run("begin_attack()\nlocked = state.aim.x"); pointer(10,10); run("update_aim()");
        require(value("facing").member("x",{}).number()>.99,"Aim changed during a committed attack");
        run("state.winding = false\nstate.attack = 0.04\nattack_buffer = 0.14\ncombat_tick(0.05)"); require(value("state").member("winding",{}).boolean(),"Buffered strike was lost at recovery boundary");
        run("state.attack = 0\nstate.winding = false\nstate.swing = weapon_definition()\nstate.aim = vec3(0, 0, 1)\nfoe = get_entity(\"adventure:Scout\")\nteleport(foe, vec3(0, 0.9, 18.4))\nresolve_attack()\nanimate_hero(0)\nanimate_combat_visuals(0.016)");
        require(!std::get<std::shared_ptr<script::List>>(value("damage_labels").data)->values.empty(),"Damage feedback was not created"); capture("combat");
        run("save_game(\"presentation\")"); require(extension.process().empty(),"Scenery save failed");
        run("load_game(\"presentation\")"); require(extension.process().empty(),"Scenery restore failed");
        require(world.entities().size()==count,"Save duplicated official decoration");
        require(requireEntity(value("scenery_root"),world,{})==world.find("adventure_scenery_v2"),"Scene root was not rebound after restoring a save");
        run("take_item(hero, IronSword, 1)\nensure_equipment()\ninventory_open = true\nrefresh_hud()\nempty_feedback = false\nfor command in canvas:\n    if command.kind == \"text\" and command.text == \"Sem arma equipada\":\n        empty_feedback = true");
        require(value("empty_feedback").boolean(),"Empty equipment still displays a fallback weapon as equipped");
        // Transparent canvas must leave the world visible; overlapping shapes retain source-over alpha.
        CanvasCommand rect; rect.kind="rect"; rect.x=10; rect.y=10; rect.w=20; rect.h=20; rect.color={255,0,0,128};
        const auto rgba=canvasImage({rect,rect}); require(rgba[3]==0 && rgba[(15*1280+15)*4+3]>=190 && rgba[(15*1280+15)*4+3]<=192,"Canvas alpha composition is wrong");
        std::cout<<"Presentation, cursor aim, input isolation, pause, feedback and save compatibility verified\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
