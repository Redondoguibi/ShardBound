#include "scripting/Runtime.h"
#include "scripting/Behavior.h"
#include "scripting/DataConversion.h"
#include "scripting/AsyncProgram.h"
#include "assets/VirtualFileSystem.h"
#include "core/PathText.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>
namespace game::script {
namespace {
struct Binding { Value value; bool constant=false; };
struct Environment {
    std::unordered_map<std::string,Binding> bindings;
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
struct AsyncFrame {
    std::shared_ptr<const AsyncProgram> program;
    size_t pc=0;
    std::vector<Value> stack;
    std::shared_ptr<Environment> env;
    Context context;
    struct Scope { std::shared_ptr<Environment> env;Context context; };
    struct Loop { std::vector<Value> values;size_t index=0;std::string name;Scope outer; };
    std::vector<Scope> scopes;
    std::vector<Loop> loops;
};
struct ScriptTask final : HostObject {
    std::uint64_t id=0,epoch=0;
    std::string owner,status="queued";
    std::weak_ptr<int> session;
    Location at;
    double deadline=0;
    bool timer=false;
    Value result;
    Diagnostic error;
    std::unique_ptr<AsyncFrame> frame;
    std::shared_ptr<ScriptTask> waiting;
    bool done() const {return status=="completed" || status=="failed" || status=="cancelled";}
    void release() {frame.reset();waiting.reset();}
    Value value(const Location& location) const {
        if(status=="failed") throw DiagnosticError(error);
        if(status=="cancelled") fail(location,"SCRIPT344","Awaited task was cancelled");
        if(status!="completed") fail(location,"SCRIPT342","Task has not completed");
        return clone(result);
    }
    Value get(const std::string& key,const Location& location) override {
        if(key=="id") return Value(double(id));
        if(key=="status") return Value(status);
        if(key=="done") return Value(done());
        if(key=="failed") return Value(status=="failed");
        if(key=="cancelled") return Value(status=="cancelled");
        if(key=="result") return value(location);
        if(key=="error") return Value(status=="failed"?error.message:std::string{});
        fail(location,"SCRIPT302","Unknown task member: "+key);
    }
    void set(const std::string&,Value,const Location& location) override {fail(location,"SCRIPT342","Task properties are read-only");}
};
std::shared_ptr<ScriptTask> taskValue(const Value& value,const Location& at) {
    if(auto host=std::get_if<std::shared_ptr<HostObject>>(&value.data)) if(auto task=std::dynamic_pointer_cast<ScriptTask>(*host)) return task;
    fail(at,"SCRIPT342","Expected Task");
}
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
    struct Profile { std::uint64_t eventCalls=0,hookCalls=0,errors=0; double milliseconds=0; };
    std::map<std::string,Profile> profiles;
    struct System { std::string owner,phase; Value callback; int priority=0; bool enabled=true; double interval=0,elapsed=0,accumulator=0; bool disableOnError=false; std::vector<std::string> after; };
    std::map<std::string,System> systems;
    struct Cost { std::uint64_t calls=0,errors=0;double total=0,last=0,peak=0; };
    std::map<std::string,Cost> systemCosts;
    std::map<std::string,Location> systemSources;
    std::vector<Diagnostic> journal;
    void remember(Diagnostic value) {
        const auto truncate=[](std::string& text,size_t maximum) { if(text.size()<=maximum) return;while(maximum && (static_cast<unsigned char>(text[maximum])&0xc0)==0x80) --maximum;text.resize(maximum);text+="..."; };
        truncate(value.code,64);truncate(value.message,4096);truncate(value.file,1024);truncate(value.mod,128);truncate(value.resource,256);
        if(journal.size()>=256) journal.erase(journal.begin());journal.push_back(std::move(value));
    }
    unsigned dispatchDepth=0;
    unsigned calls=0;
    unsigned importDepth=0;
    size_t budget=100000;
    std::map<std::uint64_t,std::shared_ptr<ScriptTask>> tasks;
    std::uint64_t nextTask=1,taskEpoch=0,lastTask=0;
    double taskTime=0;
    bool advancingTasks=false;
    std::shared_ptr<int> taskSession=std::make_shared<int>(0);
    size_t scopesSinceCollection=0;
    ~Impl() { for(auto& weak:scopes) if(auto env=weak.lock()) env->bindings.clear(); }
    std::shared_ptr<Environment> scope(std::shared_ptr<Environment> parent) {
        if(++scopesSinceCollection>=256 && scopes.size()>1024) {std::erase_if(scopes,[](const auto& weak) { return weak.expired(); });scopesSinceCollection=0;}
        auto env=std::make_shared<Environment>(); env->parent=std::move(parent); scopes.push_back(env); return env;
    }
    Context& context(const std::string& mod) {
        auto [it,created]=contexts.try_emplace(mod);
        if(created) { it->second.mod=mod; it->second.globals=scope(builtins); }
        return it->second;
    }
    void tick(const Location& at) { if(!budget) fail(at,"SCRIPT304","Execution instruction budget exceeded"); --budget; }
    Value callValue(const Value& function,const std::vector<Value>& args,const Location& at) {
        auto callable=std::get_if<std::shared_ptr<Callable>>(&function.data);if(!callable) fail(at,"SCRIPT301","Value is not callable");
        try {return (*callable)->call(args,at);}
        catch(const DiagnosticError& error) {auto d=error.diagnostic;if(d.file.empty()) {d.file=at.file;d.line=at.line;d.column=at.column;}throw DiagnosticError(std::move(d));}
        catch(const std::exception& error) {fail(at,"SCRIPT320",error.what());}
    }
    Value unary(const NodePtr& n,const Value& value) {
        if(n->text=="and" || n->text=="or") return Value(value.boolean(n->at));
        if(n->text=="not") return Value(!value.boolean(n->at));
        return Value((n->text=="-"?-1:1)*value.number(n->at));
    }
    Value binary(const NodePtr& n,const Value& left,const Value& right) {
        if(n->text=="and" || n->text=="or") return Value(right.boolean(n->at));
        if(n->text=="==") return Value(equal(left,right));if(n->text=="!=") return Value(!equal(left,right));
        if(n->text=="+" && std::holds_alternative<std::string>(left.data) && std::holds_alternative<std::string>(right.data)) return Value(left.string()+right.string());
        const auto a=left.number(n->at),b=right.number(n->at);
        if(n->text=="<") return Value(a<b);if(n->text=="<=") return Value(a<=b);
        if(n->text==">") return Value(a>b);if(n->text==">=") return Value(a>=b);
        if((n->text=="/" || n->text=="%") && b==0) fail(n->at,"SCRIPT305","Division by zero");
        const double result=n->text=="+"?a+b:n->text=="-"?a-b:n->text=="*"?a*b:n->text=="/"?a/b:std::fmod(a,b);
        if(!std::isfinite(result)) fail(n->at,"SCRIPT305","Numeric overflow");return Value(result);
    }
    std::shared_ptr<ScriptTask> makeTask(const std::string& owner,const Location& at) {
        if(tasks.size()>=256) fail(at,"SCRIPT345","Session pending task limit (256) exceeded");
        size_t owned=0;for(const auto& [id,t]:tasks) { (void)id;if(t->owner==owner && !t->done()) ++owned; }
        if(owned>=64) fail(at,"SCRIPT345","Mod pending task limit (64) exceeded");
        auto task=std::make_shared<ScriptTask>();task->id=nextTask++;task->owner=owner;task->at=at;task->epoch=taskEpoch+1;task->session=taskSession;
        tasks.emplace(task->id,task);return task;
    }
    Value handle(const std::shared_ptr<ScriptTask>& task) {return Value(std::static_pointer_cast<HostObject>(task));}
    Value eval(const NodePtr& n,std::shared_ptr<Environment> env,Context& ctx) {
        tick(n->at);
        switch(n->kind) {
        case Kind::Number:return Value(n->numericValue);
        case Kind::String: return Value(n->text);
        case Kind::Boolean: return Value(n->text=="true");
        case Kind::Null: return {};
        case Kind::Identifier: {
            if(auto value=env->find(n->text)) return value->value;
            fail(n->at,"SCRIPT102","Unknown symbol: "+n->text);
        }
        case Kind::List: { std::vector<Value> values;values.reserve(n->children.size()); for(const auto& v:n->children) values.push_back(eval(v,env,ctx)); return list(std::move(values)); }
        case Kind::Member: return eval(n->children[0],env,ctx).member(n->text,n->at);
        case Kind::Call: {
            auto function=eval(n->children[0],env,ctx);
            if(!std::holds_alternative<std::shared_ptr<Callable>>(function.data)) fail(n->at,"SCRIPT301","Value is not callable");
            std::vector<Value> args;args.reserve(n->children.size()-1);for(size_t i=1;i<n->children.size();++i) args.push_back(eval(n->children[i],env,ctx));
            return callValue(function,args,n->at);
        }
        case Kind::Unary: {
            auto value=eval(n->children[0],env,ctx);
            return unary(n,value);
        }
        case Kind::Binary: {
            auto left=eval(n->children[0],env,ctx);
            if(n->text=="and" && !left.boolean(n->at)) return Value(false);
            if(n->text=="or" && left.boolean(n->at)) return Value(true);
            auto right=eval(n->children[1],env,ctx);
            return binary(n,left,right);
        }
        default: fail(n->at,"SCRIPT306","Invalid expression node");
        }
    }
    void completeTask(ScriptTask& task,Value value) {
        // Results are snapshots. Task handles in a result would permit hidden
        // promise/container cycles; compose tasks with await instead.
        std::vector<std::pair<Value,unsigned>> pending{{value,0}};
        while(!pending.empty()) {
            auto [current,depth]=std::move(pending.back());pending.pop_back();tick(task.at);
            if(depth>64) fail(task.at,"SCRIPT343","Task result nesting exceeds 64 levels");
            if(auto host=std::get_if<std::shared_ptr<HostObject>>(&current.data)) {
                if(std::dynamic_pointer_cast<ScriptTask>(*host)) fail(task.at,"SCRIPT343","Task results cannot contain Task handles; await them first");
            } else if(auto record=std::get_if<std::shared_ptr<Object>>(&current.data)) {
                for(const auto& [key,child]:(*record)->fields) { (void)key;pending.emplace_back(child,depth+1); }
            } else if(auto list=std::get_if<std::shared_ptr<List>>(&current.data)) {
                for(const auto& child:(*list)->values) pending.emplace_back(child,depth+1);
            }
        }
        task.result=clone(value);task.status="completed";
    }
    void runTask(ScriptTask& task,size_t& remaining) {
        auto& f=*task.frame;
        ActiveMod active(activeMod,task.owner);
        ++calls;struct Depth {unsigned& value;~Depth(){--value;}} depth{calls};
        const auto pop=[&]() {auto value=std::move(f.stack.back());f.stack.pop_back();return value;};
        const auto enter=[&]() {f.scopes.push_back({f.env,f.context});f.env=scope(f.env);f.context.category.clear();};
        const auto leave=[&]() {auto outer=std::move(f.scopes.back());f.scopes.pop_back();f.env=std::move(outer.env);f.context=std::move(outer.context);};
        const auto nextLoop=[&](AsyncFrame::Loop& loop) {
            // A captured iteration keeps its own environment. Otherwise reuse
            // the storage rather than allocating an environment every iteration.
            if(f.env.use_count()!=1 || f.env->parent!=loop.outer.env) f.env=scope(loop.outer.env);
            else f.env->bindings.clear();
            f.context=loop.outer.context;f.context.category.clear();f.env->bindings[loop.name]={loop.values[loop.index]};
        };
        task.status="running";
        for(size_t slice=0;slice<256 && remaining && budget && f.pc<f.program->code.size() && task.status=="running";++slice) {
            --remaining;
            const auto& instruction=f.program->code[f.pc];const auto& n=instruction.node;tick(n->at);++f.pc;
            switch(instruction.op) {
            case AsyncOp::Eval:f.stack.push_back(eval(n,f.env,f.context));break;
            case AsyncOp::Call: {
                const auto first=f.stack.size()-instruction.argument;
                std::vector<Value> args;args.reserve(instruction.argument);
                for(size_t i=first;i<f.stack.size();++i) args.push_back(std::move(f.stack[i]));
                auto function=std::move(f.stack[first-1]);f.stack.resize(first-1);
                auto result=callValue(function,args,n->at);f.stack.push_back(std::move(result));break;
            }
            case AsyncOp::Member: {auto owner=pop();f.stack.push_back(owner.member(n->text,n->at));break;}
            case AsyncOp::Unary: {auto value=pop();f.stack.push_back(unary(n,value));break;}
            case AsyncOp::Binary: {auto right=pop(),left=pop();f.stack.push_back(binary(n,left,right));break;}
            case AsyncOp::List: {
                const auto first=f.stack.size()-instruction.argument;std::vector<Value> values;values.reserve(instruction.argument);
                for(size_t i=first;i<f.stack.size();++i) values.push_back(std::move(f.stack[i]));
                f.stack.resize(first);f.stack.push_back(list(std::move(values)));break;
            }
            case AsyncOp::Await: {
                auto dependency=taskValue(f.stack.back(),n->at);
                if(dependency->session.lock()!=taskSession) fail(n->at,"SCRIPT342","Cannot await a task from another runtime session");
                if(dependency->done()) {f.stack.back()=dependency->value(n->at);task.waiting.reset();break;}
                for(auto parent=dependency;parent;parent=parent->waiting) if(parent.get()==&task) fail(n->at,"SCRIPT343","Task dependency cycle");
                task.waiting=std::move(dependency);task.status="waiting";--f.pc;return;
            }
            case AsyncOp::Assign: {
                const auto& target=n->children[0];
                if(target->kind==Kind::Identifier) {auto value=pop();(f.context.category=="Globals"?f.context.globals:f.env)->assign(target->text,std::move(value),n->at);}
                else {auto owner=pop();auto value=pop();owner.setMember(target->text,std::move(value),n->at);}break;
            }
            case AsyncOp::Constant:
                if(f.context.category!="Globals") fail(n->at,"SCRIPT310","Global constants require @Globals");
                f.context.globals->assign(n->text,pop(),n->at,true);break;
            case AsyncOp::Discard:(void)pop();break;
            case AsyncOp::Return: {
                if(!instruction.argument) completeTask(task,{});
                else if(instruction.argument==1) completeTask(task,pop());
                else {std::vector<Value> values(instruction.argument);for(size_t i=instruction.argument;i>0;--i) values[i-1]=pop();completeTask(task,list(std::move(values)));}
                return;
            }
            case AsyncOp::Branch:if(!pop().boolean(n->at)) f.pc=instruction.argument;break;
            case AsyncOp::Jump:f.pc=instruction.argument;break;
            case AsyncOp::Enter:enter();break;
            case AsyncOp::Leave:leave();break;
            case AsyncOp::ForBegin: {
                auto sequence=pop();auto values=std::get_if<std::shared_ptr<List>>(&sequence.data);
                if(!values) fail(n->at,"SCRIPT301","for requires a List");
                if((*values)->values.empty()) {f.pc=instruction.argument;break;}
                f.loops.push_back({(*values)->values,0,n->text,{f.env,f.context}});nextLoop(f.loops.back());break;
            }
            case AsyncOp::ForNext: {
                auto& loop=f.loops.back();
                if(++loop.index<loop.values.size()) {nextLoop(loop);f.pc=instruction.argument;}
                else {f.env=std::move(loop.outer.env);f.context=std::move(loop.outer.context);f.loops.pop_back();}break;
            }
            case AsyncOp::And:case AsyncOp::Or: {
                const bool value=f.stack.back().boolean(n->at);
                if(value==(instruction.op==AsyncOp::Or)) f.pc=instruction.argument;
                else (void)pop();break;
            }
            case AsyncOp::Statement:execute(Block{n},f.env,f.context);break;
            }
        }
        if(task.status=="running") {
            if(f.pc==f.program->code.size()) completeTask(task,{});
            else task.status="queued";
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
        const auto name=pathText(path);
        if(path.extension()!=".sl") fail({name},"SCRIPT308","Imports must use .sl");
        const auto key=ctx.mod+":"+name;
        if(imports[key]==1) fail({name},"SCRIPT309","Circular import");
        if(imports[key]==2) return;
        if(importDepth>=64) fail({name},"SCRIPT321","Import nesting limit exceeded");
        ++importDepth; struct ImportDepth { unsigned& count; ~ImportDepth() { --count; } } depth{importDepth};
        imports[key]=1;
        try {
            std::ifstream input(path); std::ostringstream source; source<<input.rdbuf(); if(!input) throw std::runtime_error("Cannot read script");
            Context module=ctx; module.category.clear();
            execute(parse(source.str(),name),scope(ctx.globals),module);
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
                const auto program=n->asynchronous?compileAsync(n->body):std::shared_ptr<const AsyncProgram>{};
                auto function=native([this,n,closure,captured,program](const std::vector<Value>& args,const Location& at) mutable {
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
                    if(program) {
                        auto task=makeTask(captured.mod,at);task->frame=std::make_unique<AsyncFrame>();
                        task->frame->program=program;task->frame->env=std::move(local);task->frame->context=captured;
                        return handle(task);
                    }
                    ++calls; struct Depth { unsigned& count; ~Depth() { --count; } } guard{calls};
                    Context invocation=captured;
                    try { execute(n->body,local,invocation); } catch(const Returned& result) { return result.value; }
                    return Value{};
                });
                std::get<std::shared_ptr<Callable>>(function.data)->asynchronous=n->asynchronous;
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
                std::shared_ptr<Environment> local;
                for(const auto& value:snapshot) {
                    if(!local || local.use_count()!=1) local=scope(env);else local->bindings.clear();
                    local->bindings[n->text]={value};Context block=ctx;block.category.clear();execute(n->body,local,block);
                }break;
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
    bindBehaviorApi(*this);
    const auto arity=[](const auto& a,size_t n,const auto& at) { if(a.size()!=n) fail(at,"SCRIPT312","Invalid collection argument count"); };
    const auto timer=[this](double seconds,const Location& at) {
        if(!std::isfinite(seconds) || seconds<0 || seconds>3600) fail(at,"SCRIPT342","sleep duration must be in [0,3600] seconds");
        auto task=impl_->makeTask(impl_->activeMod,at);task->timer=true;task->deadline=impl_->taskTime+seconds;task->status="waiting";return impl_->handle(task);
    };
    bind("sleep",native([arity,timer](const auto& a,const auto& at) {arity(a,1,at);return timer(a[0].number(at),at);}));
    bind("next_frame",native([arity,timer](const auto& a,const auto& at) {arity(a,0,at);return timer(0,at);}));
    bind("task_cancel",native([this,arity](const auto& a,const auto& at) {
        arity(a,1,at);auto task=taskValue(a[0],at);
        if(task->owner!=impl_->activeMod || task->session.lock()!=impl_->taskSession) fail(at,"SCRIPT342","Cannot cancel another mod or session's task");
        if(task->done()) return Value(false);
        task->status="cancelled";return Value(true);
    }));
    bind("registry_revision",native([this,arity](const auto& a,const auto& at) { arity(a,0,at); return Value(double(impl_->registry.revision())); }));
    // Host-measured handler costs (not an estimate of total GPU/frame time).
    bind("mod_profile",native([this](const auto& a,const auto& at) {
        if(a.size()>1) fail(at,"SCRIPT312","mod_profile(mod_id?)");
        const auto profile=[&](const Impl::Profile& p) {
            auto result=object();
            result.setMember("event_calls",Value(double(p.eventCalls)),at);
            result.setMember("hook_calls",Value(double(p.hookCalls)),at);
            result.setMember("errors",Value(double(p.errors)),at);
            result.setMember("milliseconds",Value(p.milliseconds),at);
            return result;
        };
        if(!a.empty()) {
            const auto id=a[0].string(at);const auto it=impl_->profiles.find(id);
            return profile(it==impl_->profiles.end()?Impl::Profile{}:it->second);
        }
        auto result=object();
        for(const auto& [id,p]:impl_->profiles) result.setMember(id,profile(p),at);
        return result;
    }));
    bind("developer_report",native([this](const auto& a,const auto& at) { if(!a.empty()) fail(at,"SCRIPT312","developer_report()");return fromData(debugSnapshot()); }));
    // Named, mod-owned update systems: removable, replaceable and diagnosable.
    bind("register_system",native([this](const auto& a,const auto& at) {
        if(a.size()<3 || a.size()>4) fail(at,"SCRIPT330","register_system(id, phase, function, priority?)");
        const auto owner=impl_->activeMod,id=a[0].string(at),phase=a[1].string(at);
        if(owner.empty() || !id.starts_with(owner+":") || id.size()>128 || id.size()<=owner.size()+1)
            fail(at,"SCRIPT330","System ID must use the calling mod namespace");
        if(phase!="update" && phase!="fixed") fail(at,"SCRIPT330","System phase must be update or fixed");
        if(!std::holds_alternative<std::shared_ptr<Callable>>(a[2].data)) fail(at,"SCRIPT330","System callback must be a function");
        if(std::get<std::shared_ptr<Callable>>(a[2].data)->asynchronous) fail(at,"SCRIPT340","Systems require a synchronous callback; start async work from it");
        int priority=0;
        if(a.size()==4) {
            const auto number=a[3].number(at);
            if(!std::isfinite(number) || number!=std::floor(number) || number<-100 || number>100)
                fail(at,"SCRIPT330","System priority must be an integer from -100 to 100");
            priority=int(number);
        }
        if(impl_->systems.contains(id)) fail(at,"SCRIPT330","System already registered: "+id);
        unsigned owned=0;for(const auto& [name,system]:impl_->systems) { (void)name;if(system.owner==owner) ++owned; }
        if(impl_->systems.size()>=256 || owned>=64) fail(at,"SCRIPT330","Named system limit reached (64 per mod, 256 total)");
        impl_->systems.emplace(id,Impl::System{owner,phase,a[2],priority,true});
        impl_->systemSources[id]=at;impl_->systemCosts[id]={};
        return Value(true);
    }));
    bind("configure_system",native([this,arity](const auto& a,const auto& at) {
        arity(a,2,at);const auto id=a[0].string(at);auto it=impl_->systems.find(id);
        if(it==impl_->systems.end() || it->second.owner!=impl_->activeMod) fail(at,"SCRIPT331","System not owned by current mod");
        auto next=it->second;const auto fields=std::get_if<std::shared_ptr<Object>>(&a[1].data);
        if(!fields) fail(at,"SCRIPT330","Expected system options record");
        for(const auto& [key,value]:(*fields)->fields) {
            if(key=="interval") {
                next.interval=value.number(at);
                if(!std::isfinite(next.interval) || next.interval<0 || next.interval>3600) fail(at,"SCRIPT330","Interval must be in [0,3600]");
                next.elapsed=0;next.accumulator=0;
            } else if(key=="disable_on_error") next.disableOnError=value.boolean(at);
            else if(key=="after") {
                const auto values=std::get_if<std::shared_ptr<List>>(&value.data);
                if(!values || (*values)->values.size()>64) fail(at,"SCRIPT330","Expected at most 64 dependencies");
                next.after.clear();std::set<std::string> unique;
                for(const auto& v:(*values)->values) {
                    const auto dep=v.string(at);const auto found=impl_->systems.find(dep);
                    if(dep==id || found==impl_->systems.end() || found->second.phase!=next.phase || !unique.insert(dep).second)
                        fail(at,"SCRIPT330","Dependencies must be unique, registered systems of the same phase");
                    next.after.push_back(dep);
                }
            } else fail(at,"SCRIPT330","Unknown system option: "+key);
        }
        std::set<std::string> visited;
        std::function<void(const std::string&)> visit=[&](const auto& dep) {
            if(dep==id) fail(at,"SCRIPT330","System dependency cycle");
            if(!visited.insert(dep).second) return;
            const auto found=impl_->systems.find(dep);if(found!=impl_->systems.end()) for(const auto& child:found->second.after) visit(child);
        };
        for(const auto& dep:next.after) visit(dep);
        it->second=std::move(next);return Value{};
    }));
    bind("system_enabled",native([this,arity](const auto& a,const auto& at) {
        arity(a,2,at);const auto id=a[0].string(at);
        auto it=impl_->systems.find(id);
        if(it==impl_->systems.end() || it->second.owner!=impl_->activeMod) fail(at,"SCRIPT331","System not owned by current mod: "+id);
        it->second.enabled=a[1].boolean(at);return Value{};
    }));
    bind("replace_system",native([this,arity](const auto& a,const auto& at) {
        arity(a,2,at);const auto id=a[0].string(at);
        auto it=impl_->systems.find(id);
        if(it==impl_->systems.end() || it->second.owner!=impl_->activeMod) fail(at,"SCRIPT331","System not owned by current mod: "+id);
        if(!std::holds_alternative<std::shared_ptr<Callable>>(a[1].data)) fail(at,"SCRIPT330","System callback must be a function");
        if(std::get<std::shared_ptr<Callable>>(a[1].data)->asynchronous) fail(at,"SCRIPT340","Systems require a synchronous callback");
        it->second.callback=a[1];return Value{};
    }));
    bind("unregister_system",native([this,arity](const auto& a,const auto& at) {
        arity(a,1,at);const auto id=a[0].string(at);
        auto it=impl_->systems.find(id);
        if(it==impl_->systems.end()) return Value(false);
        if(it->second.owner!=impl_->activeMod) fail(at,"SCRIPT331","System not owned by current mod: "+id);
        impl_->systems.erase(it);impl_->systemCosts.erase(id);impl_->systemSources.erase(id);return Value(true);
    }));
    bind("list_systems",native([this,arity](const auto& a,const auto& at) {
        arity(a,0,at);std::vector<Value> rows;
        for(const auto& [id,system]:impl_->systems) {
            auto row=object();row.setMember("id",Value(id),at);row.setMember("owner",Value(system.owner),at);
            row.setMember("phase",Value(system.phase),at);row.setMember("enabled",Value(system.enabled),at);
            row.setMember("priority",Value(double(system.priority)),at);
            row.setMember("interval",Value(system.interval),at);row.setMember("disable_on_error",Value(system.disableOnError),at);
            std::vector<Value> after;for(const auto& dependency:system.after) after.emplace_back(dependency);row.setMember("after",list(std::move(after)),at);rows.push_back(std::move(row));
        }
        return list(std::move(rows));
    }));
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
    bind("record_set",native([arity](const auto& a,const auto& at) {
        arity(a,3,at);
        const auto record=std::get_if<std::shared_ptr<Object>>(&a[0].data);
        if(!record) fail(at,"SCRIPT301","Expected record");
        auto target=a[0];
        target.setMember(a[1].string(at),a[2],at);
        return Value{};
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
Runtime::~Runtime() {cancelTasks();}
void Runtime::cancelTasks() {
    if(impl_->advancingTasks) throw std::runtime_error("Cannot clear tasks while the scheduler is running");
    for(auto& [id,task]:impl_->tasks) { (void)id;if(!task->done()) task->status="cancelled";task->release(); }
    impl_->tasks.clear();
}
std::vector<Diagnostic> Runtime::advanceTasks(double delta) {
    if(!std::isfinite(delta) || delta<0 || delta>3600) fail({},"SCRIPT346","Task delta must be finite and in [0,3600]");
    if(impl_->advancingTasks || impl_->dispatchDepth || impl_->calls) fail({},"SCRIPT346","Task scheduler cannot be reentered");
    impl_->taskTime+=delta;++impl_->taskEpoch;
    if(impl_->tasks.empty()) return {};
    impl_->advancingTasks=true;++impl_->dispatchDepth;
    struct Guard {Impl& impl;~Guard(){impl.advancingTasks=false;--impl.dispatchDepth;}} guard{*impl_};
    impl_->budget=100000;size_t remaining=4096;
    std::vector<std::shared_ptr<ScriptTask>> snapshot;snapshot.reserve(impl_->tasks.size());
    // Rotating the starting point prevents old CPU-heavy tasks starving new ones.
    const auto append=[&](auto begin,auto end) {for(auto it=begin;it!=end;++it) snapshot.push_back(it->second);};
    auto pivot=impl_->tasks.upper_bound(impl_->lastTask);append(pivot,impl_->tasks.end());append(impl_->tasks.begin(),pivot);
    for(const auto& task:snapshot) if(task->timer && !task->done() && task->epoch<=impl_->taskEpoch && task->deadline<=impl_->taskTime) task->status="completed";
    std::vector<Diagnostic> diagnostics;
    for(const auto& task:snapshot) {
        if(!remaining || !impl_->budget) break;
        if(task->timer || task->done() || task->epoch>impl_->taskEpoch || (task->waiting && !task->waiting->done())) continue;
        impl_->lastTask=task->id;
        const auto start=std::chrono::steady_clock::now();auto& profile=impl_->profiles[task->owner];
        try {impl_->runTask(*task,remaining);}
        catch(const DiagnosticError& error) {task->error=error.diagnostic;task->status="failed";}
        catch(const std::exception& error) {task->error={"SCRIPT320",error.what(),task->at.file,task->owner,{},task->at.line,task->at.column};task->status="failed";}
        profile.milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(task->status=="failed") {
            ++profile.errors;task->error.mod=task->owner;task->error.resource="task:"+std::to_string(task->id);
            diagnostics.push_back(task->error);impl_->remember(task->error);
        }
    }
    for(auto it=impl_->tasks.begin();it!=impl_->tasks.end();) {
        if(it->second->done()) {it->second->release();it=impl_->tasks.erase(it);}else ++it;
    }
    return diagnostics;
}
void Runtime::bind(std::string name,Value value) { impl_->builtins->bindings[std::move(name)]={std::move(value),true}; }
void Runtime::registerSymbol(std::string name) { bind(name,Value(Symbol{name})); }
void Runtime::load(const std::string& mod,const std::filesystem::path& root,const std::string& entry,const std::vector<std::string>& dependencies) {
    auto& ctx=impl_->context(mod); ctx.root=root; impl_->registry.dependencies(mod,dependencies); impl_->budget=100000;try { impl_->importFile(ctx,entry); }catch(const DiagnosticError& e) { auto d=e.diagnostic;d.mod=mod;impl_->remember(d);throw; }
}
void Runtime::execute(const std::string& mod,std::string_view source,const std::string& file) {
    auto& ctx=impl_->context(mod); ctx.category.clear(); impl_->budget=100000;try { impl_->execute(parse(source,file),impl_->scope(ctx.globals),ctx); }catch(const DiagnosticError& e) { auto d=e.diagnostic;d.mod=mod;impl_->remember(d);throw; }
}
std::filesystem::path Runtime::moduleRoot(const std::string& mod) const {
    const auto it=impl_->contexts.find(mod);if(it==impl_->contexts.end() || it->second.root.empty()) throw std::runtime_error("Migration registration needs a loaded mod root");return it->second.root;
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
    const auto foundHandlers=impl_->events.find(event);const auto handlers=foundHandlers==impl_->events.end()?std::vector<Impl::Handler>{}:foundHandlers->second;
    // Snapshot systems before invoking any handler; changes during dispatch
    // take effect on the following event rather than halfway through this one.
    std::vector<std::pair<std::string,Impl::System>> systems;
    if(event=="on_update" || event=="on_fixed_update") {
        const auto phase=event=="on_update"?"update":"fixed";
        for(auto& [id,system]:impl_->systems) if(system.enabled && system.phase==phase) {
            if(system.interval>0) {
                if(args.empty() || !std::holds_alternative<double>(args[0].data) || !std::isfinite(args[0].number()) || args[0].number()<0) {
                    diagnostics.push_back({"SCRIPT330","Timed systems require a finite nonnegative delta",{},system.owner});continue;
                }
                system.elapsed+=args[0].number();system.accumulator+=args[0].number();
                if(system.accumulator+1e-9<system.interval) continue;
                system.accumulator=std::max(0.0,system.accumulator-system.interval*std::floor((system.accumulator+1e-9)/system.interval));
            }
            systems.emplace_back(id,system);system.elapsed=0;
        }
        std::sort(systems.begin(),systems.end(),[](const auto& a,const auto& b){
            return a.second.priority==b.second.priority ? a.first<b.first : a.second.priority<b.second.priority;
        });
        // Stable topological ordering among due systems. Disabled/not-due dependencies
        // impose no barrier; configuration rejects cycles before changing live state.
        std::vector<std::pair<std::string,Impl::System>> ordered;std::set<std::string> pending;
        for(const auto& [id,system]:systems) { (void)system;pending.insert(id); }
        while(!pending.empty()) {
            auto found=std::find_if(systems.begin(),systems.end(),[&](const auto& entry) {
                return pending.contains(entry.first) && std::none_of(entry.second.after.begin(),entry.second.after.end(),[&](const auto& id){return pending.contains(id);});
            });
            if(found==systems.end()) { diagnostics.push_back({"SCRIPT330","System dependency cycle"});break; }
            pending.erase(found->first);ordered.push_back(*found);
        }
        systems=std::move(ordered);
    }
    for(const auto& handler:handlers) {
        auto& profile=impl_->profiles[handler.owner];
        const auto start=std::chrono::steady_clock::now();
        try { if(impl_->dispatchDepth==1 && !impl_->calls) impl_->budget=100000;ActiveMod active(impl_->activeMod,handler.owner); invoke(handler.function,args); }
        catch(const DiagnosticError& e) { ++profile.errors;auto d=e.diagnostic; d.mod=handler.owner; diagnostics.push_back(std::move(d)); }
        catch(const std::exception& e) { ++profile.errors;diagnostics.push_back({"SCRIPT320",e.what(),{},handler.owner}); }
        catch(...) { ++profile.errors;profile.milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();++profile.eventCalls;throw; }
        profile.milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();++profile.eventCalls;
    }
    if(event=="on_update" || event=="on_fixed_update") {
        for(const auto& [id,system]:systems) {
            (void)id;
            auto& profile=impl_->profiles[system.owner];
            const auto start=std::chrono::steady_clock::now();
            const auto errorsBefore=profile.errors;
            try {
                if(impl_->dispatchDepth==1 && !impl_->calls) impl_->budget=100000;
                ActiveMod active(impl_->activeMod,system.owner);auto arguments=args;
                if(system.interval>0 && !arguments.empty()) arguments[0]=Value(system.elapsed);
                invoke(system.callback,arguments);
            }
            catch(const DiagnosticError& e) { ++profile.errors;auto d=e.diagnostic;d.mod=system.owner;d.resource=id;diagnostics.push_back(std::move(d)); }
            catch(const std::exception& e) { ++profile.errors;diagnostics.push_back({"SCRIPT320",e.what(),{},system.owner}); }
            catch(...) { ++profile.errors;++profile.eventCalls;profile.milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();throw; }
            if(profile.errors!=errorsBefore && system.disableOnError) {
                const auto current=impl_->systems.find(id);
                if(current!=impl_->systems.end() && equal(current->second.callback,system.callback)) current->second.enabled=false;
            }
            const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(impl_->systems.contains(id)) { auto& cost=impl_->systemCosts[id];++cost.calls;cost.errors+=profile.errors!=errorsBefore;cost.last=elapsed;cost.total+=elapsed;cost.peak=std::max(cost.peak,elapsed); }
            ++profile.eventCalls;profile.milliseconds+=elapsed;
        }
    }
    for(auto& d:diagnostics) { if(d.resource.empty()) d.resource=event;impl_->remember(d); }
    return diagnostics;
}
Data Runtime::debugSnapshot() const {
    auto result=object();std::vector<Value> diagnostics,systems,tasks;
    for(const auto& [id,task]:impl_->tasks) {
        auto row=object();row.setMember("id",Value(double(id)),{});row.setMember("owner",Value(task->owner),{});row.setMember("status",Value(task->status),{});
        row.setMember("file",Value(task->at.file),{});row.setMember("line",Value(double(task->at.line)),{});
        row.setMember("waiting_for",task->waiting?Value(double(task->waiting->id)):Value{},{});tasks.push_back(std::move(row));
    }
    result.setMember("tasks",list(std::move(tasks)),{});
    for(const auto& d:impl_->journal) {
        auto row=object();row.setMember("code",Value(d.code),{});row.setMember("message",Value(d.message),{});row.setMember("file",Value(d.file),{});row.setMember("mod",Value(d.mod),{});row.setMember("resource",Value(d.resource),{});row.setMember("line",Value(double(d.line)),{});row.setMember("column",Value(double(d.column)),{});diagnostics.push_back(row);
    }
    for(const auto& [id,system]:impl_->systems) {
        const auto source=impl_->systemSources.find(id);const auto found=impl_->systemCosts.find(id);const Impl::Cost cost=found==impl_->systemCosts.end()?Impl::Cost{}:found->second;
        auto row=object();row.setMember("id",Value(id),{});row.setMember("owner",Value(system.owner),{});row.setMember("phase",Value(system.phase),{});row.setMember("enabled",Value(system.enabled),{});row.setMember("interval",Value(system.interval),{});row.setMember("priority",Value(double(system.priority)),{});
        std::vector<Value> after;for(const auto& dependency:system.after) after.emplace_back(dependency);row.setMember("after",list(std::move(after)),{});
        row.setMember("file",Value(source==impl_->systemSources.end()?std::string{}:source->second.file),{});row.setMember("line",Value(double(source==impl_->systemSources.end()?0:source->second.line)),{});
        row.setMember("calls",Value(double(cost.calls)),{});row.setMember("errors",Value(double(cost.errors)),{});row.setMember("total_ms",Value(cost.total),{});row.setMember("last_ms",Value(cost.last),{});row.setMember("peak_ms",Value(cost.peak),{});systems.push_back(row);
    }
    auto profiles=object();for(const auto& [id,p]:impl_->profiles) { auto row=object();row.setMember("event_calls",Value(double(p.eventCalls)),{});row.setMember("hook_calls",Value(double(p.hookCalls)),{});row.setMember("errors",Value(double(p.errors)),{});row.setMember("milliseconds",Value(p.milliseconds),{});profiles.setMember(id,row,{}); }
    result.setMember("diagnostics",list(std::move(diagnostics)),{});result.setMember("systems",list(std::move(systems)),{});result.setMember("registry",impl_->registry.inspect(),{});result.setMember("profiles",profiles,{});return toData(result);
}
Registry& Runtime::registry() { return impl_->registry; }
std::string Runtime::currentMod() const { return impl_->activeMod; }
std::uint64_t Runtime::subscribe(std::string event,Value handler,bool hook,std::string owner) {
    if(!std::holds_alternative<std::shared_ptr<Callable>>(handler.data)) fail({},"SCRIPT323","Handler must be a function");
    if(hook && std::get<std::shared_ptr<Callable>>(handler.data)->asynchronous) fail({},"SCRIPT340","Hooks require synchronous callbacks");
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
Value Runtime::call(const std::string& builtin,const std::vector<Value>& args) {
    const auto found=impl_->builtins->bindings.find(builtin);
    if(found==impl_->builtins->bindings.end()) throw std::runtime_error("Unknown builtin: "+builtin);
    const auto function=found->second.value;return invoke(function,args);
}
Value Runtime::filter(const std::string& hook,Value value) {
    const auto foundHandlers=impl_->hooks.find(hook);const auto handlers=foundHandlers==impl_->hooks.end()?std::vector<Impl::Handler>{}:foundHandlers->second;
    for(const auto& handler:handlers) {
        auto& profile=impl_->profiles[handler.owner];
        const auto start=std::chrono::steady_clock::now();
        try { ActiveMod active(impl_->activeMod,handler.owner); value=invoke(handler.function,{value}); }
        catch(...) { ++profile.errors;++profile.hookCalls;profile.milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();throw; }
        ++profile.hookCalls;profile.milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    }
    return value;
}
}
