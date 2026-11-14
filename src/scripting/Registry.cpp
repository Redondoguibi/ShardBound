#include "scripting/Registry.h"
#include "assets/AssetId.h"
namespace game::script {
Value Registry::inspect() const {
    std::vector<Value> definitions,writes;
    for(const auto& [key,d]:definitions_) { auto row=object();row.setMember("id",Value(d.id),{});row.setMember("category",Value(d.category),{});row.setMember("type",Value(d.type),{});const auto found=sources_.find(key);if(found!=sources_.end()) { row.setMember("file",Value(found->second.file),{});row.setMember("line",Value(double(found->second.line)),{});row.setMember("column",Value(double(found->second.column)),{}); }definitions.push_back(row); }
    for(const auto& [path,writers]:writes_) for(const auto& writer:writers) { auto row=object();row.setMember("path",Value(path),{});row.setMember("mod",Value(writer.mod),{});row.setMember("file",Value(writer.at.file),{});row.setMember("line",Value(double(writer.at.line)),{});row.setMember("column",Value(double(writer.at.column)),{});writes.push_back(row); }
    auto result=object();result.setMember("revision",Value(double(revision_)),{});result.setMember("definitions",list(std::move(definitions)),{});result.setMember("writes",list(std::move(writes)),{});return result;
}
std::vector<Definition> Registry::entries(const std::string& category) const {
    std::vector<Definition> result;
    for(const auto& [key,definition]:definitions_) if(definition.category==category)
        result.push_back({definition.id,definition.category,definition.type,clone(definition.data)});
    return result;
}
bool Registry::contains(const std::string& category,const std::string& id) const { return definitions_.contains(category+"/"+id); }
std::vector<std::string> Registry::ids(const std::string& category) const {
    std::vector<std::string> result;const auto prefix=category+"/";
    for(auto it=definitions_.lower_bound(prefix);it!=definitions_.end() && it->first.starts_with(prefix);++it) result.push_back(it->second.id);return result;
}
void Registry::add(Definition definition,const Location& at) {
    (void)AssetId(definition.id);
    const auto key=definition.category+"/"+definition.id;
    if(!definitions_.emplace(key,std::move(definition)).second) fail(at,"SCRIPT401","Duplicate definition: "+key);
    sources_[key]=at;++revision_;
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
        writers.push_back(Write{mod,clone(value),at});
    }
    definitions_.at(prefix).data=std::move(updated);
    ++revision_;
}
void Registry::replace(const std::string& category,const std::string& id,Value value,const std::string& mod,const Location& at) {
    (void)get(category,id,at); const auto prefix=category+"/"+id;
    if(!std::holds_alternative<std::shared_ptr<Object>>(value.data)) fail(at,"SCRIPT404","Replacement must be an object");
    for(const auto& [path,writers]:writes_) if(path==prefix || path.starts_with(prefix+"/"))
        for(const auto& write:writers) if(write.mod!=mod && !depends(mod,write.mod)) fail(at,"SCRIPT403","Conflicting replacement by "+mod+" and "+write.mod);
    auto copied=clone(value);
    auto& writers=writes_[prefix];std::erase_if(writers,[&](const auto& writer){return writer.mod==mod;});writers.push_back({mod,clone(copied),at});
    definitions_.at(prefix).data=std::move(copied);
    ++revision_;
}
}
