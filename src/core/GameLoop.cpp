#include "core/GameLoop.h"
#include <chrono>
#include <thread>
namespace game {
void GameLoop::run(const LoopConfig& config, const std::function<void(double)>& update) {
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    running_ = true;
    std::uint64_t frames = 0;
    while (running_) {
        const auto start = Clock::now();
        const double delta = std::chrono::duration<double>(start - previous).count();
        previous = start;
        update(delta);
        if (config.maxFrames && ++frames >= config.maxFrames) break;
        if (config.frameLimit) {
            std::this_thread::sleep_until(start + std::chrono::duration<double>(1.0 / config.frameLimit));
        }
    }
    running_ = false;
}
}
