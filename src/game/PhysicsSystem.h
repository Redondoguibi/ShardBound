#pragma once
#include "ecs/World.h"
#include "physics/PhysicsWorld.h"
namespace game {
// Owns bodies; World must outlive this system. Physical entities are roots.
class PhysicsSystem {
public:
    struct Box { Vec3 halfExtent; bool dynamic; };
    explicit PhysicsSystem(World& world) : world_(world) {}
    void attachBox(EntityId entity,Vec3 halfExtent,bool dynamic);
    void detach(EntityId entity);
    bool contains(EntityId entity) const;
    Vec3 velocity(EntityId entity) const;
    void setVelocity(EntityId entity,Vec3 velocity);
    bool grounded(EntityId entity) const;
    bool clearPath(EntityId from,EntityId to) const;
    void teleport(EntityId entity,Vec3 position);
    void step(float delta);
    Box description(EntityId entity) const;
    void swapState(PhysicsSystem& other) noexcept;
private:
    struct Entry { BodyId body; Vec3 lastPosition; Box description; };
    const Entry& require(EntityId entity) const;
    World& world_;
    PhysicsWorld physics_;
    std::map<EntityId,Entry> bodies_;
};
}
