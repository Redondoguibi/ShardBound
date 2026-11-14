#pragma once
#include "mods/Manifest.h"
#include <map>
#include <set>
namespace game {
class ModProfiles {
public:
    ModProfiles(std::filesystem::path file,std::vector<ModManifest> catalog,std::set<std::string> available,bool ignoreSaved=false);
    const std::vector<ModManifest>& catalog() const { return catalog_; }
    const std::string& active() const { return active_; }
    bool available(const std::string& id) const {return available_.contains(id);}
    const std::set<std::string>& enabled() const { return profiles_.at(active_); }
    std::vector<std::string> issues() const;
    std::vector<ModManifest> selected() const;
    void toggle(const std::string& id);
    void next();
    void duplicate();
    void rename(const std::string& name);
private:
    using Profiles=std::map<std::string,std::set<std::string>>;
    std::vector<std::string> issues(const std::set<std::string>& selection) const;
    void store(Profiles profiles,std::string active);
    std::filesystem::path file_;
    std::vector<ModManifest> catalog_;
    std::set<std::string> available_;
    Profiles profiles_;
    std::string active_;
};
}
