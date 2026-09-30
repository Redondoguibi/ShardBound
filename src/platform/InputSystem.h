#pragma once
#include "platform/Events.h"
#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace game {
struct InputBinding {
    Key key = Key::Unknown;
    MouseButton mouse = MouseButton::Unknown;
    PadButton pad = PadButton::Unknown;
};
class InputSystem {
public:
    InputSystem();
    void beginFrame();
    void process(const Event& event);
    void bind(std::string action, std::vector<InputBinding> bindings);
    bool held(std::string_view action) const;
    bool pressed(std::string_view action) const;
    bool released(std::string_view action) const;
    float moveX() const;
    float moveY() const;
    float mouseX() const { return mouseX_; }
    float mouseY() const { return mouseY_; }
private:
    struct Button { bool down = false, pressed = false, released = false; };
    struct PadState { std::array<Button, static_cast<size_t>(PadButton::Count)> buttons{}; float x = 0, y = 0; };
    std::array<Button, static_cast<size_t>(Key::Count)> keys_{};
    std::array<Button, static_cast<size_t>(MouseButton::Count)> mouse_{};
    std::map<std::uint32_t, PadState> pads_;
    std::map<std::string, std::vector<InputBinding>, std::less<>> bindings_;
    bool query(std::string_view action, bool Button::* field) const;
    static void set(Button& button, bool down);
    bool focused_ = true;
    float mouseX_ = 0, mouseY_ = 0;
};
}
