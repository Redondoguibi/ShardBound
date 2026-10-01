#include "game/PhysicsSystem.h"
#include "core/Diagnostic.h"
namespace game {
const PhysicsSystem::Entry& PhysicsSystem::require(EntityId entity) const {
    const auto found=bodies_.find(entity);
    if(!world_.alive(entity) || found==bodies_.end()) throw DiagnosticError({"GAME104","Entity has no live physics body"});
    return found->second;
}
void PhysicsSystem::attachBox(EntityId entity,Vec3 halfExtent,bool dynamic) {
    if(world_.parent(entity)) throw DiagnosticError({"GAME104","Physical entities must be roots"});
    const auto position=world_.get<TransformComponent>(entity).local.position;
    auto body=physics_.box(position,halfExtent,dynamic);
    detach(entity); bodies_.emplace(entity,Entry{body,position});
}
void PhysicsSystem::detach(EntityId entity) {
    const auto found=bodies_.find(entity);
    if(found!=bodies_.end()) { physics_.destroy(found->second.body); bodies_.erase(found); }
}
bool PhysicsSystem::contains(EntityId entity) const { return world_.alive(entity) && bodies_.contains(entity); }
Vec3 PhysicsSystem::velocity(EntityId entity) const { return physics_.velocity(require(entity).body); }
void PhysicsSystem::setVelocity(EntityId entity,Vec3 velocity) { physics_.setVelocity(require(entity).body,velocity); }
void PhysicsSystem::step(float delta) {
    for(auto it=bodies_.begin();it!=bodies_.end();) {
        const auto id=it->first;
        if(!world_.alive(id) || !world_.has<TransformComponent>(id)) { physics_.destroy(it->second.body); it=bodies_.erase(it); continue; }
        if(world_.parent(id)) throw DiagnosticError({"GAME104","Physical entities must remain roots"});
        const auto p=world_.get<TransformComponent>(id).local.position; const auto old=it->second.lastPosition;
        if(p.x!=old.x || p.y!=old.y || p.z!=old.z) physics_.setPosition(it->second.body,p);
        ++it;
    }
    physics_.step(delta);
    for(auto& [entity,entry]:bodies_) {
        entry.lastPosition=physics_.position(entry.body);
        world_.get<TransformComponent>(entity).local.position=entry.lastPosition;
    }
}
}
