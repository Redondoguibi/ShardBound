#pragma once
#include "ecs/World.h"
#include "physics/PhysicsWorld.h"
#include <functional>
namespace game {
// Owns bodies; World must outlive this system. Physical entities are roots.
class PhysicsSystem {
public:
    struct Box { Vec3 halfExtent; bool dynamic;Vec3 rotation{}; };
    explicit PhysicsSystem(World& world) : world_(world) {}
    void attachBox(EntityId entity,Vec3 halfExtent,bool dynamic,Vec3 rotation={});
    void detach(EntityId entity);
    bool contains(EntityId entity) const;
    std::vector<EntityId> entities(bool dynamicOnly=false) const;
    Vec3 velocity(EntityId entity) const;
    void setVelocity(EntityId entity,Vec3 velocity);
    void setMovementVelocity(EntityId entity,Vec3 velocity,float stepHeight=0);
    void setGravityFactor(EntityId entity,float factor);
    bool grounded(EntityId entity) const;
    bool clearPath(EntityId from,EntityId to) const;
    struct RayHit { EntityId entity;float fraction;Vec3 normal; };
    std::optional<RayHit> raycast(Vec3 from,Vec3 to,const std::vector<EntityId>& ignored={}) const;
    // Procedural terrain can answer rays beyond the streamed physics bodies.
    std::function<std::optional<RayHit>(Vec3,Vec3)> terrainQuery;
    void teleport(EntityId entity,Vec3 position);
    void step(float delta);
    PhysicsWorld::Rules rules() const {return physics_.rules();}
    void rules(PhysicsWorld::Rules value) {physics_.rules(value);}
    Box description(EntityId entity) const;
    void swapState(PhysicsSystem& other) noexcept;
private:
    struct Entry { BodyId body; Vec3 lastPosition; Box description; std::optional<Vec3> movement;float stepHeight=0;bool snap=false; };
    const Entry& require(EntityId entity) const;
    World& world_;
    PhysicsWorld physics_;
    std::map<EntityId,Entry> bodies_;
};
}
