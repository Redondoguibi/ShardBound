#include "game/PhysicsSystem.h"
#include "core/Diagnostic.h"
#include <cmath>
namespace game {
const PhysicsSystem::Entry& PhysicsSystem::require(EntityId entity) const {
    const auto found=bodies_.find(entity);
    if(!world_.alive(entity) || found==bodies_.end()) throw DiagnosticError({"GAME104","Entity has no live physics body"});
    return found->second;
}
void PhysicsSystem::attachBox(EntityId entity,Vec3 halfExtent,bool dynamic,Vec3 rotation) {
    if(world_.parent(entity)) throw DiagnosticError({"GAME104","Physical entities must be roots"});
    const auto position=world_.get<TransformComponent>(entity).local.position;
    auto body=physics_.box(position,halfExtent,dynamic,rotation);
    detach(entity);bodies_.emplace(entity,Entry{body,position,{halfExtent,dynamic,rotation}});
}
void PhysicsSystem::detach(EntityId entity) {
    const auto found=bodies_.find(entity);
    if(found!=bodies_.end()) { physics_.destroy(found->second.body); bodies_.erase(found); }
}
bool PhysicsSystem::contains(EntityId entity) const { return world_.alive(entity) && bodies_.contains(entity); }
std::vector<EntityId> PhysicsSystem::entities(bool dynamicOnly) const {
    std::vector<EntityId> result;result.reserve(bodies_.size());
    for(const auto& [id,entry]:bodies_) if(world_.alive(id) && world_.has<TransformComponent>(id) && (!dynamicOnly || entry.description.dynamic)) result.push_back(id);return result;
}
PhysicsSystem::Box PhysicsSystem::description(EntityId entity) const { return require(entity).description; }
void PhysicsSystem::swapState(PhysicsSystem& other) noexcept { physics_.swapState(other.physics_); bodies_.swap(other.bodies_); }
Vec3 PhysicsSystem::velocity(EntityId entity) const { return physics_.velocity(require(entity).body); }
void PhysicsSystem::setVelocity(EntityId entity,Vec3 velocity) { physics_.setVelocity(require(entity).body,velocity); }
void PhysicsSystem::setMovementVelocity(EntityId entity,Vec3 velocity,float stepHeight) {
    if(!std::isfinite(stepHeight) || stepHeight<0 || stepHeight>2) throw DiagnosticError({"GAME104","Step height must be in [0,2]"});
    physics_.setMovementVelocity(require(entity).body,velocity);auto& entry=bodies_.at(entity);entry.movement=velocity;entry.stepHeight=stepHeight;
}
void PhysicsSystem::setGravityFactor(EntityId entity,float factor) {
    const auto& entry=require(entity);
    if(!entry.description.dynamic) throw DiagnosticError({"GAME104","Gravity factor requires a dynamic body"});
    physics_.setGravityFactor(entry.body,factor);
}
bool PhysicsSystem::grounded(EntityId entity) const { const auto& e=require(entity); return e.description.dynamic && physics_.grounded(e.body,e.description.halfExtent); }
bool PhysicsSystem::clearPath(EntityId from,EntityId to) const { return physics_.clearPath(require(from).body,require(to).body); }
std::optional<PhysicsSystem::RayHit> PhysicsSystem::raycast(Vec3 from,Vec3 to,const std::vector<EntityId>& ignored) const {
    std::vector<BodyId> bodies;for(auto id:ignored) if(contains(id)) bodies.push_back(require(id).body);
    const auto hit=physics_.raycast(from,to,bodies);auto result=terrainQuery?terrainQuery(from,to):std::optional<RayHit>{};
    if(hit && (!result || hit->fraction<=result->fraction))
        for(const auto& [id,entry]:bodies_) if(entry.body==hit->body && world_.alive(id)) return RayHit{id,hit->fraction,hit->normal};
    return result;
}
void PhysicsSystem::teleport(EntityId entity,Vec3 position) {
    const auto body=require(entity).body;
    physics_.setPosition(body,position); physics_.setVelocity(body,{});
    bodies_.at(entity).movement.reset();bodies_.at(entity).snap=false;
    world_.get<TransformComponent>(entity).local.position=position; bodies_.at(entity).lastPosition=position;
}
void PhysicsSystem::step(float delta) {
    if(!std::isfinite(delta)||delta<=0||delta>.25f) throw DiagnosticError({"GAME104","Invalid physics timestep"});
    for(auto it=bodies_.begin();it!=bodies_.end();) {
        const auto id=it->first;
        if(!world_.alive(id) || !world_.has<TransformComponent>(id)) { physics_.destroy(it->second.body); it=bodies_.erase(it); continue; }
        if(world_.parent(id)) throw DiagnosticError({"GAME104","Physical entities must remain roots"});
        const auto p=world_.get<TransformComponent>(id).local.position; const auto old=it->second.lastPosition;
        if(p.x!=old.x || p.y!=old.y || p.z!=old.z) physics_.setPosition(it->second.body,p);
        ++it;
    }
    for(auto& [id,entry]:bodies_) {(void)id;entry.snap=entry.movement && entry.description.dynamic && entry.movement->y<=.1f && physics_.grounded(entry.body,entry.description.halfExtent);if(entry.movement && entry.description.dynamic) physics_.stepUp(entry.body,*entry.movement,delta,entry.snap?entry.stepHeight:0);}
    physics_.step(delta);
    for(auto& [entity,entry]:bodies_) {
        if(entry.snap) physics_.snapToGround(entry.body,entry.stepHeight);
        entry.movement.reset();entry.snap=false;
        entry.lastPosition=physics_.position(entry.body);
        world_.get<TransformComponent>(entity).local.position=entry.lastPosition;
    }
}
}
