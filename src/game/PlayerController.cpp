#include "game/PlayerController.h"
#include "core/Diagnostic.h"
#include <algorithm>
#include <cmath>
namespace game {
void PlayerController::possess(EntityId id,float speed) {
    if(!physics_.contains(id) || !std::isfinite(speed) || speed<=0) throw DiagnosticError({"GAME105","Player needs physics and positive finite speed"});
    player_=id; speed_=speed; cameraReady_=false;
}
EntityId PlayerController::entity() const { return physics_.contains(player_) ? player_ : EntityId{}; }
void PlayerController::move(float right,float forward) {
    const auto id=entity(); if(!id) return;
    if(!std::isfinite(right) || !std::isfinite(forward)) throw DiagnosticError({"GAME105","Invalid movement input"});
    if(!enabled_ || !world_.get<ActiveComponent>(id).value || (world_.has<HealthComponent>(id) && world_.get<HealthComponent>(id).current<=0)) right=forward=0;
    const auto divisor=std::max(1.0f,std::hypot(right,forward));
    const auto length=std::hypot(viewOffset_.x,viewOffset_.z);
    const auto fx=-viewOffset_.x/length,fz=-viewOffset_.z/length;
    auto velocity=physics_.velocity(id);
    velocity.x=(right*fz+forward*fx)/divisor*speed_;
    velocity.z=(-right*fx+forward*fz)/divisor*speed_;
    physics_.setVelocity(id,velocity);
}
void PlayerController::updateCamera(float delta) {
    const auto id=entity(); if(!id || !std::isfinite(delta) || delta<0) return;
    const auto p=world_.get<TransformComponent>(id).local.position;
    if(!cameraReady_ || std::hypot(p.x-cameraTarget_.x,p.z-cameraTarget_.z)>10) { cameraTarget_=p; cameraReady_=true; return; }
    const auto blend=1-std::exp(-9*delta);
    cameraTarget_.x+=(p.x-cameraTarget_.x)*blend; cameraTarget_.z+=(p.z-cameraTarget_.z)*blend;
    if(physics_.grounded(id)) cameraTarget_.y+=(p.y-cameraTarget_.y)*blend;
}
Camera PlayerController::camera() const {
    Camera result; const auto id=entity(); if(!id) return result;
    const auto p=cameraReady_ ? cameraTarget_ : world_.get<TransformComponent>(id).local.position;
    result.target=p; result.eye={p.x+viewOffset_.x,p.y+viewOffset_.y,p.z+viewOffset_.z}; return result;
}
void PlayerController::setViewOffset(Vec3 offset) {
    if(!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z) || offset.y<=0 || std::hypot(offset.x,offset.z)<.01f ||
        std::abs(offset.x)>100 || offset.y>100 || std::abs(offset.z)>100) throw DiagnosticError({"GAME105","Invalid camera offset"});
    viewOffset_=offset;
}
bool PlayerController::jump(float speed) {
    if(!std::isfinite(speed) || speed<=0) throw DiagnosticError({"GAME105","Jump speed must be positive and finite"});
    const auto id=entity();
    if(!id || !enabled_ || !world_.get<ActiveComponent>(id).value ||
        (world_.has<HealthComponent>(id) && world_.get<HealthComponent>(id).current<=0) || !physics_.grounded(id)) return false;
    auto v=physics_.velocity(id); v.y=speed; physics_.setVelocity(id,v); return true;
}
}
