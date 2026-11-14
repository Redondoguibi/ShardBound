#include "assets/StudioPackage.h"
#include "assets/AssetManager.h"
#include "assets/ModelCompiler.h"
#include "core/Diagnostic.h"
#include "core/PathText.h"
#include <fstream>
#include <set>
#include <cmath>
#include <regex>
#include "audio/AudioSystem.h"
namespace game {
using Json=nlohmann::json;
namespace {
void need(bool valid,const std::string& message) { if(!valid) throw DiagnosticError({"STUDIO101",message}); }
Json read(const std::filesystem::path& file,const std::string& schema,const std::string& project="") {
    need(std::filesystem::file_size(file)<=64*1024*1024,"Studio document exceeds 64 MiB: "+pathText(file));
    std::ifstream stream(file); auto j=Json::parse(stream);
    need(j.at("schema")==schema && j.at("version")==1,"Unsupported Studio schema/version: "+pathText(file));
    const auto id=j.at("project").get<std::string>(); (void)AssetId(id); need(project.empty() || id==project,"Companion belongs to another project"); return j;
}
double number(const Json& j,const char* key,double lo,double hi) { const auto x=j.at(key).get<double>(); need(std::isfinite(x)&&x>=lo&&x<=hi,std::string("Invalid Studio ")+key); return x; }
void vector(const Json& j,size_t count=3) { need(j.is_array()&&j.size()==count,"Invalid Studio vector"); for(const auto& v:j) need(v.is_number()&&std::isfinite(v.get<double>())&&std::abs(v.get<double>())<=1e9,"Invalid Studio vector value"); }
bool color(const Json& value) { return value.is_string() && std::regex_match(value.get<std::string>(),std::regex("#[a-fA-F0-9]{6}")); }
void sample(const Json& channel) {
    const auto& times=channel.at("times"); need(times.is_array()&&!times.empty()&&times.size()<=240001,"Invalid player sample count"); double last=-1;
    for(const auto& t:times) { const auto x=t.get<double>(); need(std::isfinite(x)&&x>=0&&x>last&&x<=1000,"Invalid player sample time"); last=x; }
    for(size_t i=0;i<channel.at("quaternion").size();i+=4){double n=0;for(size_t j=0;j<4;j++){double x=channel["quaternion"].at(i+j);n+=x*x;}need(n>1e-12,"Zero player quaternion");}
    for(const auto* key:{"position","quaternion","scale"}) { const auto& v=channel.at(key); need(v.is_array()&&v.size()==times.size()*(std::string(key)=="quaternion" ? 4:3),"Invalid player sample values"); for(const auto& x:v) need(x.is_number()&&std::isfinite(x.get<double>()),"Non-finite player sample"); }
}
}
std::filesystem::path StudioPackage::file(const std::string& relative) const { return VirtualFileSystem::contained(root,relative); }
const Json* StudioPackage::clip(const std::string& name) const { for(const auto& c:events.at("clips")) if(c.at("name")==name) return &c; return nullptr; }
const Json* StudioPackage::effect(const std::string& id) const { for(const auto& e:effects.at("definitions")) if(e.at("id")==id) return &e; return nullptr; }
std::shared_ptr<StudioPackage> loadStudioPackage(const std::filesystem::path& manifest,const std::filesystem::path& cache,const std::string& ns) {
    auto p=std::make_shared<StudioPackage>(); p->root=std::filesystem::canonical(manifest).parent_path();
    p->manifest=read(manifest,"shardbound.studio.package"); const auto project=p->manifest.at("project").get<std::string>();
    auto own=[&](const std::string& id){const AssetId asset(id); need(ns.empty()||asset.nameSpace()==ns,"Studio IDs must use mod namespace "+ns+": "+id);}; own(project);
    number(p->manifest,"unitsPerMeter",0.001,100000);
    p->effects=read(p->file(p->manifest.at("effects")),"shardbound.studio.effects",project);
    p->events=read(p->file(p->manifest.at("events")),"shardbound.studio.events",project);
    p->player=read(p->file(p->manifest.at("playerAnimations")),"shardbound.studio.player_animations",project);
    const auto& contract=p->effects.at("renderContract"); need(contract.at("version")==1,"Unsupported Studio render contract");
    for(const auto& [key,value]:std::map<std::string,std::string>{{"filtering","nearest"},{"colorSpace","srgb"},{"particleLighting","unlit"},{"transparency","alpha-or-additive"},{"gravitySpace","emitter-local"},{"particleMotion","analytic-linear-drag"},{"spriteOrientation","camera-facing"},{"trail","world-space-ribbon"},{"glow","additive-sprite-and-point-light"}}) need(contract.at(key)==value,"Unsupported render contract: "+key);
    std::set<std::string> ids,targets,modelIds,clipNames; std::map<std::string,std::string> kinds;
    need(p->manifest.at("models").is_array()&&!p->manifest["models"].empty()&&p->manifest["models"].size()<=256,"Invalid model list");
    for(const auto& m:p->manifest["models"]) {
        const auto id=m.at("id").get<std::string>(); own(id); need(modelIds.insert(id).second,"Duplicate model ID");
        need(std::set<std::string>{"player","item","melee","ranged","armor","monster","block","effect"}.contains(m.at("type").get<std::string>()),"Invalid content type");
        const auto data=compileModel(p->file(m.at("file")),cache); std::set<std::string> local;
        for(const auto& node:data.hierarchy.nodes) if(!node.uuid.empty()) { need(local.insert(node.uuid).second,"Duplicate node UUID in GLB"); targets.insert(node.uuid); }
        for(const auto& root:m.at("roots")) need(local.contains(root.get<std::string>()),"Model root missing in GLB");
        for(const auto& binding:m.at("bindings")) { need(local.contains(binding.at("group").get<std::string>())&&local.contains(binding.at("referenceGroup").get<std::string>()),"Equipment binding missing in GLB"); need(std::set<std::string>{"Head","Body","LeftArm","RightArm","LeftLeg","RightLeg"}.contains(binding.at("target").get<std::string>()),"Invalid equipment body part"); }
    }
    if(!p->player.at("context").is_null()) { const auto& context=p->player["context"];need(context.at("groups").is_object()&&context["groups"].size()==6,"Player context must define six body groups");for(const auto* part:{"Head","Body","LeftArm","RightArm","LeftLeg","RightLeg"})need(context["groups"].contains(part),"Missing player body group"); targets.insert(context.at("root").get<std::string>()); for(const auto& [name,id]:context.at("groups").items()) { (void)name; targets.insert(id.get<std::string>()); } }
    for(const auto& c:p->player.at("clips")) { need(c.at("name").is_string(),"Invalid player clip"); number(c,"length",0.001,1000); sample(c.at("root"));need(c.at("channels").is_object()&&c["channels"].size()==6,"Player clip must define six body channels");for(const auto* part:{"Head","Body","LeftArm","RightArm","LeftLeg","RightLeg"})need(c["channels"].contains(part),"Missing player animation channel"); for(const auto& [part,ch]:c.at("channels").items()) { (void)part; sample(ch); } }
    need(p->events.at("locators").size()<=10000,"Too many locators");
    for(const auto& locator:p->events["locators"]) { targets.insert(locator.at("uuid").get<std::string>()); vector(locator.at("position")); vector(locator.at("rotation")); }
    need(p->effects.at("definitions").is_array()&&p->effects["definitions"].size()<=1024,"Invalid effects list");
    const std::map<std::string,std::pair<double,double>> ranges={{"duration",{.001,3600}},{"lifetime",{.001,120}},{"burst",{0,4096}},{"rate",{0,4096}},{"seed",{0,4294967295.0}},{"speed",{0,10000}},{"speedSpread",{0,10000}},{"spread",{0,180}},{"drag",{0,100}},{"size",{0,10000}},{"sizeEnd",{0,10000}},{"opacity",{0,1}},{"opacityEnd",{0,1}},{"rotation",{-36000,36000}},{"angularVelocity",{-36000,36000}},{"columns",{1,256}},{"rows",{1,256}},{"frames",{1,65536}},{"fps",{0,240}},{"trailLength",{.001,30}},{"trailSamples",{2,240}},{"width",{.001,10000}},{"intensity",{0,10000}},{"distance",{0,10000}}};
    for(const auto& e:p->effects["definitions"]) {
        const auto id=e.at("id").get<std::string>(),kind=e.at("kind").get<std::string>(); (void)AssetId(id); need(ids.insert(id).second,"Duplicate effect/sound ID"); need(std::set<std::string>{"particle","trail","flash","glow"}.contains(kind),"Unknown effect kind"); kinds[id]=kind;
        for(const auto& [key,r]:ranges) number(e,key.c_str(),r.first,r.second);
        for(const auto* key:{"burst","seed","columns","rows","frames","trailSamples"}) { const double x=e.at(key); need(x==std::floor(x),"Integer effect property required"); }
        for(const auto* key:{"shapeSize","direction","gravity","startOffset","endOffset"}) vector(e.at(key));
        for(const auto& v:e["shapeSize"]) need(v.get<double>()>=0,"Negative emitter size");
        need(color(e.at("color"))&&color(e.at("colorEnd")),"Invalid effect color"); need(e.at("follow").is_boolean(),"Invalid follow flag");
        need(std::set<std::string>{"point","box","sphere","cone"}.contains(e.at("shape").get<std::string>()),"Invalid emitter shape"); need(e.at("blend")=="alpha"||e.at("blend")=="additive","Invalid effect blend");
        need(e["frames"].get<int>()<=e["columns"].get<int>()*e["rows"].get<int>(),"Atlas frame outside grid");
        need(kind!="particle" || e["burst"].get<double>()+std::ceil(e["rate"].get<double>()*std::min(e["lifetime"].get<double>(),e["duration"].get<double>()))<=4096,"Particle budget exceeded");
        need(e.at("curves").is_object(),"Invalid curves");
        for(const auto& [name,points]:e["curves"].items()) { need(std::set<std::string>{"size","opacity","rotation","color"}.contains(name)&&points.is_array()&&points.size()>=2&&points.size()<=64,"Invalid effect curve"); std::set<double> ages;
            for(const auto& point:points) { need(ages.insert(number(point,"age",0,1)).second,"Duplicate curve age"); if(name=="color") need(color(point.at("value")),"Invalid color curve"); else number(point,"value",name=="rotation" ? -1e9:0,name=="opacity" ? 1:1e9); }
        }
        const auto texture=e.at("texture").get<std::string>(); if(!texture.empty()) { auto t=compileTexture(p->file(texture),cache,true,true); need(t.width% e["columns"].get<unsigned>()==0&&t.height%e["rows"].get<unsigned>()==0,"Sprite dimensions incompatible with atlas"); }
    }
    for(const auto& s:p->events.at("sounds")) { const auto id=s.at("id").get<std::string>(); own(id); need(ids.insert(id).second,"Duplicate sound/effect ID"); kinds[id]="sound"; number(s,"duration",.000001,86400); const auto file=p->file(s.at("file")); need(std::filesystem::file_size(file)<=24*1024*1024,"Audio exceeds 24 MiB");validateAudio(file); }
    need(p->events.at("clips").is_array()&&p->events["clips"].size()<=1024,"Invalid clip list");
    need(p->events.at("sounds").is_array()&&p->events["sounds"].size()<=1024,"Invalid sound list");
    need(p->manifest["projectileVisual"].is_null()||p->manifest["projectileVisual"].is_string(),"Invalid projectile visual");if(p->manifest["projectileVisual"].is_string())(void)AssetId(p->manifest["projectileVisual"].get<std::string>());
    size_t eventCount=0;
    for(const auto& c:p->events.at("clips")) {
        need(clipNames.insert(c.at("name").get<std::string>()).second,"Duplicate clip"); const auto length=number(c,"length",.001,1000); number(c,"fps",1,120); need(c["loop"]=="once"||c["loop"]=="loop"||c["loop"]=="hold","Invalid clip loop");
        std::set<std::string> eventIds; eventCount+=c.at("events").size(); need(eventCount<=100000,"Too many Studio events");
        for(const auto& e:c["events"]) { need(eventIds.insert(e.at("id").get<std::string>()).second,"Duplicate event ID"); number(e,"time",0,length); number(e,"order",0,1e9); number(e,"volume",0,1); number(e,"pitch",.1,4); vector(e.at("offset"));
            const auto resource=e.at("resource").get<std::string>(),target=e.at("target").get<std::string>(); need(kinds.contains(resource)&&kinds.at(resource)==e.at("type").get<std::string>(),"Missing/incompatible event resource"); need(target.empty()||targets.contains(target),"Missing event target"); need(e["action"]=="start"||e["action"]=="stop"||e["action"]=="burst","Invalid event action"); need(e.at("follow").is_boolean()&&e.at("loop").is_boolean()&&e.at("instance").is_string(),"Invalid event settings");
            need((e["type"]!="trail"&&e["type"]!="flash")||!target.empty(),"Trail/surface effect requires target");
        }
    }
    for(const auto& c:p->player["clips"])need(clipNames.contains(c["name"].get<std::string>()),"Player animation missing corresponding event clip");
    return p;
}
void AssetManager::registerStudioPackage(const AssetId& id,const std::filesystem::path& manifest) {
    const auto source=vfs_.resolve(id.nameSpace(),manifest); auto package=loadStudioPackage(source,cache_/id.nameSpace(),id.nameSpace());
    need(package->manifest.at("project")==id.str(),"Studio registry ID must equal exported project ID");
    for(const auto& m:package->manifest["models"]) need(!models_.contains(m.at("id").get<std::string>()),"Studio model already registered");
    for(const auto& s:package->events["sounds"]) need(!sounds_.contains(s.at("id").get<std::string>()),"Studio sound already registered");
    // All files and IDs are checked before publishing any resource.
    for(const auto& m:package->manifest["models"]) { const auto key=m.at("id").get<std::string>(); registerModel({AssetId(key),manifest.parent_path()/m.at("file").get<std::string>()}); studioModels_[key]=package; }
    for(const auto& s:package->events["sounds"]) registerSound({AssetId(s.at("id")),manifest.parent_path()/s.at("file").get<std::string>(),false});
}
}
