#include "world/WorldBridge.h"
#include "world/FiniteMap.h"
#include "world/MapImport.h"
#include "tools/BlockAuthoring.h"
#include "tools/EquipmentAuthoring.h"
#include "tools/MapPackage.h"
#include "core/AtomicFile.h"
#include "assets/StudioPackage.h"
#include <algorithm>
#include "game/FrontEnd.h"
#include "game/GameUi.h"
#include "game/InventorySystem.h"
#include "scripting/GameplayBridge.h"
#include "scripting/ExtensionBridge.h"
#include "scripting/StudioBridge.h"
#include "mods/ModLoader.h"
#include "ecs/RenderSystem.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <set>
using namespace game;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool rejected=false;try { f(); }catch(const std::exception&) { rejected=true; }check(rejected,"Invalid data accepted"); }
void capture(Renderer& renderer,const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());auto pixels=renderer.readback();std::ofstream out(path,std::ios::binary);
    out<<"P6\n"<<renderer.renderWidth()<<' '<<renderer.renderHeight()<<"\n255\n";for(size_t i=0;i<pixels.size();i+=4) out.write(reinterpret_cast<const char*>(pixels.data()+i),3);
}
}
int main() {
    try {
        WindowSystem window({"Procedural acceptance",1280,720,true});Renderer renderer(window.nativeHandle(),1280,720,"assets/core/shaders/forward.hlsl",{false,1});
        VirtualFileSystem vfs;AssetManager assets(vfs,renderer,"user/procedural-cache");ModLoader loader(vfs,assets);const auto report=loader.load("content");
        check(report.diagnostics.empty() && report.mods.size()==2,"Default content must be exactly the two official mods");
        const auto definition=WorldDefinition::load(report.mods);check(definition.generators.contains("plains:gentle"),"Plains generator missing");
        std::map<std::string,std::string> versions;for(const auto& m:report.mods) { const auto v=m.version.parts;versions[m.id]=std::to_string(v[0])+"."+std::to_string(v[1])+"."+std::to_string(v[2]); }
        TerrainGenerator a(definition,"plains:gentle","test-seed"),b(definition,"plains:gentle","test-seed"),c(definition,"plains:gentle","other-seed");
        check(floorDiv(-1,16)==-1 && floorDiv(-16,16)==-1 && floorDiv(-17,16)==-2,"Negative chunk coordinates incorrect");
        size_t differences=0;for(int z=-180;z<=180;z+=7) for(int x=-180;x<=180;x+=7) {
            check(a.height(x,z)==b.height(x,z),"Seed not reproducible");differences+=a.height(x,z)!=c.height(x,z);
            check(a.height(x,z)>=3 && a.height(x,z)<=5,"Plains relief too large");check(std::abs(a.height(x,z)-a.height(x+1,z))<=1,"Discontinuous terrain");
        }check(differences>100,"Different seeds produce same terrain");
        check(a.height(100000000000LL,-100000000000LL)==b.height(100000000000LL,-100000000000LL),"Distant coordinates unstable");
        auto flat=definition;flat.generators.at("plains:gentle").relief=0;flat.generators.at("plains:gentle").structures.clear();
        const auto chunk=buildChunk(TerrainGenerator(flat,"plains:gentle","flat"),-1,0);
        check(chunk.boxes.size()==1 && chunk.meshes.size()==1 && chunk.meshes.begin()->second.indices.size()==256*6,"Hidden voxel faces not culled or terrain collision not merged");
        BlockEdits editedBlocks;editedBlocks.emplace(std::tuple<std::int64_t,int,std::int64_t>{-12,0,4},"");
        editedBlocks.emplace(std::tuple<std::int64_t,int,std::int64_t>{-12,7,4},"plains:stone");
        const auto editedChunk=buildChunk(TerrainGenerator(flat,"plains:gentle","flat"),-1,0,&editedBlocks);
        check(editedChunk.boxes.size()>1 && editedChunk.meshes.at("plains:stone").indices.size()>0,"World edits did not rebuild mesh/collision");
        BlockEdits edgeBlocks;
        edgeBlocks.emplace(std::tuple<std::int64_t,int,std::int64_t>{0,8,0},"plains:stone");
        const auto exposedEdge=buildChunk(TerrainGenerator(flat,"plains:gentle","flat"),0,0,&edgeBlocks);
        edgeBlocks.emplace(std::tuple<std::int64_t,int,std::int64_t>{-1,8,0},"plains:stone");
        const auto hiddenEdge=buildChunk(TerrainGenerator(flat,"plains:gentle","flat"),0,0,&edgeBlocks);
        check(exposedEdge.meshes.at("plains:stone").indices.size()==hiddenEdge.meshes.at("plains:stone").indices.size()+6,"Edited neighbor chunk did not cull shared block face");
        auto invalid=definition.json();invalid["generators"]["plains:gentle"]["surface"]="missing:block";rejects([&]{(void)WorldDefinition::parse(invalid);});
        invalid=definition.json();invalid["structures"]["plains:oak"][0][0]=200;rejects([&]{(void)WorldDefinition::parse(invalid);});
        invalid=definition.json();invalid["generators"]["plains:gentle"]["relief"]=1.5;rejects([&]{(void)WorldDefinition::parse(invalid);});
        const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto testRoot=std::filesystem::path("user/procedural-tests")/unique;WorldStorage storage(testRoot/"worlds");
        const auto authoredMods=testRoot/"mods";
        std::filesystem::create_directories(authoredMods);
        std::filesystem::copy("content/foundation",authoredMods/"foundation",std::filesystem::copy_options::recursive);
        std::filesystem::copy("content/plains",authoredMods/"plains",std::filesystem::copy_options::recursive);
        AuthoredBlock authoredBlock;authoredBlock.mod="painted";authoredBlock.pixels.fill(0x397ac3);publishBlock(authoredMods,authoredBlock);
        AuthoredBlock modeledBlock;modeledBlock.mod="modeled";modeledBlock.pixels.fill(0x957455);
        modeledBlock.model="assets/core/models/cube.glb";publishBlock(authoredMods,modeledBlock);
        AuthoredEquipment authoredWeapon;authoredWeapon.mod="combo";authoredWeapon.name="Espada de teste";
        authoredWeapon.model="foundation:sword";authoredWeapon.scale=.05;authoredWeapon.grantOnStart=true;
        authoredWeapon.ground={{"ground_1","attack","arc",15},{"ground_2","attack","box",31}};
        authoredWeapon.air={{"air_1","attack","circle",48}};
        publishEquipment(authoredMods,authoredWeapon);
        auto armorFixture=testRoot/"armor-source";
        std::filesystem::copy("tests/fixtures/studio/lab",armorFixture,std::filesystem::copy_options::recursive);
        const auto editStudioJson=[&](const char* name,const auto& edit) {
            const auto path=armorFixture/name;
            nlohmann::json document;
            {std::ifstream input(path,std::ios::binary);document=nlohmann::json::parse(input);}
            edit(document);std::ofstream output(path,std::ios::binary|std::ios::trunc);output<<document.dump();
        };
        editStudioJson("studio.manifest.json",[](auto& j) {
            std::erase_if(j.at("models").template get_ref<nlohmann::json::array_t&>(),[](const auto& model){return model.at("type")!="armor";});
        });
        editStudioJson("studio.events.json",[](auto& j) {j["clips"]=nlohmann::json::array();j["sounds"]=nlohmann::json::array();});
        editStudioJson("studio.player-animations.json",[](auto& j) {j["clips"]=nlohmann::json::array();});
        AuthoredEquipment armor;armor.mod="studio_demo";armor.name="Armadura de teste";armor.armorSlot="chest";
        armor.protection=8;armor.movementBonus=1.5;armor.healthBonus=10;armor.studioManifest=armorFixture/"studio.manifest.json";
        publishEquipment(authoredMods,armor);
        {
            VirtualFileSystem files;AssetManager loadedAssets(files,renderer,testRoot/"authored-cache");
            const auto published=ModLoader(files,loadedAssets).load(authoredMods);
            check(published.diagnostics.empty(),"Authored block texture or equipment mod did not load");
            const auto all=WorldDefinition::load(published.mods);
            check(all.blocks.contains("painted:block"),"Authored block is missing from the catalog");
            check(all.blocks.at("modeled:block").model=="modeled:block" &&
                !loadedAssets.model(AssetId("modeled:block"))->parts.empty(),"Modeled block did not register its geometry");
            auto modeledWorld=all;
            auto edited=std::make_shared<FiniteMap>(*all.maps.at("plains:shardbound"));
            edited->setBlock(9,18,9,"modeled:block",all);
            modeledWorld.maps["plains:shardbound"]=edited;
            const auto modeledChunk=buildChunk(TerrainGenerator(modeledWorld,"plains:shardbound","probe"),0,0);
            check(!modeledChunk.meshes.contains("modeled:block") && modeledChunk.models.size()==1 && modeledChunk.models[0].id=="modeled:block",
                "Modeled block produced a cube instead of a model placement");
            check(bool(loadedAssets.texture(AssetId("painted:block"))),"Authored block texture did not decode");
            const auto armorModel=loadedAssets.model(AssetId("studio_demo:armor"));
            check(armorModel->studio && armorModel->studio->events.at("clips").empty(),"Authored armor acquired animation clips");
        }
        {
            const TerrainGenerator authored(definition,"plains:shardbound","first");
            const TerrainGenerator another(definition,"plains:shardbound","second");
            check(authored.biome(8,8)=="plains:meadow" && authored.biome(32,8)=="plains:grove","Authored biomes not loaded");
            check(authored.height(8,8)>0 && authored.height(8,8)==another.height(8,8) && authored.height(-25,8)==0,"Authored map ignores its bounds or changes with seed");
            auto map=*authored.finiteMap();
            map.setBlock(8,12,8,"plains:stone",definition);
            check(map.block(12,10,12).empty(),"Registered structure appeared before placement");
            map.placeStructures(nlohmann::json::array({{"plains:oak",12,10,12,90}}),definition);
            check(map.block(12,10,12)=="plains:wood","Explicit structure placement was not applied");
            rejects([&]{map.placeStructures(nlohmann::json::array({{"plains:oak",56,10,12,0}}),definition);});
            const auto file=testRoot/"authored.json";map.save(file);
            check(FiniteMap::read(file,definition).block(8,12,8)=="plains:stone","Authored edit lost on disk");
            rejects([&]{map.setBlock(-25,12,8,"plains:stone",definition);});
            rejects([&]{map.setBlock(8,12,8,"missing:block",definition);});
            const auto world=storage.create("Authored","first","plains:shardbound",definition,versions);
            check(storage.read(world.id).definition.maps.at("plains:shardbound")->json()==authored.finiteMap()->json(),"Authored map snapshot was not preserved");
            const auto editedMap=FiniteMap::read(file,definition);
            check(editedMap.biome(32,8)=="plains:grove","Authored biome lost on disk");
            const auto doors=authored.finiteMap()->doors();
            check(doors.size()==2 && doors.front().from==FiniteMap::Spawn{45,8},"Door links not loaded");
            auto invalidRoom=authored.finiteMap()->json();
            invalidRoom["interiors"][0]["return"]=invalidRoom["interiors"][0]["entrance"];
            rejects([&]{(void)FiniteMap::parse(invalidRoom,definition);});
            World actors;PhysicsSystem bodies(actors);PlayerController controller(actors,bodies);
            const auto playerId=actors.create("door-test-player");
            actors.add(playerId,TransformComponent{{{45.5f,4.9f,8.5f}}});
            bodies.attachBox(playerId,{.35f,.85f,.35f},true);controller.possess(playerId,5);
            ChunkWorld room(renderer,assets,actors,bodies,controller,authored);
            SaveGame roomSave(storage.directory(world.id),actors,bodies,controller,assets);bindWorldPersistence(roomSave,room);
            check(room.useDoor(2),"Player could not enter interior");
            const auto arrival=doors.front().to;
            check(room.blockAt(arrival.x,2,arrival.z)=="plains:stone","Interior terrain was not loaded");
            const auto playerPosition=[&] {return actors.get<TransformComponent>(controller.entity()).local.position;};
            check(std::abs(room.originX()+playerPosition().x-(arrival.x+.5))<.01,"Wrong interior arrival position");
            check(room.setBlock(arrival.x+1,3,arrival.z,"plains:brick"),"Interior edit failed");
            roomSave.save("door-test");
            const auto exit=doors.back().from;
            bodies.teleport(controller.entity(),{float(exit.x-room.originX())+.5f,3.9f,float(exit.z-room.originZ())+.5f});
            check(room.useDoor(2),"Player could not exit interior");
            check(std::abs(room.originX()+playerPosition().x-43.5)<.01,"Wrong return position");
            check(roomSave.load("door-test").empty(),"Save inside an interior failed to load");
            check(std::abs(room.originX()+playerPosition().x-(arrival.x+.5))<.01 && room.blockAt(arrival.x+1,3,arrival.z)=="plains:brick",
                "Interior position or terrain edit was lost after loading");
        }
        {
            using Json=nlohmann::json;
            const auto registerPlacement=[](const std::filesystem::path& root,const std::string& id,int x) {
                atomicWrite(root/id/"mod.json",Json{{"id",id},{"name",id},{"version","1.0.0"},{"api","0.1"},
                    {"dependencies",{"plains"}}}.dump());
                atomicWrite(root/id/"world.json",Json{{"version",1},
                    {"map_placements",Json::array({{"plains:oak",x,12,12,0}})}}.dump());
            };
            registerPlacement(authoredMods,"orchard",12);
            const auto source=WorldDefinition::load(discoverMods(authoredMods).mods);
            auto edited=*source.maps.at("plains:shardbound");
            check(edited.block(12,12,12)=="plains:wood","Fixture placement was not applied");
            edited.setBlock(12,12,12,"",source);
            edited.setBlock(14,12,12,"painted:block",source);
            edited.addProp({"foundation:sword",13,12,12,0,1});
            const auto package=testRoot/L"export-\u6E2C\u8A66"/"castle";
            std::filesystem::create_directories(package.parent_path());
            const auto validator=std::filesystem::absolute("modcheck.exe");
            exportMapPackage(edited,authoredMods,package,validator);
            const auto exported=discoverMods(package);
            check(exported.diagnostics.empty() && exported.mods.size()==8,"Map export lost installed mods or dependencies");
            const auto reopened=WorldDefinition::load(exported.mods);
            check(reopened.maps.at("plains:shardbound")->json()==edited.json(),"Map export changed the authored terrain or props");
            registerPlacement(package,"more_trees",20);
            const auto combined=WorldDefinition::load(discoverMods(package).mods);
            check(combined.maps.at("plains:shardbound")->block(20,12,12)=="plains:wood","New placement mod was ignored");
            const auto reexport=testRoot/"castle-revised";
            exportMapPackage(*combined.maps.at("plains:shardbound"),package,reexport,validator);
            const auto revised=WorldDefinition::load(discoverMods(reexport).mods);
            check(revised.maps.at("plains:shardbound")->json()==combined.maps.at("plains:shardbound")->json(),
                "Exporting a previously replaced overworld created a conflicting override");
            edited.addProp({"painted:missing",15,12,12,0,1});
            const auto invalidPackage=testRoot/"invalid-map";
            rejects([&]{exportMapPackage(edited,authoredMods,invalidPackage,validator);});
            check(!std::filesystem::exists(invalidPackage),"Invalid map package was published");
            rejects([&]{exportMapPackage(edited,authoredMods,authoredMods/"nested",validator);});
        }
        auto record=storage.create("Planicie de teste","test-seed","plains:gentle",definition,versions);
        auto second=storage.create("Planicie de teste","","plains:gentle",definition,versions);
        check(record.id!=second.id && !second.seed.empty() && storage.list().size()==3,"World identity/random seed broken");
        check(storage.read(record.id).definition.json()==definition.json(),"World generator snapshot lost");
        rejects([&]{(void)storage.read("../outside");});rejects([&]{(void)storage.create("   ","seed","plains:gentle",definition,versions);});
        auto settingsFile=testRoot/"settings.json";GameSettings settings;settings.volume=.3f;settings.scale=.5f;settings.viewDistance=5;settings.vsync=false;settings.save(settingsFile);
        const auto restoredSettings=GameSettings::load(settingsFile);check(restoredSettings.volume==.3f && restoredSettings.viewDistance==5 && !restoredSettings.vsync,"Settings persistence failed");
        renderer.configure({false,.5f});check(renderer.renderWidth()==640 && renderer.renderHeight()==360,"Render scale setting has no effect");renderer.configure({false,1});
        auto proceduralOnly=definition;proceduralOnly.maps.clear();
        InputSystem menuInput;FrontEnd menu(renderer,window,menuInput,testRoot/"menu",proceduralOnly,versions);
        bool started=false;int saves=0;WorldRecord created;
        menu.startWorld=[&](const WorldRecord& r,bool resume){check(!resume,"New world unexpectedly resumes");created=r;started=true;};menu.saveWorld=[&]{++saves;};
        menu.update();check(menu.screen()==FrontEnd::Screen::Title,"First screen is not the title");renderer.setCanvasOverlay(true);renderer.render(Camera{},std::vector<RenderItem>{},menu.texture());capture(renderer,"user/procedural-preview/title.ppm");
        auto click=[&](int x,int y) { menuInput.beginFrame();Event e;e.type=EventType::MouseMove;e.x=float(x);e.y=float(y);menuInput.process(e);e.type=EventType::MouseDown;e.mouse=MouseButton::Left;menuInput.process(e);menu.update();e.type=EventType::MouseUp;menuInput.process(e);menuInput.beginFrame();menu.update(); };
        click(150,300);check(menu.screen()==FrontEnd::Screen::Worlds,"Play did not open world list");click(150,570);check(menu.screen()==FrontEnd::Screen::Create,"World creation screen missing");
        Event e;e.type=EventType::TextInput;e.text="Mundo á";menu.event(e);e.type=EventType::KeyDown;e.key=Key::Backspace;menu.event(e);e.type=EventType::TextInput;e.text="ç";menu.event(e);
        e.type=EventType::KeyDown;e.key=Key::Tab;menu.event(e);e.type=EventType::TextInput;e.text="seed-123";menu.event(e);menu.update();renderer.render(Camera{},std::vector<RenderItem>{},menu.texture());capture(renderer,"user/procedural-preview/create.ppm");
        click(150,570);check(started && created.name=="Mundo ç" && created.seed=="seed-123" && menu.playing(),"Name/seed/UTF-8 creation flow failed");
        e.type=EventType::KeyDown;e.key=Key::Escape;menu.event(e);menu.update();click(150,410);check(menu.screen()==FrontEnd::Screen::Settings,"Pause settings missing");
        menu.applySettings=[&](const GameSettings& s){renderer.configure({s.vsync,s.scale});};click(700,305);check(renderer.renderWidth()==960,"Settings UI did not apply scale");
        renderer.render(Camera{},std::vector<RenderItem>{},menu.texture());capture(renderer,"user/procedural-preview/settings.ppm");click(150,570);check(menu.screen()==FrontEnd::Screen::Pause,"Settings lost return screen");click(150,330);check(saves==1,"Save action missing");renderer.configure({false,1});

        // A save failure raised after the UI update must appear in that same frame.
        auto warningPixels=[&] {
            renderer.render(Camera{},std::vector<RenderItem>{},menu.texture());const auto pixels=renderer.readback();
            return std::vector<unsigned char>(pixels.begin()+size_t(610*1280*4),pixels.begin()+size_t(680*1280*4));
        };
        menu.update();const auto noWarning=warningPixels();click(150,250);check(menu.playing(),"Resume failed");
        menu.error("Disco cheio: checkpoint nao foi salvo");
        check(menu.screen()==FrontEnd::Screen::Pause,"Save failure did not pause gameplay");
        const auto warning=warningPixels();check(warning!=noWarning,"Save warning not rendered immediately");
        click(150,410);click(150,570);check(warningPixels()==warning,"Settings navigation erased save failure");
        bool closed=false;menu.closeWorld=[&]{closed=true;};
        menu.saveWorld=[] { throw std::runtime_error("Disco cheio: checkpoint nao foi salvo"); };
        click(150,490);check(!closed && menu.activeWorld() && menu.screen()==FrontEnd::Screen::Pause,"Failed save closed the active world");
        check(warningPixels()==warning,"Failed retry erased save warning");
        menu.saveWorld=[&]{++saves;};click(150,330);check(saves==2 && warningPixels()==noWarning,"Successful retry did not clear save warning");
        click(150,250);menu.error("Disco cheio: checkpoint nao foi salvo");click(150,250);
        check(menu.playing(),"Continue without saving did not acknowledge failure");
        menu.event(e);menu.update();check(warningPixels()!=warning,"Acknowledged warning reappeared");

        // Keep physics support for actors beyond the player's 3x3 ring and visual range.
        // Use a flat generator and no recovery scripts: a missing floor cannot be hidden by teleporting.
        {
            World actors;PhysicsSystem bodies(actors);PlayerController controller(actors,bodies);
            auto actor=[&](Vec3 position) { const auto id=actors.create("collision_probe");actors.add(id,TransformComponent{{position}});bodies.attachBox(id,{.35f,.85f,.35f},true);return id; };
            const auto hero=actor({-43,5,-36}),foe=actor({-43,5,-39});controller.possess(hero,5);
            ChunkWorld ground(renderer,assets,actors,bodies,controller,TerrainGenerator(flat,"plains:gentle","flat"));
            ground.update(2,true);
            bodies.teleport(hero,{-15.9f,5,-39});ground.update(2,true);
            for(int i=0;i<120;++i) { ground.update(2);bodies.step(1.f/60); }
            check(bodies.grounded(foe),"Actor lost terrain collision outside player's physics ring");
            bodies.teleport(hero,{8.5f,5,8.5f});ground.update(2,true);
            check(ground.loadedChunks()>25,"Occupied chunk outside visual range was discarded");
            for(int i=0;i<120;++i) { ground.update(2);bodies.step(1.f/60); }
            check(bodies.grounded(foe),"Actor fell through terrain outside visual range");
            bodies.teleport(foe,{-48.05f,5,-16.05f});ground.update(2);
            for(int i=0;i<120;++i) { ground.update(2);bodies.step(1.f/60); }
            check(bodies.grounded(foe),"Actor at negative chunk seam lost support");
            bodies.teleport(hero,{140,5,8.5f});ground.update(2,true);
            for(int i=0;i<120;++i) { ground.update(2);bodies.step(1.f/60); }
            check(bodies.grounded(foe),"Floating origin lost distant actor collision");
            actors.destroy(foe);ground.update(2,true);bodies.step(1.f/60);
            check(ground.loadedChunks()==25 && actors.entities().size()==10,"Unoccupied chunks or physics bodies leaked after despawn");
        }

        World world;PhysicsSystem physics(world);PlayerController player(world,physics);InputSystem input;AudioSystem audio(true);script::Runtime runtime;
        GameUi ui(runtime,renderer);bindRuntimeApi(runtime,world,assets);bindGameplayApi(runtime,world,physics,player,input,audio,assets);StudioRuntime studio(world,renderer,audio);bindStudioApi(runtime,world,studio);
        Components components;SaveGame savesystem(storage.directory(record.id),world,physics,player,assets);ExtensionBridge extensions(runtime,world,components,savesystem);
        ChunkWorld terrain(renderer,assets,world,physics,player,TerrainGenerator(definition,record.generator,record.seed));bindWorldApi(runtime,terrain,record);
        bindWorldPersistence(savesystem,terrain);
        savesystem.setMods(versions);savesystem.includeEntity=[&](EntityId id){return !terrain.transient(id);};savesystem.definitionAvailable=[&](const std::string& id){return runtime.registry().contains("Entities",id);};
        for(const auto& m:report.mods) { std::vector<std::string> dependencies;for(const auto& d:m.dependencies)dependencies.push_back(d.id);runtime.load(m.id,m.root,m.entry,dependencies); }
        auto emit=[&](const std::string& name,std::vector<script::Value> args=std::vector<script::Value>{}) { auto errors=runtime.emit(name,args);if(!errors.empty())throw DiagnosticError(errors.front()); };
        emit("on_start");emit("on_world_start");terrain.update(2,true);emit("on_ready");
        runtime.execute("foundation","@Globals\nterrain_sample = terrain_info(8.5, 8.5)\nbiome_sample = terrain_biome(8.5, 8.5)\nblock_sample = terrain_block(8.5, terrain_sample.height - 1, 8.5)\nnoise_sample = world_noise(8, 8, 64, 17)\n");
        check(runtime.global("foundation","biome_sample").string()==a.biome(8,8) && runtime.global("foundation","block_sample").string()==a.column(8,8).blocks.back(),"Terrain query APIs disagree with generated world");
        rejects([&]{runtime.execute("foundation","world_noise(0, 0, 0, 17)");});
        const int editHeight=terrain.localHeight(12,12);
        const std::string addStone="@Globals\nedit_changed = terrain_set_block(12, " + std::to_string(editHeight+3) + ", 12, \"plains:stone\")\n";
        runtime.execute("foundation",addStone);
        check(runtime.global("foundation","edit_changed").boolean() && terrain.blockAt(12,editHeight+3,12)=="plains:stone","Script world edit not applied");
        runtime.execute("foundation",addStone);check(!runtime.global("foundation","edit_changed").boolean(),"Identical world edit was not idempotent");
        runtime.execute("foundation","@Globals\nremoved_block = terrain_set_block(13, terrain_height(13, 13) - 1, 13, \"\")\n");
        check(runtime.global("foundation","removed_block").boolean() && terrain.blockAt(13,terrain.localHeight(13,13)-1,13).empty(),"Terrain removal did not override original world");
        rejects([&]{runtime.execute("foundation","terrain_set_block(12, -1, 12, \"plains:stone\")");});
        rejects([&]{runtime.execute("foundation","terrain_set_block(12, 6, 12, \"missing:block\")");});
        check(terrain.blockEdits().size()==2,"Invalid world edits mutated terrain");
        check(bool(player.entity()) && itemCount(world,player.entity(),AssetId("foundation:IronSword"))==1,"Mod did not supply player/equipment");
        const auto comboManifest=readManifest(authoredMods/"combo");
        runtime.load(comboManifest.id,comboManifest.root,comboManifest.entry,{"foundation"});
        runtime.execute("foundation",R"(
give_item(hero, "combo:Equipment", 1)
equip("combo:Equipment")
teleport(hero, vec3(8.5, 30, 8.5))
begin_attack()
@Globals
air_combo_damage = attack_profile.damage
)");
        check(runtime.global("foundation","air_combo_damage").number()==48,"Authored aerial strike was not selected");
        physics.teleport(player.entity(),{8.5f,float(terrain.localHeight(8.5f,8.5f))+physics.description(player.entity()).halfExtent.y+.05f,8.5f});
        terrain.update(2,true);
        for(int i=0;i<120 && !physics.grounded(player.entity());++i) physics.step(1.f/60);
        check(physics.grounded(player.entity()),"Combo test player did not land");
        runtime.execute("foundation",R"(
begin_attack()
begin_attack()
@Globals
second_combo_damage = attack_profile.damage
)");
        check(runtime.global("foundation","second_combo_damage").number()==31,"Second authored combo strike was not selected");
        runtime.execute("foundation",R"(
@Globals
arc_target = spawn(Scout, vec3(hero.position.x + 0.8, hero.position.y, hero.position.z + 1))
add_box(arc_target, vec3(0.35, 0.85, 0.35), true)
add_component(arc_target, Brain)
set_health(arc_target, 100)
facing = vec3(0, 0, 1)
combo_time = 0
begin_attack()
attack_profile.width = 1
resolve_hit()
arc_narrow_health = health(arc_target)
attack_profile.width = 2
resolve_hit()
arc_wide_health = health(arc_target)
destroy(arc_target)
)");
        check(runtime.global("foundation","arc_narrow_health").number()==100 &&
            runtime.global("foundation","arc_wide_health").number()==85,"Arc width did not change which targets were hit");
        runtime.execute("foundation","equip(\"foundation:IronSword\")\n");
        {
            const auto manifest=readManifest(authoredMods/"studio_demo");
            vfs.mount(manifest.id,manifest.root);
            assets.registerStudioPackage(AssetId("studio_demo:bow"),"studio/studio.manifest.json");
            runtime.load(manifest.id,manifest.root,manifest.entry,{"foundation"});
            runtime.execute("foundation",R"(
@Globals
armor_speed_before = get_attribute(hero, "foundation:movement_speed")
armor_health_before = get_attribute(hero, "foundation:max_health")
give_item(hero, "studio_demo:Equipment", 1)
inventory_open = true
draw_interface()
inventory_open = false
equip("studio_demo:Equipment")
armor_speed_after = get_attribute(hero, "foundation:movement_speed")
armor_health_after = get_attribute(hero, "foundation:max_health")
equip_instance(hero, "chest", null)
)");
            check(runtime.global("foundation","armor_speed_after").number()==runtime.global("foundation","armor_speed_before").number()+1.5 &&
                runtime.global("foundation","armor_health_after").number()==runtime.global("foundation","armor_health_before").number()+10,
                "Authored armor bonuses did not reach player attributes");
        }
        runtime.execute("foundation",R"(@Globals
qa_system_ticks = 0
func qa_system_tick(delta):
    qa_system_ticks = qa_system_ticks + 1
register_system("foundation:qa", "fixed", qa_system_tick, 5)
)");
        check(runtime.global("foundation","qa_system_ticks").number()==0,"Named mod system executed before its phase");
        rejects([&]{runtime.execute("foundation",R"(register_system("other:qa", "fixed", qa_system_tick))");});
        rejects([&]{runtime.execute("foundation",R"(register_system("foundation:qa", "fixed", qa_system_tick))");});
        runtime.execute("foundation",R"(@Globals
initial_equipped = equipped_item(hero, "weapon")
nearby_actors = entities_near(hero, 2, "foundation:Actor")
)");
        check(runtime.global("foundation","initial_equipped").string()=="foundation:IronSword","Foundation equipped weapon not synchronized");
        check(std::get<std::shared_ptr<script::List>>(runtime.global("foundation","nearby_actors").data)->values.size()==1,"Spatial query incorrect");
        rejects([&]{runtime.execute("foundation",R"(equip_slot(hero, "artifact", "foundation:HeavyBlade"))");});
        runtime.execute("foundation",R"(@Globals
give_item(hero, "foundation:ForestSword", 1)
equip_changed = equip_slot(hero, "artifact", "foundation:ForestSword")
equipped_artifact = equipped_item(hero, "artifact")
equipped_count = list_count(equipment_slots(hero))
)");
        check(runtime.global("foundation","equip_changed").boolean() && runtime.global("foundation","equipped_artifact").string()=="foundation:ForestSword" && runtime.global("foundation","equipped_count").number()==2,"Equipment slot API failed");
        rejects([&]{runtime.execute("foundation",R"(equip_slot(hero, "artifact", "foundation:IronSword"))");});
        check(!takeItem(world,player.entity(),AssetId("foundation:ForestSword"),1),"Inventory consumed a reserved equipment copy");
        runtime.execute("foundation",R"(@Globals
hit_lower = record()
hit_lower.id = "lower"
hit_lower.x = 20
hit_lower.y = 20
hit_lower.w = 100
hit_lower.h = 50
hit_upper = record()
hit_upper.id = "upper"
hit_upper.x = 30
hit_upper.y = 25
hit_upper.w = 40
hit_upper.h = 25
hit_regions = [hit_lower, hit_upper]
hit_pointer = record()
hit_pointer.valid = true
hit_pointer.x = 35
hit_pointer.y = 30
hit_selection = canvas_hit_test(hit_regions, hit_pointer)
)");
        check(runtime.global("foundation","hit_selection").string()=="upper","Canvas hit testing ignored painter order");
        runtime.execute("foundation",R"(@Globals
hit_upper.enabled = false
hit_selection = canvas_hit_test(hit_regions, hit_pointer)
)");
        check(runtime.global("foundation","hit_selection").string()=="lower","Disabled canvas hit region intercepted pointer");
        runtime.execute("foundation",R"(@Globals
hit_pointer.valid = false
hit_selection = canvas_hit_test(hit_regions, hit_pointer)
)");
        check(std::holds_alternative<std::monostate>(runtime.global("foundation","hit_selection").data),"Canvas hit testing ignored pointer validity");
        for(int i=0;i<120;++i) { emit("on_fixed_update",{script::Value(1.0/60)});physics.step(1.f/60);emit("on_update",{script::Value(1.0/60)});studio.update(1.0/60); }
        runtime.execute("foundation",R"(@Globals
foundation_profile = mod_profile("foundation")
)");
        check(runtime.global("foundation","foundation_profile").member("event_calls",{}).number()>120,"Per-mod profiler did not count callbacks");
        check(runtime.global("foundation","qa_system_ticks").number()==120,"Named mod system did not execute exactly once per fixed tick");
        runtime.execute("foundation",R"(@Globals
system_enabled("foundation:qa", false)
)");
        emit("on_fixed_update",{script::Value(1.0/60)});
        check(runtime.global("foundation","qa_system_ticks").number()==120,"Disabled mod system still ran");
        runtime.execute("foundation",R"(@Globals
func qa_replacement(delta):
    qa_system_ticks = qa_system_ticks + 5
replace_system("foundation:qa", qa_replacement)
system_enabled("foundation:qa", true)
)");
        emit("on_fixed_update",{script::Value(1.0/60)});
        check(runtime.global("foundation","qa_system_ticks").number()==125,"Mod system replacement failed");
        const auto switcher=runtime.subscribe("on_fixed_update",script::native([&](const auto&,const auto&) {
            runtime.execute("foundation",R"(system_enabled("foundation:qa", false))");
            return script::Value{};
        }),false,"foundation");
        emit("on_fixed_update",{script::Value(1.0/60)});
        check(runtime.global("foundation","qa_system_ticks").number()==130,"System changed during dispatch instead of next cycle");
        check(runtime.unsubscribe(switcher),"Cannot unsubscribe mod system test event");
        emit("on_fixed_update",{script::Value(1.0/60)});
        check(runtime.global("foundation","qa_system_ticks").number()==130,"Disabled mod system ran on next cycle");
        runtime.execute("foundation",R"(@Globals
qa_removed = unregister_system("foundation:qa")
)");
        check(runtime.global("foundation","qa_removed").boolean(),"Mod system could not unregister");
        check(physics.grounded(player.entity()),"Player fell through procedural terrain");
        auto render=[&]{auto items=terrain.renderItems();auto actors=collectRenderItems(world);items.insert(items.end(),actors.begin(),actors.end());renderer.render(player.camera(),items,ui.texture());};
        player.updateCamera(0);render();check(renderer.stats().triangles>1000,"Terrain not rendered");const auto pixels=renderer.readback();size_t green=0;for(size_t i=0;i<pixels.size();i+=4)green+=pixels[i+1]>pixels[i]+8 && pixels[i+1]>pixels[i+2]+8;check(green>10000,"Voxel ground not visible on GPU");capture(renderer,"user/procedural-preview/plains.ppm");
        // Real combat/drop path from the default mod, then preserve it in the world checkpoint.
        runtime.execute("foundation",R"(@Globals
testfoe = spawn(Scout, vec3(hero.position.x + 1, hero.position.y, hero.position.z))
add_box(testfoe, vec3(0.35, 0.85, 0.35), true)
set_health(testfoe, 40)
testlook = spawn(ScoutVisual, vec3(0, -0.85, 0))
set_parent(testlook, testfoe)
add_component(testfoe, Brain)
testbrain = get_component(testfoe, Brain)
testbrain.key = "test-encounter"
testbrain.visual = entity_key(testlook)
set_component(testfoe, Brain, testbrain)
damage(testfoe, 100)
combat_tick(0)
)");
        emit("on_fixed_update",{script::Value(1.0/60)});check(itemCount(world,player.entity(),AssetId("foundation:ForestSword"))>=1,"Enemy drop not collected");
        const auto lootCount=itemCount(world,player.entity(),AssetId("foundation:ForestSword"));
        const auto lootOriginX=terrain.originX(),lootOriginZ=terrain.originZ();const auto lootPosition=world.get<TransformComponent>(player.entity()).local.position;
        runtime.execute("foundation",R"(@Globals
farloot = spawn(Drop, vec3(hero.position.x + 20, hero.position.y, hero.position.z))
add_component(farloot, GroundDrop)
farlootdata = get_component(farloot, GroundDrop)
farlootdata.item = "foundation:ForestSword"
set_component(farloot, GroundDrop, farlootdata)
)");
        physics.teleport(player.entity(),{220,6,220});terrain.update(2,true);runtime.execute("foundation","update_encounters()");
        const auto stored=runtime.global("foundation","state").member("stored_drops",{});check(!std::get<std::shared_ptr<script::List>>(stored.data)->values.empty(),"Distant loot not unloaded to mod data");
        const auto returnX=float(lootOriginX-terrain.originX())+lootPosition.x+20,returnZ=float(lootOriginZ-terrain.originZ())+lootPosition.z;
        physics.teleport(player.entity(),{returnX,float(terrain.localHeight(returnX,returnZ))+1,returnZ});terrain.update(2,true);runtime.execute("foundation","update_encounters()");emit("on_fixed_update",{script::Value(1.0/60)});
        check(itemCount(world,player.entity(),AssetId("foundation:ForestSword"))>lootCount,"Distant loot lost on return");
        const auto initialEntities=world.entities().size();
        for(int i=1;i<=16;++i) { const float x=float(i*130),z=float(-i*146);physics.teleport(player.entity(),{x,float(terrain.localHeight(x,z))+1,z});terrain.update(2,true);runtime.execute("foundation","update_encounters()");terrain.update(2,true);check(terrain.loadedChunks()<=25 && world.entities().size()<2000,"Chunk streaming leaks resources"); }
        check(terrain.originX()!=0 && terrain.originZ()<0,"Floating origin did not move");
        check(initialEntities<2000,"Excessive initial physics objects");
        const auto ox=terrain.originX(),oz=terrain.originZ();const auto savedPosition=world.get<TransformComponent>(player.entity()).local.position;
        runtime.execute("foundation",R"(@Globals
equip_slot(hero, "weapon", null)
instance_options = record()
instance_options.maximum = 50
instance_options.stats = record()
instance_options.affixes = record()
qa_weapon_instance = create_item_instance(hero, "foundation:IronSword", instance_options)
equip_instance(hero, "weapon", qa_weapon_instance)
item_durability(hero, qa_weapon_instance, 0)
)");
        emit("on_before_save");savesystem.save("checkpoint");
        const auto imported=importMapFromSave(storage.directory(record.id)/"checkpoint.json",{11,11,4,4});
        check(imported.block(12,editHeight+3,12)=="plains:stone" &&
            imported.block(13,terrain.generator().height(13,13)-1,13).empty(),"Save import lost placed or removed voxels");
        check(imported.block(12,0,12)==terrain.blockAt(12,0,12) && imported.block(10,0,12).empty(),"Save import lost base terrain or escaped the crop");
        auto saved=nlohmann::json::parse(encodeData(SaveGame::inspect(storage.directory(record.id)/"checkpoint.json")));check(saved["entities"].size()<100,"Regenerable terrain leaked into save");
        physics.teleport(player.entity(),{0,30,0});auto diagnostics=savesystem.load("checkpoint");check(diagnostics.empty(),"World checkpoint failed to load");emit("on_loaded");terrain.update(2,true);emit("on_ready");
        check(world.get<TransformComponent>(player.entity()).local.position.x==savedPosition.x && terrain.originX()==ox && terrain.originZ()==oz,"Distant position/origin not restored");
        check(terrain.blockEdits().size()==2 && terrain.blockAt(12,editHeight+3,12)=="plains:stone" && terrain.blockAt(13,terrain.generator().height(13,13)-1,13).empty(),"Block edits lost after checkpoint reload");check(itemCount(world,player.entity(),AssetId("foundation:ForestSword"))>=1,"Inventory lost on reload");
        runtime.execute("foundation",R"(@Globals
restored_equipment = equipped_item(hero, "artifact")
restored_instance = equipped_instance(hero, "weapon")
)");
        check(runtime.global("foundation","restored_instance").string()==runtime.global("foundation","qa_weapon_instance").string(),"Foundation did not resume broken unique weapon");
        check(runtime.global("foundation","restored_equipment").string()=="foundation:ForestSword","Equipment slot not persisted");
        const auto defeated=runtime.global("foundation","state").member("defeated",{});check(std::get<std::shared_ptr<script::List>>(defeated.data)->values.size()>=1,"Defeated encounter history lost");
        auto corrupt=saved;corrupt["state"]["core"]["origin_x"]="17";writeJsonAtomic(storage.directory(record.id)/"corrupt.json",corrupt);
        auto before=player.entity();rejects([&]{(void)savesystem.load("corrupt");});check(player.entity()==before && terrain.originX()==ox,"Rejected world load changed live state");
        corrupt=saved;corrupt["state"]["core"]["blocks"][0][3]="missing:block";writeJsonAtomic(storage.directory(record.id)/"bad-block.json",corrupt);
        rejects([&]{(void)savesystem.load("bad-block");});check(terrain.blockEdits().size()==2 && player.entity()==before,"Invalid saved block edit modified world");
        runtime.execute("foundation","@Globals\ncoordinate_text = number_text(100000000001)\n");check(runtime.global("foundation","coordinate_text").string()=="100000000001","Global coordinate text loses precision");
        for(int i=0;i<120;++i) physics.step(1.f/60);check(physics.grounded(player.entity()),"Reload did not restore chunk collision");
        render();capture(renderer,"user/procedural-preview/distant.ppm");
        runtime.execute("foundation",R"(@Globals
set_attribute(hero, "foundation:movement_speed", 9)
set_attribute(hero, "foundation:max_health", 150)
set_attribute(hero, "foundation:jump_speed", 12)
developer_probe = spawn(Scout, vec3(hero.position.x + 3, hero.position.y, hero.position.z))
developer_probe.name = "developer_probe"
)");
        check(player.speed()==9 && world.get<HealthComponent>(player.entity()).maximum==150,"Registered player attributes did not reach gameplay");
        emit("on_developer_spawn",{runtime.global("foundation","developer_probe")});const auto probe=world.find("developer_probe");
        check(physics.contains(probe) && world.has<HealthComponent>(probe) && world.custom(probe).contains("foundation:Brain"),"Developer-spawned enemy lacks physics, health or AI");
        runtime.execute("foundation",R"(@Globals
probe_visual = get_entity_by_key(get_component(developer_probe, Brain).visual)
destroy(probe_visual)
destroy(developer_probe)
)");
        emit("on_before_save");savesystem.save("attributes");runtime.execute("foundation","set_attribute(hero, \"foundation:movement_speed\", 3)");
        check(savesystem.load("attributes").empty(),"Player attribute save did not load");emit("on_loaded");
        check(player.speed()==9 && world.get<HealthComponent>(player.entity()).maximum==150,"Attribute effects did not survive reload");
        std::cout<<"PASS procedural: seed, negative/distant coordinates, mesh, content validation, menu/text/settings, mod combat/loot, streaming, floating origin, attributes, developer spawns and world save/load\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL procedural: "<<e.what()<<'\n';return 1;}
}
