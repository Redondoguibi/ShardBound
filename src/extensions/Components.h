#pragma once
#include "ecs/World.h"
#include <functional>
namespace game {
class Components {
public:
    void define(const AssetId& type,unsigned version,Data defaults);
    void migration(const AssetId& type,unsigned from,std::function<Data(const Data&)> migrate);
    void add(World& world,EntityId entity,const AssetId& type) const;
    Data get(World& world,EntityId entity,const AssetId& type) const;
    void set(World& world,EntityId entity,const AssetId& type,Data data) const;
    bool known(const AssetId& type) const;
private:
    struct Type { unsigned version; Data defaults; std::map<unsigned,std::function<Data(const Data&)>> migrations; };
    const Type& require(const AssetId& type) const;
    std::map<std::string,Type> types_;
};
}
