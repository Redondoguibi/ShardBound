#pragma once
#include <cstdint>
namespace game {
enum class Key { Unknown, W, A, S, D, Space, E, Escape, M, I, Q, One, Two, Three, F5, F9, Count };
enum class MouseButton { Unknown, Left, Middle, Right, Count };
enum class PadButton { Unknown, South, East, West, North, Count };
enum class EventType {
    Closed, Resized, FocusGained, FocusLost, Minimized, Restored,
    KeyDown, KeyUp, MouseMove, MouseDown, MouseUp,
    PadConnected, PadDisconnected, PadDown, PadUp, PadAxis
};
struct Event {
    EventType type{};
    Key key = Key::Unknown;
    MouseButton mouse = MouseButton::Unknown;
    PadButton pad = PadButton::Unknown;
    std::uint32_t device = 0;
    int width = 0, height = 0;
    float x = 0, y = 0;
};
}
