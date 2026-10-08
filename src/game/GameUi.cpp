#include "game/GameUi.h"
#include "platform/TextImage.h"
#include <cmath>
namespace game {
GameUi::GameUi(script::Runtime& runtime,Renderer& renderer) {
    using namespace script;
    runtime.bind("pause_game",native([this](const auto& a,const auto& at) { if(a.size()!=1) fail(at,"UI101","pause_game requires a boolean"); paused_=a[0].boolean(at); pauseManaged_=true; return Value{}; }));
    runtime.bind("quit_game",native([this](const auto& a,const auto& at) { if(!a.empty()) fail(at,"UI101","quit_game takes no arguments"); quit_=true; return Value{}; }));
    runtime.bind("set_environment",native([&renderer](const auto& a,const auto& at) {
        if(a.size()!=1) fail(at,"UI101","set_environment requires a record");
        auto color=[&](const char* key) { auto v=a[0].member(key,at); Color c;
            const auto x=v.member("x",at).number(at),y=v.member("y",at).number(at),z=v.member("z",at).number(at);
            if(!std::isfinite(x+y+z) || x<0 || y<0 || z<0 || x>2 || y>2 || z>2) fail(at,"UI101","Environment colors must be in [0,2]");
            c.r=float(x); c.g=float(y); c.b=float(z); return c; };
        Environment env; env.enabled=true; env.sky=color("sky"); env.sun=color("sun"); env.ambient=color("ambient");
        const auto near=a[0].member("fog_start",at).number(at),far=a[0].member("fog_end",at).number(at);
        if(!std::isfinite(near+far) || near<0 || far<=near || far>10000) fail(at,"UI101","Invalid fog interval");
        env.fogStart=float(near); env.fogEnd=float(far); env.shadows=a[0].member("shadows",at).boolean(at); renderer.setEnvironment(env); return Value{};
    }));
    runtime.bind("show_canvas",native([this,&renderer](const auto& a,const auto& at) {
        if(a.size()!=1) fail(at,"UI101","show_canvas requires a command list");
        auto list=std::get_if<std::shared_ptr<List>>(&a[0].data); if(!list || (*list)->values.size()>2048) fail(at,"UI101","Canvas limit is 2048 commands");
        std::vector<CanvasCommand> commands;
        for(const auto& value:(*list)->values) {
            CanvasCommand command; command.kind=value.member("kind",at).string(at);
            if(command.kind!="text" && command.kind!="rect" && command.kind!="diamond") fail(at,"UI101","Unsupported canvas primitive");
            auto number=[&](const char* key,double low,double high) { double v=value.member(key,at).number(at); if(!std::isfinite(v) || v<low || v>high) fail(at,"UI101","Invalid canvas numeric field"); return v; };
            command.x=int(number("x",-1280,2560)); command.y=int(number("y",-720,1440)); command.w=int(number("w",0,2560)); command.h=int(number("h",0,1440));
            auto color=value.member("color",at); double rgb[]={color.member("x",at).number(at),color.member("y",at).number(at),color.member("z",at).number(at)};
            for(size_t i=0;i<3;++i) { if(!std::isfinite(rgb[i]) || rgb[i]<0 || rgb[i]>1) fail(at,"UI101","Canvas colors must be in [0,1]"); command.color[i]=static_cast<unsigned char>(rgb[i]*255); }
            command.color[3]=static_cast<unsigned char>(number("alpha",0,1)*255);
            if(command.kind=="text") { command.text=value.member("text",at).string(at); command.size=int(number("size",10,72)); command.align=value.member("align",at).string(at);
                if(command.text.size()>1024 || (command.align!="left" && command.align!="center" && command.align!="right")) fail(at,"UI101","Invalid canvas text"); }
            commands.push_back(std::move(command));
        }
        if(canvas_ && commands_==commands && texture_) return Value{};
        auto texture=renderer.createTexture(1280,720,canvasImage(commands),false,false);
        texture_=std::move(texture); commands_=std::move(commands); canvas_=true; renderer.setCanvasOverlay(true); return Value{};
    }));
    runtime.bind("show_ui",script::native([this,&renderer](const auto& a,const auto& at) {
        if(a.size()!=3 && a.size()!=4) script::fail(at,"BRIDGE101","show_ui requires text, map visibility, markers and optional expanded flag");
        const auto text=a[0].string(at); const bool map=a[1].boolean(at);
        const bool expanded=a.size()==4 && a[3].boolean(at);
        auto list=std::get_if<std::shared_ptr<script::List>>(&a[2].data);
        if(!list || (*list)->values.size()>64 || text.size()>4096) script::fail(at,"UI101","UI text/marker limit exceeded");
        std::vector<UiMarker> markers;
        for(const auto& v:(*list)->values) {
            const auto x=v.member("x",at).number(at),z=v.member("z",at).number(at);
            if(!std::isfinite(x) || !std::isfinite(z) || std::abs(x)>10000 || std::abs(z)>10000) script::fail(at,"UI101","Invalid map coordinate");
            const auto label=v.member("label",at).string(at);
            if(label.size()>128) script::fail(at,"UI101","Map label exceeds 128 bytes");
            markers.push_back({float(x),float(z),label,v.member("active",at).boolean(at)});
        }
        if(!canvas_ && texture_ && text_==text && map_==map && expanded_==expanded && (!map || markers_==markers)) return script::Value{};
        auto texture=renderer.createTexture(600,(map || expanded) ? 440 : 156,gamePanelImage(text,map,markers,expanded),false,false);
        canvas_=false; renderer.setCanvasOverlay(false);
        texture_=std::move(texture); text_=text; map_=map; expanded_=expanded; markers_=std::move(markers);
        return script::Value{};
    }));
}
}
