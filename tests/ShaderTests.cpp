#include "assets/CoreAssets.h"
#include "assets/ShaderCompiler.h"
#include "mods/ModLoader.h"
#include "scripting/Bridge.h"
#include "scripting/DataConversion.h"
#include "ecs/RenderSystem.h"
#include "world/ChunkWorld.h"
#include "world/WorldStorage.h"
#include "persistence/SaveGame.h"
#include "platform/WindowSystem.h"
#include "game/DemoScene.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <limits>
using namespace game;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* message) {bool rejected=false;try {action();}catch(const std::exception&) {rejected=true;}require(rejected,message);}
void write(const std::filesystem::path& path,const std::string& value) {std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<value;if(!out) throw std::runtime_error("Cannot write fixture");}
}
int main() {
    try {
        const std::filesystem::path root="user/shader-tests";
        const auto mod=root/"mods/paint";
        write(mod/"mod.json",R"({"id":"paint","name":"Shader tests","version":"1.0.0","api":"0.1"})");
        write(mod/"inc/color.hlsli","float3 palette() { return modParameters[0].rgb; }\n");
        const std::string surface=R"(#include "inc/color.hlsli"
void modSurface(VertexOutput v,inout float4 base,inout float3 normal,inout float3 emission) {
    base=float4(palette(),base.a*modParameters[0].a);emission=modParameters[1].rgb;
}
)";
        write(mod/"surface.hlsl",surface);
        write(mod/"post.hlsl",R"(float4 modPost(float2 uv,float4 color) {return float4(color.rgb*modParameters[0].rgb+modParameters[1].rgb,color.a);})");
        write(mod/"sample.hlsl",R"(void modSurface(VertexOutput v,inout float4 base,inout float3 normal,inout float3 emission) {base=modTexture.Sample(modSampler,v.uv);base.r+=modFrame.x;})");
        write(mod/"assets.json",R"({"shaders":{
            "paint:surface":{"source":"surface.hlsl","kind":"surface","version":1},
            "paint:post":{"source":"post.hlsl","kind":"postprocess","version":1},
            "paint:sample":{"source":"sample.hlsl","kind":"surface","version":1}}})");
        WindowSystem window({"Shader tests",256,256,true});Renderer renderer(window.nativeHandle(),256,256,"assets/core/shaders/forward.hlsl",{false,1});
        VirtualFileSystem vfs;vfs.mount("core","assets/core");AssetManager assets(vfs,renderer,root/"cache");registerCoreModels(assets);
        assets.registerTexture({AssetId("core:checker"),"textures/checker.png"});
        ModLoader loader(vfs,assets);const auto report=loader.load(root/"mods");
        for(const auto& d:report.diagnostics) std::cerr<<DiagnosticError(d).what()<<'\n';
        require(report.diagnostics.empty() && report.registeredAssets==3,"Mod shader registry did not compile");
        auto red=assets.shaderInstance("paint:surface",{1,0,0,1});auto green=assets.shaderInstance("paint:surface",{0,1,0,1});
        require(red->shader==green->shader,"Shader program cache not shared");
        rejects([&]{assets.registerShader({"paint:surface","surface.hlsl"});},"Duplicate shader accepted");
        rejects([&]{assets.shaderInstance("paint:post");},"Wrong shader kind accepted");
        rejects([&]{assets.shaderInstance("paint:surface",{std::numeric_limits<float>::infinity()});},"Infinite parameter accepted");
        const Vertex vertices[]={{{-1,-1,0},{0,0,-1},{0,1}},{{-1,1,0},{0,0,-1},{0,0}},{{1,1,0},{0,0,-1},{1,0}},{{1,-1,0},{0,0,-1},{1,1}}};
        const std::uint32_t indices[]={0,1,2,0,2,3};const auto quad=renderer.createMesh(vertices,indices);
        const Camera camera{{0,0,-4},{0,0,0}};
        RenderItem item{quad,{{1,1,1,1},{},true},{}};item.material.shader=red;
        auto pixel=[&](float x=.5f,float y=.5f) {auto data=renderer.readback();const size_t i=(size_t(y*renderer.renderHeight())*renderer.renderWidth()+size_t(x*renderer.renderWidth()))*4;return std::array<int,3>{data[i],data[i+1],data[i+2]};};
        auto draw=[&] {renderer.render(camera,std::span(&item,1));};
        draw();require(pixel()[0]>250 && pixel()[1]<3,"Surface shader not executed on GPU");
        RenderItem pair[]={item,item};pair[0].world=transformMatrix({{-.65f,0,0},{},{.4f,.4f,.4f}});pair[1].world=transformMatrix({{.65f,0,0},{},{.4f,.4f,.4f}});pair[1].material.shader=green;
        renderer.render(camera,pair);require(renderer.stats().draws==2 && pixel(.3f)[0]>250 && pixel(.7f)[1]>250,"Batching mixed shader parameters");
        pair[1].material.shader=assets.shaderInstance("paint:surface",{1,0,0,1});renderer.render(camera,pair);require(renderer.stats().draws==1,"Identical shader parameters lost instancing");
        RenderItem layers[]={item,item};layers[0].material.unlit=false;layers[0].material.alphaMode=AlphaMode::Blend;layers[0].material.shader=assets.shaderInstance("paint:surface",{1,0,0,.5f});
        layers[1].material.shader=green;layers[1].world=transformMatrix({{0,0,.5f}});renderer.render(camera,layers);
        require(std::abs(pixel()[1]-128)<=1 && pixel()[0]>40,"Lit surface shader ignored blend alpha");
        const std::uint8_t bluePixel[]={0,0,255,255};const auto blue=renderer.createTexture(1,1,bluePixel,false);
        auto sampled=std::make_shared<ShaderInstance>(*assets.shaderInstance("paint:sample"));sampled->texture=blue;
        item.material.shader=sampled;renderer.setShaderTime(.25);draw();require(std::abs(pixel()[0]-64)<=1 && pixel()[2]>250,"Extra texture or shader time binding failed");
        item.material.shader=red;
        auto add=assets.shaderInstance("paint:post",{1,1,1,0,0,.5f,0},"",ShaderKind::PostProcess);
        auto multiply=assets.shaderInstance("paint:post",{.5f,.5f,.5f},"",ShaderKind::PostProcess);
        renderer.setPostEffect("paint:z",multiply,10);renderer.setPostEffect("paint:a",add,0);draw();
        require(renderer.stats().postDraws==2 && std::abs(pixel()[0]-128)<=1 && std::abs(pixel()[1]-64)<=1,"Postprocess order or ping-pong targets failed");
        renderer.setPostEffect("paint:z",multiply,-10);draw();require(std::abs(pixel()[1]-128)<=1,"Postprocess priority update ignored");
        renderer.setPostEffect("paint:z",multiply,0);draw();require(std::abs(pixel()[1]-64)<=1,"Equal-priority passes are not ordered by ID");
        renderer.setCanvasOverlay(true);renderer.render(camera,std::span(&item,1),blue);require(pixel()[2]>250 && pixel()[0]<3,"Postprocessing affected UI overlay");
        renderer.resize(320,240);renderer.configure({false,.5f});draw();require(renderer.renderWidth()==160 && std::abs(pixel()[1]-64)<=1,"Postprocess resize/scale failed");
        renderer.configure({false,1});renderer.resize(256,256);
        for(int i=0;i<6;++i) renderer.setPostEffect("paint:extra"+std::to_string(i),multiply);
        rejects([&]{renderer.setPostEffect("paint:overflow",multiply);},"Postprocess pass budget ignored");require(renderer.postEffectCount()==8,"Rejected pass modified pipeline");
        renderer.clearShaderEffects();draw();require(pixel()[0]>250 && pixel()[1]<3,"Clearing shader effects did not restore render");
        const auto checker=assets.texture(AssetId("core:checker"));item.material.texture=checker;item.material.shader={};
        renderer.setTextureShader(checker,green,"paint");draw();require(pixel()[1]>250 && pixel()[0]<3,"Texture override did not apply");
        rejects([&]{renderer.setTextureShader(checker,red,"other");},"Conflicting texture owner accepted");
        item.material.shader=red;draw();require(pixel()[0]>250,"Entity material did not override texture rule");
        renderer.setTextureShader(checker,{},"paint");item.material.texture={};
        // The same surface alpha function must run in the shadow pass.
        Environment environment;environment.enabled=true;environment.fogStart=100;environment.fogEnd=150;renderer.setEnvironment(environment);
        RenderItem ground{createDemoCube(renderer),{{.6f,.6f,.6f,1},{},false},transformMatrix({{0,-.15f,0},{},{5,.15f,5}})};
        RenderItem caster{ground.mesh,{{1,1,1,1},{},false},transformMatrix({{0,1,0},{},{.5f,1,.5f}})};
        const Camera sunCamera{{5,7,-9},{0,0,0}};renderer.render(sunCamera,std::span(&ground,1));const auto baseline=renderer.readback();
        caster.material.alphaMode=AlphaMode::Mask;caster.material.shader=assets.shaderInstance("paint:surface",{1,0,0,0});RenderItem shadowItems[]={ground,caster};
        renderer.render(sunCamera,shadowItems);require(renderer.readback()==baseline,"Surface alpha disagrees with shadow alpha");
        shadowItems[1].material.shader=red;renderer.render(sunCamera,shadowItems);const auto solid=renderer.readback();
        unsigned darkened=0;for(size_t i=0;i<solid.size();i+=4) if(int(baseline[i])-solid[i]>15) ++darkened;
        require(darkened>15,"Custom surface no longer casts shadows");renderer.setEnvironment({});
        // Invalid assets produce origin-aware errors and cannot replace an existing program.
        write(mod/"broken.hlsl","void modSurface(VertexOutput v,inout float4 base,inout float3 normal,inout float3 emission) { base=missing_value; }\n");
        bool diagnostic=false;
        try {assets.registerShader({"paint:broken","broken.hlsl"});}catch(const DiagnosticError& e) {diagnostic=e.diagnostic.code=="SHADER101" && e.diagnostic.mod=="paint" && e.diagnostic.resource=="paint:broken" && !e.diagnostic.message.empty() && e.diagnostic.line==1 && std::filesystem::weakly_canonical(e.diagnostic.file)==std::filesystem::canonical(mod/"broken.hlsl");}
        require(diagnostic,"HLSL diagnostic lost asset origin");
        write(mod/"inc/broken.hlsli","// nested error\nfloat3 nested() { return missing_value; }\n");
        write(mod/"nested.hlsl","#include \"inc/broken.hlsli\"\n"+surface);diagnostic=false;
        try {assets.registerShader({"paint:nested","nested.hlsl"});}catch(const DiagnosticError& e) {diagnostic=e.diagnostic.line==2 && std::filesystem::weakly_canonical(e.diagnostic.file)==std::filesystem::canonical(mod/"inc/broken.hlsli");}
        require(diagnostic,"Nested HLSL diagnostic lost file or line");
        write(root/"secret.hlsli","float secret=1;\n");write(mod/"escape.hlsl","#include \"../../secret.hlsli\"\n"+surface);
        rejects([&]{assets.registerShader({"paint:escape","escape.hlsl"});},"Shader include escaped mod root");
        write(mod/"cycle.hlsl","#include \"cycle.hlsl\"\n"+surface);rejects([&]{assets.registerShader({"paint:cycle","cycle.hlsl"});},"Cyclic includes accepted");
        write(mod/"unknown.hlsl",R"(Texture2D secretTexture:register(t9);void modSurface(VertexOutput v,inout float4 base,inout float3 normal,inout float3 emission) {base=secretTexture.Load(int3(0,0,0));})");
        rejects([&]{assets.registerShader({"paint:unknown","unknown.hlsl"});},"Unsupported resource binding accepted");
        write(mod/"repaired.hlsl",surface);assets.registerShader({"paint:broken","repaired.hlsl"});
        rejects([&]{shaderSource("paint:x",{{"source","surface.hlsl"},{"kind","surface"},{"version",2}});},"Unknown shader ABI accepted");
        // Runtime calls and persistence use the same typed bindings as the renderer.
        World world;PhysicsSystem physics(world);PlayerController player(world,physics);script::Runtime runtime;bindRuntimeApi(runtime,world,assets);
        const auto entity=world.create("subject");world.add(entity,TransformComponent{});world.add(entity,ModelComponent{AssetId("core:cube"),assets.model(AssetId("core:cube"))});
        runtime.bind("subject",entityValue(world,entity));
        runtime.bind("opts",script::object());
        runtime.execute("paint",R"(opts.parameters = [0, 1, 0, 1]
set_entity_shader(subject, "paint:surface", opts)
set_post_effect("paint:grade", "paint:post")
set_texture_shader("core:checker", "paint:surface", opts)
)");
        require(world.get<ModelComponent>(entity).shader->parameters[1]==1 && renderer.postEffectCount()==1,"Script shader APIs failed");
        rejects([&]{runtime.execute("other","set_post_effect(\"paint:grade\", null)");},"Other mod removed a postprocess pass");
        rejects([&]{runtime.execute("paint","set_entity_shader(subject, \"paint:post\")");},"Wrong kind replaced entity shader");
        require(world.get<ModelComponent>(entity).shader->parameters[1]==1,"Failed shader change mutated entity");
        rejects([&]{runtime.execute("paint","opts.parameters = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]\nset_entity_shader(subject, \"paint:surface\", opts)");},"Parameter overflow accepted");
        SaveGame saves(root/"saves",world,physics,player,assets);saves.setMods({{"paint","1.0.0"}});saves.save("shader");
        auto issues=saves.load("shader");require(issues.empty(),"Shader save/load produced diagnostics");
        auto restored=world.find("subject");require(world.get<ModelComponent>(restored).shader->parameters[1]==1,"Entity shader was not persisted");
        require(collectRenderItems(world).front().material.shader==world.get<ModelComponent>(restored).shader,"Entity binding not collected for rendering");
        // Old saves have no shader field. Invalid optional data must not swap live world state.
        auto saved=nlohmann::json::parse(encodeData(SaveGame::inspect(root/"saves/shader.json")));auto corrupt=saved;corrupt["entities"][0]["shader"]["parameters"]={1};write(root/"saves/corrupt.json",corrupt.dump());
        rejects([&]{saves.load("corrupt");},"Malformed shader save accepted");require(world.alive(restored),"Malformed save swapped live world");
        auto legacy=saved;legacy["entities"][0].erase("shader");write(root/"saves/legacy.json",legacy.dump());require(saves.load("legacy").empty() && !world.get<ModelComponent>(world.find("subject")).shader,"Legacy save no longer loads");
        assets.unmount("paint");issues=saves.load("shader");require(!issues.empty() && world.get<ModelComponent>(world.find("subject")).shader && !world.get<ModelComponent>(world.find("subject")).shader->shader,"Missing shader binding not retained");
        saves.save("retained");const auto retained=nlohmann::json::parse(encodeData(SaveGame::inspect(root/"saves/retained.json")));require(retained["entities"][0]["shader"]==saved["entities"][0]["shader"],"Missing shader state lost on save");
        vfs.mount("paint",mod);assets.registerShader({"paint:surface","surface.hlsl"});require(saves.load("retained").empty() && world.get<ModelComponent>(world.find("subject")).shader->shader,"Returning shader did not restore binding");
        // Block shader snapshots survive world metadata and are used by chunk publication.
        auto mods=loader.load("content");require(mods.diagnostics.empty(),"Default content failed");auto definition=WorldDefinition::load(mods.mods);
        definition.blocks.at("plains:grass").shader="paint:surface";definition.blocks.at("plains:grass").shaderParameters={0,1,0,1};
        WorldStorage storage(root/"worlds");auto record=storage.create("Shader terrain","shader seed",definition.generators.begin()->first,definition,{});auto loaded=storage.read(record.id);
        require(loaded.definition.json()==definition.json(),"Block shader lost in world metadata");
        renderer.clearShaderEffects();const auto actor=world.find("subject");physics.attachBox(actor,{.3f,.9f,.3f},true);player.possess(actor,5);
        ChunkWorld terrain(renderer,assets,world,physics,player,TerrainGenerator(loaded.definition,record.generator,record.seed));terrain.update(2,true);
        bool blockShader=false;for(const auto& part:terrain.renderItems()) if(part.material.shader && part.material.shader->shaderId=="paint:surface") blockShader=true;
        require(blockShader,"Block shader not applied to chunk meshes");
        std::cout<<"Shader GPU, script, persistence and terrain checks passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
