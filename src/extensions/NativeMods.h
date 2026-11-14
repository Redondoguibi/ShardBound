#pragma once
#include "extensions/Components.h"
#include "persistence/SaveGame.h"
#include "scripting/Runtime.h"
namespace game {
class NativeMods {
public:
    NativeMods(script::Runtime& runtime,World& world,Components& components,SaveGame& saves);
    ~NativeMods();
    void load(const std::string& mod,const std::filesystem::path& root,const std::filesystem::path& relative);
    std::function<Data(const std::string&,const std::string&,const Data&)> engineOperation;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
