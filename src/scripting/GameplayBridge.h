#pragma once
#include "scripting/Bridge.h"
#include "game/PlayerController.h"
#include "platform/InputSystem.h"
#include "audio/AudioSystem.h"
namespace game {
void bindGameplayApi(script::Runtime& runtime,World& world,PhysicsSystem& physics,
    PlayerController& player,InputSystem& input,AudioSystem& audio,AssetManager& assets);
}
