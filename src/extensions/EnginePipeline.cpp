#include "extensions/EnginePipeline.h"
#include "assets/AssetId.h"
#include "core/Diagnostic.h"
#include "core/Log.h"
#include <algorithm>
#include <chrono>
namespace game {
namespace {
[[noreturn]] void invalid(const std::string& message) {throw DiagnosticError({"ENGINE101",message});}
Data string(const std::string& value) {return Data{value};}
}
const std::vector<std::string>& EnginePipeline::stages() {static const std::vector<std::string> names={"fixed","movement","physics","update","streaming","render"};return names;}
EnginePipeline::Entry& EnginePipeline::owned(const std::string& owner,const std::string& id) {
    const auto found=entries_.find(id);if(found==entries_.end() || found->second.owner!=owner) invalid("Engine extension does not belong to caller: "+id);return found->second;
}
void EnginePipeline::add(std::string owner,std::string id,std::string stage,Callback callback,Options options) {
    if(!validNamespace(owner) || AssetId(id).nameSpace()!=owner || !callback) invalid("Engine extension requires its owner's namespace and callback");
    if(entries_.contains(id)) invalid("Duplicate engine extension: "+id);
    if(std::find(stages().begin(),stages().end(),stage)==stages().end() || options.api!=1) invalid("Unknown engine stage or unsupported API version");
    if(options.mode!="before" && options.mode!="after" && options.mode!="replace") invalid("Engine extension mode must be before, after or replace");
    if(options.priority< -100 || options.priority>100) invalid("Engine priority must be in [-100,100]");
    if(entries_.size()>=256 || std::count_if(entries_.begin(),entries_.end(),[&](const auto& pair){return pair.second.owner==owner;})>=32) invalid("Engine extension limit exceeded");
    auto chain=replacements_[stage];
    if(options.mode=="replace") {
        std::string prior="core:default";for(auto i=chain.rbegin();i!=chain.rend();++i) if(entries_.at(*i).enabled) {prior=*i;break;}
        if(options.replaces!=prior) invalid("Replacement must explicitly name current provider: "+prior);
        if(prior!="core:default") {const auto& old=entries_.at(prior);if(old.owner!=owner && (!depends || !depends(owner,old.owner))) invalid("Declare a dependency before replacing "+prior);}
        chain.push_back(id);
    } else if(!options.replaces.empty()) invalid("Only replacements may specify replaces");
    Entry entry{owner,id,stage,std::move(callback),std::move(options)};entry.generation=++sequence_;
    entries_.emplace(id,std::move(entry));replacements_[stage]=std::move(chain);
}
void EnginePipeline::remove(const std::string& owner,const std::string& id) {const auto stage=owned(owner,id).stage;entries_.erase(id);std::erase(replacements_[stage],id);}
void EnginePipeline::enabled(const std::string& owner,const std::string& id,bool value) {owned(owner,id).enabled=value;}
void EnginePipeline::invokeDefault(Frame& frame,const Data::Object& overrides) {
    if(frame.started) invalid("Default engine stage may execute only once");
    auto parameters=frame.parameters;
    for(const auto& [key,value]:overrides) {if(key=="stage" || key=="api" || key=="player_key" || !parameters.contains(key)) invalid("Unknown or read-only engine parameter: "+key);parameters[key]=value;}
    // Validate overrides before marking the default as started or mutating the engine.
    Data input{std::move(parameters)};if(frame.validate) frame.validate(input);
    frame.started=true;try {frame.implementation(input);}catch(...) {frame.failure=std::current_exception();throw;}
}
void EnginePipeline::runDefault(const std::string& owner,const Data::Object& overrides) {
    if(!frame_ || frame_->owner!=owner || frame_->mode!="replace") invalid("engine_default is only available inside the active replacement");invokeDefault(*frame_,overrides);
}
void EnginePipeline::run(const std::string& stage,Data::Object parameters,const Default& implementation,const Default& validate) {
    if(std::find(stages().begin(),stages().end(),stage)==stages().end() || !implementation) invalid("Invalid engine dispatch");
    if(frame_) invalid("Recursive engine dispatch is forbidden");
    parameters["stage"]=string(stage);parameters["api"]=Data{1.0};
    Frame frame{stage,{},{},std::move(parameters),implementation,validate};frame_=&frame;struct Reset {Frame*& pointer;~Reset(){pointer=nullptr;}} reset{frame_};
    std::vector<Entry> before,after;std::optional<Entry> replacement;
    for(const auto& [id,entry]:entries_) { (void)id;if(entry.stage==stage && entry.enabled) {if(entry.options.mode=="before") before.push_back(entry);else if(entry.options.mode=="after") after.push_back(entry);}}
    if(const auto found=replacements_.find(stage);found!=replacements_.end()) for(auto i=found->second.rbegin();i!=found->second.rend();++i) if(entries_.at(*i).enabled) {replacement=entries_.at(*i);break;}
    const auto order=[](const Entry& a,const Entry& b){return a.options.priority==b.options.priority?a.id<b.id:a.options.priority<b.options.priority;};std::sort(before.begin(),before.end(),order);std::sort(after.begin(),after.end(),order);
    const auto invoke=[&](const Entry& entry) {
        frame.owner=entry.owner;frame.mode=entry.options.mode;const auto start=std::chrono::steady_clock::now();bool handled=false,failed=false;std::string error;
        try {handled=entry.callback(Data{frame.parameters});}catch(const std::exception& failure) {failed=true;error=failure.what();if(error.empty()) error="Engine extension failed without a message";}catch(...) {failed=true;error="Unknown engine extension exception";}
        const auto cost=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(auto found=entries_.find(entry.id);found!=entries_.end() && found->second.generation==entry.generation) {auto& current=found->second;++current.calls;current.last=cost;current.peak=std::max(current.peak,cost);current.total+=cost;if(failed) {++current.errors;current.enabled=false;}}
        if(failed) {if(diagnostics_.size()==64) diagnostics_.pop_front();diagnostics_.push_back(Data{Data::Object{{"extension",string(entry.id)},{"stage",string(stage)},{"message",string(error)}}});LOG_ERROR("ENGINE",entry.id+": "+error);handled=false;}
        frame.owner.clear();frame.mode.clear();return handled;
    };
    for(const auto& entry:before) invoke(entry);
    const bool handled=replacement && invoke(*replacement);
    if(!handled && !frame.started) invokeDefault(frame,{});
    if(frame.failure) std::rethrow_exception(frame.failure);
    for(const auto& entry:after) invoke(entry);
}
Data EnginePipeline::inspect() const {
    Data::Array slots,extensions,errors(diagnostics_.begin(),diagnostics_.end());
    for(const auto& stage:stages()) {std::string provider="core:default";if(const auto found=replacements_.find(stage);found!=replacements_.end()) for(auto i=found->second.rbegin();i!=found->second.rend();++i) if(entries_.at(*i).enabled) {provider=*i;break;}slots.push_back(Data{Data::Object{{"stage",string(stage)},{"api",Data{1.0}},{"provider",string(provider)}}});}
    for(const auto& [id,entry]:entries_) extensions.push_back(Data{Data::Object{{"id",string(id)},{"owner",string(entry.owner)},{"stage",string(entry.stage)},{"mode",string(entry.options.mode)},{"enabled",Data{entry.enabled}},{"priority",Data{double(entry.options.priority)}},{"calls",Data{double(entry.calls)}},{"errors",Data{double(entry.errors)}},{"last_ms",Data{entry.last}},{"peak_ms",Data{entry.peak}},{"total_ms",Data{entry.total}}}});
    return Data{Data::Object{{"api",Data{1.0}},{"stages",Data{std::move(slots)}},{"extensions",Data{std::move(extensions)}},{"diagnostics",Data{std::move(errors)}}}};
}
}
