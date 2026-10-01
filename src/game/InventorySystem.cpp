#include "game/InventorySystem.h"
#include "core/Diagnostic.h"
#include <limits>
namespace game {
void giveItem(World& world,EntityId entity,const AssetId& item,std::uint32_t count) {
    if(!count) throw DiagnosticError({"GAME102","Item count must be positive"});
    if(!world.has<InventoryComponent>(entity)) world.add(entity,InventoryComponent{});
    auto& items=world.get<InventoryComponent>(entity).items;
    const auto found=items.find(item.str()); const auto old=found==items.end() ? 0 : found->second;
    if(count>std::numeric_limits<std::uint32_t>::max()-old) throw DiagnosticError({"GAME102","Inventory count overflow"});
    items.insert_or_assign(item.str(),old+count);
}
bool takeItem(World& world,EntityId entity,const AssetId& item,std::uint32_t count) {
    if(!count) throw DiagnosticError({"GAME102","Item count must be positive"});
    if(!world.has<InventoryComponent>(entity)) return false;
    auto& items=world.get<InventoryComponent>(entity).items; const auto found=items.find(item.str());
    if(found==items.end() || found->second<count) return false;
    found->second-=count; if(!found->second) items.erase(found); return true;
}
std::uint32_t itemCount(World& world,EntityId entity,const AssetId& item) {
    if(!world.has<InventoryComponent>(entity)) return 0;
    const auto& items=world.get<InventoryComponent>(entity).items; const auto found=items.find(item.str());
    return found==items.end() ? 0 : found->second;
}
}
