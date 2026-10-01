#include "scripting/GameplayBridge.h"
#include "game/HealthSystem.h"
#include "game/InventorySystem.h"
#include "game/InteractionSystem.h"
#include <cmath>
#include <limits>
namespace game {
namespace {
using namespace script;
void arity(const std::vector<Value>& args,size_t n,const Location& at) {
    if(args.size()!=n) fail(at,"BRIDGE101","Expected "+std::to_string(n)+" arguments");
}
float number(const Value& value,const Location& at) {
    const auto n=value.number(at);
    if(!std::isfinite(n) || std::abs(n)>std::numeric_limits<float>::max()) fail(at,"BRIDGE102","Number outside finite float range");
    return static_cast<float>(n);
}
Vec3 vector(const Value& value,const Location& at) { return {number(value.member("x",at),at),number(value.member("y",at),at),number(value.member("z",at),at)}; }
std::uint32_t count(const Value& value,const Location& at) {
    const auto n=value.number(at);
    if(!std::isfinite(n) || n<1 || n>std::numeric_limits<std::uint32_t>::max() || n!=std::floor(n)) fail(at,"BRIDGE105","Expected positive integer item count");
    return static_cast<std::uint32_t>(n);
}
AssetId itemId(Runtime& runtime,const Value& value,const Location& at) {
    const auto symbol=std::get_if<Symbol>(&value.data); const auto id=symbol ? symbol->name : value.string(at);
    (void)runtime.registry().get("Items",id,at); return AssetId(id);
}
}
void bindGameplayApi(script::Runtime& runtime,World& world,PhysicsSystem& physics,
    PlayerController& player,InputSystem& input,AudioSystem& audio,AssetManager& assets) {
    using namespace script;
    runtime.bind("set_scale",native([&](const auto& a,const auto& at) {
        arity(a,2,at); const auto id=requireEntity(a[0],world,at); const auto scale=vector(a[1],at);
        if(scale.x<=0 || scale.y<=0 || scale.z<=0) fail(at,"BRIDGE105","Scale must be positive");
        world.get<TransformComponent>(id).local.scale=scale; return Value{};
    }));
    runtime.bind("add_box",native([&](const auto& a,const auto& at) {
        arity(a,3,at); physics.attachBox(requireEntity(a[0],world,at),vector(a[1],at),a[2].boolean(at)); return Value{};
    }));
    runtime.bind("set_player",native([&](const auto& a,const auto& at) {
        arity(a,2,at); player.possess(requireEntity(a[0],world,at),number(a[1],at)); return Value{};
    }));
    runtime.bind("set_health",native([&](const auto& a,const auto& at) {
        arity(a,2,at); setHealth(world,requireEntity(a[0],world,at),number(a[1],at)); return Value{};
    }));
    runtime.bind("health",native([&](const auto& a,const auto& at) {
        arity(a,1,at); return Value(double(world.get<HealthComponent>(requireEntity(a[0],world,at)).current));
    }));
    runtime.bind("damage",native([&](const auto& a,const auto& at) {
        arity(a,2,at); return Value(double(damage(world,requireEntity(a[0],world,at),number(a[1],at))));
    }));
    runtime.bind("heal",native([&](const auto& a,const auto& at) {
        arity(a,2,at); return Value(double(heal(world,requireEntity(a[0],world,at),number(a[1],at))));
    }));
    runtime.bind("give_item",native([&](const auto& a,const auto& at) {
        arity(a,3,at); giveItem(world,requireEntity(a[0],world,at),itemId(runtime,a[1],at),count(a[2],at)); return Value{};
    }));
    runtime.bind("take_item",native([&](const auto& a,const auto& at) {
        arity(a,3,at); return Value(takeItem(world,requireEntity(a[0],world,at),itemId(runtime,a[1],at),count(a[2],at)));
    }));
    runtime.bind("item_count",native([&](const auto& a,const auto& at) {
        arity(a,2,at); return Value(double(itemCount(world,requireEntity(a[0],world,at),itemId(runtime,a[1],at))));
    }));
    runtime.bind("set_interactable",native([&](const auto& a,const auto& at) {
        arity(a,2,at); world.add(requireEntity(a[0],world,at),InteractableComponent{a[1].boolean(at)}); return Value{};
    }));
    runtime.bind("nearest_interactable",native([&](const auto& a,const auto& at) {
        arity(a,2,at); return entityValue(world,nearestInteractable(world,requireEntity(a[0],world,at),number(a[1],at)));
    }));
    runtime.bind("action_pressed",native([&](const auto& a,const auto& at) {
        arity(a,1,at); return Value(input.pressed(a[0].string(at)));
    }));
    runtime.bind("action_held",native([&](const auto& a,const auto& at) {
        arity(a,1,at); return Value(input.held(a[0].string(at)));
    }));
    runtime.bind("play_sound",native([&](const auto& a,const auto& at) {
        arity(a,1,at); const auto sound=assets.sound(AssetId(a[0].string(at))); audio.play(sound.source,sound.streaming); return Value{};
    }));
}
}
