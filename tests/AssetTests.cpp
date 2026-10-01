#include "assets/AssetManager.h"
#include "assets/TextureCompiler.h"
#include "assets/Fingerprint.h"
#include "assets/ModelCompiler.h"
#include "assets/CoreAssets.h"
#include "platform/WindowSystem.h"
#include "mods/ModLoader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <nlohmann/json.hpp>
void require(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        bool rejected = false;
        try { game::AssetId bad("core:../escape"); } catch (...) { rejected = true; }
        require(rejected,"Traversal ID accepted");
        game::VirtualFileSystem vfs; vfs.mount("core","assets/core");
        rejected = false;
        try { vfs.resolve("core","../../CMakeCache.txt"); } catch (...) { rejected = true; }
        require(rejected,"VFS traversal accepted");
        const auto original = game::fingerprint("assets/core/textures/checker.png","a");
        require(original != game::fingerprint("assets/core/textures/checker.png","b"),"Settings do not invalidate cache");
        game::WindowSystem window({"Asset test",128,128,true});
        game::Renderer renderer(window.nativeHandle(),128,128,"assets/core/shaders/forward.hlsl");
        game::AssetManager assets(vfs,renderer,"user/test-cache");
        assets.registerTexture({game::AssetId("core:checker"),"textures/checker.png"});
        auto texture = assets.texture(game::AssetId("core:checker"));
        require(texture == assets.texture(game::AssetId("core:checker")),"Texture not shared");
        std::weak_ptr<const game::TextureResource> weak = texture; texture.reset();
        require(weak.expired(),"Asset lifetime cannot unload");
        auto cached = assets.texture(game::AssetId("core:checker"));
        require(assets.cacheHits() >= 1,"Warm cache not used");
        game::registerCoreModels(assets);
        auto model=assets.model(game::AssetId("core:cube"));
        require(model->parts.size()==1 && bool(model->parts[0].material.texture),"Embedded GLB texture was not imported");
        renderer.render({},model->parts);
        require(renderer.stats().triangles==12,"Imported GLB was not drawn");
        model.reset(); model=assets.model(game::AssetId("core:cube"));
        require(assets.cacheHits()>=2,"Compiled model was not reused");
        game::ModLoader loader(vfs,assets);
        const auto report=loader.load("mods");
        require(report.mods.size()==1 && report.registeredAssets==1 && report.diagnostics.empty(),"Mod discovery/load failed");
        require(bool(assets.model(game::AssetId("example:cube"))),"External model registry did not load");
        struct Prototype { const char* name; size_t parts, triangles; game::Transform display; };
        const Prototype prototypes[]={
            {"sword",1,20,{{0.3f,-1.7f,-1.8f},{},{0.15f,0.15f,0.15f}}},
            {"tree",47,564,{{-1.7f,0,1},{},{0.025f,0.025f,0.025f}}},
            {"waystone",8,288,{{1.5f,0.04f,0.8f},{},{0.04f,0.04f,0.04f}}}};
        std::vector<game::RenderItem> gallery;
        for (const auto& prototype:prototypes) {
            auto asset=assets.model(game::AssetId(std::string("core:")+prototype.name));
            require(asset->parts.size()==prototype.parts,"Prototype lost mesh parts");
            auto items=asset->parts;
            for (auto& part:items) {
                require(part.material.texture && part.material.alphaMode==game::AlphaMode::Mask &&
                    part.material.doubleSided && std::abs(part.material.alphaCutoff-0.05f)<0.0001f,
                    "Prototype material was not preserved");
                part.world=game::multiply(part.world,game::transformMatrix(prototype.display));
            }
            renderer.render({},{});
            const auto background=renderer.readback();
            renderer.render({},items);
            const auto pixels=renderer.readback();
            size_t changed=0;
            for (size_t i=0;i<pixels.size();i+=4)
                if (pixels[i]!=background[i] || pixels[i+1]!=background[i+1] || pixels[i+2]!=background[i+2]) ++changed;
            require(changed>20 && renderer.stats().triangles==prototype.triangles,"Prototype did not render visible geometry");
            gallery.insert(gallery.end(),items.begin(),items.end());
            const auto cachedModel=game::compileModel(std::string("assets/core/models/")+prototype.name+".glb","user/test-cache/core");
            require(cachedModel.cacheHit && cachedModel.parts.size()==prototype.parts,"Prototype cache round trip failed");
            require(cachedModel.parts[0].alphaMode==game::AlphaMode::Mask && cachedModel.parts[0].doubleSided &&
                std::abs(cachedModel.parts[0].alphaCutoff-0.05f)<0.0001f,"Cache lost material settings");
        }
        renderer.render({},gallery);
        require(renderer.stats().draws==56 && renderer.stats().triangles==872,"Gallery lost model parts");
        if (argc==2 && std::string_view(argv[1])=="--preview") {
            renderer.resize(960,640); renderer.render({},gallery);
            const auto pixels=renderer.readback();
            std::ofstream preview("user/asset-gallery.ppm",std::ios::binary);
            preview << "P6\n960 640\n255\n";
            for (size_t i=0;i<pixels.size();i+=4) preview.write(reinterpret_cast<const char*>(pixels.data()+i),3);
            require(bool(preview),"Could not save GPU preview");
        }
        // Mutate only the JSON chunk of a valid GLB to exercise importer modes.
        std::ifstream fixtureInput("assets/core/models/cube.glb",std::ios::binary);
        const std::vector<char> originalGlb((std::istreambuf_iterator<char>(fixtureInput)),{});
        std::uint32_t jsonLength=0; std::memcpy(&jsonLength,originalGlb.data()+12,4);
        const auto fixtureJson=nlohmann::json::parse(originalGlb.begin()+20,originalGlb.begin()+20+jsonLength);
        const auto fixturePath=std::filesystem::path("user/test-cache/material-fixture.glb");
        auto writeFixture=[&](const char* mode) {
            auto json=fixtureJson;
            json["materials"][0]["alphaMode"]=mode;
            json["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"]={1,1,1,0.25};
            auto text=json.dump(); text.append((4-text.size()%4)%4,' ');
            const auto tail=originalGlb.size()-20-jsonLength;
            const std::uint32_t header[]={0x46546C67,2,static_cast<std::uint32_t>(20+text.size()+tail),
                static_cast<std::uint32_t>(text.size()),0x4E4F534A};
            std::ofstream output(fixturePath,std::ios::binary);
            output.write(reinterpret_cast<const char*>(header),sizeof(header));
            output.write(text.data(),static_cast<std::streamsize>(text.size()));
            output.write(originalGlb.data()+20+jsonLength,static_cast<std::streamsize>(tail));
            require(bool(output),"Could not write GLB material fixture");
        };
        writeFixture("MASK");
        const auto masked=game::compileModel(fixturePath,"user/test-cache/materials");
        require(masked.parts[0].alphaMode==game::AlphaMode::Mask && masked.parts[0].alphaCutoff==0.5f &&
            !masked.parts[0].doubleSided && masked.parts[0].color.a==0.25f,"glTF material defaults/factor lost");
        writeFixture("OPAQUE");
        require(game::compileModel(fixturePath,"user/test-cache/materials").parts[0].alphaMode==game::AlphaMode::Opaque,
            "OPAQUE material rejected");
        writeFixture("BLEND"); rejected=false;
        try { (void)game::compileModel(fixturePath,"user/test-cache/materials"); }
        catch (const std::exception& error) { rejected=std::string(error.what()).find("BLEND")!=std::string::npos; }
        require(rejected,"BLEND must produce an explicit unsupported-material diagnostic");
        assets.unmount("core"); rejected = false;
        try { assets.texture(game::AssetId("core:checker")); } catch (...) { rejected = true; }
        require(rejected,"Removed namespace still resolves");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
