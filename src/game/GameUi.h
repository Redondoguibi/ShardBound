#pragma once
#include "scripting/Runtime.h"
#include "renderer/Renderer.h"
#include "platform/TextImage.h"
namespace game {
// Script-owned presentation and simulation control, independent of content rules.
class GameUi {
public:
    GameUi(script::Runtime& runtime,Renderer& renderer);
    bool paused() const { return paused_; }
    bool quitRequested() const { return quit_; }
    bool handlesPause() const { return pauseManaged_; }
    TextureHandle texture() const { return texture_; }
private:
    TextureHandle texture_;
    bool paused_=false,quit_=false,canvas_=false,pauseManaged_=false;
    std::vector<CanvasCommand> commands_;
    std::string text_;
    bool map_=false,expanded_=false;
    std::vector<UiMarker> markers_;
};
}
