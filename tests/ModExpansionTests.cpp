#include "world/WorldBridge.h"
#include "game/WidgetUi.h"
#include "game/GameUi.h"
#include "game/FrontEnd.h"
#include "game/Equipment.h"
#include "game/InventorySystem.h"
#include "scripting/GameplayBridge.h"
#include "scripting/ExtensionBridge.h"
#include "scripting/DataConversion.h"
#include "mods/ModLoader.h"
#include "platform/WindowSystem.h"
#include <Windows.h>
#include <Psapi.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <limits>
using namespace game;
using namespace game::script;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F action) { bool rejected=false;try { action(); }catch(const std::exception&) { rejected=true; }check(rejected,"Invalid operation accepted"); }
Value json(const char* text) { return fromData(decodeData(text)); }
Value text(const char* value) { return Value(std::string(value)); }
size_t memory() { PROCESS_MEMORY_COUNTERS_EX counters{};if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters))) throw std::runtime_error("Cannot measure process memory");return counters.PrivateUsage; }
}
int main(int argc,char** argv) {
    try {
        std::cout<<std::unitbuf;
        const int iterations=argc==3 && std::string(argv[1])=="--soak"?std::clamp(std::stoi(argv[2]),1,10000):24;
        const auto directory=std::filesystem::path("user/expansion-tests")/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        WindowSystem window({"Mod platform expansion",1280,720,true});Renderer renderer(window.nativeHandle(),1280,720,"assets/core/shaders/forward.hlsl",{false,1});
        VirtualFileSystem vfs;AssetManager assets(vfs,renderer,"user/expansion-cache");ModLoader loader(vfs,assets);const auto mods=loader.load("content");check(mods.diagnostics.empty(),"Official content failed to load");
        auto definition=WorldDefinition::load(mods.mods);auto& settings=definition.generators.at("plains:gentle");settings.relief=0;settings.noise.clear();settings.structures.clear();
        RegionEdits pages(directory/"pages");
        for(int cx=-48;cx<48;++cx) { BlockEdits cells;for(int x=0;x<8;++x) for(int z=0;z<8;++z) cells[{cx*16+x,6,z}]="plains:stone";pages.replace({cx,0},std::move(cells));check(pages.residentPages()<=64,"Page cache grew beyond its limit"); }
        const auto checkpoint=pages.snapshot();check(pages.all().size()==6144 && pages.residentPages()<=64,"Paged edits were lost during eviction");
        check(pages.neighborhood({0,0}).size()==72,"Chunk job copied unrelated edits or omitted its border column");
        pages.replace({-48,0},{{{-768,6,0},"plains:dirt"}});
        auto restored=RegionEdits::restore(directory/"pages",checkpoint,definition);check(restored.get(-768,6,0)=="plains:stone" && restored.all().size()==6144,"New edit changed an earlier checkpoint");
        auto traversal=checkpoint;traversal["pages"][0][2]="../checkpoint.json";rejects([&]{RegionEdits::restore(directory/"pages",traversal,definition);});
        World world;PhysicsSystem physics(world);PlayerController player(world,physics);InputSystem input;AudioSystem audio(true);Runtime runtime;
        bindRuntimeApi(runtime,world,assets);bindGameplayApi(runtime,world,physics,player,input,audio,assets);SaveGame saves(directory,world,physics,player,assets);Components components;ExtensionBridge extensions(runtime,world,components,saves);
        WorldRecord record;record.seed="streaming-expansion";record.generator="plains:gentle";ChunkWorld terrain(renderer,assets,world,physics,player,TerrainGenerator(definition,record.generator,record.seed));bindWorldApi(runtime,terrain,record);bindWorldPersistence(saves,terrain);
        runtime.execute("qa","@Items\nitem Blade:\n    .damage = 10\n@Globals\nready = true\nscroll_result = 0");
        const auto call=[&](const char* name,std::vector<Value> args={}) { return runtime.invoke(runtime.global("qa",name),args); };
        auto actor=world.create("actor");world.add(actor,TransformComponent{{{8,5,8}}});physics.attachBox(actor,{.3f,.9f,.3f},true);player.possess(actor,4);terrain.update(2,true);
        BlockEdits first,second;for(int i=0;i<4096;++i) first[{1000+i%128,9,1000+i/128}]="plains:stone";for(int i=0;i<2048;++i) second[{1000+i%128,10,1000+i/128}]="plains:stone";
        check(terrain.editBlocks(first)==4096 && terrain.editBlocks(second)==2048 && terrain.blockEdits().size()==6144,"World retained a global 4096 edit ceiling");
        const auto actorKey=world.get<IdentityComponent>(actor).persistent;saves.save("large");check(saves.load("large").empty(),"Large paged save failed to load");actor=world.findPersistent(actorKey);check(terrain.blockEdits().size()==6144,"Large save lost distant edits");
        const auto originalPages=terrain.streamingStats().pages;BlockEdits sparse;for(int i=0;i<96;++i) sparse[{-20000+i*16,11,-20000}]="plains:stone";
        check(terrain.editBlocks(sparse)==96 && terrain.streamingStats().residentPages<=64,"Sparse transaction exceeded cache limit");check(terrain.undoEdits() && terrain.blockEdits().size()==6144 && terrain.streamingStats().pages==originalPages,"Sparse undo left tombstone pages or lost previous edits");
        terrain.setBlock(1000,9,1000,"plains:dirt");saves.save("later");check(saves.load("large").empty() && terrain.blockAt(1000,9,1000)=="plains:stone","Checkpoint did not preserve immutable terrain pages");actor=world.findPersistent(actorKey);
        // Exercise migration from the previous inline block format.
        auto legacy=nlohmann::json::parse(encodeData(SaveGame::inspect(directory/"large.json")));legacy["state"]["core"].erase("regions");legacy["state"]["core"]["blocks"]={{"2000","-2000",8,"plains:stone"}};writeJsonAtomic(directory/"legacy.json",legacy);
        check(saves.load("legacy").empty() && terrain.blockAt(2000,8,-2000)=="plains:stone","Legacy inline terrain did not migrate");actor=world.findPersistent(actorKey);
        // Keep a valid live world while testing a corrupt external page.
        const auto large=nlohmann::json::parse(encodeData(SaveGame::inspect(directory/"large.json")));
        const auto pageNames=RegionEdits::referencedFiles(directory/"terrain",large["state"]["core"]["regions"]);
        const auto pageFile=directory/"terrain"/(*std::find_if(pageNames.begin(),pageNames.end(),[](const auto& name){return name.ends_with(".json");}));
        std::ifstream pageIn(pageFile,std::ios::binary);const std::string original{std::istreambuf_iterator<char>(pageIn),{}};pageIn.close();
        { std::ofstream corrupt(pageFile,std::ios::binary);corrupt<<original<<" "; }
        rejects([&]{saves.load("large");});check(world.alive(actor) && player.entity()==actor && terrain.blockAt(2000,8,-2000)=="plains:stone","Corrupt terrain page changed the live world");
        { std::ofstream repair(pageFile,std::ios::binary);repair<<original; }
        giveItem(world,actor,AssetId("qa:Blade"),1);const auto value=entityValue(world,actor);auto item=call("create_item_instance",{value,text("qa:Blade"),json(R"({"maximum":100,"stats":{},"affixes":{}})")});call("item_durability",{value,item,Value(37.0)});
        auto recipient=world.create("recipient");auto target=entityValue(world,recipient);auto newId=call("transfer_item_instance",{value,target,item});
        check(itemCount(world,actor,AssetId("qa:Blade"))==0 && itemCount(world,recipient,AssetId("qa:Blade"))==1 && readEquipment(world,recipient).items.at(newId.string()).durability==37,"Transfer lost inventory/durability or duplicated item");
        check(world.entitiesWithCustom("core:item_instances").size()==2,"Atomic transfer desynchronized the component index");
        call("equip_instance",{target,text("weapon"),newId});rejects([&]{call("transfer_item_instance",{target,value,newId});});check(itemCount(world,recipient,AssetId("qa:Blade"))==1,"Rejected transfer changed inventory");call("equip_instance",{target,text("weapon"),Value{}});
        const auto token=runtime.subscribe("core:transfer_instance",native([](const auto& a,const auto& at){auto result=clone(a[0]);result.setMember("allow",Value(false),at);return result;}),true,"qa");
        check(std::holds_alternative<std::monostate>(call("transfer_item_instance",{target,value,newId}).data) && itemCount(world,recipient,AssetId("qa:Blade"))==1,"Transfer ignored veto");runtime.unsubscribe(token);
        giveItem(world,actor,AssetId("qa:Blade"),std::numeric_limits<std::uint32_t>::max());
        rejects([&]{call("transfer_item_instance",{target,value,newId});});check(readEquipment(world,recipient).items.contains(newId.string()) && itemCount(world,recipient,AssetId("qa:Blade"))==1,"Overflow transfer partially committed");
        check(takeItem(world,actor,AssetId("qa:Blade"),std::numeric_limits<std::uint32_t>::max()),"Overflow fixture cleanup failed");world.destroy(recipient);
        auto obstacle=world.create("moving_obstacle");world.add(obstacle,TransformComponent{{{4.5f,5,.5f}}});physics.attachBox(obstacle,{.45f,1,.45f},true);
        const auto start=json(R"({"x":0,"y":4,"z":0})"),goal=json(R"({"x":8,"y":4,"z":0})");auto options=json(R"({"max_nodes":512,"clearance":2,"step":0,"drop":0,"avoid_entities":true})");
        auto path=call("terrain_path",{start,goal,options});check(path.member("status",{}).string()=="found","Dynamic obstacle blocked all routes");
        bool detour=false;for(const auto& point:std::get<std::shared_ptr<List>>(path.member("points",{}).data)->values) detour|=point.member("z",{}).number()!=.5;check(detour,"Navigation walked through a dynamic body");
        world.get<ActiveComponent>(obstacle).value=false;path=call("terrain_path",{start,goal,options});bool inactiveDetour=false;for(const auto& point:std::get<std::shared_ptr<List>>(path.member("points",{}).data)->values) inactiveDetour|=point.member("z",{}).number()!=.5;check(inactiveDetour,"Navigation ignored an inactive entity whose physical body still collides");world.get<ActiveComponent>(obstacle).value=true;
        options.setMember("ignore",list({entityValue(world,obstacle)}),{});auto ignoredPath=call("terrain_path",{start,goal,options});check(std::get<std::shared_ptr<List>>(ignoredPath.member("points",{}).data)->values.size()==9,"Navigation did not honor its ignore list");options.setMember("ignore",list({}),{});
        physics.teleport(obstacle,{4.5f,5,8.5f});path=call("terrain_path",{start,goal,options});check(std::get<std::shared_ptr<List>>(path.member("points",{}).data)->values.size()==9,"Navigation retained a stale dynamic obstacle");physics.detach(obstacle);world.destroy(obstacle);
        runtime.execute("qa",R"(@Globals
func measured(dt):
    ready = true
register_system("qa:measured", "fixed", measured)
)","measurement.sl");check(runtime.emit("on_fixed_update",{Value(1.0/60)}).empty(),"Measured system failed");
        rejects([&]{runtime.execute("qa","script_error(\"diagnostic fixture\")","fixture.sl");});auto report=call("developer_report");const auto diagnostics=std::get<std::shared_ptr<List>>(report.member("diagnostics",{}).data)->values;check(!diagnostics.empty() && diagnostics.back().member("file",{}).string()=="fixture.sl" && diagnostics.back().member("line",{}).number()==1,"Diagnostic lost source location");
        const auto systems=std::get<std::shared_ptr<List>>(report.member("systems",{}).data)->values;check(systems.size()==1 && systems[0].member("calls",{}).number()==1 && systems[0].member("file",{}).string()=="measurement.sl","Per-system profiler lacks calls/source");
        check(call("inspect_entity",{entityValue(world,actor)}).member("key",{}).string()==actorKey,"Entity inspector lost persistent identity");saves.save("diagnostics");const auto diagnosticSlot=SaveGame::inspect(directory/"diagnostics.json");check(std::filesystem::exists(call("write_debug_report").string()),"Diagnostics export was not written");check(SaveGame::inspect(directory/"diagnostics.json")==diagnosticSlot,"Diagnostic export overwrote a save slot");
        const auto widths=textAdvances("ação😀");check(widths.back().first==std::string("ação😀").size() && widths.back().second>0,"Text measurement lost UTF-8/surrogate boundaries");
        runtime.bind("native_failure",native([](const auto&,const auto&)->Value { throw std::runtime_error("native fixture"); }));rejects([&]{runtime.execute("qa","native_failure()","native-failure.sl");});
        auto nativeErrors=std::get<std::shared_ptr<List>>(call("developer_report").member("diagnostics",{}).data)->values;check(nativeErrors.back().member("file",{}).string()=="native-failure.sl","Native exception lost its script callsite");
        for(int i=0;i<300;++i) rejects([&]{runtime.execute("qa","script_error(\"bounded fixture\")","bounded.sl");});check(std::get<std::shared_ptr<List>>(call("developer_report").member("diagnostics",{}).data)->values.size()==256,"Diagnostic journal grew without a bound");
        // Test the real editing code with an injected clipboard, leaving the user's clipboard intact.
        WidgetUi widgets(runtime,input);std::string clipboard;widgets.readClipboard=[&]{return clipboard;};widgets.writeClipboard=[&](const auto& content){clipboard=content;};
        runtime.bind("spec",json(R"({"layout":"column","modal":true,"x":0,"y":0,"w":0.5,"h":0.5,"content_height":1080,"gap":0,"widgets":[{"id":"input","kind":"input","value":"olá"},{"id":"slider","kind":"slider","min":0,"max":10,"step":1,"value":0},{"id":"bottom","kind":"button","text":"Fim"}]})"));runtime.execute("qa","show_widgets(\"qa:editing\", spec)\nfocus_widget(\"qa:editing\", \"input\")");
        const auto event=[&](Event e) { input.beginFrame();input.process(e);widgets.event(e);widgets.update(); };
        const auto key=[&](Key k,bool control=false,bool shift=false) { Event e;e.type=EventType::KeyDown;e.key=k;e.control=control;e.shift=shift;event(e); };
        key(Key::A,true);key(Key::C,true);check(clipboard=="olá","Copy did not preserve selected UTF-8");clipboard.clear();key(Key::V,true);check(call("widget_value",{text("qa:editing"),text("input")}).string()=="olá","Empty clipboard erased selected text");clipboard="ação";key(Key::V,true);check(call("widget_value",{text("qa:editing"),text("input")}).string()=="ação","Paste did not replace selection");
        key(Key::Home);key(Key::Right,false,true);key(Key::X,true);check(clipboard=="a" && call("widget_value",{text("qa:editing"),text("input")}).string()=="ção","Cut damaged UTF-8 boundaries");key(Key::Delete);check(call("widget_value",{text("qa:editing"),text("input")}).string()=="ão","Forward delete split a UTF-8 codepoint");
        runtime.execute("qa","focus_widget(\"qa:editing\", \"slider\")");runtime.execute("qa","scroll_result = scroll_widgets(\"qa:editing\", 360)");check(runtime.global("qa","scroll_result").number()==360,"Screen did not scroll");
        Event mouse;mouse.type=EventType::MouseMove;mouse.x=100;mouse.y=100;event(mouse);mouse.type=EventType::MouseDown;mouse.mouse=MouseButton::Left;event(mouse);mouse.type=EventType::MouseMove;mouse.x=600;event(mouse);check(call("widget_value",{text("qa:editing"),text("slider")}).number()>=9,"Slider did not follow pointer drag");mouse.type=EventType::MouseUp;event(mouse);
        mouse.type=EventType::MouseWheel;mouse.y=-4;event(mouse);runtime.execute("qa","scroll_result = scroll_widgets(\"qa:editing\", 9999)");check(runtime.global("qa","scroll_result").number()==720,"Scroll did not clamp to content");
        auto preview=widgets.render(renderer,{});check(bool(preview),"Scrollable UI failed to render");renderer.render(Camera{},std::vector<RenderItem>{},preview);
        { const auto pixels=renderer.readback();std::ofstream image("user/expansion-ui-preview.ppm",std::ios::binary);image<<"P6\n1280 720\n255\n";for(size_t i=0;i<pixels.size();i+=4) image.write(reinterpret_cast<const char*>(pixels.data()+i),3); }
runtime.execute("qa","hide_widgets(\"qa:editing\")");
        runtime.bind("drag_spec",json(R"({"layout":"absolute","modal":true,"x":0,"y":0,"w":1,"h":0.5,"widgets":[{"id":"source","kind":"button","text":"Espada","x":0,"y":0,"w":0.25,"h":0.5,"draggable":true,"payload":{"instance":"i1"}},{"id":"target","kind":"panel","x":0.5,"y":0,"w":0.25,"h":0.5,"drop_target":true}]})"));runtime.execute("qa","show_widgets(\"qa:drag\", drag_spec)");
        std::vector<std::string> dragEvents;bool delivered=false;const auto dragToken=runtime.subscribe("on_ui",native([&](const auto& a,const auto&) { if(a[0].string()=="qa:drag") { dragEvents.push_back(a[2].string());if(a[2].string()=="drop") check(a[3].member("payload",{}).member("instance",{}).string()=="i1","Drop lost its payload");if(a[2].string()=="drag_end") delivered=a[3].member("delivered",{}).boolean(); }return Value{}; }),false,"qa");
        mouse.type=EventType::MouseMove;mouse.x=50;mouse.y=50;event(mouse);mouse.type=EventType::MouseDown;mouse.mouse=MouseButton::Left;event(mouse);mouse.type=EventType::MouseMove;mouse.x=800;event(mouse);mouse.type=EventType::MouseUp;event(mouse);
        check(dragEvents==std::vector<std::string>{"drag_start","drop","drag_end"} && delivered,"Drag and drop did not deliver exactly once");
        runtime.bind("block_spec",json(R"({"layout":"column","modal":false,"x":0.5,"y":0,"w":0.25,"h":0.25,"widgets":[{"id":"blocked","kind":"panel","enabled":false}]})"));runtime.execute("qa","show_widgets(\"qa:block\", block_spec)");dragEvents.clear();delivered=true;
        mouse.type=EventType::MouseMove;mouse.x=50;event(mouse);mouse.type=EventType::MouseDown;event(mouse);mouse.type=EventType::MouseMove;mouse.x=800;event(mouse);mouse.type=EventType::MouseUp;event(mouse);
        check(dragEvents==std::vector<std::string>{"drag_start","drag_end"} && !delivered,"Drop crossed a disabled upper panel");runtime.execute("qa","hide_widgets(\"qa:block\")");dragEvents.clear();
        mouse.type=EventType::MouseMove;mouse.x=50;event(mouse);mouse.type=EventType::MouseDown;event(mouse);mouse.type=EventType::MouseMove;mouse.x=800;event(mouse);mouse.type=EventType::FocusLost;event(mouse);check(dragEvents==std::vector<std::string>{"drag_start","drag_cancel"},"Window focus loss did not cancel drag");mouse.type=EventType::FocusGained;event(mouse);
        runtime.unsubscribe(dragToken);runtime.execute("qa","hide_widgets(\"qa:drag\")");
        { GameUi panel(runtime,renderer,&input);panel.diagnostics=[] { return std::string("Diagnósticos UTF-8\nSistema qa:measured"); };panel.toggleDiagnostics();panel.renderWidgets(renderer);check(bool(panel.texture()),"F3 overlay did not render without mod widgets");panel.diagnostics=[]()->std::string { throw std::runtime_error("report fixture"); };panel.toggleDiagnostics();panel.toggleDiagnostics();panel.renderWidgets(renderer);check(bool(panel.texture()),"Diagnostic provider failure escaped into gameplay");panel.toggleDiagnostics();panel.renderWidgets(renderer);check(!panel.texture(),"Closed diagnostic overlay retained its texture"); }
        {
            GameUi integrated(runtime,renderer,&input);FrontEnd front(renderer,window,input,directory/"frontend",definition,{});front.startWorld=[](const auto&,bool){};front.open(record,false);
            runtime.execute("qa","show_widgets(\"qa:escape\", spec)");int cancelled=0;const auto cancelToken=runtime.subscribe("on_ui",native([&](const auto& a,const auto&){if(a[0].string()=="qa:escape" && a[2].string()=="cancel") ++cancelled;return Value{};}),false,"qa");
            Event escape;escape.type=EventType::KeyDown;escape.key=Key::Escape;integrated.event(escape);front.event(escape,integrated.capturesMenuInput(escape));integrated.updateInput();check(front.playing() && cancelled==1,"Widget Escape also paused the front end");
            runtime.execute("qa","hide_widgets(\"qa:escape\")");front.event(escape,integrated.capturesMenuInput(escape));check(front.screen()==FrontEnd::Screen::Pause,"Closing widgets disabled normal Escape pause");runtime.unsubscribe(cancelToken);
        }
        terrain.clear();terrain.update(3);terrain.setBlock(-31,8,-31,"plains:stone");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
        while(terrain.loadedChunks()<49 && std::chrono::steady_clock::now()<deadline) { terrain.update(3);std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        check(terrain.loadedChunks()==49 && terrain.streamingStats().generated>0,"Background streaming did not complete");
        const auto stone=assets.texture(AssetId(definition.blocks.at("plains:stone").texture));bool updatedMesh=false;
        for(const auto& renderItem:terrain.renderItems()) if(renderItem.world.values[12]==-32 && renderItem.world.values[14]==-32 && renderItem.material.texture==stone) updatedMesh=true;
        check(updatedMesh,"An old background result overwrote an edited chunk");
        std::vector<double> durations;size_t peakMemory=memory();const auto startMemory=peakMemory;nlohmann::json samples=nlohmann::json::array();
        for(int i=0;i<iterations;++i) {
            const auto begin=std::chrono::steady_clock::now();physics.teleport(actor,{float(130+(i%5)*16),5,float(-130-(i%7)*16)});terrain.update(3);
            if(i%4==0) terrain.editBlocks({{{terrain.originX()+10,8,terrain.originZ()+10},"plains:stone"}});
            terrain.update(3);const auto stats=terrain.streamingStats();check(stats.loaded<=49 && stats.queued<=24 && stats.ready<=8 && stats.residentPages<=64 && world.entities().size()<400,"Exploration exceeded bounded resources");
            durations.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
            if(i%8==0) { player.updateCamera(0);renderer.render(player.camera(),terrain.renderItems());renderer.present(); }
            const auto privateBytes=memory();peakMemory=std::max(peakMemory,privateBytes);if(i%16==0 || i+1==iterations) samples.push_back({{"iteration",i+1},{"private_bytes",privateBytes},{"loaded",stats.loaded},{"queued",stats.queued}});std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::sort(durations.begin(),durations.end());const auto stats=terrain.streamingStats();
        nlohmann::json measurements={{"iterations",iterations},{"private_samples",std::move(samples)},{"update_pair_p50_ms",durations[durations.size()/2]},{"update_pair_p95_ms",durations[std::min(durations.size()-1,durations.size()*95/100)]},{"update_pair_max_ms",durations.back()},{"private_bytes_start",startMemory},{"private_bytes_end",memory()},{"private_bytes_peak",peakMemory},{"loaded",stats.loaded},{"queued",stats.queued},{"resident_pages",stats.residentPages},{"generated",stats.generated},{"discarded",stats.discarded}};
        std::ofstream out("user/expansion-soak-"+std::to_string(iterations)+".json");out<<measurements.dump(2);out.close();check(bool(out),"Cannot write soak measurements");
        std::cout<<"PASS: paged persistence, asynchronous streaming, diagnostics, transfer, navigation and UI editing\n";return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
