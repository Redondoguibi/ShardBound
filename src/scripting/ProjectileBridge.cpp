#include "scripting/ProjectileBridge.h"
#include "game/Projectile.h"
#include "core/Log.h"
#include <algorithm>
#include <cmath>
namespace game {
namespace {
using namespace script;
double number(const Value& value,double low,double high,const Location& at) { const auto n=value.number(at);if(!std::isfinite(n) || n<low || n>high) fail(at,"AI103","Invalid projectile parameter");return n; }
Value vector(Vec3 v) { auto value=object();value.setMember("x",Value(double(v.x)),{});value.setMember("y",Value(double(v.y)),{});value.setMember("z",Value(double(v.z)),{});return value; }
}
void bindProjectileApi(script::Runtime& runtime,World& world,PhysicsSystem& physics) {
    runtime.bind("launch_projectile",native([&](const auto& a,const auto& at) {
        if(a.size()!=5) fail(at,"AI103","launch_projectile(entity, source_or_null, velocity, lifetime, gravity)");
        const auto entity=requireEntity(a[0],world,at);
        if(physics.contains(entity) || world.parent(entity) || !world.has<TransformComponent>(entity)) fail(at,"AI103","Projectile must be a nonphysical root with transform");
        const auto count=world.entitiesWithCustom("core:projectile").size();
        if(count>=512 && !world.custom(entity).contains("core:projectile")) fail(at,"AI103","Projectile limit (512) exceeded");
        ProjectileState state;state.velocity={float(number(a[2].member("x",at),-1000,1000,at)),float(number(a[2].member("y",at),-1000,1000,at)),float(number(a[2].member("z",at),-1000,1000,at))};
        state.remaining=number(a[3],.001,60,at);state.gravity=number(a[4],0,100,at);
        if(!std::holds_alternative<std::monostate>(a[1].data)) state.source=world.get<IdentityComponent>(requireEntity(a[1],world,at)).persistent;
        world.setCustom(entity,AssetId("core:projectile"),{1,projectileData(state)});return Value{};
    }));
    runtime.bind("stop_projectile",native([&](const auto& a,const auto& at) {
        if(a.size()!=1) fail(at,"AI103","stop_projectile(entity)");return Value(world.removeCustom(requireEntity(a[0],world,at),AssetId("core:projectile")));
    }));
    runtime.subscribe("on_fixed_update",native([&](const auto& a,const auto& at) {
        const auto entities=world.entitiesWithCustom("core:projectile");if(entities.empty()) return Value{};
        if(a.size()!=1) fail(at,"AI103","Projectile simulation requires fixed delta");const auto dt=number(a[0],0,.25,at);if(dt==0) return Value{};
        // Snapshot IDs only; callbacks may destroy other projectiles or load state.
        std::string failure;
        for(const auto entity:entities) {
            if(!world.alive(entity) || !world.get<ActiveComponent>(entity).value) continue;
            const auto it=world.custom(entity).find("core:projectile");if(it==world.custom(entity).end()) continue;
            try {
                validateProjectile(world,entity);if(physics.contains(entity)) fail(at,"AI103","Projectile acquired a physics body");
                auto state=readProjectile(it->second);const auto source=state.source.empty()?EntityId{}:world.findPersistent(state.source);
                std::optional<PhysicsSystem::RayHit> hit;auto position=world.get<TransformComponent>(entity).local.position;
                double remaining=std::min(dt,state.remaining);
                while(remaining>1e-9 && !hit) {
                    const auto step=float(std::min(remaining,1.0/120));const Vec3 next{position.x+state.velocity.x*step,position.y+state.velocity.y*step-float(.5*state.gravity*step*step),position.z+state.velocity.z*step};
                    hit=physics.raycast(position,next,source?std::vector<EntityId>{source}:std::vector<EntityId>{});
                    const float fraction=hit?hit->fraction:1;
                    position={position.x+(next.x-position.x)*fraction,position.y+(next.y-position.y)*fraction,position.z+(next.z-position.z)*fraction};
                    state.velocity.y-=float(state.gravity*step);remaining-=step;
                }
                state.remaining-=dt;world.get<TransformComponent>(entity).local.position=position;
                if(hit || state.remaining<=0) {
                    world.removeCustom(entity,AssetId("core:projectile"));
                    const auto event=hit?"on_projectile_hit":"on_projectile_expired";
                    const auto args=hit?std::vector<Value>{entityValue(world,entity),entityValue(world,source),entityValue(world,hit->entity),vector(position),vector(hit->normal)}:std::vector<Value>{entityValue(world,entity),entityValue(world,source)};
                    for(const auto& d:runtime.emit(event,args)) LOG_ERROR("SCRIPT",DiagnosticError(d).what());
                    if(world.alive(entity)) world.destroy(entity);
                } else world.setCustom(entity,AssetId("core:projectile"),{1,projectileData(state)});
            } catch(const std::exception& error) {
                if(world.alive(entity)) world.removeCustom(entity,AssetId("core:projectile"));
                LOG_ERROR("SCRIPT",error.what());if(failure.empty()) failure=error.what();
            }
        }
        if(!failure.empty()) fail(at,"AI103",failure);return Value{};
    }),false,"core");
}
}
