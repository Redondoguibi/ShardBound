#include "scripting/GameplayBridge.h"
#include "scripting/EquipmentBridge.h"
#include "scripting/AttributeBridge.h"
#include "scripting/ProjectileBridge.h"
#include "game/Equipment.h"
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
    bindEquipmentApi(runtime,world);
    bindAttributeApi(runtime,world);
    bindProjectileApi(runtime,world,physics);
    runtime.bind("set_scale",native([&](const auto& a,const auto& at) {
        arity(a,2,at); const auto id=requireEntity(a[0],world,at); const auto scale=vector(a[1],at);
        if(scale.x<=0 || scale.y<=0 || scale.z<=0) fail(at,"BRIDGE105","Scale must be positive");
        world.get<TransformComponent>(id).local.scale=scale; return Value{};
    }));
    runtime.bind("set_model",native([&](const auto& a,const auto& at) {
        arity(a,2,at); const auto entity=requireEntity(a[0],world,at); const AssetId id(a[1].string(at));
        const auto model=assets.model(id);
        if(world.has<ModelComponent>(entity)) {
            auto& visual=world.get<ModelComponent>(entity); visual.definition=id; visual.model=model; visual.poses.clear(); visual.nodeWorld.clear();
        } else world.add(entity,ModelComponent{id,model,{},{}});
        return Value{};
    }));
    runtime.bind("add_box",native([&](const auto& a,const auto& at) {
        if(a.size()!=3 && a.size()!=4) fail(at,"GAME104","add_box(entity, half, dynamic, rotation?)");physics.attachBox(requireEntity(a[0],world,at),vector(a[1],at),a[2].boolean(at),a.size()==4?vector(a[3],at):Vec3{});return Value{};
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
    runtime.bind("set_maximum_health",native([&](const auto& a,const auto& at) {
        arity(a,2,at);const auto maximum=number(a[1],at);if(maximum<=0 || maximum>1e6f) fail(at,"BRIDGE105","Maximum health must be in (0,1000000]");
        auto& health=world.get<HealthComponent>(requireEntity(a[0],world,at));health.maximum=maximum;health.current=std::min(health.current,maximum);return Value{};
    }));
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
    // Stable, capped spatial query for mod AI and proximity mechanics.
    runtime.bind("entities_near",native([&](const auto& a,const auto& at) {
        if(a.size()!=2 && a.size()!=3) fail(at,"BRIDGE101","entities_near(center, radius, definition?)");
        Vec3 center;
        if(std::holds_alternative<std::shared_ptr<HostObject>>(a[0].data))
            center=world.get<TransformComponent>(requireEntity(a[0],world,at)).local.position;
        else center=vector(a[0],at);
        const double radius=a[1].number(at);
        if(!std::isfinite(radius) || radius<0 || radius>20000) fail(at,"BRIDGE105","Invalid query radius");
        const std::string definition=a.size()==3 ? a[2].string(at) : std::string{};
        const double r2=radius*radius;
        std::vector<std::pair<double,EntityId>> matches;
        for(const auto id:world.entities()) {
            if(!world.has<TransformComponent>(id) || !world.has<ActiveComponent>(id) || !world.get<ActiveComponent>(id).value) continue;
            const auto& identity=world.get<IdentityComponent>(id);
            if(identity.definition=="runtime_chunk" || (!definition.empty() && identity.definition!=definition)) continue;
            const auto p=world.get<TransformComponent>(id).local.position;
            const double dx=double(p.x)-center.x,dy=double(p.y)-center.y,dz=double(p.z)-center.z,dist=dx*dx+dy*dy+dz*dz;
            if(dist<=r2) matches.emplace_back(dist,id);
        }
        std::sort(matches.begin(),matches.end(),[](const auto& a,const auto& b){
            return a.first==b.first?a.second.value<b.second.value:a.first<b.first;
        });
        if(matches.size()>2048) fail(at,"BRIDGE105","entities_near query exceeded 2048 results");
        std::vector<Value> values;values.reserve(matches.size());
        for(const auto& [distance,id]:matches) { (void)distance;values.push_back(entityValue(world,id)); }
        return list(std::move(values));
    }));
    const auto equipmentState=[&world](EntityId entity,const Location& at) {
        Data::Object slots;
        const auto& custom=world.custom(entity);
        if(const auto it=custom.find("core:equipment");it!=custom.end()) {
            if(it->second.version!=1) fail(at,"BRIDGE105","Unknown equipment data version");
            const auto* data=std::get_if<Data::Object>(&it->second.data.value);
            if(!data) fail(at,"BRIDGE105","Invalid equipment data");
            slots=*data;
        }
        return slots;
    };
    runtime.bind("equipped_item",native([&world,equipmentState](const auto& a,const auto& at) {
        arity(a,2,at);const auto e=requireEntity(a[0],world,at);const auto slot=a[1].string(at);
        auto slots=equipmentState(e,at);const auto it=slots.find(slot);
        if(it==slots.end()) {
            for(const auto& [id,item]:readEquipment(world,e).items) { (void)id;if(item.slot==slot) return Value(item.item); }
            return Value{};
        }
        const auto* item=std::get_if<std::string>(&it->second.value);
        if(!item) fail(at,"BRIDGE105","Corrupt equipment slot");
        return Value(*item);
    }));
    runtime.bind("equipment_slots",native([&world,equipmentState](const auto& a,const auto& at) {
        arity(a,1,at);const auto actor=requireEntity(a[0],world,at);auto slots=equipmentState(actor,at);
        std::vector<Value> result;
        for(const auto& [slot,data]:slots) {
            const auto* item=std::get_if<std::string>(&data.value);
            if(!item) fail(at,"BRIDGE105","Corrupt equipment slot");
            auto row=object();row.setMember("slot",Value(slot),at);row.setMember("item",Value(*item),at);result.push_back(std::move(row));
        }
        for(const auto& [id,item]:readEquipment(world,actor).items) if(!item.slot.empty()) {
            auto row=object();row.setMember("slot",Value(item.slot),at);row.setMember("item",Value(item.item),at);row.setMember("instance",Value(id),at);result.push_back(std::move(row));
        }
        return list(std::move(result));
    }));
    runtime.bind("equip_slot",native([&world,&runtime,equipmentState](const auto& a,const auto& at) {
        arity(a,3,at);const auto e=requireEntity(a[0],world,at);const auto slot=a[1].string(at);
        if(!validEquipmentSlot(slot)) fail(at,"BRIDGE105","Invalid equipment slot identifier");
        if(instanceSlotOccupied(world,e,slot)) fail(at,"BRIDGE105","Unequip item instance before using this slot");
        const auto item=std::holds_alternative<std::monostate>(a[2].data) ? std::string{} :
            std::holds_alternative<Symbol>(a[2].data) ? std::get<Symbol>(a[2].data).name : a[2].string(at);
        if(!item.empty()) {
            (void)runtime.registry().get("Items",item,at);
            if(!world.has<InventoryComponent>(e) || !world.get<InventoryComponent>(e).items.contains(item))
                fail(at,"BRIDGE105","Cannot equip an item not in inventory");
        }
        auto slots=equipmentState(e,at);
        if(const auto it=slots.find(slot);it!=slots.end()) {
            const auto* current=std::get_if<std::string>(&it->second.value);
            if(!current) fail(at,"BRIDGE105","Corrupt equipment slot");
            if(*current==item) return Value(false);
        } else if(item.empty()) return Value(false);
        if(!item.empty()) {
            unsigned occupied=0;
            for(const auto& [id,instance]:readEquipment(world,e).items) { (void)id;if(instance.item==item) ++occupied; }
            for(const auto& [existingSlot,data]:slots) {
                const auto* equipped=std::get_if<std::string>(&data.value);
                if(!equipped) fail(at,"BRIDGE105","Corrupt equipment slot");
                if(existingSlot!=slot && *equipped==item) ++occupied;
            }
            if(world.get<InventoryComponent>(e).items.at(item)<=occupied)
                fail(at,"BRIDGE105","Not enough copies to equip item in another slot");
        }
        auto context=object();context.setMember("actor",a[0],at);context.setMember("slot",Value(slot),at);
        context.setMember("item",Value(item),at);context.setMember("allow",Value(true),at);
        context=runtime.filter("core:equip",context);
        if(!context.member("allow",at).boolean(at)) return Value(false);
        (void)requireEntity(a[0],world,at);validateEquipment(world,e);slots=equipmentState(e,at);
        if(instanceSlotOccupied(world,e,slot)) fail(at,"BRIDGE105","Slot changed during equip hook");
        if(!item.empty()) {
            unsigned needed=reservedItems(world,e,item);
            if(const auto old=slots.find(slot);old!=slots.end() && std::get<std::string>(old->second.value)==item) --needed;
            if(itemCount(world,e,AssetId(item))<=needed) fail(at,"BRIDGE105","Inventory changed during equip hook");
        }
        if(item.empty()) slots.erase(slot);
        else {
            size_t occupied=slots.size();for(const auto& [id,instance]:readEquipment(world,e).items) {(void)id;occupied+=!instance.slot.empty();}
            if(!slots.contains(slot) && occupied>=32) fail(at,"BRIDGE105","Equipment slot limit exceeded");Data d;d.value=item;slots[slot]=std::move(d);
        }
        Data state;state.value=std::move(slots);
        world.setCustom(e,AssetId("core:equipment"),CustomComponent{1,std::move(state)});
        for(const auto& error:runtime.emit("on_equipped",{a[0],Value(slot),item.empty()?Value{}:Value(item)})) LOG_ERROR("SCRIPT",DiagnosticError(error).what());
        return Value(true);
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
        arity(a,0,at); const float scale=std::min(input.width()/1280.0f,input.height()/720.0f)*input.uiScale();
        const float x=(input.mouseX()-(input.width()-1280*scale)/2)/scale,y=(input.mouseY()-(input.height()-720*scale)/2)/scale;
        auto out=object(); out.setMember("x",Value(double(x)),at); out.setMember("y",Value(double(y)),at);
        out.setMember("valid",Value(input.hasPointer() && x>=0 && y>=0 && x<1280 && y<720),at); return out;
    }));
    const auto cameraMatrix=[&]() {
        using namespace DirectX; const auto c=player.viewCamera();
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
        const float w=clip.w==0 ? 1 : clip.w,scale=std::min(input.width()/1280.0f,input.height()/720.0f)*input.uiScale();
        out.setMember("x",Value(double(((clip.x/w*.5f+.5f)*input.width()-(input.width()-1280*scale)/2)/scale)),at);
        out.setMember("y",Value(double(((-clip.y/w*.5f+.5f)*input.height()-(input.height()-720*scale)/2)/scale)),at);
        out.setMember("visible",Value(visible),at); return out;
    }));
    runtime.bind("audio_volume",native([&](const auto& a,const auto& at) { arity(a,1,at); audio.setVolume(number(a[0],at)); return Value{}; }));
    runtime.bind("play_sound",native([&](const auto& a,const auto& at) {
        if(a.size()!=1 && a.size()!=2) fail(at,"AUDIO111","play_sound(id, bus?)");AudioBus bus=AudioBus::Effects;
        if(a.size()==2) {const auto name=a[1].string(at);if(name=="music") bus=AudioBus::Music;else if(name=="interface") bus=AudioBus::Interface;else if(name!="effects") fail(at,"AUDIO111","Unknown audio bus");}
        const auto sound=assets.sound(AssetId(a[0].string(at))); audio.play(sound.source,sound.streaming,bus); return Value{};
    }));
    runtime.bind("jump",native([&](const auto& a,const auto& at) { arity(a,1,at); return Value(player.jump(number(a[0],at))); }));
    runtime.bind("set_player_step_height",native([&](const auto& a,const auto& at) {arity(a,1,at);player.stepHeight(number(a[0],at));return Value{};}));
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
    runtime.bind("physics_raycast",native([&](const auto& a,const auto& at) {
        arity(a,3,at);const auto from=vector(a[0],at),to=vector(a[1],at);
        const auto entries=std::get_if<std::shared_ptr<List>>(&a[2].data);
        if(!entries || (*entries)->values.size()>16) fail(at,"BRIDGE105","Raycast ignore list exceeds 16 entities");
        std::vector<EntityId> ignored;for(const auto& v:(*entries)->values) ignored.push_back(requireEntity(v,world,at));
        const auto hit=physics.raycast(from,to,ignored);if(!hit) return Value{};
        auto result=object();result.setMember("entity",entityValue(world,hit->entity),at);result.setMember("fraction",Value(double(hit->fraction)),at);
        const auto vec=[&](Vec3 v) { auto row=object();row.setMember("x",Value(double(v.x)),at);row.setMember("y",Value(double(v.y)),at);row.setMember("z",Value(double(v.z)),at);return row; };
        result.setMember("position",vec({from.x+(to.x-from.x)*hit->fraction,from.y+(to.y-from.y)*hit->fraction,from.z+(to.z-from.z)*hit->fraction}),at);
        result.setMember("normal",vec(hit->normal),at);return result;
    }));
    runtime.bind("set_rotation",native([&](const auto& a,const auto& at) { arity(a,2,at); world.get<TransformComponent>(requireEntity(a[0],world,at)).local.rotation=vector(a[1],at); return Value{}; }));
    runtime.bind("attach_socket",native([&](const auto& a,const auto& at) {
        arity(a,4,at);const auto child=requireEntity(a[0],world,at),parent=requireEntity(a[1],world,at);const auto node=a[2].string(at);const auto offset=vector(a[3],at);
        if(physics.contains(child)) fail(at,"GAME104","Socket visuals must not have physics bodies");
        if(node.empty() || node.size()>256 || !world.socketMatrix(parent,node)) fail(at,"BRIDGE105","Socket node is missing or ambiguous");
        if(!world.has<TransformComponent>(child)) fail(at,"BRIDGE105","Socket visual needs transform");
        world.setParent(child,parent);world.setCustom(child,AssetId("core:socket"),{1,Data{node}});
        world.get<TransformComponent>(child).local.position=offset;return Value{};
    }));
    runtime.bind("set_parent",native([&](const auto& a,const auto& at) {
        arity(a,2,at); const auto child=requireEntity(a[0],world,at);
        if(physics.contains(child)) fail(at,"GAME104","Physical entities must remain roots");
        world.setParent(child,std::holds_alternative<std::monostate>(a[1].data) ? EntityId{} : requireEntity(a[1],world,at));world.removeCustom(child,AssetId("core:socket")); return Value{};
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
        arity(a,1,at); const auto value=a[0].number(at);if(!std::isfinite(value)) fail(at,"BRIDGE105","Expected finite number");
        std::ostringstream text; text<<std::fixed<<std::setprecision(0)<<value; return Value(text.str());
    }));
}
}
