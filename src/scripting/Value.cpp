#include "scripting/Value.h"
#include <set>
namespace game::script {
double Value::number(const Location& at) const { if(auto p=std::get_if<double>(&data)) return *p; fail(at,"SCRIPT301","Expected Number"); }
bool Value::boolean(const Location& at) const { if(auto p=std::get_if<bool>(&data)) return *p; fail(at,"SCRIPT301","Expected Boolean"); }
std::string Value::string(const Location& at) const { if(auto p=std::get_if<std::string>(&data)) return *p; fail(at,"SCRIPT301","Expected String"); }
Value Value::member(const std::string& key,const Location& at) const {
    if(auto p=std::get_if<std::shared_ptr<HostObject>>(&data)) return (*p)->get(key,at);
    if(auto p=std::get_if<std::shared_ptr<Object>>(&data)) {
        auto it=(*p)->fields.find(key); if(it!=(*p)->fields.end()) return it->second;
    }
    fail(at,"SCRIPT302","Unknown member: "+key);
}
void Value::setMember(const std::string& key,Value value,const Location& at) {
    if(auto p=std::get_if<std::shared_ptr<HostObject>>(&data)) { (*p)->set(key,std::move(value),at); return; }
    if(auto p=std::get_if<std::shared_ptr<Object>>(&data)) {
        std::vector<Value> pending{value}; std::set<const void*> seen;
        while(!pending.empty()) {
            auto current=std::move(pending.back()); pending.pop_back();
            if(auto object=std::get_if<std::shared_ptr<Object>>(&current.data)) {
                if(object->get()==p->get()) fail(at,"SCRIPT324","Object assignment would create a cycle");
                if(seen.insert(object->get()).second) for(const auto& [name,child]:(*object)->fields) { (void)name; pending.push_back(child); }
            } else if(auto sequence=std::get_if<std::shared_ptr<List>>(&current.data)) {
                if(seen.insert(sequence->get()).second) for(const auto& child:(*sequence)->values) pending.push_back(child);
            }
        }
        (*p)->fields.insert_or_assign(key,std::move(value)); return;
    }
    fail(at,"SCRIPT301","Member assignment requires Object or EntityRef");
}
Value native(std::function<Value(const std::vector<Value>&,const Location&)> call) { return Value(std::make_shared<Callable>(std::move(call))); }
Value list(std::vector<Value> values) { return Value(std::make_shared<List>(std::move(values))); }
Value object() { return Value(std::make_shared<Object>()); }
Value clone(const Value& value) {
    if(auto p=std::get_if<std::shared_ptr<Object>>(&value.data)) {
        auto output=std::make_shared<Object>(); for(const auto& [name,v]:(*p)->fields) output->fields[name]=clone(v); return Value(output);
    }
    if(auto p=std::get_if<std::shared_ptr<List>>(&value.data)) {
        std::vector<Value> output; for(const auto& v:(*p)->values) output.push_back(clone(v)); return list(std::move(output));
    }
    return value;
}
bool equal(const Value& a,const Value& b) {
    if(a.data.index()!=b.data.index()) return false;
    if(auto p=std::get_if<std::shared_ptr<HostObject>>(&a.data)) return (*p)->equals(*std::get<std::shared_ptr<HostObject>>(b.data));
    if(auto p=std::get_if<std::shared_ptr<Object>>(&a.data)) {
        const auto& rhs=std::get<std::shared_ptr<Object>>(b.data)->fields;
        if((*p)->fields.size()!=rhs.size()) return false;
        for(const auto& [name,v]:(*p)->fields) { auto it=rhs.find(name); if(it==rhs.end() || !equal(v,it->second)) return false; } return true;
    }
    if(auto p=std::get_if<std::shared_ptr<List>>(&a.data)) {
        const auto& rhs=std::get<std::shared_ptr<List>>(b.data)->values;
        if((*p)->values.size()!=rhs.size()) return false;
        for(size_t i=0;i<rhs.size();++i) if(!equal((*p)->values[i],rhs[i])) return false; return true;
    }
    return a.data==b.data;
}
}
