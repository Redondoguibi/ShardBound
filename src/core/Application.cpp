#include "core/Application.h"
#include "core/Log.h"
#include "assets/CoreAssets.h"
#include "ecs/RenderSystem.h"
#include "mods/ModLoader.h"
#include "game/PreparationScreen.h"
#include "scripting/Bridge.h"
#include "scripting/GameplayBridge.h"
#include "scripting/StudioBridge.h"
#include "world/WorldBridge.h"
#include "world/FiniteMap.h"
#include "core/AtomicFile.h"
#include "platform/UserPaths.h"
#include "scripting/EngineServices.h"
#include <Windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <intrin.h>
#include <cstring>
#include <algorithm>
#include <charconv>
#include <exception>
#include <string_view>
#include <stdexcept>
#include <cmath>
namespace game {
namespace {
std::filesystem::path launchPath(int index,const char* fallback) {
    int count=0;
    std::unique_ptr<wchar_t*,decltype(&LocalFree)> arguments(CommandLineToArgvW(GetCommandLineW(),&count),LocalFree);
    if(arguments && index<count) {
        const auto length=WideCharToMultiByte(CP_ACP,0,arguments.get()[index],-1,nullptr,0,nullptr,nullptr);
        std::string narrow(size_t(std::max(length,1)),'\0');
        if(length) WideCharToMultiByte(CP_ACP,0,arguments.get()[index],-1,narrow.data(),length,nullptr,nullptr);
        if(length && std::string_view(narrow.c_str())==fallback) return arguments.get()[index];
    }
    return fallback;
}
}
Application::~Application() { shutdown(); }
int Application::execute(int argc, char** argv) {
    static constexpr const char* usage="Usage: game [--frames N] [--fps N] [--hidden] [--no-vsync] [--mods DIRECTORY] [--allow-native] [--performance] [--portable] [--user-dir DIRECTORY] [--create-world NAME] [--seed SEED] [--open-world ID] [--test-map FILE]";
    try {
        root_ = std::filesystem::absolute(launchPath(0,argv[0])).parent_path();
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if(arg=="--help") {LOG_INFO("CORE",usage);return 0;}
            if (arg == "--frames" && i + 1 < argc) {
                const std::string_view value = argv[++i];
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), config_.maxFrames);
                if (error != std::errc{} || end != value.data() + value.size() || !config_.maxFrames)
                    throw std::invalid_argument("--frames requires a positive integer");
            } else if (arg == "--hidden") windowConfig_.hidden = true;
            else if(arg=="--performance") forcePerformance_=true;
            else if(arg=="--portable") portable_=true;
            else if (arg == "--no-vsync") { renderConfig_.vsync = false;noVsyncOverride_=true; }
            else if (arg == "--allow-native") allowNative_=true;
            else if (arg == "--mods" && i + 1 < argc) { ++i;modsDirectory_=launchPath(i,argv[i]);explicitMods_=true; }
            else if (arg == "--create-world" && i + 1 < argc) autoWorldName_=argv[++i];
            else if (arg == "--seed" && i + 1 < argc) autoWorldSeed_=argv[++i];
            else if (arg == "--open-world" && i + 1 < argc) openWorldId_=argv[++i];
            else if (arg == "--test-map" && i + 1 < argc) {++i;testMap_=std::filesystem::absolute(launchPath(i,argv[i]));}
            else if (arg == "--user-dir" && i + 1 < argc) {++i;userDirectory_=std::filesystem::absolute(launchPath(i,argv[i]));}
            else if (arg == "--fps" && i + 1 < argc) {
                const std::string_view value = argv[++i];
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), config_.frameLimit);
                if (error != std::errc{} || end != value.data() + value.size()) throw std::invalid_argument("--fps requires an unsigned integer (0 = unlimited)");
            } else throw std::invalid_argument(usage);
        }
        if(!testMap_.empty() && (!autoWorldName_.empty() || !openWorldId_.empty() || userDirectory_.empty()))
            throw std::invalid_argument("--test-map requires a separate --user-dir and cannot be combined with --create-world or --open-world");
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
    if(userDirectory_.empty()) userDirectory_=defaultUserDirectory(root_,portable_);
    window_ = std::make_unique<WindowSystem>(windowConfig_);
    renderer_ = std::make_unique<Renderer>(window_->nativeHandle(),window_->width(),window_->height(),root_ / "assets/core/shaders/forward.hlsl", renderConfig_);
    input_.setViewport(window_->width(),window_->height());
    vfs_.mount("core",root_ / "assets/core");
    assets_ = std::make_unique<AssetManager>(vfs_,*renderer_,userDirectory_/"cache");
    const auto discovered=discoverMods(root_/modsDirectory_);
    procedural_=!explicitMods_;
    for(const auto& mod:discovered.mods) if(std::filesystem::exists(mod.root/"world.json")) procedural_=true;
    if(!procedural_) registerCoreModels(*assets_);
    PreparationScreen preparation(*renderer_);
    ModLoader loader(vfs_,*assets_);
    const auto report=loader.load(root_/modsDirectory_,[&](std::string_view mod,size_t done,size_t total) {
        for (const auto& event:window_->pollEvents()) {
            if (event.type==EventType::Closed) throw PreparationCancelled();
            if (event.type==EventType::Resized) renderer_->resize(event.width,event.height);
        }
        if (!window_->minimized()) preparation.show(mod,done,total);
    });
    mods_=report.mods;
    availableMods_=mods_;
    for(const auto& mod:mods_) { const auto& v=mod.version.parts;modVersions_[mod.id]=std::to_string(v[0])+"."+std::to_string(v[1])+"."+std::to_string(v[2]); }
    if(procedural_) {
        WorldDefinition definition;try {definition=WorldDefinition::load(mods_);}catch(const std::exception& error) {LOG_WARN("MODS",error.what());}
        frontEnd_=std::make_unique<FrontEnd>(*renderer_,*window_,input_,userDirectory_,definition,modVersions_);
        std::set<std::string> available;for(const auto& mod:availableMods_) available.insert(mod.id);
        frontEnd_->configureMods(discovered.mods,std::move(available),report.diagnostics);
        frontEnd_->startWorld=[this](const auto& record,bool resume){ openWorld(record,resume); };
        frontEnd_->saveWorld=[this]{ saveWorld(); };
        frontEnd_->closeWorld=[this]{ clearSession();currentWorld_.reset(); };
        frontEnd_->applySettings=[this](const auto& settings){ applySettings(settings); };
        frontEnd_->exportPerformance=[this]{return exportPerformance();};
        applySettings(frontEnd_->settings());
        if(!testMap_.empty()) {
            auto record=frontEnd_->createMapTestRecord(testMap_);
            frontEnd_->open(record,false);
        } else if(!autoWorldName_.empty()) {
            auto record=frontEnd_->createRecord(autoWorldName_,autoWorldSeed_);
            frontEnd_->open(record,false);
        }
        else if(!openWorldId_.empty()) frontEnd_->open(frontEnd_->storage().read(openWorldId_),true);
    } else {initializeSession(userDirectory_/"saves");if(forcePerformance_) applySettings(GameSettings{});}
    initialized_ = true;
    LOG_INFO("CORE", "Runtime initialized");
    return true;
}
void Application::clearSession() {
    if(window_) window_->relativeMouse(false);
    if(saves_ && worldSavePending_) {try {pollWorldSave(true);}catch(const std::exception& e) {LOG_ERROR("SAVE",e.what());}}worldSavePending_=false;worldSaveRequest_.reset();
    if(renderer_) {renderer_->clearShaderEffects();renderer_->setEnvironment({});renderer_->setPointLights({});}shaderTime_=0;
    terrain_.reset();native_.reset();engine_.reset();extensions_.reset();saves_.reset();components_.reset();
    developer_.reset();scripting_.reset();ui_.reset();studio_.reset();player_.reset();physics_.reset();audio_.reset();world_.reset();
    input_.keyboardMovementOnly(false);
}
void Application::initializeSession(const std::filesystem::path& directory) {
    clearSession();
    world_=std::make_unique<World>();physics_=std::make_unique<PhysicsSystem>(*world_);player_=std::make_unique<PlayerController>(*world_,*physics_);
    try { audio_=std::make_unique<AudioSystem>(); } catch(const DiagnosticError& error) { LOG_WARN("AUDIO",error.what());audio_=std::make_unique<AudioSystem>(true); }
    scripting_=std::make_unique<script::Runtime>();ui_=std::make_unique<GameUi>(*scripting_,*renderer_,&input_);
    bindRuntimeApi(*scripting_,*world_,*assets_);bindGameplayApi(*scripting_,*world_,*physics_,*player_,input_,*audio_,*assets_);
    studio_=std::make_unique<StudioRuntime>(*world_,*renderer_,*audio_);bindStudioApi(*scripting_,*world_,*studio_);
    components_=std::make_unique<Components>();saves_=std::make_unique<SaveGame>(directory,*world_,*physics_,*player_,*assets_);saves_->setPolicies(mods_);
    developer_=std::make_unique<DeveloperMode>(*scripting_,*world_,*physics_,*player_,input_);
    ui_->performanceReport=[this]{return profiler_.summary();};ui_->performanceHistory=[this]{return profiler_.history();};ui_->performanceVisible(profiler_.enabled());
    saves_->definitionAvailable=[this](const std::string& id){return scripting_->registry().contains("Entities",id);};
    saves_->includeEntity=[this](EntityId id){return !terrain_ || !terrain_->transient(id);};
    extensions_=std::make_unique<ExtensionBridge>(*scripting_,*world_,*components_,*saves_);native_=std::make_unique<NativeMods>(*scripting_,*world_,*components_,*saves_);
    engine_=std::make_unique<EngineBridge>(*scripting_);bindEngineServices(*engine_,*physics_,*renderer_);
    native_->engineOperation=[this](const auto& owner,const auto& operation,const auto& input){return engine_->request(owner,operation,input);};
    for(const auto& mod:mods_) {
        if(mod.dependencyCycle) continue;
        try {
            std::vector<std::string> dependencies;
            for(const auto& dep:mod.dependencies) for(const auto& installed:mods_)
                if(installed.id==dep.id && !installed.dependencyCycle && dep.accepts(installed.version)) dependencies.push_back(dep.id);
            scripting_->registry().dependencies(mod.id,dependencies);
            if(!mod.entry.empty()) scripting_->load(mod.id,mod.root,mod.entry,dependencies);
        } catch(const std::exception& error) { if(procedural_) throw;LOG_ERROR("SCRIPT",mod.id+": "+error.what()); }
        if(!mod.native.empty()) {
            if(!allowNative_) LOG_WARN("NATIVE",mod.id+": native extension disabled; use --allow-native for trusted DLLs");
            else try { native_->load(mod.id,mod.root,mod.native); } catch(const std::exception& error) { LOG_ERROR("NATIVE",error.what()); }
        }
    }
    ui_->diagnostics=[this] {
        const auto data=scripting_->debugSnapshot();const auto& report=std::get<Data::Object>(data.value);std::string result="ShardBound | F3: diagnósticos | custos em ms (CPU)";
        result+="\nEntidades: "+std::to_string(world_->entities().size());
        result+=" | passes de pós-processamento: "+std::to_string(renderer_->postEffectCount());
        if(terrain_) { const auto stats=terrain_->streamingStats();result+=" | chunks: "+std::to_string(stats.loaded)+" | fila: "+std::to_string(stats.queued)+" | prontos: "+std::to_string(stats.ready);result+="\nPáginas residentes: "+std::to_string(stats.residentPages)+" / "+std::to_string(stats.pages)+" | gerados: "+std::to_string(stats.generated)+" | descartados: "+std::to_string(stats.discarded); }
        const auto& systems=std::get<Data::Array>(report.at("systems").value);size_t count=0;
        for(const auto& entry:systems) { if(count++>=9) break;const auto& row=std::get<Data::Object>(entry.value);result+="\n"+std::get<std::string>(row.at("id").value)+" | último: "+std::to_string(std::get<double>(row.at("last_ms").value))+" | pico: "+std::to_string(std::get<double>(row.at("peak_ms").value))+" | erros: "+std::to_string(int(std::get<double>(row.at("errors").value))); }
        const auto& errors=std::get<Data::Array>(report.at("diagnostics").value);result+="\nDiagnósticos recentes: "+std::to_string(errors.size());
        for(size_t i=errors.size()>3?errors.size()-3:0;i<errors.size();++i) { const auto& row=std::get<Data::Object>(errors[i].value);result+="\n"+std::get<std::string>(row.at("mod").value)+" | "+std::get<std::string>(row.at("file").value)+":"+std::to_string(int(std::get<double>(row.at("line").value)))+" | "+std::get<std::string>(row.at("message").value); }
        if(engine_) {const auto snapshot=engine_->pipeline.inspect();const auto& fields=std::get<Data::Object>(snapshot.value);result+="\nEngine: "+std::to_string(std::get<Data::Array>(fields.at("extensions").value).size())+" extensoes | erros: "+std::to_string(std::get<Data::Array>(fields.at("diagnostics").value).size());}
        return result;
    };
    saves_->setMods(modVersions_);
    if(!procedural_) {
        for(const auto& diagnostic:scripting_->emit("on_start")) LOG_ERROR("SCRIPT",DiagnosticError(diagnostic).what());
        for(const auto& diagnostic:extensions_->process()) LOG_WARN("SAVE",DiagnosticError(diagnostic).what());
        for(const auto& diagnostic:scripting_->emit("on_ready")) LOG_ERROR("SCRIPT",DiagnosticError(diagnostic).what());
    }
}
void Application::applySettings(const GameSettings& settings) {
    settings.validate();
    renderer_->configure({settings.vsync && !noVsyncOverride_,settings.scale});renderer_->setShadows(settings.shadows);
    window_->fullscreen(settings.fullscreen && !windowConfig_.hidden);if(audio_) {audio_->setVolume(settings.volume);audio_->setBusVolume(AudioBus::Effects,settings.effectsVolume);audio_->setBusVolume(AudioBus::Music,settings.musicVolume);audio_->setBusVolume(AudioBus::Interface,settings.interfaceVolume);}
    input_.overrides(settings.bindings);input_.uiScale(settings.uiScale);renderer_->uiScale(settings.uiScale);
    if(developer_) developer_->sensitivity(settings.sensitivity);
    const bool enabled=settings.performance || forcePerformance_;renderer_->performance(enabled);profiler_.enabled(enabled);if(ui_) ui_->performanceVisible(enabled);
}
std::string Application::exportPerformance() {
    if(!profiler_.size()) throw std::runtime_error("Ative o diagnostico e jogue antes de exportar");
    nlohmann::json metadata={{"mods_at_export",modVersions_}};if(currentWorld_) metadata["world_at_export"]={{"id",currentWorld_->id},{"seed",currentWorld_->seed},{"generator",currentWorld_->generator}};
    if(engine_) metadata["engine_at_export"]=nlohmann::json::parse(encodeData(engine_->pipeline.inspect()));
    SYSTEM_INFO system{};GetNativeSystemInfo(&system);metadata["hardware"]["logical_processors"]=system.dwNumberOfProcessors;MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);if(GlobalMemoryStatusEx(&memory)) metadata["hardware"]["physical_memory_bytes"]=memory.ullTotalPhys;
    int info[4]{};__cpuid(info,int(0x80000000u));if(unsigned(info[0])>=0x80000004u) {char brand[49]{};for(unsigned i=0;i<3;++i) {__cpuid(info,int(0x80000002u+i));std::memcpy(brand+i*16,info,16);}metadata["hardware"]["cpu"]=brand;}
    if(frontEnd_) {const auto& s=frontEnd_->settings();metadata["settings_at_export"]={{"render_scale",s.scale},{"view_distance",s.viewDistance},{"vsync",s.vsync && !noVsyncOverride_},{"shadows",s.shadows},{"ui_scale",s.uiScale}};}
    const auto file=userDirectory_/"performance"/("session-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".json");
    atomicWrite(file,encodeData(profiler_.report(renderer_->deviceName(),decodeData(metadata.dump()))));return "Relatorio salvo: "+file.string();
}
void Application::openWorld(const WorldRecord& record,bool resume) {
    if(!availableMods_.empty()) {mods_.clear();modVersions_.clear();for(const auto& mod:availableMods_) if(record.mods.contains(mod.id)) {mods_.push_back(mod);const auto v=mod.version.parts;modVersions_[mod.id]=std::to_string(v[0])+"."+std::to_string(v[1])+"."+std::to_string(v[2]);}}
    currentWorld_=record;
    try {
        for(const auto& [id,v]:record.mods) {(void)v;if(!modVersions_.contains(id)) throw std::runtime_error("Required world mod absent: "+id);}
        for(const auto& mod:mods_) {if(mod.dependencyCycle) throw std::runtime_error("Mod dependency cycle: "+mod.id);for(const auto& dep:mod.dependencies) {const auto found=std::find_if(mods_.begin(),mods_.end(),[&](const auto& value){return value.id==dep.id;});if((found==mods_.end() && !dep.optional) || (found!=mods_.end() && !dep.accepts(found->version))) throw std::runtime_error(mod.id+": dependencia ausente ou incompativel: "+dep.id);}}
        initializeSession(frontEnd_->storage().directory(record.id));
        if(resume) {
            // Metadata may lag a successfully published checkpoint after interruption; the checkpoint is authoritative.
            const auto saved=nlohmann::json::parse(encodeData(SaveGame::inspect(saves_->directory()/"checkpoint.json")));
            auto versions=saved.at("mods").get<std::map<std::string,std::string>>();
            // Explicit world selection can disable a mod whose orphaned data is
            // retained in the checkpoint. Missing still-selected mods are errors.
            std::erase_if(versions,[&](const auto& item){return !record.mods.contains(item.first);});
            saves_->checkCompatibility(versions,saved.value("mod_schemas",std::map<std::string,unsigned>{}));
        }
        terrain_=std::make_unique<ChunkWorld>(*renderer_,*assets_,*world_,*physics_,*player_,TerrainGenerator(record.definition,record.generator,record.seed));
        developer_->terrain(terrain_.get(),record.seed);
        auto emit=[this](const std::string& event){auto errors=scripting_->emit(event);if(!errors.empty()) throw DiagnosticError(errors.front());};
        bindWorldApi(*scripting_,*terrain_,record);
        bindWorldPersistence(*saves_,*terrain_);
        emit("on_start");
        if(resume) {
            auto diagnostics=saves_->load("checkpoint");for(const auto& d:diagnostics) LOG_WARN("SAVE",DiagnosticError(d).what());
            emit("on_loaded");
        } else emit("on_world_start");
        if(!player_->entity()) throw std::runtime_error("Um mod de gameplay deve criar e possuir o jogador");
        terrain_->update(frontEnd_->settings().viewDistance);emit("on_ready");applySettings(frontEnd_->settings());
        player_->updateCamera(0);autosave_=0;saveWorld();LOG_INFO("WORLD","World ready: "+record.name+" ("+record.generator+")");
    } catch(...) { clearSession();currentWorld_.reset();throw; }
}
void Application::pollWorldSave(bool wait) {
    if(!worldSavePending_ || !saves_) return;
    try {if(!saves_->finishSave(wait,worldSaveRequest_)) return;}catch(...) {worldSavePending_=false;worldSaveRequest_.reset();throw;}
    worldSavePending_=false;worldSaveRequest_.reset();frontEnd_->saved();
    for(const auto& error:scripting_->emit("on_saved")) LOG_WARN("SAVE",DiagnosticError(error).what());
}
void Application::saveWorld(bool background) {
    if(!currentWorld_ || !saves_ || !terrain_) return;
    if(worldSavePending_) {if(background) return;pollWorldSave(true);}
    auto errors=scripting_->emit("on_before_save");if(!errors.empty()) throw DiagnosticError(errors.front());
    auto record=*currentWorld_;record.mods=modVersions_;for(const auto& mod:mods_) record.schemas[mod.id]=mod.saveSchema;
    const auto storage=frontEnd_->storage();auto publish=[storage,record]{storage.update(record);};
    if(background) {worldSaveRequest_=saves_->requestSave("checkpoint",publish);worldSavePending_=true;}
    else {saves_->save("checkpoint",publish);frontEnd_->saved();for(const auto& error:scripting_->emit("on_saved")) LOG_WARN("SAVE",DiagnosticError(error).what());}
    *currentWorld_=std::move(record);autosave_=0;
}
void Application::run() {
    if (!initialized_) throw std::logic_error("Application is not initialized");
    LOG_INFO("CORE", "Running");
    LoopCallbacks callbacks;
    callbacks.poll = [&] {
        frameStart_=std::chrono::steady_clock::now();simulationMs_=streamingMs_=0;
        menuFrame_=frontEnd_ && !frontEnd_->playing();
        input_.beginFrame();
        for (const auto& event : window_->pollEvents()) {
            input_.process(event);
            const bool playing=!frontEnd_ || frontEnd_->playing();
            const bool captured=developer_ && developer_->event(event,playing && !ui_->paused(),ui_->modalInput());
            if(ui_ && playing && !captured) { if(event.type==EventType::KeyDown && event.key==Key::F3 && !event.repeat) ui_->toggleDiagnostics();ui_->event(event); }
            if(frontEnd_ && !event.repeat) frontEnd_->event(event,captured || (ui_ && ui_->capturesMenuInput(event)));
            if (event.type == EventType::Closed) loop_.stop();
            if (event.type == EventType::Resized) renderer_->resize(event.width,event.height);
        }
        if(ui_ && (!frontEnd_ || frontEnd_->playing())) { ui_->updateInput();if(developer_) {developer_->update(0,!ui_->paused(),ui_->modalInput());developer_->processCommands();}window_->textInput(ui_->textInput() || (developer_ && developer_->textInput())); }
        if(developer_ && frontEnd_ && !frontEnd_->playing()) developer_->update(0,false);
        window_->relativeMouse(developer_ && developer_->relativeMouse());
        if(worldSavePending_) {try {pollWorldSave();}catch(const std::exception& error) {LOG_ERROR("SAVE",error.what());frontEnd_->error(error.what());}}
        if(frontEnd_) { frontEnd_->update();if(frontEnd_->quitRequested()) loop_.stop();if(terrain_ && frontEnd_->playing() && !ui_->paused()) {const auto start=std::chrono::steady_clock::now();const auto center=developer_->streamingCenter();
            engine_->pipeline.run("streaming",{{"distance",Data{double(frontEnd_->settings().viewDistance)}},{"center",center?engineVector(*center):Data{}}},[&](const Data& context){const auto& value=std::get<Data::Object>(context.value).at("center");terrain_->update(int(engineNumber(context,"distance")),false,std::holds_alternative<std::monostate>(value.value)?std::optional<Vec3>{}:std::optional<Vec3>{engineVector(value)});},validateEngineContext);
            streamingMs_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();} }
        else if (input_.pressed("Quit") && !ui_->handlesPause()) loop_.stop();
    };
    callbacks.suspended = [&] {if(window_->minimized()) lastPresent_={};if(studio_) studio_->setSuspended(window_->minimized()||ui_->paused()||(frontEnd_ && !frontEnd_->playing())); return window_->minimized(); };
    callbacks.fixedUpdate=[&](double delta) {
        if(!world_ || ui_->paused() || (frontEnd_ && (!frontEnd_->playing() || menuFrame_))) return;
        const auto start=std::chrono::steady_clock::now();
        const auto order=engine_->fixedOrder;
        for(const auto& stage:order) {
            if(stage=="fixed") engine_->pipeline.run(stage,{{"delta",Data{delta}}},[&](const Data& context){for(const auto& d:scripting_->emit("on_fixed_update",{script::Value(engineNumber(context,"delta"))})) LOG_ERROR("SCRIPT",DiagnosticError(d).what());},validateEngineContext);
            else if(stage=="movement") {const auto actor=player_->entity();const auto key=actor?world_->get<IdentityComponent>(actor).persistent:std::string{};engine_->pipeline.run(stage,{{"right",Data{double(input_.moveX())}},{"forward",Data{double(input_.moveY())}},{"player_key",Data{key}}},[&](const Data& context){player_->move(float(engineNumber(context,"right")),float(engineNumber(context,"forward")));},validateEngineContext);}
            else engine_->pipeline.run(stage,{{"delta",Data{delta}}},[&](const Data& context){physics_->step(float(engineNumber(context,"delta")));},validateEngineContext);
        }
        simulationMs_+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    };
    callbacks.update=[&](double delta) {
        if(!world_ || (frontEnd_ && (!frontEnd_->playing() || menuFrame_))) return;
        if(terrain_ && !ui_->paused() && !ui_->modalInput() && !developer_->textInput() && input_.pressed("Interact")) {
            if(terrain_->useDoor(frontEnd_->settings().viewDistance)) autosave_=60;
        }
        if(!ui_->paused()) for(const auto& d:scripting_->advanceTasks(delta)) LOG_ERROR("SCRIPT",DiagnosticError(d).what());
        engine_->pipeline.run("update",{{"delta",Data{delta}}},[&](const Data& context){for(const auto& d:scripting_->emit("on_update",{script::Value(engineNumber(context,"delta"))})) LOG_ERROR("SCRIPT",DiagnosticError(d).what());},validateEngineContext);
        for(const auto& d:extensions_->process()) LOG_WARN("SAVE",DiagnosticError(d).what());
        studio_->setSuspended(ui_->paused());
        studio_->update(delta);
        if(!ui_->paused()) {shaderTime_+=delta;renderer_->setShaderTime(shaderTime_); }
        if(audio_->offline()) (void)audio_->mix(static_cast<unsigned>(delta*48000));
        audio_->update();
        if(!ui_->paused()) player_->updateCamera(static_cast<float>(delta));
        if(developer_) developer_->update(static_cast<float>(delta),!ui_->paused(),ui_->modalInput());
        if(ui_->quitRequested()) loop_.stop();
        if(terrain_) {
            autosave_+=delta;
            if(input_.pressed("Save") || autosave_>=60) { try { saveWorld(true); } catch(const std::exception& error) { LOG_ERROR("SAVE",error.what());frontEnd_->error(error.what());autosave_=0; } }
        }
    };
    callbacks.render = [&](double) {
        const auto camera=developer_ ? developer_->camera() : player_ ? player_->camera() : Camera{};
        const bool menu=frontEnd_ && !frontEnd_->playing();
        if(menu) {
            auto items=world_?collectRenderItems(*world_):std::vector<RenderItem>{};
            if(studio_) {auto effects=studio_->render(camera);items.insert(items.end(),effects.begin(),effects.end());}
            if(terrain_) {auto ground=terrain_->renderItems();items.insert(items.end(),ground.begin(),ground.end());}
            renderer_->setCanvasOverlay(true);renderer_->render(camera,items,frontEnd_->texture());
        }
        else {
            bool drawn=false;const auto draw=[&](const Data& context) {
                const auto view=engineCamera(std::get<Data::Object>(context.value).at("camera"));auto items=engineBool(context,"world") && world_?collectRenderItems(*world_):std::vector<RenderItem>{};
                if(engineBool(context,"effects") && studio_) {auto effects=studio_->render(view);items.insert(items.end(),effects.begin(),effects.end());}
                if(engineBool(context,"terrain") && terrain_) {auto ground=terrain_->renderItems();items.insert(items.end(),ground.begin(),ground.end());}
                const bool hud=engineBool(context,"ui");if(ui_ && hud) ui_->renderWidgets(*renderer_);
                renderer_->render(view,items,hud && ui_?ui_->texture():TextureHandle{},developer_?developer_->render(*renderer_):TextureHandle{},hud && ui_?ui_->underlay():TextureHandle{});drawn=true;
            };
            engine_->pipeline.run("render",{{"camera",engineCamera(camera)},{"world",Data{true}},{"terrain",Data{true}},{"effects",Data{true}},{"ui",Data{true}}},draw,validateEngineContext);
            // A fully handled render can omit the scene, but never reuses stale GPU statistics or blocks presentation.
            if(!drawn) {if(ui_) ui_->renderWidgets(*renderer_);renderer_->render(camera,std::vector<RenderItem>{},ui_?ui_->texture():TextureHandle{},developer_?developer_->render(*renderer_):TextureHandle{},ui_?ui_->underlay():TextureHandle{});}
        }
        const auto submitted=std::chrono::steady_clock::now();renderer_->present();const auto presented=std::chrono::steady_clock::now();
        if(profiler_.enabled() && lastPresent_!=std::chrono::steady_clock::time_point{}) {
            if(presented-memorySample_>=std::chrono::seconds(1)) {PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))) privateBytes_=memory.PrivateUsage;memorySample_=presented;}
            const auto stats=renderer_->stats();profiler_.sample({std::chrono::duration<double,std::milli>(presented-lastPresent_).count(),std::chrono::duration<double,std::milli>(submitted-frameStart_).count(),stats.gpuMilliseconds,std::chrono::duration<double,std::milli>(presented-submitted).count(),simulationMs_,streamingMs_,privateBytes_,stats.draws,world_?world_->entityCount():0,terrain_?terrain_->streamingStats().loaded:0,!menu});
        }
        lastPresent_=presented;
    };
    loop_.run(config_, callbacks);
}
void Application::shutdown() noexcept {
    loop_.stop();
    if(initialized_ && profiler_.enabled() && profiler_.size()) {try {LOG_INFO("PERFORMANCE",exportPerformance());}catch(const std::exception& error) {LOG_WARN("PERFORMANCE",error.what());}}
    if(initialized_ && currentWorld_) { try { saveWorld(); } catch(const std::exception& error) { LOG_ERROR("SAVE",error.what()); } }
    if(initialized_ && scripting_) {
        try { for(const auto& d:scripting_->emit("on_shutdown")) LOG_WARN("SCRIPT",DiagnosticError(d).what()); }
        catch(...) { LOG_ERROR("SCRIPT","Shutdown event failed"); }
    }
    clearSession();frontEnd_.reset();currentWorld_.reset();
    assets_.reset();
    vfs_=VirtualFileSystem{};
    renderer_.reset();
    window_.reset();
    if (!initialized_) return;
    LOG_INFO("CORE", "Shutting down");
    initialized_ = false;
}
}
