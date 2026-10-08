#pragma once
#include "game/PhysicsSystem.h"
namespace game {
class PlayerController {
public:
    PlayerController(World& world,PhysicsSystem& physics) : world_(world),physics_(physics) {}
    void possess(EntityId entity,float speed);
    EntityId entity() const;
    void move(float right,float forward);
    bool jump(float speed);
    void setViewOffset(Vec3 offset);
    Vec3 viewOffset() const { return viewOffset_; }
    void setEnabled(bool enabled) { enabled_=enabled; }
    void updateCamera(float delta);
    Camera camera() const;
    float speed() const { return speed_; }
    void release() noexcept { player_={}; speed_=0; enabled_=true; cameraReady_=false; }
private:
    World& world_; PhysicsSystem& physics_; EntityId player_{}; float speed_=0;
    bool enabled_=true;
    Vec3 cameraTarget_{};
    bool cameraReady_=false;
    Vec3 viewOffset_{0,8,-10};
};
}
