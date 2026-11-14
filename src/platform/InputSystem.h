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
    bool operator==(const InputBinding&) const = default;
};
class InputSystem {
public:
    InputSystem();
    void beginFrame();
    void captureGameplay() { captured_=true; }
    void process(const Event& event);
    void bind(std::string action, std::vector<InputBinding> bindings);
    void overrides(std::map<std::string,std::vector<InputBinding>,std::less<>> bindings);
    std::map<std::string,std::vector<InputBinding>,std::less<>> bindings() const;
    void uiScale(float value);
    float uiScale() const { return uiScale_; }
    bool held(std::string_view action) const;
    bool pressed(std::string_view action) const;
    bool released(std::string_view action) const;
    float moveX() const;
    float moveY() const;
    void keyboardMovementOnly(bool value) { keyboardOnly_=value; }
    void setViewport(int width,int height);
    int width() const { return width_; }
    int height() const { return height_; }
    bool hasPointer() const { return pointer_ && focused_; }
    bool pointerPressed() const { return focused_ && mouse_[size_t(MouseButton::Left)].pressed; }
    float mouseX() const { return mouseX_; }
    float mouseY() const { return mouseY_; }
private:
    struct Button { bool down = false, pressed = false, released = false; };
    struct PadState { std::array<Button, static_cast<size_t>(PadButton::Count)> buttons{}; float x = 0, y = 0; };
    std::array<Button, static_cast<size_t>(Key::Count)> keys_{};
    std::array<Button, static_cast<size_t>(MouseButton::Count)> mouse_{};
    std::map<std::uint32_t, PadState> pads_;
    std::map<std::string, std::vector<InputBinding>, std::less<>> bindings_;
    std::map<std::string, std::vector<InputBinding>, std::less<>> overrides_;
    float uiScale_=1;
    bool query(std::string_view action, bool Button::* field) const;
    static void set(Button& button, bool down);
    bool captured_=false;
    bool focused_ = true;
    bool keyboardOnly_ = false;
    float mouseX_ = 0, mouseY_ = 0;
    int width_=1280,height_=720;
    bool pointer_=false;
};
}
