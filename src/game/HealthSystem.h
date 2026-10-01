#pragma once
#include "ecs/World.h"
namespace game {
void setHealth(World& world,EntityId entity,float maximum);
float damage(World& world,EntityId entity,float amount);
float heal(World& world,EntityId entity,float amount);
}
