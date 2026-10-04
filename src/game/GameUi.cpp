#include "game/GameUi.h"
#include "platform/TextImage.h"
#include <cmath>
namespace game {
GameUi::GameUi(script::Runtime& runtime,Renderer& renderer) {
    runtime.bind("show_ui",script::native([this,&renderer](const auto& a,const auto& at) {
        if(a.size()!=3) script::fail(at,"BRIDGE101","show_ui requires text, map visibility and markers");
        const auto text=a[0].string(at); const bool map=a[1].boolean(at);
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
        texture_=renderer.createTexture(600,map ? 440 : 156,gamePanelImage(text,map,markers),false,false);
        return script::Value{};
    }));
}
}
