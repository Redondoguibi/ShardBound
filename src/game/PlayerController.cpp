#include "game/PlayerController.h"
#include "core/Diagnostic.h"
#include <algorithm>
#include <cmath>
namespace game {
void PlayerController::possess(EntityId id,float speed) {
    if(!physics_.contains(id) || !std::isfinite(speed) || speed<=0) throw DiagnosticError({"GAME105","Player needs physics and positive finite speed"});
    player_=id; speed_=speed;
}
EntityId PlayerController::entity() const { return physics_.contains(player_) ? player_ : EntityId{}; }
void PlayerController::move(float right,float forward) {
    const auto id=entity(); if(!id) return;
    if(!std::isfinite(right) || !std::isfinite(forward)) throw DiagnosticError({"GAME105","Invalid movement input"});
    if(!world_.get<ActiveComponent>(id).value || (world_.has<HealthComponent>(id) && world_.get<HealthComponent>(id).current<=0)) right=forward=0;
    const auto divisor=std::max(1.0f,std::hypot(right,forward));
    // Camera looks along +Z from above; input is aligned to its ground plane.
    auto velocity=physics_.velocity(id); velocity.x=right/divisor*speed_; velocity.z=forward/divisor*speed_;
    physics_.setVelocity(id,velocity);
}
Camera PlayerController::camera() const {
    Camera result; const auto id=entity(); if(!id) return result;
    const auto p=world_.get<TransformComponent>(id).local.position;
    result.target=p; result.eye={p.x,p.y+8,p.z-10}; return result;
}
}
