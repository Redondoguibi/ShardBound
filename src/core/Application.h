#pragma once
#include "core/GameLoop.h"
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
    bool initialized_ = false;
};
}
