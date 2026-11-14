#include "studio/StudioEffects.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <stdexcept>
namespace game::studio {
Vec3 vec(const Json& j) { return {j.at(0).get<float>(),j.at(1).get<float>(),j.at(2).get<float>()}; }
Color tint(const std::string& hex) { return {std::stoi(hex.substr(1,2),nullptr,16)/255.f,std::stoi(hex.substr(3,2),nullptr,16)/255.f,std::stoi(hex.substr(5,2),nullptr,16)/255.f,1}; }
static double mix(double a,double b,double t) { return a+(b-a)*t; }
static auto pair(const Json& e,const std::string& key,double age,Json start,Json end) {
    std::vector<Json> points; if(e.at("curves").contains(key)) for(const auto& p:e["curves"][key]) points.push_back(p);
    else points={{{"age",0},{"value",start}},{{"age",1},{"value",end}}};
    std::sort(points.begin(),points.end(),[](const auto& a,const auto& b){return a.at("age").template get<double>()<b.at("age").template get<double>();});
    auto a=points.front(),b=points.back(); if(age<=a["age"].get<double>()) b=a; else if(age>=b["age"].get<double>()) a=b;
    else for(size_t i=1;i<points.size();i++) if(age<=points[i]["age"].get<double>()) {a=points[i-1];b=points[i];break;}
    const auto x=a["age"].get<double>(),y=b["age"].get<double>(); return std::tuple<Json,Json,double>{a["value"],b["value"],x==y ? 0:std::clamp((age-x)/(y-x),0.,1.)};
}
float curve(const Json& e,const std::string& key,double age,float start,float end) { auto [a,b,t]=pair(e,key,age,start,end); return float(mix(a.get<double>(),b.get<double>(),t)); }
Color colorCurve(const Json& e,double age) { auto [a,b,t]=pair(e,"color",age,e.at("color"),e.at("colorEnd")); auto x=tint(a),y=tint(b); return {float(mix(x.r,y.r,t)),float(mix(x.g,y.g,t)),float(mix(x.b,y.b,t)),1}; }
std::vector<Interval> intervals(const StudioPackage& p,const Json& clip) {
    std::vector<const Json*> events; for(const auto& e:clip.at("events")) events.push_back(&e);
    std::stable_sort(events.begin(),events.end(),[](auto a,auto b){return std::tuple{a->at("time").template get<double>(),a->at("order").template get<int>(),a->at("id").template get<std::string>()}<std::tuple{b->at("time").template get<double>(),b->at("order").template get<int>(),b->at("id").template get<std::string>()};});
    std::vector<Interval> result; std::map<std::string,size_t> active;
    for(const auto* event:events) { const auto& e=*event; auto key=e.at("instance").get<std::string>(); if(key.empty()) key=e["resource"].get<std::string>()+"|"+e["target"].get<std::string>();
        if(e["action"]=="stop") { if(active.contains(key)) { auto& old=result[active.at(key)]; old.end=std::min(old.end,e["time"].get<double>()); active.erase(key); } continue; }
        const auto* effect=p.effect(e["resource"]); if(!effect||e["type"]=="sound") continue;
        const bool continuous=e["action"]=="start"; if(continuous&&active.contains(key)) {auto& old=result[active.at(key)];old.end=std::min(old.end,e["time"].get<double>());active.erase(key);}
        const double start=e["time"]; result.push_back({event,effect,start,std::min(clip["length"].get<double>(),start+effect->at("duration").get<double>()),continuous}); if(continuous) active[key]=result.size()-1;
    } return result;
}
using D3=std::array<double,3>;
static D3 unit(D3 a) { const double n=std::hypot(a[0],a[1],a[2]); return n ? D3{a[0]/n,a[1]/n,a[2]/n}:D3{0,1,0}; }
static D3 cross(D3 a,D3 b) { return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}; }
struct Random { std::uint32_t n; double operator()() { n+=0x6d2b79f5u; auto t=(n^(n>>15))*(n|1u);t^=t+((t^(t>>7))*(t|61u));return double(t^(t>>14))/4294967296.; } };
static D3 direction(D3 axis,double spread,Random& r) { const auto n=unit(axis),u=unit(cross(std::abs(n[1])<.99?D3{0,1,0}:D3{1,0,0},n)),v=cross(n,u); const auto c=mix(1,std::cos(spread*std::numbers::pi/180),r()),s=std::sqrt(std::max(0.,1-c*c)),a=r()*std::numbers::pi*2; D3 out;for(size_t i=0;i<3;i++)out[i]=n[i]*c+s*(std::cos(a)*u[i]+std::sin(a)*v[i]);return out; }
std::vector<Particle> particles(const Interval& in,double time) {
    const auto& e=*in.definition; const double lifetime=e["lifetime"]; if(time<in.start||time>in.end+lifetime) return {};
    std::vector<std::pair<unsigned,double>> births; const unsigned burst=e["burst"]; for(unsigned i=0;i<burst;i++)births.emplace_back(i,0);
    const double elapsed=time-in.start,rate=e["rate"];
    if(in.continuous&&rate>0) { const auto first=std::max(1,int(std::ceil((elapsed-lifetime)*rate))),last=int(std::floor((std::min(time,in.end)-in.start)*rate+1e-8)); for(int i=first;i<=last;i++)births.emplace_back(burst+unsigned(i),i/rate); }
    std::vector<Particle> result;
    for(auto [index,birth]:births) { const double age=elapsed-birth;if(age<0||age>=lifetime)continue; Random r{e["seed"].get<std::uint32_t>()+(index+1)*2654435761u}; D3 origin{};const auto size=e["shapeSize"].get<D3>();
        if(e["shape"]=="box") for(size_t i=0;i<3;i++)origin[i]=(r()-.5)*size[i];
        else if(e["shape"]!="point") { const auto v=direction({0,1,0},180,r);const double radius=std::cbrt(r()); if(e["shape"]=="sphere")for(size_t i=0;i<3;i++)origin[i]=v[i]*radius*size[i]/2;
            else { const double angle=r()*std::numbers::pi*2,height=std::cbrt(r()),radial=std::sqrt(r())*height;origin={std::cos(angle)*radial*size[0]/2,height*size[1],std::sin(angle)*radial*size[2]/2}; } }
        const auto heading=direction(e["direction"].get<D3>(),e["spread"],r),gravity=e["gravity"].get<D3>(); const double speed=std::max(0.,e["speed"].get<double>()+(r()*2-1)*e["speedSpread"].get<double>()),drag=e["drag"],vt=drag>0?(1-std::exp(-drag*age))/drag:age,gt=drag>0?(age-vt)/drag:age*age/2;
        D3 pos; for(size_t i=0;i<3;i++)pos[i]=origin[i]+heading[i]*speed*vt+gravity[i]*gt;const auto t=age/lifetime; Particle p{index,unsigned(std::floor(age*e["fps"].get<double>()))%e["frames"].get<unsigned>(),in.start+birth,{float(pos[0]),float(pos[1]),float(pos[2])},curve(e,"size",t,e["size"],e["sizeEnd"]),curve(e,"opacity",t,e["opacity"],e["opacityEnd"]),curve(e,"rotation",t,e["rotation"],e["rotation"].get<float>()+e["angularVelocity"].get<float>()*float(lifetime))*float(std::numbers::pi/180),colorCurve(e,t)};result.push_back(p);
    }
    if(result.size()>4096) throw std::runtime_error("Studio particle budget exceeded");return result;
}
std::vector<double> trailTimes(const Interval& i,double time) { const auto& e=*i.definition;const double length=e["trailLength"],end=std::min(time,i.end),start=std::max(i.start,time-length); if(time<i.start||time>i.end+length||start>=end)return{};const int count=e["trailSamples"];std::vector<double> r;for(int n=0;n<count;n++)r.push_back(mix(start,end,double(n)/(count-1)));return r; }
}
