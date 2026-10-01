#pragma once
#include "scripting/Value.h"
#include <set>
namespace game::script {
struct Definition { std::string id,category,type; Value data; };
class Registry {
public:
    void add(Definition definition,const Location& at);
    const Definition& get(const std::string& category,const std::string& id,const Location& at={}) const;
    void patch(const std::string& category,const std::string& id,const Value& changes,const std::string& mod,const Location& at);
    void dependencies(std::string mod,std::vector<std::string> ids);
    static Value merge(const Value& base,const Value& changes);
private:
    std::map<std::string,Definition> definitions_;
    struct Write { std::string mod; Value value; };
    std::map<std::string,Write> writes_;
    std::map<std::string,std::vector<std::string>> dependencies_;
    bool depends(const std::string& mod,const std::string& dependency) const;
};
}
