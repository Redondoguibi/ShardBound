#pragma once
#include <cstdint>
#include <string>
namespace game {
enum class Key { Unknown, W, A, S, D, Space, E, Escape, M, I, Q, One, Two, Three, F5, F9, Enter, Backspace, Tab, Left, Right, Up, Down, F3, C, V, X, Delete, Home, End, PageUp, PageDown, Slash, Shift, B,F,G,H,J,K,L,N,O,P,R,T,U,Y,Z,Zero,Four,Five,Six,Seven,Eight,Nine,F1,F2,F4,F6,F7,F8,F10,F11,F12,Control,Alt,Count };
std::string keyName(Key key);
Key keyFromName(const std::string& name);
enum class MouseButton { Unknown, Left, Middle, Right, Count };
enum class PadButton { Unknown, South, East, West, North, Count };
enum class EventType {
    Closed, Resized, FocusGained, FocusLost, Minimized, Restored,
    KeyDown, KeyUp, MouseMove, MouseDown, MouseUp,
    PadConnected, PadDisconnected, PadDown, PadUp, PadAxis, TextInput, MouseWheel
};
struct Event {
    EventType type{};
    Key key = Key::Unknown;
    MouseButton mouse = MouseButton::Unknown;
    PadButton pad = PadButton::Unknown;
    std::uint32_t device = 0;
    int width = 0, height = 0;
    float x = 0, y = 0;
    std::string text;
    bool control=false,shift=false,repeat=false;
    float dx=0,dy=0;
};
}
