#include "scripting/Registry.h"
#include "assets/AssetId.h"
namespace game::script {
std::vector<Definition> Registry::entries(const std::string& category) const {
    std::vector<Definition> result;
    for(const auto& [key,definition]:definitions_) if(definition.category==category)
        result.push_back({definition.id,definition.category,definition.type,clone(definition.data)});
    return result;
}
bool Registry::contains(const std::string& category,const std::string& id) const { return definitions_.contains(category+"/"+id); }
void Registry::add(Definition definition,const Location& at) {
    (void)AssetId(definition.id);
    const auto key=definition.category+"/"+definition.id;
    if(!definitions_.emplace(key,std::move(definition)).second) fail(at,"SCRIPT401","Duplicate definition: "+key);
}
const Definition& Registry::get(const std::string& category,const std::string& id,const Location& at) const {
    auto it=definitions_.find(category+"/"+id); if(it==definitions_.end()) fail(at,"SCRIPT402","Unknown definition: "+category+"/"+id); return it->second;
}
Value Registry::merge(const Value& base,const Value& changes) {
    auto lhs=std::get_if<std::shared_ptr<Object>>(&base.data),rhs=std::get_if<std::shared_ptr<Object>>(&changes.data);
    if(!lhs || !rhs) return clone(changes);
    auto output=clone(base); auto& fields=std::get<std::shared_ptr<Object>>(output.data)->fields;
    for(const auto& [key,value]:(*rhs)->fields) {
        auto it=fields.find(key); fields[key]=it==fields.end() ? clone(value) : merge(it->second,value);
    }
    return output;
}
void Registry::dependencies(std::string mod,std::vector<std::string> ids) { dependencies_[std::move(mod)]=std::move(ids); }
bool Registry::depends(const std::string& mod,const std::string& dependency) const {
    std::set<std::string> seen; std::vector<std::string> pending{mod};
    while(!pending.empty()) {
        auto current=pending.back(); pending.pop_back(); if(!seen.insert(current).second) continue;
        auto it=dependencies_.find(current); if(it==dependencies_.end()) continue;
        for(const auto& id:it->second) { if(id==dependency) return true; pending.push_back(id); }
    }
    return false;
}
void Registry::patch(const std::string& category,const std::string& id,const Value& changes,const std::string& mod,const Location& at) {
    const auto& definition=get(category,id,at);
    std::vector<std::pair<std::string,Value>> fields;
    std::function<void(const Value&,std::string)> flatten=[&](const Value& value,std::string path) {
        if(auto p=std::get_if<std::shared_ptr<Object>>(&value.data)) {
            for(const auto& [key,child]:(*p)->fields) flatten(child,path+"/"+key);
        } else fields.emplace_back(std::move(path),value);
    };
    const auto prefix=category+"/"+id; flatten(changes,prefix);
    for(const auto& [path,value]:fields) {
        for(const auto& [written,writers]:writes_) for(const auto& write:writers) {
            const bool overlap=path==written || path.starts_with(written+"/") || written.starts_with(path+"/");
            if(overlap && write.mod!=mod && !depends(mod,write.mod) && (path!=written || !equal(value,write.value))) fail(at,"SCRIPT403","Conflicting modify by "+mod+" and "+write.mod+": "+path);
        }
    }
    auto updated=merge(definition.data,changes);
    for(const auto& [path,value]:fields) {
        auto& writers=writes_[path]; std::erase_if(writers,[&](const auto& write) { return write.mod==mod; });
        writers.push_back(Write{mod,clone(value)});
    }
    definitions_.at(prefix).data=std::move(updated);
}
void Registry::replace(const std::string& category,const std::string& id,Value value,const std::string& mod,const Location& at) {
    (void)get(category,id,at); const auto prefix=category+"/"+id;
    if(!std::holds_alternative<std::shared_ptr<Object>>(value.data)) fail(at,"SCRIPT404","Replacement must be an object");
    for(const auto& [path,writers]:writes_) if(path==prefix || path.starts_with(prefix+"/"))
        for(const auto& write:writers) if(write.mod!=mod && !depends(mod,write.mod)) fail(at,"SCRIPT403","Conflicting replacement by "+mod+" and "+write.mod);
    auto copied=clone(value);
    writes_[prefix].push_back({mod,clone(copied)});
    definitions_.at(prefix).data=std::move(copied);
}
}
