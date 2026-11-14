#pragma once
#include "game/DeveloperCommands.h"
#include "platform/InputSystem.h"
#include "platform/TextImage.h"
#include <deque>
#include <set>
namespace game {
// Session-only controls. They never change the saved gameplay camera or input bindings.
class DeveloperMode {
public:
    DeveloperMode(script::Runtime& runtime,World& world,PhysicsSystem& physics,PlayerController& player,InputSystem& input);
    ~DeveloperMode();
    void terrain(ChunkWorld* value,std::string seed);
    // Returns true when this event belongs to the developer UI/camera.
    bool event(const Event& event,bool gameplay=true,bool otherTextInput=false);
    void update(float delta,bool gameplay=true,bool otherInput=false);
    void processCommands();
    void setCamera(const std::string& mode);
    Camera camera();
    std::optional<Vec3> streamingCenter();
    std::vector<CanvasCommand> overlay() const;
    TextureHandle render(Renderer& renderer);
    bool enabled() const { return enabled_; }
    bool textInput() const { return console_; }
    bool relativeMouse() const { return enabled_ && looking_ && !console_ && mode_!="default"; }
    const std::string& cameraMode() const { return mode_; }
    const std::string& line() const { return line_; }
    const std::deque<std::string>& output() const { return output_; }
    void sensitivity(float value);
    std::function<std::string()> readClipboard;
    std::function<void(const std::string&)> writeClipboard;
private:
    void enable(bool value);
    void closeConsole();
    void insert(const std::string& text);
    void changed();
    void print(const std::string& text);
    void syncOrigin();
    void complete(bool backwards);
    void recall(bool older);
    DeveloperCommands commands_;
    World& world_;PhysicsSystem& physics_;PlayerController& player_;InputSystem& input_;
    ChunkWorld* terrain_=nullptr;
    std::int64_t originX_=0,originZ_=0;
    bool enabled_=false,console_=false,looking_=false,up_=false,down_=false,chord_=false,openingSlash_=false;
    std::set<Key> keys_;
    std::string mode_="default",line_,draft_;
    size_t cursor_=0,anchor_=0,historyIndex_=0,scroll_=0;
    std::vector<std::string> history_,suggestions_,tabChoices_;
    std::vector<std::pair<size_t,int>> lineAdvances_{{0,0}};
    std::deque<std::string> output_,pending_;
    size_t tabIndex_=0;
    float yaw_=0,pitch_=.35f,distance_=6;
    float sensitivity_=1;
    Vec3 freeEye_{};
    float messageTime_=0;
    TextureHandle texture_;
    std::vector<CanvasCommand> rendered_;
};
}
