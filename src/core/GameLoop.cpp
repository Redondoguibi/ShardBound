#include "core/GameLoop.h"
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace game {
unsigned FixedStepper::advance(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid timestep");
    accumulator_ += std::min(seconds, 0.25);
    unsigned steps = 0;
    while (accumulator_ + 1e-12 >= delta) { accumulator_ -= delta; ++steps; }
    accumulator_ = std::max(0.0, accumulator_);
    return steps;
}
void GameLoop::run(const LoopConfig& config, const std::function<void(double)>& update) {
    LoopCallbacks callbacks; callbacks.update = update;
    run(config, callbacks);
}
void GameLoop::run(const LoopConfig& config, const LoopCallbacks& callbacks) {
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    running_ = true;
    std::uint64_t frames = 0;
    FixedStepper stepper;
    while (running_) {
        const auto start = Clock::now();
        const double delta = std::min(0.25, std::chrono::duration<double>(start - previous).count());
        previous = start;
        if (callbacks.poll) callbacks.poll();
        if (!running_) break;
        if (callbacks.suspended && callbacks.suspended()) {
            stepper.reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            previous = Clock::now();
        } else {
            const auto steps = stepper.advance(delta);
            for (unsigned i = 0; i < steps; ++i) if (callbacks.fixedUpdate) callbacks.fixedUpdate(FixedStepper::delta);
            if (callbacks.update) callbacks.update(delta);
            if (callbacks.render) callbacks.render(stepper.alpha());
        }
        if (config.maxFrames && ++frames >= config.maxFrames) break;
        if (config.frameLimit) {
            std::this_thread::sleep_until(start + std::chrono::duration<double>(1.0 / config.frameLimit));
        }
    }
    running_ = false;
}
}
