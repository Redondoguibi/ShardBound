#pragma once
#include "scripting/Bridge.h"
#include "game/PlayerController.h"
#include "world/ChunkWorld.h"
#include <functional>
namespace game {
class DeveloperCommands {
public:
    DeveloperCommands(script::Runtime& runtime,World& world,PhysicsSystem& physics,PlayerController& player);
    void terrain(ChunkWorld* value,std::string seed={}) { terrain_=value;seed_=std::move(seed); }
    std::string execute(const std::string& line);
    std::vector<std::string> complete(const std::string& prefix) const;
    std::function<void(const std::string&)> changeCamera;
private:
    struct Argument { std::string name,type;bool optional=false;std::vector<std::string> choices; };
    struct Command { std::string usage,description,owner;std::vector<Argument> arguments;std::function<std::string(const std::vector<std::string>&)> action; };
    void add(std::string name,Command command);
    ChunkWorld& requireTerrain() const;
    EntityId actor() const;
    double coordinate(const std::string& token,int axis,bool block=false) const;
    Vec3 localPosition(const std::vector<std::string>& args,size_t offset) const;
    std::vector<std::string> identifiers(const std::string& type) const;
    script::Runtime& runtime_;World& world_;PhysicsSystem& physics_;PlayerController& player_;
    ChunkWorld* terrain_=nullptr;std::string seed_;
    std::map<std::string,Command> commands_;
};
}
