#include "studio/StudioRuntime.h"
#include "studio/StudioEffects.h"
#include "assets/CoreAssets.h"
#include "assets/StudioPackage.h"
#include "assets/ModelCompiler.h"
#include "ecs/RenderSystem.h"
#include "platform/WindowSystem.h"
#include "scripting/StudioBridge.h"
#include "mods/ModLoader.h"
#include "scripting/GameplayBridge.h"
#include "scripting/ExtensionBridge.h"
#include "game/GameUi.h"
#include <fstream>
#include <iostream>
#include <cmath>
using namespace game;
using Json=nlohmann::json;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void close(double a,double b){if(std::abs(a-b)>1e-4)throw std::runtime_error("Numerical preview mismatch: "+std::to_string(a)+" vs "+std::to_string(b));}
template<class F> void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"Invalid operation accepted");}
Json read(const std::filesystem::path& p){std::ifstream f(p);return Json::parse(f);}
void write(const std::filesystem::path& p,const Json& j){std::ofstream(p)<<j.dump(2);}
int main(){try{
    auto golden=read("tests/fixtures/studio/effects-golden.json");
    for(const auto& s:golden["samples"]){studio::Interval interval{nullptr,&s["definition"],s["start"],s["end"],s["continuous"]};auto actual=studio::particles(interval,s["time"]);check(actual.size()==s["particles"].size(),"Particle count differs from Blockbench");for(size_t i=0;i<actual.size();i++){const auto& a=actual[i];const auto& e=s["particles"][i];close(a.index,e["index"]);close(a.frame,e["frame"]);close(a.birth,e["birth"]);close(a.position.x,e["position"][0]);close(a.position.y,e["position"][1]);close(a.position.z,e["position"][2]);close(a.size,e["size"]);close(a.opacity,e["opacity"]);close(a.rotation,e["rotation"]);close(a.color.r,e["color"][0]);close(a.color.g,e["color"][1]);close(a.color.b,e["color"][2]);}}
    for(const auto& s:golden["trails"]){studio::Interval interval{nullptr,&golden["trailDefinition"],.25,.75,true};auto values=studio::trailTimes(interval,s["time"]);check(values.size()==s["values"].size(),"Trail count mismatch");for(size_t i=0;i<values.size();i++)close(values[i],s["values"][i]);}
    const auto fixture=std::filesystem::path("examples/studio/studio_demo/sword");
    auto package=loadStudioPackage(fixture/"studio.manifest.json","user/studio-test-cache","studio_demo");check(package->manifest["models"].size()==1,"Manifest model count");
    auto modelData=compileModel(fixture/"models/studio_demo/crystal_sword.glb","user/studio-test-cache");check(modelData.cacheHit,"Animation cache miss");check(modelData.hierarchy.clips.size()>=4,"GLB animations discarded");
    auto broken=modelData.hierarchy;broken.clips[0].tracks[0].times[0]=-1;rejects([&]{validateHierarchy(broken);});
    auto p0=sampleModel(modelData.hierarchy,"attack",0),p1=sampleModel(modelData.hierarchy,"attack",.5);check(p0.size()==p1.size()&&p0.size()>3,"Hierarchy discarded");bool moved=false;for(size_t i=0;i<p0.size();i++)if(p0[i].values!=p1[i].values)moved=true;check(moved,"Animation does not move nodes");
    // Validation must reject unsupported versions, missing targets and path escapes before registration.
    std::filesystem::create_directories("user/studio-invalid");std::filesystem::copy(fixture,"user/studio-invalid",std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
    auto manifest=read("user/studio-invalid/studio.manifest.json");auto original=manifest;manifest["version"]=99;write("user/studio-invalid/studio.manifest.json",manifest);rejects([&]{loadStudioPackage("user/studio-invalid/studio.manifest.json","user/studio-test-cache");});manifest=original;manifest["effects"]="../outside.json";write("user/studio-invalid/studio.manifest.json",manifest);rejects([&]{loadStudioPackage("user/studio-invalid/studio.manifest.json","user/studio-test-cache");});write("user/studio-invalid/studio.manifest.json",original);
    auto ev=read("user/studio-invalid/studio.events.json");ev["clips"][3]["events"][0]["target"]="absent";write("user/studio-invalid/studio.events.json",ev);rejects([&]{loadStudioPackage("user/studio-invalid/studio.manifest.json","user/studio-test-cache");});
    WindowSystem window({"Studio integration tests",640,480,true});Renderer renderer(window.nativeHandle(),640,480,"assets/core/shaders/forward.hlsl");
    VirtualFileSystem vfs;vfs.mount("core","assets/core");AssetManager assets(vfs,renderer,"user/studio-test-cache");registerCoreModels(assets);ModLoader loader(vfs,assets);auto report=loader.load("examples/studio");check(report.diagnostics.empty(),"Studio mod did not register");
    World world;AudioSystem audio(true);StudioRuntime runtime(world,renderer,audio);auto spawn=[&](const std::string& id){auto e=world.create();world.add(e,TransformComponent{});world.add(e,ActiveComponent{});world.add(e,ModelComponent{AssetId(id),assets.model(AssetId(id))});return e;};
    auto actor=spawn("core:player"),sword=spawn("studio_demo:crystal_sword");runtime.attach(sword,actor);runtime.play(sword,"attack");runtime.update(.55);check(audio.activeVoices()==1,"Timeline sound missing or duplicated");auto mixed=audio.mix(4096);check(std::any_of(mixed.begin(),mixed.end(),[](float x){return std::abs(x)>.001f;}),"Timeline audio is silent");
    Camera camera{{3,2,-4},{0,1,0}};auto effects=runtime.render(camera);auto stats=runtime.stats();check(stats.particles>0&&stats.trails>0&&stats.flashes>0&&stats.glows>0,"Missing exported effect kind");auto items=collectRenderItems(world);renderer.render(camera,items);auto before=renderer.readback();items.insert(items.end(),effects.begin(),effects.end());renderer.render(camera,items);auto after=renderer.readback();check(before!=after,"Effects do not affect rendered frame");
    std::filesystem::create_directories("user/studio-preview");std::ofstream image("user/studio-preview/runtime.ppm",std::ios::binary);image<<"P6\n640 480\n255\n";for(size_t i=0;i<after.size();i+=4)image.write(reinterpret_cast<const char*>(after.data()+i),3);image.close();
    const auto& wm=*world.get<ModelComponent>(sword).model;auto ref=package->manifest["models"][0]["bindings"][0]["referenceGroup"].get<std::string>();size_t refIndex=0,armIndex=0;for(size_t i=0;i<wm.hierarchy.nodes.size();i++)if(wm.hierarchy.nodes[i].uuid==ref)refIndex=i;const auto& body=*world.get<ModelComponent>(actor).model;for(size_t i=0;i<body.hierarchy.nodes.size();i++)if(body.hierarchy.nodes[i].name=="RightArm"&&i>armIndex)armIndex=i;
    auto equipmentMatrix=world.get<ModelComponent>(sword).nodeWorld[refIndex],armMatrix=world.get<ModelComponent>(actor).nodeWorld[armIndex];close(equipmentMatrix.values[12],armMatrix.values[12]);close(equipmentMatrix.values[13],armMatrix.values[13]);close(equipmentMatrix.values[14],armMatrix.values[14]);
    {
        auto contextual=std::make_shared<StudioPackage>(*package);Json flash;for(const auto& e:contextual->events["clips"][3]["events"])if(e["type"]=="flash")flash=e;flash["target"]=contextual->player["context"]["groups"]["Head"];contextual->events["clips"][3]["events"]=Json::array({flash});
        auto model=std::make_shared<ModelResource>(*world.get<ModelComponent>(sword).model);model->studio=contextual;
        World contextWorld;auto a=contextWorld.create(),w=contextWorld.create();contextWorld.add(a,TransformComponent{});contextWorld.add(w,TransformComponent{});contextWorld.add(a,ModelComponent{AssetId("core:player"),assets.model(AssetId("core:player"))});contextWorld.add(w,ModelComponent{AssetId("studio_demo:crystal_sword"),model});
        StudioRuntime contextRuntime(contextWorld,renderer,audio);contextRuntime.attach(w,a);contextRuntime.play(w,"attack");contextRuntime.update(.55);auto contextEffects=contextRuntime.render(camera);check(!contextEffects.empty()&&contextRuntime.stats().flashes>0,"Reference-player target outside weapon GLB was lost");
    }
    // Glow must illuminate actual geometry independently of its billboard.
    renderer.setPointLights({});renderer.render(camera,collectRenderItems(world));auto unlitGlow=renderer.readback();(void)runtime.render(camera);renderer.render(camera,collectRenderItems(world));check(renderer.readback()!=unlitGlow,"Glow point light did not illuminate geometry");
    rejects([&]{runtime.play(sword,"attack","invalid");});check(runtime.currentClip(sword)=="attack","Invalid play modified active clip");runtime.pause(sword,true);const auto time=runtime.time(sword);runtime.update(.1);close(runtime.time(sword),time);runtime.pause(sword,false);runtime.setSuspended(true);runtime.update(.1);close(runtime.time(sword),time);runtime.setSuspended(false);
    world.get<ActiveComponent>(sword).value=false;runtime.update(.1);close(runtime.time(sword),time);world.get<ActiveComponent>(sword).value=true;
    runtime.seek(sword,0);check(audio.activeVoices()==0,"Seek did not cancel sound");runtime.update(.6);check(runtime.stats().soundsStarted==2,"Crossed sound event was not fired once");runtime.update(.01);check(runtime.stats().soundsStarted==2,"Sound event fired twice");runtime.stop(sword);check(audio.activeVoices()==0,"Stop leaked sound");check(world.get<ModelComponent>(actor).nodeWorld.empty(),"Stop leaked player pose");
    runtime.play(sword,"attack","loop");runtime.update(2.6);check(runtime.stats().soundsStarted==5,"Loop crossings lost events");world.destroy(sword);runtime.update(0);check(audio.activeVoices()==0,"Destroy leaked audio");check(world.get<ModelComponent>(actor).nodeWorld.empty(),"Destroy leaked player pose");
    // Every content category from a real Blockbench 5.2.2 export, including geometry-free effects.
    auto lab=loadStudioPackage("tests/fixtures/studio/lab/studio.manifest.json","user/studio-test-cache");assets.unmount("studio_demo");vfs.mount("studio_demo","tests/fixtures/studio/lab");assets.registerStudioPackage(AssetId(lab->manifest["project"]),"studio.manifest.json");for(const auto& m:lab->manifest["models"]){auto data=compileModel(lab->file(m["file"]),"user/studio-test-cache");check(!data.hierarchy.nodes.empty(),"Lab hierarchy missing");}
    auto armor=spawn("studio_demo:armor"),weapon=spawn("studio_demo:sword"),monster=spawn("studio_demo:monster"),related=spawn("studio_demo:related_monster");runtime.attach(armor,actor,false);runtime.attach(weapon,actor,true);runtime.play(weapon,"attack");runtime.play(monster,"attack");runtime.play(related,"attack");runtime.update(.2);check(audio.activeVoices()==1,"Independent model events duplicated audio");
    const auto& am=world.get<ModelComponent>(armor);const auto& playerPose=world.get<ModelComponent>(actor).nodeWorld;for(const auto& m:lab->manifest["models"])if(m["id"]=="studio_demo:armor")for(const auto& b:m["bindings"]){size_t equipmentNode=0,parentNode=0;for(size_t i=0;i<am.model->hierarchy.nodes.size();i++)if(am.model->hierarchy.nodes[i].uuid==b["referenceGroup"].get<std::string>())equipmentNode=i;for(size_t i=0;i<body.hierarchy.nodes.size();i++)if(body.hierarchy.nodes[i].name==b["target"].get<std::string>())parentNode=i;for(size_t axis=12;axis<15;axis++)close(am.nodeWorld[equipmentNode].values[axis],playerPose[parentNode].values[axis]);}
    runtime.stop(weapon);runtime.detach(weapon);runtime.stop(monster);runtime.stop(related);runtime.stop(armor);runtime.detach(armor);check(audio.activeVoices()==0,"Lab cleanup leaked sound");
    auto changed=spawn("studio_demo:sword");runtime.attach(changed,actor);runtime.play(changed,"attack");runtime.update(.2);world.get<ModelComponent>(changed).model=assets.model(AssetId("core:cube"));runtime.update(0);check(audio.activeVoices()==0&&world.get<ModelComponent>(actor).nodeWorld.empty(),"Model change leaked playback");world.destroy(changed);
    check(assets.model(AssetId("core:arrow"))->parts.size()==4,"Shared arrow visual unavailable");
    for(const auto* extension:{"wav","ogg","mp3"}){auto file=std::filesystem::path("tests/fixtures/studio/audio")/(std::string("tone.")+extension);auto voice=audio.playVoice(file,.5f,1.5f,true);auto samples=audio.mix(4800);check(std::any_of(samples.begin(),samples.end(),[](float x){return std::abs(x)>.001f;}),"Audio decoder silent");audio.pauseVoice(voice,true);(void)audio.mix(256);samples=audio.mix(4800);check(std::all_of(samples.begin(),samples.end(),[](float x){return std::abs(x)<.0001f;}),"Paused voice still audible");audio.pauseVoice(voice,false);samples=audio.mix(4800);check(std::any_of(samples.begin(),samples.end(),[](float x){return std::abs(x)>.001f;}),"Voice failed to resume");audio.stopVoice(voice);check(audio.activeVoices()==0,"Stopped voice leaked");}
    // Exercise the real adventure equipment path rather than only the standalone demo.
    assets.unmount("studio_demo");vfs.mount("studio_demo","examples/studio/studio_demo");assets.registerStudioPackage(AssetId("studio_demo:crystal_sword"),"sword/studio.manifest.json");
    World adventure;PhysicsSystem physics(adventure);PlayerController player(adventure,physics);InputSystem input;script::Runtime scripts;StudioRuntime playback(adventure,renderer,audio);bindRuntimeApi(scripts,adventure,assets);bindGameplayApi(scripts,adventure,physics,player,input,audio,assets);bindStudioApi(scripts,adventure,playback);
    Components components;SaveGame saves("user/studio-adventure-saves",adventure,physics,player,assets);ExtensionBridge extension(scripts,adventure,components,saves);GameUi ui(scripts,renderer);scripts.load("adventure","examples/legacy/adventure","main.sl");auto errors=scripts.emit("on_start");if(!errors.empty())throw DiagnosticError(errors.front());
    scripts.execute("adventure",R"(@Globals
item = equipped_item()
item.attributes.model = "studio_demo:crystal_sword"
equip_instance(item)
begin_attack()
animate_hero(0.016)
)");
    auto blade=adventure.find("player_sword");check(playback.currentClip(blade)=="attack","Adventure did not choose authored attack");playback.update(.55);check(audio.activeVoices()==1,"Adventure did not play authored sound");check(playback.render(camera).size()>0,"Adventure did not render authored effects");
    scripts.execute("adventure","@Globals\nstate.attack = 0\nbegin_attack()");close(playback.time(blade),0);check(audio.activeVoices()==0,"Repeated attack leaked the previous voice or default swing");
    scripts.execute("adventure",R"(@Globals
state.attack = 0
item.attributes.model = "core:iron_sword"
equip_instance(item)
)");playback.update(0);check(audio.activeVoices()==0&&playback.currentClip(blade).empty(),"Switching back to ordinary equipment leaked Studio state");
    std::cout<<"PASS Studio: preview parity, GLB/cache, validation, D3D VFX, player attachment, lifecycle, WAV/OGG/MP3\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
