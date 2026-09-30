#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
namespace game {
struct LoopConfig {
    unsigned frameLimit = 60;
    std::uint64_t maxFrames = 0;
};
class GameLoop {
public:
    void run(const LoopConfig& config, const std::function<void(double)>& update);
    void stop() noexcept { running_ = false; }
private:
    std::atomic_bool running_ = false;
};
}
