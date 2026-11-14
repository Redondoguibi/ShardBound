#include "game/Attributes.h"
#include <cmath>
namespace game {
AttributeBases readAttributeBases(World& world,EntityId actor) {
    const auto& custom=world.custom(actor);const auto found=custom.find("core:attributes");if(found==custom.end()) return {};
    if(found->second.version!=1) throw std::runtime_error("Unknown attribute data version");
    const auto& rows=std::get<Data::Object>(found->second.data.value);if(rows.size()>64) throw std::runtime_error("Attribute override limit (64) exceeded");
    AttributeBases result;
    for(const auto& [id,value]:rows) {
        (void)AssetId(id);if(id.size()>64) throw std::runtime_error("Attribute ID exceeds 64 bytes");
        const auto n=std::get<double>(value.value);if(!std::isfinite(n) || std::abs(n)>1e12) throw std::runtime_error("Invalid attribute base");result[id]=n;
    }
    return result;
}
double attributeBase(World& world,EntityId actor,const std::string& id,double fallback) {
    if(!world.custom(actor).contains("core:attributes")) return fallback;
    const auto values=readAttributeBases(world,actor);const auto found=values.find(id);return found==values.end()?fallback:found->second;
}
void writeAttributeBase(World& world,EntityId actor,const std::string& id,std::optional<double> value) {
    (void)AssetId(id);if(id.size()>64 || (value && (!std::isfinite(*value) || std::abs(*value)>1e12))) throw std::runtime_error("Invalid attribute override");
    auto values=readAttributeBases(world,actor);if(value) values[id]=*value;else values.erase(id);
    if(values.size()>64) throw std::runtime_error("Attribute override limit (64) exceeded");
    Data::Object rows;for(const auto& [key,n]:values) rows[key]=Data{n};
    if(rows.empty()) world.removeCustom(actor,AssetId("core:attributes"));else world.setCustom(actor,AssetId("core:attributes"),{1,Data{std::move(rows)}});
}
}
