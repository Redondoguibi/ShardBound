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
        emit("on_start"); auto hero=player.entity();
        require(world.entities().size()<3000,"Official scenery exceeds the save/mod headroom budget");
        const auto count=world.entities().size(); run("configure_presentation()\nconfigure_presentation()"); require(world.entities().size()==count,"Scenery migration is not idempotent");
        emit("on_ready"); require(ui.paused(),"Title screen failed to pause the simulation"); capture("title");
        click(640,310); require(!ui.paused() && !value("title_open").boolean(),"Title mouse button failed");
        run("notice_time = 0\nnotice = \"\"\nlast_notice = \"\"\nanimate_hero(0)"); capture("hub");
        key(Key::Escape); require(ui.paused(),"Escape did not pause");
        run("state.combat = 5"); emit("on_fixed_update",{script::Value(1.0)});
        require(value("state").member("combat",{}).number()==5,"Paused scripts advanced combat timers"); capture("pause");
        key(Key::Escape); require(!ui.paused(),"Escape did not resume");
        key(Key::I); require(value("inventory_open").boolean(),"Inventory did not open");
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
        run("take_item(hero, IronSword, 1)\nensure_equipment()\ninventory_open = true\nrefresh_hud()\nempty_feedback = false\nfor command in canvas:\n    if command.kind == \"text\" and command.text == \"Sem arma equipada\":\n        empty_feedback = true");
        require(value("empty_feedback").boolean(),"Empty equipment still displays a fallback weapon as equipped");
        // Transparent canvas must leave the world visible; overlapping shapes retain source-over alpha.
        CanvasCommand rect; rect.kind="rect"; rect.x=10; rect.y=10; rect.w=20; rect.h=20; rect.color={255,0,0,128};
        const auto rgba=canvasImage({rect,rect}); require(rgba[3]==0 && rgba[(15*1280+15)*4+3]>=190 && rgba[(15*1280+15)*4+3]<=192,"Canvas alpha composition is wrong");
        std::cout<<"Presentation, cursor aim, input isolation, pause, feedback and save compatibility verified\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
