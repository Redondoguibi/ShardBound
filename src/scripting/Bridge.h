#pragma once
#include "scripting/Runtime.h"
#include "ecs/World.h"
namespace game {
void bindRuntimeApi(script::Runtime& scripting,World& world,AssetManager& assets);
script::Value entityValue(World& world,EntityId entity);
EntityId requireEntity(const script::Value& value,World& world,const script::Location& at);
}
