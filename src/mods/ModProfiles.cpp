#include "mods/ModProfiles.h"
#include "assets/AssetId.h"
#include "core/AtomicFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
namespace game {
using Json=nlohmann::json;
ModProfiles::ModProfiles(std::filesystem::path file,std::vector<ModManifest> catalog,std::set<std::string> available,bool ignoreSaved)
    :file_(std::move(file)),catalog_(std::move(catalog)),available_(std::move(available)) {
    active_="Padrao";profiles_[active_]=available_;
    if(ignoreSaved || !std::filesystem::exists(file_)) return;
    if(std::filesystem::file_size(file_)>1024*1024) throw std::runtime_error("Mod profiles exceed 1 MiB");
    std::ifstream input(file_);const auto json=Json::parse(input);
    if(json.at("version")!=1 || !json.at("profiles").is_object() || json["profiles"].empty() || json["profiles"].size()>32) throw std::runtime_error("Invalid mod profiles");
    Profiles loaded;
    for(const auto& [name,ids]:json["profiles"].items()) {
        if(name.empty() || name.size()>64 || std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;}) || !ids.is_array() || ids.size()>1024) throw std::runtime_error("Invalid mod profile");
        auto& selection=loaded[name];for(const auto& value:ids) {auto id=value.get<std::string>();if(!validNamespace(id)) throw std::runtime_error("Invalid profile mod ID");selection.insert(id);}
    }
    auto active=json.at("active").get<std::string>();if(!loaded.contains(active)) throw std::runtime_error("Missing active mod profile");profiles_=std::move(loaded);active_=std::move(active);
}
std::vector<std::string> ModProfiles::issues(const std::set<std::string>& selection) const {
    std::vector<std::string> errors;
    for(const auto& id:selection) {
        const auto mod=std::find_if(catalog_.begin(),catalog_.end(),[&](const auto& m){return m.id==id;});
        if(mod==catalog_.end() || !available_.contains(id)) {errors.push_back(id+": pacote indisponivel ou invalido");continue;}
        if(mod->dependencyCycle) errors.push_back(id+": ciclo de dependencias");
        for(const auto& dep:mod->dependencies) {
            const auto found=std::find_if(catalog_.begin(),catalog_.end(),[&](const auto& m){return m.id==dep.id;});
            if(!selection.contains(dep.id)) {if(!dep.optional) errors.push_back(id+": requer "+dep.id);}
            else if(found==catalog_.end() || !dep.accepts(found->version)) errors.push_back(id+": versao incompativel de "+dep.id);
        }
    }
    return errors;
}
std::vector<std::string> ModProfiles::issues() const {return issues(enabled());}
std::vector<ModManifest> ModProfiles::selected() const {
    const auto errors=issues();if(!errors.empty()) throw std::runtime_error(errors.front());
    std::vector<ModManifest> result;for(const auto& m:catalog_) if(enabled().contains(m.id)) result.push_back(m);return result;
}
void ModProfiles::store(Profiles profiles,std::string active) {
    Json json={{"version",1},{"active",active},{"profiles",Json::object()}};for(const auto& [name,ids]:profiles) json["profiles"][name]=ids;
    atomicWrite(file_,json.dump(2),true);profiles_=std::move(profiles);active_=std::move(active);
}
void ModProfiles::toggle(const std::string& id) {
    auto profiles=profiles_;auto& selection=profiles.at(active_);
    if(selection.erase(id)) {
        bool changed=true;while(changed) {changed=false;for(const auto& m:catalog_) if(selection.contains(m.id)) for(const auto& d:m.dependencies) if(!d.optional && !selection.contains(d.id)) {selection.erase(m.id);changed=true;break;}}
    } else {
        std::set<std::string> visited;std::function<void(const std::string&)> add=[&](const auto& next) {
            if(!visited.insert(next).second) return;
            const auto mod=std::find_if(catalog_.begin(),catalog_.end(),[&](const auto& m){return m.id==next;});
            if(mod==catalog_.end() || !available_.contains(next)) throw std::runtime_error("Mod indisponivel: "+next);
            selection.insert(next);for(const auto& dep:mod->dependencies) if(!dep.optional) add(dep.id);
        };add(id);const auto errors=issues(selection);if(!errors.empty()) throw std::runtime_error(errors.front());
    }
    store(std::move(profiles),active_);
}
void ModProfiles::next() {auto i=profiles_.find(active_);if(++i==profiles_.end()) i=profiles_.begin();store(profiles_,i->first);}
void ModProfiles::duplicate() {if(profiles_.size()>=32) throw std::runtime_error("Limite de 32 perfis");auto profiles=profiles_;size_t n=1;std::string name;do {name="Perfil "+std::to_string(n++);}while(profiles.contains(name));profiles[name]=enabled();store(std::move(profiles),name);}
void ModProfiles::rename(const std::string& name) {
    if(name==active_) return;
    if(name.empty() || name.size()>64 || std::all_of(name.begin(),name.end(),[](unsigned char c){return c<=32;}) || std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;}) || profiles_.contains(name)) throw std::runtime_error("Nome de perfil vazio, longo ou ja utilizado");
    auto profiles=profiles_;const auto selection=enabled();profiles.erase(active_);profiles.emplace(name,selection);store(std::move(profiles),name);
}
}
