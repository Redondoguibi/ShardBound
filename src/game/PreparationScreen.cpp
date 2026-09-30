#include "game/PreparationScreen.h"
#include "platform/TextImage.h"
namespace game {
PreparationScreen::PreparationScreen(Renderer& renderer) : renderer_(renderer) {
    const Vertex vertices[]={{{-2,-0.75f,0},{0,0,-1},{0,1}},{{-2,0.75f,0},{0,0,-1},{0,0}},{{2,0.75f,0},{0,0,-1},{1,0}},{{2,-0.75f,0},{0,0,-1},{1,1}}};
    const std::uint32_t indices[]={0,1,2,0,2,3}; panel_=renderer.createMesh(vertices,indices);
}
void PreparationScreen::show(std::string_view mod,size_t done,size_t total) {
    auto pixels=preparationImage(std::string(mod),done,total);
    RenderItem item{panel_,{{},renderer_.createTexture(640,240,pixels,false,false),true},{}};
    Camera camera; camera.eye={0,0,-5}; renderer_.render(camera,std::span(&item,1)); renderer_.present();
}
}
