#include "scripting/GameplayBridge.h"
#include "game/HealthSystem.h"
#include "game/InventorySystem.h"
#include "game/InteractionSystem.h"
#include "core/Log.h"
#include <cmath>
#include <algorithm>
#include <bit>
#include <DirectXMath.h>
#include <limits>
#include <sstream>
#include <iomanip>
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
    runtime.bind("set_model",native([&](const auto& a,const auto& at) {
        arity(a,2,at); const auto entity=requireEntity(a[0],world,at); const AssetId id(a[1].string(at));
        const auto model=assets.model(id);
        if(world.has<ModelComponent>(entity)) {
            auto& visual=world.get<ModelComponent>(entity); visual.definition=id; visual.model=model; visual.poses.clear();
        } else world.add(entity,ModelComponent{id,model,{},{}});
        return Value{};
    }));
    runtime.bind("add_box",native([&](const auto& a,const auto& at) {
        arity(a,3,at); physics.attachBox(requireEntity(a[0],world,at),vector(a[1],at),a[2].boolean(at)); return Value{};
    }));
    runtime.bind("set_gravity_factor",native([&](const auto& a,const auto& at) {
        arity(a,2,at); physics.setGravityFactor(requireEntity(a[0],world,at),number(a[1],at)); return Value{};
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
    runtime.bind("maximum_health",native([&](const auto& a,const auto& at) { arity(a,1,at); return Value(double(world.get<HealthComponent>(requireEntity(a[0],world,at)).maximum)); }));
    const auto applyDamage=[&](Value victim,Value requested,Value source,const Location& at) {
        const bool attributed=!std::holds_alternative<std::monostate>(source.data);
        if(attributed) (void)requireEntity(source,world,at);
        auto context=object(); context.setMember("target",victim,at); context.setMember("amount",requested,at);
        context.setMember("source",source,at);
        context=runtime.filter("core:damage",context);
        const auto target=context.member("target",at); const auto amount=number(context.member("amount",at),at);
        const auto applied=damage(world,requireEntity(target,world,at),amount);
        for(const auto& error:runtime.emit("on_damage",{target,Value(double(applied))})) LOG_ERROR("SCRIPT",DiagnosticError(error).what());
        if(attributed) for(const auto& error:runtime.emit("on_damage_dealt",{source,target,Value(double(applied))})) LOG_ERROR("SCRIPT",DiagnosticError(error).what());
        return Value(double(applied));
    };
    runtime.bind("damage",native([applyDamage](const auto& a,const auto& at) { arity(a,2,at); return applyDamage(a[0],a[1],Value{},at); }));
    runtime.bind("damage_from",native([applyDamage](const auto& a,const auto& at) { arity(a,3,at); return applyDamage(a[1],a[2],a[0],at); }));
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
    runtime.bind("inventory_entries",native([&](const auto& a,const auto& at) {
        arity(a,1,at); const auto entity=requireEntity(a[0],world,at); std::vector<Value> values;
        if(world.has<InventoryComponent>(entity)) for(const auto& [id,count]:world.get<InventoryComponent>(entity).items) {
            auto item=object(); item.setMember("id",Value(id),at); item.setMember("count",Value(double(count)),at); values.push_back(std::move(item));
        }
        return list(std::move(values));
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
    runtime.bind("pointer_position",native([&](const auto& a,const auto& at) {
        arity(a,0,at); const float scale=std::min(input.width()/1280.0f,input.height()/720.0f);
        const float x=(input.mouseX()-(input.width()-1280*scale)/2)/scale,y=(input.mouseY()-(input.height()-720*scale)/2)/scale;
        auto out=object(); out.setMember("x",Value(double(x)),at); out.setMember("y",Value(double(y)),at);
        out.setMember("valid",Value(input.hasPointer() && x>=0 && y>=0 && x<1280 && y<720),at); return out;
    }));
    const auto cameraMatrix=[&]() {
        using namespace DirectX; const auto c=player.camera();
        return XMMatrixLookAtLH(XMVectorSet(c.eye.x,c.eye.y,c.eye.z,1),XMVectorSet(c.target.x,c.target.y,c.target.z,1),XMVectorSet(0,1,0,0))*
            XMMatrixPerspectiveFovLH(c.fov,float(input.width())/input.height(),c.nearPlane,c.farPlane);
    };
    runtime.bind("aim_position",native([&,cameraMatrix](const auto& a,const auto& at) {
        arity(a,0,at); if(!input.hasPointer() || !player.entity()) return Value{};
        using namespace DirectX; const auto inverse=XMMatrixInverse(nullptr,cameraMatrix());
        const float x=2*input.mouseX()/input.width()-1,y=1-2*input.mouseY()/input.height();
        XMFLOAT3 nearPoint,farPoint; XMStoreFloat3(&nearPoint,XMVector3TransformCoord(XMVectorSet(x,y,0,1),inverse)); XMStoreFloat3(&farPoint,XMVector3TransformCoord(XMVectorSet(x,y,1,1),inverse));
        const auto plane=world.get<TransformComponent>(player.entity()).local.position.y;
        const float dy=farPoint.y-nearPoint.y; if(std::abs(dy)<.0001f) return Value{};
        const float t=(plane-nearPoint.y)/dy; if(t<0 || t>1) return Value{};
        auto out=object(); out.setMember("x",Value(double(nearPoint.x+t*(farPoint.x-nearPoint.x))),at); out.setMember("y",Value(double(plane)),at); out.setMember("z",Value(double(nearPoint.z+t*(farPoint.z-nearPoint.z))),at); return out;
    }));
    runtime.bind("screen_position",native([&,cameraMatrix](const auto& a,const auto& at) {
        arity(a,1,at); using namespace DirectX; const auto v=vector(a[0],at);
        XMFLOAT4 clip; XMStoreFloat4(&clip,XMVector4Transform(XMVectorSet(v.x,v.y,v.z,1),cameraMatrix()));
        auto out=object(); const bool visible=clip.w>0 && clip.z>=0 && clip.z<=clip.w && std::abs(clip.x)<=clip.w && std::abs(clip.y)<=clip.w;
        const float w=clip.w==0 ? 1 : clip.w,scale=std::min(input.width()/1280.0f,input.height()/720.0f);
        out.setMember("x",Value(double(((clip.x/w*.5f+.5f)*input.width()-(input.width()-1280*scale)/2)/scale)),at);
        out.setMember("y",Value(double(((-clip.y/w*.5f+.5f)*input.height()-(input.height()-720*scale)/2)/scale)),at);
        out.setMember("visible",Value(visible),at); return out;
    }));
    runtime.bind("audio_volume",native([&](const auto& a,const auto& at) { arity(a,1,at); audio.setVolume(number(a[0],at)); return Value{}; }));
    runtime.bind("play_sound",native([&](const auto& a,const auto& at) {
        arity(a,1,at); const auto sound=assets.sound(AssetId(a[0].string(at))); audio.play(sound.source,sound.streaming); return Value{};
    }));
    runtime.bind("jump",native([&](const auto& a,const auto& at) { arity(a,1,at); return Value(player.jump(number(a[0],at))); }));
    runtime.bind("set_camera_offset",native([&](const auto& a,const auto& at) { arity(a,1,at); player.setViewOffset(vector(a[0],at)); return Value{}; }));
    runtime.bind("movement_enabled",native([&](const auto& a,const auto& at) { arity(a,1,at); player.setEnabled(a[0].boolean(at)); return Value{}; }));
    runtime.bind("keyboard_movement_only",native([&](const auto& a,const auto& at) { arity(a,1,at); input.keyboardMovementOnly(a[0].boolean(at)); return Value{}; }));
    runtime.bind("is_grounded",native([&](const auto& a,const auto& at) { arity(a,1,at); return Value(physics.grounded(requireEntity(a[0],world,at))); }));
    runtime.bind("get_velocity",native([&](const auto& a,const auto& at) {
        arity(a,1,at); auto v=physics.velocity(requireEntity(a[0],world,at)); auto result=object();
        result.setMember("x",Value(double(v.x)),at); result.setMember("y",Value(double(v.y)),at); result.setMember("z",Value(double(v.z)),at); return result;
    }));
    runtime.bind("set_velocity",native([&](const auto& a,const auto& at) { arity(a,2,at); physics.setVelocity(requireEntity(a[0],world,at),vector(a[1],at)); return Value{}; }));
    runtime.bind("teleport",native([&](const auto& a,const auto& at) { arity(a,2,at); physics.teleport(requireEntity(a[0],world,at),vector(a[1],at)); return Value{}; }));
    runtime.bind("line_of_sight",native([&](const auto& a,const auto& at) { arity(a,2,at); return Value(physics.clearPath(requireEntity(a[0],world,at),requireEntity(a[1],world,at))); }));
    runtime.bind("set_rotation",native([&](const auto& a,const auto& at) { arity(a,2,at); world.get<TransformComponent>(requireEntity(a[0],world,at)).local.rotation=vector(a[1],at); return Value{}; }));
    runtime.bind("set_parent",native([&](const auto& a,const auto& at) {
        arity(a,2,at); const auto child=requireEntity(a[0],world,at);
        if(physics.contains(child)) fail(at,"GAME104","Physical entities must remain roots");
        world.setParent(child,std::holds_alternative<std::monostate>(a[1].data) ? EntityId{} : requireEntity(a[1],world,at)); return Value{};
    }));
    runtime.bind("set_tint",native([&](const auto& a,const auto& at) {
        arity(a,2,at); const auto v=vector(a[1],at);
        if(v.x<0 || v.x>1 || v.y<0 || v.y>1 || v.z<0 || v.z>1) fail(at,"BRIDGE105","Tint must be in [0,1]");
        world.get<ModelComponent>(requireEntity(a[0],world,at)).tint={v.x,v.y,v.z,1}; return Value{};
    }));
    runtime.bind("pose_part",native([&](const auto& a,const auto& at) {
        arity(a,4,at); auto& model=world.get<ModelComponent>(requireEntity(a[0],world,at));
        const auto prefix=a[1].string(at); bool found=false;
        if(model.model) for(const auto& name:model.model->names) found|=name.starts_with(prefix);
        if(prefix.empty() || !found) fail(at,"BRIDGE105","Model part prefix not found");
        const auto pivot=vector(a[2],at),rotation=vector(a[3],at);
        model.poses[prefix]=multiply(transformMatrix({{-pivot.x,-pivot.y,-pivot.z}}),transformMatrix({pivot,rotation})); return Value{};
    }));
    runtime.bind("get_entities",native([&](const auto& a,const auto& at) {
        arity(a,1,at); const auto definition=a[0].string(at); std::vector<Value> values;
        for(auto id:world.entities()) if(world.get<IdentityComponent>(id).definition==definition) values.push_back(entityValue(world,id));
        return list(std::move(values));
    }));
    runtime.bind("entity_definition",native([&](const auto& a,const auto& at) {
        arity(a,1,at); return Value(world.get<IdentityComponent>(requireEntity(a[0],world,at)).definition);
    }));
    runtime.bind("entity_visual",native([&](const auto& a,const auto& at) {
        arity(a,1,at); const auto id=requireEntity(a[0],world,at);
        const auto& transform=world.get<TransformComponent>(id).local;
        const auto& model=world.get<ModelComponent>(id);
        auto vec=[&](Vec3 v) { auto out=object(); out.setMember("x",Value(double(v.x)),at); out.setMember("y",Value(double(v.y)),at); out.setMember("z",Value(double(v.z)),at); return out; };
        auto out=object(); out.setMember("model",Value(model.definition.str()),at);
        out.setMember("position",vec(transform.position),at); out.setMember("scale",vec(transform.scale),at);
        out.setMember("rotation",vec(transform.rotation),at); out.setMember("tint",vec({model.tint.r,model.tint.g,model.tint.b}),at);
        // A stable, exact visual key lets content reconcile its old decoration without relying on entity order.
        std::ostringstream signature; signature<<'v'<<std::hex<<std::setfill('0');
        for(unsigned char ch:model.definition.str()) signature<<std::setw(2)<<unsigned(ch);
        for(float value:{transform.position.x,transform.position.y,transform.position.z,transform.rotation.x,transform.rotation.y,transform.rotation.z,transform.scale.x,transform.scale.y,transform.scale.z,model.tint.r,model.tint.g,model.tint.b})
            signature<<'_'<<std::setw(8)<<std::bit_cast<std::uint32_t>(value);
        out.setMember("signature",Value(signature.str()),at);
        out.setMember("physical",Value(physics.contains(id)),at); return out;
    }));
    runtime.bind("math_floor",native([](const auto& a,const auto& at) { arity(a,1,at); return Value(std::floor(a[0].number(at))); }));
    runtime.bind("math_sin",native([](const auto& a,const auto& at) { arity(a,1,at); return Value(std::sin(number(a[0],at))); }));
    runtime.bind("math_sqrt",native([](const auto& a,const auto& at) { arity(a,1,at); const auto v=number(a[0],at); if(v<0) fail(at,"BRIDGE105","sqrt requires nonnegative input"); return Value(std::sqrt(double(v))); }));
    runtime.bind("math_atan2",native([](const auto& a,const auto& at) { arity(a,2,at); return Value(std::atan2(double(number(a[0],at)),double(number(a[1],at)))); }));
    runtime.bind("number_text",native([](const auto& a,const auto& at) {
        arity(a,1,at); std::ostringstream text; text<<std::fixed<<std::setprecision(0)<<number(a[0],at); return Value(text.str());
    }));
}
}
