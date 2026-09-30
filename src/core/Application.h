#pragma once
#include "core/GameLoop.h"
#include "platform/WindowSystem.h"
#include "platform/InputSystem.h"
#include "renderer/Renderer.h"
#include "assets/AssetManager.h"
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
    MeshHandle demoMesh_;
    std::filesystem::path root_;
    VirtualFileSystem vfs_;
    std::unique_ptr<AssetManager> assets_;
    TextureHandle demoTexture_;
    bool initialized_ = false;
};
}
