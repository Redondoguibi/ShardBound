#include "assets/AssetManager.h"
#include "assets/TextureCompiler.h"
#include "assets/Fingerprint.h"
#include "assets/ModelCompiler.h"
#include "platform/WindowSystem.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
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
        assets.registerModel({game::AssetId("core:cube"),"models/cube.glb"});
        auto model=assets.model(game::AssetId("core:cube"));
        require(model->parts.size()==1 && bool(model->parts[0].material.texture),"Embedded GLB texture was not imported");
        renderer.render({},model->parts);
        require(renderer.stats().triangles==12,"Imported GLB was not drawn");
        model.reset(); model=assets.model(game::AssetId("core:cube"));
        require(assets.cacheHits()>=2,"Compiled model was not reused");
        assets.unmount("core"); rejected = false;
        try { assets.texture(game::AssetId("core:checker")); } catch (...) { rejected = true; }
        require(rejected,"Removed namespace still resolves");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
