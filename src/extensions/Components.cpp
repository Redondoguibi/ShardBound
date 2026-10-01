#include "extensions/Components.h"
#include "core/Diagnostic.h"
namespace game {
const Components::Type& Components::require(const AssetId& id) const {
    const auto it=types_.find(id.str());
    if(it==types_.end()) throw DiagnosticError({"EXT101","Unregistered component",{},id.nameSpace(),id.str()});
    return it->second;
}
bool Components::known(const AssetId& id) const { return types_.contains(id.str()); }
void Components::define(const AssetId& id,unsigned version,Data defaults) {
    if(!version || !std::holds_alternative<Data::Object>(defaults.value)) throw DiagnosticError({"EXT102","Component requires positive version and object defaults"});
    (void)encodeData(defaults);
    if(!types_.emplace(id.str(),Type{version,std::move(defaults),{}}).second) throw DiagnosticError({"EXT103","Duplicate component type: "+id.str()});
}
void Components::migration(const AssetId& id,unsigned from,std::function<Data(const Data&)> migrate) {
    const auto& type=require(id);
    if(!from || from>=type.version || !migrate) throw DiagnosticError({"EXT104","Invalid migration version"});
    if(!types_.at(id.str()).migrations.emplace(from,std::move(migrate)).second) throw DiagnosticError({"EXT104","Duplicate migration"});
}
void Components::add(World& world,EntityId entity,const AssetId& id) const {
    const auto& type=require(id);
    if(world.custom(entity).contains(id.str())) throw DiagnosticError({"EXT105","Component already attached"});
    world.setCustom(entity,id,{type.version,type.defaults});
}
Data Components::get(World& world,EntityId entity,const AssetId& id) const {
    const auto& type=require(id); const auto& values=world.custom(entity); const auto it=values.find(id.str());
    if(it==values.end()) throw DiagnosticError({"EXT106","Component missing"});
    auto value=it->second;
    if(value.version>type.version) throw DiagnosticError({"EXT107","Component was saved by a newer version; data retained"});
    while(value.version<type.version) {
        const auto step=type.migrations.find(value.version);
        if(step==type.migrations.end()) throw DiagnosticError({"EXT108","Missing component migration; data retained"});
        value.data=step->second(value.data);
        if(!std::holds_alternative<Data::Object>(value.data.value)) throw DiagnosticError({"EXT109","Migration must return an object"});
        (void)encodeData(value.data); ++value.version;
    }
    world.setCustom(entity,id,value); return value.data;
}
void Components::set(World& world,EntityId entity,const AssetId& id,Data data) const {
    const auto& type=require(id);
    if(!std::holds_alternative<Data::Object>(data.value)) throw DiagnosticError({"EXT109","Component data must be an object"});
    (void)encodeData(data);
    (void)get(world,entity,id); world.setCustom(entity,id,{type.version,std::move(data)});
}
}
