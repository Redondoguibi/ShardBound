#include "game/Projectile.h"
#include <cmath>
namespace game {
ProjectileState readProjectile(const CustomComponent& component) {
    if(component.version!=1) throw std::runtime_error("Unknown projectile version");
    const auto& fields=std::get<Data::Object>(component.data.value);
    const auto number=[&](const char* key,double limit) { const auto n=std::get<double>(fields.at(key).value);if(!std::isfinite(n) || std::abs(n)>limit) throw std::runtime_error("Invalid projectile number");return n; };
    ProjectileState state;state.velocity={float(number("vx",100000)),float(number("vy",100000)),float(number("vz",100000))};
    state.remaining=number("remaining",60);state.gravity=number("gravity",100);state.source=std::get<std::string>(fields.at("source").value);
    if(state.remaining<=0 || state.gravity<0 || state.source.size()>128) throw std::runtime_error("Invalid projectile state");return state;
}
Data projectileData(const ProjectileState& state) {
    return Data{Data::Object{{"vx",Data{double(state.velocity.x)}},{"vy",Data{double(state.velocity.y)}},{"vz",Data{double(state.velocity.z)}},
        {"remaining",Data{state.remaining}},{"gravity",Data{state.gravity}},{"source",Data{state.source}}}};
}
void validateProjectile(World& world,EntityId entity) {
    if(const auto it=world.custom(entity).find("core:projectile");it!=world.custom(entity).end()) {
        (void)readProjectile(it->second);
        if(!world.has<TransformComponent>(entity) || world.parent(entity)) throw std::runtime_error("Projectile must be a root with transform");
    }
}
}
