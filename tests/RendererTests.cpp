#include "platform/WindowSystem.h"
#include "game/DemoScene.h"
#include <iostream>
#include <stdexcept>
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
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
