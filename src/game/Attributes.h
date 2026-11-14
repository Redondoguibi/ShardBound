#pragma once
#include "ecs/World.h"
namespace game {
// Base overrides are ordinary versioned save data; unknown mod attributes survive removal.
using AttributeBases=std::map<std::string,double>;
AttributeBases readAttributeBases(World& world,EntityId actor);
double attributeBase(World& world,EntityId actor,const std::string& id,double fallback);
void writeAttributeBase(World& world,EntityId actor,const std::string& id,std::optional<double> value);
}
