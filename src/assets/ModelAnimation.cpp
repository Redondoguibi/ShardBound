#include "assets/ModelAnimation.h"
#include <DirectXMath.h>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <set>
#include <cmath>
namespace game {
using namespace DirectX;
static XMMATRIX load(const Matrix& m) { XMFLOAT4X4 v; std::copy_n(m.values.begin(),16,&v._11); return XMLoadFloat4x4(&v); }
static Matrix store(FXMMATRIX m) { XMFLOAT4X4 v; XMStoreFloat4x4(&v,m); Matrix r; std::copy_n(&v._11,16,r.values.begin()); return r; }
Matrix composeTRS(Vec3 p,std::array<float,4> q,Vec3 s) { return store(XMMatrixScaling(s.x,s.y,s.z)*XMMatrixRotationQuaternion(XMQuaternionNormalize(XMVectorSet(q[0],q[1],q[2],q[3])))*XMMatrixTranslation(p.x,p.y,p.z)); }
Vec3 transformPoint(Vec3 p,const Matrix& m) { XMFLOAT3 r; XMStoreFloat3(&r,XMVector3TransformCoord(XMVectorSet(p.x,p.y,p.z,1),load(m))); return {r.x,r.y,r.z}; }
Matrix inverse(const Matrix& m) { const auto value=load(m); if(std::abs(XMVectorGetX(XMMatrixDeterminant(value)))<1e-12f) throw std::runtime_error("Singular attachment transform"); return store(XMMatrixInverse(nullptr,value)); }
std::array<float,4> sampleTrack(const ModelTrack& t,float time) {
    if(t.times.empty() || t.times.size()!=t.values.size()) throw std::runtime_error("Invalid animation track");
    auto upper=std::upper_bound(t.times.begin(),t.times.end(),time);
    if(upper==t.times.begin()) return t.values.front(); if(upper==t.times.end()) return t.values.back();
    const size_t b=size_t(upper-t.times.begin()),a=b-1; if(t.step) return t.values[a];
    const float u=(time-t.times[a])/(t.times[b]-t.times[a]); std::array<float,4> r;
    if(t.path==2) { const auto& x=t.values[a]; const auto& y=t.values[b]; XMFLOAT4 out;
        XMStoreFloat4(&out,XMQuaternionNormalize(XMQuaternionSlerp(XMVectorSet(x[0],x[1],x[2],x[3]),XMVectorSet(y[0],y[1],y[2],y[3]),u))); r={out.x,out.y,out.z,out.w};
    } else for(size_t i=0;i<4;i++) r[i]=t.values[a][i]+(t.values[b][i]-t.values[a][i])*u;
    return r;
}
void validateHierarchy(const ModelHierarchy& h) {
    auto need=[](bool ok){if(!ok)throw std::runtime_error("Invalid model hierarchy/animation data");};need(h.nodes.size()<=10000&&h.clips.size()<=1024);
    for(const auto& n:h.nodes){need(n.parent>=-1&&(n.parent<0||size_t(n.parent)<h.nodes.size()));for(float x:n.local.values)need(std::isfinite(x));}
    size_t samples=0;std::set<std::string> names;for(const auto& c:h.clips){need(!c.name.empty()&&names.insert(c.name).second&&std::isfinite(c.length)&&c.length>=0);std::set<std::pair<size_t,int>> targets;
        for(const auto& t:c.tracks){need(t.node<h.nodes.size()&&t.path>=1&&t.path<=3&&!t.times.empty()&&t.times.size()==t.values.size()&&targets.emplace(t.node,t.path).second);samples+=t.times.size();need(samples<=2000000);float previous=-1;for(float x:t.times){need(std::isfinite(x)&&x>=0&&x>previous&&x<=c.length);previous=x;}for(const auto& v:t.values){for(float x:v)need(std::isfinite(x));if(t.path==2)need(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]+v[3]*v[3]>1e-12f);}}
    }
}
std::vector<Matrix> sampleModel(const ModelHierarchy& h,const std::string& clip,float time,const std::map<size_t,Matrix>& overrides,const std::map<size_t,Matrix>& globals) {
    std::vector<Matrix> local,world(h.nodes.size()); for(const auto& n:h.nodes) local.push_back(n.local);
    const auto found=std::find_if(h.clips.begin(),h.clips.end(),[&](const auto& c){return c.name==clip;});
    if(found!=h.clips.end()) {
        struct Pose { Vec3 p,s; std::array<float,4> q; }; std::map<size_t,Pose> poses;
        for(const auto& t:found->tracks) {
            auto [it,fresh]=poses.try_emplace(t.node); auto& pose=it->second;
            if(fresh) { XMVECTOR s,q,p; if(!XMMatrixDecompose(&s,&q,&p,load(local.at(t.node)))) throw std::runtime_error("Animated node matrix cannot be decomposed"); XMFLOAT3 a,b; XMFLOAT4 c; XMStoreFloat3(&a,p); XMStoreFloat3(&b,s); XMStoreFloat4(&c,q); pose={{a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z,c.w}}; }
            const auto v=sampleTrack(t,time); if(t.path==1) pose.p={v[0],v[1],v[2]}; else if(t.path==2) pose.q=v; else pose.s={v[0],v[1],v[2]};
        }
        for(const auto& [i,p]:poses) local.at(i)=composeTRS(p.p,p.q,p.s);
    }
    for(const auto& [i,m]:overrides) local.at(i)=m;
    std::vector<int> visited(h.nodes.size()); std::function<void(size_t,size_t)> visit=[&](size_t i,size_t depth) {
        if(visited.at(i)==2) return; if(visited[i]==1 || depth>64) throw std::runtime_error("Invalid node hierarchy"); visited[i]=1;
        if(auto it=globals.find(i);it!=globals.end()) world[i]=it->second;
        else if(h.nodes[i].parent>=0) { const auto p=size_t(h.nodes[i].parent); visit(p,depth+1); world[i]=multiply(local[i],world.at(p)); } else world[i]=local[i]; visited[i]=2;
    }; for(size_t i=0;i<h.nodes.size();i++) visit(i,0); return world;
}
}
