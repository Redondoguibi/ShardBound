#pragma once
#include "core/Math.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
namespace game {
struct BodyId { std::uint64_t value=0; auto operator<=>(const BodyId&) const = default; };
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();
    BodyId box(Vec3 position,Vec3 halfExtent,bool dynamic,Vec3 rotation={});
    void destroy(BodyId id);
    Vec3 position(BodyId id) const;
    Vec3 velocity(BodyId id) const;
    void setVelocity(BodyId id,Vec3 velocity);
    // Velocity-driven locomotion must slide along walls. Applies to the next step only.
    void setMovementVelocity(BodyId id,Vec3 velocity);
    bool stepUp(BodyId id,Vec3 velocity,float delta,float height);
    void snapToGround(BodyId id,float height);
    void setGravityFactor(BodyId id,float factor);
    void setPosition(BodyId id,Vec3 position);
    bool grounded(BodyId id,Vec3 halfExtent) const;
    bool clearPath(BodyId from,BodyId to) const;
    struct RayHit { BodyId body; float fraction;Vec3 normal; };
    std::optional<RayHit> raycast(Vec3 from,Vec3 to,const std::vector<BodyId>& ignored={}) const;
    void step(float delta);
    struct Rules {float gravity=9.81f,supportNormal=.6f;};
    Rules rules() const;
    void rules(Rules value);
    void swapState(PhysicsWorld& other) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
