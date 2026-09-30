#include "core/Application.h"
#include "core/Log.h"
#include <charconv>
#include <exception>
#include <string_view>
#include <stdexcept>
namespace game {
Application::~Application() { shutdown(); }
int Application::execute(int argc, char** argv) {
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg == "--frames" && i + 1 < argc) {
                const std::string_view value = argv[++i];
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), config_.maxFrames);
                if (error != std::errc{} || end != value.data() + value.size() || !config_.maxFrames)
                    throw std::invalid_argument("--frames requires a positive integer");
            } else throw std::invalid_argument("Usage: game [--frames positive-integer]");
        }
        if (!initialize()) return 1;
        run();
        shutdown();
        return 0;
    } catch (const std::exception& error) {
        LOG_ERROR("CORE", error.what());
        shutdown();
        return 1;
    }
}
bool Application::initialize() {
    if (initialized_) return true;
    LOG_INFO("CORE", "Application starting");
    initialized_ = true;
    LOG_INFO("CORE", "Runtime initialized");
    return true;
}
void Application::run() {
    if (!initialized_) throw std::logic_error("Application is not initialized");
    LOG_INFO("CORE", "Running");
    loop_.run(config_, [](double) {});
}
void Application::shutdown() noexcept {
    loop_.stop();
    if (!initialized_) return;
    LOG_INFO("CORE", "Shutting down");
    initialized_ = false;
}
}
