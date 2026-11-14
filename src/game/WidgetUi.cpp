#include "game/WidgetUi.h"
#include "core/Log.h"
#include "platform/WindowSystem.h"
#include "scripting/DataConversion.h"
#include <cmath>
#include <algorithm>
#include <set>
namespace game {
namespace {
using namespace script;
const std::map<std::string,Value>& fields(const Value& v,const Location& at) {
    const auto f=std::get_if<std::shared_ptr<Object>>(&v.data);if(!f) fail(at,"UI102","Expected widget record");return (*f)->fields;
}
double num(const Value& v,double low,double high,const Location& at) {
    const auto n=v.number(at);if(!std::isfinite(n) || n<low || n>high) fail(at,"UI102","Invalid widget number");return n;
}
bool interactive(const std::string& kind) { return kind!="label" && kind!="panel"; }
}
WidgetUi::WidgetUi(script::Runtime& runtime,InputSystem& input):runtime_(runtime),input_(input) {
    readClipboard=clipboardText;writeClipboard=setClipboardText;
    runtime.bind("show_widgets",native([this](const auto& a,const auto& at) {
        if(a.size()!=2) fail(at,"UI102","show_widgets(id, {layout,modal,x,y,w,h,gap,widgets})");
        (void)toData(a[1]); // Validate finite, bounded, UTF-8 data before replacing the live screen.
        Screen screen;screen.id=a[0].string(at);screen.owner=runtime_.currentMod();
        if(screen.owner.empty() || !screen.id.starts_with(screen.owner+":") || screen.id.size()<=screen.owner.size()+1 || screen.id.size()>128) fail(at,"UI102","Screen ID must belong to the calling mod");
        const auto& options=fields(a[1],at);const auto layout=a[1].member("layout",at).string(at);
        if(layout!="row" && layout!="column" && layout!="absolute") fail(at,"UI102","Unknown widget layout");
        screen.modal=a[1].member("modal",at).boolean(at);
        // Screen bounds are fractions of the letterboxed UI viewport.
        const double x=num(a[1].member("x",at),0,1,at)*1280,y=num(a[1].member("y",at),0,1,at)*720;
        const double w=num(a[1].member("w",at),.001,1,at)*1280,h=num(a[1].member("h",at),.001,1,at)*720;
        const double gap=options.contains("gap")?num(options.at("gap"),0,128,at):8;
        screen.x=int(x);screen.y=int(y);screen.w=int(w);screen.h=int(h);screen.contentHeight=options.contains("content_height")?int(num(options.at("content_height"),h,16384,at)):int(h);
        if(x+w>1280.001 || y+h>720.001) fail(at,"UI102","Screen exceeds viewport");
        const auto rows=a[1].member("widgets",at);const auto list=std::get_if<std::shared_ptr<List>>(&rows.data);
        if(!list || (*list)->values.empty() || (*list)->values.size()>128) fail(at,"UI102","Expected 1 to 128 widgets");
        const size_t count=(*list)->values.size();std::set<std::string> ids;
        const double cellWidth=layout=="row"?(w-gap*double(count-1))/double(count):w;
        const double cellHeight=layout=="column"?(screen.contentHeight-gap*double(count-1))/double(count):h;
        if(cellWidth<1 || cellHeight<1) fail(at,"UI102","Widgets do not fit layout");
        for(const auto& row:(*list)->values) {
            Widget widget;const auto& f=fields(row,at);widget.id=row.member("id",at).string(at);widget.kind=row.member("kind",at).string(at);
            if(widget.id.empty() || widget.id.size()>128 || !ids.insert(widget.id).second) fail(at,"UI102","Invalid/duplicate widget ID");
            if(widget.kind!="button" && widget.kind!="label" && widget.kind!="toggle" && widget.kind!="slider" && widget.kind!="input" && widget.kind!="panel") fail(at,"UI102","Unsupported widget kind");
            widget.text=f.contains("text")?f.at("text").string(at):std::string{};
            if(widget.text.size()>1024) fail(at,"UI102","Widget text exceeds 1024 bytes");
            if(f.contains("enabled")) widget.enabled=f.at("enabled").boolean(at);
            if(f.contains("draggable")) widget.draggable=f.at("draggable").boolean(at);
            if(f.contains("drop_target")) widget.dropTarget=f.at("drop_target").boolean(at);
            if(f.contains("payload")) widget.payload=clone(f.at("payload"));
            if(widget.draggable && widget.kind!="button" && widget.kind!="panel") fail(at,"UI102","Only buttons and panels can be dragged");
            if(layout=="absolute") {
                const double rx=num(row.member("x",at),0,1,at),ry=num(row.member("y",at),0,1,at);
                const double rw=num(row.member("w",at),.001,1,at),rh=num(row.member("h",at),.001,1,at);
                if(rx+rw>1.000001 || ry+rh>1.000001) fail(at,"UI102","Widget exceeds screen");
                widget.x=int(x+rx*w);widget.y=int(y+ry*screen.contentHeight);widget.w=int(rw*w);widget.h=int(rh*screen.contentHeight);
            } else {
                widget.x=int(x+(layout=="row"?double(screen.widgets.size())*(cellWidth+gap):0));
                widget.y=int(y+(layout=="column"?double(screen.widgets.size())*(cellHeight+gap):0));widget.w=int(cellWidth);widget.h=int(cellHeight);
            }
            if(widget.w<1 || widget.h<1) fail(at,"UI102","Empty widget rectangle");
            if(widget.kind=="toggle") widget.value=Value(row.member("value",at).boolean(at));
            else if(widget.kind=="input") { widget.value=Value(row.member("value",at).string(at));if(widget.value.string().size()>1024) fail(at,"UI102","Input exceeds 1024 UTF-8 bytes"); }
            else if(widget.kind=="slider") {
                widget.minimum=num(row.member("min",at),-1e9,1e9,at);widget.maximum=num(row.member("max",at),-1e9,1e9,at);
                if(widget.maximum<=widget.minimum) fail(at,"UI102","Empty slider interval");
                widget.step=num(row.member("step",at),.000001,widget.maximum-widget.minimum,at);
                widget.value=Value(num(row.member("value",at),widget.minimum,widget.maximum,at));
            }
            if(widget.kind=="input") { widget.cursor=widget.anchor=widget.value.string().size();widget.advances=textAdvances(widget.value.string()); }
            screen.widgets.push_back(std::move(widget));
        }
        const auto old=std::find_if(screens_.begin(),screens_.end(),[&](const auto& s){return s.id==screen.id;});
        size_t total=screen.widgets.size();for(const auto& s:screens_) if(s.id!=screen.id) total+=s.widgets.size();
        if(total>512) fail(at,"UI102","Global widget limit (512) exceeded");
        if(old!=screens_.end()) {
            if(std::any_of(screen.widgets.begin(),screen.widgets.end(),[&](const auto& w){return w.id==old->focus && w.enabled && interactive(w.kind);})) screen.focus=old->focus;
            screen.scroll=std::clamp(old->scroll,0,screen.contentHeight-screen.h);
            for(auto& field:screen.widgets) if(field.kind=="input") for(const auto& previous:old->widgets) if(field.id==previous.id && previous.kind=="input" && field.value.string()==previous.value.string()) { field.cursor=previous.cursor;field.anchor=previous.anchor;field.textOffset=previous.textOffset; }
            *old=std::move(screen);
        } else {
            size_t owned=0;for(const auto& s:screens_) owned+=s.owner==screen.owner;
            if(owned>=4 || screens_.size()>=16 || total>512) fail(at,"UI102","Widget screen capacity reached");screens_.push_back(std::move(screen));
        }
        return Value{};
    }));
    runtime.bind("hide_widgets",native([this](const auto& a,const auto& at) {
        if(a.size()!=1) fail(at,"UI102","hide_widgets(screen_id)");const auto id=a[0].string(at);
        const auto found=std::find_if(screens_.begin(),screens_.end(),[&](const auto& s){return s.id==id;});
        if(found==screens_.end()) return Value(false);
        if(found->owner!=runtime_.currentMod()) fail(at,"UI102","Screen belongs to another mod");if(drag_ && drag_->screen==id) drag_.reset();screens_.erase(found);return Value(true);
    }));
    runtime.bind("widget_value",native([this](const auto& a,const auto& at) {
        if(a.size()!=2) fail(at,"UI102","widget_value(screen, widget)");
        for(const auto& s:screens_) if(s.id==a[0].string(at)) for(const auto& w:s.widgets) if(w.id==a[1].string(at)) return w.value;
        fail(at,"UI102","Unknown widget");
    }));
    runtime.bind("scroll_widgets",native([this](const auto& a,const auto& at) {
        if(a.size()!=2) fail(at,"UI102","scroll_widgets(screen, pixels)");for(auto& screen:screens_) if(screen.id==a[0].string(at)) {
            if(screen.owner!=runtime_.currentMod()) fail(at,"UI102","Screen belongs to another mod");screen.scroll=std::clamp(int(num(a[1],0,16384,at)),0,screen.contentHeight-screen.h);return Value(double(screen.scroll));
        }fail(at,"UI102","Unknown screen");
    }));
    runtime.bind("focus_widget",native([this](const auto& a,const auto& at) {
        if(a.size()!=2) fail(at,"UI102","focus_widget(screen, widget)");
        for(auto& s:screens_) if(s.id==a[0].string(at)) {
            if(s.owner!=runtime_.currentMod()) fail(at,"UI102","Screen belongs to another mod");
            for(const auto& w:s.widgets) if(w.id==a[1].string(at) && w.enabled && interactive(w.kind)) { for(auto& other:screens_) if(other.id!=s.id) other.focus.clear();s.focus=w.id;s.scroll=std::clamp(w.y-s.y,0,s.contentHeight-s.h);return Value{}; }
        }
        fail(at,"UI102","Unknown/disabled widget");
    }));
}
WidgetUi::Widget* WidgetUi::hit(Screen& screen,float x,float y,bool drop) {
    if(x<screen.x || y<screen.y || x>=screen.x+screen.w || y>=screen.y+screen.h) return nullptr;
    for(auto w=screen.widgets.rbegin();w!=screen.widgets.rend();++w) if(x>=w->x && y>=w->y-screen.scroll && x<w->x+w->w && y<w->y-screen.scroll+w->h && (w->kind!="label" || (drop && w->dropTarget))) return &*w;
    return nullptr;
}
void WidgetUi::event(const Event& event) { if(events_.size()<512) events_.push_back(event); }
bool WidgetUi::capturesMenuInput(const Event& event) const {
    if(event.type!=EventType::KeyDown || event.key!=Key::Escape) return false;
    if(drag_) return true;
    for(auto s=screens_.rbegin();s!=screens_.rend();++s) {
        if(s->modal) return true;
        for(const auto& w:s->widgets) if(w.id==s->focus && w.enabled && interactive(w.kind)) return true;
    }
    return false;
}
bool WidgetUi::textInput() const {
    for(auto s=screens_.rbegin();s!=screens_.rend();++s) {
        for(const auto& w:s->widgets) if(w.id==s->focus && w.kind=="input" && w.enabled) return true;
        if(s->modal) break;
    }
    return false;
}
void WidgetUi::update() {
    using namespace script;std::vector<std::vector<Value>> notifications;
    bool capture=pointerCaptured_ || std::any_of(screens_.begin(),screens_.end(),[](const auto& s){return s.modal;}) || textInput();
    const float scale=std::min(input_.width()/1280.0f,input_.height()/720.0f)*input_.uiScale();
    float px=(input_.mouseX()-(input_.width()-1280*scale)/2)/scale,py=(input_.mouseY()-(input_.height()-720*scale)/2)/scale;
    for(const auto& event:events_) {
        if(event.type==EventType::FocusLost) { if(drag_ && drag_->started) notifications.push_back({Value(drag_->screen),Value(drag_->widget),Value(std::string("drag_cancel")),Value{}});drag_.reset();pointerCaptured_=false;dragScreen_.clear();dragWidget_.clear();for(auto& s:screens_) s.focus.clear();continue; }
        if(event.type==EventType::KeyDown && event.key==Key::Escape && drag_) {
            if(drag_->started) notifications.push_back({Value(drag_->screen),Value(drag_->widget),Value(std::string("drag_cancel")),Value{}});
            drag_.reset();pointerCaptured_=false;dragScreen_.clear();dragWidget_.clear();capture=true;continue;
        }
        if(event.type==EventType::MouseUp && event.mouse==MouseButton::Left) { pointerCaptured_=false;dragScreen_.clear();dragWidget_.clear(); }
        if(event.type==EventType::MouseMove) { px=(event.x-(input_.width()-1280*scale)/2)/scale;py=(event.y-(input_.height()-720*scale)/2)/scale; }
        if(drag_ && event.type==EventType::MouseMove) {
            drag_->x=px;drag_->y=py;
            if(!drag_->started && std::hypot(px-drag_->startX,py-drag_->startY)>=4) { drag_->started=true;notifications.push_back({Value(drag_->screen),Value(drag_->widget),Value(std::string("drag_start")),clone(drag_->payload)}); }
            if(drag_->started) { capture=true;continue; }
        }
        if(drag_ && event.type==EventType::MouseUp && event.mouse==MouseButton::Left) {
            bool sourceLive=false;for(const auto& screen:screens_) if(screen.id==drag_->screen) for(const auto& w:screen.widgets) if(w.id==drag_->widget && w.enabled && w.draggable) sourceLive=true;
            if(sourceLive && drag_->started) {
                auto value=object();value.setMember("source_screen",Value(drag_->screen),{});value.setMember("source_widget",Value(drag_->widget),{});value.setMember("payload",clone(drag_->payload),{});value.setMember("x",Value(double(px)),{});value.setMember("y",Value(double(py)),{});bool accepted=false;
                for(auto screen=screens_.rbegin();screen!=screens_.rend();++screen) {
                    const auto* target=hit(*screen,px,py,true);
                    if(target && target->enabled && target->dropTarget) { notifications.push_back({Value(screen->id),Value(target->id),Value(std::string("drop")),clone(value)});accepted=true; }
                    if(target || screen->modal) break;
                }
                value.setMember("delivered",Value(accepted),{});notifications.push_back({Value(drag_->screen),Value(drag_->widget),Value(std::string("drag_end")),value});
            } else if(sourceLive) for(auto screen=screens_.rbegin();screen!=screens_.rend();++screen) {
                const auto* target=hit(*screen,px,py);
                if(target && target->enabled && screen->id==drag_->screen && target->id==drag_->widget && target->kind=="button") notifications.push_back({Value(screen->id),Value(target->id),Value(std::string("activate")),target->value});
                if(target || screen->modal) break;
            }
            drag_.reset();capture=true;continue;
        }
        for(auto s=screens_.rbegin();s!=screens_.rend();++s) {
            const bool inside=px>=s->x && py>=s->y && px<s->x+s->w && py<s->y+s->h;
            if(event.type==EventType::MouseWheel && inside) { s->scroll=std::clamp(s->scroll-int(event.y*48),0,s->contentHeight-s->h);capture=true;break; }
            if(event.type==EventType::KeyDown && event.key==Key::Escape && (s->modal || std::any_of(s->widgets.begin(),s->widgets.end(),[&](const auto& w){return w.id==s->focus && w.enabled && interactive(w.kind);}))) {
                notifications.push_back({Value(s->id),Value(s->focus),Value(std::string("cancel")),Value{}});
                if(!s->modal) s->focus.clear();capture=true;break;
            }
            if(s->modal && event.type==EventType::MouseDown && event.mouse==MouseButton::Left) pointerCaptured_=true;
            Widget* target=nullptr;bool blocked=false;
            if(event.type==EventType::MouseDown && event.mouse==MouseButton::Left && input_.hasPointer()) {
                if(auto* w=hit(*s,px,py)) { if(w->enabled && (interactive(w->kind) || w->draggable)) target=w;else { capture=true;blocked=true; } }
                if(target) { for(auto& other:screens_) if(other.id!=s->id) other.focus.clear();s->focus=target->id; }
            } else if(event.type==EventType::KeyDown && event.key==Key::Tab) {
                std::vector<Widget*> order;for(auto& w:s->widgets) if(w.enabled && interactive(w.kind)) order.push_back(&w);
                if(!order.empty()) { auto it=std::find_if(order.begin(),order.end(),[&](auto w){return w->id==s->focus;});size_t next=it==order.end()?0:size_t(it-order.begin());if(it!=order.end()) next=event.shift?(next+order.size()-1)%order.size():(next+1)%order.size();s->focus=order[next]->id;
                    const auto* focused=order[next];if(focused->y-s->scroll<s->y) s->scroll=focused->y-s->y;else if(focused->y+focused->h-s->scroll>s->y+s->h) s->scroll=focused->y+focused->h-s->y-s->h;s->scroll=std::clamp(s->scroll,0,s->contentHeight-s->h);for(auto& other:screens_) if(other.id!=s->id) other.focus.clear();capture=true;break; }
            } else {
                for(auto& w:s->widgets) if(w.id==s->focus && w.enabled) { target=&w;break; }
            }
            if(blocked) { pointerCaptured_=true;break; }
            if(target) {
                auto& w=*target;bool changed=false;std::string eventName="change";
                const bool click=event.type==EventType::MouseDown && event.mouse==MouseButton::Left;
                const bool enter=event.type==EventType::KeyDown && event.key==Key::Enter && !event.repeat;
                if(click) { pointerCaptured_=true;if(w.kind=="slider" || w.kind=="input") { dragScreen_=s->id;dragWidget_=w.id; } }
                if(click && w.draggable) drag_=Drag{s->id,w.id,w.text,clone(w.payload),px,py,px,py,false};
                if((click && !w.draggable) || enter) {
                    capture=true;
                    if(w.kind=="button") { changed=true;eventName="activate"; }
                    if(w.kind=="toggle") { w.value=Value(!w.value.boolean());changed=true; }
                    if(w.kind=="input" && enter) { changed=true;eventName="submit"; }
                }
                if(w.kind=="input") {
                    auto text=w.value.string();const auto old=text;
                    const auto previous=[&](size_t cursor) { if(!cursor) return size_t{0};--cursor;while(cursor && (static_cast<unsigned char>(text[cursor])&0xc0)==0x80) --cursor;return cursor; };
                    const auto following=[&](size_t cursor) { if(cursor>=text.size()) return text.size();++cursor;while(cursor<text.size() && (static_cast<unsigned char>(text[cursor])&0xc0)==0x80) ++cursor;return cursor; };
                    const auto erase=[&] { const auto first=std::min(w.anchor,w.cursor),last=std::max(w.anchor,w.cursor);text.erase(first,last-first);w.cursor=w.anchor=first; };
                    const auto insert=[&](std::string value) {
                        std::erase_if(value,[](unsigned char c){return c<32 || c==127;});
                        if(value.empty()) return;
                        try { (void)toData(Value(value)); }catch(const std::exception&) { return; }
                        const size_t available=1024-(text.size()-(std::max(w.anchor,w.cursor)-std::min(w.anchor,w.cursor)));
                        if(value.size()>available) { size_t end=available;while(end && (static_cast<unsigned char>(value[end])&0xc0)==0x80) --end;value.resize(end); }
                        if(value.empty()) return;
                        erase();text.insert(w.cursor,value);w.cursor+=value.size();w.anchor=w.cursor;
                    };
                    const bool dragging=event.type==EventType::MouseMove && pointerCaptured_ && dragScreen_==s->id && dragWidget_==w.id;
                    if(click || dragging) {
                        const double x=px-w.x-8+w.textOffset;auto point=w.advances.begin();for(auto it=w.advances.begin();it!=w.advances.end();++it) if(std::abs(it->second-x)<std::abs(point->second-x)) point=it;
                        w.cursor=point->first;if(click && !event.shift) w.anchor=w.cursor;capture=true;
                    }
                    if(event.type==EventType::TextInput && !event.control) insert(event.text);
                    if(event.type==EventType::KeyDown) {
                        if(event.control && event.key==Key::A) { w.anchor=0;w.cursor=text.size(); }
                        else if(event.control && (event.key==Key::C || event.key==Key::X)) {
                            if(w.anchor!=w.cursor) { try { writeClipboard(text.substr(std::min(w.anchor,w.cursor),std::max(w.anchor,w.cursor)-std::min(w.anchor,w.cursor)));if(event.key==Key::X) erase(); }catch(const std::exception& e) { LOG_WARN("UI",e.what()); } }
                        } else if(event.control && event.key==Key::V) { try { insert(readClipboard()); }catch(const std::exception& e) { LOG_WARN("UI",e.what()); } }
                        else if(event.key==Key::Left || event.key==Key::Right || event.key==Key::Home || event.key==Key::End) {
                            if(event.key==Key::Home) w.cursor=0;else if(event.key==Key::End) w.cursor=text.size();
                            else if(!event.shift && w.anchor!=w.cursor) w.cursor=event.key==Key::Left?std::min(w.anchor,w.cursor):std::max(w.anchor,w.cursor);
                            else w.cursor=event.key==Key::Left?previous(w.cursor):following(w.cursor);
                            if(!event.shift) w.anchor=w.cursor;
                        } else if(event.key==Key::Backspace || event.key==Key::Delete) {
                            if(w.anchor!=w.cursor) erase();
                            else if(event.key==Key::Backspace && w.cursor) { w.anchor=previous(w.cursor);erase(); }
                            else if(event.key==Key::Delete && w.cursor<text.size()) { w.anchor=following(w.cursor);erase(); }
                        }
                        capture=true;
                    }
                    if(text!=old) { w.value=Value(text);w.advances=textAdvances(text);changed=true; }
                    int caret=0;for(const auto& point:w.advances) if(point.first==w.cursor) caret=point.second;
                    if(caret<w.textOffset) w.textOffset=caret;if(caret>w.textOffset+std::max(1,w.w-20)) w.textOffset=caret-std::max(1,w.w-20);
                }
                if(w.kind=="slider") {
                    double value=w.value.number();
                    if(click || (event.type==EventType::MouseMove && pointerCaptured_ && dragScreen_==s->id && dragWidget_==w.id)) value=w.minimum+(w.maximum-w.minimum)*std::clamp((px-w.x)/w.w,0.0f,1.0f);
                    if(event.type==EventType::KeyDown && event.key==Key::Left) value-=w.step;
                    if(event.type==EventType::KeyDown && event.key==Key::Right) value+=w.step;
                    value=std::clamp(w.minimum+std::round((value-w.minimum)/w.step)*w.step,w.minimum,w.maximum);
                    if(value!=w.value.number()) { w.value=Value(value);changed=true;capture=true; }
                }
                if(changed) notifications.push_back({Value(s->id),Value(w.id),Value(eventName),w.value});
                if(click || enter || changed || ((event.type==EventType::TextInput || event.type==EventType::KeyDown || event.type==EventType::MouseMove) && w.kind=="input")) break;
            }
            if(s->modal) break;
        }
    }
    events_.clear();if(capture || pointerCaptured_ || textInput()) input_.captureGameplay();
    // Delivery after traversal allows callbacks to remove/replace their screens.
    for(const auto& event:notifications) for(const auto& d:runtime_.emit("on_ui",event)) LOG_ERROR("SCRIPT",DiagnosticError(d).what());
}
TextureHandle WidgetUi::render(Renderer& renderer,const std::vector<CanvasCommand>& background,const std::vector<CanvasCommand>& foreground) {
    if(screens_.empty() && foreground.empty()) { texture_.reset();rendered_.clear();return {}; }
    auto commands=background;
    for(const auto& s:screens_) {
        if(s.modal) { CanvasCommand c;c.kind="rect";c.w=1280;c.h=720;c.color={0,0,0,160};commands.push_back(c); }
        for(const auto& w:s.widgets) {
            CanvasCommand c;c.kind="rect";c.x=w.x;c.y=w.y-s.scroll;c.w=w.w;c.h=w.h;c.clipX=s.x;c.clipY=s.y;c.clipW=s.w;c.clipH=s.h;
            c.color=w.enabled?(s.focus==w.id?std::array<unsigned char,4>{58,108,140,255}:std::array<unsigned char,4>{32,43,55,245}):std::array<unsigned char,4>{40,40,40,210};
            if(w.kind!="label") commands.push_back(c);
            if(w.kind=="slider") { auto bar=c;bar.y+=bar.h-5;bar.h=5;bar.w=int(bar.w*(w.value.number()-w.minimum)/(w.maximum-w.minimum));bar.color={105,207,160,255};commands.push_back(bar); }
            if(w.kind=="input" && s.focus==w.id) {
                int start=0,end=0;for(const auto& point:w.advances) { if(point.first==std::min(w.anchor,w.cursor)) start=point.second;if(point.first==std::max(w.anchor,w.cursor)) end=point.second; }
                auto selection=c;selection.x+=8+start-w.textOffset;selection.y+=(w.h-24)/2;selection.w=end-start;selection.h=24;selection.clipX=std::max(s.x,w.x+8);selection.clipW=std::max(0,std::min(s.x+s.w,w.x+w.w-8)-selection.clipX);selection.color={58,126,184,255};if(selection.w) commands.push_back(selection);
            }
            if(w.kind!="panel") {
                c.kind="text";c.x+=8;c.w=std::max(0,c.w-16);c.size=20;c.align="left";c.color=w.enabled?std::array<unsigned char,4>{245,245,245,255}:std::array<unsigned char,4>{150,150,150,255};c.text=w.text;
                if(w.kind=="toggle") c.text=(w.value.boolean()?"[x] ":"[ ] ")+c.text;
                if(w.kind=="input") { c.text=w.value.string();c.clipX=std::max(s.x,w.x+8);c.clipW=std::max(0,std::min(s.x+s.w,w.x+w.w-8)-c.clipX);c.x-=w.textOffset;c.w+=w.textOffset; }
                if(w.kind=="slider") c.text+=" "+std::to_string(w.value.number());commands.push_back(c);
            }
            if(w.kind=="input" && s.focus==w.id) {
                int offset=0;for(const auto& point:w.advances) if(point.first==w.cursor) offset=point.second;
                c.kind="rect";c.x=w.x+8+offset-w.textOffset;c.y=w.y-s.scroll+(w.h-24)/2;c.w=1;c.h=24;c.color={230,240,255,255};commands.push_back(c);
            }
        }
        if(s.contentHeight>s.h) { CanvasCommand bar;bar.kind="rect";bar.x=s.x+s.w-4;bar.w=4;bar.h=std::max(8,s.h*s.h/s.contentHeight);bar.y=s.y+(s.h-bar.h)*s.scroll/(s.contentHeight-s.h);bar.color={115,145,170,240};commands.push_back(bar); }
    }
    if(drag_ && drag_->started) { CanvasCommand ghost;ghost.kind="rect";ghost.x=int(drag_->x)+12;ghost.y=int(drag_->y)+12;ghost.w=180;ghost.h=36;ghost.color={60,110,150,230};commands.push_back(ghost);ghost.kind="text";ghost.text=drag_->label;ghost.size=18;ghost.align="center";ghost.color={255,255,255,255};commands.push_back(ghost); }
    commands.insert(commands.end(),foreground.begin(),foreground.end());
    if(!texture_ || commands!=rendered_) { auto next=renderer.createTexture(1280,720,canvasImage(commands),false,false);texture_=std::move(next);rendered_=std::move(commands); }
    renderer.setCanvasOverlay(true);return texture_;
}
}
