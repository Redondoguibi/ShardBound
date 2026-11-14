#include "world/WorldTools.h"
#include "scripting/Bridge.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
namespace game {
namespace {
using namespace script;
double number(const Value& v,double low,double high,const Location& at) {
    const double n=v.number(at);
    if(!std::isfinite(n) || n<low || n>high || n!=std::floor(n)) fail(at,"TERRAIN103","Expected bounded integer");
    return n;
}
using Cell=std::tuple<std::int64_t,int,std::int64_t>;
Cell cell(const Value& v,const ChunkWorld& world,const Location& at) {
    return {world.originX()+std::int64_t(number(v.member("x",at),-100000,100000,at)),
        int(number(v.member("y",at),0,47,at)),world.originZ()+std::int64_t(number(v.member("z",at),-100000,100000,at))};
}
}
void bindWorldTools(script::Runtime& runtime,ChunkWorld& world) {
    runtime.bind("world_streaming_stats",native([&world](const auto& a,const auto& at) {
        if(!a.empty()) fail(at,"TERRAIN103","world_streaming_stats()");const auto stats=world.streamingStats();auto result=object();
        result.setMember("queued",Value(double(stats.queued)),at);result.setMember("ready",Value(double(stats.ready)),at);result.setMember("loaded",Value(double(stats.loaded)),at);result.setMember("resident_pages",Value(double(stats.residentPages)),at);result.setMember("pages",Value(double(stats.pages)),at);result.setMember("generated",Value(double(stats.generated)),at);result.setMember("discarded",Value(double(stats.discarded)),at);result.setMember("generation_ms",Value(stats.generationMs),at);result.setMember("commit_ms",Value(stats.commitMs),at);return result;
    }));
    runtime.bind("terrain_edit",native([&world](const auto& a,const auto& at) {
        if(a.size()!=1) fail(at,"TERRAIN103","terrain_edit([{x,y,z,block}, ...])");
        const auto rows=std::get_if<std::shared_ptr<List>>(&a[0].data);
        if(!rows || (*rows)->values.size()>4096) fail(at,"TERRAIN103","Expected at most 4096 edits");
        BlockEdits changes;
        for(const auto& row:(*rows)->values) if(!changes.emplace(cell(row,world,at),row.member("block",at).string(at)).second)
            fail(at,"TERRAIN103","Duplicate block in transaction");
        try { return Value(double(world.editBlocks(changes))); }catch(const std::exception& e) { fail(at,"TERRAIN103",e.what()); }
    }));
    runtime.bind("terrain_undo",native([&world](const auto& a,const auto& at) {
        if(!a.empty()) fail(at,"TERRAIN103","terrain_undo()");return Value(world.undoEdits());
    }));
    runtime.bind("terrain_redo",native([&world](const auto& a,const auto& at) {
        if(!a.empty()) fail(at,"TERRAIN103","terrain_redo()");return Value(world.redoEdits());
    }));
    runtime.bind("terrain_stamp",native([&world](const auto& a,const auto& at) {
        if(a.size()!=3) fail(at,"TERRAIN103","terrain_stamp(structure, origin, quarter_turns)");
        const auto id=a[0].string(at);const auto found=world.generator().definition().structures.find(id);
        if(found==world.generator().definition().structures.end()) fail(at,"TERRAIN103","Unknown structure");
        const auto [x,y,z]=cell(a[1],world,at);const int turns=int(number(a[2],0,3,at));BlockEdits changes;
        for(const auto& c:found->second.cells) { int dx=c.x,dz=c.z;for(int i=0;i<turns;++i) { const int old=dx;dx=-dz;dz=old; }changes[{x+dx,y+c.y,z+dz}]=c.block; }
        try { return Value(double(world.editBlocks(changes))); }catch(const std::exception& e) { fail(at,"TERRAIN103",e.what()); }
    }));
    runtime.bind("terrain_surface",native([&world](const auto& a,const auto& at) {
        if(a.size()!=2) fail(at,"TERRAIN103","terrain_surface(local_x, local_z)");
        auto blocks=world.columnBlocks(world.originX()+std::int64_t(number(a[0],-100000,100000,at)),world.originZ()+std::int64_t(number(a[1],-100000,100000,at)));
        for(int y=47;y>=0;--y) if(!blocks[size_t(y)].empty() && world.generator().definition().blocks.at(blocks[size_t(y)]).solid) return Value(double(y+1));
        return Value(0.0);
    }));
    runtime.bind("terrain_path",native([&world](const auto& a,const auto& at) {
        if(a.size()!=3) fail(at,"TERRAIN103","terrain_path(start, goal, {max_nodes,clearance,step,drop})");
        const auto start=cell(a[0],world,at),goal=cell(a[1],world,at);
        const int budget=int(number(a[2].member("max_nodes",at),1,4096,at));
        const int clearance=int(number(a[2].member("clearance",at),1,8,at));
        const int step=int(number(a[2].member("step",at),0,4,at)),drop=int(number(a[2].member("drop",at),0,8,at));
        const auto [sx,sy,sz]=start;const auto [gx,gy,gz]=goal;
        if(std::abs(gx-sx)>128 || std::abs(gz-sz)>128) fail(at,"TERRAIN103","Path goal exceeds 128-block search radius");
        const auto& optionFields=std::get<std::shared_ptr<Object>>(a[2].data)->fields;
        const bool avoid=optionFields.contains("avoid_entities") && optionFields.at("avoid_entities").boolean(at);
        std::vector<EntityId> ignored;
        if(optionFields.contains("ignore")) {
            const auto list=std::get_if<std::shared_ptr<List>>(&optionFields.at("ignore").data);
            if(!list || (*list)->values.size()>16) fail(at,"TERRAIN103","Expected at most 16 ignored entities");
            for(const auto& value:(*list)->values) ignored.push_back(requireEntity(value,world.ecs(),at));
        }
        const auto obstacles=avoid?world.navigationObstacles(ignored):std::vector<std::pair<Vec3,Vec3>>{};
        // Generate each queried column once. Repeated blockAt calls regenerate decorations.
        std::map<std::pair<std::int64_t,std::int64_t>,std::array<bool,48>> columns;
        auto solid=[&](std::int64_t x,int y,std::int64_t z) {
            if(y<0 || y>=48) return true;
            const auto key=std::pair{x,z};auto it=columns.find(key);
            if(it==columns.end()) {
                std::array<bool,48> values{};auto blocks=world.columnBlocks(x,z);
                for(size_t i=0;i<48;++i) values[i]=!blocks[i].empty() && world.generator().definition().blocks.at(blocks[i]).solid;
                it=columns.emplace(key,values).first;
            }
            return it->second[size_t(y)];
        };
        auto clear=[&](std::int64_t x,int low,int high,std::int64_t z) {
            for(int y=low;y<high;++y) if(solid(x,y,z)) return false;
            const double lx=double(x-world.originX()),lz=double(z-world.originZ());
            for(const auto& [minimum,maximum]:obstacles) if(lx<maximum.x && lx+1>minimum.x && lz<maximum.z && lz+1>minimum.z && low<maximum.y && high>minimum.y) return false;
            return true;
        };
        auto stand=[&](Cell p) { auto [x,y,z]=p;return y>0 && solid(x,y-1,z) && clear(x,y,y+clearance,z); };
        std::vector<Value> points;std::string status="unreachable";size_t expanded=0;
        if(stand(start) && stand(goal)) {
            struct Node { Cell parent;int cost; };
            using Queue=std::tuple<int,int,Cell>;
            std::priority_queue<Queue,std::vector<Queue>,std::greater<Queue>> open;
            std::map<Cell,Node> nodes;nodes.emplace(start,Node{start,0});
            const auto heuristic=[&](Cell p) { return int(std::abs(std::get<0>(p)-gx)+std::abs(std::get<2>(p)-gz)); };
            open.emplace(heuristic(start),0,start);
            while(!open.empty()) {
                auto [estimate,cost,p]=open.top();open.pop();(void)estimate;
                if(cost!=nodes.at(p).cost) continue;
                if(p==goal) {
                    status="found";
                    for(Cell v=goal;;v=nodes.at(v).parent) {
                        const auto [x,y,z]=v;auto row=object();row.setMember("x",Value(double(x-world.originX())+.5),at);row.setMember("y",Value(double(y)),at);row.setMember("z",Value(double(z-world.originZ())+.5),at);points.push_back(row);if(v==start) break;
                    }
                    std::reverse(points.begin(),points.end());break;
                }
                if(expanded>=size_t(budget)) { status="budget";break; }
                ++expanded;
                const auto [x,y,z]=p;
                for(const auto [dx,dz]:std::array<std::pair<int,int>,4>{{{1,0},{0,1},{-1,0},{0,-1}}}) {
                    const auto nx=x+dx,nz=z+dz;
                    if(std::abs(nx-sx)>128 || std::abs(nz-sz)>128) continue;
                    for(int ny=y+step;ny>=y-drop;--ny) {
                        Cell next{nx,ny,nz};if(!stand(next)) continue;
                        // Check headroom over the entire step/drop, including the source.
                        if(!clear(x,y,std::max(y,ny)+clearance,z) || !clear(nx,ny,std::max(y,ny)+clearance,nz)) continue;
                        const int newCost=cost+1+std::abs(ny-y);
                        const auto it=nodes.find(next);
                        if(it==nodes.end() || newCost<it->second.cost) {
                            nodes.insert_or_assign(next,Node{p,newCost});open.emplace(newCost+heuristic(next),newCost,next);
                        }
                        break;
                    }
                }
            }
        } else status="invalid_endpoint";
        auto result=object();result.setMember("status",Value(status),at);result.setMember("points",list(std::move(points)),at);result.setMember("expanded",Value(double(expanded)),at);return result;
    }));
}
}
