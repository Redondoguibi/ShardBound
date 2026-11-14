#include "physics/PhysicsWorld.h"
#include "core/Diagnostic.h"
#include "core/Log.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <map>
#include <cstdarg>
#include <cstdio>
namespace game {
namespace {
void physicsTrace(const char* format,...) {
    char message[2048]{};va_list args;va_start(args,format);std::vsnprintf(message,sizeof(message),format,args);va_end(args);LOG_WARN("PHYSICS",message);std::fflush(stdout);
}
struct Library {
    Library() { JPH::Trace=physicsTrace;JPH::RegisterDefaultAllocator(); JPH::Factory::sInstance=new JPH::Factory(); JPH::RegisterTypes(); }
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
struct MovementContacts final : JPH::ContactListener {
    std::vector<JPH::BodyID> driven;
    float supportNormal=.6f;
    void adjust(const JPH::Body& a,const JPH::Body& b,const JPH::ContactManifold& contact,JPH::ContactSettings& settings) const {
        // Reapplying horizontal velocity creates normal impulses every step. Wall
        // friction must not turn those impulses into support against gravity.
        // Keep ordinary floor/ceiling friction and all contacts of other bodies.
        if(std::abs(contact.mWorldSpaceNormal.GetY())<supportNormal &&
            (std::find(driven.begin(),driven.end(),a.GetID())!=driven.end() || std::find(driven.begin(),driven.end(),b.GetID())!=driven.end())) settings.mCombinedFriction=0;
    }
    void OnContactAdded(const JPH::Body& a,const JPH::Body& b,const JPH::ContactManifold& contact,JPH::ContactSettings& settings) override { adjust(a,b,contact,settings); }
    void OnContactPersisted(const JPH::Body& a,const JPH::Body& b,const JPH::ContactManifold& contact,JPH::ContactSettings& settings) override { adjust(a,b,contact,settings); }
};
}
struct PhysicsWorld::Impl {
    static constexpr unsigned maxBodies=4096;
    BroadLayers layers; PairFilter pairs; BroadFilter broad;
    JPH::TempAllocatorImpl allocator{10*1024*1024};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    MovementContacts contacts;
    JPH::PhysicsSystem physics;
    std::map<BodyId,JPH::BodyID> bodies;
    unsigned insertions=0;
    Impl() { physics.Init(maxBodies,0,4096,4096,layers,broad,pairs);physics.SetContactListener(&contacts); }
    void prepareInsertion() {
        // Jolt defers recycling old quadtree nodes until Update/Optimize. Bound
        // structural churn even when an editor replaces bodies without stepping.
        if(insertions>=maxBodies/16) { physics.OptimizeBroadPhase();insertions=0; }
        ++insertions;
    }
    JPH::BodyID require(BodyId id) const {
        const auto found=bodies.find(id); if(found==bodies.end()) throw DiagnosticError({"PHYS101","Invalid physics body"}); return found->second;
    }
    struct Hit {float fraction;Vec3 normal;bool stationary;};
    std::optional<Hit> sweep(BodyId id,Vec3 from,Vec3 delta,float minimumNormalY=-1) const {
        const auto raw=require(id);const auto shape=physics.GetBodyInterface().GetShape(raw);
        struct Collector final:JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> {
            JPH::Vec3 direction;float minimumNormalY;
            Collector(Vec3 delta,float minimum):direction(delta.x,delta.y,delta.z),minimumNormalY(minimum) {}
            void AddHit(const JPH::ShapeCastResult& hit) override {
                if((-hit.mPenetrationAxis).Dot(direction)<-1e-6f && (minimumNormalY<0 || (-hit.mPenetrationAxis).Normalized().GetY()>minimumNormalY)) JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>::AddHit(hit);
            }
        } collector(delta,minimumNormalY);
        JPH::RShapeCast cast(shape.GetPtr(),JPH::Vec3::sReplicate(1),JPH::RMat44::sRotationTranslation(physics.GetBodyInterface().GetRotation(raw),JPH::RVec3(from.x,from.y,from.z)),JPH::Vec3(delta.x,delta.y,delta.z));
        physics.GetNarrowPhaseQuery().CastShape(cast,JPH::ShapeCastSettings{},JPH::RVec3::sZero(),collector,{}, {},JPH::IgnoreSingleBodyFilter(raw));
        if(!collector.HadHit()) return {};
        const auto& hit=collector.mHit;const auto normal=-hit.mPenetrationAxis.Normalized();JPH::BodyLockRead lock(physics.GetBodyLockInterface(),hit.mBodyID2);
        return Hit{hit.mFraction,{normal.GetX(),normal.GetY(),normal.GetZ()},lock.Succeeded() && lock.GetBody().IsStatic()};
    }
    ~Impl() { for(const auto& [id,raw]:bodies) { (void)id; physics.GetBodyInterface().RemoveBody(raw); physics.GetBodyInterface().DestroyBody(raw); } }
};
PhysicsWorld::PhysicsWorld() { static Library library; (void)library; impl_=std::make_unique<Impl>(); }
PhysicsWorld::~PhysicsWorld()=default;
PhysicsWorld::Rules PhysicsWorld::rules() const {return {-impl_->physics.GetGravity().GetY(),impl_->contacts.supportNormal};}
void PhysicsWorld::rules(Rules value) {
    if(!std::isfinite(value.gravity) || value.gravity<0 || value.gravity>100 || !std::isfinite(value.supportNormal) || value.supportNormal<.173648f || value.supportNormal>.999848f) throw DiagnosticError({"PHYS107","Invalid gravity or walkable slope"});
    const bool gravityChanged=-impl_->physics.GetGravity().GetY()!=value.gravity;
    impl_->physics.SetGravity(JPH::Vec3(0,-value.gravity,0));impl_->contacts.supportNormal=value.supportNormal;
    if(gravityChanged) {auto& bodies=impl_->physics.GetBodyInterface();for(const auto& [id,body]:impl_->bodies) {(void)id;if(bodies.GetMotionType(body)==JPH::EMotionType::Dynamic) bodies.ActivateBody(body);}}
}
void PhysicsWorld::swapState(PhysicsWorld& other) noexcept { impl_.swap(other.impl_); }
BodyId PhysicsWorld::box(Vec3 p,Vec3 half,bool dynamic,Vec3 rotation) {
    if(!finite(p) || !finite(half) || !finite(rotation) || half.x<=0 || half.y<=0 || half.z<=0) throw DiagnosticError({"PHYS102","Invalid box dimensions/position"});
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(half.x,half.y,half.z));
    const auto shape=shapeSettings.Create(); if(shape.HasError()) throw DiagnosticError({"PHYS102",shape.GetError().c_str()});
    // Match transformMatrix: roll, pitch, then yaw, rather than Jolt's XYZ order.
    const auto orientation=JPH::Quat::sRotation(JPH::Vec3::sAxisY(),rotation.y)*JPH::Quat::sRotation(JPH::Vec3::sAxisX(),rotation.x)*JPH::Quat::sRotation(JPH::Vec3::sAxisZ(),rotation.z);
    JPH::BodyCreationSettings settings(shape.Get(),JPH::RVec3(p.x,p.y,p.z),orientation,dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,dynamic ? 1 : 0);
    settings.mAllowedDOFs=JPH::EAllowedDOFs::TranslationX|JPH::EAllowedDOFs::TranslationY|JPH::EAllowedDOFs::TranslationZ;
    impl_->prepareInsertion();
    auto raw=impl_->physics.GetBodyInterface().CreateAndAddBody(settings,dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    if(raw.IsInvalid()) throw DiagnosticError({"PHYS103","Physics body capacity exhausted"});
    static std::atomic_uint64_t sequence{1}; BodyId id{sequence.fetch_add(1)}; impl_->bodies.emplace(id,raw); return id;
}
void PhysicsWorld::destroy(BodyId id) {
    auto raw=impl_->require(id); auto& api=impl_->physics.GetBodyInterface(); api.RemoveBody(raw); api.DestroyBody(raw); impl_->bodies.erase(id);
    std::erase(impl_->contacts.driven,raw);
}
Vec3 PhysicsWorld::position(BodyId id) const { auto p=impl_->physics.GetBodyInterface().GetPosition(impl_->require(id)); return {float(p.GetX()),float(p.GetY()),float(p.GetZ())}; }
Vec3 PhysicsWorld::velocity(BodyId id) const { auto p=impl_->physics.GetBodyInterface().GetLinearVelocity(impl_->require(id)); return {p.GetX(),p.GetY(),p.GetZ()}; }
void PhysicsWorld::setVelocity(BodyId id,Vec3 v) {
    if(!finite(v)) throw DiagnosticError({"PHYS104","Non-finite velocity"});
    impl_->physics.GetBodyInterface().SetLinearVelocity(impl_->require(id),JPH::Vec3(v.x,v.y,v.z));
}
void PhysicsWorld::setMovementVelocity(BodyId id,Vec3 v) {
    setVelocity(id,v);
    const auto body=impl_->require(id);auto& api=impl_->physics.GetBodyInterface();if(api.GetMotionType(body)!=JPH::EMotionType::Dynamic) return;if(api.GetMotionQuality(body)!=JPH::EMotionQuality::LinearCast) api.SetMotionQuality(body,JPH::EMotionQuality::LinearCast);
    const auto raw=impl_->require(id);auto& driven=impl_->contacts.driven;
    if(std::find(driven.begin(),driven.end(),raw)==driven.end()) driven.push_back(raw);
}
bool PhysicsWorld::stepUp(BodyId id,Vec3 velocity,float delta,float height) {
    if(std::hypot(velocity.x,velocity.z)<.001f) return false;
    const auto p=position(id);const Vec3 travel{velocity.x*delta,0,velocity.z*delta};
    const auto obstacle=impl_->sweep(id,{p.x,p.y+.02f,p.z},travel);
    if(!obstacle || obstacle->normal.y>=impl_->contacts.supportNormal) return false;
    float lift=height;
    if(lift>0) if(const auto ceiling=impl_->sweep(id,p,{0,lift,0})) lift=std::max(0.f,lift*ceiling->fraction-.02f);
    const Vec3 raised{p.x,p.y+lift,p.z};
    if(lift>.04f && !impl_->sweep(id,raised,travel)) {
        const Vec3 across{raised.x+travel.x,raised.y,raised.z+travel.z};
        if(const auto floor=impl_->sweep(id,across,{0,-lift-.04f,0});floor && floor->stationary && floor->normal.y>=impl_->contacts.supportNormal) {
            const float rise=lift-(lift+.04f)*floor->fraction+.005f;
            if(rise>.025f && rise<=height+.01f) {setPosition(id,{across.x,p.y+rise,across.z});setVelocity(id,{0,0,0});return true;}
        }
    }
    // A steep ramp must not convert a horizontal motor into an upward force.
    if(obstacle->stationary && obstacle->normal.y>.01f && obstacle->normal.y<impl_->contacts.supportNormal) {
        const auto n=obstacle->normal;const float length=n.x*n.x+n.z*n.z,dot=velocity.x*n.x+velocity.z*n.z;
        if(length>.001f && dot<0) {velocity.x-=dot*n.x/length;velocity.z-=dot*n.z/length;setVelocity(id,velocity);}
    }
    return false;
}
void PhysicsWorld::snapToGround(BodyId id,float height) {
    if(height<=0) return;const auto p=position(id);
    if(const auto floor=impl_->sweep(id,{p.x,p.y+.02f,p.z},{0,-height-.02f,0});floor && floor->stationary && floor->normal.y>=impl_->contacts.supportNormal) {
        const float drop=(height+.02f)*floor->fraction-.02f;
        if(drop>.025f && drop<=height) {setPosition(id,{p.x,p.y-drop+.005f,p.z});auto v=velocity(id);v.y=0;setVelocity(id,v);}
    }
}
void PhysicsWorld::setGravityFactor(BodyId id,float factor) {
    if(!std::isfinite(factor) || factor<0) throw DiagnosticError({"PHYS104","Gravity factor must be finite and non-negative"});
    impl_->physics.GetBodyInterface().SetGravityFactor(impl_->require(id),factor);
}
void PhysicsWorld::step(float delta) {
    if(!std::isfinite(delta) || delta<=0 || delta>0.25f) throw DiagnosticError({"PHYS105","Invalid physics timestep"});
    const auto result=impl_->physics.Update(delta,int(std::ceil(delta*60)),&impl_->allocator,&impl_->jobs);
    impl_->contacts.driven.clear();
    if(result!=JPH::EPhysicsUpdateError::None) throw DiagnosticError({"PHYS106","Physics update capacity exceeded"});
    impl_->insertions=0;
}
void PhysicsWorld::setPosition(BodyId id,Vec3 p) {
    if(!finite(p)) throw DiagnosticError({"PHYS104","Non-finite position"});
    impl_->physics.GetBodyInterface().SetPosition(impl_->require(id),JPH::RVec3(p.x,p.y,p.z),JPH::EActivation::Activate);
}
bool PhysicsWorld::grounded(BodyId id,Vec3) const {
    if(velocity(id).y>0.1f) return false;
    // Use the actual oriented body, rather than rays based on unrotated extents.
    // Filter walls before selecting the closest hit so they cannot hide a floor.
    const auto p=position(id);
    return impl_->sweep(id,{p.x,p.y+.02f,p.z},{0,-.08f,0},impl_->contacts.supportNormal).has_value();
}
bool PhysicsWorld::clearPath(BodyId from,BodyId to) const {
    auto a=position(from),b=position(to);
    JPH::RRayCast ray(JPH::RVec3(a.x,a.y,a.z),JPH::Vec3(b.x-a.x,b.y-a.y,b.z-a.z));
    JPH::RayCastResult hit;
    return !impl_->physics.GetNarrowPhaseQuery().CastRay(ray,hit,{}, {},JPH::IgnoreSingleBodyFilter(impl_->require(from))) || hit.mBodyID==impl_->require(to);
}
std::optional<PhysicsWorld::RayHit> PhysicsWorld::raycast(Vec3 from,Vec3 to,const std::vector<BodyId>& ignored) const {
    if(!finite(from) || !finite(to) || ignored.size()>16) throw DiagnosticError({"PHYS104","Invalid raycast"});
    const Vec3 delta{to.x-from.x,to.y-from.y,to.z-from.z};
    if(!finite(delta)) throw DiagnosticError({"PHYS104","Raycast overflow"});
    if(delta.x==0 && delta.y==0 && delta.z==0) return {};
    struct Filter final:JPH::BodyFilter {
        std::vector<JPH::BodyID> ids;
        bool ShouldCollide(const JPH::BodyID& id) const override { return std::find(ids.begin(),ids.end(),id)==ids.end(); }
    } filter;
    for(auto id:ignored) filter.ids.push_back(impl_->require(id));
    JPH::RRayCast ray(JPH::RVec3(from.x,from.y,from.z),JPH::Vec3(delta.x,delta.y,delta.z));JPH::RayCastResult hit;
    if(!impl_->physics.GetNarrowPhaseQuery().CastRay(ray,hit,{}, {},filter)) return {};
    JPH::BodyLockRead lock(impl_->physics.GetBodyLockInterface(),hit.mBodyID);
    if(!lock.Succeeded()) return {};
    const auto normal=lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,ray.GetPointOnRay(hit.mFraction));
    for(const auto& [id,body]:impl_->bodies) if(body==hit.mBodyID) return RayHit{id,hit.mFraction,{normal.GetX(),normal.GetY(),normal.GetZ()}};
    return {};
}
}
