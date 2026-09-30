#include "core/Application.h"
#include "core/Log.h"
#include "ecs/RenderSystem.h"
#include <charconv>
#include <exception>
#include <string_view>
#include <stdexcept>
namespace game {
Application::~Application() { shutdown(); }
int Application::execute(int argc, char** argv) {
    try {
        root_ = std::filesystem::absolute(argv[0]).parent_path();
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg == "--frames" && i + 1 < argc) {
                const std::string_view value = argv[++i];
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), config_.maxFrames);
                if (error != std::errc{} || end != value.data() + value.size() || !config_.maxFrames)
                    throw std::invalid_argument("--frames requires a positive integer");
            } else if (arg == "--hidden") windowConfig_.hidden = true;
            else if (arg == "--no-vsync") renderConfig_.vsync = false;
            else if (arg == "--fps" && i + 1 < argc) {
                const std::string_view value = argv[++i];
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), config_.frameLimit);
                if (error != std::errc{} || end != value.data() + value.size()) throw std::invalid_argument("--fps requires an unsigned integer (0 = unlimited)");
            } else throw std::invalid_argument("Usage: game [--frames N] [--fps N] [--hidden]");
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
    window_ = std::make_unique<WindowSystem>(windowConfig_);
    renderer_ = std::make_unique<Renderer>(window_->nativeHandle(),window_->width(),window_->height(),root_ / "assets/core/shaders/forward.hlsl", renderConfig_);
    vfs_.mount("core",root_ / "assets/core");
    assets_ = std::make_unique<AssetManager>(vfs_,*renderer_,root_ / "user/cache");
    assets_->registerModel({AssetId("core:cube"),"models/cube.glb"});
    world_ = std::make_unique<World>();
    auto cube=world_->create("cube");
    world_->add(cube,TransformComponent{});
    world_->add(cube,ModelComponent{AssetId("core:cube"),assets_->model(AssetId("core:cube"))});
    initialized_ = true;
    LOG_INFO("CORE", "Runtime initialized");
    return true;
}
void Application::run() {
    if (!initialized_) throw std::logic_error("Application is not initialized");
    LOG_INFO("CORE", "Running");
    LoopCallbacks callbacks;
    callbacks.poll = [&] {
        input_.beginFrame();
        for (const auto& event : window_->pollEvents()) {
            input_.process(event);
            if (event.type == EventType::Closed) loop_.stop();
            if (event.type == EventType::Resized) renderer_->resize(event.width,event.height);
        }
        if (input_.pressed("Quit")) loop_.stop();
    };
    callbacks.suspended = [&] { return window_->minimized(); };
    callbacks.render = [&](double) {
        const auto items=collectRenderItems(*world_);
        renderer_->render({},items); renderer_->present();
    };
    loop_.run(config_, callbacks);
}
void Application::shutdown() noexcept {
    loop_.stop();
    world_.reset();
    assets_.reset();
    vfs_.unmount("core");
    renderer_.reset();
    window_.reset();
    if (!initialized_) return;
    LOG_INFO("CORE", "Shutting down");
    initialized_ = false;
}
}
