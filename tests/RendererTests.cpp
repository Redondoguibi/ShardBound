#include "platform/WindowSystem.h"
#include "game/DemoScene.h"
#include <iostream>
#include <stdexcept>
void require(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        game::WindowSystem window({"Renderer test",320,240,true});
        game::Renderer renderer(window.nativeHandle(),320,240,"assets/core/shaders/forward.hlsl",{false,0.5f});
        game::RenderItem item{game::createDemoCube(renderer),{{0,1,0,1},{},true},{}};
        for (const auto size : {320,400}) {
            renderer.resize(size,240);
            renderer.render({},std::span(&item,1));
            auto pixels = renderer.readback();
            unsigned green = 0;
            for (size_t i=0;i<pixels.size();i+=4) if (pixels[i+1] > 180 && pixels[i] < 40) ++green;
            if (green < 100 || renderer.stats().triangles != 12 || renderer.renderWidth() != size/2)
                throw std::runtime_error("Mesh rendering/readback/resize failed");
            renderer.present();
        }
        renderer.resize(320,240);
        const game::Vertex vertices[]={
            {{-1,-1,0},{0,0,-1},{0,1}},{{-1,1,0},{0,0,-1},{0,0}},
            {{1,1,0},{0,0,-1},{1,0}},{{1,-1,0},{0,0,-1},{1,1}}};
        const std::uint32_t indices[]={0,1,2,0,2,3};
        const auto quad=renderer.createMesh(vertices,indices);
        const std::uint8_t cutout[]={255,255,255,0,255,255,255,255};
        game::RenderItem front{quad,{{1,0,0,1},renderer.createTexture(2,1,cutout,false),true},{}};
        front.material.alphaMode=game::AlphaMode::Mask;
        game::RenderItem back{quad,{{0,1,0,1},{},true},game::transformMatrix({{0,0,0.5f}})};
        const game::Camera camera{{0,0,-4},{0,0,0}};
        auto sample=[&](float x) {
            const auto pixels=renderer.readback();
            const auto i=(size_t(renderer.renderHeight()/2)*renderer.renderWidth()+size_t(float(renderer.renderWidth())*x))*4;
            return std::array<std::uint8_t,3>{pixels[i],pixels[i+1],pixels[i+2]};
        };
        auto drawLayers=[&]() { const game::RenderItem layers[]={front,back}; renderer.render(camera,layers); };
        // Draw the cutout first: discarded fragments must not occlude the later green quad.
        drawLayers();
        require(sample(0.4f)[1]>240 && sample(0.4f)[0]<10,"MASK holes wrote color or depth");
        require(sample(0.6f)[0]>240 && sample(0.6f)[1]<10,"MASK solid texel was discarded");
        front.material.alphaMode=game::AlphaMode::Opaque;
        drawLayers();
        require(sample(0.4f)[0]>240,"OPAQUE incorrectly used texture alpha");
        front.material.alphaMode=game::AlphaMode::Mask;
        front.material.texture.reset();
        front.material.color.a=0.5f;
        drawLayers();
        require(sample(0.5f)[0]>240,"MASK cutoff equality should survive");
        front.material.color.a=0.49f;
        drawLayers();
        require(sample(0.5f)[1]>240,"MASK ignored baseColorFactor alpha");
        front.material.color.a=1;
        front.material.alphaMode=game::AlphaMode::Opaque;
        const game::Camera opposite{{0,0,4},{0,0,0}};
        renderer.render(opposite,std::span(&front,1));
        require(sample(0.5f)[0]<30,"Single-sided back face was visible");
        front.material.doubleSided=true;
        renderer.render(opposite,std::span(&front,1));
        require(sample(0.5f)[0]>240,"Double-sided back face disappeared");
        front.material.unlit=false;
        renderer.render(camera,std::span(&front,1));
        const auto frontLight=sample(0.5f)[0];
        renderer.render(opposite,std::span(&front,1));
        require(frontLight>sample(0.5f)[0]+30,"Double-sided back-face normal was not reversed");
        front.material.unlit=true;
        front.material.doubleSided=false;
        front.world=game::transformMatrix({{},{},{-1,1,1}});
        renderer.render(camera,std::span(&front,1));
        require(sample(0.5f)[0]>240,"Mirrored transform changed front-face visibility");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
