#include "core/AtomicFile.h"
#include "core/IoExecutor.h"
#include "assets/CoreAssets.h"
#include "assets/Fingerprint.h"
#include "mods/ModLoader.h"
#include "scripting/ExtensionBridge.h"
#include "world/WorldBridge.h"
#include "game/FrontEnd.h"
#include "platform/WindowSystem.h"
#include <Windows.h>
#include <Psapi.h>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
using namespace game;
using Json=nlohmann::json;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* message) {bool error=false;try {action();}catch(const std::exception&) {error=true;}check(error,message);}
void write(const std::filesystem::path& file,const std::string& text) {std::filesystem::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out<<text;out.close();check(bool(out),"Cannot write fixture");}
std::string read(const std::filesystem::path& file) {std::ifstream input(file,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};}
size_t memory() {PROCESS_MEMORY_COUNTERS_EX value{};check(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&value),sizeof(value)),"Memory measurement failed");return value.PrivateUsage;}
void crashWriter(const std::filesystem::path& file,const std::string& stage) {
    wchar_t executable[32768];check(GetModuleFileNameW(nullptr,executable,32768)!=0,"Executable path unavailable");
    std::wstring command=L"\""+std::wstring(executable)+L"\" --interrupt \""+std::filesystem::absolute(file).wstring()+L"\" "+std::wstring(stage.begin(),stage.end());
    STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION process{};
    check(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&start,&process),"Cannot start interrupted writer");
    const auto wait=WaitForSingleObject(process.hProcess,10000);if(wait!=WAIT_OBJECT_0) TerminateProcess(process.hProcess,99);
    DWORD status=0;GetExitCodeProcess(process.hProcess,&status);CloseHandle(process.hThread);CloseHandle(process.hProcess);check(wait==WAIT_OBJECT_0 && status==74,"Writer did not terminate at requested boundary");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==4 && std::string(argv[1])=="--interrupt") {
            const auto boundary=std::string(argv[3])=="flushed"?WriteStage::Flushed:WriteStage::Published;
            atomicWrite(argv[2],"new",true,[boundary](WriteStage stage){if(stage==boundary) TerminateProcess(GetCurrentProcess(),74);});return 1;
        }
        std::cout<<std::unitbuf;
        const int iterations=argc==3 && std::string(argv[1])=="--soak"?std::clamp(std::stoi(argv[2]),1,100000):64;
        const auto root=std::filesystem::path("user/robustness")/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        // Windows may briefly deny directory renames while a scanner holds a handle.
        const auto locked=root/"locked",renamed=root/"renamed";std::filesystem::create_directories(locked);
        const auto handle=CreateFileW(locked.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        check(handle!=INVALID_HANDLE_VALUE,"Cannot hold directory sharing lock");
        std::jthread unlock([handle]{std::this_thread::sleep_for(std::chrono::milliseconds(100));CloseHandle(handle);});renameDirectory(locked,renamed);unlock.join();
        check(std::filesystem::is_directory(renamed) && !std::filesystem::exists(locked),"Directory rename did not survive a transient sharing lock");
        std::filesystem::create_directories(locked);write(renamed/"preserved","data");rejects([&]{renameDirectory(locked,renamed);},"Directory rename overwrote an existing destination");check(read(renamed/"preserved")=="data" && std::filesystem::exists(locked),"Rejected rename changed either directory");
        // Actual process termination on both sides of atomic publication.
        const auto interrupted=root/"interrupted.json";atomicWrite(interrupted,"old");crashWriter(interrupted,"flushed");check(read(interrupted)=="old","Unpublished write replaced live file");
        crashWriter(interrupted,"published");check(read(interrupted)=="new" && read(interrupted.wstring()+L".bak")=="old","Published write or previous generation was lost");
        WindowSystem window({"Runtime robustness",320,240,true});Renderer renderer(window.nativeHandle(),320,240,"assets/core/shaders/forward.hlsl",{false,1});
        VirtualFileSystem vfs;vfs.mount("core","assets/core");AssetManager assets(vfs,renderer,root/"cache");registerCoreModels(assets);
        // A valid first asset followed by a broken one must publish neither the asset nor its mount.
        auto package=[&](const std::string& id,const Json& registry,const Json& extra=Json::object()) {
            const auto folder=root/"mods"/id;Json manifest={{"id",id},{"name",id},{"api","0.1"},{"version","1.0.0"}};manifest.update(extra);
            write(folder/"mod.json",manifest.dump());write(folder/"assets.json",registry.dump());
            std::filesystem::copy_file("assets/core/textures/checker.png",folder/"checker.png",std::filesystem::copy_options::overwrite_existing);
        };
        package("broken",{{"textures",{{"broken:a_valid",{{"source","checker.png"}}},{"broken:z_missing",{{"source","absent.png"}}}}}});
        package("dependent",{{"textures",{{"dependent:valid",{{"source","checker.png"}}}}}},{{"dependencies",{"broken"}}});
        package("optional",{{"textures",{{"optional:valid",{{"source","checker.png"}}}}}},{{"optional_dependencies",{"broken"}}});
        package("audio_bad",{{"sounds",{{"audio_bad:bad",{{"source","broken.wav"}}}}}});write(root/"mods/audio_bad/broken.wav","not an audio stream");
        package("syntax_bad",{{"textures",{{"syntax_bad:valid",{{"source","checker.png"}}}}}},{{"entry","main.sl"}});write(root/"mods/syntax_bad/main.sl","func broken(\n");
        for(int i=0;i<24;++i) {const auto id="independent_"+std::to_string(i);package(id,{{"textures",{{id+":valid",{{"source","checker.png"}}}}}},{{"entry","main.sl"}});
            write(root/"mods"/id/"main.sl","@Globals\nticks = 0\nupdates = 0\nfunc update_tick(delta):\n    ticks = ticks + 1\n    updates = updates + 1\n@Events\non_start():\n    register_system(\""+id+":tick\", \"update\", update_tick)\non_before_save():\n    state = record()\n    state.ticks = ticks\n    set_save_state(state)\non_loaded():\n    state = get_save_state()\n    ticks = state.ticks\n");}
        ModLoader loader(vfs,assets);const auto report=loader.load(root/"mods");check(report.mods.size()==25 && report.registeredAssets==25,"Package isolation/dependency propagation failed");
        rejects([&]{assets.texture(AssetId("broken:a_valid"));},"Failed package leaked an asset");rejects([&]{vfs.resolve("broken","checker.png");},"Failed package leaked its mount");
        rejects([&]{assets.texture(AssetId("dependent:valid"));},"Required dependent initialized after dependency failed");check(bool(assets.texture(AssetId("optional:valid"))),"Optional dependency failure blocked independent content");
        const auto cancel=root/"cancel/cancelled";write(cancel/"mod.json",R"({"id":"cancelled","name":"Cancellation","api":"0.1","version":"1.0.0"})");
        write(cancel/"assets.json",R"({"models":{"cancelled:a":{"source":"cube.glb"},"cancelled:b":{"source":"cube.glb"}}})");std::filesystem::copy_file("assets/core/models/cube.glb",cancel/"cube.glb");
        rejects([&]{loader.load(root/"cancel",[](auto,size_t done,size_t){if(done==1) throw PreparationCancelled();});},"Cancellation did not interrupt preparation");
        rejects([&]{assets.model(AssetId("cancelled:a"));},"Cancelled transaction leaked a model");check(loader.load(root/"cancel").registeredAssets==2,"Cancelled package could not be retried");
        const auto content=loader.load("content");check(content.diagnostics.empty(),"Default content failed");const auto definition=WorldDefinition::load(content.mods);
        // Cold-page windows own immutable data while live edits continue.
        RegionEdits edits(root/"pages");for(int i=0;i<128;++i) edits.replace({i,0},{{{i*16,4,0},"plains:stone"}});
        IoExecutor::instance().submit([]{}).get();check(edits.pendingWrites()==0,"Completed terrain writes retained evicted voxel payloads");
        auto old=edits.window({0,0});edits.replace({0,0},{{{0,4,0},"plains:dirt"}});check(old.get(0,4,0)=="plains:stone","Queued chunk snapshot changed with live edits");
        const auto blockedPages=root/"blocked-pages";write(blockedPages,"blocked");RegionEdits retry(blockedPages);
        for(int i=0;i<80;++i) retry.replace({i,0},{{{i*16,4,0},"plains:stone"}});
        rejects([&]{retry.checkpoint();},"Failed terrain write was hidden");std::filesystem::remove(blockedPages);std::filesystem::create_directories(blockedPages);
        (void)retry.checkpoint();check(retry.all().size()==80,"Retry after storage failure lost evicted edits");
        const auto compact=edits.checkpoint();check(compact.at("version")==2 && compact.dump().size()<256,"Checkpoint retained an inline page index");
        check(RegionEdits::restore(root/"pages",compact,definition).all().size()==128,"External terrain index lost pages");
        auto invalid=compact;invalid["index"]="../outside.index";rejects([&]{RegionEdits::restore(root/"pages",invalid,definition);},"Terrain index traversal accepted");
        // Immutable save capture proceeds without waiting for deliberately blocked disk work.
        World world;PhysicsSystem physics(world);PlayerController player(world,physics);SaveGame saves(root/"saves",world,physics,player,assets);
        saves.setState("state",Data{Data::Object{{"counter",Data{1.0}}}});
        auto gate=std::make_shared<std::promise<void>>();auto ready=gate->get_future().share();auto blocked=IoExecutor::instance().submit([ready]{ready.wait();});
        saves.requestSave("async");saves.setState("state",Data{Data::Object{{"counter",Data{2.0}}}});check(!saves.finishSave(),"Async save waited for disk on main thread");
        gate->set_value();blocked.get();saves.finishSave(true);check(saves.load("async").empty() && std::get<double>(std::get<Data::Object>(saves.state("state").value).at("counter").value)==1,"Save capture was not immutable");
        const auto parent=world.create("parent"),child=world.create("child");world.setParent(child,parent);saves.includeEntity=[parent](EntityId entity){return entity!=parent;};
        rejects([&]{saves.save("async");},"Save published an excluded parent reference");saves.includeEntity={};world.destroy(child);world.destroy(parent);
        const auto failRoot=root/"file-as-directory";write(failRoot,"not a directory");SaveGame failed(failRoot,world,physics,player,assets);failed.requestSave("failed");rejects([&]{failed.finishSave(true);},"Background write error was swallowed");
        // Explicit version interval plus isolated schema migration; no live host API in the migration runtime.
        World migrationWorld;PhysicsSystem migrationPhysics(migrationWorld);PlayerController migrationPlayer(migrationWorld,migrationPhysics);
        SaveGame migrations(root/"migration-saves",migrationWorld,migrationPhysics,migrationPlayer,assets);migrations.setMods({{"upgrade","1.0.0"}});migrations.setState("upgrade",Data{Data::Object{{"count",Data{3.0}}}});
        auto subject=migrationWorld.create("before");migrationWorld.get<IdentityComponent>(subject).owner="upgrade";migrations.save("old");
        ModManifest policy;policy.id="upgrade";policy.version=Version::parse("1.1.0");policy.saveSchema=2;policy.compatibleFrom="1.0.0";
        check(policy.acceptsSave("1.0.0",1) && !policy.acceptsSave("1.2.0",2) && !policy.acceptsSave("1.0.0",3),"Save version policy accepted downgrade/future schema");
        migrations.setPolicies({policy});migrations.setMods({{"upgrade","1.1.0"}});rejects([&]{migrations.load("old");},"Missing schema migration accepted");check(migrationWorld.alive(subject),"Missing migration changed live world");
        const auto migrationMod=root/"migration-mod";
        write(migrationMod/"main.sl","register_save_migration(1, \"schema1.sl\")\n");
        write(migrationMod/"schema1.sl","func migrate(bundle):\n    bundle.state.count = bundle.state.count + 1\n    entity = list_at(bundle.entities, 0)\n    entity.name = \"after\"\n    return bundle\n");
        script::Runtime runtime;Components components;bindRuntimeApi(runtime,migrationWorld,assets);ExtensionBridge bridge(runtime,migrationWorld,components,migrations);runtime.load("upgrade",migrationMod,"main.sl");
        const auto original=fingerprint(root/"migration-saves/old.json","migration");(void)migrations.load("old");check(migrationWorld.find("after") && std::get<double>(std::get<Data::Object>(migrations.state("upgrade").value).at("count").value)==4,"Isolated migration did not transform owned data");
        check(fingerprint(root/"migration-saves/old.json","migration")==original,"Loading a migration rewrote source save");migrations.save("upgraded");
        const auto upgraded=Json::parse(encodeData(SaveGame::inspect(root/"migration-saves/upgraded.json")));check(upgraded.at("mod_schemas").at("upgrade")==2,"New schema was not recorded");
        SaveGame currentOnly(root/"migration-saves",migrationWorld,migrationPhysics,migrationPlayer,assets);auto strict=policy;strict.compatibleFrom.clear();currentOnly.setPolicies({strict});
        rejects([&]{currentOnly.checkCompatibility({{"upgrade","1.0.0"}},{{"upgrade",1}});},"Strict policy accepted stale schema metadata");
        currentOnly.checkCompatibility(upgraded.at("mods").get<std::map<std::string,std::string>>(),upgraded.at("mod_schemas").get<std::map<std::string,unsigned>>());
        write(migrationMod/"schema1.sl","func migrate(bundle):\n    destroy(get_entity(\"after\"))\n    return bundle\n");const auto still=migrationWorld.find("after");rejects([&]{migrations.load("old");},"Migration accessed live entity APIs");check(migrationWorld.alive(still),"Failed migration mutated live world");
        // Complete world backup, checksum verification, GC with old slots, and recovery of an interrupted restore.
        WorldStorage storage(root/"worlds");auto record=storage.create("Robust world","robust",definition.generators.begin()->first,definition,{});const auto folder=storage.directory(record.id);
        World terrainWorld;PhysicsSystem terrainPhysics(terrainWorld);PlayerController terrainPlayer(terrainWorld,terrainPhysics);SaveGame terrainSaves(folder,terrainWorld,terrainPhysics,terrainPlayer,assets);
        ChunkWorld terrain(renderer,assets,terrainWorld,terrainPhysics,terrainPlayer,TerrainGenerator(definition,record.generator,record.seed));bindWorldPersistence(terrainSaves,terrain);
        terrain.setBlock(1000,10,1000,"plains:stone");terrainSaves.save("checkpoint");const auto backup=storage.backup(record.id);
        terrain.setBlock(1000,10,1000,"plains:dirt");terrainSaves.save("checkpoint");terrain.setBlock(1000,10,1000,"plains:wood");terrainSaves.save("checkpoint");
        // GC is an offline API: discard the live terrain store before deleting unreferenced pages.
        terrain.restoreBlockEdits({});const auto removed=storage.collect(record.id);check(removed>0,"GC retained every unreachable generation");
        check(terrainSaves.load("checkpoint").empty() && terrain.blockAt(1000,10,1000)=="plains:wood","GC removed current terrain");
        const auto previous=Json::parse(encodeData(SaveGame::inspect(folder/"checkpoint.json.bak")));(void)RegionEdits::restore(folder/"terrain",previous.at("state").at("core").at("regions"),definition);
        terrain.restoreBlockEdits({});storage.restoreBackup(record.id,backup);check(terrainSaves.load("checkpoint").empty() && terrain.blockAt(1000,10,1000)=="plains:stone","Complete backup failed to restore terrain");
        const auto backupFolder=root/"world-backups"/record.id/backup;const auto checkpointHash=fingerprint(folder/"checkpoint.json","backup");write(backupFolder/"checkpoint.json","corrupt");
        rejects([&]{storage.restoreBackup(record.id,backup);},"Corrupt backup accepted");check(fingerprint(folder/"checkpoint.json","backup")==checkpointHash,"Corrupt restore modified live files");
        terrain.restoreBlockEdits({});const auto oldFolder=root/"worlds"/(record.id+".restore-old");std::filesystem::rename(folder,oldFolder);atomicWrite(root/"worlds"/("restore-"+record.id+".json"),"{}");
        check(storage.read(record.id).id==record.id && std::filesystem::exists(folder/"checkpoint.json"),"Interrupted restore did not recover previous world");
        InputSystem menuInput;menuInput.setViewport(1280,720);FrontEnd menu(renderer,window,menuInput,root,definition,{});
        const auto click=[&](int x,int y){menuInput.beginFrame();Event e;e.type=EventType::MouseMove;e.x=float(x);e.y=float(y);menuInput.process(e);e.type=EventType::MouseDown;e.mouse=MouseButton::Left;menuInput.process(e);menu.update();e.type=EventType::MouseUp;menuInput.process(e);menuInput.beginFrame();menu.update();};
        click(150,300);click(900,230);check(menu.screen()==FrontEnd::Screen::Backups,"World menu did not offer backup selection");
        auto menuGate=std::make_shared<std::promise<void>>();auto menuReady=menuGate->get_future().share();auto menuBlocked=IoExecutor::instance().submit([menuReady]{menuReady.wait();});
        click(150,230);Event escape;escape.type=EventType::KeyDown;escape.key=Key::Escape;menu.event(escape);check(menu.screen()==FrontEnd::Screen::Backups,"Navigation escaped a pending restore");
        menuGate->set_value();menuBlocked.get();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(menu.screen()==FrontEnd::Screen::Backups && std::chrono::steady_clock::now()<deadline) {menu.update();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(menu.screen()==FrontEnd::Screen::Worlds,"Background restore did not finish in world menu");check(terrainSaves.load("checkpoint").empty(),"World restored through menu is invalid");
        // Repeated asynchronous snapshots, loads and distant edits under a live streaming workload.
        auto actor=terrainWorld.create("actor");terrainWorld.add(actor,TransformComponent{{{8,8,8}}});terrainPhysics.attachBox(actor,{.3f,.9f,.3f},true);terrainPlayer.possess(actor,4);
        script::Runtime stressRuntime;Components stressComponents;bindRuntimeApi(stressRuntime,terrainWorld,assets);ExtensionBridge stressBridge(stressRuntime,terrainWorld,stressComponents,terrainSaves);bindWorldPersistence(terrainSaves,terrain);
        std::map<std::string,std::string> versions;for(const auto& mod:report.mods) versions[mod.id]="1.0.0";terrainSaves.setMods(versions);
        for(const auto& mod:report.mods) if(!mod.entry.empty()) stressRuntime.load(mod.id,mod.root,mod.entry);
        check(stressRuntime.emit("on_start").empty(),"Combined mod initialization failed");
        const auto actorKey=terrainWorld.get<IdentityComponent>(actor).persistent;std::vector<double> timings;const auto initialMemory=memory();size_t peak=initialMemory;
        for(int i=0;i<iterations;++i) {
            const auto start=std::chrono::steady_clock::now();check(stressRuntime.emit("on_update",{script::Value(1.0/60)}).empty(),"Combined mod system failed");terrainWorld.get<TransformComponent>(actor).local.position.x=float((i%48)*32+8);terrain.update(3);
            if(i%16==0 && !terrainSaves.saving()) {terrain.setBlock(100000+i*16,12,-1000,"plains:stone");check(stressRuntime.emit("on_before_save").empty(),"Combined mod state capture failed");terrainSaves.requestSave("stress");}
            if(terrainSaves.saving()) terrainSaves.finishSave();
            if(i%128==127) {terrainSaves.finishSave(true);check(terrainSaves.load("stress").empty(),"Stress snapshot failed to load");actor=terrainWorld.findPersistent(actorKey);check(stressRuntime.emit("on_loaded").empty(),"Combined mod state restoration failed");}
            timings.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());peak=std::max(peak,memory());
            if(i%8==0) {renderer.render(terrainPlayer.camera(),terrain.renderItems());renderer.present();}std::this_thread::sleep_for(std::chrono::milliseconds(1));
            check(terrain.streamingStats().residentPages<=64 && terrain.streamingStats().queued<=24,"Streaming bounds exceeded during async saves");
        }
        terrainSaves.finishSave(true);std::sort(timings.begin(),timings.end());
        for(const auto& mod:report.mods) if(!mod.entry.empty()) check(stressRuntime.global(mod.id,"updates").number()==iterations,"Combined mod lost system updates");
        Json result={{"device",renderer.deviceName()},{"iterations",iterations},{"mods",report.mods.size()},{"private_start",initialMemory},{"private_end",memory()},{"private_peak_sampled",peak},{"update_p95_ms",timings[timings.size()*95/100]},{"backup_gc_removed",removed}};
        if(iterations>=1024) {
            // Repeated immutable page references create an index above the old global JSON ceiling without manufacturing hundreds of thousands of files.
            RegionEdits seed(root/"large-index");seed.replace({0,0},{{{0,4,0},"plains:stone"}});const auto page=seed.snapshot().at("pages")[0][2];Json rows=Json::array();
            for(int i=0;i<500000;++i) rows.push_back({std::to_string(i),"0",page});
            auto large=RegionEdits::restore(root/"large-index",{{"version",1},{"pages",std::move(rows)}},definition);const auto index=large.checkpoint();
            const auto bytes=std::filesystem::file_size(root/"large-index"/index.at("index").get<std::string>());check(bytes>16*1024*1024 && index.dump().size()<256,"Large index did not cross old save ceiling");
            auto restored=RegionEdits::restore(root/"large-index",index,definition);check(restored.pageCount()==500000 && restored.get(499999*16,4,0)=="plains:stone","Large external index failed to restore");result["index_bytes"]=bytes;result["index_pages"]=restored.pageCount();
        }
        atomicWrite(root/"result.json",result.dump(2));std::cout<<result.dump(2)<<"\nReport: "<<(root/"result.json").string()<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
