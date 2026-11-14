#include "platform/InputSystem.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace game {
namespace {
const std::vector<std::string> keyNames={"Unknown","W","A","S","D","Space","E","Escape","M","I","Q","1","2","3","F5","F9","Enter","Backspace","Tab","Left","Right","Up","Down","F3","C","V","X","Delete","Home","End","PageUp","PageDown","Slash","Shift","B","F","G","H","J","K","L","N","O","P","R","T","U","Y","Z","0","4","5","6","7","8","9","F1","F2","F4","F6","F7","F8","F10","F11","F12","Control","Alt"};
}
std::string keyName(Key key) { return keyNames.at(size_t(key)); }
Key keyFromName(const std::string& name) {const auto i=std::find(keyNames.begin(),keyNames.end(),name);if(i==keyNames.end()) throw std::runtime_error("Unknown key: "+name);return Key(i-keyNames.begin());}
void InputSystem::overrides(std::map<std::string,std::vector<InputBinding>,std::less<>> bindings) { overrides_=std::move(bindings); }
std::map<std::string,std::vector<InputBinding>,std::less<>> InputSystem::bindings() const {auto result=bindings_;for(const auto& [action,values]:overrides_) result[action]=values;return result;}
void InputSystem::uiScale(float value) {if(!std::isfinite(value)||value<.75f||value>1) throw std::invalid_argument("UI scale must be in [0.75,1]");uiScale_=value;}
InputSystem::InputSystem() {
    bind("MoveForward", {{Key::W}}); bind("MoveBackward", {{Key::S}});
    bind("MoveLeft", {{Key::A}}); bind("MoveRight", {{Key::D}});
    bind("Attack", {{Key::Unknown, MouseButton::Left, PadButton::West}});
    bind("Dodge", {{Key::Space, MouseButton::Unknown, PadButton::South}});
    bind("Interact", {{Key::E, MouseButton::Unknown, PadButton::North}});
    bind("Quit", {{Key::Escape}});
    bind("Jump", {{Key::Space}}); bind("Map", {{Key::M}}); bind("Inventory", {{Key::I}});
    bind("Equip", {{Key::Q}}); bind("Travel1", {{Key::One}}); bind("Travel2", {{Key::Two}}); bind("Travel3", {{Key::Three}});
    bind("Save", {{Key::F5}}); bind("Load", {{Key::F9}});
}
void InputSystem::beginFrame() {
    captured_=false;
    auto clear = [](auto& buttons) { for (auto& b : buttons) b.pressed = b.released = false; };
    clear(keys_); clear(mouse_);
    for (auto& [id, pad] : pads_) { (void)id; clear(pad.buttons); }
}
void InputSystem::set(Button& button, bool down) {
    if (button.down == down) return;
    button.down = down;
    (down ? button.pressed : button.released) = true;
}
void InputSystem::process(const Event& e) {
    if(e.type==EventType::Resized) setViewport(e.width,e.height);
    if (e.type == EventType::FocusLost) {
        focused_ = false;
        for (auto& b : keys_) set(b, false);
        for (auto& b : mouse_) set(b, false);
        for (auto& [id, pad] : pads_) { (void)id; for (auto& b : pad.buttons) set(b, false); pad.x = pad.y = 0; }
    } else if (e.type == EventType::FocusGained) focused_ = true;
    else if (e.type == EventType::PadConnected) pads_.try_emplace(e.device);
    else if (e.type == EventType::PadDisconnected) pads_.erase(e.device);
    else if (focused_) {
        if (!e.repeat && (e.type == EventType::KeyDown || e.type == EventType::KeyUp)) set(keys_.at(static_cast<size_t>(e.key)), e.type == EventType::KeyDown);
        if (e.type == EventType::MouseDown || e.type == EventType::MouseUp) set(mouse_.at(static_cast<size_t>(e.mouse)), e.type == EventType::MouseDown);
        if (e.type == EventType::MouseMove) { mouseX_ = e.x; mouseY_ = e.y; pointer_=true; }
        if (e.type == EventType::PadDown || e.type == EventType::PadUp) set(pads_[e.device].buttons.at(static_cast<size_t>(e.pad)), e.type == EventType::PadDown);
        if (e.type == EventType::PadAxis) { pads_[e.device].x = e.x; pads_[e.device].y = e.y; }
    }
}
void InputSystem::setViewport(int width,int height) { if(width>0 && height>0) { width_=width; height_=height; } }
void InputSystem::bind(std::string action, std::vector<InputBinding> bindings) { bindings_.insert_or_assign(std::move(action), std::move(bindings)); }
bool InputSystem::query(std::string_view action, bool Button::* field) const {
    if(captured_) return false;
    const auto override=overrides_.find(action);const auto it=bindings_.find(action);
    if(override==overrides_.end() && it==bindings_.end()) return false;
    for (const auto& b : override!=overrides_.end()?override->second:it->second) {
        if (b.key != Key::Unknown && keys_.at(static_cast<size_t>(b.key)).*field) return true;
        if (b.mouse != MouseButton::Unknown && mouse_.at(static_cast<size_t>(b.mouse)).*field) return true;
        if (b.pad != PadButton::Unknown) for (const auto& [id, pad] : pads_) {
            (void)id; if (pad.buttons.at(static_cast<size_t>(b.pad)).*field) return true;
        }
    }
    return false;
}
bool InputSystem::held(std::string_view action) const { return query(action, &Button::down); }
bool InputSystem::pressed(std::string_view action) const { return query(action, &Button::pressed); }
bool InputSystem::released(std::string_view action) const { return query(action, &Button::released); }
float InputSystem::moveX() const {
    if(captured_) return 0;
    float value = float(held("MoveRight")) - float(held("MoveLeft"));
    if(!keyboardOnly_) for (const auto& [id, pad] : pads_) { (void)id; if (std::abs(pad.x) > 0.15f) value += pad.x; }
    return std::clamp(value, -1.0f, 1.0f);
}
float InputSystem::moveY() const {
    if(captured_) return 0;
    float value = float(held("MoveForward")) - float(held("MoveBackward"));
    if(!keyboardOnly_) for (const auto& [id, pad] : pads_) { (void)id; if (std::abs(pad.y) > 0.15f) value -= pad.y; }
    return std::clamp(value, -1.0f, 1.0f);
}
}
