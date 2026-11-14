#pragma once
#include "scripting/Runtime.h"
#include "renderer/Renderer.h"
#include "platform/TextImage.h"
#include "game/WidgetUi.h"
#include <chrono>
namespace game {
// Script-owned presentation and simulation control, independent of content rules.
class GameUi {
public:
    GameUi(script::Runtime& runtime,Renderer& renderer,InputSystem* input=nullptr);
    void event(const Event& e) { if(widgets_) widgets_->event(e); }
    void updateInput() { if(widgets_) widgets_->update(); }
    bool textInput() const { return widgets_ && widgets_->textInput(); }
    bool modalInput() const { return widgets_ && widgets_->modalInput(); }
    bool capturesMenuInput(const Event& event) const { return widgets_ && widgets_->capturesMenuInput(event); }
    void renderWidgets(Renderer& renderer);
    void toggleDiagnostics() { diagnosticsVisible_=!diagnosticsVisible_;diagnosticsUpdated_={}; }
    std::function<std::string()> diagnostics;
    std::function<std::string()> performanceReport;
    std::function<std::vector<double>()> performanceHistory;
    void performanceVisible(bool value) {performanceVisible_=value;diagnosticsUpdated_={};}
    bool paused() const { return paused_; }
    bool quitRequested() const { return quit_; }
    bool handlesPause() const { return pauseManaged_; }
    TextureHandle texture() const { return widgetTexture_ ? widgetTexture_ : texture_; }
    TextureHandle underlay() const {return widgetTexture_ && !canvas_?texture_:TextureHandle{};}
private:
    std::unique_ptr<WidgetUi> widgets_;
    TextureHandle widgetTexture_;
    TextureHandle texture_;
    bool paused_=false,quit_=false,canvas_=false,pauseManaged_=false;
    std::vector<CanvasCommand> commands_;
    std::string text_;
    bool map_=false,expanded_=false;
    std::vector<UiMarker> markers_;
    bool diagnosticsVisible_=false;
    bool performanceVisible_=false;
    std::chrono::steady_clock::time_point diagnosticsUpdated_{};
    std::vector<CanvasCommand> diagnosticCommands_;
};
}
