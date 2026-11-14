#pragma once
#include "world/WorldDefinition.h"
#include "platform/InputSystem.h"
namespace game {
void writeJsonAtomic(const std::filesystem::path& path,const nlohmann::json& value);
struct GameSettings {
    bool vsync=true,fullscreen=false,shadows=true;
    float scale=1,volume=.7f;
    int viewDistance=3;
    float sensitivity=1,uiScale=1,effectsVolume=1,musicVolume=1,interfaceVolume=1;
    bool performance=false;
    std::map<std::string,std::vector<InputBinding>,std::less<>> bindings;
    static GameSettings load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
    void validate() const;
};
struct WorldRecord {
    std::string id,name,seed,generator;
    WorldDefinition definition;
    std::map<std::string,std::string> mods;
    std::map<std::string,unsigned> schemas;
};
class WorldStorage {
public:
    explicit WorldStorage(std::filesystem::path directory):directory_(std::move(directory)) {}
    WorldRecord create(std::string name,std::string seed,const std::string& generator,const WorldDefinition& definition,const std::map<std::string,std::string>& mods);
    void update(const WorldRecord& record) const;
    std::string backup(const std::string& id) const;
    std::vector<std::string> backups(const std::string& id) const;
    void restoreBackup(const std::string& id,const std::string& backup) const;
    size_t collect(const std::string& id) const;
    void recover(const std::string& id) const;
    WorldRecord read(const std::string& id) const;
    std::vector<WorldRecord> list(std::vector<std::string>* errors=nullptr) const;
    std::filesystem::path directory(const std::string& id) const;
private:
    std::filesystem::path directory_;
};
}
