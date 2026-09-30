#include "platform/InputSystem.h"
#include <algorithm>
#include <cmath>
namespace game {
InputSystem::InputSystem() {
    bind("MoveForward", {{Key::W}}); bind("MoveBackward", {{Key::S}});
    bind("MoveLeft", {{Key::A}}); bind("MoveRight", {{Key::D}});
    bind("Attack", {{Key::Unknown, MouseButton::Left, PadButton::West}});
    bind("Dodge", {{Key::Space, MouseButton::Unknown, PadButton::South}});
    bind("Interact", {{Key::E, MouseButton::Unknown, PadButton::North}});
    bind("Quit", {{Key::Escape}});
}
void InputSystem::beginFrame() {
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
    if (e.type == EventType::FocusLost) {
        focused_ = false;
        for (auto& b : keys_) set(b, false);
        for (auto& b : mouse_) set(b, false);
        for (auto& [id, pad] : pads_) { (void)id; for (auto& b : pad.buttons) set(b, false); pad.x = pad.y = 0; }
    } else if (e.type == EventType::FocusGained) focused_ = true;
    else if (e.type == EventType::PadConnected) pads_.try_emplace(e.device);
    else if (e.type == EventType::PadDisconnected) pads_.erase(e.device);
    else if (focused_) {
        if (e.type == EventType::KeyDown || e.type == EventType::KeyUp) set(keys_.at(static_cast<size_t>(e.key)), e.type == EventType::KeyDown);
        if (e.type == EventType::MouseDown || e.type == EventType::MouseUp) set(mouse_.at(static_cast<size_t>(e.mouse)), e.type == EventType::MouseDown);
        if (e.type == EventType::MouseMove) { mouseX_ = e.x; mouseY_ = e.y; }
        if (e.type == EventType::PadDown || e.type == EventType::PadUp) set(pads_[e.device].buttons.at(static_cast<size_t>(e.pad)), e.type == EventType::PadDown);
        if (e.type == EventType::PadAxis) { pads_[e.device].x = e.x; pads_[e.device].y = e.y; }
    }
}
void InputSystem::bind(std::string action, std::vector<InputBinding> bindings) { bindings_.insert_or_assign(std::move(action), std::move(bindings)); }
bool InputSystem::query(std::string_view action, bool Button::* field) const {
    const auto it = bindings_.find(action);
    if (it == bindings_.end()) return false;
    for (const auto& b : it->second) {
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
    float value = float(held("MoveRight")) - float(held("MoveLeft"));
    for (const auto& [id, pad] : pads_) { (void)id; if (std::abs(pad.x) > 0.15f) value += pad.x; }
    return std::clamp(value, -1.0f, 1.0f);
}
float InputSystem::moveY() const {
    float value = float(held("MoveForward")) - float(held("MoveBackward"));
    for (const auto& [id, pad] : pads_) { (void)id; if (std::abs(pad.y) > 0.15f) value -= pad.y; }
    return std::clamp(value, -1.0f, 1.0f);
}
}
