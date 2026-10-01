#include "core/Data.h"
#include "core/Diagnostic.h"
#include <nlohmann/json.hpp>
#include <cmath>
namespace game {
namespace {
using Json=nlohmann::json;
Json encode(const Data& data,unsigned depth) {
    if(depth>64) throw DiagnosticError({"DATA101","Data nesting exceeds 64"});
    return std::visit([&](const auto& value)->Json {
        using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,std::monostate>) return nullptr;
        else if constexpr(std::is_same_v<T,Data::Array>) { auto result=Json::array(); for(const auto& v:value) result.push_back(encode(v,depth+1)); return result; }
        else if constexpr(std::is_same_v<T,Data::Object>) { auto result=Json::object(); for(const auto& [k,v]:value) result[k]=encode(v,depth+1); return result; }
        else { if constexpr(std::is_same_v<T,double>) if(!std::isfinite(value)) throw DiagnosticError({"DATA102","Non-finite data number"}); return value; }
    },data.value);
}
Data decode(const Json& json,unsigned depth) {
    if(depth>64) throw DiagnosticError({"DATA101","Data nesting exceeds 64"});
    if(json.is_null()) return {};
    if(json.is_boolean()) return Data{json.get<bool>()};
    if(json.is_number()) { auto n=json.get<double>(); if(!std::isfinite(n)) throw DiagnosticError({"DATA102","Non-finite data number"}); return Data{n}; }
    if(json.is_string()) return Data{json.get<std::string>()};
    if(json.is_array()) { Data::Array result; for(const auto& v:json) result.push_back(decode(v,depth+1)); return Data{std::move(result)}; }
    Data::Object result; for(const auto& [key,v]:json.items()) result.emplace(key,decode(v,depth+1)); return Data{std::move(result)};
}
}
std::string encodeData(const Data& data) {
    auto text=encode(data,0).dump();
    if(text.size()>16*1024*1024) throw DiagnosticError({"DATA103","Data exceeds 16 MiB"});
    return text;
}
Data decodeData(const std::string& text) {
    if(text.size()>16*1024*1024) throw DiagnosticError({"DATA103","Data exceeds 16 MiB"});
    try {
        auto json=Json::parse(text,[](int depth,Json::parse_event_t,Json&) {
            if(depth>64) throw DiagnosticError({"DATA101","Data nesting exceeds 64"}); return true;
        });
        return decode(json,0);
    } catch(const DiagnosticError&) { throw; }
    catch(const std::exception& e) { throw DiagnosticError({"DATA104",e.what()}); }
}
}
