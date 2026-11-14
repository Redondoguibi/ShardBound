#include "game/DeveloperMode.h"
#include "platform/WindowSystem.h"
#include "scripting/DataConversion.h"
#include <algorithm>
#include <cmath>
namespace game {
namespace {
size_t previous(const std::string& text,size_t at) { if(!at) return 0;--at;while(at && (static_cast<unsigned char>(text[at])&0xc0)==0x80) --at;return at; }
size_t next(const std::string& text,size_t at) { if(at>=text.size()) return text.size();++at;while(at<text.size() && (static_cast<unsigned char>(text[at])&0xc0)==0x80) ++at;return at; }
Vec3 direction(float yaw,float pitch) { return {std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch)}; }
int advance(const std::vector<std::pair<size_t,int>>& values,size_t cursor) { for(const auto& [at,x]:values) if(at==cursor) return x;return 0; }
}
DeveloperMode::DeveloperMode(script::Runtime& runtime,World& world,PhysicsSystem& physics,PlayerController& player,InputSystem& input):commands_(runtime,world,physics,player),world_(world),physics_(physics),player_(player),input_(input) {
    commands_.changeCamera=[this](const auto& mode){setCamera(mode);};readClipboard=clipboardText;writeClipboard=setClipboardText;
    player_.cameraOverride=[this] {return camera();};
}
DeveloperMode::~DeveloperMode() {player_.cameraOverride={};player_.movementHeading({});}
void DeveloperMode::sensitivity(float value) {if(!std::isfinite(value)||value<.1f||value>4) throw std::invalid_argument("Camera sensitivity must be in [0.1,4]");sensitivity_=value;}
void DeveloperMode::terrain(ChunkWorld* value,std::string seed) { terrain_=value;originX_=value?value->originX():0;originZ_=value?value->originZ():0;commands_.terrain(value,std::move(seed)); }
void DeveloperMode::enable(bool value) {
    enabled_=value;closeConsole();keys_.clear();looking_=false;
    if(!enabled_) { mode_="default";player_.movementHeading({});pending_.clear(); }
    print(enabled_?"Modo de desenvolvedor ativado. / abre o console; /help mostra os comandos.":"Modo de desenvolvedor desativado.");
}
void DeveloperMode::closeConsole() { console_=false;openingSlash_=false;looking_=false;keys_.clear();tabChoices_.clear(); }
void DeveloperMode::syncOrigin() {
    if(!terrain_) return;freeEye_.x=std::clamp(freeEye_.x-float(terrain_->originX()-originX_),-100000.0f,100000.0f);freeEye_.z=std::clamp(freeEye_.z-float(terrain_->originZ()-originZ_),-100000.0f,100000.0f);originX_=terrain_->originX();originZ_=terrain_->originZ();
}
void DeveloperMode::setCamera(const std::string& mode) {
    if(!enabled_) throw std::runtime_error("Ative o modo de desenvolvedor primeiro");
    if(mode!="default" && mode!="thirdperson" && mode!="free") throw std::runtime_error("Modo de câmera inválido");
    syncOrigin();const auto current=camera();
    if(mode_=="default") {
        const auto x=current.target.x-current.eye.x,z=current.target.z-current.eye.z;const auto horizontal=std::hypot(x,z);
        yaw_=std::atan2(x,z);pitch_=mode=="thirdperson"?.35f:std::clamp(std::atan2(current.eye.y-current.target.y,horizontal),-.2f,1.3f);
    }
    if(mode=="free" && mode_!="free") freeEye_=current.eye;
    mode_=mode;looking_=false;keys_.clear();player_.movementHeading(mode_=="thirdperson"?std::optional<float>(yaw_):std::nullopt);
}
void DeveloperMode::changed() { lineAdvances_=textAdvances(line_,20);suggestions_=commands_.complete(line_.substr(0,cursor_));tabChoices_.clear();scroll_=0; }
void DeveloperMode::insert(const std::string& text) {
    if(text.empty()) return;
    try {
        (void)script::toData(script::Value(text));if(std::any_of(text.begin(),text.end(),[](unsigned char c){return c<32 || c==127;})) throw std::runtime_error("Use apenas uma linha no comando");
        const auto low=std::min(cursor_,anchor_),high=std::max(cursor_,anchor_);if(line_.size()-(high-low)+text.size()>1024) throw std::runtime_error("Limite de 1024 bytes no comando");
        line_.replace(low,high-low,text);cursor_=anchor_=low+text.size();changed();
    }catch(const std::exception& e) { print(std::string("Erro: ")+e.what()); }
}
void DeveloperMode::print(const std::string& raw) {
    // Bound history even when a mod emits a long result. Wrap UTF-8 without splitting code points.
    std::string text;try { (void)script::toData(script::Value(raw));text=raw; }catch(const std::exception&) {text="Erro: a saída do comando contém texto inválido.";}
    size_t limit=std::min<size_t>(text.size(),65536);while(limit<text.size() && limit && (static_cast<unsigned char>(text[limit])&0xc0)==0x80) --limit;size_t begin=0;
    while(begin<limit) {
        auto end=text.find('\n',begin);if(end==std::string::npos || end>limit) end=limit;
        size_t measured=begin;for(unsigned i=0;i<256 && measured<end;++i) measured=next(text,measured);
        const auto points=textAdvances(text.substr(begin,measured-begin),18);size_t count=0;
        for(const auto& [offset,width]:points) { if(width>1210) break;count=offset; }
        if(count==0 && end>begin) count=next(text,begin)-begin;
        if(end==begin) { output_.emplace_back();if(output_.size()>2048) output_.pop_front();++begin;continue; }
        output_.push_back(text.substr(begin,count));begin+=count;if(begin==end && begin<limit && text[begin]=='\n') ++begin;
        while(output_.size()>2048) output_.pop_front();
    }
    if(text.size()>limit) output_.push_back("Saída truncada no limite de 65536 bytes.");while(output_.size()>2048) output_.pop_front();scroll_=0;messageTime_=6;
}
void DeveloperMode::recall(bool older) {
    if(history_.empty()) return;if(historyIndex_==history_.size()) draft_=line_;
    if(older && historyIndex_>0) --historyIndex_;else if(!older && historyIndex_<history_.size()) ++historyIndex_;
    line_=historyIndex_==history_.size()?draft_:history_[historyIndex_];cursor_=anchor_=line_.size();changed();
}
void DeveloperMode::complete(bool backwards) {
    if(tabChoices_.empty()) { tabChoices_=commands_.complete(line_.substr(0,cursor_));tabIndex_=backwards && !tabChoices_.empty()?tabChoices_.size()-1:0; }
    else tabIndex_=backwards?(tabIndex_+tabChoices_.size()-1)%tabChoices_.size():(tabIndex_+1)%tabChoices_.size();
    if(tabChoices_.empty()) return;
    auto suffix=line_.substr(cursor_);if(tabChoices_[tabIndex_].size()+suffix.size()>1024) {print("Erro: limite de 1024 bytes no comando.");return;}
    line_=tabChoices_[tabIndex_]+suffix;cursor_=anchor_=tabChoices_[tabIndex_].size();suggestions_=tabChoices_;lineAdvances_=textAdvances(line_,20);
}
bool DeveloperMode::event(const Event& e,bool gameplay,bool otherTextInput) {
    if(e.type==EventType::FocusLost || e.type==EventType::Minimized) { up_=down_=chord_=false;closeConsole();return false; }
    if(!gameplay) { up_=down_=chord_=false;closeConsole();return false; }
    if(e.type==EventType::KeyDown || e.type==EventType::KeyUp) {
        const bool pressed=e.type==EventType::KeyDown;
        if(e.key==Key::PageUp) up_=pressed;if(e.key==Key::PageDown) down_=pressed;
        if(!up_ || !down_) chord_=false;
        else if(pressed && !e.repeat && !chord_) { chord_=true;enable(!enabled_);input_.captureGameplay();return true; }
        if(pressed) keys_.insert(e.key);else { keys_.erase(e.key);if(e.key==Key::Slash) openingSlash_=false; }
    }
    if(!enabled_) return false;
    if(!console_ && !otherTextInput && e.type==EventType::KeyDown && e.key==Key::Slash && !e.repeat && !e.control) {
        console_=true;openingSlash_=true;looking_=false;keys_.clear();line_="/";cursor_=anchor_=1;historyIndex_=history_.size();draft_=line_;changed();input_.captureGameplay();return true;
    }
    if(console_) {
        input_.captureGameplay();
        if(e.type==EventType::TextInput) { auto text=e.text;if(openingSlash_ && text.starts_with('/')) text.erase(0,1);openingSlash_=false;insert(text); }
        if(e.type==EventType::MouseWheel) { const auto amount=size_t(std::min(100.0f,std::abs(e.y)*3));if(e.y>0) scroll_=std::min(output_.size(),scroll_+amount);else scroll_=scroll_>amount?scroll_-amount:0; }
        if(e.type!=EventType::KeyDown) return true;
        if(e.key==Key::Escape) { closeConsole();return true; }
        if(e.key==Key::Enter && !e.repeat) {
            if(line_.size()>1 && pending_.size()<8) { pending_.push_back(line_);if(history_.empty() || history_.back()!=line_) history_.push_back(line_);if(history_.size()>128) history_.erase(history_.begin()); }
            closeConsole();return true;
        }
        if(e.key==Key::Tab) { complete(e.shift);return true; }
        if(e.key==Key::Up) { recall(true);return true; }if(e.key==Key::Down) { recall(false);return true; }
        if(e.control && e.key==Key::A) { anchor_=0;cursor_=line_.size(); }
        else if(e.control && (e.key==Key::C || e.key==Key::X)) {
            const auto low=std::min(cursor_,anchor_),high=std::max(cursor_,anchor_);
            if(high>low) try { writeClipboard(line_.substr(low,high-low));if(e.key==Key::X) {line_.erase(low,high-low);cursor_=anchor_=low;} }catch(const std::exception& error) {print(std::string("Erro: ")+error.what());}
        }else if(e.control && e.key==Key::V) { try {insert(readClipboard());}catch(const std::exception& error) {print(std::string("Erro: ")+error.what());} }
        else if(e.key==Key::Left || e.key==Key::Right || e.key==Key::Home || e.key==Key::End) {
            if(!e.shift && cursor_!=anchor_ && (e.key==Key::Left || e.key==Key::Right)) cursor_=e.key==Key::Left?std::min(cursor_,anchor_):std::max(cursor_,anchor_);
            else cursor_=e.key==Key::Home?0:e.key==Key::End?line_.size():e.key==Key::Left?previous(line_,cursor_):next(line_,cursor_);
            if(!e.shift) anchor_=cursor_;
        }else if(e.key==Key::Backspace || e.key==Key::Delete) {
            auto low=std::min(cursor_,anchor_),high=std::max(cursor_,anchor_);if(low==high) {if(e.key==Key::Backspace) low=previous(line_,low);else high=next(line_,high);}line_.erase(low,high-low);cursor_=anchor_=low;
        }
        changed();return true;
    }
    if(mode_!="default" && !otherTextInput) {
        if(e.type==EventType::KeyDown && (e.key==Key::Escape || e.key==Key::F3)) return false;
        if(e.type==EventType::MouseDown && e.mouse==MouseButton::Right) { looking_=true;input_.captureGameplay();return true; }
        if(e.type==EventType::MouseUp && e.mouse==MouseButton::Right) { looking_=false;return true; }
        if(e.type==EventType::MouseMove && looking_) {
            if(std::isfinite(e.dx) && std::isfinite(e.dy)) { yaw_=std::remainder(yaw_+e.dx*.004f*sensitivity_,6.2831853f);pitch_=std::clamp(pitch_+e.dy*.004f*sensitivity_,mode_=="free"?-1.5f:-.2f,mode_=="free"?1.5f:1.3f); }return true;
        }
        if(e.type==EventType::MouseWheel && mode_=="thirdperson" && std::isfinite(e.y)) {distance_=std::clamp(distance_-e.y*.5f,1.5f,16.0f);return true;}
        if(mode_=="free") {input_.captureGameplay();return true;}
    }
    return false;
}
void DeveloperMode::processCommands() { while(enabled_ && !pending_.empty()) {const auto line=std::move(pending_.front());pending_.pop_front();print("> "+line);print(commands_.execute(line));} }
void DeveloperMode::update(float delta,bool gameplay,bool otherInput) {
    if(std::isfinite(delta) && delta>0) messageTime_=std::max(0.0f,messageTime_-delta);
    syncOrigin();if(!gameplay) {up_=down_=chord_=false;keys_.clear();looking_=false;closeConsole();player_.movementHeading({});return;}
    if(!enabled_) return;
    if(console_ || mode_=="free") input_.captureGameplay();
    player_.movementHeading(mode_=="thirdperson"?std::optional<float>(yaw_):std::nullopt);
    if(otherInput && !console_) { keys_.clear();looking_=false;return; }
    if(mode_!="free" || console_ || !std::isfinite(delta) || delta<=0) return;
    const auto held=[&](Key k){return keys_.contains(k)?1.0f:0.0f;};const auto f=held(Key::W)-held(Key::S),r=held(Key::D)-held(Key::A),u=held(Key::Space)-held(Key::Q);
    const auto d=direction(yaw_,pitch_);Vec3 velocity{d.x*f+std::cos(yaw_)*r,d.y*f+u,d.z*f-std::sin(yaw_)*r};
    const auto length=std::max(1.0f,std::sqrt(velocity.x*velocity.x+velocity.y*velocity.y+velocity.z*velocity.z));const auto step=std::min(delta,.1f)*(keys_.contains(Key::Shift)?40.0f:10.0f)/length;
    freeEye_.x=std::clamp(freeEye_.x+velocity.x*step,-100000.0f,100000.0f);freeEye_.y=std::clamp(freeEye_.y+velocity.y*step,-100000.0f,100000.0f);freeEye_.z=std::clamp(freeEye_.z+velocity.z*step,-100000.0f,100000.0f);
}
Camera DeveloperMode::camera() {
    syncOrigin();if(!enabled_ || mode_=="default" || !player_.entity()) return player_.camera();
    const auto d=direction(yaw_,pitch_);Camera result;
    if(mode_=="free") {result.eye=freeEye_;result.target={freeEye_.x+d.x,freeEye_.y+d.y,freeEye_.z+d.z};return result;}
    auto p=world_.get<TransformComponent>(player_.entity()).local.position;p.y+=.5f;result.target=p;result.eye={p.x-d.x*distance_,p.y-d.y*distance_,p.z-d.z*distance_};
    if(auto hit=physics_.raycast(result.target,result.eye,{player_.entity()})) {
        const auto fraction=std::max(.01f,hit->fraction-.25f/distance_);result.eye={p.x-d.x*distance_*fraction,p.y-d.y*distance_*fraction,p.z-d.z*distance_*fraction};
    }
    return result;
}
std::optional<Vec3> DeveloperMode::streamingCenter() { syncOrigin();return enabled_ && mode_=="free"?std::optional<Vec3>(freeEye_):std::nullopt; }
std::vector<CanvasCommand> DeveloperMode::overlay() const {
    std::vector<CanvasCommand> commands;
    const auto rect=[&](int x,int y,int w,int h,std::array<unsigned char,4> color) {CanvasCommand c;c.kind="rect";c.x=x;c.y=y;c.w=w;c.h=h;c.color=color;commands.push_back(std::move(c));};
    const auto text=[&](std::string value,int x,int y,int w=1232) {CanvasCommand c;c.kind="text";c.x=x;c.y=y;c.w=w;c.h=24;c.size=18;c.text=std::move(value);c.align="left";commands.push_back(std::move(c));};
    if(!enabled_) return commands;
    rect(12,12,620,32,{12,20,28,235});text("DEV | / comandos | câmera: "+mode_+" | Page Up + Page Down: sair",22,16,600);
    if(!console_) {
        if(messageTime_>0 && !output_.empty()) {const auto begin=output_.size()>3?output_.size()-3:0;int y=52;for(auto i=begin;i<output_.size();++i) {rect(12,y,1256,26,{10,16,24,220});text(output_[i],24,y);y+=26;} }
        return commands;
    }
    rect(12,292,1256,416,{10,16,24,245});const auto end=output_.size()-std::min(scroll_,output_.size());const auto begin=end>11?end-11:0;
    int y=304;for(auto i=begin;i<end;++i) {text(output_[i],24,y);y+=24;}
    text("Tab: completar | ↑ ↓: histórico | Esc: fechar",24,574);
    for(size_t i=0;i<std::min<size_t>(2,suggestions_.size());++i) text(suggestions_[i],24,602+int(i)*24);
    rect(20,660,1240,36,{28,40,52,255});const int caret=advance(lineAdvances_,cursor_),offset=std::max(0,caret-1200);
    const auto low=advance(lineAdvances_,std::min(cursor_,anchor_)),high=advance(lineAdvances_,std::max(cursor_,anchor_));
    if(high>low) {rect(28+low-offset,664,high-low,28,{58,126,184,255});auto& c=commands.back();c.clipX=28;c.clipY=660;c.clipW=1220;c.clipH=36;}
    text(line_,28-offset,664,1220+offset);auto& c=commands.back();c.size=20;c.clipX=28;c.clipY=660;c.clipW=1220;c.clipH=36;
    rect(28+caret-offset,666,1,24,{245,245,245,255});return commands;
}
TextureHandle DeveloperMode::render(Renderer& renderer) {
    if(!enabled_) {texture_.reset();rendered_.clear();return {};}
    auto commands=overlay();if(!texture_ || commands!=rendered_) { auto texture=renderer.createTexture(1280,720,canvasImage(commands),false,false);texture_=std::move(texture);rendered_=std::move(commands); }return texture_;
}
}
