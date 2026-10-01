#include "scripting/Bridge.h"
#include "mods/ModLoader.h"
#include "ecs/RenderSystem.h"
#include "platform/WindowSystem.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    try {
        game::WindowSystem window({"Bridge test",320,240,true});
        game::Renderer renderer(window.nativeHandle(),320,240,"assets/core/shaders/forward.hlsl");
        game::VirtualFileSystem vfs; game::AssetManager assets(vfs,renderer,"user/bridge-cache");
        game::ModLoader loader(vfs,assets); auto report=loader.load("mods");
        game::World world; game::script::Runtime script; game::bindRuntimeApi(script,world,assets);
        for(const auto& mod:report.mods) if(!mod.entry.empty()) script.load(mod.id,mod.root,mod.entry);
        require(script.emit("on_start").empty(),"Mod startup failed");
        require(world.entities().size()==1,"Script did not spawn the external model");
        auto id=world.entities()[0];
        require(script.emit("on_update",{game::script::Value(0.5)}).empty(),"Update hook failed");
        require(world.get<game::TransformComponent>(id).local.position.x>0,"Script did not move entity");
        renderer.render({},game::collectRenderItems(world));
        auto pixels=renderer.readback(); unsigned visible=0;
        for(size_t i=0;i<pixels.size();i+=4) if(pixels[i]>30 || pixels[i+1]>30) ++visible;
        require(visible>100 && renderer.stats().triangles==12,"Spawned model was not visible");
        script.execute("remove","destroy(get_entity(\"example:Cube\"))\n");
        require(world.entities().empty(),"Script destroy failed");
        require(!script.emit("on_update",{game::script::Value(0.5)}).empty(),"Stale entity did not produce a controlled error");
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
