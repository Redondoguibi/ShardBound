#include "core/Application.h"
#include "game/Equipment.h"
#include "game/InventorySystem.h"
#include "scripting/DataConversion.h"
#include <chrono>
#include <fstream>
#include <iostream>
namespace game {
namespace {
using namespace script;
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F action) {bool caught=false;try {action();}catch(const std::exception&) {caught=true;}check(caught,"Invalid operation accepted");}
void write(const std::filesystem::path& file,const std::string& text) {std::ofstream out(file,std::ios::binary);out<<text;if(!out) throw std::runtime_error("Cannot write regression fixture");}
Value text(const std::string& value) {return Value(value);}
}
// Exercise the same Application object as the executable, without changing runtime visibility.
struct ApplicationRegressionAccess {
    static void run() {
        Application app;app.root_=std::filesystem::current_path();
        const auto root=app.root_/"user"/("integration-regressions-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto mods=root/"mods";std::filesystem::create_directories(root);std::filesystem::copy(app.root_/"content",mods,std::filesystem::copy_options::recursive);
        std::filesystem::create_directories(mods/"review");
        write(mods/"review/mod.json",R"({"id":"review","name":"Regression probe","version":"1.0.0","api":"0.1","entry":"main.sl","dependencies":["foundation >= 0.2.0"]})");
        write(mods/"review/main.sl",R"(@Globals
spec = get_definition("Entities", "foundation:Actor")
spec.speed = 9
spec.max_health = 150
spec.jump = 12
replace_definition("Entities", "foundation:Actor", spec)
@Items
item Token:
    .name = "Regression token"
)");
        app.userDirectory_=root/"user";app.modsDirectory_=mods;app.explicitMods_=true;app.windowConfig_.hidden=true;app.noVsyncOverride_=true;app.autoWorldName_="Integration regression";app.autoWorldSeed_="regression";
        check(app.initialize(),"Application failed to initialize");
        auto actor=app.player_->entity();
        check(app.player_->speed()==9 && app.world_->get<HealthComponent>(actor).maximum==150,"Dependent Actor patches were overwritten by captured defaults");
        check(app.scripting_->call("get_attribute",{entityValue(*app.world_,actor),text("foundation:jump_speed")}).number()==12,"Jump default ignored final Actor definition");
        app.scripting_->execute("review",R"(@Globals
attribute = get_definition("Attributes", "foundation:movement_speed")
attribute.default = 11
replace_definition("Attributes", "foundation:movement_speed", attribute)
)");
        app.scripting_->execute("foundation","apply_player_attributes()");check(app.player_->speed()==11,"Explicit numeric attribute default did not override its definition reference");
        app.scripting_->call("set_attribute",{entityValue(*app.world_,actor),text("foundation:movement_speed"),Value(17.0)});
        app.scripting_->call("set_attribute",{entityValue(*app.world_,actor),text("foundation:max_health"),Value(180.0)});
        app.saveWorld();const auto record=*app.currentWorld_;app.openWorld(record,true);actor=app.player_->entity();
        check(app.player_->speed()==17 && app.world_->get<HealthComponent>(actor).maximum==180,"Saved attribute bases were overwritten by dynamic defaults after reopening the world");

        auto& world=*app.world_;auto& runtime=*app.scripting_;const auto oldInventory=world.get<InventoryComponent>(actor);const auto oldCustom=world.custom(actor);
        giveItem(world,actor,AssetId("review:Token"),40);EquipmentState instances;instances.next=32;
        for(int i=1;i<=31;++i) {ItemInstance item;item.item="review:Token";item.slot="review_"+std::to_string(i);instances.items["i"+std::to_string(i)]=item;}
        writeEquipment(world,actor,instances);validateEquipment(world,actor);
        const auto before=encodeData(world.custom(actor).at("core:equipment").data);
        rejects([&]{runtime.call("equip_slot",{entityValue(world,actor),text("extra"),text("review:Token")});});
        check(before==encodeData(world.custom(actor).at("core:equipment").data),"Rejected 33rd slot changed equipment data");validateEquipment(world,actor);app.saveWorld();
        check(runtime.call("equip_slot",{entityValue(world,actor),text("weapon"),text("review:Token")}).boolean(),"Replacing a stack slot at the combined limit was rejected");
        runtime.call("equip_instance",{entityValue(world,actor),text("review_1"),Value{}});
        check(runtime.call("equip_slot",{entityValue(world,actor),text("extra"),text("review:Token")}).boolean(),"Freed instance slot did not become available to stack equipment");
        validateEquipment(world,actor);app.saveWorld();world.replaceEntityData({{actor,oldInventory,oldCustom}});

        auto savedEvents=std::make_shared<int>(0);runtime.subscribe("on_saved",native([savedEvents](const auto&,const auto&){++*savedEvents;return Value{};}),false,"review");
        const auto checkpoint=app.saves_->directory()/"checkpoint.json",original=app.saves_->directory()/"original-checkpoint.fixture";
        const auto failWrite=[&] {std::filesystem::rename(checkpoint,original);std::filesystem::create_directory(checkpoint);app.worldSaveRequest_=app.saves_->requestSave("checkpoint");app.worldSavePending_=true;};
        const auto restoreFile=[&] {std::filesystem::remove(checkpoint);std::filesystem::rename(original,checkpoint);};
        failWrite();rejects([&]{app.saveWorld();});
        check(!app.worldSavePending_ && !app.worldSaveRequest_,"Failed synchronous drain left an Application completion pending");app.pollWorldSave();
        check(*savedEvents==0 && std::filesystem::is_directory(checkpoint),"Failed checkpoint emitted on_saved");restoreFile();app.saveWorld();check(*savedEvents==1,"Manual retry after failure did not save successfully");
        failWrite();rejects([&]{app.saves_->finishSave(true);}); // another script/native operation observes the same failure first
        restoreFile();const auto other=app.saves_->requestSave("other");
        rejects([&]{app.pollWorldSave(true);});
        check(!app.worldSavePending_ && *savedEvents==1 && app.saves_->saving(),"Another consumer hid the failure or its newer request was consumed");
        app.saves_->finishSave(true,other);app.saveWorld();check(*savedEvents==2,"Application did not recover after independently consumed failure");
        app.saveWorld(true);app.pollWorldSave(true);check(*savedEvents==3 && !app.worldSavePending_,"Successful async completion was not delivered exactly once");app.pollWorldSave();check(*savedEvents==3,"Async completion was delivered twice");

        runtime.bind("review_hud",fromData(decodeData(R"({"layout":"absolute","modal":false,"x":0,"y":0,"w":0.2,"h":0.1,"widgets":[{"id":"label","kind":"label","text":"Persistent HUD","x":0,"y":0,"w":1,"h":1}]})")));
        runtime.execute("review",R"(@Globals
cancels = 0
func on_ui(screen, widget, event, value):
    if event == "cancel":
        cancels = cancels + 1
listener = on_event("on_ui", on_ui)
show_widgets("review:hud", review_hud)
)");
        Event escape;escape.type=EventType::KeyDown;escape.key=Key::Escape;app.input_.beginFrame();app.input_.process(escape);
        check(!app.ui_->capturesMenuInput(escape),"Nonmodal label-only HUD owns Escape");app.ui_->event(escape);app.frontEnd_->event(escape,app.ui_->capturesMenuInput(escape));app.ui_->updateInput();
        check(app.frontEnd_->screen()==FrontEnd::Screen::Pause && runtime.global("review","cancels").number()==0,"Nonmodal HUD blocked pause or received an unrelated cancel");
        app.frontEnd_->event(escape);check(app.frontEnd_->playing(),"Pause did not resume");
        runtime.bind("review_modal",fromData(decodeData(R"({"layout":"absolute","modal":true,"x":0.1,"y":0.1,"w":0.3,"h":0.2,"widgets":[{"id":"label","kind":"label","text":"Dialog","x":0,"y":0,"w":1,"h":1}]})")));
        runtime.execute("review","show_widgets(\"review:modal\", review_modal)");check(app.ui_->capturesMenuInput(escape),"Modal dialog no longer owns Escape");
        app.ui_->event(escape);app.frontEnd_->event(escape,app.ui_->capturesMenuInput(escape));app.ui_->updateInput();
        check(app.frontEnd_->playing() && runtime.global("review","cancels").number()==1,"Modal cancel leaked to game pause");
        runtime.execute("review","hide_widgets(\"review:modal\")");
        runtime.bind("review_input",fromData(decodeData(R"({"layout":"absolute","modal":false,"x":0.1,"y":0.1,"w":0.3,"h":0.2,"widgets":[{"id":"input","kind":"input","value":"editing","x":0,"y":0,"w":1,"h":1}]})")));
        runtime.execute("review","show_widgets(\"review:input\", review_input)\nfocus_widget(\"review:input\", \"input\")");
        check(app.ui_->capturesMenuInput(escape),"Focused nonmodal input no longer owns Escape");app.ui_->event(escape);app.frontEnd_->event(escape,app.ui_->capturesMenuInput(escape));app.ui_->updateInput();
        check(app.frontEnd_->playing() && runtime.global("review","cancels").number()==2,"Focused input cancel leaked to game pause");
        check(!app.ui_->capturesMenuInput(escape),"Cancelled nonmodal input kept trapping Escape");
        app.ui_->event(escape);app.frontEnd_->event(escape,app.ui_->capturesMenuInput(escape));app.ui_->updateInput();check(app.frontEnd_->screen()==FrontEnd::Screen::Pause,"Second Escape could not pause after leaving the nonmodal input");
        app.shutdown();
    }
};
}
int main() {try {game::ApplicationRegressionAccess::run();std::cout<<"PASS integration regressions: async save outcomes, combined equipment slots, late mod defaults, saved overrides and HUD Escape routing\n";return 0;}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
