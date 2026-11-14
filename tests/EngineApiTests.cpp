#include "core/Application.h"
#include "core/AtomicFile.h"
#include "scripting/EngineServices.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
namespace game {
namespace {
using Json=nlohmann::json;
void check(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
template<class F> void rejects(F action) {bool caught=false;try {action();}catch(const std::exception&) {caught=true;}check(caught,"Invalid engine operation accepted");}
Json inspect(EnginePipeline& pipeline) {return Json::parse(encodeData(pipeline.inspect()));}
void pipelineTests() {
    EnginePipeline pipeline;unsigned defaults=0;std::string order;const auto implementation=[&](const Data&){++defaults;order+='d';};
    pipeline.run("physics",{{"delta",Data{1./60}}},implementation,validateEngineContext);check(defaults==1,"Default pipeline did not run");order.clear();
    pipeline.add("one","one:b","physics",[&](const Data&){order+='b';return false;},{});
    pipeline.add("one","one:a","physics",[&](const Data&){order+='a';return false;},{});
    EnginePipeline::Options after;after.mode="after";pipeline.add("one","one:z","physics",[&](const Data&){order+='z';return false;},after);
    EnginePipeline::Options replacement;replacement.mode="replace";replacement.replaces="core:default";
    pipeline.add("one","one:replace","physics",[&](const Data&){order+='r';pipeline.runDefault("one");return true;},replacement);
    pipeline.run("physics",{{"delta",Data{1./60}}},implementation,validateEngineContext);check(order=="abrdz" && defaults==2,"Engine dispatch order or default delegation failed");
    rejects([&]{pipeline.runDefault("one");});rejects([&]{pipeline.remove("two","one:a");});
    replacement.replaces="one:replace";rejects([&]{pipeline.add("two","two:replace","physics",[](const Data&){return true;},replacement);});
    pipeline.depends=[](const auto& owner,const auto& dependency){return owner=="two" && dependency=="one";};
    pipeline.add("two","two:replace","physics",[](const Data&){return true;},replacement);order.clear();pipeline.run("physics",{{"delta",Data{1./60}}},implementation);check(defaults==2 && order=="abz","Explicit replacement did not suppress the default");
    pipeline.remove("two","two:replace");pipeline.enabled("one","one:replace",false);check(inspect(pipeline)["stages"][2]["provider"]=="core:default","Disabled replacement remained active");
    replacement.replaces="core:default";pipeline.add("other","other:replace","physics",[](const Data&){return false;},replacement);pipeline.remove("other","other:replace");
    EnginePipeline broken;unsigned healthy=0;broken.add("bad","bad:callback","physics",[](const Data&)->bool{throw std::runtime_error("expected callback failure");},{});broken.add("good","good:callback","physics",[&](const Data&){++healthy;return false;},{});
    broken.run("physics",{{"delta",Data{1./60}}},implementation);broken.run("physics",{{"delta",Data{1./60}}},implementation);check(healthy==2 && inspect(broken)["extensions"][0]["errors"]==1,"Failing observer was not isolated and disabled");
    for(int i=0;i<70;++i) {broken.enabled("bad","bad:callback",true);broken.run("physics",{},implementation);}check(inspect(broken)["diagnostics"].size()==64,"Engine diagnostic history is unbounded");
    EnginePipeline invalid;invalid.add("bad","bad:driver","physics",[&](const Data&){invalid.runDefault("bad",{{"delta",Data{-1.}}});return true;},replacement);const auto before=defaults;
    invalid.run("physics",{{"delta",Data{1./60}}},implementation,validateEngineContext);check(defaults==before+1 && inspect(invalid)["extensions"][0]["enabled"]==false,"Invalid override did not fall back safely");
    EnginePipeline twice;twice.add("bad","bad:twice","physics",[&](const Data&){twice.runDefault("bad");twice.runDefault("bad");return true;},replacement);const auto count=defaults;twice.run("physics",{},implementation);check(defaults==count+1,"Faulting replacement double-executed the engine");
    EnginePipeline coreFailure;coreFailure.add("wrapper","wrapper:driver","physics",[&](const Data&){try {coreFailure.runDefault("wrapper");}catch(...) {}return true;},replacement);rejects([&]{coreFailure.run("physics",{},[](const Data&){throw std::runtime_error("expected core failure");});});
    EnginePipeline snapshot;unsigned removed=0;bool first=true;snapshot.add("mod","mod:a","update",[&](const Data&){if(first) {snapshot.remove("mod","mod:b");first=false;}return false;},{});snapshot.add("mod","mod:b","update",[&](const Data&){++removed;return false;},{});snapshot.run("update",{},[](const Data&){});snapshot.run("update",{},[](const Data&){});check(removed==1,"Engine mutation affected current dispatch snapshot");
    EnginePipeline recursive;recursive.add("bad","bad:recursive","update",[&](const Data&){recursive.run("physics",{},[](const Data&){});return false;},{});recursive.run("update",{},[](const Data&){});check(inspect(recursive)["diagnostics"].size()==1,"Recursive engine dispatch was not rejected");
    EnginePipeline limits;for(int i=0;i<32;++i) limits.add("limit","limit:s"+std::to_string(i),"fixed",[](const Data&){return false;},{});rejects([&]{limits.add("limit","limit:extra","fixed",[](const Data&){return false;},{});});
    replacement.api=2;rejects([&]{limits.add("new","new:api","physics",[](const Data&){return false;},replacement);});rejects([&]{limits.add("new","other:foreign","render",[](const Data&){return false;},{});});
}
void scriptAndServices(const std::filesystem::path&) {
    WindowSystem window({"Engine service tests",320,240,true});Renderer renderer(window.nativeHandle(),320,240,"assets/core/shaders/forward.hlsl");World world;PhysicsSystem physics(world);PlayerController player(world,physics);script::Runtime runtime;EngineBridge bridge(runtime);bindEngineServices(bridge,physics,renderer);
    runtime.bind("record",script::native([](const auto&,const auto&){return script::object();}));
    runtime.execute("first",R"(@Globals
p = record()
p.gravity = 2
p.max_slope = 45
configure_engine("physics", p)
calls = 0
func observer(context):
    calls = calls + 1
register_engine_extension("first:observer", "fixed", observer)
)");check(std::abs(physics.rules().gravity-2)<.0001f && std::abs(physics.rules().supportNormal-.7071067f)<.0001f,"Physics configuration was not applied");
    bridge.pipeline.run("fixed",{{"delta",Data{1./60}}},[](const Data&){},validateEngineContext);check(runtime.global("first","calls").number()==1,"Script engine extension did not execute");
    rejects([&]{runtime.execute("second","@Globals\np = record()\np.gravity = 3\nconfigure_engine(\"physics\", p)\n");});check(physics.rules().gravity==2,"Conflicting engine configuration was partially applied");
    runtime.registry().dependencies("second",{"first"});runtime.execute("second","configure_engine(\"physics\", p)");check(physics.rules().gravity==3,"Dependent configuration override was rejected");
    rejects([&]{runtime.execute("second","p.gravity = 4\np.max_slope = 100\nconfigure_engine(\"physics\", p)");});check(physics.rules().gravity==3,"Invalid physics configuration was partially applied");
    const auto config=bridge.service("configuration",{});rejects([&]{bridge.request("third","engine.configure",decodeData(R"({"service":"render","options":{"environment":{"sky":{"x":0,"y":0,"z":0}},"lights":[{}]}})"));});check(config==bridge.service("configuration",{}),"Invalid render configuration was partially applied");
    rejects([&]{bridge.request("third","engine.configure",decodeData(R"({"service":"scheduler","options":{"fixed_order":["physics","physics","update"]}})"));});check(bridge.fixedOrder==std::vector<std::string>{"fixed","movement","physics"},"Invalid scheduler order was published");
    rejects([&]{bridge.request("third","engine.configure",decodeData(R"({"service":"configuration","options":{"gravity":5}})"));});rejects([&]{renderer.scene({},std::vector<PointLight>(33));});
    runtime.execute("budget",R"(@Globals
func endless(context):
    for i in number_range(4096):
        for j in number_range(4096):
            value = 1
register_engine_extension("budget:loop", "fixed", endless)
)");bridge.pipeline.run("fixed",{{"delta",Data{1./60}}},[](const Data&){},validateEngineContext);const auto diagnostics=inspect(bridge.pipeline)["diagnostics"];check(diagnostics.size()==1 && diagnostics[0]["message"].get<std::string>().find("SCRIPT304")!=std::string::npos && runtime.global("first","calls").number()==2,"Engine script budget failure was not isolated");
    const auto actor=world.create();world.add(actor,TransformComponent{{{0,20,0}}});physics.attachBox(actor,{.2f,.2f,.2f},true);PhysicsWorld baseline;const auto falling=baseline.box({0,20,0},{.2f,.2f,.2f},true);for(int i=0;i<60;++i) {physics.step(1.f/60);baseline.step(1.f/60);}check(std::abs(physics.velocity(actor).y/baseline.velocity(falling).y-3.f/9.81f)<.001f,"Configured gravity did not affect real simulation");
    PhysicsWorld sleeping;sleeping.rules({0,.6f});const auto floating=sleeping.box({0,20,0},{.2f,.2f,.2f},true);for(int i=0;i<180;++i) sleeping.step(1.f/60);sleeping.rules({3,.6f});for(int i=0;i<60;++i) sleeping.step(1.f/60);check(sleeping.position(floating).y<19,"Changing gravity did not wake a sleeping body");
    bridge.request("sky","engine.configure",decodeData(R"({"service":"render","options":{"environment":{"sky":{"x":0.1,"y":0.2,"z":0.3}}}})"));
    bridge.request("sun","engine.configure",decodeData(R"({"service":"render","options":{"environment":{"sun":{"x":0.4,"y":0.5,"z":0.6}}}})"));check(renderer.environment().sky.r==.1f && renderer.environment().sun.r==.4f,"Independent engine fields conflicted");
    rejects([&]{bridge.request("other","engine.configure",decodeData(R"({"service":"render","options":{"environment":{"sky":{"x":0.4,"y":0.2,"z":0.3}}}})"));});
    runtime.registry().dependencies("pixel",{"sky","sun"});
    bridge.request("pixel","engine.configure",decodeData(R"({"service":"render","options":{"environment":{"enabled":true,"shadows":false,"sky":{"x":0,"y":0,"z":0},"sun":{"x":0,"y":0,"z":0},"ambient":{"x":0,"y":0,"z":0},"fog_start":9000,"fog_end":10000},"lights":[]}})"));
    const std::vector<Vertex> vertices={{{-1,0,0},{0,0,-1},{0,1}},{{-1,2,0},{0,0,-1},{0,0}},{{1,2,0},{0,0,-1},{1,0}},{{1,0,0},{0,0,-1},{1,1}}};const std::vector<std::uint32_t> indices={0,1,2,0,2,3};Material material;material.doubleSided=true;const std::vector<RenderItem> items={{renderer.createMesh(vertices,indices),material,transformMatrix(Transform{})}};Camera camera;camera.eye={0,1,-3};camera.target={0,1,0};renderer.render(camera,items);const auto dark=renderer.readback();
    bridge.request("pixel","engine.configure",decodeData(R"({"service":"render","options":{"lights":[{"position":{"x":0,"y":1,"z":-1},"color":{"x":1,"y":1,"z":1},"range":5,"intensity":3}]}})"));renderer.render(camera,items);const auto lit=renderer.readback();const size_t center=(size_t(120)*320+160)*4;check(int(lit[center])+lit[center+1]+lit[center+2]>int(dark[center])+dark[center+1]+dark[center+2]+30,"Engine point lights did not affect actual rendered pixels");
}
}
struct EngineIntegrationAccess {
    static void run(const std::filesystem::path& root) {
        Application app;app.root_=std::filesystem::current_path();app.modsDirectory_=root/"mods";std::filesystem::create_directories(root);std::filesystem::copy("content",app.modsDirectory_,std::filesystem::copy_options::recursive);
        const auto mod=app.modsDirectory_/"engine_probe";std::filesystem::create_directories(mod);std::filesystem::copy_file("tests/native/engine_probe.dll",mod/"probe.dll");
        atomicWrite(mod/"mod.json",R"({"id":"engine_probe","name":"Engine integration probe","version":"1.0.0","api":"0.1","entry":"main.sl","native":"probe.dll","dependencies":["foundation >= 0.2.0"]})");
        atomicWrite(mod/"main.sl",R"(@Globals
fixed_calls = 0
movement_calls = 0
update_calls = 0
stream_calls = 0
render_calls = 0
sequence = ""
schedule_verified = 0
async_ticks = 0
async_job = null
async func background():
    for tick in number_range(4096):
        await next_frame()
        async_ticks = async_ticks + 1
func observer(context):
    if context.stage == "fixed":
        fixed_calls = fixed_calls + 1
        state = get_save_state()
        if sequence == "movement" and state.native_physics_ticks > 0:
            schedule_verified = schedule_verified + 1
        sequence = "fixed"
    if context.stage == "update":
        update_calls = update_calls + 1
    if context.stage == "streaming":
        stream_calls = stream_calls + 1
func movement(context):
    movement_calls = movement_calls + 1
    sequence = "movement"
    options = record()
    options.right = 0
    options.forward = 0
    engine_default(options)
    return true
func rendering(context):
    render_calls = render_calls + 1
    options = record()
    options.world = false
    options.terrain = false
    options.effects = false
    engine_default(options)
    return true
@Events
on_start():
    physics = record()
    physics.gravity = 2
    physics.max_slope = 45
    configure_engine("physics", physics)
    schedule = record()
    schedule.fixed_order = ["movement", "physics", "fixed"]
    configure_engine("scheduler", schedule)
    register_engine_extension("engine_probe:fixed", "fixed", observer)
    register_engine_extension("engine_probe:update", "update", observer)
    register_engine_extension("engine_probe:stream", "streaming", observer)
    options = record()
    options.mode = "replace"
    options.replaces = "core:default"
    options.api = 1
    register_engine_extension("engine_probe:motion", "movement", movement, options)
    register_engine_extension("engine_probe:render", "render", rendering, options)
on_ready():
    async_job = background()
    scene = record()
    light = record()
    light.position = vec3(8, 4, 8)
    light.color = vec3(1, 0.5, 0.2)
    light.range = 8
    light.intensity = 2
    scene.lights = [light]
    configure_engine("render", scene)
)");
        app.userDirectory_=root/"user";app.explicitMods_=true;app.allowNative_=true;app.windowConfig_.hidden=true;app.noVsyncOverride_=true;app.autoWorldName_="Engine API";app.autoWorldSeed_="engine-api";check(app.initialize(),"Engine integration startup failed");
        check(app.physics_->rules().gravity==2 && app.renderer_->pointLights().size()==1,"Engine services did not reach the real application");check(app.engine_->fixedOrder==std::vector<std::string>{"movement","physics","fixed"},"Application did not receive scheduler order");
        app.config_.maxFrames=80;app.config_.frameLimit=120;app.run();
        check(app.scripting_->global("engine_probe","async_ticks").number()>10,"Application did not advance async tasks");
        for(const auto* counter:{"fixed_calls","movement_calls","update_calls","stream_calls","render_calls"}) check(app.scripting_->global("engine_probe",counter).number()>0,"An actual application engine phase was not dispatched");
        check(app.renderer_->stats().draws==0,"Render replacement did not control actual scene submission");const auto state=Json::parse(encodeData(app.saves_->state("engine_probe")));check(state.at("native_physics_ticks").get<unsigned>()>10,"Native engine provider did not run or delegate physics");
        check(app.scripting_->global("engine_probe","schedule_verified").number()>10,"Configured execution order did not affect actual engine stages");
        const auto renderCalls=app.scripting_->global("engine_probe","render_calls").number();const auto asyncTicks=app.scripting_->global("engine_probe","async_ticks").number();const auto oldTask=app.scripting_->global("engine_probe","async_job");Event escape;escape.type=EventType::KeyDown;escape.key=Key::Escape;app.frontEnd_->event(escape);app.config_.maxFrames=1;app.run();check(app.renderer_->stats().draws>0 && app.scripting_->global("engine_probe","render_calls").number()==renderCalls,"Render extension interfered with the protected pause screen");check(app.scripting_->global("engine_probe","async_ticks").number()==asyncTicks,"Async tasks advanced during pause");app.frontEnd_->event(escape);
        const auto original=*app.currentWorld_;app.saveWorld();app.openWorld(original,true);check(app.physics_->rules().gravity==2,"Save/load reset the mod's physics rules");check(std::abs(app.physics_->rules().supportNormal-.7071067f)<.0001f,"Save/load reset the mod's slope rules");
        check(oldTask.member("cancelled",{}).boolean(),"Reload retained the old world's async tasks");
        app.frontEnd_->closeWorld();check(!app.engine_ && app.renderer_->pointLights().empty(),"Closing a world retained engine extensions or lights");
        const auto checkMods=app.frontEnd_->storage().read(original.id);check(checkMods.mods.contains("engine_probe"),"Save format lost selected engine mod");app.shutdown();
    }
};
}
int main() {try {const auto root=std::filesystem::current_path()/"user"/("engine-api-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));game::pipelineTests();game::scriptAndServices(root);game::EngineIntegrationAccess::run(root/"application");std::cout<<"PASS engine API: lifecycle, script/native providers, six stages, configurations and save compatibility\n";return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
