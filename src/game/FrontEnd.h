#pragma once
#include "world/WorldStorage.h"
#include "platform/InputSystem.h"
#include "platform/WindowSystem.h"
#include "platform/TextImage.h"
#include <functional>
#include <future>
#include "mods/ModProfiles.h"
namespace game {
class FrontEnd {
public:
    enum class Screen { Title,Worlds,Create,Settings,Playing,Pause,Backups,Controls,Audio,Mods,ModIssues,ProfileName };
    FrontEnd(Renderer& renderer,WindowSystem& window,InputSystem& input,std::filesystem::path user,WorldDefinition definition,std::map<std::string,std::string> mods);
    void event(const Event& event,bool uiCaptured=false);
    void update();
    TextureHandle texture();
    bool playing() const { return screen_==Screen::Playing; }
    bool activeWorld() const { return active_; }
    bool quitRequested() const { return quit_; }
    const GameSettings& settings() const { return settings_; }
    WorldStorage& storage() { return storage_; }
    void open(const WorldRecord& record,bool resume);
    void error(std::string message);
    void saved() { saveError_.clear(); }
    Screen screen() const { return screen_; }
    void configureMods(std::vector<ModManifest> catalog,std::set<std::string> available,std::vector<Diagnostic> diagnostics={});
    WorldRecord createRecord(std::string name,std::string seed);
    WorldRecord createMapTestRecord(const std::filesystem::path& source);
    std::function<void(const WorldRecord&,bool)> startWorld;
    std::function<void()> saveWorld;
    std::function<void()> closeWorld;
    std::function<void(const GameSettings&)> applySettings;
    std::function<std::string()> exportPerformance;
private:
    friend struct ReadinessTestAccess;
    friend struct ReadinessReviewAccess;
    void changedSettings(const GameSettings& old);
    void refreshProfile();
    std::vector<std::string> profileIds() const;
    void navigate(Screen screen);
    void action(int id);
    void draw();
    void button(int id,int x,int y,int w,int h,const std::string& label);
    void text(int x,int y,int w,int size,std::string value,std::array<unsigned char,4> color={225,233,228,255});
    void rectangle(int x,int y,int w,int h,std::array<unsigned char,4> color);
    bool hit(int x,int y,int w,int h) const;
    Renderer& renderer_;WindowSystem& window_;InputSystem& input_;
    WorldStorage storage_;std::filesystem::path settingsFile_;
    WorldDefinition definition_;std::map<std::string,std::string> mods_;GameSettings settings_;
    Screen screen_=Screen::Title,settingsReturn_=Screen::Title;
    bool active_=false,quit_=false,click_=false;
    int focus_=0;size_t page_=0,generatorIndex_=0;
    std::string name_,seed_,message_,saveError_;
    std::vector<WorldRecord> worlds_;
    std::string backupWorld_;std::vector<std::string> backups_;
    std::vector<CanvasCommand> commands_,previous_;
    TextureHandle texture_;
    std::future<std::string> maintenance_;
    std::unique_ptr<ModProfiles> profiles_;
    std::vector<Diagnostic> modDiagnostics_;
    std::string rebinding_;
    size_t controlsPage_=0,modsPage_=0;
    std::string profileWorld_;
    size_t issuesPage_=0;
    std::string profileName_;
    bool bindingConsumed_=false;
};
}
