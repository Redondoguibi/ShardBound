#pragma once
#include "game/PhysicsSystem.h"
namespace game {
class PlayerController {
public:
    PlayerController(World& world,PhysicsSystem& physics) : world_(world),physics_(physics) {}
    void possess(EntityId entity,float speed);
    EntityId entity() const;
    void move(float right,float forward);
    Camera camera() const;
private:
    World& world_; PhysicsSystem& physics_; EntityId player_{}; float speed_=0;
};
}
