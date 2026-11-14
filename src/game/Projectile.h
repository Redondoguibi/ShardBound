#pragma once
#include "ecs/World.h"
namespace game {
struct ProjectileState { Vec3 velocity;double remaining=1,gravity=0;std::string source; };
ProjectileState readProjectile(const CustomComponent& component);
void validateProjectile(World& world,EntityId entity);
Data projectileData(const ProjectileState& state);
}
