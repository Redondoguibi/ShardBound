#pragma once
#include "core/GameLoop.h"
#include "platform/WindowSystem.h"
#include "platform/InputSystem.h"
namespace game {
class Application {
public:
    ~Application();
    int execute(int argc, char** argv);
    bool initialize();
    void run();
    void shutdown() noexcept;
private:
    LoopConfig config_;
    GameLoop loop_;
    WindowConfig windowConfig_;
    std::unique_ptr<WindowSystem> window_;
    InputSystem input_;
    bool initialized_ = false;
};
}
