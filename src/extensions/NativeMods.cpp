#include "extensions/NativeMods.h"
#include "scripting/DataConversion.h"
#include "scripting/Bridge.h"
#include "core/Log.h"
#include "shardbound.h"
#include <Windows.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <set>
#include <cstring>
#include <cmath>
namespace game {
struct NativeMods::Impl {
    script::Runtime& runtime; World& world; Components& components; SaveGame& saves;
    NativeMods* owner=nullptr;
    struct Module {
        Impl& host; std::string mod; HMODULE library=nullptr; SbHost api{}; SbPlugin plugin{};
        std::set<std::uint64_t> tokens; std::thread::id thread=std::this_thread::get_id();
        bool initialized=false,stopping=false;
        Module(Impl& host,std::string mod) :host(host),mod(std::move(mod)) {}
        ~Module() {
            stopping=true;
            if(initialized && plugin.shutdown) try { plugin.shutdown(plugin.user); } catch(...) { LOG_ERROR("NATIVE","Exception during native shutdown: "+mod); }
            for(auto token:tokens) host.runtime.unsubscribe(token);
            if(library) FreeLibrary(library);
        }
        void check() const { if(std::this_thread::get_id()!=thread) throw std::runtime_error("Native API is main-thread only"); }
        void own(const AssetId& id) const { if(id.nameSpace()!=mod) throw std::runtime_error("Component registration requires own namespace"); }
    };
    std::vector<std::unique_ptr<Module>> modules;
    using Json=nlohmann::json;
    Impl(script::Runtime& runtime,World& world,Components& components,SaveGame& saves):runtime(runtime),world(world),components(components),saves(saves) {}
    ~Impl() { while(!modules.empty()) modules.pop_back(); }
    static Data payload(const script::Value& value,World& world,unsigned depth=0) {
        if(depth>64) throw std::runtime_error("Native payload too deeply nested");
        if(std::holds_alternative<std::shared_ptr<script::HostObject>>(value.data))
            return Data{Data::Object{{"$entity",Data{std::to_string(requireEntity(value,world,{}).value)}}}};
        if(auto p=std::get_if<std::shared_ptr<script::List>>(&value.data)) { Data::Array result; for(const auto& v:(*p)->values) result.push_back(payload(v,world,depth+1)); return Data{std::move(result)}; }
        if(auto p=std::get_if<std::shared_ptr<script::Object>>(&value.data)) { Data::Object result; for(const auto& [k,v]:(*p)->fields) result[k]=payload(v,world,depth+1); return Data{std::move(result)}; }
        return script::toData(value);
    }
    static script::Value restorePayload(const Data& value,World& world) {
        if(auto p=std::get_if<Data::Object>(&value.value)) {
            if(p->size()==1 && p->contains("$entity")) { Json j={{"entity",std::get<std::string>(p->at("$entity").value)}}; return entityValue(world,entity(j,world)); }
            auto result=script::object(); for(const auto& [k,v]:*p) result.setMember(k,restorePayload(v,world),{}); return result;
        }
        if(auto p=std::get_if<Data::Array>(&value.value)) { std::vector<script::Value> result; for(const auto& v:*p) result.push_back(restorePayload(v,world)); return script::list(std::move(result)); }
        return script::fromData(value);
    }
    static EntityId entity(const Json& j,World& world) {
        const auto text=j.at("entity").get<std::string>(); size_t end=0; const auto value=std::stoull(text,&end);
        EntityId id{value}; if(end!=text.size() || !world.alive(id)) throw std::runtime_error("Invalid entity"); return id;
    }
    static int SB_CALL call(void* context,const char* operation,const char* input,char* output,uint32_t capacity,uint32_t* required) {
        try {
            auto& m=*static_cast<Module*>(context); m.check(); if(!operation || !input) return SB_ERROR;
            (void)decodeData(input); const auto args=Json::parse(input); Json result=nullptr; auto& h=m.host; const std::string op(operation);
            const bool readOnly=op=="component.get" || op=="state.get" || op=="entity.find" || op=="engine.info" || op=="engine.configuration";
            if(!readOnly && (!output || capacity<32)) { if(required) *required=32; return SB_BUFFER_TOO_SMALL; }
            if(op=="log") LOG_INFO("NATIVE",m.mod+": "+args.at("message").get<std::string>());
            else if(op=="component.define") { AssetId id(args.at("type").get<std::string>()); m.own(id); const auto v=args.at("version").get<double>(); if(v<1 || v>1000000 || v!=std::floor(v)) throw std::runtime_error("Invalid version"); h.components.define(id,static_cast<unsigned>(v),decodeData(args.at("defaults").dump())); }
            else if(op=="entity.find") result=std::to_string(h.world.find(args.at("name").get<std::string>()).value);
            else if(op=="entity.create") {
                const auto created=h.world.create(args.at("name").get<std::string>());
                h.world.get<IdentityComponent>(created).owner=m.mod; result=std::to_string(created.value);
            }
            else if(op=="entity.destroy") h.world.destroy(entity(args,h.world));
            else if(op=="component.add") h.components.add(h.world,entity(args,h.world),AssetId(args.at("type").get<std::string>()));
            else if(op=="component.get") {
                const auto data=h.components.get(h.world,entity(args,h.world),AssetId(args.at("type").get<std::string>()));
                result=Json::parse(encodeData(data));
            }
            else if(op=="component.set") h.components.set(h.world,entity(args,h.world),AssetId(args.at("type").get<std::string>()),decodeData(args.at("data").dump()));
            else if(op=="component.remove") result=h.world.removeCustom(entity(args,h.world),AssetId(args.at("type").get<std::string>()));
            else if(op=="state.get") result=Json::parse(encodeData(h.saves.state(m.mod)));
            else if(op=="state.set") h.saves.setState(m.mod,decodeData(args.dump()));
            else if(op.starts_with("engine.") && !m.stopping && h.owner->engineOperation) result=Json::parse(encodeData(h.owner->engineOperation(m.mod,op,decodeData(input))));
            else throw std::runtime_error("Unknown native operation");
            const auto text=result.dump(); if(required) *required=static_cast<uint32_t>(text.size()+1);
            if(!output || capacity<text.size()+1) return SB_BUFFER_TOO_SMALL;
            std::memcpy(output,text.c_str(),text.size()+1); return SB_OK;
        } catch(const std::exception& e) { LOG_ERROR("NATIVE",e.what()); return SB_ERROR; }
        catch(...) { return SB_ERROR; }
    }
    static int SB_CALL subscribe(void* context,const char* event,SbCallback callback,void* user,int hook,uint64_t* token) {
        try {
            auto& m=*static_cast<Module*>(context); m.check(); if(m.stopping || !event || !callback || !token) return SB_ERROR;
            const auto id=m.host.runtime.subscribe(event,script::native([&m,callback,user,hook](const auto& args,const auto&) {
                const auto input=encodeData(payload(hook ? args.at(0) : script::list(args),m.host.world));
                std::vector<char> output(1024*1024,0);
                if(callback(user,input.c_str(),output.data(),static_cast<uint32_t>(output.size()))!=SB_OK) throw DiagnosticError({"NATIVE104","Native callback failed",{},m.mod});
                if(!hook) return script::Value{};
                if(!std::memchr(output.data(),0,output.size())) throw DiagnosticError({"NATIVE104","Native output lacks terminator"});
                return restorePayload(decodeData(output.data()),m.host.world);
            }),hook!=0,m.mod);
            m.tokens.insert(id); *token=id; return SB_OK;
        } catch(...) { return SB_ERROR; }
    }
    static int SB_CALL unsubscribe(void* context,uint64_t token) {
        try { auto& m=*static_cast<Module*>(context); m.check(); if(!m.tokens.erase(token)) return SB_ERROR; m.host.runtime.unsubscribe(token); return SB_OK; }
        catch(...) { return SB_ERROR; }
    }
};
NativeMods::NativeMods(script::Runtime& r,World& w,Components& c,SaveGame& s):impl_(std::make_unique<Impl>(r,w,c,s)) {impl_->owner=this;}
NativeMods::~NativeMods()=default;
void NativeMods::load(const std::string& mod,const std::filesystem::path& root,const std::filesystem::path& relative) {
    auto path=VirtualFileSystem::contained(root,relative);
    if(path.extension()!=L".dll") throw DiagnosticError({"NATIVE101","Native module must be a .dll",path.string(),mod});
    for(const auto& prior:impl_->modules) if(prior->mod==mod) throw DiagnosticError({"NATIVE101","Native module already loaded",path.string(),mod});
    auto module=std::make_unique<Impl::Module>(*impl_,mod);
    module->library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module->library) throw DiagnosticError({"NATIVE102","Cannot load x64 DLL; Win32 error "+std::to_string(GetLastError()),path.string(),mod});
    auto init=reinterpret_cast<SbInit>(GetProcAddress(module->library,"sb_mod_init"));
    if(!init) throw DiagnosticError({"NATIVE103","Missing sb_mod_init export",path.string(),mod});
    module->api={sizeof(SbHost),SB_ABI_VERSION,module.get(),&Impl::call,&Impl::subscribe,&Impl::unsubscribe};
    module->plugin={sizeof(SbPlugin),SB_ABI_VERSION,nullptr,nullptr};
    if(init(&module->api,&module->plugin)!=SB_OK || module->plugin.size!=sizeof(SbPlugin) || module->plugin.abi!=SB_ABI_VERSION)
        throw DiagnosticError({"NATIVE103","Native initialization or ABI negotiation failed",path.string(),mod});
    module->initialized=true; impl_->modules.push_back(std::move(module));
}
}
