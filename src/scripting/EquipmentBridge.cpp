#include "scripting/EquipmentBridge.h"
#include "scripting/DataConversion.h"
#include "game/Equipment.h"
#include "game/InventorySystem.h"
#include "core/Log.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <limits>
namespace game {
namespace {
using namespace script;
void arity(const std::vector<Value>& a,size_t n,const Location& at) { if(a.size()!=n) fail(at,"ITEM101","Invalid item instance arguments"); }
double number(const Value& v,double low,double high,const Location& at) {
    const double n=v.number(at);if(!std::isfinite(n) || n<low || n>high) fail(at,"ITEM101","Invalid equipment number");return n;
}
ItemInstance& instance(EquipmentState& state,const Value& id,const Location& at) {
    const auto it=state.items.find(id.string(at));if(it==state.items.end()) fail(at,"ITEM101","Unknown owned item instance");return it->second;
}
void emit(Runtime& runtime,const std::string& name,const std::vector<Value>& args) {
    for(const auto& error:runtime.emit(name,args)) LOG_ERROR("SCRIPT",DiagnosticError(error).what());
}
}
void bindEquipmentApi(script::Runtime& runtime,World& world) {
    runtime.bind("roll_affixes",native([](const auto& a,const auto& at) {
        arity(a,3,at);const auto seed=a[0].string(at);if(seed.size()>256) fail(at,"ITEM101","Affix seed exceeds 256 bytes");
        const auto pool=std::get_if<std::shared_ptr<List>>(&a[1].data);const auto count=number(a[2],0,8,at);
        if(!pool || (*pool)->values.size()>64 || count!=std::floor(count) || count>(*pool)->values.size()) fail(at,"ITEM101","Invalid affix pool/count");
        struct Range { double minAdd,maxAdd,minMultiply,maxMultiply; };
        struct Choice { std::string id;double weight;std::map<std::string,Range> stats; };
        std::vector<Choice> choices;std::set<std::string> ids;
        for(const auto& row:(*pool)->values) {
            Choice choice;choice.id=row.member("id",at).string(at);(void)AssetId(choice.id);
            if(!ids.insert(choice.id).second) fail(at,"ITEM101","Duplicate affix ID");choice.weight=number(row.member("weight",at),.001,1000,at);
            const auto data=toData(row.member("stats",at));const auto& stats=std::get<Data::Object>(data.value);if(stats.size()>32) fail(at,"ITEM101","Affix stat limit exceeded");
            for(const auto& [stat,value]:stats) {
                if(!validEquipmentSlot(stat)) fail(at,"ITEM101","Invalid affix stat ID");const auto bounds=fromData(value);
                Range range{number(bounds.member("add_min",at),-1e6,1e6,at),number(bounds.member("add_max",at),-1e6,1e6,at),number(bounds.member("multiply_min",at),0,100,at),number(bounds.member("multiply_max",at),0,100,at)};
                if(range.minAdd>range.maxAdd || range.minMultiply>range.maxMultiply) fail(at,"ITEM101","Inverted affix range");choice.stats.emplace(stat,range);
            }
            choices.push_back(std::move(choice));
        }
        // FNV-1a seed followed by SplitMix64; no global RNG or order-dependent state.
        std::uint64_t state=14695981039346656037ULL;for(unsigned char c:seed) { state^=c;state*=1099511628211ULL; }
        const auto random=[&]() { auto x=(state+=0x9e3779b97f4a7c15ULL);x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;x^=x>>31;return double(x>>11)/9007199254740992.0; };
        auto result=object();
        for(int i=0;i<int(count);++i) {
            double weight=0;for(const auto& choice:choices) weight+=choice.weight;double cursor=random()*weight;size_t selected=0;
            while(selected+1<choices.size() && cursor>=choices[selected].weight) { cursor-=choices[selected].weight;++selected; }
            const auto& choice=choices[selected];auto stats=object();
            for(const auto& [id,range]:choice.stats) {
                auto bonus=object();bonus.setMember("add",Value(range.minAdd+(range.maxAdd-range.minAdd)*random()),at);bonus.setMember("multiply",Value(range.minMultiply+(range.maxMultiply-range.minMultiply)*random()),at);stats.setMember(id,bonus,at);
            }
            result.setMember(choice.id,stats,at);choices.erase(choices.begin()+selected);
        }
        return result;
    }));
    runtime.bind("create_item_instance",native([&](const auto& a,const auto& at) {
        arity(a,3,at);const auto actor=requireEntity(a[0],world,at);const auto item=a[1].string(at);
        (void)runtime.registry().get("Items",item,at);validateEquipment(world,actor);
        if(itemCount(world,actor,AssetId(item))<=reservedItems(world,actor,item)) fail(at,"ITEM101","No unreserved inventory copy");
        auto state=readEquipment(world,actor);
        if(state.items.size()>=256 || state.next>=9007199254740991ULL) fail(at,"ITEM101","Item instance limit reached");
        ItemInstance value;value.item=item;value.maximum=number(a[2].member("maximum",at),1,1e9,at);value.durability=value.maximum;
        value.stats=readBonuses(toData(a[2].member("stats",at)));
        const auto affixes=toData(a[2].member("affixes",at));const auto& rows=std::get<Data::Object>(affixes.value);
        if(rows.size()>8) fail(at,"ITEM101","Affix limit (8) exceeded");
        for(const auto& [id,stats]:rows) { (void)AssetId(id);value.affixes[id]=readBonuses(stats); }
        const auto id="i"+std::to_string(state.next++);state.items.emplace(id,std::move(value));writeEquipment(world,actor,state);return Value(id);
    }));
    runtime.bind("item_instances",native([&](const auto& a,const auto& at) {
        arity(a,1,at);const auto actor=requireEntity(a[0],world,at);(void)readEquipment(world,actor);
        const auto it=world.custom(actor).find("core:item_instances");
        std::vector<Value> rows;
        if(it!=world.custom(actor).end()) for(const auto& [id,data]:std::get<Data::Object>(std::get<Data::Object>(it->second.data.value).at("items").value)) {
            auto row=fromData(data);row.setMember("id",Value(id),at);rows.push_back(std::move(row));
        }
        return list(std::move(rows));
    }));
    runtime.bind("item_instance",native([&](const auto& a,const auto& at) {
        arity(a,2,at);const auto actor=requireEntity(a[0],world,at);auto state=readEquipment(world,actor);(void)instance(state,a[1],at);
        auto result=fromData(std::get<Data::Object>(std::get<Data::Object>(world.custom(actor).at("core:item_instances").data.value).at("items").value).at(a[1].string(at)));
        result.setMember("id",a[1],at);return result;
    }));
    runtime.bind("equipped_instance",native([&](const auto& a,const auto& at) {
        arity(a,2,at);const auto actor=requireEntity(a[0],world,at);const auto slot=a[1].string(at);
        for(const auto& [id,item]:readEquipment(world,actor).items) if(item.slot==slot) return Value(id);return Value{};
    }));
    runtime.bind("available_item_count",native([&](const auto& a,const auto& at) {
        arity(a,2,at);const auto actor=requireEntity(a[0],world,at);const auto item=a[1].string(at);validateEquipment(world,actor);
        return Value(double(itemCount(world,actor,AssetId(item))-reservedItems(world,actor,item)));
    }));
    runtime.bind("equip_instance",native([&](const auto& a,const auto& at) {
        arity(a,3,at);const auto actor=requireEntity(a[0],world,at);const auto slot=a[1].string(at);
        if(!validEquipmentSlot(slot)) fail(at,"ITEM101","Invalid equipment slot");
        const auto id=std::holds_alternative<std::monostate>(a[2].data)?std::string{}:a[2].string(at);
        auto state=readEquipment(world,actor);validateEquipment(world,actor);
        if(!id.empty()) { auto& value=instance(state,a[2],at);if(value.slot==slot) return Value(false);if(value.durability==0) fail(at,"ITEM101","Repair broken item before equipping"); }
        else if(!instanceSlotOccupied(world,actor,slot)) return Value(false);
        if(const auto found=world.custom(actor).find("core:equipment");found!=world.custom(actor).end() && std::get<Data::Object>(found->second.data.value).contains(slot)) fail(at,"ITEM101","Unequip the stack item before equipping an instance in its slot");
        auto context=object();context.setMember("actor",a[0],at);context.setMember("slot",Value(slot),at);context.setMember("instance",a[2],at);context.setMember("allow",Value(true),at);
        // Do not hold entity/component references across mod callbacks.
        context=runtime.filter("core:equip_instance",context);if(!context.member("allow",at).boolean(at)) return Value(false);
        (void)requireEntity(a[0],world,at);validateEquipment(world,actor);state=readEquipment(world,actor);
        if(const auto found=world.custom(actor).find("core:equipment");found!=world.custom(actor).end() && std::get<Data::Object>(found->second.data.value).contains(slot)) fail(at,"ITEM101","Slot changed during equip hook");
        for(auto& [key,value]:state.items) { (void)key;if(value.slot==slot) value.slot.clear(); }
        if(!id.empty()) { auto& value=instance(state,a[2],at);if(value.durability==0) fail(at,"ITEM101","Item broke during equip hook");value.slot=slot; }
        size_t occupied=0;for(const auto& [key,value]:state.items) { (void)key;occupied+=!value.slot.empty(); }
        if(const auto found=world.custom(actor).find("core:equipment");found!=world.custom(actor).end()) occupied+=std::get<Data::Object>(found->second.data.value).size();
        if(occupied>32) fail(at,"ITEM101","Equipment slot limit (32) exceeded");
        writeEquipment(world,actor,state);emit(runtime,"on_instance_equipped",{a[0],a[1],a[2]});return Value(true);
    }));
    runtime.bind("transfer_item_instance",native([&](const auto& a,const auto& at) {
        arity(a,3,at);auto source=requireEntity(a[0],world,at),target=requireEntity(a[1],world,at);
        if(source==target) fail(at,"ITEM101","Transfer requires distinct owners");
        auto checkOwners=[&] { validateEquipment(world,source);validateEquipment(world,target);auto state=readEquipment(world,source);if(!instance(state,a[2],at).slot.empty()) fail(at,"ITEM101","Unequip before transferring an instance"); };
        checkOwners();auto context=object();context.setMember("source",a[0],at);context.setMember("target",a[1],at);context.setMember("instance",a[2],at);context.setMember("allow",Value(true),at);
        context=runtime.filter("core:transfer_instance",context);if(!context.member("allow",at).boolean(at)) return Value{};
        source=requireEntity(a[0],world,at);target=requireEntity(a[1],world,at);checkOwners();
        auto from=readEquipment(world,source),to=readEquipment(world,target);const auto value=instance(from,a[2],at);
        if(to.items.size()>=256 || to.next>=9007199254740991ULL) fail(at,"ITEM101","Recipient instance capacity exceeded");
        const auto id="i"+std::to_string(to.next++);to.items.emplace(id,value);from.items.erase(a[2].string(at));
        EntityDataUpdate left{source,world.get<InventoryComponent>(source),world.custom(source)};
        EntityDataUpdate right{target,world.has<InventoryComponent>(target)?world.get<InventoryComponent>(target):InventoryComponent{},world.custom(target)};
        auto& count=left.inventory.items.at(value.item);if(--count==0) left.inventory.items.erase(value.item);
        auto& received=right.inventory.items[value.item];if(received==std::numeric_limits<std::uint32_t>::max()) fail(at,"ITEM101","Recipient inventory overflow");++received;
        left.custom["core:item_instances"]=equipmentData(from);right.custom["core:item_instances"]=equipmentData(to);
        world.replaceEntityData({std::move(left),std::move(right)});
        emit(runtime,"on_instance_transferred",{a[0],a[1],a[2],Value(id)});return Value(id);
    }));
    runtime.bind("item_durability",native([&](const auto& a,const auto& at) {
        arity(a,3,at);const auto actor=requireEntity(a[0],world,at);auto state=readEquipment(world,actor);auto& value=instance(state,a[1],at);
        const double next=number(a[2],0,value.maximum,at),old=value.durability;
        if(next==old) return Value(false);value.durability=next;writeEquipment(world,actor,state);
        emit(runtime,"on_item_durability",{a[0],a[1],Value(old),Value(next)});return Value(true);
    }));
    runtime.bind("release_item_instance",native([&](const auto& a,const auto& at) {
        arity(a,2,at);const auto actor=requireEntity(a[0],world,at);auto state=readEquipment(world,actor);
        if(!instance(state,a[1],at).slot.empty()) fail(at,"ITEM101","Unequip before releasing an instance");
        state.items.erase(a[1].string(at));writeEquipment(world,actor,state);return Value{};
    }));
    runtime.bind("equipment_stat",native([&](const auto& a,const auto& at) {
        arity(a,3,at);return Value(equipmentStat(world,requireEntity(a[0],world,at),a[1].string(at),a[2].number(at)));
    }));
}
}
