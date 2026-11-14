#include "game/DeveloperMode.h"
#include "game/Attributes.h"
#include "game/Equipment.h"
#include "game/InventorySystem.h"
#include "mods/ModLoader.h"
#include "scripting/GameplayBridge.h"
#include "scripting/ExtensionBridge.h"
#include "scripting/DataConversion.h"
#include "world/WorldBridge.h"
#include "platform/WindowSystem.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <cmath>
#include <limits>
using namespace game;
using namespace game::script;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F action) { bool caught=false;try {action();}catch(const std::exception&) {caught=true;}check(caught,"Invalid operation accepted"); }
double distance(Vec3 a,Vec3 b) { return std::sqrt(double(a.x-b.x)*(a.x-b.x)+double(a.y-b.y)*(a.y-b.y)+double(a.z-b.z)*(a.z-b.z)); }
Value text(const char* v) { return Value(std::string(v)); }
}
int main() {
    try {
        std::cout<<std::unitbuf;
        const auto directory=std::filesystem::path("user/developer-tests")/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        WindowSystem window({"Developer tools validation",1280,720,true});Renderer renderer(window.nativeHandle(),1280,720,"assets/core/shaders/forward.hlsl",{false,1});
        VirtualFileSystem vfs;AssetManager assets(vfs,renderer,"user/developer-cache");ModLoader loader(vfs,assets);const auto mods=loader.load("content");check(mods.diagnostics.empty(),"Content load failed");
        auto definition=WorldDefinition::load(mods.mods);auto& generator=definition.generators.at("plains:gentle");generator.structures.clear();generator.relief=0;generator.noise.clear();
        World world;PhysicsSystem physics(world);PlayerController player(world,physics);InputSystem input;AudioSystem audio(true);Runtime runtime;
        bindRuntimeApi(runtime,world,assets);bindGameplayApi(runtime,world,physics,player,input,audio,assets);DeveloperMode dev(runtime,world,physics,player,input);
        ChunkWorld terrain(renderer,assets,world,physics,player,TerrainGenerator(definition,"plains:gentle","developer-seed"));dev.terrain(&terrain,"developer-seed");
        SaveGame saves(directory,world,physics,player,assets);Components components;ExtensionBridge extensions(runtime,world,components,saves);
        saves.setMods({{"qa","1.0.0"}});
        saves.includeEntity=[&](EntityId id){return !terrain.transient(id);};bindWorldPersistence(saves,terrain);
        auto actor=world.create("player");world.add(actor,TransformComponent{{{8.5f,6.4f,8.5f}}});physics.attachBox(actor,{.3f,.9f,.3f},true);player.possess(actor,5);player.updateCamera(0);terrain.update(2,true);
        runtime.execute("qa",R"(@Items
item Token:
    .label = "Token"
@Entities
entity Box:
    .label = "Box"
@Globals
spec = record()
spec.default = 10
spec.min = 0
spec.max = 100
register_attribute("qa:power", spec)
func custom(args, actor):
    set_attribute(actor, "qa:power", args.value)
    return "custom result"
arg = record()
arg.name = "value"
arg.type = "number"
arg.optional = false
options = record()
options.description = "Sets power"
options.arguments = [arg]
register_command("qa:power", options, custom)
func broken(args, actor):
    script_error("command failure")
options.arguments = []
register_command("qa:broken", options, broken)
)");
        runtime.bind("script_error",native([](const auto&,const auto& at)->Value {fail(at,"TEST101","command failure");}));
        auto send=[&](Event e) {input.process(e);return dev.event(e);};
        auto key=[&](Key k,bool down=true,bool repeat=false,bool control=false) {Event e;e.type=down?EventType::KeyDown:EventType::KeyUp;e.key=k;e.repeat=repeat;e.control=control;return send(e);};
        auto tap=[&](Key k,bool control=false) {key(k,true,false,control);key(k,false,false,control);};
        auto chord=[&] {key(Key::PageUp);key(Key::PageDown);key(Key::PageUp,false);key(Key::PageDown,false);};
        auto type=[&](std::string value) {Event e;e.type=EventType::TextInput;e.text=std::move(value);send(e);};
        auto command=[&](const std::string& line,bool process=true) {
            const auto begin=dev.output().size();input.beginFrame();tap(Key::Slash);tap(Key::A,true);type(line);tap(Key::Enter);if(process) dev.processCommands();
            std::string out;for(size_t i=begin;i<dev.output().size();++i) out+=dev.output()[i]+"\n";return out;
        };
        tap(Key::Slash);check(!dev.textInput(),"Console opened without developer mode");
        key(Key::PageUp);key(Key::PageDown);check(dev.enabled(),"Chord did not enable developer mode");key(Key::PageDown,true,true);check(dev.enabled(),"Repeat toggled developer mode");
        key(Key::PageDown,false);key(Key::PageUp,false);
        auto out=command("/list");check(out.find("/attribute")!=std::string::npos && out.find("/qa:power")!=std::string::npos,"List omitted registered commands");check(out.find("/heal")==std::string::npos && out.find("/save")==std::string::npos && out.find("/setblock")==std::string::npos,"Excluded commands were added");
        check(command("/help tp").find("/tp <x>")!=std::string::npos,"Help omitted usage");
        check(command("/list items").find("qa:Token")!=std::string::npos,"Item enumeration failed");
        check(command("/seed").find("developer-seed")!=std::string::npos,"Seed not reported");
        check(command("/give qa:Token 2").find("Erro:")==std::string::npos && itemCount(world,actor,AssetId("qa:Token"))==2,"Give failed");
        for(const auto* bad:{"/give qa:Token -1","/give qa:Token 1.5","/give qa:Token nan","/give qa:Token 4294967296","/give missing:thing","/give qa:Token 1 extra","/give \"qa:Token"}) check(command(bad).find("Erro:")!=std::string::npos,"Bad command accepted");
        check(itemCount(world,actor,AssetId("qa:Token"))==2,"Rejected give changed inventory");
        auto p=world.get<TransformComponent>(actor).local.position;command("/tp ~ ~2 ~");check(std::abs(world.get<TransformComponent>(actor).local.position.y-p.y-2)<.001f,"Relative teleport failed");
        check(command("/tp 1e30 0 0").find("Erro:")!=std::string::npos,"Unbounded teleport accepted");
        const auto before=world.entities().size();command("/spawn qa:Box");check(world.entities().size()==before+1,"Spawn failed");check(command("/spawn qa:Box 1").find("Erro:")!=std::string::npos && world.entities().size()==before+1,"Partial spawn coordinates mutated world");
        check(command("/set 8 12 8 9 12 9 plains:stone").find("4 blocos")!=std::string::npos,"Region edit failed");check(terrain.blockAt(8,12,8)=="plains:stone","Edit not applied");command("/undo");check(terrain.blockAt(8,12,8).empty(),"Undo failed");command("/redo");check(terrain.blockAt(8,12,8)=="plains:stone","Redo failed");
        check(command("/set -10000 10 -10000 10000 10 10000 plains:stone").find("Erro:")!=std::string::npos,"Oversized region accepted");check(terrain.blockAt(8,12,8)=="plains:stone","Rejected region changed terrain");
        command("/set ~ ~3 ~ ~ ~3 ~ plains:dirt");check(terrain.blockAt(8,11,8)=="plains:dirt","Relative block coordinates did not floor the player position");
        command("/set 8 12 8 8 12 8 air");check(terrain.blockAt(8,12,8).empty(),"Air edit failed");
        check(command("/attribute qa:power get").find("base=10 efetivo=10")!=std::string::npos,"Attribute default missing");command("/attribute qa:power set 20");check(attributeBase(world,actor,"qa:power",0)==20,"Attribute set failed");
        giveItem(world,actor,AssetId("qa:Token"),1);EquipmentState equipment;equipment.next=2;ItemInstance item;item.item="qa:Token";item.slot="weapon";item.stats["qa:power"]={5,2};equipment.items["i1"]=item;writeEquipment(world,actor,equipment);
        check(command("/attribute qa:power get").find("efetivo=50")!=std::string::npos,"Equipment was not included in attributes");
        check(command("/attribute qa:power set 101").find("Erro:")!=std::string::npos && attributeBase(world,actor,"qa:power",0)==20,"Attribute range not enforced");
        check(command("/attribute qa:power get 5").find("Erro:")!=std::string::npos && command("/attribute qa:power set").find("Erro:")!=std::string::npos,"Wrong get/set arity accepted");
        check(command("/qa:power 30").find("custom result")!=std::string::npos && attributeBase(world,actor,"qa:power",0)==30,"Mod command callback failed");check(command("/qa:broken").find("Erro:")!=std::string::npos,"Callback exception escaped console");
        runtime.execute("qa",R"(@Globals
func once(args, actor):
    unregister_command("qa:once")
    return "removed safely"
func loop_forever(args, actor):
    value = 0
    while true:
        value = value + 1
register_command("qa:once", options, once)
register_command("qa:loop", options, loop_forever)
)");
        check(command("/qa:once").find("removed safely")!=std::string::npos && command("/qa:once").find("Erro:")!=std::string::npos,"Reentrant unregister invalidated a running command");
        check(command("/qa:loop").find("Erro:")!=std::string::npos,"Command escaped the instruction budget");
        rejects([&]{runtime.execute("other","unregister_command(\"qa:power\")");});rejects([&]{runtime.execute("other","register_attribute(\"qa:power\", record())");});
        rejects([&]{runtime.execute("qa","register_command(\"qa:power\", options, custom)");});
        tap(Key::Slash);type("gi");tap(Key::Tab);check(dev.line()=="/give","Command completion failed");type(" qa:T");tap(Key::Tab);check(dev.line()=="/give qa:Token","Content completion failed");tap(Key::Escape);
        tap(Key::Slash);tap(Key::Up);check(dev.line()=="/qa:loop","History failed");tap(Key::Down);check(dev.line()=="/","Draft not restored");
        tap(Key::A,true);type("/é🙂");tap(Key::Backspace);check(dev.line()=="/é","UTF-8 backspace split a code point");tap(Key::Left);tap(Key::Delete);check(dev.line()=="/","UTF-8 delete split a code point");
        dev.readClipboard=[] {return std::string("/pos");};tap(Key::A,true);tap(Key::V,true);check(dev.line()=="/pos","Console paste failed");std::string clipboard;dev.writeClipboard=[&](const auto& s){clipboard=s;};tap(Key::A,true);tap(Key::C,true);check(clipboard=="/pos","Console copy failed");tap(Key::Escape);
        input.beginFrame();tap(Key::Slash);tap(Key::W);check(input.moveY()==0 && !input.pressed("Attack"),"Console leaked gameplay input");tap(Key::Escape);
        Event slash;slash.type=EventType::KeyDown;slash.key=Key::Slash;dev.event(slash,true,true);check(!dev.textInput(),"Console stole a modal text field");
        auto gameplayCamera=player.camera();command("/camera thirdperson");check(dev.cameraMode()=="thirdperson" && distance(gameplayCamera.eye,dev.camera().eye)>.1,"Third-person camera unavailable");
        const auto target=dev.camera().target;const auto projected=runtime.call("screen_position",{runtime.call("vec3",{Value(double(target.x)),Value(double(target.y)),Value(double(target.z))})});
        check(std::abs(projected.member("x",{}).number()-640)<.1 && std::abs(projected.member("y",{}).number()-360)<.1,"Gameplay projection uses a different camera than rendering");
        const auto c=dev.camera();auto wall=world.create("camera wall");world.add(wall,TransformComponent{{{(c.eye.x+c.target.x)*.5f,(c.eye.y+c.target.y)*.5f,(c.eye.z+c.target.z)*.5f}}});physics.attachBox(wall,{.5f,.5f,.5f},false);
        check(distance(dev.camera().eye,c.target)<distance(c.eye,c.target)-.1,"Third-person camera passed through a wall");physics.detach(wall);world.destroy(wall);
        Event mouse;mouse.type=EventType::MouseDown;mouse.mouse=MouseButton::Right;send(mouse);check(dev.relativeMouse(),"Camera did not capture mouse");mouse.type=EventType::MouseMove;mouse.dx=100;send(mouse);dev.update(0);player.move(0,1);check(std::abs(physics.velocity(actor).x)>1,"Movement did not follow orbit yaw");mouse.type=EventType::MouseUp;mouse.mouse=MouseButton::Right;send(mouse);check(!dev.relativeMouse(),"Camera retained mouse capture");
        command("/camera free");const auto freeBefore=dev.camera().eye;input.beginFrame();key(Key::W);dev.update(.1f);check(distance(freeBefore,dev.camera().eye)>.5 && input.moveY()==0,"Free camera did not move independently");key(Key::W,false);
        const auto still=dev.camera().eye;key(Key::W);dev.update(.1f,true,true);check(distance(still,dev.camera().eye)<.001,"Camera moved through another modal UI");key(Key::W,false);
        const auto freeGlobal=dev.camera().eye;physics.teleport(actor,{300,8,8});terrain.update(2,false,dev.streamingCenter());dev.update(0);check(std::abs(double(dev.camera().eye.x)+terrain.originX()-freeGlobal.x)<.001,"Origin shift moved free camera globally");
        const auto validOrigin=terrain.originX();physics.teleport(actor,{300,8,8});rejects([&]{terrain.update(2,false,Vec3{std::numeric_limits<float>::quiet_NaN(),0,0});});check(terrain.originX()==validOrigin,"Invalid streaming center mutated the origin");physics.teleport(actor,{float(300-terrain.originX()),8,8});
        const auto globalPos=command("/pos");check(globalPos.find("300")!=std::string::npos,"Position used local floating origin");command("/tp 302 8 8");check(std::abs(world.get<TransformComponent>(actor).local.position.x+float(terrain.originX())-302)<.001,"Global teleport ignored floating origin");
        Event focus;focus.type=EventType::FocusLost;send(focus);check(!dev.relativeMouse() && !dev.textInput(),"Focus loss did not release console/camera");
        Event escape;escape.type=EventType::KeyDown;escape.key=Key::Escape;check(!dev.event(escape),"Free camera swallowed pause");
        const auto persistent=world.get<IdentityComponent>(actor).persistent;saves.save("attributes");
        command("/attribute qa:power set 5");check(saves.load("attributes").empty(),"Attribute save failed to reload");actor=world.findPersistent(persistent);check(attributeBase(world,actor,"qa:power",0)==30,"Attribute value did not persist");
        auto corrupt=nlohmann::json::parse(encodeData(SaveGame::inspect(directory/"attributes.json")));for(auto& entity:corrupt["entities"]) if(entity["id"]==persistent) entity["custom"]["core:attributes"]["version"]=2;
        writeJsonAtomic(directory/"corrupt-attributes.json",corrupt);const auto unchanged=player.entity();rejects([&]{saves.load("corrupt-attributes");});check(player.entity()==unchanged && attributeBase(world,actor,"qa:power",0)==30,"Corrupt attribute load changed live state");
        auto oldEquipment=readEquipment(world,actor);auto excessive=oldEquipment;auto& bonusItem=excessive.items.at("i1");bonusItem.stats["qa:power"]={0,10};for(int i=0;i<5;++i) bonusItem.affixes["qa:a"+std::to_string(i)]["qa:power"]={0,100};writeEquipment(world,actor,excessive);
        writeAttributeBase(world,actor,"qa:power",5);check(command("/attribute qa:power set 20").find("Erro:")!=std::string::npos && attributeBase(world,actor,"qa:power",0)==5,"Attribute overflow was committed before validation");writeEquipment(world,actor,oldEquipment);writeAttributeBase(world,actor,"qa:power",30);
        rejects([&]{world.setCustom(actor,AssetId("core:attributes"),{1,Data{Data::Object{{"qa:power",Data{std::numeric_limits<double>::infinity()}}}}});});
        world.setCustom(actor,AssetId("core:attributes"),{2,Data{Data::Object{{"qa:power",Data{30.0}}}}});rejects([&]{saves.save("invalid-attribute");});world.setCustom(actor,AssetId("core:attributes"),{1,Data{Data::Object{{"qa:power",Data{30.0}}}}});
        const auto oldCount=itemCount(world,actor,AssetId("qa:Token"));command("/give qa:Token 10",false);chord();dev.processCommands();check(!dev.enabled() && itemCount(world,actor,AssetId("qa:Token"))==oldCount && dev.cameraMode()=="default","Disabling developer mode executed pending commands or retained camera");
        check(distance(player.camera().eye,dev.camera().eye)<.001,"Default camera not restored");
        chord();tap(Key::Slash);type("help");auto foreground=dev.render(renderer);check(bool(foreground),"Developer overlay not produced");
        std::vector<std::uint8_t> pixels(600*156*4,255);for(size_t i=0;i<pixels.size();i+=4) {pixels[i]=15;pixels[i+1]=30;pixels[i+2]=200;}
        auto legacy=renderer.createTexture(600,156,pixels,false,false);renderer.setCanvasOverlay(false);renderer.render(dev.camera(),{},legacy);const auto baseline=renderer.readback();renderer.render(dev.camera(),{},legacy,foreground);const auto combined=renderer.readback();const auto pixel=(140*1280+300)*4;
        check(std::equal(baseline.begin()+pixel,baseline.begin()+pixel+4,combined.begin()+pixel),"Developer overlay removed existing HUD");check(baseline!=combined,"Developer console did not render over the HUD");
        std::cout<<"Developer controls, console, commands, camera collision/origin, attributes, mod callbacks, saves and HUD composition passed.\n";
        return 0;
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
