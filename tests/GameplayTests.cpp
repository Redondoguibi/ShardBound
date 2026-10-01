#include "scripting/GameplayBridge.h"
#include "game/HealthSystem.h"
#include "game/InventorySystem.h"
#include "game/InteractionSystem.h"
#include "mods/ModLoader.h"
#include "ecs/RenderSystem.h"
#include "platform/WindowSystem.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    using namespace game;
    try {
        WindowSystem window({"Gameplay test",320,240,true});
        Renderer renderer(window.nativeHandle(),320,240,"assets/core/shaders/forward.hlsl");
        VirtualFileSystem vfs; vfs.mount("core","assets/core"); AssetManager assets(vfs,renderer,"user/gameplay-cache");
        assets.registerModel({AssetId("core:cube"),"models/cube.glb"});
        ModLoader loader(vfs,assets); auto report=loader.load("examples/gameplay");
        require(report.diagnostics.empty() && report.registeredAssets==2,"Sandbox assets failed");
        World world; PhysicsSystem physics(world); PlayerController player(world,physics); InputSystem input; AudioSystem audio(true);
        script::Runtime runtime; bindRuntimeApi(runtime,world,assets); bindGameplayApi(runtime,world,physics,player,input,audio,assets);
        for(const auto& mod:report.mods) runtime.load(mod.id,mod.root,mod.entry);
        const auto diagnostics=runtime.emit("on_start");
        for(const auto& d:diagnostics) std::cerr<<DiagnosticError(d).what()<<'\n';
        require(diagnostics.empty() && world.entities().size()==3,"Gameplay startup failed");
        const auto actor=player.entity(); require(bool(actor),"No player possessed");
        for(int i=0;i<90;++i) { player.move(0,1); physics.step(1.0f/60); }
        const auto p=world.get<TransformComponent>(actor).local.position;
        require(p.z>1 && p.z<2.4f && p.y>0.95f && p.y<1.1f,"Player failed to move/collide with floor and target");
        const auto target=nearestInteractable(world,actor,2); require(bool(target),"Nearby interaction target missing");
        Event interaction; interaction.type=EventType::KeyDown; interaction.key=Key::E; input.process(interaction);
        require(runtime.emit("on_update",{script::Value(0.016)}).empty(),"Script interaction failed");
        require(itemCount(world,actor,AssetId("sandbox:Token"))==1,"Interaction did not grant item");
        auto samples=audio.mix(4800); bool audible=false; for(auto sample:samples) audible|=std::abs(sample)>0.001f;
        require(audible,"Interaction sound was silent");
        require(!takeItem(world,actor,AssetId("sandbox:Token"),2),"Inventory removed unavailable items");
        require(takeItem(world,actor,AssetId("sandbox:Token"),1),"Inventory could not remove item");
        giveItem(world,actor,AssetId("sandbox:Token"),std::numeric_limits<std::uint32_t>::max());
        bool rejected=false; try { giveItem(world,actor,AssetId("sandbox:Token"),1); } catch(const DiagnosticError&) { rejected=true; }
        require(rejected && itemCount(world,actor,AssetId("sandbox:Token"))==std::numeric_limits<std::uint32_t>::max(),"Inventory overflow was not atomic");
        require(damage(world,target,100)==30 && world.get<HealthComponent>(target).current==0,"Damage clamp failed");
        require(heal(world,target,100)==30,"Healing clamp failed");
        renderer.render(player.camera(),collectRenderItems(world));
        require(renderer.stats().triangles==96,"Player parts or sandbox meshes missing");
        world.get<TransformComponent>(actor).local.position={5,2,0}; physics.step(1.0f/60);
        require(world.get<TransformComponent>(actor).local.position.x>4.9f,"Script teleport did not reach physics");
        world.destroy(actor); physics.step(1.0f/60); require(!player.entity(),"Destroyed player retained a body");
        bool cancelled=false;
        assets.unmount("sandbox");
        try { loader.load("examples/gameplay",[](auto,auto,auto) { throw PreparationCancelled(); }); }
        catch(const PreparationCancelled&) { cancelled=true; }
        require(cancelled,"Startup cancellation was swallowed");
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
