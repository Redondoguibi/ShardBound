#include "scripting/DataConversion.h"
namespace game::script {
namespace {
Data convert(const Value& value,unsigned depth) {
    if(depth>64) fail({},"EXT110","Cyclic or excessively nested data");
    return std::visit([&](const auto& v)->Data {
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,std::monostate>) return {};
        else if constexpr(std::is_same_v<T,bool> || std::is_same_v<T,double> || std::is_same_v<T,std::string>) return Data{v};
        else if constexpr(std::is_same_v<T,Symbol>) return Data{v.name};
        else if constexpr(std::is_same_v<T,std::shared_ptr<List>>) { Data::Array result; for(const auto& x:v->values) result.push_back(convert(x,depth+1)); return Data{std::move(result)}; }
        else if constexpr(std::is_same_v<T,std::shared_ptr<Object>>) { Data::Object result; for(const auto& [k,x]:v->fields) result[k]=convert(x,depth+1); return Data{std::move(result)}; }
        else { fail({},"EXT110","Functions and entity/host references cannot be serialized"); }
    },value.data);
}
}
Data toData(const Value& value) { auto result=convert(value,0); (void)encodeData(result); return result; }
Value fromData(const Data& data) {
    return std::visit([&](const auto& value)->Value {
        using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,std::monostate>) return {};
        else if constexpr(std::is_same_v<T,Data::Array>) { std::vector<Value> result; for(const auto& v:value) result.push_back(fromData(v)); return list(std::move(result)); }
        else if constexpr(std::is_same_v<T,Data::Object>) { auto result=object(); for(const auto& [k,v]:value) result.setMember(k,fromData(v),{}); return result; }
        else return Value(value);
    },data.value);
}
}
