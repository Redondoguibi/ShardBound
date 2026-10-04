#include "core/Application.h"
#include "core/Log.h"
#include "assets/CoreAssets.h"
#include "ecs/RenderSystem.h"
#include "mods/ModLoader.h"
#include "game/PreparationScreen.h"
#include "scripting/Bridge.h"
#include "scripting/GameplayBridge.h"
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
            else if (arg == "--allow-native") allowNative_=true;
            else if (arg == "--mods" && i + 1 < argc) modsDirectory_=argv[++i];
            else if (arg == "--fps" && i + 1 < argc) {
                const std::string_view value = argv[++i];
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), config_.frameLimit);
                if (error != std::errc{} || end != value.data() + value.size()) throw std::invalid_argument("--fps requires an unsigned integer (0 = unlimited)");
            } else throw std::invalid_argument("Usage: game [--frames N] [--fps N] [--hidden] [--no-vsync] [--mods DIRECTORY] [--allow-native]");
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
    registerCoreModels(*assets_);
    world_ = std::make_unique<World>();
    physics_=std::make_unique<PhysicsSystem>(*world_);
    player_=std::make_unique<PlayerController>(*world_,*physics_);
    try { audio_=std::make_unique<AudioSystem>(); }
    catch(const DiagnosticError& error) { LOG_WARN("AUDIO",error.what()); audio_=std::make_unique<AudioSystem>(true); }
    scripting_=std::make_unique<script::Runtime>();
    ui_=std::make_unique<GameUi>(*scripting_,*renderer_);
    bindRuntimeApi(*scripting_,*world_,*assets_);
    bindGameplayApi(*scripting_,*world_,*physics_,*player_,input_,*audio_,*assets_);
    components_=std::make_unique<Components>();
    saves_=std::make_unique<SaveGame>(root_/"user/saves",*world_,*physics_,*player_,*assets_);
    saves_->definitionAvailable=[this](const std::string& id) { try { (void)scripting_->registry().get("Entities",id); return true; } catch(...) { return false; } };
    extensions_=std::make_unique<ExtensionBridge>(*scripting_,*world_,*components_,*saves_);
    native_=std::make_unique<NativeMods>(*scripting_,*world_,*components_,*saves_);
    PreparationScreen preparation(*renderer_);
    ModLoader loader(vfs_,*assets_);
    const auto report=loader.load(root_/modsDirectory_,[&](std::string_view mod,size_t done,size_t total) {
        for (const auto& event:window_->pollEvents()) {
            if (event.type==EventType::Closed) throw PreparationCancelled();
            if (event.type==EventType::Resized) renderer_->resize(event.width,event.height);
        }
        if (!window_->minimized()) preparation.show(mod,done,total);
    });
    for(const auto& mod:report.mods) {
        if(mod.dependencyCycle) continue;
        try {
            std::vector<std::string> dependencies;
            for(const auto& dep:mod.dependencies) for(const auto& installed:report.mods)
                if(installed.id==dep.id && !installed.dependencyCycle && dep.accepts(installed.version)) dependencies.push_back(dep.id);
            if(!mod.entry.empty()) scripting_->load(mod.id,mod.root,mod.entry,dependencies);
        } catch(const std::exception& error) { LOG_ERROR("SCRIPT",mod.id+": "+error.what()); }
        if(!mod.native.empty()) {
            if(!allowNative_) LOG_WARN("NATIVE",mod.id+": native extension disabled; use --allow-native for trusted DLLs");
            else try { native_->load(mod.id,mod.root,mod.native); } catch(const std::exception& error) { LOG_ERROR("NATIVE",error.what()); }
        }
    }
    std::map<std::string,std::string> versions;
    for(const auto& mod:report.mods) { const auto& v=mod.version.parts; versions[mod.id]=std::to_string(v[0])+"."+std::to_string(v[1])+"."+std::to_string(v[2]); }
    saves_->setMods(std::move(versions));
    for(const auto& diagnostic:scripting_->emit("on_start")) LOG_ERROR("SCRIPT",DiagnosticError(diagnostic).what());
    for(const auto& diagnostic:extensions_->process()) LOG_WARN("SAVE",DiagnosticError(diagnostic).what());
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
    callbacks.fixedUpdate=[&](double delta) {
        for(const auto& d:scripting_->emit("on_fixed_update",{script::Value(delta)})) LOG_ERROR("SCRIPT",DiagnosticError(d).what());
        player_->move(input_.moveX(),input_.moveY());
        physics_->step(static_cast<float>(delta));
    };
    callbacks.update=[&](double delta) {
        for(const auto& d:scripting_->emit("on_update",{script::Value(delta)})) LOG_ERROR("SCRIPT",DiagnosticError(d).what());
        for(const auto& d:extensions_->process()) LOG_WARN("SAVE",DiagnosticError(d).what());
        if(audio_->offline()) (void)audio_->mix(static_cast<unsigned>(delta*48000));
        audio_->update();
    };
    callbacks.render = [&](double) {
        const auto items=collectRenderItems(*world_);
        renderer_->render(player_->camera(),items,ui_->texture()); renderer_->present();
    };
    loop_.run(config_, callbacks);
}
void Application::shutdown() noexcept {
    loop_.stop();
    if(initialized_ && scripting_) {
        try { for(const auto& d:scripting_->emit("on_shutdown")) LOG_WARN("SCRIPT",DiagnosticError(d).what()); }
        catch(...) { LOG_ERROR("SCRIPT","Shutdown event failed"); }
    }
    native_.reset();
    extensions_.reset();
    saves_.reset();
    components_.reset();
    scripting_.reset();
    ui_.reset();
    player_.reset();
    physics_.reset();
    audio_.reset();
    world_.reset();
    assets_.reset();
    vfs_=VirtualFileSystem{};
    renderer_.reset();
    window_.reset();
    if (!initialized_) return;
    LOG_INFO("CORE", "Shutting down");
    initialized_ = false;
}
}
