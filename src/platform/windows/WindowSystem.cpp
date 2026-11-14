#include "platform/WindowSystem.h"
#include "core/Log.h"
#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <map>
#include <stdexcept>
namespace game {
std::string clipboardText() { char* value=SDL_GetClipboardText();if(!value) throw std::runtime_error(SDL_GetError());std::string result(value);SDL_free(value);return result; }
void setClipboardText(const std::string& text) { if(!SDL_SetClipboardText(text.c_str())) throw std::runtime_error(SDL_GetError()); }
namespace {
Key keyFrom(SDL_Scancode key) {
    switch (key) {
    case SDL_SCANCODE_B: return Key::B;case SDL_SCANCODE_F: return Key::F;case SDL_SCANCODE_G: return Key::G;case SDL_SCANCODE_H: return Key::H;case SDL_SCANCODE_J: return Key::J;case SDL_SCANCODE_K: return Key::K;case SDL_SCANCODE_L: return Key::L;case SDL_SCANCODE_N: return Key::N;case SDL_SCANCODE_O: return Key::O;case SDL_SCANCODE_P: return Key::P;case SDL_SCANCODE_R: return Key::R;case SDL_SCANCODE_T: return Key::T;case SDL_SCANCODE_U: return Key::U;case SDL_SCANCODE_Y: return Key::Y;case SDL_SCANCODE_Z: return Key::Z;
    case SDL_SCANCODE_0: return Key::Zero;case SDL_SCANCODE_4: return Key::Four;case SDL_SCANCODE_5: return Key::Five;case SDL_SCANCODE_6: return Key::Six;case SDL_SCANCODE_7: return Key::Seven;case SDL_SCANCODE_8: return Key::Eight;case SDL_SCANCODE_9: return Key::Nine;
    case SDL_SCANCODE_F1: return Key::F1;case SDL_SCANCODE_F2: return Key::F2;case SDL_SCANCODE_F4: return Key::F4;case SDL_SCANCODE_F6: return Key::F6;case SDL_SCANCODE_F7: return Key::F7;case SDL_SCANCODE_F8: return Key::F8;case SDL_SCANCODE_F10: return Key::F10;case SDL_SCANCODE_F11: return Key::F11;case SDL_SCANCODE_F12: return Key::F12;
    case SDL_SCANCODE_LCTRL:case SDL_SCANCODE_RCTRL: return Key::Control;case SDL_SCANCODE_LALT:case SDL_SCANCODE_RALT: return Key::Alt;
    case SDL_SCANCODE_W: return Key::W; case SDL_SCANCODE_A: return Key::A;
    case SDL_SCANCODE_S: return Key::S; case SDL_SCANCODE_D: return Key::D;
    case SDL_SCANCODE_E: return Key::E; case SDL_SCANCODE_SPACE: return Key::Space;
    case SDL_SCANCODE_M: return Key::M; case SDL_SCANCODE_I: return Key::I; case SDL_SCANCODE_Q: return Key::Q;
    case SDL_SCANCODE_1: return Key::One; case SDL_SCANCODE_2: return Key::Two; case SDL_SCANCODE_3: return Key::Three;
    case SDL_SCANCODE_F5: return Key::F5; case SDL_SCANCODE_F9: return Key::F9;
    case SDL_SCANCODE_F3: return Key::F3;case SDL_SCANCODE_C: return Key::C;case SDL_SCANCODE_V: return Key::V;case SDL_SCANCODE_X: return Key::X;
    case SDL_SCANCODE_DELETE: return Key::Delete;case SDL_SCANCODE_HOME: return Key::Home;case SDL_SCANCODE_END: return Key::End;
    case SDL_SCANCODE_PAGEUP: return Key::PageUp;case SDL_SCANCODE_PAGEDOWN: return Key::PageDown;
    case SDL_SCANCODE_SLASH: return Key::Slash;
    case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_RSHIFT: return Key::Shift;
    case SDL_SCANCODE_RETURN: return Key::Enter; case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
    case SDL_SCANCODE_TAB: return Key::Tab;
    case SDL_SCANCODE_LEFT: return Key::Left;case SDL_SCANCODE_RIGHT: return Key::Right;
    case SDL_SCANCODE_UP: return Key::Up;case SDL_SCANCODE_DOWN: return Key::Down;
    case SDL_SCANCODE_ESCAPE: return Key::Escape; default: return Key::Unknown;
    }
}
MouseButton mouseFrom(Uint8 b) {
    switch (b) { case SDL_BUTTON_LEFT: return MouseButton::Left; case SDL_BUTTON_RIGHT: return MouseButton::Right;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle; default: return MouseButton::Unknown; }
}
PadButton padFrom(Uint8 b) {
    switch (b) { case SDL_GAMEPAD_BUTTON_SOUTH: return PadButton::South; case SDL_GAMEPAD_BUTTON_EAST: return PadButton::East;
    case SDL_GAMEPAD_BUTTON_WEST: return PadButton::West; case SDL_GAMEPAD_BUTTON_NORTH: return PadButton::North;
    default: return PadButton::Unknown; }
}
}
struct WindowSystem::Impl {
    SDL_Window* window = nullptr;
    std::map<SDL_JoystickID, SDL_Gamepad*> pads;
    bool initialized = false, minimized = false, textEnabled=false;
    int width = 0, height = 0;
    ~Impl() {
        for (auto [id, pad] : pads) { (void)id; SDL_CloseGamepad(pad); }
        if (window) SDL_DestroyWindow(window);
        if (initialized) SDL_Quit();
    }
};
WindowSystem::WindowSystem(const WindowConfig& config) : impl_(std::make_unique<Impl>()) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) throw std::runtime_error(SDL_GetError());
    impl_->initialized = true;
    impl_->window = SDL_CreateWindow(config.title.c_str(), config.width, config.height,
        SDL_WINDOW_RESIZABLE | (config.hidden ? SDL_WINDOW_HIDDEN : 0));
    if (!impl_->window) throw std::runtime_error(SDL_GetError());
    SDL_GetWindowSizeInPixels(impl_->window, &impl_->width, &impl_->height);
    LOG_INFO("PLATFORM", "SDL3 window initialized");
}
WindowSystem::~WindowSystem() = default;
void* WindowSystem::nativeHandle() const {
    return SDL_GetPointerProperty(SDL_GetWindowProperties(impl_->window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
}
bool WindowSystem::minimized() const { return impl_->minimized; }
int WindowSystem::width() const { return impl_->width; }
int WindowSystem::height() const { return impl_->height; }
void WindowSystem::textInput(bool enabled) {
    if(impl_->textEnabled==enabled) return;
    const bool ok=enabled ? SDL_StartTextInput(impl_->window) : SDL_StopTextInput(impl_->window);
    if(!ok) throw std::runtime_error(SDL_GetError());
    impl_->textEnabled=enabled;
}
void WindowSystem::fullscreen(bool enabled) { if(!SDL_SetWindowFullscreen(impl_->window,enabled)) throw std::runtime_error(SDL_GetError()); }
void WindowSystem::relativeMouse(bool enabled) {
    if(SDL_GetWindowRelativeMouseMode(impl_->window)!=enabled && !SDL_SetWindowRelativeMouseMode(impl_->window,enabled)) throw std::runtime_error(SDL_GetError());
}
std::vector<Event> WindowSystem::pollEvents() {
    std::vector<Event> result;
    int logicalWidth=1,logicalHeight=1; SDL_GetWindowSize(impl_->window,&logicalWidth,&logicalHeight);
    int pixelWidth=1,pixelHeight=1; SDL_GetWindowSizeInPixels(impl_->window,&pixelWidth,&pixelHeight);
    const float pixelX=logicalWidth>0 ? float(pixelWidth)/logicalWidth : 1;
    const float pixelY=logicalHeight>0 ? float(pixelHeight)/logicalHeight : 1;
    SDL_Event raw;
    while (SDL_PollEvent(&raw)) {
        Event e;
        switch (raw.type) {
        case SDL_EVENT_TEXT_INPUT: e.type=EventType::TextInput; e.text=raw.text.text; break;
        case SDL_EVENT_QUIT: case SDL_EVENT_WINDOW_CLOSE_REQUESTED: e.type = EventType::Closed; break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            e.type = EventType::Resized; e.width = impl_->width = raw.window.data1; e.height = impl_->height = raw.window.data2; break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED: e.type = EventType::FocusGained; break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: e.type = EventType::FocusLost; break;
        case SDL_EVENT_WINDOW_MINIMIZED: e.type = EventType::Minimized; impl_->minimized = true; break;
        case SDL_EVENT_WINDOW_RESTORED: e.type = EventType::Restored; impl_->minimized = false; break;
        case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP:
            e.repeat=raw.key.repeat;e.control=(raw.key.mod&SDL_KMOD_CTRL)!=0;e.shift=(raw.key.mod&SDL_KMOD_SHIFT)!=0;
            e.type = raw.type == SDL_EVENT_KEY_DOWN ? EventType::KeyDown : EventType::KeyUp;
            e.key = raw.key.key==SDLK_SLASH ? Key::Slash : keyFrom(raw.key.scancode); break;
        case SDL_EVENT_MOUSE_MOTION: e.type = EventType::MouseMove; e.x = raw.motion.x*pixelX; e.y = raw.motion.y*pixelY;e.dx=raw.motion.xrel;e.dy=raw.motion.yrel; break;
        case SDL_EVENT_MOUSE_WHEEL: e.type=EventType::MouseWheel;e.x=raw.wheel.x;e.y=raw.wheel.y;if(raw.wheel.direction==SDL_MOUSEWHEEL_FLIPPED) { e.x=-e.x;e.y=-e.y; }break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP:
            e.type = raw.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? EventType::MouseDown : EventType::MouseUp;
            { Event pointer; pointer.type=EventType::MouseMove; pointer.x=raw.button.x*pixelX; pointer.y=raw.button.y*pixelY; result.push_back(pointer); }
            e.mouse = mouseFrom(raw.button.button); break;
        case SDL_EVENT_GAMEPAD_ADDED: {
            auto* pad = SDL_OpenGamepad(raw.gdevice.which);
            if (!pad) { LOG_WARN("INPUT", SDL_GetError()); continue; }
            if (!impl_->pads.emplace(raw.gdevice.which, pad).second) { SDL_CloseGamepad(pad); continue; }
            e.type = EventType::PadConnected; e.device = raw.gdevice.which; break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (auto it = impl_->pads.find(raw.gdevice.which); it != impl_->pads.end()) { SDL_CloseGamepad(it->second); impl_->pads.erase(it); }
            e.type = EventType::PadDisconnected; e.device = raw.gdevice.which; break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_GAMEPAD_BUTTON_UP:
            e.type = raw.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? EventType::PadDown : EventType::PadUp;
            e.device = raw.gbutton.which; e.pad = padFrom(raw.gbutton.button); break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            auto it = impl_->pads.find(raw.gaxis.which); if (it == impl_->pads.end()) continue;
            e.type = EventType::PadAxis; e.device = raw.gaxis.which;
            e.x = SDL_GetGamepadAxis(it->second, SDL_GAMEPAD_AXIS_LEFTX) / 32768.0f;
            e.y = SDL_GetGamepadAxis(it->second, SDL_GAMEPAD_AXIS_LEFTY) / 32768.0f; break;
        }
        default: continue;
        }
        if(e.type==EventType::MouseMove || e.type==EventType::MouseDown || e.type==EventType::MouseUp) { const auto modifiers=SDL_GetModState();e.control=(modifiers&SDL_KMOD_CTRL)!=0;e.shift=(modifiers&SDL_KMOD_SHIFT)!=0; }
        result.push_back(e);
    }
    return result;
}
}
