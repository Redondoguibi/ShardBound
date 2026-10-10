#pragma once
#include "core/Math.h"
#include <cstdint>
#include <memory>
namespace game {
struct BodyId { std::uint64_t value=0; auto operator<=>(const BodyId&) const = default; };
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();
    BodyId box(Vec3 position,Vec3 halfExtent,bool dynamic);
    void destroy(BodyId id);
    Vec3 position(BodyId id) const;
    Vec3 velocity(BodyId id) const;
    void setVelocity(BodyId id,Vec3 velocity);
    void setGravityFactor(BodyId id,float factor);
    void setPosition(BodyId id,Vec3 position);
    bool grounded(BodyId id,Vec3 halfExtent) const;
    bool clearPath(BodyId from,BodyId to) const;
    void step(float delta);
    void swapState(PhysicsWorld& other) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
