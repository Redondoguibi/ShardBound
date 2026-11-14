#include "scripting/EngineBridge.h"
#include "scripting/DataConversion.h"
#include "assets/AssetId.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
namespace game {
namespace {
using Json=nlohmann::json;
EnginePipeline::Options options(const Data& input) {
    const auto j=Json::parse(encodeData(input));if(!j.is_object()) throw std::runtime_error("Engine options must be an object");EnginePipeline::Options out;
    for(const auto& [key,value]:j.items()) {
        if(key=="mode") out.mode=value.get<std::string>();else if(key=="replaces") out.replaces=value.get<std::string>();
        else if(key=="api" || key=="priority") {if(!value.is_number() || value.get<double>()!=std::floor(value.get<double>()) || std::abs(value.get<double>())>10000) throw std::runtime_error("Engine option must be an integer");if(key=="api") out.api=value.get<int>();else out.priority=value.get<int>();}
        else throw std::runtime_error("Unknown engine option: "+key);
    }return out;
}
bool handled(const script::Value& value,const std::string& mode) {
    if(mode!="replace" || std::holds_alternative<std::monostate>(value.data)) return false;
    return value.boolean({});
}
void arity(size_t count,size_t low,size_t high) {if(count<low || count>high) throw std::runtime_error("Invalid engine API argument count");}
}
EngineBridge::EngineBridge(script::Runtime& runtime):runtime_(runtime) {
    using namespace script;
    pipeline.depends=[&runtime](const auto& owner,const auto& dependency){return runtime.registry().dependsOn(owner,dependency);};
    runtime.bind("engine_info",native([this](const auto& args,const auto&){arity(args.size(),0,0);return fromData(pipeline.inspect());}));
    runtime.bind("register_engine_extension",native([this](const auto& args,const auto& at){
        arity(args.size(),3,4);const auto owner=runtime_.currentMod();const auto callback=args[2];
        if(!std::holds_alternative<std::shared_ptr<Callable>>(callback.data)) fail(at,"ENGINE101","Engine callback must be a function");
        if(std::get<std::shared_ptr<Callable>>(callback.data)->asynchronous) fail(at,"SCRIPT340","Engine phases require synchronous callbacks; start async work from them");
        auto opts=args.size()==4?options(toData(args[3])):EnginePipeline::Options{};const auto mode=opts.mode;
        pipeline.add(owner,args[0].string(at),args[1].string(at),[this,callback,mode](const Data& context){return handled(runtime_.invoke(callback,{fromData(context)}),mode);},std::move(opts));return Value{};
    }));
    runtime.bind("unregister_engine_extension",native([this](const auto& args,const auto& at){arity(args.size(),1,1);pipeline.remove(runtime_.currentMod(),args[0].string(at));return Value{};}));
    runtime.bind("engine_extension_enabled",native([this](const auto& args,const auto& at){arity(args.size(),2,2);pipeline.enabled(runtime_.currentMod(),args[0].string(at),args[1].boolean(at));return Value{};}));
    runtime.bind("engine_default",native([this](const auto& args,const auto&){arity(args.size(),0,1);const auto data=args.empty()?Data{Data::Object{}}:toData(args[0]);const auto* object=std::get_if<Data::Object>(&data.value);if(!object) throw std::runtime_error("engine_default expects an object");pipeline.runDefault(runtime_.currentMod(),*object);return Value{};}));
    runtime.bind("configure_engine",native([this](const auto& args,const auto& at){arity(args.size(),2,2);configure(runtime_.currentMod(),args[0].string(at),toData(args[1]));return Value{};}));
    runtime.bind("engine_configuration",native([this](const auto& args,const auto&){arity(args.size(),0,0);if(!service) throw std::runtime_error("Engine service unavailable");return fromData(service("configuration",{}));}));
}
void EngineBridge::configure(const std::string& owner,const std::string& name,const Data& input) {
    if(name!="physics" && name!="render" && name!="scheduler") throw std::runtime_error("Unknown configurable engine service: "+name);
    if(!service || !validNamespace(owner)) throw std::runtime_error("Engine service or caller unavailable");
    const auto* fields=std::get_if<Data::Object>(&input.value);if(!fields || fields->empty()) throw std::runtime_error("Engine configuration must be a nonempty object");auto writes=writes_;
    std::vector<std::pair<std::string,Data>> leaves;std::function<void(const Data&,const std::string&)> flatten=[&](const Data& data,const std::string& path) {if(const auto* nested=std::get_if<Data::Object>(&data.value)) {for(const auto& [key,value]:*nested) flatten(value,path+"/"+key);}else leaves.emplace_back(path,data);};
    for(const auto& [key,value]:*fields) flatten(value,name+"/"+key);
    for(const auto& [key,value]:leaves) {
        auto& writers=writes[key];
        for(const auto& [mod,previous]:writers) if(mod!=owner && previous!=value && !runtime_.registry().dependsOn(owner,mod)) throw std::runtime_error("Conflicting engine configuration: "+owner+" and "+mod+" at "+key);
        std::erase_if(writers,[&](const auto& writer){return writer.first==owner;});writers.emplace_back(owner,value);
    }
    service(name,input);writes_=std::move(writes);
}
Data EngineBridge::request(const std::string& owner,const std::string& operation,const Data& input) {
    const auto j=Json::parse(encodeData(input));
    if(operation=="engine.info") return pipeline.inspect();
    if(operation=="engine.default") {const auto* object=std::get_if<Data::Object>(&input.value);if(!object) throw std::runtime_error("Engine default request must be an object");pipeline.runDefault(owner,*object);}
    else if(operation=="engine.remove") pipeline.remove(owner,j.at("id").get<std::string>());
    else if(operation=="engine.enabled") pipeline.enabled(owner,j.at("id").get<std::string>(),j.at("enabled").get<bool>());
    else if(operation=="engine.configure") configure(owner,j.at("service"),decodeData(j.at("options").dump()));
    else if(operation=="engine.configuration") {if(!service) throw std::runtime_error("Engine service unavailable");return service("configuration",{});}
    else if(operation=="engine.register") {
        const auto handler=j.at("handler").get<std::string>();if(AssetId(handler).nameSpace()!=owner) throw std::runtime_error("Native engine handler requires own namespace");
        auto opts=options(decodeData(j.value("options",Json::object()).dump()));const auto mode=opts.mode;
        pipeline.add(owner,j.at("id"),j.at("stage"),[this,handler,mode](const Data& context){return handled(runtime_.filter(handler,script::fromData(context)),mode);},std::move(opts));
    } else throw std::runtime_error("Unknown engine operation");return {};
}
}
