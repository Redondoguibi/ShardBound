#pragma once
#include <map>
#include <string>
#include <variant>
#include <vector>
namespace game {
// Value-owned, serializable data; no engine handles, functions or pointers.
struct Data {
    using Array=std::vector<Data>;
    using Object=std::map<std::string,Data>;
    std::variant<std::monostate,bool,double,std::string,Array,Object> value;
    bool operator==(const Data&) const = default;
};
std::string encodeData(const Data& data);
Data decodeData(const std::string& json);
}
