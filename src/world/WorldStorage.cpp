#include "world/WorldStorage.h"
#include "assets/VirtualFileSystem.h"
#include "assets/AssetId.h"
#include <Windows.h>
#include <fstream>
#include <random>
#include <chrono>
#include <cmath>
#include <algorithm>
#include "core/AtomicFile.h"
namespace game {
namespace {
using Json=nlohmann::json;
constexpr size_t maxWorldMetadata=16*1024*1024;
Json readJson(const std::filesystem::path& file,size_t limit=2*1024*1024) {
    if(std::filesystem::file_size(file)>limit) throw std::runtime_error("JSON file exceeds size limit");
    std::ifstream in(file,std::ios::binary);if(!in) throw std::runtime_error("Cannot open "+file.string());return Json::parse(in);
}
void writeWorldMetadata(const std::filesystem::path& path,const Json& metadata,bool backup) {
    const auto text=metadata.dump();
    if(text.size()>maxWorldMetadata) throw std::runtime_error("World metadata exceeds 16 MiB");
    atomicWrite(path,text,backup);
}
std::string randomId() { static std::mt19937_64 rng(std::random_device{}());return std::to_string(rng()); }
void validateText(const std::string& s,size_t max,const char* label) {
    if(s.empty() || s.size()>max || std::all_of(s.begin(),s.end(),[](unsigned char c) { return c<=32; })) throw std::runtime_error(std::string(label)+": texto vazio ou longo demais");
    for(unsigned char c:s) if(c<32 || c==127) throw std::runtime_error(std::string(label)+": caracteres de controle nao permitidos");
}
}
void writeJsonAtomic(const std::filesystem::path& file,const Json& value) {
    atomicWrite(file,value.dump(2),true);
}
GameSettings GameSettings::load(const std::filesystem::path& path) {
    GameSettings s;if(!std::filesystem::exists(path)) return s;
    auto j=readJson(path);if(j.at("version")!=1 && j.at("version")!=2) throw std::runtime_error("Unsupported settings version");
    s.vsync=j.at("vsync");s.fullscreen=j.at("fullscreen");s.shadows=j.at("shadows");s.scale=j.at("scale");s.volume=j.at("volume");s.viewDistance=j.at("view_distance");
    s.sensitivity=j.value("sensitivity",1.f);s.uiScale=j.value("ui_scale",1.f);s.performance=j.value("performance",false);
    s.effectsVolume=j.value("effects_volume",1.f);s.musicVolume=j.value("music_volume",1.f);s.interfaceVolume=j.value("interface_volume",1.f);
    if(!std::isfinite(s.scale+s.volume+s.sensitivity+s.uiScale+s.effectsVolume+s.musicVolume+s.interfaceVolume) || s.scale<.25f || s.scale>1 || s.volume<0 || s.volume>1 || s.viewDistance<2 || s.viewDistance>5 || s.sensitivity<.1f || s.sensitivity>4 || s.uiScale<.75f || s.uiScale>1 || s.effectsVolume<0 || s.effectsVolume>1 || s.musicVolume<0 || s.musicVolume>1 || s.interfaceVolume<0 || s.interfaceVolume>1) throw std::runtime_error("Invalid settings values");
    if(j.contains("bindings")) {
        if(!j["bindings"].is_object() || j["bindings"].size()>128) throw std::runtime_error("Invalid input bindings");
        for(const auto& [action,values]:j["bindings"].items()) {
            if(action.empty() || action.size()>128 || !values.is_array() || values.empty() || values.size()>8) throw std::runtime_error("Invalid input action");
            for(const auto& value:values) {InputBinding b;b.key=keyFromName(value.at("key"));if(!value.at("mouse").is_number_integer() || !value.at("pad").is_number_integer()) throw std::runtime_error("Invalid input button type");const int mouse=value.at("mouse"),pad=value.at("pad");if(mouse<0 || mouse>=int(MouseButton::Count) || pad<0 || pad>=int(PadButton::Count)) throw std::runtime_error("Invalid input button");b.mouse=MouseButton(mouse);b.pad=PadButton(pad);if(b.key==Key::Unknown && b.mouse==MouseButton::Unknown && b.pad==PadButton::Unknown) throw std::runtime_error("Empty input binding");s.bindings[action].push_back(b);}
        }
    }
    s.validate();return s;
}
void GameSettings::validate() const {
    if(!std::isfinite(scale+volume+sensitivity+uiScale+effectsVolume+musicVolume+interfaceVolume) || scale<.25f || scale>1 || volume<0 || volume>1 || viewDistance<2 || viewDistance>5 || sensitivity<.1f || sensitivity>4 || uiScale<.75f || uiScale>1 || effectsVolume<0 || effectsVolume>1 || musicVolume<0 || musicVolume>1 || interfaceVolume<0 || interfaceVolume>1 || bindings.size()>128) throw std::runtime_error("Invalid settings values");
    for(const auto& [action,values]:bindings) {if(action.empty() || action.size()>128 || values.empty() || values.size()>8 || std::any_of(action.begin(),action.end(),[](unsigned char c){return c<32;})) throw std::runtime_error("Invalid input action");for(const auto& b:values) if(size_t(b.key)>=size_t(Key::Count) || size_t(b.mouse)>=size_t(MouseButton::Count) || size_t(b.pad)>=size_t(PadButton::Count) || (b.key==Key::Unknown && b.mouse==MouseButton::Unknown && b.pad==PadButton::Unknown)) throw std::runtime_error("Invalid input binding");}
}
void GameSettings::save(const std::filesystem::path& path) const {
    validate();
    Json j={{"version",2},{"vsync",vsync},{"fullscreen",fullscreen},{"shadows",shadows},{"scale",scale},{"volume",volume},{"view_distance",viewDistance},{"sensitivity",sensitivity},{"ui_scale",uiScale},{"performance",performance},{"effects_volume",effectsVolume},{"music_volume",musicVolume},{"interface_volume",interfaceVolume},{"bindings",Json::object()}};
    for(const auto& [action,values]:bindings) {j["bindings"][action]=Json::array();for(const auto& b:values) j["bindings"][action].push_back({{"key",keyName(b.key)},{"mouse",int(b.mouse)},{"pad",int(b.pad)}});}
    writeJsonAtomic(path,j);
}
std::filesystem::path WorldStorage::directory(const std::string& id) const {
    if(id.empty() || id.size()>40 || !std::all_of(id.begin(),id.end(),[](char c){return c>='0' && c<='9';})) throw std::runtime_error("Invalid world ID");
    recover(id);
    // Validate symbolic links/junctions as well as the lexical filename.
    return VirtualFileSystem::contained(directory_,std::filesystem::path(id)/"world.json").parent_path();
}
WorldRecord WorldStorage::create(std::string name,std::string seed,const std::string& generator,const WorldDefinition& definition,const std::map<std::string,std::string>& mods) {
    validateText(name,96,"Nome");if(seed.empty()) seed=randomId();validateText(seed,128,"Seed");
    if(!definition.hasTerrain(generator)) throw std::runtime_error("Terreno indisponivel");
    std::filesystem::create_directories(directory_);std::string id;
    for(int attempt=0;attempt<16;++attempt) { id=randomId();if(std::filesystem::create_directory(directory_/id)) break;id.clear(); }
    if(id.empty()) throw std::runtime_error("Cannot allocate world directory");
    WorldRecord record{id,std::move(name),std::move(seed),generator,definition,mods};
    try {writeWorldMetadata(directory_/id/"world.json",{{"format","shardbound-world"},{"version",1},{"id",id},{"name",record.name},{"seed",record.seed},{"generator",generator},{"definition",definition.json()},{"mods",mods}},false);}
    catch(...) {std::error_code error;std::filesystem::remove(directory_/id,error);throw;}
    return record;
}
WorldRecord WorldStorage::read(const std::string& id) const {
    auto j=readJson(directory(id)/"world.json",maxWorldMetadata);if(j.at("format")!="shardbound-world" || j.at("version")!=1 || j.at("id")!=id) throw std::runtime_error("Unsupported world metadata");
    WorldRecord r{id,j.at("name"),j.at("seed"),j.at("generator"),WorldDefinition::parse(j.at("definition")),j.at("mods").get<std::map<std::string,std::string>>(),j.value("mod_schemas",std::map<std::string,unsigned>{})};
    if(j.contains("mod_schemas")) for(const auto& [modId,value]:j.at("mod_schemas").items()) {
        const auto schema=value.get<double>();if(!validNamespace(modId) || !std::isfinite(schema) || schema<1 || schema>1000000 || schema!=std::floor(schema)) throw std::runtime_error("Invalid world mod schema");
    }
    validateText(r.name,96,"Nome");validateText(r.seed,128,"Seed");if(!r.definition.hasTerrain(r.generator)) throw std::runtime_error("Missing saved terrain");return r;
}
void WorldStorage::update(const WorldRecord& record) const {
    const auto file=directory(record.id)/"world.json";
    auto json=readJson(file,maxWorldMetadata);json["mods"]=record.mods;json["mod_schemas"]=record.schemas;writeWorldMetadata(file,json,true);
}
std::vector<WorldRecord> WorldStorage::list(std::vector<std::string>* errors) const {
    std::vector<WorldRecord> result;if(!std::filesystem::exists(directory_)) return result;
    for(const auto& entry:std::filesystem::directory_iterator(directory_)) if(entry.is_regular_file()) {
        const auto name=entry.path().filename().string();if(name.starts_with("restore-") && name.ends_with(".json")) {
            try {recover(name.substr(8,name.size()-13));}catch(const std::exception& e) {if(errors) errors->push_back(e.what());}
        }
    }
    for(const auto& entry:std::filesystem::directory_iterator(directory_)) if(entry.is_directory() && entry.path().filename().string().find_first_not_of("0123456789")==std::string::npos) {
        try { result.push_back(read(entry.path().filename().string())); } catch(const std::exception& e) {
            const auto id=entry.path().filename().string();if(errors) errors->push_back(id+": "+e.what());
            try {if(!backups(id).empty()) {WorldRecord damaged;damaged.id=id;damaged.name="Mundo indisponivel: "+id;damaged.seed="restaure um backup";result.push_back(std::move(damaged));}}catch(const std::exception&) {}
        }
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) { return a.name<b.name || (a.name==b.name && a.id<b.id); });return result;
}
}
