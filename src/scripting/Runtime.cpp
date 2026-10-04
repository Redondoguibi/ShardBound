#include "scripting/Runtime.h"
#include "assets/VirtualFileSystem.h"
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
namespace game::script {
namespace {
struct Binding { Value value; bool constant=false; };
struct Environment {
    std::map<std::string,Binding> bindings;
    std::shared_ptr<Environment> parent;
    Binding* find(const std::string& name) { auto it=bindings.find(name); return it!=bindings.end() ? &it->second : parent ? parent->find(name) : nullptr; }
    void assign(const std::string& name,Value value,const Location& at,bool constant=false) {
        if(auto existing=find(name)) {
            if(existing->constant || constant) fail(at,"SCRIPT303","Cannot reassign constant: "+name);
            existing->value=std::move(value);
        } else bindings.emplace(name,Binding{std::move(value),constant});
    }
};
struct Context { std::string mod,category; std::filesystem::path root; std::shared_ptr<Environment> globals; };
struct Returned { Value value; };
struct ActiveMod {
    std::string& target; std::string old;
    ActiveMod(std::string& target,const std::string& mod) : target(target),old(target) { target=mod; }
    ~ActiveMod() { target=std::move(old); }
};
std::string reference(const Value& value,const Location& at) {
    if(auto symbol=std::get_if<Symbol>(&value.data)) return symbol->name;
    return value.string(at);
}
}
struct Runtime::Impl {
    Registry registry;
    std::shared_ptr<Environment> builtins=std::make_shared<Environment>();
    std::map<std::string,Context> contexts;
    std::vector<std::weak_ptr<Environment>> scopes;
    std::map<std::string,int> imports;
    struct Handler { std::uint64_t token; std::string owner; Value function; };
    std::map<std::string,std::vector<Handler>> events,hooks;
    std::uint64_t nextToken=1;
    std::string activeMod;
    unsigned dispatchDepth=0;
    unsigned calls=0;
    unsigned importDepth=0;
    size_t budget=100000;
    ~Impl() { for(auto& weak:scopes) if(auto env=weak.lock()) env->bindings.clear(); }
    std::shared_ptr<Environment> scope(std::shared_ptr<Environment> parent) {
        if(scopes.size()>1024) std::erase_if(scopes,[](const auto& weak) { return weak.expired(); });
        auto env=std::make_shared<Environment>(); env->parent=std::move(parent); scopes.push_back(env); return env;
    }
    Context& context(const std::string& mod) {
        auto [it,created]=contexts.try_emplace(mod);
        if(created) { it->second.mod=mod; it->second.globals=scope(builtins); }
        return it->second;
    }
    void tick(const Location& at) { if(!budget) fail(at,"SCRIPT304","Execution instruction budget exceeded"); --budget; }
    Value eval(const NodePtr& n,std::shared_ptr<Environment> env,Context& ctx) {
        tick(n->at);
        switch(n->kind) {
        case Kind::Number: { double number=std::stod(n->text); if(!std::isfinite(number)) fail(n->at,"SCRIPT301","Number is not finite"); return Value(number); }
        case Kind::String: return Value(n->text);
        case Kind::Boolean: return Value(n->text=="true");
        case Kind::Null: return {};
        case Kind::Identifier: {
            if(auto value=env->find(n->text)) return value->value;
            fail(n->at,"SCRIPT102","Unknown symbol: "+n->text);
        }
        case Kind::List: { std::vector<Value> values; for(const auto& v:n->children) values.push_back(eval(v,env,ctx)); return list(std::move(values)); }
        case Kind::Member: return eval(n->children[0],env,ctx).member(n->text,n->at);
        case Kind::Call: {
            auto function=eval(n->children[0],env,ctx);
            auto callable=std::get_if<std::shared_ptr<Callable>>(&function.data); if(!callable) fail(n->at,"SCRIPT301","Value is not callable");
            std::vector<Value> args; for(size_t i=1;i<n->children.size();++i) args.push_back(eval(n->children[i],env,ctx));
            return (*callable)->call(args,n->at);
        }
        case Kind::Unary: {
            auto value=eval(n->children[0],env,ctx);
            if(n->text=="not") return Value(!value.boolean(n->at));
            return Value((n->text=="-" ? -1 : 1)*value.number(n->at));
        }
        case Kind::Binary: {
            auto left=eval(n->children[0],env,ctx);
            if(n->text=="and" && !left.boolean(n->at)) return Value(false);
            if(n->text=="or" && left.boolean(n->at)) return Value(true);
            auto right=eval(n->children[1],env,ctx);
            if(n->text=="and" || n->text=="or") return Value(right.boolean(n->at));
            if(n->text=="==") return Value(equal(left,right)); if(n->text=="!=") return Value(!equal(left,right));
            if(n->text=="+" && std::holds_alternative<std::string>(left.data) && std::holds_alternative<std::string>(right.data)) return Value(left.string()+right.string());
            const auto a=left.number(n->at),b=right.number(n->at);
            if(n->text=="<") return Value(a<b); if(n->text=="<=") return Value(a<=b);
            if(n->text==">") return Value(a>b); if(n->text==">=") return Value(a>=b);
            if((n->text=="/" || n->text=="%") && b==0) fail(n->at,"SCRIPT305","Division by zero");
            double result=n->text=="+" ? a+b : n->text=="-" ? a-b : n->text=="*" ? a*b : n->text=="/" ? a/b : std::fmod(a,b);
            if(!std::isfinite(result)) fail(n->at,"SCRIPT305","Numeric overflow"); return Value(result);
        }
        default: fail(n->at,"SCRIPT306","Invalid expression node");
        }
    }
    Value data(const Block& nodes,std::shared_ptr<Environment> env,Context& ctx) {
        auto result=object(); auto& fields=std::get<std::shared_ptr<Object>>(result.data)->fields;
        for(const auto& n:nodes) {
            if(n->kind!=Kind::Section && n->kind!=Kind::Property) fail(n->at,"SCRIPT307","Only sections and properties are valid in declarations");
            if(fields.contains(n->text)) fail(n->at,"SCRIPT307","Duplicate property: "+n->text);
            fields[n->text]=n->kind==Kind::Section ? data(n->body,env,ctx) : n->children.empty() ? Value(true) : eval(n->children[0],env,ctx);
        }
        return result;
    }
    void importFile(Context& ctx,const std::string& entry) {
        const auto path=VirtualFileSystem::contained(ctx.root,entry);
        if(path.extension()!=".sl") fail({path.string()},"SCRIPT308","Imports must use .sl");
        const auto key=ctx.mod+":"+path.string();
        if(imports[key]==1) fail({path.string()},"SCRIPT309","Circular import");
        if(imports[key]==2) return;
        if(importDepth>=64) fail({path.string()},"SCRIPT321","Import nesting limit exceeded");
        ++importDepth; struct ImportDepth { unsigned& count; ~ImportDepth() { --count; } } depth{importDepth};
        imports[key]=1;
        try {
            std::ifstream input(path); std::ostringstream source; source<<input.rdbuf(); if(!input) throw std::runtime_error("Cannot read script");
            Context module=ctx; module.category.clear();
            execute(parse(source.str(),path.string()),scope(ctx.globals),module);
            imports[key]=2;
        } catch(...) { imports.erase(key); throw; }
    }
    void execute(const Block& nodes,std::shared_ptr<Environment> env,Context& ctx) {
        ActiveMod active(activeMod,ctx.mod);
        for(const auto& n:nodes) {
            tick(n->at);
            switch(n->kind) {
            case Kind::Category: ctx.category=n->text; break;
            case Kind::Import: importFile(ctx,n->text); break;
            case Kind::Constant:
                if(ctx.category!="Globals") fail(n->at,"SCRIPT310","Global constants require @Globals");
                ctx.globals->assign(n->text,eval(n->children.at(0),env,ctx),n->at,true); break;
            case Kind::Assignment: {
                const auto& target=n->children[0]; auto value=eval(n->children[1],env,ctx);
                if(target->kind==Kind::Identifier) (ctx.category=="Globals" ? ctx.globals : env)->assign(target->text,std::move(value),n->at);
                else { auto owner=eval(target->children[0],env,ctx); owner.setMember(target->text,std::move(value),n->at); }
                break;
            }
            case Kind::Expression: (void)eval(n->children[0],env,ctx); break;
            case Kind::Function: {
                auto closure=env; Context captured=ctx; captured.category.clear();
                auto function=native([this,n,closure,captured](const std::vector<Value>& args,const Location& at) mutable {
                    ActiveMod active(activeMod,captured.mod);
                    if(calls>=64) fail(at,"SCRIPT311","Call depth limit exceeded");
                    if(args.size()>n->parameters.size()) fail(at,"SCRIPT312","Too many arguments for "+n->text);
                    auto parent=closure;
                    auto local=scope(parent);
                    for(size_t i=0;i<n->parameters.size();++i) {
                        const auto& p=n->parameters[i];
                        if(i>=args.size() && !p.defaultValue) fail(at,"SCRIPT312","Missing argument: "+p.name);
                        local->bindings[p.name]={i<args.size() ? args[i] : eval(p.defaultValue,parent,captured)};
                    }
                    ++calls; struct Depth { unsigned& count; ~Depth() { --count; } } guard{calls};
                    Context invocation=captured;
                    try { execute(n->body,local,invocation); } catch(const Returned& result) { return result.value; }
                    return Value{};
                });
                auto destination=calls ? env : ctx.globals;
                if(destination->bindings.contains(n->text) && n->extra!="event") fail(n->at,"SCRIPT314","Duplicate function: "+n->text);
                if(n->extra!="event") destination->bindings[n->text]={function,true};
                if(n->extra=="event") {
                    if(ctx.category!="Events") fail(n->at,"SCRIPT315","Event handlers require @Events");
                    events[n->text].push_back({nextToken++,ctx.mod,function});
                }
                break;
            }
            case Kind::Return: {
                if(!calls) fail(n->at,"SCRIPT316","return outside function");
                std::vector<Value> values; for(const auto& v:n->children) values.push_back(eval(v,env,ctx));
                throw Returned{values.empty() ? Value{} : values.size()==1 ? values[0] : list(std::move(values))};
            }
            case Kind::If: {
                Context block=ctx; block.category.clear();
                execute(eval(n->children[0],env,ctx).boolean(n->at) ? n->body : n->alternative,scope(env),block); break;
            }
            case Kind::For: {
                auto sequence=eval(n->children[0],env,ctx); auto values=std::get_if<std::shared_ptr<List>>(&sequence.data);
                if(!values) fail(n->at,"SCRIPT301","for requires a List");
                const auto snapshot=(*values)->values;
                for(const auto& value:snapshot) { auto local=scope(env); local->bindings[n->text]={value}; Context block=ctx; block.category.clear(); execute(n->body,local,block); } break;
            }
            case Kind::Declaration: case Kind::Modify: {
                const bool core=ctx.category.starts_with("Core/");
                const auto category=core ? ctx.category.substr(5) : ctx.category;
                if(category.empty() || category=="Globals" || category=="Events") fail(n->at,"SCRIPT317","Definition requires a content category");
                const auto id=n->text.find(':')!=std::string::npos ? n->text : (core ? "core" : ctx.mod)+":"+n->text;
                auto changes=data(n->body,env,ctx);
                if(n->kind==Kind::Modify) registry.patch(category,id,changes,ctx.mod,n->at);
                else {
                    if(core && ctx.mod!="core") fail(n->at,"SCRIPT318","New definitions cannot use the core namespace");
                    if(!n->children.empty()) {
                        auto base=reference(eval(n->children[0],env,ctx),n->at); if(base.find(':')==std::string::npos) base=ctx.mod+":"+base;
                        changes=Registry::merge(registry.get(category,base,n->at).data,changes);
                    }
                    registry.add({id,category,n->extra,changes},n->at);
                    ctx.globals->bindings[n->text]={Value(Symbol{id}),true};
                }
                break;
            }
            default: fail(n->at,"SCRIPT319","Invalid statement context");
            }
        }
    }
};
Runtime::Runtime() : impl_(std::make_unique<Impl>()) {
    const auto arity=[](const auto& a,size_t n,const auto& at) { if(a.size()!=n) fail(at,"SCRIPT312","Invalid collection argument count"); };
    bind("registry_revision",native([this,arity](const auto& a,const auto& at) { arity(a,0,at); return Value(double(impl_->registry.revision())); }));
    bind("list_count",native([arity](const auto& a,const auto& at) {
        arity(a,1,at); const auto p=std::get_if<std::shared_ptr<List>>(&a[0].data);
        if(!p) fail(at,"SCRIPT301","Expected list"); return Value(double((*p)->values.size()));
    }));
    bind("number_range",native([arity](const auto& a,const auto& at) {
        arity(a,1,at); const auto count=a[0].number(at);
        if(!std::isfinite(count) || count<0 || count>4096 || count!=std::floor(count)) fail(at,"SCRIPT301","Range requires integer in [0,4096]");
        std::vector<Value> values; for(size_t i=0;i<count;++i) values.emplace_back(double(i)); return list(std::move(values));
    }));
    bind("list_append",native([arity](const auto& a,const auto& at) {
        arity(a,2,at); const auto p=std::get_if<std::shared_ptr<List>>(&a[0].data);
        if(!p || (*p)->values.size()>=4096) fail(at,"SCRIPT301","Expected list with fewer than 4096 entries");
        // Copy the value to avoid aliasing mutable item attributes and list cycles.
        (*p)->values.push_back(clone(a[1])); return Value{};
    }));
    bind("list_at",native([arity](const auto& a,const auto& at) {
        arity(a,2,at); const auto p=std::get_if<std::shared_ptr<List>>(&a[0].data); const auto index=a[1].number(at);
        if(!p || !std::isfinite(index) || index<0 || index!=std::floor(index) || index>=(*p)->values.size()) fail(at,"SCRIPT301","List index out of range");
        return (*p)->values[static_cast<size_t>(index)];
    }));
    bind("list_contains",native([arity](const auto& a,const auto& at) {
        arity(a,2,at); const auto p=std::get_if<std::shared_ptr<List>>(&a[0].data);
        if(!p) fail(at,"SCRIPT301","Expected list");
        for(const auto& value:(*p)->values) if(equal(value,a[1])) return Value(true);
        return Value(false);
    }));
    bind("record_has",native([arity](const auto& a,const auto& at) {
        arity(a,2,at); const auto p=std::get_if<std::shared_ptr<Object>>(&a[0].data);
        if(!p) fail(at,"SCRIPT301","Expected record"); return Value((*p)->fields.contains(a[1].string(at)));
    }));
    bind("text_length",native([arity](const auto& a,const auto& at) { arity(a,1,at); return Value(double(a[0].string(at).size())); }));
    bind("data_merge",native([arity](const auto& a,const auto& at) { arity(a,2,at); return Registry::merge(a[0],a[1]); }));
    bind("definition_exists",native([this,arity](const auto& a,const auto& at) {
        arity(a,2,at); return Value(impl_->registry.contains(a[0].string(at),reference(a[1],at)));
    }));
    bind("get_definitions",native([this,arity](const auto& a,const auto& at) {
        arity(a,1,at); std::vector<Value> values;
        for(const auto& entry:impl_->registry.entries(a[0].string(at))) {
            auto value=object(); value.setMember("id",Value(entry.id),at); value.setMember("type",Value(entry.type),at); value.setMember("data",entry.data,at); values.push_back(std::move(value));
        }
        return list(std::move(values));
    }));
}
Runtime::~Runtime()=default;
void Runtime::bind(std::string name,Value value) { impl_->builtins->bindings[std::move(name)]={std::move(value),true}; }
void Runtime::registerSymbol(std::string name) { bind(name,Value(Symbol{name})); }
void Runtime::load(const std::string& mod,const std::filesystem::path& root,const std::string& entry,const std::vector<std::string>& dependencies) {
    auto& ctx=impl_->context(mod); ctx.root=root; impl_->registry.dependencies(mod,dependencies); impl_->budget=100000; impl_->importFile(ctx,entry);
}
void Runtime::execute(const std::string& mod,std::string_view source,const std::string& file) {
    auto& ctx=impl_->context(mod); ctx.category.clear(); impl_->budget=100000; impl_->execute(parse(source,file),impl_->scope(ctx.globals),ctx);
}
Value Runtime::global(const std::string& mod,const std::string& name) const {
    auto it=impl_->contexts.find(mod); if(it==impl_->contexts.end()) throw std::runtime_error("Unknown script module");
    auto binding=it->second.globals->find(name); if(!binding) throw std::runtime_error("Unknown script global: "+name); return binding->value;
}
std::vector<Diagnostic> Runtime::emit(const std::string& event,const std::vector<Value>& args) {
    std::vector<Diagnostic> diagnostics;
    if(impl_->dispatchDepth>=64) return {{"SCRIPT322","Event recursion limit exceeded"}};
    if(!impl_->dispatchDepth && !impl_->calls) impl_->budget=100000;
    ++impl_->dispatchDepth; struct Depth { unsigned& value; ~Depth() { --value; } } guard{impl_->dispatchDepth};
    const auto handlers=impl_->events[event];
    for(const auto& handler:handlers) {
        try { ActiveMod active(impl_->activeMod,handler.owner); invoke(handler.function,args); }
        catch(const DiagnosticError& e) { auto d=e.diagnostic; d.mod=handler.owner; diagnostics.push_back(std::move(d)); }
        catch(const std::exception& e) { diagnostics.push_back({"SCRIPT320",e.what(),{},handler.owner}); }
    }
    return diagnostics;
}
Registry& Runtime::registry() { return impl_->registry; }
std::string Runtime::currentMod() const { return impl_->activeMod; }
std::uint64_t Runtime::subscribe(std::string event,Value handler,bool hook,std::string owner) {
    if(!std::holds_alternative<std::shared_ptr<Callable>>(handler.data)) fail({},"SCRIPT323","Handler must be a function");
    if(owner.empty()) owner=impl_->activeMod;
    auto token=impl_->nextToken++; (hook ? impl_->hooks : impl_->events)[std::move(event)].push_back({token,std::move(owner),std::move(handler)}); return token;
}
bool Runtime::unsubscribe(std::uint64_t token) {
    bool removed=false;
    for(auto* table:{&impl_->events,&impl_->hooks}) for(auto& [name,handlers]:*table) {
        (void)name; removed|=std::erase_if(handlers,[&](const auto& h) { return h.token==token; })!=0;
    }
    return removed;
}
Value Runtime::invoke(const Value& function,const std::vector<Value>& args) {
    if(!std::holds_alternative<std::shared_ptr<Callable>>(function.data)) fail({},"SCRIPT323","Expected function");
    if(impl_->dispatchDepth>=64) fail({},"SCRIPT322","Dispatch recursion limit exceeded");
    if(!impl_->dispatchDepth && !impl_->calls) impl_->budget=100000;
    ++impl_->dispatchDepth; struct Depth { unsigned& value; ~Depth() { --value; } } guard{impl_->dispatchDepth};
    return std::get<std::shared_ptr<Callable>>(function.data)->call(args,{});
}
Value Runtime::filter(const std::string& hook,Value value) {
    const auto handlers=impl_->hooks[hook];
    for(const auto& handler:handlers) { ActiveMod active(impl_->activeMod,handler.owner); value=invoke(handler.function,{value}); }
    return value;
}
}
