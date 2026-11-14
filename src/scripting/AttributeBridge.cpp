#include "scripting/AttributeBridge.h"
#include "game/Attributes.h"
#include "game/Equipment.h"
#include "scripting/DataConversion.h"
#include "core/Log.h"
#include <algorithm>
#include <cmath>
namespace game {
namespace {
using namespace script;
struct Bounds { double initial,minimum,maximum; };
Bounds bounds(Runtime& runtime,const Value& data,const Location& at) {
    auto initial=data.member("default",at);
    if(std::holds_alternative<std::shared_ptr<Object>>(initial.data)) {
        const auto category=initial.member("category",at).string(at),id=initial.member("id",at).string(at),field=initial.member("field",at).string(at);
        initial=runtime.registry().get(category,id,at).data.member(field,at);
    }
    Bounds b{initial.number(at),data.member("min",at).number(at),data.member("max",at).number(at)};
    if(!std::isfinite(b.initial) || !std::isfinite(b.minimum) || !std::isfinite(b.maximum) || b.minimum>b.initial || b.initial>b.maximum || std::abs(b.minimum)>1e12 || std::abs(b.maximum)>1e12) fail(at,"ATTR101","Invalid attribute default/min/max");return b;
}
void arity(const std::vector<Value>& a,size_t n,const Location& at) { if(a.size()!=n) fail(at,"ATTR101","Invalid attribute arguments"); }
}
void bindAttributeApi(script::Runtime& runtime,World& world) {
    runtime.bind("register_attribute",native([&runtime](const auto& a,const auto& at) {
        arity(a,2,at);const auto id=a[0].string(at);const auto owner=runtime.currentMod();
        if(owner.empty() || AssetId(id).nameSpace()!=owner || id.size()>64) fail(at,"ATTR101","Attribute ID must belong to current mod and fit 64 bytes");
        if(runtime.registry().ids("Attributes").size()>=1024) fail(at,"ATTR101","Attribute definition limit exceeded");
        (void)bounds(runtime,a[1],at);(void)toData(a[1]);runtime.registry().add({id,"Attributes","attribute",clone(a[1])},at);return Value{};
    }));
    runtime.bind("get_base_attribute",native([&](const auto& a,const auto& at) {
        arity(a,2,at);const auto actor=requireEntity(a[0],world,at);const auto id=a[1].string(at);const auto b=bounds(runtime,runtime.registry().get("Attributes",id,at).data,at);
        return Value(attributeBase(world,actor,id,b.initial));
    }));
    runtime.bind("get_attribute",native([&](const auto& a,const auto& at) {
        arity(a,2,at);const auto actor=requireEntity(a[0],world,at);const auto id=a[1].string(at);const auto b=bounds(runtime,runtime.registry().get("Attributes",id,at).data,at);
        return Value(std::clamp(equipmentStat(world,actor,id,b.initial),b.minimum,b.maximum));
    }));
    const auto update=[&](const std::vector<Value>& a,const Location& at,bool reset) {
        arity(a,reset?2:3,at);const auto actor=requireEntity(a[0],world,at);const auto id=a[1].string(at);const auto b=bounds(runtime,runtime.registry().get("Attributes",id,at).data,at);
        const auto next=reset?b.initial:a[2].number(at);if(!std::isfinite(next) || next<b.minimum || next>b.maximum) fail(at,"ATTR101","Attribute base outside registered range");
        (void)equipmentStat(world,actor,id,next,false);
        const auto before=attributeBase(world,actor,id,b.initial);writeAttributeBase(world,actor,id,reset?std::nullopt:std::optional<double>(next));
        if(before!=next) for(const auto& error:runtime.emit("on_attribute_changed",{a[0],Value(id),Value(before),Value(next)})) LOG_ERROR("SCRIPT",DiagnosticError(error).what());
        return Value(next);
    };
    runtime.bind("set_attribute",native([update](const auto& a,const auto& at) { return update(a,at,false); }));
    runtime.bind("reset_attribute",native([update](const auto& a,const auto& at) { return update(a,at,true); }));
}
}
