#pragma once
#include "core/GameLoop.h"
#include "platform/WindowSystem.h"
#include "platform/InputSystem.h"
#include "renderer/Renderer.h"
#include "assets/AssetManager.h"
#include "ecs/World.h"
#include "scripting/Runtime.h"
#include "game/PlayerController.h"
#include "audio/AudioSystem.h"
#include "scripting/ExtensionBridge.h"
#include "extensions/NativeMods.h"
#include "game/GameUi.h"
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
    RenderConfig renderConfig_;
    std::unique_ptr<Renderer> renderer_;
    std::filesystem::path root_;
    std::filesystem::path modsDirectory_ = "content";
    VirtualFileSystem vfs_;
    std::unique_ptr<AssetManager> assets_;
    std::unique_ptr<World> world_;
    std::unique_ptr<PhysicsSystem> physics_;
    std::unique_ptr<PlayerController> player_;
    std::unique_ptr<AudioSystem> audio_;
    std::unique_ptr<script::Runtime> scripting_;
    std::unique_ptr<GameUi> ui_;
    std::unique_ptr<Components> components_;
    std::unique_ptr<SaveGame> saves_;
    std::unique_ptr<ExtensionBridge> extensions_;
    std::unique_ptr<NativeMods> native_;
    bool allowNative_=false;
    bool initialized_ = false;
};
}
