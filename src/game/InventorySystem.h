#pragma once
#include "ecs/World.h"
namespace game {
void giveItem(World& world,EntityId entity,const AssetId& item,std::uint32_t count);
bool takeItem(World& world,EntityId entity,const AssetId& item,std::uint32_t count);
std::uint32_t itemCount(World& world,EntityId entity,const AssetId& item);
}
