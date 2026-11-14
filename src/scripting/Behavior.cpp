#include "scripting/Behavior.h"
#include <cmath>
#include <set>
namespace game::script {
namespace {
const std::map<std::string,Value>& fields(const Value& v,const Location& at) {
    const auto p=std::get_if<std::shared_ptr<Object>>(&v.data);
    if(!p) fail(at,"AI101","Expected behavior record");return (*p)->fields;
}
struct BehaviorNode { std::string kind;Value action;std::vector<BehaviorNode> children;bool reactive=false; };
BehaviorNode parseTree(const Value& v,size_t& count,unsigned depth,const Location& at) {
    if(++count>128 || depth>16) fail(at,"AI101","Behavior limit: 128 nodes / 16 levels");
    const auto& f=fields(v,at);BehaviorNode n;n.kind=v.member("kind",at).string(at);
    if(n.kind=="action") {
        n.action=v.member("run",at);
        if(!std::holds_alternative<std::shared_ptr<Callable>>(n.action.data)) fail(at,"AI101","Action run must be a function");
    } else if(n.kind=="sequence" || n.kind=="selector" || n.kind=="invert") {
        const auto children=v.member("children",at);const auto list=std::get_if<std::shared_ptr<List>>(&children.data);
        if(!list || (*list)->values.empty() || (*list)->values.size()>128 || (n.kind=="invert" && (*list)->values.size()!=1)) fail(at,"AI101","Invalid behavior children");
        if(const auto it=f.find("reactive");it!=f.end()) n.reactive=it->second.boolean(at);
        for(const auto& child:(*list)->values) n.children.push_back(parseTree(child,count,depth+1,at));
    } else fail(at,"AI101","Unknown behavior node: "+n.kind);
    return n;
}
}
void bindBehaviorApi(Runtime& runtime) {
    runtime.bind("behavior_tick",native([&runtime](const auto& a,const auto& at) {
        if(a.size()!=3) fail(at,"AI101","behavior_tick(tree, context, memory)");
        size_t count=0;const auto tree=parseTree(a[0],count,0,at);
        const auto savedMemory=clone(a[2]);const auto& previous=fields(savedMemory,at);if(previous.size()>128) fail(at,"AI101","Behavior memory limit exceeded");
        for(const auto& [key,value]:previous) {
            const double n=value.number(at);
            if(key.size()>128 || !std::isfinite(n) || n<0 || n>127 || n!=std::floor(n)) fail(at,"AI101","Invalid behavior cursor");
        }
        size_t recognized=0;
        std::function<void(const BehaviorNode&,const std::string&)> validateMemory=[&](const auto& node,const auto& path) {
            if(const auto it=previous.find(path);it!=previous.end()) {
                if((node.kind!="sequence" && node.kind!="selector") || size_t(it->second.number(at))>=node.children.size()) fail(at,"AI101","Cursor does not match behavior tree");
                ++recognized;
            }
            for(size_t i=0;i<node.children.size();++i) validateMemory(node.children[i],path+"."+std::to_string(i));
        };
        validateMemory(tree,"root");if(recognized!=previous.size()) fail(at,"AI101","Memory contains unknown behavior paths");
        auto memory=object();
        std::function<std::string(const BehaviorNode&,const std::string&)> tick=[&](const BehaviorNode& node,const std::string& path) -> std::string {
            if(node.kind=="action") {
                const auto status=runtime.invoke(node.action,{a[1]}).string(at);
                if(status!="success" && status!="failure" && status!="running") fail(at,"AI102","Action must return success, failure or running");
                return status;
            }
            if(node.kind=="invert") { const auto status=tick(node.children[0],path+".0");return status=="running"?status:status=="success"?"failure":"success"; }
            size_t cursor=0;
            if(!node.reactive) if(const auto it=previous.find(path);it!=previous.end()) {
                cursor=size_t(it->second.number(at));if(cursor>=node.children.size()) fail(at,"AI101","Cursor does not match behavior tree");
            }
            for(;cursor<node.children.size();++cursor) {
                const auto status=tick(node.children[cursor],path+"."+std::to_string(cursor));
                if(status=="running") { if(!node.reactive) memory.setMember(path,Value(double(cursor)),at);return status; }
                if(node.kind=="sequence" && status=="failure") return status;
                if(node.kind=="selector" && status=="success") return status;
            }
            return node.kind=="sequence"?"success":"failure";
        };
        const auto status=tick(tree,"root");auto result=object();result.setMember("status",Value(status),at);
        result.setMember("memory",status=="running"?memory:object(),at);return result;
    }));
    // A state chart supports explicit transitions, enter/exit, and mod-authored
    // phase selection without engine-specific enemy types or health thresholds.
    runtime.bind("state_tick",native([&runtime](const auto& a,const auto& at) {
        if(a.size()!=3) fail(at,"AI101","state_tick(states, current_state, context)");
        const auto& states=fields(a[0],at);const auto current=a[1].string(at);
        if(states.empty() || states.size()>64 || !states.contains(current)) fail(at,"AI101","Invalid state chart/current state");
        for(const auto& [id,state]:states) {
            if(id.empty() || id.size()>128) fail(at,"AI101","Invalid state identifier");
            for(const auto& [key,callback]:fields(state,at)) {
                if((key!="tick" && key!="enter" && key!="exit") || !std::holds_alternative<std::shared_ptr<Callable>>(callback.data)) fail(at,"AI101","Invalid state callback");
            }
            (void)state.member("tick",at);
        }
        // Snapshot callbacks so a transition cannot change its own validation.
        const auto snapshot=clone(a[0]);const auto state=snapshot.member(current,at);
        const auto result=runtime.invoke(state.member("tick",at),{a[2]});
        if(std::holds_alternative<std::monostate>(result.data)) return Value(current);
        const auto next=result.string(at);
        if(!fields(snapshot,at).contains(next)) fail(at,"AI102","State tick returned an unknown target");
        if(next!=current) {
            const auto& old=fields(state,at);if(old.contains("exit")) runtime.invoke(old.at("exit"),{a[2]});
            const auto& target=fields(snapshot.member(next,at),at);if(target.contains("enter")) runtime.invoke(target.at("enter"),{a[2]});
        }
        return Value(next);
    }));
}
}
