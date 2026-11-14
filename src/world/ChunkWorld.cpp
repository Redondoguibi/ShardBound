#include "world/ChunkWorld.h"
#include "world/FiniteMap.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <limits>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <numbers>
namespace game {
ChunkData buildChunk(const TerrainGenerator& generator,std::int64_t cx,std::int64_t cz,const BlockEdits* edits) {
    constexpr int n=16,hmax=TerrainGenerator::maxHeight,pitch=18;
    const auto& def=generator.definition();
    std::vector<std::string> names{std::string{}}; std::map<std::string,unsigned> ids;
    for(const auto& [id,b]:def.blocks) { (void)b; ids[id]=unsigned(names.size()); names.push_back(id); }
    std::vector<unsigned> voxels(pitch*pitch*hmax);
    auto voxel=[&](int x,int y,int z)->unsigned& { return voxels[(size_t(y)*pitch+size_t(z+1))*pitch+size_t(x+1)]; };
    for(int z=-1;z<=n;++z) for(int x=-1;x<=n;++x) {
        const auto wx=cx*n+x,wz=cz*n+z; const auto column=generator.column(wx,wz);
        for(size_t y=0;y<column.blocks.size();++y) if(!column.blocks[y].empty()) voxel(x,int(y),z)=ids.at(column.blocks[y]);
    }
    const auto decoration=generator.decoration(cx,cz);
    for(const auto& c:decoration) voxel(c.x,c.y,c.z)=ids.at(c.block);
    if(edits) for(auto it=edits->lower_bound({cx*n-1,0,std::numeric_limits<std::int64_t>::min()});it!=edits->end();++it) {
        const auto& [key,id]=*it;
        const auto [wx,y,wz]=key;
        if(wx>cx*n+n) break;
        if(y<0 || y>=hmax || wx<cx*n-1 || wx>cx*n+n || wz<cz*n-1 || wz>cz*n+n) continue;
        const int x=int(wx-cx*n),z=int(wz-cz*n);
        voxel(x,y,z)=id.empty()?0:ids.at(id);
    }
    ChunkData data;
    // Clockwise triangles viewed from outside (the D3D11 default front face).
    const std::array<Vec3,6> normals={Vec3{0,1,0},Vec3{0,-1,0},Vec3{1,0,0},Vec3{-1,0,0},Vec3{0,0,1},Vec3{0,0,-1}};
    const std::array<std::array<Vec3,4>,6> corners={{{Vec3{0,1,0},Vec3{0,1,1},Vec3{1,1,1},Vec3{1,1,0}},
        {Vec3{0,0,1},Vec3{0,0,0},Vec3{1,0,0},Vec3{1,0,1}},
        {Vec3{1,0,0},Vec3{1,1,0},Vec3{1,1,1},Vec3{1,0,1}},
        {Vec3{0,0,1},Vec3{0,1,1},Vec3{0,1,0},Vec3{0,0,0}},
        {Vec3{1,0,1},Vec3{1,1,1},Vec3{0,1,1},Vec3{0,0,1}},
        {Vec3{0,0,0},Vec3{0,1,0},Vec3{1,1,0},Vec3{1,0,0}}}};
    const std::array<Vec2,4> uv={Vec2{0,1},Vec2{0,0},Vec2{1,0},Vec2{1,1}};
    for(int z=0;z<n;++z) for(int x=0;x<n;++x) for(int y=0;y<hmax;++y) {
        const auto b=voxel(x,y,z); if(!b) continue;
        const auto& block=def.blocks.at(names[b]);
        if(!block.model.empty()) {
            data.models.push_back({block.model,{x+.5f,float(y),z+.5f}});
            continue;
        }
        for(size_t face=0;face<6;++face) {
            const auto normal=normals[face]; const int ny=y+int(normal.y);
            if(ny<0) continue;
            if(ny<hmax) {
                const auto adjacent=voxel(x+int(normal.x),ny,z+int(normal.z));
                if(adjacent && def.blocks.at(names[adjacent]).model.empty()) continue;
            }
            auto& mesh=data.meshes[names[b]]; const auto first=std::uint32_t(mesh.vertices.size());
            for(size_t i=0;i<4;++i) { auto p=corners[face][i]; p.x+=x;p.y+=y;p.z+=z; mesh.vertices.push_back({p,normal,uv[i]}); }
            for(auto index:{0U,1U,2U,0U,2U,3U}) mesh.indices.push_back(first+index);
        }
    }
    // Merge columns with identical solid occupancy, including caves and edits.
    // Non-solid decorative/fluid blocks never create collision bodies.
    std::array<std::uint64_t,n*n> occupancy{};
    for(int z=0;z<n;++z) for(int x=0;x<n;++x) for(int y=0;y<hmax;++y) {
        const auto id=voxel(x,y,z);if(id && def.blocks.at(names[id]).solid) occupancy[z*n+x]|=std::uint64_t{1}<<y;
    }
    std::array<bool,n*n> visited{};
    for(int z=0;z<n;++z) for(int x=0;x<n;++x) {
        if(visited[z*n+x]) continue;const auto mask=occupancy[z*n+x];int w=1,d=1;
        while(x+w<n && !visited[z*n+x+w] && occupancy[z*n+x+w]==mask) ++w;
        bool extend=true;while(z+d<n && extend) {
            for(int i=0;i<w;++i) if(visited[(z+d)*n+x+i] || occupancy[(z+d)*n+x+i]!=mask) extend=false;
            if(extend) ++d;
        }
        for(int j=0;j<d;++j) for(int i=0;i<w;++i) visited[(z+j)*n+x+i]=true;
        for(int y=0;y<hmax;) {
            if(!(mask&(std::uint64_t{1}<<y))) { ++y;continue; }
            const int start=y++;while(y<hmax && (mask&(std::uint64_t{1}<<y))) ++y;
            const float half=float(y-start)*.5f;data.boxes.push_back({{x+w*.5f,float(start)+half,z+d*.5f},{w*.5f,half,d*.5f}});
        }
    }
    if(const auto* map=generator.finiteMap()) for(const auto& area:map->areas()) {
        const auto b=area.bounds();
        const auto baseX=cx*n,baseZ=cz*n;
        const auto lowX=std::max<std::int64_t>(baseX,b.minX),highX=std::min<std::int64_t>(baseX+n,b.minX+b.width);
        const auto lowZ=std::max<std::int64_t>(baseZ,b.minZ),highZ=std::min<std::int64_t>(baseZ+n,b.minZ+b.depth);
        if(lowX<highX && lowZ<highZ) {
            const auto wallX=[&](std::int64_t edge,float outside) {
                data.boxes.push_back({{float(edge-baseX)+outside,24,float(lowZ-baseZ)+float(highZ-lowZ)*.5f},{.25f,64,float(highZ-lowZ)*.5f}});
            };
            const auto wallZ=[&](std::int64_t edge,float outside) {
                data.boxes.push_back({{float(lowX-baseX)+float(highX-lowX)*.5f,24,float(edge-baseZ)+outside},{float(highX-lowX)*.5f,64,.25f}});
            };
            if(lowX==b.minX) wallX(lowX,-.25f);
            if(highX==b.minX+b.width) wallX(highX,.25f);
            if(lowZ==b.minZ) wallZ(lowZ,-.25f);
            if(highZ==b.minZ+b.depth) wallZ(highZ,.25f);
        }
    }
    return data;
}
struct ChunkWorld::Worker {
    struct Job { Key key;RegionEdits edits;std::uint64_t epoch; };
    struct Result { Key key;ChunkData data;std::exception_ptr error;std::uint64_t epoch; };
    const TerrainGenerator& generator;
    mutable std::mutex mutex;std::condition_variable condition;
    std::deque<Job> jobs;std::deque<Result> ready;std::set<Key> requested;
    std::uint64_t epoch=0,generated=0,discarded=0;double generationMs=0,commitMs=0;
    bool stopping=false;std::exception_ptr fatal;std::thread thread;
    explicit Worker(const TerrainGenerator& value):generator(value),thread([this]{try {run();}catch(...) { std::lock_guard lock(mutex);fatal=std::current_exception(); }}) {}
    ~Worker() { {std::lock_guard lock(mutex);stopping=true;}condition.notify_all();thread.join(); }
    void reset() { std::lock_guard lock(mutex);++epoch;discarded+=jobs.size()+ready.size();jobs.clear();ready.clear();requested.clear();condition.notify_all(); }
    void prune(Key center,int distance) {
        std::lock_guard lock(mutex);const auto outside=[&](Key key) { return std::max(std::abs(key.first-center.first),std::abs(key.second-center.second))>distance; };
        discarded+=std::erase_if(jobs,[&](const auto& job){return outside(job.key);});discarded+=std::erase_if(ready,[&](const auto& result){return outside(result.key);});std::erase_if(requested,outside);condition.notify_all();
    }
    void run() {
        for(;;) {
            Job job;
            { std::unique_lock lock(mutex);condition.wait(lock,[&]{return stopping || (!jobs.empty() && ready.size()<8);});if(stopping) return;job=std::move(jobs.front());jobs.pop_front(); }
            const auto start=std::chrono::steady_clock::now();Result result{job.key,{},nullptr,job.epoch};
            try { auto local=job.edits.neighborhood(job.key);result.data=buildChunk(generator,job.key.first,job.key.second,&local); }catch(...) { result.error=std::current_exception(); }
            { std::lock_guard lock(mutex);++generated;generationMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
              if(result.epoch==epoch) ready.push_back(std::move(result));else ++discarded; }
        }
    }
};
ChunkWorld::ChunkWorld(Renderer& renderer,AssetManager& assets,World& world,PhysicsSystem& physics,PlayerController& player,TerrainGenerator generator)
    :renderer_(renderer),assets_(assets),world_(world),physics_(physics),player_(player),generator_(std::move(generator)) {
    for(const auto& [id,b]:generator_.definition().blocks) {
        textures_[id]=assets_.texture(AssetId(b.texture));
        shaders_[id]=b.shader.empty()?ShaderInstanceHandle{}:assets_.shaderInstance(b.shader,b.shaderParameters,b.shaderTexture);
        if(!b.model.empty() && !blockModels_.contains(b.model)) blockModels_.emplace(b.model,assets_.model(AssetId(b.model)));
    }
    if(const auto* map=generator_.finiteMap()) for(const auto& door:map->doors()) {
        doors_.push_back({{float(door.from.x)+.5f,float(door.from.z)+.5f},{float(door.to.x)+.5f,float(door.to.z)+.5f}});
    }
    if(!doors_.empty()) {
        const std::array<Vertex,4> vertices{{{{-.42f,0,-.42f},{0,1,0},{0,0}},{{-.42f,0,.42f},{0,1,0},{0,1}},
            {{.42f,0,.42f},{0,1,0},{1,1}},{{.42f,0,-.42f},{0,1,0},{1,0}}}};
        constexpr std::array<std::uint32_t,6> indices{0,1,2,0,2,3};
        doorMarker_=renderer_.createMesh(vertices,indices);
    }
    if(const auto* map=generator_.finiteMap()) for(const auto& area:map->areas()) {
        for(const auto& prop:area.map->props()) {
            const auto model=assets_.model(AssetId(prop.model));
            const int x=prop.x+area.offsetX,z=prop.z+area.offsetZ;
            const Key key{floorDiv(x,16),floorDiv(z,16)};
            Transform transform;transform.position={float(x-key.first*16)+.5f,float(prop.y),float(z-key.second*16)+.5f};
            transform.rotation.y=prop.yaw*std::numbers::pi_v<float>/180;transform.scale={prop.scale,prop.scale,prop.scale};
            for(auto part:model->parts) {part.world=multiply(part.world,transformMatrix(transform));props_[key].push_back(std::move(part));}
        }
    }
    if(physics_.terrainQuery) throw std::runtime_error("Physics world already has procedural terrain");
    worker_=std::make_unique<Worker>(generator_);
    physics_.terrainQuery=[this](Vec3 from,Vec3 to){return raycast(from,to);};
}
ChunkWorld::~ChunkWorld() { worker_.reset();physics_.terrainQuery={};clear(); }
void ChunkWorld::unload(Chunk& chunk) { for(auto id:chunk.colliders) { physics_.detach(id); if(world_.alive(id)) world_.destroy(id); transient_.erase(id); } chunk.colliders.clear(); }
void ChunkWorld::clear() { if(worker_) worker_->reset();for(auto& [key,chunk]:chunks_) { (void)key; unload(chunk); } chunks_.clear(); }

bool ChunkWorld::useDoor(int viewDistance) {
    const auto actor=player_.entity();
    if(!actor || !physics_.contains(actor)) return false;
    const auto prior=world_.get<TransformComponent>(actor).local.position;
    const auto body=physics_.description(actor);
    const float feet=prior.y-body.halfExtent.y;
    for(const auto& [from,to]:doors_) {
        const float dx=prior.x-(from.x-float(originX_)),dz=prior.z-(from.y-float(originZ_));
        if(dx*dx+dz*dz>1.0f) continue;
        const int floor=doorFloor(from),landing=doorFloor(to);
        if(!floor || !landing || std::abs(feet-float(floor))>1.0f) continue;
        physics_.teleport(actor,{to.x-float(originX_),float(landing)+body.halfExtent.y+.05f,to.y-float(originZ_)});
        update(viewDistance,true);
        player_.updateCamera(0);
        return true;
    }
    return false;
}

int ChunkWorld::doorFloor(Vec2 cell) const {
    const auto x=std::int64_t(std::floor(cell.x)),z=std::int64_t(std::floor(cell.y));
    for(int y=TerrainGenerator::maxHeight-3;y>=0;--y) {
        const auto id=blockAt(x,y,z);
        if(!id.empty() && generator_.definition().blocks.at(id).solid &&
            blockAt(x,y+1,z).empty() && blockAt(x,y+2,z).empty()) return y+1;
    }
    return 0;
}
void ChunkWorld::restoreOrigin(std::int64_t x,std::int64_t z) {
    if(x%16 || z%16 || std::abs(double(x))>1e12 || std::abs(double(z))>1e12) throw std::runtime_error("Invalid world origin");
    clear(); originX_=x;originZ_=z;
}
int ChunkWorld::localHeight(float x,float z) const { return generator_.height(originX_+std::int64_t(std::floor(x)),originZ_+std::int64_t(std::floor(z))); }
std::string ChunkWorld::blockAt(std::int64_t x,int y,std::int64_t z) const {
    if(y<0 || y>=TerrainGenerator::maxHeight) return {};
    if(const auto* map=generator_.finiteMap();map && !map->areaAt(x,z)) return {};
    const auto value=edits_.get(x,y,z);
    return value?*value:generator_.block(x,y,z);
}
std::vector<std::string> ChunkWorld::columnBlocks(std::int64_t x,std::int64_t z,bool edited) const {
    auto blocks=generator_.column(x,z).blocks;blocks.resize(TerrainGenerator::maxHeight);
    const auto cx=floorDiv(x,16),cz=floorDiv(z,16);
    const Key key{cx,cz};auto it=decorationCache_.find(key);
    if(it==decorationCache_.end()) {
        if(decorationCache_.size()>=256) decorationCache_.clear();
        it=decorationCache_.emplace(key,generator_.decoration(cx,cz)).first;
    }
    for(const auto& c:it->second) if(c.x==x-cx*16 && c.z==z-cz*16) blocks[size_t(c.y)]=c.block;
    if(edited) for(int y=0;y<TerrainGenerator::maxHeight;++y) if(const auto edit=edits_.get(x,y,z)) blocks[size_t(y)]=*edit;
    return blocks;
}
std::optional<PhysicsSystem::RayHit> ChunkWorld::raycast(Vec3 from,Vec3 to) const {
    const double start[3]={from.x,from.y,from.z},end[3]={to.x,to.y,to.z};double delta[3],next[3],increment[3];int step[3];std::int64_t cell[3];
    double distance=0;
    for(size_t i=0;i<3;++i) {
        if(!std::isfinite(start[i]) || !std::isfinite(end[i]) || std::abs(start[i])>100000 || std::abs(end[i])>100000) throw std::runtime_error("Terrain ray exceeds local coordinate range");
        delta[i]=end[i]-start[i];distance+=std::abs(delta[i]);cell[i]=std::int64_t(std::floor(start[i]));step[i]=delta[i]>0?1:-1;
        increment[i]=delta[i]==0?std::numeric_limits<double>::infinity():std::abs(1/delta[i]);
        next[i]=delta[i]==0?increment[i]:((step[i]>0?cell[i]+1:cell[i])-start[i])/delta[i];
    }
    if(distance>4096) throw std::runtime_error("Terrain ray exceeds 4096-cell traversal bound");
    if(distance==0) return {};
    std::map<Key,std::vector<std::string>> columns;double fraction=0;Vec3 normal{};
    for(unsigned visited=0;visited<4100 && fraction<=1;++visited) {
        if(cell[1]>=0 && cell[1]<48) {
            const Key key{originX_+cell[0],originZ_+cell[2]};auto found=columns.find(key);
            if(found==columns.end()) found=columns.emplace(key,columnBlocks(key.first,key.second)).first;
            const auto& block=found->second[size_t(cell[1])];
            if(!block.empty() && generator_.definition().blocks.at(block).solid) return PhysicsSystem::RayHit{{},float(fraction),normal};
        }
        const size_t axis=next[0]<=next[1] && next[0]<=next[2]?0:next[1]<=next[2]?1:2;
        fraction=next[axis];next[axis]+=increment[axis];cell[axis]+=step[axis];normal={};
        if(axis==0) normal.x=float(-step[axis]);else if(axis==1) normal.y=float(-step[axis]);else normal.z=float(-step[axis]);
    }
    return {};
}
bool ChunkWorld::setBlock(std::int64_t x,int y,std::int64_t z,const std::string& block) {
    return editBlocks({{{x,y,z},block}})!=0;
}
size_t ChunkWorld::editBlocks(const BlockEdits& changes) {
    if(changes.size()>4096) throw std::runtime_error("Batch edit limit (4096) exceeded");
    BlockEdits actual,inverse;std::map<Key,std::vector<std::string>> columns;
    for(const auto& [cell,block]:changes) {
        const auto [x,y,z]=cell;
        if(std::abs(double(x))>1e12 || std::abs(double(z))>1e12 || y<0 || y>=TerrainGenerator::maxHeight) throw std::runtime_error("World edit outside supported coordinates");
        if(const auto* map=generator_.finiteMap();map && !map->areaAt(x,z)) throw std::runtime_error("World edit outside map bounds");
        if(!block.empty() && !generator_.definition().blocks.contains(block)) throw std::runtime_error("Unknown block: "+block);
        const Key key{x,z};auto it=columns.find(key);if(it==columns.end()) it=columns.emplace(key,columnBlocks(x,z)).first;
        const auto& old=it->second[size_t(y)];if(old!=block) { actual[cell]=block;inverse[cell]=old; }
    }
    if(actual.empty()) return 0;
    undo_.push_back(std::move(inverse));try { commitEdits(actual); }catch(...) { undo_.pop_back();throw; }
    if(undo_.size()>16) undo_.erase(undo_.begin());redo_.clear();return actual.size();
}
void ChunkWorld::commitEdits(const BlockEdits& changes) {
    RegionEdits next(storageDirectory_);std::set<Key> affected;std::map<Key,BlockEdits> pages;
    for(const auto& [cell,block]:changes) {
        const auto [x,y,z]=cell;const auto cx=floorDiv(x,16),cz=floorDiv(z,16);const Key key{cx,cz};
        auto it=pages.find(key);if(it==pages.end()) it=pages.emplace(key,edits_.chunk(key)).first;
        if(columnBlocks(x,z,false)[size_t(y)]==block) it->second.erase(cell);else it->second[cell]=block;
        affected.insert(key);
        if(x-cx*16==0) affected.emplace(cx-1,cz);if(x-cx*16==15) affected.emplace(cx+1,cz);
        if(z-cz*16==0) affected.emplace(cx,cz-1);if(z-cz*16==15) affected.emplace(cx,cz+1);
    }
    for(auto& [key,page]:pages) next.stage(key,std::move(page));
    std::map<Key,Chunk> staged;
    try {
        for(auto key:affected) {
            const auto old=chunks_.find(key);if(old==chunks_.end()) continue;
            auto& chunk=staged[key];auto local=edits_.neighborhood(key);
            for(const auto& [cell,block]:changes) { const auto [x,y,z]=cell;(void)y;if(x>=key.first*16-1 && x<=key.first*16+16 && z>=key.second*16-1 && z<=key.second*16+16) local[cell]=block; }
            chunk=makeChunk(buildChunk(generator_,key.first,key.second,&local));
            if(!old->second.colliders.empty()) collide(key,chunk,true);
        }
        edits_.preparePublish(next);
    } catch(...) { for(auto& [key,chunk]:staged) { (void)key;unload(chunk); }throw; }
    for(auto& [key,chunk]:staged) { auto& old=chunks_.at(key);unload(old);std::swap(old,chunk); }
    edits_.publish(std::move(next));worker_->reset();
}
bool ChunkWorld::undoEdits() {
    if(undo_.empty()) return false;BlockEdits inverse;for(const auto& [cell,block]:undo_.back()) { (void)block;const auto [x,y,z]=cell;inverse[cell]=blockAt(x,y,z); }
    redo_.push_back(std::move(inverse));try { commitEdits(undo_.back()); }catch(...) { redo_.pop_back();throw; }undo_.pop_back();return true;
}
bool ChunkWorld::redoEdits() {
    if(redo_.empty()) return false;BlockEdits inverse;for(const auto& [cell,block]:redo_.back()) { (void)block;const auto [x,y,z]=cell;inverse[cell]=blockAt(x,y,z); }
    undo_.push_back(std::move(inverse));try { commitEdits(redo_.back()); }catch(...) { undo_.pop_back();throw; }redo_.pop_back();return true;
}
RegionEdits ChunkWorld::readLegacyEdits(const BlockEdits& edits) const {
    RegionEdits next(storageDirectory_);std::map<Key,BlockEdits> pages;
    for(const auto& [cell,block]:edits) {
        const auto [x,y,z]=cell;
        if(std::abs(double(x))>1e12 || std::abs(double(z))>1e12 || y<0 || y>=TerrainGenerator::maxHeight || (!block.empty() && !generator_.definition().blocks.contains(block))) throw std::runtime_error("Invalid saved block edit");
        pages[{floorDiv(x,16),floorDiv(z,16)}][cell]=block;
    }
    for(auto& [key,page]:pages) next.replace(key,std::move(page));return next;
}
void ChunkWorld::restoreBlockEdits(BlockEdits edits) { restoreEditSnapshot(readLegacyEdits(edits)); }
RegionEdits ChunkWorld::readEditSnapshot(const nlohmann::json& value) const { return RegionEdits::restore(storageDirectory_,value,generator_.definition()); }
void ChunkWorld::restoreEditSnapshot(RegionEdits edits) { clear();edits_=std::move(edits);undo_.clear();redo_.clear(); }
ChunkWorld::Chunk ChunkWorld::makeChunk(ChunkData data) {
    Chunk chunk;chunk.boxes=std::move(data.boxes);
    for(const auto& [id,mesh]:data.meshes) { Material material;material.texture=textures_.at(id);material.shader=shaders_.at(id);chunk.items.push_back({renderer_.createMesh(mesh.vertices,mesh.indices),material,{}}); }
    for(const auto& placement:data.models) {
        const auto& model=blockModels_.at(placement.id);
        Transform transform;transform.position=placement.position;
        const auto matrix=transformMatrix(transform);
        for(auto part:model->parts) {part.world=multiply(part.world,matrix);chunk.items.push_back(std::move(part));}
    }
    return chunk;
}
void ChunkWorld::install(Key key,ChunkData data) {chunks_.emplace(key,makeChunk(std::move(data)));}
void ChunkWorld::load(Key key) { const auto local=edits_.neighborhood(key);install(key,buildChunk(generator_,key.first,key.second,&local)); }
ChunkWorld::StreamingStats ChunkWorld::streamingStats() const {
    std::lock_guard lock(worker_->mutex);return {worker_->requested.size(),worker_->ready.size(),chunks_.size(),edits_.residentPages(),edits_.pageCount(),worker_->generated,worker_->discarded,worker_->generationMs,worker_->commitMs};
}
std::vector<std::pair<Vec3,Vec3>> ChunkWorld::navigationObstacles(const std::vector<EntityId>& ignored) const {
    std::vector<std::pair<Vec3,Vec3>> result;
    for(auto id:physics_.entities()) if(!transient(id) && std::find(ignored.begin(),ignored.end(),id)==ignored.end()) {
        const auto body=physics_.description(id);const auto p=world_.get<TransformComponent>(id).local.position;const auto h=body.halfExtent;
        result.push_back({{p.x-h.x,p.y-h.y,p.z-h.z},{p.x+h.x,p.y+h.y,p.z+h.z}});
    }
    return result;
}
void ChunkWorld::collide(Key key,Chunk& chunk,bool enabled) {
    if(!enabled) { if(!chunk.colliders.empty()) unload(chunk); return; }
    if(!chunk.colliders.empty()) return;
    try {
        for(const auto& b:chunk.boxes) {
            auto id=world_.create("runtime_chunk"); chunk.colliders.push_back(id);transient_.insert(id);
            auto p=b.center;p.x+=float(key.first*16-originX_);p.z+=float(key.second*16-originZ_);
            world_.add(id,TransformComponent{{p}});physics_.attachBox(id,b.half,false);
        }
    } catch(...) { unload(chunk); throw; }
}
void ChunkWorld::recenter() {
    const auto entity=player_.entity();if(!entity) return;
    auto p=world_.get<TransformComponent>(entity).local.position;
    if(std::abs(p.x)<128 && std::abs(p.z)<128) return;
    const auto dx=std::int64_t(std::floor(p.x/16))*16,dz=std::int64_t(std::floor(p.z/16))*16;
    // 64-bit global coordinates; all render/physics objects stay near the origin.
    if(std::abs(double(originX_+dx))>1e12 || std::abs(double(originZ_+dz))>1e12) throw std::runtime_error("World coordinate safety limit reached");
    for(auto& [key,chunk]:chunks_) { (void)key;unload(chunk); }
    for(auto id:world_.entities()) if(!world_.parent(id) && world_.has<TransformComponent>(id)) {
        auto& t=world_.get<TransformComponent>(id).local;auto position=t.position;position.x-=float(dx);position.z-=float(dz);
        if(physics_.contains(id)) { auto velocity=physics_.velocity(id);physics_.teleport(id,position);physics_.setVelocity(id,velocity); }
        else t.position=position;
    }
    originX_+=dx;originZ_+=dz;player_.possess(entity,player_.speed());
}
void ChunkWorld::update(int viewDistance,bool immediate,std::optional<Vec3> viewCenter) {
    { std::lock_guard lock(worker_->mutex);if(worker_->fatal) std::rethrow_exception(worker_->fatal); }
    if(viewCenter && (!std::isfinite(viewCenter->x) || !std::isfinite(viewCenter->z) || std::abs(viewCenter->x)>100000 || std::abs(viewCenter->z)>100000)) throw std::runtime_error("Invalid streaming view center");
    if(!player_.entity()) return;
    const auto oldX=originX_,oldZ=originZ_;
    recenter(); viewDistance=std::clamp(viewDistance,2,5);
    if(viewCenter) {
        viewCenter->x=std::clamp(viewCenter->x-float(originX_-oldX),-100000.0f,100000.0f);viewCenter->z=std::clamp(viewCenter->z-float(originZ_-oldZ),-100000.0f,100000.0f);
    }
    const auto p=viewCenter.value_or(world_.get<TransformComponent>(player_.entity()).local.position);
    const auto cx=floorDiv(originX_+std::int64_t(std::floor(p.x)),16),cz=floorDiv(originZ_+std::int64_t(std::floor(p.z)),16);
    worker_->prune({cx,cz},viewDistance);
    // A visible chunk can still support an actor outside the player's physics ring.
    // Include the body's footprint and a small margin for movement across seams.
    std::set<Key> occupied;
    for(auto id:physics_.entities(true)) {
        const auto body=physics_.description(id);
        const auto position=world_.get<TransformComponent>(id).local.position;
        const auto minX=floorDiv(originX_+std::int64_t(std::floor(double(position.x)-body.halfExtent.x-1)),16);
        const auto maxX=floorDiv(originX_+std::int64_t(std::floor(double(position.x)+body.halfExtent.x+1)),16);
        const auto minZ=floorDiv(originZ_+std::int64_t(std::floor(double(position.z)-body.halfExtent.z-1)),16);
        const auto maxZ=floorDiv(originZ_+std::int64_t(std::floor(double(position.z)+body.halfExtent.z+1)),16);
        for(auto z=minZ;z<=maxZ;++z) for(auto x=minX;x<=maxX;++x) occupied.emplace(x,z);
    }
    for(auto it=chunks_.begin();it!=chunks_.end();) {
        const auto distance=std::max(std::abs(it->first.first-cx),std::abs(it->first.second-cz));
        const bool needed=occupied.contains(it->first);
        if(distance>viewDistance && !needed) { unload(it->second);it=chunks_.erase(it); }
          else { collide(it->first,it->second,(!viewCenter && distance<=1) || needed);++it; }
    }
    // Physics support is synchronous even when visual streaming has exhausted its budget.
    for(auto key:occupied) { if(!chunks_.contains(key)) load(key);collide(key,chunks_.at(key),true); }
    // Upload at most two completed chunks or four milliseconds per frame.
    const auto frameStart=std::chrono::steady_clock::now();
    for(unsigned committed=0;committed<2 && std::chrono::steady_clock::now()-frameStart<std::chrono::milliseconds(4);++committed) {
        Worker::Result result;
        { std::lock_guard lock(worker_->mutex);if(worker_->ready.empty()) break;result=std::move(worker_->ready.front());worker_->ready.pop_front();worker_->requested.erase(result.key);worker_->condition.notify_all(); }
        if(result.error) std::rethrow_exception(result.error);
        if(chunks_.contains(result.key) || std::max(std::abs(result.key.first-cx),std::abs(result.key.second-cz))>viewDistance) continue;
        install(result.key,std::move(result.data));
    }
    { std::lock_guard lock(worker_->mutex);worker_->commitMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count(); }
    for(int ring=0;ring<=viewDistance;++ring) for(int z=-ring;z<=ring;++z) for(int x=-ring;x<=ring;++x) {
        if(std::max(std::abs(x),std::abs(z))!=ring) continue;
        const Key key{cx+x,cz+z};if(chunks_.contains(key)) continue;
        // Spawn, teleports and immediate initialization must have physical support.
        if(immediate || (!viewCenter && ring<=1)) { load(key);collide(key,chunks_.at(key),(!viewCenter && ring<=1) || occupied.contains(key));continue; }
        { std::lock_guard lock(worker_->mutex);if(worker_->requested.contains(key)) continue;if(worker_->requested.size()>=24) return; }
        auto local=edits_.window(key);
        { std::lock_guard lock(worker_->mutex);worker_->requested.insert(key);worker_->jobs.push_back({key,std::move(local),worker_->epoch}); }
        worker_->condition.notify_one();
    }
}
std::vector<RenderItem> ChunkWorld::renderItems() const {
    std::vector<RenderItem> result;
    for(const auto& [key,chunk]:chunks_) { Transform transform;transform.position={float(key.first*16-originX_),0,float(key.second*16-originZ_)}; const auto matrix=transformMatrix(transform);
        for(auto item:chunk.items) { item.world=multiply(item.world,matrix);result.push_back(std::move(item)); }
        if(const auto found=props_.find(key);found!=props_.end()) for(auto item:found->second) {
            item.world=multiply(item.world,matrix);result.push_back(std::move(item));
        }
    }
    for(const auto& [from,to]:doors_) {
        (void)to;
        const auto x=std::int64_t(std::floor(from.x)),z=std::int64_t(std::floor(from.y));
        if(!chunks_.contains({floorDiv(x,16),floorDiv(z,16)})) continue;
        const auto floor=doorFloor(from);
        if(!floor) continue;
        Transform transform;transform.position={from.x-float(originX_),float(floor)+.025f,from.y-float(originZ_)};
        Material material;material.color={1.f,.43f,.10f,1.f};material.unlit=true;material.doubleSided=true;
        result.push_back({doorMarker_,material,transformMatrix(transform)});
    }
    return result;
}
}
