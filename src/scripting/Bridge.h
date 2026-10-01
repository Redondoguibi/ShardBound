#pragma once
#include "scripting/Runtime.h"
#include "ecs/World.h"
namespace game {
void bindRuntimeApi(script::Runtime& scripting,World& world,AssetManager& assets);
}
