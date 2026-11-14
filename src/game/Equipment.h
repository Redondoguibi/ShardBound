#pragma once
#include "ecs/World.h"
namespace game {
struct StatBonus { double add=0,multiply=1; };
using StatBonuses=std::map<std::string,StatBonus>;
struct ItemInstance {
    std::string item,slot;
    double durability=1,maximum=1;
    StatBonuses stats;
    std::map<std::string,StatBonuses> affixes;
};
struct EquipmentState { std::uint64_t next=1;std::map<std::string,ItemInstance> items; };
StatBonuses readBonuses(const Data& data);
EquipmentState readEquipment(World& world,EntityId actor);
void writeEquipment(World& world,EntityId actor,const EquipmentState& state);
CustomComponent equipmentData(const EquipmentState& state);
void validateEquipment(World& world,EntityId actor);
unsigned reservedItems(World& world,EntityId actor,const std::string& item);
bool instanceSlotOccupied(World& world,EntityId actor,const std::string& slot);
bool validEquipmentSlot(const std::string& slot);
double equipmentStat(World& world,EntityId actor,const std::string& stat,double base,bool useAttributeBase=true);
}
