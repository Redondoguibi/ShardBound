#pragma once
#include "scripting/Runtime.h"
#include "renderer/Renderer.h"
#include "platform/TextImage.h"
namespace game {
// Script-owned text/map. Rendering has no knowledge of adventure rules.
class GameUi {
public:
    GameUi(script::Runtime& runtime,Renderer& renderer);
    TextureHandle texture() const { return texture_; }
private:
    TextureHandle texture_;
    std::string text_;
    bool map_=false,expanded_=false;
    std::vector<UiMarker> markers_;
};
}
