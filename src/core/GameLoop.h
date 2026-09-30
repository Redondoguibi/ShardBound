#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
namespace game {
struct LoopConfig {
    unsigned frameLimit = 60;
    std::uint64_t maxFrames = 0;
};
class FixedStepper {
public:
    static constexpr double delta = 1.0 / 60.0;
    unsigned advance(double seconds);
    void reset() { accumulator_ = 0; }
    double alpha() const { return accumulator_ / delta; }
private:
    double accumulator_ = 0;
};
struct LoopCallbacks {
    std::function<void()> poll;
    std::function<bool()> suspended;
    std::function<void(double)> fixedUpdate;
    std::function<void(double)> update;
    std::function<void(double)> render;
};
class GameLoop {
public:
    void run(const LoopConfig& config, const std::function<void(double)>& update);
    void run(const LoopConfig& config, const LoopCallbacks& callbacks);
    void stop() noexcept { running_ = false; }
private:
    std::atomic_bool running_ = false;
};
}
