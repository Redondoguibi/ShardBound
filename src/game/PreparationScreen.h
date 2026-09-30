#pragma once
#include "renderer/Renderer.h"
namespace game {
class PreparationScreen {
public:
    explicit PreparationScreen(Renderer& renderer);
    void show(std::string_view mod,size_t done,size_t total);
private:
    Renderer& renderer_;
    MeshHandle panel_;
};
}
