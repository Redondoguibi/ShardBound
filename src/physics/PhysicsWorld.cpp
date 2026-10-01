#include "physics/PhysicsWorld.h"
#include "core/Diagnostic.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <atomic>
#include <cmath>
#include <map>
namespace game {
namespace {
struct Library {
    Library() { JPH::RegisterDefaultAllocator(); JPH::Factory::sInstance=new JPH::Factory(); JPH::RegisterTypes(); }
    ~Library() { JPH::UnregisterTypes(); delete JPH::Factory::sInstance; JPH::Factory::sInstance=nullptr; }
};
struct BroadLayers final : JPH::BroadPhaseLayerInterface {
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override { return JPH::BroadPhaseLayer(static_cast<JPH::uint8>(layer)); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override { return static_cast<JPH::uint8>(layer)==0 ? "Static" : "Moving"; }
#endif
};
struct PairFilter final : JPH::ObjectLayerPairFilter {
    bool ShouldCollide(JPH::ObjectLayer a,JPH::ObjectLayer b) const override { return a==1 || b==1; }
};
struct BroadFilter final : JPH::ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer a,JPH::BroadPhaseLayer b) const override { return a==1 || static_cast<JPH::uint8>(b)==1; }
};
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
}
struct PhysicsWorld::Impl {
    BroadLayers layers; PairFilter pairs; BroadFilter broad;
    JPH::TempAllocatorImpl allocator{10*1024*1024};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    JPH::PhysicsSystem physics;
    std::map<BodyId,JPH::BodyID> bodies;
    Impl() { physics.Init(4096,0,4096,4096,layers,broad,pairs); }
    JPH::BodyID require(BodyId id) const {
        const auto found=bodies.find(id); if(found==bodies.end()) throw DiagnosticError({"PHYS101","Invalid physics body"}); return found->second;
    }
    ~Impl() { for(const auto& [id,raw]:bodies) { (void)id; physics.GetBodyInterface().RemoveBody(raw); physics.GetBodyInterface().DestroyBody(raw); } }
};
PhysicsWorld::PhysicsWorld() { static Library library; (void)library; impl_=std::make_unique<Impl>(); }
PhysicsWorld::~PhysicsWorld()=default;
BodyId PhysicsWorld::box(Vec3 p,Vec3 half,bool dynamic) {
    if(!finite(p) || !finite(half) || half.x<=0 || half.y<=0 || half.z<=0) throw DiagnosticError({"PHYS102","Invalid box dimensions/position"});
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(half.x,half.y,half.z));
    const auto shape=shapeSettings.Create(); if(shape.HasError()) throw DiagnosticError({"PHYS102",shape.GetError().c_str()});
    JPH::BodyCreationSettings settings(shape.Get(),JPH::RVec3(p.x,p.y,p.z),JPH::Quat::sIdentity(),dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,dynamic ? 1 : 0);
    settings.mAllowedDOFs=JPH::EAllowedDOFs::TranslationX|JPH::EAllowedDOFs::TranslationY|JPH::EAllowedDOFs::TranslationZ;
    auto raw=impl_->physics.GetBodyInterface().CreateAndAddBody(settings,dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    if(raw.IsInvalid()) throw DiagnosticError({"PHYS103","Physics body capacity exhausted"});
    static std::atomic_uint64_t sequence{1}; BodyId id{sequence.fetch_add(1)}; impl_->bodies.emplace(id,raw); return id;
}
void PhysicsWorld::destroy(BodyId id) {
    auto raw=impl_->require(id); auto& api=impl_->physics.GetBodyInterface(); api.RemoveBody(raw); api.DestroyBody(raw); impl_->bodies.erase(id);
}
Vec3 PhysicsWorld::position(BodyId id) const { auto p=impl_->physics.GetBodyInterface().GetPosition(impl_->require(id)); return {float(p.GetX()),float(p.GetY()),float(p.GetZ())}; }
Vec3 PhysicsWorld::velocity(BodyId id) const { auto p=impl_->physics.GetBodyInterface().GetLinearVelocity(impl_->require(id)); return {p.GetX(),p.GetY(),p.GetZ()}; }
void PhysicsWorld::setVelocity(BodyId id,Vec3 v) {
    if(!finite(v)) throw DiagnosticError({"PHYS104","Non-finite velocity"});
    impl_->physics.GetBodyInterface().SetLinearVelocity(impl_->require(id),JPH::Vec3(v.x,v.y,v.z));
}
void PhysicsWorld::step(float delta) {
    if(!std::isfinite(delta) || delta<=0 || delta>0.25f) throw DiagnosticError({"PHYS105","Invalid physics timestep"});
    if(impl_->physics.Update(delta,int(std::ceil(delta*60)),&impl_->allocator,&impl_->jobs)!=JPH::EPhysicsUpdateError::None) throw DiagnosticError({"PHYS106","Physics update capacity exceeded"});
}
void PhysicsWorld::setPosition(BodyId id,Vec3 p) {
    if(!finite(p)) throw DiagnosticError({"PHYS104","Non-finite position"});
    impl_->physics.GetBodyInterface().SetPosition(impl_->require(id),JPH::RVec3(p.x,p.y,p.z),JPH::EActivation::Activate);
}
}
