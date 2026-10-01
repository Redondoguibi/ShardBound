#include "game/InteractionSystem.h"
#include "core/Diagnostic.h"
#include <cmath>
namespace game {
EntityId nearestInteractable(World& world,EntityId actor,float radius) {
    if(!std::isfinite(radius) || radius<=0) throw DiagnosticError({"GAME103","Interaction radius must be positive and finite"});
    if(!world.has<TransformComponent>(actor)) throw DiagnosticError({"GAME103","Actor needs a transform"});
    const auto origin=world.worldMatrix(actor); double best=double(radius)*radius; EntityId selected{};
    for(auto id:world.entities()) {
        if(id==actor || !world.has<InteractableComponent>(id) || !world.get<InteractableComponent>(id).enabled ||
            !world.has<TransformComponent>(id) || !world.has<ActiveComponent>(id) || !world.get<ActiveComponent>(id).value) continue;
        const auto p=world.worldMatrix(id);
        const double x=double(p.values[12])-origin.values[12],y=double(p.values[13])-origin.values[13],z=double(p.values[14])-origin.values[14];
        const auto distance=x*x+y*y+z*z;
        if(distance<=best && (!selected || distance<best)) { selected=id; best=distance; }
    }
    return selected;
}
}
