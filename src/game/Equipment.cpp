#include "game/Equipment.h"
#include "game/Attributes.h"
#include <cmath>
#include <algorithm>
#include <set>
namespace game {
namespace {
const Data::Object& object(const Data& value) { return std::get<Data::Object>(value.value); }
double number(const Data& data,double low,double high) {
    const double n=std::get<double>(data.value);
    if(!std::isfinite(n) || n<low || n>high) throw std::runtime_error("Invalid equipment number");return n;
}
Data bonusesData(const StatBonuses& stats) {
    Data::Object result;for(const auto& [id,bonus]:stats) result[id]=Data{Data::Object{{"add",Data{bonus.add}},{"multiply",Data{bonus.multiply}}}};return Data{result};
}
}
bool validEquipmentSlot(const std::string& slot) {
    return !slot.empty() && slot.size()<=64 && std::all_of(slot.begin(),slot.end(),[](unsigned char c){return (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c==':' || c=='-';});
}
StatBonuses readBonuses(const Data& data) {
    StatBonuses result;const auto& entries=object(data);
    if(entries.size()>32) throw std::runtime_error("Equipment permits 32 stats per source");
    for(const auto& [id,value]:entries) {
        if(!validEquipmentSlot(id)) throw std::runtime_error("Invalid stat ID");
        const auto& bonus=object(value);if(bonus.size()!=2) throw std::runtime_error("Stat requires add and multiply");
        result[id]={number(bonus.at("add"),-1e6,1e6),number(bonus.at("multiply"),0,100)};
    }
    return result;
}
EquipmentState readEquipment(World& world,EntityId actor) {
    EquipmentState state;const auto& custom=world.custom(actor);const auto found=custom.find("core:item_instances");
    if(found==custom.end()) return state;
    if(found->second.version!=1) throw std::runtime_error("Unknown item instance version");
    const auto& root=object(found->second.data);const auto next=number(root.at("next"),1,9007199254740991.0);
    if(next!=std::floor(next)) throw std::runtime_error("Invalid item instance sequence");state.next=std::uint64_t(next);
    const auto& entries=object(root.at("items"));if(entries.size()>256) throw std::runtime_error("Item instance limit (256) exceeded");
    std::set<std::string> slots;
    for(const auto& [id,value]:entries) {
        // Monotonic IDs are exact in script Numbers and survive save/load.
        if(!id.starts_with("i") || id.size()>17) throw std::runtime_error("Invalid instance ID");
        std::uint64_t sequence=0;
        for(size_t i=1;i<id.size();++i) { if(id[i]<'0' || id[i]>'9') throw std::runtime_error("Invalid instance ID");sequence=sequence*10+unsigned(id[i]-'0'); }
        if(sequence==0 || sequence>=state.next || id!="i"+std::to_string(sequence)) throw std::runtime_error("Invalid instance sequence");
        const auto& row=object(value);ItemInstance item;item.item=std::get<std::string>(row.at("item").value);(void)AssetId(item.item);
        item.maximum=number(row.at("maximum"),1,1e9);item.durability=number(row.at("durability"),0,item.maximum);
        item.slot=std::get<std::string>(row.at("slot").value);
        if(!item.slot.empty() && (!validEquipmentSlot(item.slot) || !slots.insert(item.slot).second)) throw std::runtime_error("Invalid/duplicate instance slot");
        item.stats=readBonuses(row.at("stats"));const auto& affixes=object(row.at("affixes"));
        if(affixes.size()>8) throw std::runtime_error("Affix limit (8) exceeded");
        for(const auto& [affix,stats]:affixes) { (void)AssetId(affix);item.affixes[affix]=readBonuses(stats); }
        state.items.emplace(id,std::move(item));
    }
    if(slots.size()>32) throw std::runtime_error("Instance equipment slot limit (32) exceeded");return state;
}
CustomComponent equipmentData(const EquipmentState& state) {
    Data::Object items;
    for(const auto& [id,item]:state.items) {
        Data::Object affixes;for(const auto& [name,stats]:item.affixes) affixes[name]=bonusesData(stats);
        items[id]=Data{Data::Object{{"item",Data{item.item}},{"slot",Data{item.slot}},{"maximum",Data{item.maximum}},
            {"durability",Data{item.durability}},{"stats",bonusesData(item.stats)},{"affixes",Data{affixes}}}};
    }
    return {1,Data{Data::Object{{"next",Data{double(state.next)}},{"items",Data{items}}}}};
}
void writeEquipment(World& world,EntityId actor,const EquipmentState& state) { world.setCustom(actor,AssetId("core:item_instances"),equipmentData(state)); }
unsigned reservedItems(World& world,EntityId actor,const std::string& item) {
    unsigned reserved=0;
    for(const auto& [id,instance]:readEquipment(world,actor).items) { (void)id;if(instance.item==item) ++reserved; }
    if(const auto found=world.custom(actor).find("core:equipment");found!=world.custom(actor).end()) {
        if(found->second.version!=1) throw std::runtime_error("Unknown equipment slot version");
        for(const auto& [slot,value]:object(found->second.data)) { (void)slot;if(std::get<std::string>(value.value)==item) ++reserved; }
    }
    return reserved;
}
bool instanceSlotOccupied(World& world,EntityId actor,const std::string& slot) {
    for(const auto& [id,item]:readEquipment(world,actor).items) { (void)id;if(item.slot==slot) return true; }return false;
}
void validateEquipment(World& world,EntityId actor) {
    const auto instances=readEquipment(world,actor);std::map<std::string,unsigned> reserved;std::set<std::string> slots;
    for(const auto& [id,item]:instances.items) { (void)id;++reserved[item.item];if(!item.slot.empty()) slots.insert(item.slot); }
    if(const auto found=world.custom(actor).find("core:equipment");found!=world.custom(actor).end()) {
        if(found->second.version!=1) throw std::runtime_error("Unknown equipment slot version");
        for(const auto& [slot,value]:object(found->second.data)) {
            if(!validEquipmentSlot(slot) || !slots.insert(slot).second) throw std::runtime_error("Invalid/duplicate equipment slot");
            const auto item=std::get<std::string>(value.value);(void)AssetId(item);++reserved[item];
        }
    }
    if(slots.size()>32) throw std::runtime_error("Equipment slot limit (32) exceeded");
    for(const auto& [item,count]:reserved) {
        if(!world.has<InventoryComponent>(actor)) throw std::runtime_error("Equipment inventory missing");
        const auto& inventory=world.get<InventoryComponent>(actor).items;const auto found=inventory.find(item);
        if(found==inventory.end() || found->second<count) throw std::runtime_error("Equipment exceeds owned inventory copies");
    }
}
double equipmentStat(World& world,EntityId actor,const std::string& stat,double base,bool useAttributeBase) {
    if(!validEquipmentSlot(stat) || !std::isfinite(base) || std::abs(base)>1e12) throw std::runtime_error("Invalid base attribute");
    if(useAttributeBase) base=attributeBase(world,actor,stat,base);double add=0,multiply=1;
    const auto combine=[&](const StatBonuses& bonuses) { if(auto it=bonuses.find(stat);it!=bonuses.end()) { add+=it->second.add;multiply*=it->second.multiply; } };
    for(const auto& [id,item]:readEquipment(world,actor).items) if(!item.slot.empty() && item.durability>0) {
        (void)id;combine(item.stats);for(const auto& [affix,bonuses]:item.affixes) { (void)affix;combine(bonuses); }
    }
    const double result=(base+add)*multiply;
    if(!std::isfinite(result) || std::abs(result)>1e12) throw std::runtime_error("Equipment attribute overflow");return result;
}
}
