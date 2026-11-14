#pragma once
#include "platform/Events.h"
#include <memory>
#include <string>
#include <vector>
namespace game {
std::string clipboardText();
void setClipboardText(const std::string& text);
struct WindowConfig { std::string title = "Game Runtime"; int width = 1280, height = 720; bool hidden = false; };
class WindowSystem {
public:
    explicit WindowSystem(const WindowConfig& config);
    ~WindowSystem();
    WindowSystem(const WindowSystem&) = delete;
    WindowSystem& operator=(const WindowSystem&) = delete;
    std::vector<Event> pollEvents();
    void* nativeHandle() const;
    bool minimized() const;
    int width() const;
    int height() const;
    void textInput(bool enabled);
    void fullscreen(bool enabled);
    void relativeMouse(bool enabled);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
