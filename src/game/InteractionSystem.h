#pragma once
#include "ecs/World.h"
namespace game {
EntityId nearestInteractable(World& world,EntityId actor,float radius);
}
