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
#include "studio/StudioRuntime.h"
#include "scripting/ExtensionBridge.h"
#include "extensions/NativeMods.h"
#include "game/GameUi.h"
#include "game/FrontEnd.h"
#include "game/DeveloperMode.h"
#include "world/ChunkWorld.h"
#include "mods/Manifest.h"
#include "core/FrameProfiler.h"
#include "scripting/EngineBridge.h"
#include <chrono>
namespace game {
class Application {
public:
    ~Application();
    int execute(int argc, char** argv);
    bool initialize();
    void run();
    void shutdown() noexcept;
private:
    friend struct ApplicationRegressionAccess;
    friend struct ReadinessTestAccess;
    friend struct ReadinessReviewAccess;
    friend struct EngineIntegrationAccess;
    void initializeSession(const std::filesystem::path& saves);
    void clearSession();
    void openWorld(const WorldRecord& record,bool resume);
    void saveWorld(bool background=false);
    void pollWorldSave(bool wait=false);
    void applySettings(const GameSettings& settings);
    std::string exportPerformance();
    LoopConfig config_;
    GameLoop loop_;
    WindowConfig windowConfig_;
    std::unique_ptr<WindowSystem> window_;
    InputSystem input_;
    RenderConfig renderConfig_;
    std::unique_ptr<Renderer> renderer_;
    std::filesystem::path root_;
    std::filesystem::path userDirectory_;
    std::filesystem::path modsDirectory_ = "content";
    std::filesystem::path testMap_;
    VirtualFileSystem vfs_;
    std::unique_ptr<AssetManager> assets_;
    std::unique_ptr<World> world_;
    std::unique_ptr<PhysicsSystem> physics_;
    std::unique_ptr<PlayerController> player_;
    std::unique_ptr<AudioSystem> audio_;
    std::unique_ptr<StudioRuntime> studio_;
    std::unique_ptr<script::Runtime> scripting_;
    std::unique_ptr<GameUi> ui_;
    std::unique_ptr<DeveloperMode> developer_;
    std::unique_ptr<Components> components_;
    std::unique_ptr<SaveGame> saves_;
    std::unique_ptr<ExtensionBridge> extensions_;
    std::unique_ptr<NativeMods> native_;
    std::unique_ptr<EngineBridge> engine_;
    std::unique_ptr<ChunkWorld> terrain_;
    std::unique_ptr<FrontEnd> frontEnd_;
    std::vector<ModManifest> mods_;
    std::vector<ModManifest> availableMods_;
    std::map<std::string,std::string> modVersions_;
    std::optional<WorldRecord> currentWorld_;
    bool procedural_=true,explicitMods_=false;
    bool noVsyncOverride_=false;
    std::string autoWorldName_,autoWorldSeed_;
    std::string openWorldId_;
    double autosave_=0,shaderTime_=0;
    bool menuFrame_=false,worldSavePending_=false;
    SaveGame::SaveRequest worldSaveRequest_;
    bool allowNative_=false;
    bool initialized_ = false;
    FrameProfiler profiler_;
    bool forcePerformance_=false;
    bool portable_=false;
    std::chrono::steady_clock::time_point frameStart_{},lastPresent_{},memorySample_{};
    double simulationMs_=0,streamingMs_=0;
    std::uint64_t privateBytes_=0;
};
}
