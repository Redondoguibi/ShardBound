#pragma once
#include "scripting/Bridge.h"
#include "game/PhysicsSystem.h"
namespace game { void bindProjectileApi(script::Runtime& runtime,World& world,PhysicsSystem& physics); }
