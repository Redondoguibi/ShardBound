#pragma once
#include "scripting/Value.h"
#include <set>
#include <cstdint>
namespace game::script {
struct Definition { std::string id,category,type; Value data; };
class Registry {
public:
    void add(Definition definition,const Location& at);
    const Definition& get(const std::string& category,const std::string& id,const Location& at={}) const;
    std::vector<Definition> entries(const std::string& category) const;
    bool contains(const std::string& category,const std::string& id) const;
    std::uint64_t revision() const { return revision_; }
    void patch(const std::string& category,const std::string& id,const Value& changes,const std::string& mod,const Location& at);
    void dependencies(std::string mod,std::vector<std::string> ids);
    static Value merge(const Value& base,const Value& changes);
    void replace(const std::string& category,const std::string& id,Value value,const std::string& mod,const Location& at);
private:
    std::map<std::string,Definition> definitions_;
    std::uint64_t revision_=0;
    struct Write { std::string mod; Value value; };
    std::map<std::string,std::vector<Write>> writes_;
    std::map<std::string,std::vector<std::string>> dependencies_;
    bool depends(const std::string& mod,const std::string& dependency) const;
};
}
