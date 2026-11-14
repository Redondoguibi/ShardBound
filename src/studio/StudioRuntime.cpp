#include "studio/StudioRuntime.h"
#include "studio/StudioEffects.h"
#include "assets/TextureCompiler.h"
#include "core/Diagnostic.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <set>
#include <numbers>
#include <fstream>
#include <DirectXMath.h>
namespace game {
namespace {
using Json=nlohmann::json;
void require(bool ok,const std::string& message) { if(!ok) throw DiagnosticError({"STUDIO201",message}); }
Vec3 add(Vec3 a,Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};} Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};} Vec3 scale(Vec3 a,float x){return {a.x*x,a.y*x,a.z*x};}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};} Vec3 unit(Vec3 a){const auto n=std::hypot(a.x,a.y,a.z);return n>1e-6f?scale(a,1/n):Vec3{1,0,0};}
Matrix scaling(float x){return transformMatrix({{},{},{x,x,x}});}
size_t nodeIndex(const ModelResource& model,const std::string& uuid) {for(size_t i=0;i<model.hierarchy.nodes.size();i++)if(model.hierarchy.nodes[i].uuid==uuid)return i;return size_t(-1);}
size_t bodyNode(const ModelResource& model,const std::string& name) {
    // Meshes may share names with groups; select the actual parent group.
    for(size_t i=0;i<model.hierarchy.nodes.size();i++) if(model.hierarchy.nodes[i].name==name && std::any_of(model.hierarchy.nodes.begin(),model.hierarchy.nodes.end(),[&](const auto& n){return n.parent==int(i);})) return i;
    return size_t(-1);
}
Matrix playerSample(const Json& ch,double time,float units,const Matrix& rest,bool nativeMetres) {
    std::array<float,4> p{},q{},s{};
    for(auto [name,path,size,out]:{std::tuple{"position",1,3,&p},std::tuple{"quaternion",2,4,&q},std::tuple{"scale",3,3,&s}}) {
        ModelTrack t;t.path=path;t.times=ch.at("times").get<std::vector<float>>();const auto values=ch.at(name).get<std::vector<float>>();
        for(size_t i=0;i<t.times.size();i++){std::array<float,4> v{};for(int j=0;j<size;j++)v[size_t(j)]=values[i*size+size_t(j)];t.values.push_back(v);} *out=sampleTrack(t,float(time));
    }
    Vec3 position{p[0],p[1],p[2]};
    if(nativeMetres) { const auto& first=ch["position"];position={rest.values[12]+(p[0]-first[0].get<float>())/units,rest.values[13]+(p[1]-first[1].get<float>())/units,rest.values[14]+(p[2]-first[2].get<float>())/units}; }
    return composeTRS(position,q,{s[0],s[1],s[2]});
}
}
struct StudioRuntime::Impl {
    World& world;Renderer& renderer;AudioSystem& audio; bool suspended=false;StudioStats stats;
    struct Voice {std::uint64_t handle;std::string instance;bool loop;};
    struct State { ModelHandle model;std::string clip,mode="once";double cursor=0,length=0;float speed=1;bool paused=false,initial=true,finished=false,inactive=false;std::vector<Voice> voices;std::deque<std::pair<double,Matrix>> history; };
    struct Binding {EntityId player;bool drive;ModelHandle equipmentModel,playerModel;};
    std::map<std::pair<EntityId,double>,std::vector<Matrix>> poseCache;std::map<EntityId,State> states;std::map<EntityId,Binding> bindings;std::map<std::string,TextureHandle> textures;std::set<std::string> usedTextures;TextureHandle radial;std::map<std::pair<EntityId,std::string>,MeshHandle> meshes;
    Impl(World& w,Renderer& r,AudioSystem& a):world(w),renderer(r),audio(a){}
    ModelComponent& visual(EntityId id){require(world.alive(id)&&world.has<ModelComponent>(id)&&world.get<ModelComponent>(id).model,"Expected entity with model");return world.get<ModelComponent>(id);}
    void clearVoices(State& s){for(auto v:s.voices)audio.stopVoice(v.handle);s.voices.clear();}
    const Json* clip(const State& s) const {return s.model->studio?s.model->studio->clip(s.clip):nullptr;}
    double modelTime(const State& s,double time)const{return std::clamp(time,0.,s.length);}
    std::map<size_t,Matrix> playerOverrides(EntityId parent,const State& driver,double at) {
        auto& body=visual(parent);std::map<size_t,Matrix> out;if(!driver.model->studio)return out;const auto& package=*driver.model->studio;
        for(const auto& c:package.player["clips"])if(c["name"]==driver.clip){
            const float units=package.manifest["unitsPerMeter"],ratio=body.model->studio?body.model->studio->manifest["unitsPerMeter"].get<float>()/units:1;
            for(const auto& [part,ch]:c["channels"].items()){const auto i=bodyNode(*body.model,part);require(i!=size_t(-1),"Player lacks group "+part);auto m=playerSample(ch,at,units,body.model->hierarchy.nodes[i].local,!body.model->studio);if(body.model->studio){m.values[12]*=ratio;m.values[13]*=ratio;m.values[14]*=ratio;}out[i]=m;}
            const auto bodyIndex=bodyNode(*body.model,"Body");if(bodyIndex!=size_t(-1)){const auto root=body.model->hierarchy.nodes[bodyIndex].parent;if(root>=0){auto m=playerSample(c["root"],at,units,body.model->hierarchy.nodes[size_t(root)].local,!body.model->studio);if(body.model->studio){m.values[12]*=ratio;m.values[13]*=ratio;m.values[14]*=ratio;}out[size_t(root)]=m;}}
        }return out;
    }
    const std::vector<Matrix>& pose(EntityId entity,const State& state,double at) {
        const auto key=std::pair{entity,at};if(poseCache.contains(key))return poseCache.at(key);
        std::map<size_t,Matrix> globals;
        if(auto b=bindings.find(entity);b!=bindings.end()) {
            auto& parent=visual(b->second.player);const auto parentState=states.find(b->second.player);
            std::map<size_t,Matrix> overrides;
            if(b->second.drive)overrides=playerOverrides(b->second.player,state,at);
            else for(const auto& [other,binding]:bindings) if(binding.player==b->second.player&&binding.drive&&states.contains(other)&&!states.at(other).inactive) {const auto& driver=states.at(other);overrides=playerOverrides(binding.player,driver,modelTime(driver,driver.cursor));break;}
            auto parentPose=sampleModel(parent.model->hierarchy,parentState==states.end()?"":parentState->second.clip,float(parentState==states.end()?0:modelTime(parentState->second,parentState->second.cursor)),overrides);
            if(overrides.empty()&&(parentState==states.end()||parentState->second.clip.empty()))for(size_t i=0;i<parentPose.size();i++)for(const auto& [name,matrix]:parent.poses){int n=int(i);while(n>=0&&parent.model->hierarchy.nodes[size_t(n)].name!=name)n=parent.model->hierarchy.nodes[size_t(n)].parent;if(n>=0){parentPose[i]=multiply(parentPose[i],matrix);break;}}
            const auto& package=*state.model->studio;const float ratio=(parent.model->studio?parent.model->studio->manifest["unitsPerMeter"].get<float>():1)/package.manifest["unitsPerMeter"].get<float>();
            const auto& context=package.player["context"];
            for(const auto& [part,uuid]:context["groups"].items()) {const auto target=nodeIndex(*state.model,uuid),source=bodyNode(*parent.model,part);if(target!=size_t(-1)&&source!=size_t(-1)) globals[target]=multiply(scaling(ratio),parentPose[source]);}
        }
        return poseCache.emplace(key,sampleModel(state.model->hierarchy,state.clip,float(modelTime(state,at)),{},globals)).first->second;
    }
    Matrix entityAt(EntityId id,const State& s,double at) {
        if(s.history.empty()||at>=s.cursor)return world.worldMatrix(id);
        auto upper=std::upper_bound(s.history.begin(),s.history.end(),at,[](double t,const auto& a){return t<a.first;});
        if(upper==s.history.begin())return upper->second;if(upper==s.history.end())return s.history.back().second;
        const auto& a=*(upper-1);const auto& b=*upper;const float t=float((at-a.first)/(b.first-a.first));using namespace DirectX; XMFLOAT4X4 am,bm;std::copy(a.second.values.begin(),a.second.values.end(),&am.m[0][0]);std::copy(b.second.values.begin(),b.second.values.end(),&bm.m[0][0]);XMVECTOR as,aq,ap,bs,bq,bp;
        if(!XMMatrixDecompose(&as,&aq,&ap,XMLoadFloat4x4(&am))||!XMMatrixDecompose(&bs,&bq,&bp,XMLoadFloat4x4(&bm)))return a.second;
        XMFLOAT4X4 result;XMStoreFloat4x4(&result,XMMatrixAffineTransformation(XMVectorLerp(as,bs,t),XMVectorZero(),XMQuaternionSlerp(aq,bq,t),XMVectorLerp(ap,bp,t)));Matrix out;std::copy(&result.m[0][0],&result.m[0][0]+16,out.values.begin());return out;
    }
    size_t contextNode(EntityId id,const State& s,const std::string& target) {
        auto b=bindings.find(id);if(b==bindings.end()||!s.model->studio)return size_t(-1);const auto& context=s.model->studio->player["context"];if(context.is_null())return size_t(-1);const auto& model=*visual(b->second.player).model;
        for(const auto& [part,uuid]:context["groups"].items())if(uuid==target)return bodyNode(model,part);
        if(context["root"]==target){const auto body=bodyNode(model,"Body");if(body!=size_t(-1)&&model.hierarchy.nodes[body].parent>=0)return size_t(model.hierarchy.nodes[body].parent);}return size_t(-1);
    }
    std::vector<Matrix> contextPose(EntityId id,const State& s,double at) {
        const auto parent=bindings.at(id).player;auto& body=visual(parent);const auto st=states.find(parent);auto override=bindings.at(id).drive?playerOverrides(parent,s,modelTime(s,at)):std::map<size_t,Matrix>{};
        auto values=sampleModel(body.model->hierarchy,st==states.end()?"":st->second.clip,float(st==states.end()?0:modelTime(st->second,at)),override);
        if(override.empty()&&st==states.end())for(size_t i=0;i<values.size();i++)for(const auto& [name,m]:body.poses){int n=int(i);while(n>=0&&body.model->hierarchy.nodes[size_t(n)].name!=name)n=body.model->hierarchy.nodes[size_t(n)].parent;if(n>=0){values[i]=multiply(values[i],m);break;}}return values;
    }
    Matrix parentAt(EntityId id,const State& s,double at) {return multiply(inverse(transformMatrix(world.get<TransformComponent>(id).local)),entityAt(id,s,at));}
    Matrix anchor(EntityId id,const State& s,const std::string& target,double at) {
        Matrix m=scaling(1/s.model->studio->manifest["unitsPerMeter"].get<float>());
        if(!target.empty()) {const auto i=nodeIndex(*s.model,target);if(i==size_t(-1)){const auto parentNode=contextNode(id,s,target);require(parentNode!=size_t(-1),"Effect target unavailable in this model or attached player");const auto& body=*visual(bindings.at(id).player).model;const float ratio=(body.studio?body.studio->manifest["unitsPerMeter"].get<float>():1)/s.model->studio->manifest["unitsPerMeter"].get<float>();return multiply(multiply(scaling(ratio),contextPose(id,s,at).at(parentNode)),parentAt(id,s,at));}m=pose(id,s,at).at(i);}
        return multiply(m,entityAt(id,s,at));
    }
    bool owns(const State& s,const Json& e)const {
        const auto target=e["target"].get<std::string>();
        if(!target.empty()) {const auto i=nodeIndex(*s.model,target);if(i!=size_t(-1)&&s.model->hierarchy.nodes[i].role!="reference_transform")return true;}
        // Project-wide events and shared reference-player events have one owner.
        return (target.empty()||(!s.model->studio->player["context"].is_null()&&([&]{const auto& context=s.model->studio->player["context"];if(context["root"]==target)return true;for(const auto& id:context["groups"])if(id==target)return true;return false;})()))&&s.model->studio->manifest["models"][0]["id"]==s.model->studioModelId;
    }
    void sounds(State& s,double from,double to,bool initial) {
        const auto* c=clip(s);if(!c)return;std::vector<const Json*> events;
        for(const auto& e:(*c)["events"])if(owns(s,e)&&e["type"]=="sound"&&(e["time"].get<double>()>from||(initial&&e["time"]==0))&&e["time"].get<double>()<=to)events.push_back(&e);
        std::stable_sort(events.begin(),events.end(),[](auto a,auto b){return std::tuple{a->at("time").template get<double>(),a->at("order").template get<int>(),a->at("id").template get<std::string>()}<std::tuple{b->at("time").template get<double>(),b->at("order").template get<int>(),b->at("id").template get<std::string>()};});
        for(const auto* ev:events){const auto& e=*ev;auto instance=e["instance"].get<std::string>();if(instance.empty())instance=e["resource"].get<std::string>()+"|"+e["target"].get<std::string>();
            if(e["action"]=="stop"||e["action"]=="start")std::erase_if(s.voices,[&](const auto& v){if(v.instance!=instance)return false;audio.stopVoice(v.handle);return true;});
            if(e["action"]=="stop")continue;
            for(const auto& resource:s.model->studio->events["sounds"])if(resource["id"]==e["resource"]){const bool loop=e["loop"];double offset=(to-e["time"].get<double>())*e["pitch"].get<double>();if(!loop&&offset>=resource["duration"].get<double>())offset=0;
                const auto voice=audio.playVoice(s.model->studio->file(resource["file"]),e["volume"],e["pitch"].get<float>()*s.speed,loop,offset);s.voices.push_back({voice,instance,loop});stats.soundsStarted++;break;}
        }
    }
    TextureHandle texture(const StudioPackage& p,const Json& effect) {
        const auto source=effect["texture"].get<std::string>();if(source.empty()){if(!radial){std::vector<std::uint8_t> pixels(32*32*4,255);for(int y=0;y<32;y++)for(int x=0;x<32;x++){const float r=std::hypot((x+.5f-16)/16,(y+.5f-16)/16);pixels[size_t(y*32+x)*4+3]=std::uint8_t(std::clamp((1-r)/.65f,0.f,1.f)*255);}radial=renderer.createTexture(32,32,pixels,true,true);}return radial;}
        const auto path=p.file(source);const auto key=path.string();usedTextures.insert(key);if(!textures.contains(key)){std::ifstream input(path,std::ios::binary);std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});auto data=decodeTexture(bytes.data(),bytes.size());textures[key]=renderer.createTexture(data.width,data.height,data.pixels,true,true);}return textures.at(key);
    }
};
StudioRuntime::StudioRuntime(World& w,Renderer& r,AudioSystem& a):impl_(std::make_unique<Impl>(w,r,a)){}
StudioRuntime::~StudioRuntime(){for(auto& [id,s]:impl_->states){(void)id;impl_->clearVoices(s);}}
bool StudioRuntime::hasClip(EntityId e,const std::string& name)const{auto& v=impl_->visual(e);if(v.model->studio&&v.model->studio->clip(name))return true;return std::any_of(v.model->hierarchy.clips.begin(),v.model->hierarchy.clips.end(),[&](const auto& c){return c.name==name;});}
std::string StudioRuntime::currentClip(EntityId e)const{auto it=impl_->states.find(e);return it==impl_->states.end()?"":it->second.clip;}
double StudioRuntime::time(EntityId e)const{auto it=impl_->states.find(e);return it==impl_->states.end()?0:it->second.cursor;}
StudioStats StudioRuntime::stats()const{return impl_->stats;}
void StudioRuntime::play(EntityId e,const std::string& name,const std::string& mode,float speed){auto& p=*impl_;p.poseCache.clear();auto& v=p.visual(e);require(hasClip(e,name),"Animation not found: "+name);require(std::isfinite(speed)&&speed>=.1f&&speed<=4,"Animation speed must be 0.1–4");
    const auto* c=v.model->studio?v.model->studio->clip(name):nullptr;double length=0;for(const auto& clip:v.model->hierarchy.clips)if(clip.name==name)length=clip.length;if(c)length=(*c)["length"];
    const std::string selected=mode=="authored"?(c?(*c)["loop"].get<std::string>():"once"):mode;require(selected=="once"||selected=="loop"||selected=="hold","Invalid animation mode");require(length>0,"Animation has zero duration");
    auto& s=p.states[e];p.clearVoices(s);s={};s.model=v.model;s.clip=name;s.mode=selected;s.speed=speed;s.length=length;s.history.emplace_back(0,p.world.worldMatrix(e));v.nodeWorld=p.pose(e,s,0);}
void StudioRuntime::stop(EntityId e){auto& p=*impl_;p.poseCache.clear();if(auto b=p.bindings.find(e);b!=p.bindings.end()&&b->second.drive&&p.world.alive(b->second.player))p.visual(b->second.player).nodeWorld.clear();if(p.states.contains(e)){p.clearVoices(p.states.at(e));if(p.bindings.contains(e)){auto model=p.states.at(e).model;p.states.at(e)={};p.states.at(e).model=model;}else p.states.erase(e);}if(p.world.alive(e)&&p.world.has<ModelComponent>(e))p.visual(e).nodeWorld.clear();}
void StudioRuntime::seek(EntityId e,double at){auto& p=*impl_;p.poseCache.clear();require(p.states.contains(e),"No active animation");auto& s=p.states.at(e);require(std::isfinite(at)&&at>=0&&at<=s.length,"Seek outside clip");p.clearVoices(s);s.cursor=at;s.initial=false;s.finished=false;s.history.clear();s.history.emplace_back(at,p.world.worldMatrix(e));p.visual(e).nodeWorld=p.pose(e,s,at);}
void StudioRuntime::pause(EntityId e,bool paused){auto& p=*impl_;p.poseCache.clear();require(p.states.contains(e),"No active animation");auto& s=p.states.at(e);s.paused=paused;for(auto v:s.voices)p.audio.pauseVoice(v.handle,paused||p.suspended||s.inactive);}
void StudioRuntime::setSuspended(bool value){auto& p=*impl_;p.poseCache.clear();if(p.suspended==value)return;p.suspended=value;for(auto& [id,s]:p.states){(void)id;for(auto v:s.voices)p.audio.pauseVoice(v.handle,value||s.paused||s.inactive);}}
void StudioRuntime::attach(EntityId e,EntityId parent,bool drive){auto& p=*impl_;p.poseCache.clear();auto& v=p.visual(e);auto& body=p.visual(parent);require(e!=parent&&v.model->studio&&!v.model->studio->player["context"].is_null(),"Equipment requires Studio player context");
    require(!p.bindings.contains(parent),"Attach equipment to a player, not to other equipment");for(const auto& [other,b]:p.bindings)require(!drive||other==e||b.player!=parent||!b.drive,"Only one equipment clip may drive player pose");
    for(const auto& [part,uuid]:v.model->studio->player["context"]["groups"].items()){(void)uuid;require(bodyNode(*body.model,part)!=size_t(-1),"Player lacks group "+part);}
    require(p.world.has<TransformComponent>(e),"Equipment requires transform");p.world.setParent(e,parent);if(auto old=p.bindings.find(e);old!=p.bindings.end()&&old->second.drive&&p.world.alive(old->second.player))p.visual(old->second.player).nodeWorld.clear();p.world.get<TransformComponent>(e).local={};p.bindings[e]={parent,drive,v.model,body.model};
    if(!p.states.contains(e)){Impl::State s;s.model=v.model;p.states[e]=std::move(s);}
}
void StudioRuntime::detach(EntityId e){auto& p=*impl_;p.poseCache.clear();if(auto b=p.bindings.find(e);b!=p.bindings.end()&&b->second.drive&&p.world.alive(b->second.player))p.visual(b->second.player).nodeWorld.clear();p.bindings.erase(e);if(p.world.alive(e))p.world.setParent(e,{});}
void StudioRuntime::update(double delta){auto& p=*impl_;p.poseCache.clear();require(std::isfinite(delta)&&delta>=0&&delta<=10,"Invalid Studio frame delta");
    std::erase_if(p.bindings,[&](const auto& entry){const auto& [id,b]=entry;const bool invalid=!p.world.alive(id)||!p.world.alive(b.player)||!p.world.has<ModelComponent>(id)||!p.world.has<ModelComponent>(b.player)||p.world.get<ModelComponent>(id).model!=b.equipmentModel||p.world.get<ModelComponent>(b.player).model!=b.playerModel||p.world.parent(id)!=b.player;if(invalid&&b.drive&&p.world.alive(b.player)&&p.world.has<ModelComponent>(b.player))p.world.get<ModelComponent>(b.player).nodeWorld.clear();return invalid;});
    for(auto it=p.states.begin();it!=p.states.end();){auto& [id,s]=*it;if(!p.world.alive(id)||!p.world.has<ModelComponent>(id)||p.world.get<ModelComponent>(id).model!=s.model){p.clearVoices(s);if(p.world.alive(id)&&p.world.has<ModelComponent>(id))p.world.get<ModelComponent>(id).nodeWorld.clear();it=p.states.erase(it);}else ++it;}
    for(auto& [id,s]:p.states){const bool inactive=p.world.has<ActiveComponent>(id)&&!p.world.get<ActiveComponent>(id).value;if(inactive!=s.inactive){if(inactive&&p.bindings.contains(id)&&p.bindings.at(id).drive)p.visual(p.bindings.at(id).player).nodeWorld.clear();s.inactive=inactive;for(auto v:s.voices)p.audio.pauseVoice(v.handle,inactive||s.paused||p.suspended||s.inactive);}std::erase_if(s.voices,[&](const auto& v){return !p.audio.voiceActive(v.handle);});if(!s.paused&&!p.suspended&&!s.inactive&&!s.clip.empty()){
        double remaining=delta*s.speed;unsigned loops=0;do{const double before=s.cursor;if(s.mode=="loop"&&s.cursor+remaining>=s.length){require(++loops<=10000,"Animation loop budget exceeded");p.sounds(s,before,s.length,s.initial);remaining-=s.length-s.cursor;p.clearVoices(s);s.cursor=0;s.initial=true;s.history.clear();}
            else{s.cursor+=remaining;p.sounds(s,before,std::min(s.cursor,s.length),s.initial);s.initial=false;remaining=0;if(s.cursor>=s.length&&!s.finished){s.finished=true;std::erase_if(s.voices,[&](const auto& v){if(v.loop)p.audio.stopVoice(v.handle);return v.loop;});}s.cursor=std::min(s.cursor,s.length+120.);} }while(remaining>0);
        const auto matrix=p.world.worldMatrix(id);if(s.history.empty()||s.history.back().first!=s.cursor)s.history.emplace_back(s.cursor,matrix);while(s.history.size()>8192||(s.history.size()>2&&s.history[1].first<s.cursor-120))s.history.pop_front();
    }p.visual(id).nodeWorld=sampleModel(s.model->hierarchy,s.clip,float(p.modelTime(s,s.cursor)));}
    for(const auto& [id,b]:p.bindings)if(b.drive&&p.states.contains(id)&&!p.states.at(id).clip.empty()&&!p.states.at(id).inactive){const auto& s=p.states.at(id);auto& parent=p.visual(b.player);auto it=p.states.find(b.player);parent.nodeWorld=sampleModel(parent.model->hierarchy,it==p.states.end()?"":it->second.clip,float(it==p.states.end()?0:p.modelTime(it->second,it->second.cursor)),p.playerOverrides(b.player,s,p.modelTime(s,s.cursor)));}
    for(const auto& [id,b]:p.bindings){(void)b;if(p.states.contains(id))p.visual(id).nodeWorld=p.pose(id,p.states.at(id),p.states.at(id).cursor);}
}
std::vector<RenderItem> StudioRuntime::render(const Camera& camera){auto& p=*impl_;p.poseCache.clear();p.usedTextures.clear();p.stats.particles=p.stats.trails=p.stats.flashes=p.stats.glows=0;std::vector<RenderItem> output;std::vector<PointLight> lights;std::set<std::pair<EntityId,std::string>> usedMeshes;
    const auto forward=unit(sub(camera.target,camera.eye)),right=unit(cross({0,1,0},forward)),up=unit(cross(forward,right));
    for(const auto& [id,s]:p.states){if(!p.world.alive(id)||!s.model->studio||(p.world.has<ActiveComponent>(id)&&!p.world.get<ActiveComponent>(id).value))continue;const auto* clip=p.clip(s);if(!clip)continue;const auto& package=*s.model->studio;
        for(const auto& interval:studio::intervals(package,*clip)){const auto& e=*interval.definition;const auto& event=*interval.event;if(!p.owns(s,event))continue;const auto target=event["target"].get<std::string>();if(!target.empty()&&nodeIndex(*s.model,target)==size_t(-1)&&p.contextNode(id,s,target)==size_t(-1))continue;
            const bool follow=event["follow"].get<bool>()||e["follow"].get<bool>();const auto offset=studio::vec(event["offset"]);std::vector<Vertex> vertices;std::vector<std::uint32_t> indices;
            auto quad=[&](Vec3 center,float size,float rotation,Color color,unsigned frame,const Matrix& anchor){const auto world=p.world.worldMatrix(id);const float units=std::hypot(world.values[0],world.values[1],world.values[2])/package.manifest["unitsPerMeter"].get<float>();(void)anchor;const auto r=scale(add(scale(right,std::cos(rotation)),scale(up,std::sin(rotation))),size*units*.5f),u=scale(sub(scale(up,std::cos(rotation)),scale(right,std::sin(rotation))),size*units*.5f);const unsigned cols=e["columns"],rows=e["rows"];const float x=float(frame%cols)/cols,y=float(frame/cols)/rows,w=1.f/cols,h=1.f/rows;const auto n=std::uint32_t(vertices.size());
                vertices.insert(vertices.end(),{{sub(sub(center,r),u),scale(forward,-1),{x,y+h},color},{add(sub(center,u),r),scale(forward,-1),{x+w,y+h},color},{add(add(center,r),u),scale(forward,-1),{x+w,y},color},{add(sub(center,r),u),scale(forward,-1),{x,y},color}});indices.insert(indices.end(),{n,n+1,n+2,n,n+2,n+3});};
            if(e["kind"]=="particle"){const auto particles=studio::particles(interval,s.cursor);p.stats.particles+=particles.size();require(p.stats.particles<=32768,"Global Studio particle budget exceeded (32768)");struct Sprite{studio::Particle particle;Matrix anchor;Vec3 center;float depth;};std::vector<Sprite> sprites;for(const auto& particle:particles){const auto anchor=p.anchor(id,s,target,follow?s.cursor:particle.birth);const auto center=transformPoint(add(particle.position,offset),anchor);const auto d=sub(center,camera.eye);sprites.push_back({particle,anchor,center,d.x*forward.x+d.y*forward.y+d.z*forward.z});}std::stable_sort(sprites.begin(),sprites.end(),[](const auto& a,const auto& b){return a.depth>b.depth;});for(const auto& sprite:sprites){const auto& particle=sprite.particle;auto c=particle.color;c.a=particle.opacity;quad(sprite.center,particle.size,particle.rotation,c,particle.frame,sprite.anchor);}}
            else if(e["kind"]=="trail"){const auto times=studio::trailTimes(interval,s.cursor);if(times.empty())continue;p.stats.trails++;const double age=std::clamp((s.cursor-interval.start)/e["duration"].get<double>(),0.,1.);const float width=e["width"].get<float>()*studio::curve(e,"size",age,e["size"],e["sizeEnd"]),opacity=studio::curve(e,"opacity",age,e["opacity"],e["opacityEnd"]);const auto start=studio::vec(e["startOffset"]),end=studio::vec(e["endOffset"]);auto c=studio::colorCurve(e,age);
                for(size_t i=0;i<times.size();i++){const auto anchor=p.anchor(id,s,target,times[i]);c.a=opacity*float(std::max(0.,1-(s.cursor-times[i])/e["trailLength"].get<double>()));const float u=float(i)/float(times.size()-1);vertices.push_back({transformPoint(add(start,offset),anchor),{0,1,0},{u,0},c});vertices.push_back({transformPoint(add(add(start,scale(sub(end,start),width)),offset),anchor),{0,1,0},{u,1},c});if(i){const auto j=std::uint32_t(i*2);indices.insert(indices.end(),{j-2,j-1,j,j-1,j+1,j});}}}
            else {if(s.cursor<interval.start||s.cursor>=interval.end)continue;const double age=(s.cursor-interval.start)/e["duration"].get<double>();auto c=studio::colorCurve(e,age);c.a=studio::curve(e,"opacity",age,e["opacity"],e["opacityEnd"]);const auto at=follow?s.cursor:interval.start;const auto anchor=p.anchor(id,s,target,at);
                if(e["kind"]=="glow"){const auto center=transformPoint(offset,anchor);quad(center,studio::curve(e,"size",age,e["size"],e["sizeEnd"]),0,c,0,anchor);lights.push_back({center,e["distance"].get<float>()*std::hypot(p.world.worldMatrix(id).values[0],p.world.worldMatrix(id).values[1],p.world.worldMatrix(id).values[2])/package.manifest["unitsPerMeter"].get<float>(),c,e["intensity"].get<float>()*float(1-age)});p.stats.glows++;}
                else {
                    auto model=s.model;auto targetIndex=nodeIndex(*model,target);auto matrices=p.pose(id,s,at);auto world=p.entityAt(id,s,at);
                    const auto parentNode=p.contextNode(id,s,target);if(parentNode!=size_t(-1)){model=p.visual(p.bindings.at(id).player).model;targetIndex=parentNode;matrices=p.contextPose(id,s,at);world=p.parentAt(id,s,at);}
                    if(targetIndex==size_t(-1))continue;if(model->hierarchy.nodes[targetIndex].role=="locator")targetIndex=size_t(model->hierarchy.nodes[targetIndex].parent);const auto delta=sub(transformPoint(offset,anchor),transformPoint({},anchor));
                    for(size_t i=0;i<model->parts.size();i++){int node=int(model->partNodes[i]);while(node>=0&&size_t(node)!=targetIndex)node=model->hierarchy.nodes[size_t(node)].parent;if(node<0)continue;auto item=model->parts[i];item.material.unlit=true;item.material.surfaceOverlay=true;item.material.alphaCutoff=item.material.alphaMode==AlphaMode::Mask?item.material.alphaCutoff:-1;item.material.alphaMode=AlphaMode::Additive;item.material.color=c;item.world=multiply(matrices[model->partNodes[i]],world);item.world.values[12]+=delta.x;item.world.values[13]+=delta.y;item.world.values[14]+=delta.z;output.push_back(item);p.stats.flashes++;}
                }
            }
            if(!indices.empty()){Material material;material.texture=p.texture(package,e);material.unlit=true;material.doubleSided=true;material.alphaMode=e["blend"]=="additive"||e["kind"]=="glow"?AlphaMode::Additive:AlphaMode::Blend;const auto key=std::pair{id,event["id"].get<std::string>()};usedMeshes.insert(key);auto& mesh=p.meshes[key];mesh=p.renderer.updateDynamicMesh(mesh,vertices,indices);output.push_back({mesh,material,{}});}
        }
    }std::erase_if(p.meshes,[&](const auto& entry){return !usedMeshes.contains(entry.first);});std::erase_if(p.textures,[&](const auto& entry){return !p.usedTextures.contains(entry.first);});p.renderer.setPointLights(lights);return output;
}
}
