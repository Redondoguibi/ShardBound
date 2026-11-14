#pragma once
#include "game/PlayerController.h"
#include "core/Diagnostic.h"
#include <functional>
#include <future>
#include "mods/Manifest.h"
namespace game {
class SaveGame {
public:
    SaveGame(std::filesystem::path directory,World& world,PhysicsSystem& physics,PlayerController& player,AssetManager& assets);
    ~SaveGame();
    void setPolicies(const std::vector<ModManifest>& mods);
    void registerMigration(const std::string& mod,unsigned from,std::function<Data(const Data&)> migration);
    void checkCompatibility(const std::map<std::string,std::string>& mods,const std::map<std::string,unsigned>& schemas) const;
    // A ticket retains this operation's result even when another API drains the pending slot.
    using SaveRequest=std::shared_ptr<const std::shared_future<void>>;
    SaveRequest requestSave(const std::string& slot,std::function<void()> afterWrite={});
    bool finishSave(bool wait=false,SaveRequest request={});
    bool saving() const { return bool(pending_); }
    // Captures immutable data on the main thread; callback executes on the I/O worker.
    std::function<std::function<Data()>()> captureCore;
    void setMods(std::map<std::string,std::string> versions);
    void setState(const std::string& mod,Data state);
    Data state(const std::string& mod) const;
    void save(const std::string& slot,std::function<void()> afterWrite={});
    std::vector<Diagnostic> load(const std::string& slot);
    static Data inspect(const std::filesystem::path& file);
    const std::filesystem::path& directory() const { return directory_; }
    std::filesystem::path writeDebugReport(const Data& data) const;
    std::function<bool(const std::string&)> definitionAvailable;
    // Runs before committing the staged world; throwing leaves live state intact.
    std::function<void(const Data&)> validateState;
    // Chunk collision/render data is regenerated and must never inflate world saves.
    std::function<bool(EntityId)> includeEntity;
    std::function<void()> prepareSave;
    std::function<void()> didLoad;
private:
    std::filesystem::path path(const std::string& slot) const;
    std::filesystem::path directory_;
    World& world_; PhysicsSystem& physics_; PlayerController& player_; AssetManager& assets_;
    std::map<std::string,std::string> mods_,savedMods_;
    Data::Object state_;
    Data::Array orphans_;
    Data orphanPlayer_;
    nlohmann::json capture();
    nlohmann::json migrate(nlohmann::json json) const;
    std::map<std::string,ModManifest> policies_;
    std::map<std::string,std::map<unsigned,std::function<Data(const Data&)>>> migrations_;
    std::map<std::string,unsigned> savedSchemas_;
    SaveRequest pending_;
};
}
