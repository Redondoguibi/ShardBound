#pragma once
#include "game/PlayerController.h"
#include "core/Diagnostic.h"
#include <functional>
namespace game {
class SaveGame {
public:
    SaveGame(std::filesystem::path directory,World& world,PhysicsSystem& physics,PlayerController& player,AssetManager& assets);
    void setMods(std::map<std::string,std::string> versions);
    void setState(const std::string& mod,Data state);
    Data state(const std::string& mod) const;
    void save(const std::string& slot);
    std::vector<Diagnostic> load(const std::string& slot);
    static Data inspect(const std::filesystem::path& file);
    std::function<bool(const std::string&)> definitionAvailable;
    // Runs before committing the staged world; throwing leaves live state intact.
    std::function<void(const Data&)> validateState;
private:
    std::filesystem::path path(const std::string& slot) const;
    std::filesystem::path directory_;
    World& world_; PhysicsSystem& physics_; PlayerController& player_; AssetManager& assets_;
    std::map<std::string,std::string> mods_,savedMods_;
    Data::Object state_;
    Data::Array orphans_;
    Data orphanPlayer_;
};
}
