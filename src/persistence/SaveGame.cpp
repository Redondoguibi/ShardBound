#include "persistence/SaveGame.h"
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <fstream>
#include <set>
#include <cmath>
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
void SaveGame::save(const std::string& slot) {
    const auto file=path(slot);
    try {
        Json json={{"format","shardbound-save"},{"version",1},{"mods",savedMods_},{"state",Json::parse(encodeData(Data{state_}))},{"entities",Json::array()}};
        for(const auto& [id,v]:mods_) json["mods"][id]=v;
        std::set<std::string> keys;
        for(auto id:world_.entities()) {
            const auto& identity=world_.get<IdentityComponent>(id);
            if(identity.persistent.empty() || !keys.insert(identity.persistent).second) throw std::runtime_error("Duplicate/empty persistent identity");
            Json e={{"id",identity.persistent},{"definition",identity.definition},{"owner",identity.owner},{"name",world_.get<NameComponent>(id).name},
                {"active",world_.get<ActiveComponent>(id).value},{"custom",Json::object()}};
            if(auto parent=world_.parent(id)) e["parent"]=world_.get<IdentityComponent>(parent).persistent;
            if(world_.has<TransformComponent>(id)) { const auto& t=world_.get<TransformComponent>(id).local; e["transform"]={vector(t.position),vector(t.rotation),vector(t.scale)}; }
            if(world_.has<ModelComponent>(id)) e["model"]=world_.get<ModelComponent>(id).definition.str();
            if(world_.has<HealthComponent>(id)) { const auto h=world_.get<HealthComponent>(id); e["health"]={h.current,h.maximum}; }
            if(world_.has<InventoryComponent>(id)) e["inventory"]=world_.get<InventoryComponent>(id).items;
            if(world_.has<InteractableComponent>(id)) e["interactable"]=world_.get<InteractableComponent>(id).enabled;
            for(const auto& [type,value]:world_.custom(id)) e["custom"][type]={{"version",value.version},{"data",Json::parse(encodeData(value.data))}};
            if(physics_.contains(id)) { auto box=physics_.description(id); e["physics"]={{"half",vector(box.halfExtent)},{"dynamic",box.dynamic},{"velocity",vector(physics_.velocity(id))}}; }
            json["entities"].push_back(std::move(e));
        }
        for(const auto& orphan:orphans_) {
            auto e=Json::parse(encodeData(orphan));
            if(!keys.insert(e.at("id").get<std::string>()).second) throw std::runtime_error("Persistent ID collides with orphan");
            json["entities"].push_back(std::move(e));
        }
        json["player"]=Json::parse(encodeData(orphanPlayer_));
        if(auto id=player_.entity()) json["player"]={{"entity",world_.get<IdentityComponent>(id).persistent},{"speed",player_.speed()}};
        const auto text=encodeData(decodeData(json.dump()));
        std::filesystem::create_directories(directory_);
        const auto temp=file.wstring()+L".tmp",backup=file.wstring()+L".bak";
        { std::ofstream out(temp,std::ios::binary|std::ios::trunc); out.write(text.data(),static_cast<std::streamsize>(text.size())); out.flush(); if(!out) throw std::runtime_error("Cannot write save"); }
        if(std::filesystem::exists(file)) {
            if(!ReplaceFileW(file.c_str(),temp.c_str(),backup.c_str(),0,nullptr,nullptr)) throw std::runtime_error("Cannot atomically replace save");
        } else if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot publish save");
    } catch(const std::exception& e) { throw DiagnosticError({"SAVE103",e.what(),file.string()}); }
}
std::vector<Diagnostic> SaveGame::load(const std::string& slot) {
    const auto file=path(slot); std::vector<Diagnostic> diagnostics;
    try {
        const auto json=read(file); auto savedMods=json.at("mods").get<std::map<std::string,std::string>>();
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
        std::map<std::string,EntityId> ids; Data::Array orphans;
        for(const auto& [key,e]:entries) {
            if(missing.contains(key)) { orphans.push_back(decodeData(e.dump())); diagnostics.push_back({"SAVE203","Entity definition unavailable; record retained",file.string(),{},key}); continue; }
            const auto id=staged.create(e.at("name").get<std::string>()); ids[key]=id;
            const auto definition=e.at("definition").get<std::string>();
            staged.add(id,IdentityComponent{key,definition,e.value("owner",definition.empty() ? std::string("core") : AssetId(definition).nameSpace())});
            staged.add(id,ActiveComponent{e.at("active").get<bool>()});
            if(e.contains("transform")) { const auto& t=e["transform"]; if(!t.is_array() || t.size()!=3) throw std::runtime_error("Invalid transform"); staged.add(id,TransformComponent{{vector(t[0]),vector(t[1]),vector(t[2])}}); }
            if(e.contains("model")) { AssetId model(e["model"].get<std::string>()); ModelHandle handle; try { handle=assets_.model(model); } catch(const std::exception& error) { diagnostics.push_back({"SAVE204",error.what(),file.string(),model.nameSpace(),model.str()}); } staged.add(id,ModelComponent{model,handle}); }
            if(e.contains("health")) { const auto& h=e["health"]; if(!h.is_array() || h.size()!=2) throw std::runtime_error("Invalid health"); float current=finite(h[0]),maximum=finite(h[1]); if(current<0 || maximum<=0 || current>maximum) throw std::runtime_error("Invalid health range"); staged.add(id,HealthComponent{current,maximum}); }
            if(e.contains("inventory")) {
                InventoryComponent inventory;
                for(const auto& [item,count]:e["inventory"].items()) { (void)AssetId(item); const auto n=count.get<double>(); if(n<1 || n>UINT32_MAX || n!=std::floor(n)) throw std::runtime_error("Invalid inventory count"); inventory.items[item]=static_cast<std::uint32_t>(n); }
                staged.add(id,std::move(inventory));
            }
            if(e.contains("interactable")) staged.add(id,InteractableComponent{e["interactable"].get<bool>()});
            if(!e.at("custom").is_object()) throw std::runtime_error("Invalid custom component map");
            for(const auto& [type,c]:e["custom"].items()) staged.setCustom(id,AssetId(type),{version(c.at("version")),decodeData(c.at("data").dump())});
            if(e.contains("physics")) { const auto& p=e["physics"]; stagedPhysics.attachBox(id,vector(p.at("half")),p.at("dynamic").get<bool>()); stagedPhysics.setVelocity(id,vector(p.at("velocity"))); }
        }
        for(const auto& [key,id]:ids) { const auto parent=entries.at(key).value("parent",std::string{}); if(!parent.empty()) { if(stagedPhysics.contains(id)) throw std::runtime_error("Physical child is not supported"); staged.setParent(id,ids.at(parent)); } }
        EntityId player{}; float speed=0; Data orphanPlayer;
        if(json.contains("player") && !json["player"].is_null()) {
            const auto key=json["player"].at("entity").get<std::string>(); speed=finite(json["player"].at("speed"));
            if(!entries.contains(key)) throw std::runtime_error("Player entity missing");
            if(missing.contains(key)) orphanPlayer=decodeData(json["player"].dump());
            else { player=ids.at(key); stagedPlayer.possess(player,speed); }
        }
        auto state=std::get<Data::Object>(decodeData(json.at("state").dump()).value);
        world_.swapState(staged); physics_.swapState(stagedPhysics); player_.release();
        if(player) player_.possess(player,speed);
        state_.swap(state); orphans_.swap(orphans); orphanPlayer_=std::move(orphanPlayer); savedMods_.swap(savedMods);
        return diagnostics;
    } catch(const std::exception& e) { throw DiagnosticError({"SAVE104",e.what(),file.string()}); }
}
}
