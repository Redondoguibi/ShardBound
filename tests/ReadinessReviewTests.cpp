#include "core/Application.h"
#include "core/AtomicFile.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <functional>
#include <stdexcept>
namespace game {
namespace {
void check(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
}
struct ReadinessReviewAccess {
    static std::unique_ptr<Application> application(const std::filesystem::path& root) {
        auto app=std::make_unique<Application>();app->root_=std::filesystem::current_path();
        app->modsDirectory_=root/"mods";std::filesystem::create_directories(root);std::filesystem::copy("content",app->modsDirectory_,std::filesystem::copy_options::recursive);
        app->userDirectory_=root/"user";app->explicitMods_=true;app->windowConfig_.hidden=true;app->noVsyncOverride_=true;
        app->autoWorldName_="Review world";app->autoWorldSeed_="readiness-review";check(app->initialize(),"Review application initialization failed");return app;
    }
    static void gamepadRemapping(const std::filesystem::path& root) {
        WindowSystem window({"Input review",320,240,true});Renderer renderer(window.nativeHandle(),320,240,"assets/core/shaders/forward.hlsl");InputSystem input;
        FrontEnd menu(renderer,window,input,root,{},{});menu.applySettings=[&](const auto& settings){input.overrides(settings.bindings);};menu.action(2);menu.action(80);
        const auto attack=[&]{size_t i=0;for(const auto& [id,bindings]:input.bindings()) {(void)bindings;if(id=="Quit") continue;if(id=="Attack") return i;++i;}throw std::runtime_error("Attack action missing");}();
        menu.controlsPage_=attack/5;menu.action(500+int(attack%5));Event key;key.type=EventType::KeyDown;key.key=Key::R;menu.event(key);
        Event pad;pad.type=EventType::PadDown;pad.device=1;pad.pad=PadButton::West;input.process(pad);check(input.pressed("Attack"),"Keyboard remapping removed the controller button");
        input.beginFrame();pad.type=EventType::PadUp;input.process(pad);input.process(key);check(input.pressed("Attack"),"Remapped keyboard key did not activate Attack");
        const auto saved=GameSettings::load(root/"settings.json");InputSystem reopened;reopened.overrides(saved.bindings);pad.type=EventType::PadDown;reopened.process(pad);check(reopened.pressed("Attack"),"Controller binding was lost after loading settings");
    }
    static void keyboardSession(const std::filesystem::path& root) {
        auto app=application(root);Event pad;pad.type=EventType::PadAxis;pad.device=1;pad.x=1;app->input_.process(pad);check(app->input_.moveX()==0,"Foundation did not request keyboard-only movement");
        app->frontEnd_->action(32);check(!app->frontEnd_->activeWorld(),"Review world did not close");check(app->input_.moveX()==1,"Closed world left keyboard-only movement active");
    }
    static void environmentSession(const std::filesystem::path& root) {
        auto app=application(root);app->renderer_->setEnvironment({});app->renderer_->render(Camera{},std::vector<RenderItem>{});const auto baseline=app->renderer_->readback();
        app->scripting_->execute("foundation","configure_view()");app->frontEnd_->action(32);app->renderer_->render(Camera{},std::vector<RenderItem>{});const auto closed=app->renderer_->readback();
        check(std::equal(baseline.begin(),baseline.begin()+4,closed.begin()),"Closed world leaked its environment into the next session");
    }
    static void frozenWorldProfile(const std::filesystem::path& root) {
        auto app=application(root);auto& menu=*app->frontEnd_;const auto original=*app->currentWorld_;menu.action(32);
        // Runtime resources are valid and the world's generation definition is frozen.
        // A subsequent invalid generation edit must only block creation of NEW worlds.
        atomicWrite(app->modsDirectory_/"plains/world.json","{invalid generation");bool failed=false;try {menu.refreshProfile();}catch(const std::exception&) {failed=true;}check(failed,"Invalid generation fixture was accepted");
        bool creationRejected=false;try {menu.createRecord("Invalid new world","review");}catch(const std::exception&) {creationRejected=true;}check(creationRejected,"Invalid generation allowed creating a new world");
        menu.profileWorld_=original.id;menu.action(94);menu.maintenance_.wait();menu.update();const auto restored=menu.storage_.read(original.id);
        check(restored.mods==original.mods && restored.definition.json()==original.definition.json(),"Applying a profile discarded selected mods or changed frozen terrain");
        menu.open(restored,true);check(menu.playing() && bool(app->player_->entity()),"Existing world could not resume with its frozen generation definition");
    }
    static void rotatedSupport(const std::filesystem::path&) {
        PhysicsWorld physics;physics.box({.65f,-.5f,0},{.08f,.5f,.5f},false);
        const Vec3 half{.8f,1,.2f};const auto actor=physics.box({0,1,0},half,true,{0,1.5707963f,0});
        check(!physics.grounded(actor,half),"Rotated collider found support outside its physical footprint");
        physics.box({0,-.5f,.65f},{.15f,.5f,.08f},false);
        check(physics.grounded(actor,half),"Rotated collider missed support beneath its physical footprint");
    }
    static int run() {
        const auto root=std::filesystem::current_path()/"user"/("readiness-review-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const std::vector<std::pair<std::string,std::function<void(const std::filesystem::path&)>>> cases={{"gamepad remapping",gamepadRemapping},{"keyboard session isolation",keyboardSession},{"environment session isolation",environmentSession},{"frozen world profile",frozenWorldProfile},{"rotated collider support",rotatedSupport}};
        int failures=0;for(size_t i=0;i<cases.size();++i) try {cases[i].second(root/std::to_string(i));std::cout<<"PASS "<<cases[i].first<<'\n';}catch(const std::exception& error) {++failures;std::cerr<<"FAIL "<<cases[i].first<<": "<<error.what()<<'\n';}
        return failures?1:0;
    }
};
}
int main() {return game::ReadinessReviewAccess::run();}
