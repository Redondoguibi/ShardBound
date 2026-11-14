#pragma once
#include "scripting/EngineBridge.h"
#include "game/PhysicsSystem.h"
namespace game {
void bindEngineServices(EngineBridge& bridge,PhysicsSystem& physics,Renderer& renderer);
void validateEngineContext(const Data& context);
double engineNumber(const Data& context,const std::string& key);
bool engineBool(const Data& context,const std::string& key);
Data engineVector(Vec3 value);
Vec3 engineVector(const Data& value);
Data engineCamera(const Camera& value);
Camera engineCamera(const Data& value);
}
