#pragma once
#include "scripting/Runtime.h"
#include "platform/InputSystem.h"
#include "platform/TextImage.h"
#include "renderer/Renderer.h"
#include <optional>
namespace game {
class WidgetUi {
public:
    WidgetUi(script::Runtime& runtime,InputSystem& input);
    void event(const Event& event);
    void update();
    bool textInput() const;
    bool visible() const { return !screens_.empty(); }
    bool modalInput() const { for(const auto& screen:screens_) if(screen.modal) return true;return textInput(); }
    bool capturesMenuInput(const Event& event) const;
    TextureHandle render(Renderer& renderer,const std::vector<CanvasCommand>& background,const std::vector<CanvasCommand>& foreground={});
    std::function<std::string()> readClipboard;
    std::function<void(const std::string&)> writeClipboard;
private:
    struct Widget {
        std::string id,kind,text;script::Value value;
        int x=0,y=0,w=0,h=0;bool enabled=true;
        double minimum=0,maximum=1,step=.1;
        size_t cursor=0,anchor=0;int textOffset=0;
        std::vector<std::pair<size_t,int>> advances;
        bool draggable=false,dropTarget=false;script::Value payload;
    };
    struct Screen { std::string id,owner,focus;bool modal=false;std::vector<Widget> widgets;int x=0,y=0,w=0,h=0,contentHeight=0,scroll=0; };
    Widget* hit(Screen& screen,float x,float y,bool drop=false);
    script::Runtime& runtime_;InputSystem& input_;
    std::vector<Screen> screens_;
    std::vector<Event> events_;
    TextureHandle texture_;
    bool pointerCaptured_=false;
    std::string dragScreen_,dragWidget_;
    struct Drag { std::string screen,widget,label;script::Value payload;float startX=0,startY=0,x=0,y=0;bool started=false; };
    std::optional<Drag> drag_;
    std::vector<CanvasCommand> rendered_;
};
}
