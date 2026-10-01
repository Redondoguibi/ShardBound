#include "game/HealthSystem.h"
#include "core/Diagnostic.h"
#include <algorithm>
#include <cmath>
namespace game {
namespace {
void validate(float amount) {
    if(!std::isfinite(amount) || amount<0) throw DiagnosticError({"GAME101","Health amount must be finite and nonnegative"});
}
}
void setHealth(World& world,EntityId entity,float maximum) {
    validate(maximum);
    if(maximum==0) throw DiagnosticError({"GAME101","Maximum health must be positive"});
    world.add(entity,HealthComponent{maximum,maximum});
}
float damage(World& world,EntityId entity,float amount) {
    validate(amount); auto& health=world.get<HealthComponent>(entity);
    const auto applied=std::min(health.current,amount); health.current-=applied; return applied;
}
float heal(World& world,EntityId entity,float amount) {
    validate(amount); auto& health=world.get<HealthComponent>(entity);
    const auto applied=std::min(health.maximum-health.current,amount); health.current+=applied; return applied;
}
}
