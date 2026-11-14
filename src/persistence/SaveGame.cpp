#include "persistence/SaveGame.h"
#include "game/Equipment.h"
#include "game/Attributes.h"
#include "game/Projectile.h"
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <fstream>
#include <set>
#include <cmath>
#include <thread>
#include <chrono>
#include "core/IoExecutor.h"
#include "core/AtomicFile.h"
namespace game {
namespace {
using Json=nlohmann::json;
Json vector(Vec3 v) { return Json::array({v.x,v.y,v.z}); }
float finite(const Json& j) { const auto n=j.get<double>(); if(!std::isfinite(n) || std::abs(n)>1e20) throw std::runtime_error("Invalid save number"); return static_cast<float>(n); }
Vec3 vector(const Json& j) { if(!j.is_array() || j.size()!=3) throw std::runtime_error("Invalid vector"); return {finite(j[0]),finite(j[1]),finite(j[2])}; }
unsigned version(const Json& j) { const double n=j.get<double>(); if(n<1 || n>1000000 || n!=std::floor(n)) throw std::runtime_error("Invalid component version"); return static_cast<unsigned>(n); }
Json read(const std::filesystem::path& path) {
    if(std::filesystem::file_size(path)>16*1024*1024) throw std::runtime_error("Save exceeds 16 MiB");
    std::ifstream file(path,std::ios::binary); std::string text((std::istreambuf_iterator<char>(file)),{});
    if(!file) throw std::runtime_error("Cannot read save");
    (void)decodeData(text); auto result=Json::parse(text);
    if(result.at("format")!="shardbound-save" || result.at("version")!=1 || !result.at("entities").is_array() ||
        !result.at("mods").is_object() || !result.at("state").is_object()) throw std::runtime_error("Unsupported save format/version");
    return result;
}
}
SaveGame::SaveGame(std::filesystem::path directory,World& world,PhysicsSystem& physics,PlayerController& player,AssetManager& assets)
    :directory_(std::move(directory)),world_(world),physics_(physics),player_(player),assets_(assets) {}
void SaveGame::setMods(std::map<std::string,std::string> versions) { mods_=std::move(versions); }
std::filesystem::path SaveGame::writeDebugReport(const Data& data) const {
    const auto text=encodeData(data);const auto folder=directory_/"diagnostics";std::filesystem::create_directories(folder);const auto file=folder/"report.json",temp=folder/"report.json.tmp";
    { std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(text.data(),std::streamsize(text.size()));out.flush();if(!out) throw std::runtime_error("Cannot write diagnostics report"); }
    if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot publish diagnostics report");return file;
}
void SaveGame::setState(const std::string& mod,Data state) {
    if(!validNamespace(mod)) throw DiagnosticError({"SAVE101","Invalid mod state namespace"});
    (void)encodeData(state); state_[mod]=std::move(state);
}
Data SaveGame::state(const std::string& mod) const { const auto it=state_.find(mod); return it==state_.end() ? Data{} : it->second; }
std::filesystem::path SaveGame::path(const std::string& slot) const {
    if(!validNamespace(slot) || slot.size()>64) throw DiagnosticError({"SAVE101","Slot must contain only lowercase letters, digits, hyphens or underscores"});
    return directory_/(slot+".json");
}
Data SaveGame::inspect(const std::filesystem::path& file) {
    try { return decodeData(read(file).dump()); }
    catch(const std::exception& e) { throw DiagnosticError({"SAVE102",e.what(),file.string()}); }
}
SaveGame::~SaveGame() {if(pending_) pending_->wait();}
void SaveGame::setPolicies(const std::vector<ModManifest>& mods) {for(const auto& mod:mods) policies_.insert_or_assign(mod.id,mod);}
void SaveGame::registerMigration(const std::string& mod,unsigned from,std::function<Data(const Data&)> migration) {
    if(!validNamespace(mod) || !from || from>=1000000 || !migration || !migrations_[mod].emplace(from,std::move(migration)).second) throw std::runtime_error("Invalid/duplicate save migration");
}
void SaveGame::checkCompatibility(const std::map<std::string,std::string>& mods,const std::map<std::string,unsigned>& schemas) const {
    for(const auto& [id,version]:mods) {
        const auto policy=policies_.find(id);if(policy==policies_.end()) throw std::runtime_error("Required world mod absent: "+id);
        const auto it=schemas.find(id);const unsigned schema=it==schemas.end()?1:it->second;
        if(!schema || !policy->second.acceptsSave(version,schema)) throw std::runtime_error("World save incompatible with installed mod: "+id+" "+version);
        for(unsigned v=schema;v<policy->second.saveSchema;++v) {
            const auto chain=migrations_.find(id);if(chain==migrations_.end() || !chain->second.contains(v)) throw std::runtime_error("Missing save migration for "+id+" schema "+std::to_string(v));
        }
    }
}
nlohmann::json SaveGame::migrate(Json json) const {
    const auto versions=json.at("mods").get<std::map<std::string,std::string>>();
    std::map<std::string,unsigned> schemas;
    if(json.contains("mod_schemas")) {
        if(!json.at("mod_schemas").is_object()) throw std::runtime_error("Invalid save schema map");
        for(const auto& [id,value]:json.at("mod_schemas").items()) {
            if(!validNamespace(id) || !value.is_number()) throw std::runtime_error("Invalid save schema");
            const double schema=value.get<double>();if(!std::isfinite(schema) || schema<1 || schema>1000000 || schema!=std::floor(schema)) throw std::runtime_error("Invalid save schema");schemas[id]=unsigned(schema);
        }
    }
    for(const auto& [id,version]:versions) {
        const auto policy=policies_.find(id);if(policy==policies_.end()) continue;
        checkCompatibility({{id,version}},schemas);
        unsigned current=schemas.contains(id)?schemas.at(id):1;
        for(;current<policy->second.saveSchema;++current) {
            Json owned=Json::array();std::set<std::string> keys;
            for(const auto& e:json.at("entities")) if(e.value("owner",std::string("core"))==id) {owned.push_back(e);keys.insert(e.at("id").get<std::string>());}
            const Json input={{"state",json.at("state").value(id,Json())},{"entities",owned}};
            const auto changed=Json::parse(encodeData(migrations_.at(id).at(current)(decodeData(input.dump()))));
            if(!changed.is_object() || !changed.contains("state") || !changed.at("entities").is_array()) throw std::runtime_error("Migration must return state and entities");
            std::map<std::string,Json> replacement;
            for(const auto& e:changed.at("entities")) {
                const auto key=e.at("id").get<std::string>();if(!keys.contains(key) || e.value("owner",std::string{})!=id || !replacement.emplace(key,e).second) throw std::runtime_error("Migration changed entity ownership/identity");
            }
            if(replacement.size()!=keys.size()) throw std::runtime_error("Migration must preserve owned entity identities");
            json["state"][id]=changed.at("state");for(auto& e:json["entities"]) {const auto key=e.at("id").get<std::string>();if(replacement.contains(key)) e=replacement.at(key);}
        }
        schemas[id]=current;
    }
    json["mod_schemas"]=schemas;return json;
}
nlohmann::json SaveGame::capture() {
    if(prepareSave) prepareSave();
    Json state=Json::object();for(const auto& [id,value]:state_) if(!captureCore || id!="core") state[id]=Json::parse(encodeData(value));
        Json json={{"format","shardbound-save"},{"version",1},{"mods",savedMods_},{"state",std::move(state)},{"entities",Json::array()}};
        for(const auto& [id,v]:mods_) json["mods"][id]=v;
        std::set<std::string> keys;
        for(auto id:world_.entities()) {
            if(includeEntity && !includeEntity(id)) continue;
            validateEquipment(world_,id);validateProjectile(world_,id);(void)readAttributeBases(world_,id);
            const auto& identity=world_.get<IdentityComponent>(id);
            if(identity.persistent.empty() || !keys.insert(identity.persistent).second) throw std::runtime_error("Duplicate/empty persistent identity");
            Json e={{"id",identity.persistent},{"definition",identity.definition},{"owner",identity.owner},{"name",world_.get<NameComponent>(id).name},
                {"active",world_.get<ActiveComponent>(id).value},{"custom",Json::object()}};
            if(auto parent=world_.parent(id)) e["parent"]=world_.get<IdentityComponent>(parent).persistent;
            if(world_.has<TransformComponent>(id)) { const auto& t=world_.get<TransformComponent>(id).local; e["transform"]={vector(t.position),vector(t.rotation),vector(t.scale)}; }
            if(world_.has<ModelComponent>(id)) {
                const auto& model=world_.get<ModelComponent>(id); e["model"]=model.definition.str();
                e["tint"]={model.tint.r,model.tint.g,model.tint.b};
                if(model.shader) e["shader"]={{"id",model.shader->shaderId},{"parameters",model.shader->parameters},{"texture",model.shader->textureId}};
            }
            if(world_.has<HealthComponent>(id)) { const auto h=world_.get<HealthComponent>(id); e["health"]={h.current,h.maximum}; }
            if(world_.has<InventoryComponent>(id)) e["inventory"]=world_.get<InventoryComponent>(id).items;
            if(world_.has<InteractableComponent>(id)) e["interactable"]=world_.get<InteractableComponent>(id).enabled;
            for(const auto& [type,value]:world_.custom(id)) e["custom"][type]={{"version",value.version},{"data",Json::parse(encodeData(value.data))}};
            if(physics_.contains(id)) { auto box=physics_.description(id); e["physics"]={{"half",vector(box.halfExtent)},{"dynamic",box.dynamic},{"velocity",vector(physics_.velocity(id))}};if(box.rotation.x!=0 || box.rotation.y!=0 || box.rotation.z!=0) e["physics"]["rotation"]=vector(box.rotation); }
            json["entities"].push_back(std::move(e));
        }
        for(const auto& orphan:orphans_) {
            auto e=Json::parse(encodeData(orphan));
            if(!keys.insert(e.at("id").get<std::string>()).second) throw std::runtime_error("Persistent ID collides with orphan");
            json["entities"].push_back(std::move(e));
        }
        if(json["entities"].size()>4096) throw std::runtime_error("Save entity limit exceeded");
        for(const auto& entity:json["entities"]) if(entity.contains("parent") && !keys.contains(entity.at("parent").get<std::string>())) throw std::runtime_error("Saved entity refers to an excluded parent");
        json["player"]=Json::parse(encodeData(orphanPlayer_));
        if(auto id=player_.entity()) {json["player"]={{"entity",world_.get<IdentityComponent>(id).persistent},{"speed",player_.speed()},{"camera",vector(player_.viewOffset())}};if(player_.stepHeight()!=0) json["player"]["step_height"]=player_.stepHeight();}
    json["mod_schemas"]=savedSchemas_;for(const auto& [id,policy]:policies_) json["mod_schemas"][id]=policy.saveSchema;
    return json;
}
namespace {
void writeSnapshot(const std::filesystem::path& file,Json json,const std::function<Data()>& core,const std::function<void()>& after) {
    try {
        if(core) json["state"]["core"]=Json::parse(encodeData(core()));
        atomicWrite(file,encodeData(decodeData(json.dump())),true);if(after) after();
    } catch(const std::exception& error) {throw DiagnosticError({"SAVE103",error.what(),file.string()});}
}
}
bool SaveGame::finishSave(bool wait,SaveRequest request) {
    if(!request) request=pending_;if(!request) return true;
    if(!wait && request->wait_for(std::chrono::seconds(0))!=std::future_status::ready) return false;
    if(pending_==request) pending_.reset();
    request->get();return true;
}
SaveGame::SaveRequest SaveGame::requestSave(const std::string& slot,std::function<void()> afterWrite) {
    if(saving()) throw std::runtime_error("A save is already pending");
    const auto file=path(slot);auto json=capture();auto core=captureCore?captureCore():std::function<Data()>{};
    auto request=std::make_shared<std::shared_future<void>>();
    *request=IoExecutor::instance().submit([file,json=std::move(json),core=std::move(core),after=std::move(afterWrite)] {writeSnapshot(file,json,core,after);}).share();
    pending_=std::move(request);return pending_;
}
void SaveGame::save(const std::string& slot,std::function<void()> afterWrite) {
    finishSave(true);auto json=capture();auto core=captureCore?captureCore():std::function<Data()>{};writeSnapshot(path(slot),std::move(json),core,afterWrite);
}
std::vector<Diagnostic> SaveGame::load(const std::string& slot) {
    const auto file=path(slot); std::vector<Diagnostic> diagnostics;
    try {
        finishSave(true);const auto json=migrate(read(file)); auto savedMods=json.at("mods").get<std::map<std::string,std::string>>();
        for(const auto& [mod,v]:savedMods) {
            const auto it=mods_.find(mod);
            if(it==mods_.end()) diagnostics.push_back({"SAVE201","Mod absent; its state is retained",file.string(),mod});
            else if(it->second!=v) diagnostics.push_back({"SAVE202","Mod version changed from "+v+" to "+it->second,file.string(),mod});
        }
        if(json["entities"].size()>4096) throw std::runtime_error("Save entity limit exceeded");
        std::map<std::string,Json> entries; std::set<std::string> missing;
        for(const auto& e:json["entities"]) {
            const auto key=e.at("id").get<std::string>();
            if(key.empty() || !entries.emplace(key,e).second) throw std::runtime_error("Duplicate/empty persistent identity");
            const auto definition=e.at("definition").get<std::string>();
            const auto owner=e.value("owner",definition.empty() ? std::string("core") : AssetId(definition).nameSpace());
            if(!validNamespace(owner)) throw std::runtime_error("Invalid entity owner namespace");
            if(owner!="core" && !mods_.contains(owner)) missing.insert(key);
            if(!definition.empty()) { AssetId def(definition); if((def.nameSpace()!="core" && !mods_.contains(def.nameSpace())) || (definitionAvailable && !definitionAvailable(definition))) missing.insert(key); }
        }
        for(const auto& [key,e]:entries) {
            std::set<std::string> chain{key}; auto parent=e.value("parent",std::string{});
            while(!parent.empty()) {
                if(!entries.contains(parent) || !chain.insert(parent).second) throw std::runtime_error("Missing parent or hierarchy cycle");
                parent=entries.at(parent).value("parent",std::string{});
            }
        }
        bool changed=true;
        while(changed) { changed=false; for(const auto& [key,e]:entries) if(missing.contains(e.value("parent",std::string{}))) changed|=missing.insert(key).second; }
        World staged; PhysicsSystem stagedPhysics(staged); PlayerController stagedPlayer(staged,stagedPhysics);
        stagedPhysics.rules(physics_.rules());
        std::map<std::string,EntityId> ids; Data::Array orphans;
        for(const auto& [key,e]:entries) {
            if(missing.contains(key)) { orphans.push_back(decodeData(e.dump())); diagnostics.push_back({"SAVE203","Entity definition unavailable; record retained",file.string(),{},key}); continue; }
            const auto id=staged.create(e.at("name").get<std::string>()); ids[key]=id;
            const auto definition=e.at("definition").get<std::string>();
            staged.add(id,IdentityComponent{key,definition,e.value("owner",definition.empty() ? std::string("core") : AssetId(definition).nameSpace())});
            staged.add(id,ActiveComponent{e.at("active").get<bool>()});
            if(e.contains("transform")) { const auto& t=e["transform"]; if(!t.is_array() || t.size()!=3) throw std::runtime_error("Invalid transform"); staged.add(id,TransformComponent{{vector(t[0]),vector(t[1]),vector(t[2])}}); }
            if(e.contains("model")) { AssetId model(e["model"].get<std::string>()); ModelHandle handle; try { handle=assets_.model(model); } catch(const std::exception& error) { diagnostics.push_back({"SAVE204",error.what(),file.string(),model.nameSpace(),model.str()}); } staged.add(id,ModelComponent{model,handle}); }
            if(e.contains("shader")) {
                if(!staged.has<ModelComponent>(id)) throw std::runtime_error("Saved shader requires a model");
                const auto& spec=e.at("shader");const auto shaderId=spec.at("id").get<std::string>();(void)AssetId(shaderId);
                const auto texture=spec.at("texture").get<std::string>();if(!texture.empty()) (void)AssetId(texture);
                const auto& values=spec.at("parameters");if(!values.is_array() || values.size()!=16) throw std::runtime_error("Invalid saved shader parameters");
                ShaderParameters parameters{};for(size_t i=0;i<16;++i) {
                    const auto value=values[i].get<double>();if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max()) throw std::runtime_error("Invalid saved shader parameter");parameters[i]=float(value);
                }
                ShaderInstanceHandle shader;
                try {shader=assets_.shaderInstance(shaderId,parameters,texture);}
                catch(const DiagnosticError& error) {
                    if(error.diagnostic.code!="SHADER104" && error.diagnostic.code!="ASSET302") throw;
                    auto retained=std::make_shared<ShaderInstance>();retained->shaderId=shaderId;retained->textureId=texture;retained->parameters=parameters;shader=std::move(retained);
                    diagnostics.push_back({"SAVE205","Shader asset absent; binding retained with default rendering",file.string(),AssetId(shaderId).nameSpace(),shaderId});
                }
                staged.get<ModelComponent>(id).shader=std::move(shader);
            }
            if(e.contains("health")) { const auto& h=e["health"]; if(!h.is_array() || h.size()!=2) throw std::runtime_error("Invalid health"); float current=finite(h[0]),maximum=finite(h[1]); if(current<0 || maximum<=0 || current>maximum) throw std::runtime_error("Invalid health range"); staged.add(id,HealthComponent{current,maximum}); }
            if(e.contains("tint") && staged.has<ModelComponent>(id)) {
                const auto t=vector(e["tint"]); if(t.x<0 || t.x>1 || t.y<0 || t.y>1 || t.z<0 || t.z>1) throw std::runtime_error("Invalid model tint");
                staged.get<ModelComponent>(id).tint={t.x,t.y,t.z,1};
            }
            if(e.contains("inventory")) {
                InventoryComponent inventory;
                for(const auto& [item,count]:e["inventory"].items()) { (void)AssetId(item); const auto n=count.get<double>(); if(n<1 || n>UINT32_MAX || n!=std::floor(n)) throw std::runtime_error("Invalid inventory count"); inventory.items[item]=static_cast<std::uint32_t>(n); }
                staged.add(id,std::move(inventory));
            }
            if(e.contains("interactable")) staged.add(id,InteractableComponent{e["interactable"].get<bool>()});
            if(!e.at("custom").is_object()) throw std::runtime_error("Invalid custom component map");
            for(const auto& [type,c]:e["custom"].items()) staged.setCustom(id,AssetId(type),{version(c.at("version")),decodeData(c.at("data").dump())});
            validateEquipment(staged,id);validateProjectile(staged,id);(void)readAttributeBases(staged,id);
            if(e.contains("physics") && staged.custom(id).contains("core:projectile")) throw std::runtime_error("Saved projectile cannot have a physics body");
            if(e.contains("physics")) { const auto& p=e["physics"]; stagedPhysics.attachBox(id,vector(p.at("half")),p.at("dynamic").get<bool>(),p.contains("rotation")?vector(p.at("rotation")):Vec3{}); stagedPhysics.setVelocity(id,vector(p.at("velocity"))); }
        }
        for(const auto& [key,id]:ids) { const auto parent=entries.at(key).value("parent",std::string{}); if(!parent.empty()) { if(stagedPhysics.contains(id)) throw std::runtime_error("Physical child is not supported"); staged.setParent(id,ids.at(parent)); } }
        size_t projectileCount=0;for(auto id:staged.entities()) { validateProjectile(staged,id);projectileCount+=staged.custom(id).contains("core:projectile"); }
        if(projectileCount>512) throw std::runtime_error("Saved projectile limit exceeded");
        EntityId player{}; float speed=0,stepHeight=0; Vec3 camera{0,8,-10}; Data orphanPlayer;
        if(json.contains("player") && !json["player"].is_null()) {
            const auto key=json["player"].at("entity").get<std::string>(); speed=finite(json["player"].at("speed"));
            if(!entries.contains(key)) throw std::runtime_error("Player entity missing");
            if(missing.contains(key)) orphanPlayer=decodeData(json["player"].dump());
            else {
                player=ids.at(key); stagedPlayer.possess(player,speed);
                if(json["player"].contains("camera")) camera=vector(json["player"]["camera"]);
                stagedPlayer.setViewOffset(camera);
                if(json["player"].contains("step_height")) stepHeight=finite(json["player"]["step_height"]);stagedPlayer.stepHeight(stepHeight);
            }
        }
        auto state=std::get<Data::Object>(decodeData(json.at("state").dump()).value);
        if(validateState) validateState(Data{state});
        auto savedSchemas=json.value("mod_schemas",std::map<std::string,unsigned>{});
        world_.swapState(staged); physics_.swapState(stagedPhysics); player_.release();
        if(player) { player_.possess(player,speed); player_.setViewOffset(camera);player_.stepHeight(stepHeight); }
        state_.swap(state); orphans_.swap(orphans); orphanPlayer_=std::move(orphanPlayer); savedMods_.swap(savedMods);savedSchemas_.swap(savedSchemas);
        if(didLoad) didLoad();
        return diagnostics;
    } catch(const std::exception& e) { throw DiagnosticError({"SAVE104",e.what(),file.string()}); }
}
}
