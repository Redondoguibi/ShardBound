#pragma once
#include "world/WorldDefinition.h"
#include "game/PlayerController.h"
#include "world/RegionEdits.h"
#include <set>
#include <tuple>
namespace game {
struct ChunkMesh { std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; };
struct TerrainBox { Vec3 center,half; };
struct ChunkBlockModel { std::string id;Vec3 position; };
struct ChunkData { std::map<std::string,ChunkMesh> meshes; std::vector<TerrainBox> boxes;std::vector<ChunkBlockModel> models; };
// Sparse player/mod edits; an empty block ID denotes air.
ChunkData buildChunk(const TerrainGenerator& generator,std::int64_t x,std::int64_t z,const BlockEdits* edits=nullptr);
class ChunkWorld {
public:
    ChunkWorld(Renderer& renderer,AssetManager& assets,World& world,PhysicsSystem& physics,PlayerController& player,TerrainGenerator generator);
    ~ChunkWorld();
    void update(int viewDistance,bool immediate=false,std::optional<Vec3> viewCenter={});
    void clear();
    bool useDoor(int viewDistance);
    std::vector<RenderItem> renderItems() const;
    bool transient(EntityId entity) const { return transient_.contains(entity); }
    int localHeight(float x,float z) const;
    std::string blockAt(std::int64_t x,int y,std::int64_t z) const;
    bool setBlock(std::int64_t x,int y,std::int64_t z,const std::string& block);
    size_t editBlocks(const BlockEdits& changes);
    bool undoEdits();
    bool redoEdits();
    std::vector<std::string> columnBlocks(std::int64_t x,std::int64_t z,bool edited=true) const;
    std::optional<PhysicsSystem::RayHit> raycast(Vec3 from,Vec3 to) const;
    BlockEdits blockEdits() const { return edits_.all(); }
    void storageDirectory(const std::filesystem::path& path) { edits_.directory(path);storageDirectory_=path; }
    nlohmann::json editSnapshot() const { return edits_.snapshot(); }
    RegionEdits editStore() const { return edits_; }
    RegionEdits readEditSnapshot(const nlohmann::json& value) const;
    RegionEdits readLegacyEdits(const BlockEdits& edits) const;
    void restoreEditSnapshot(RegionEdits edits);
    struct StreamingStats { size_t queued=0,ready=0,loaded=0,residentPages=0,pages=0;std::uint64_t generated=0,discarded=0;double generationMs=0,commitMs=0; };
    StreamingStats streamingStats() const;
    std::vector<std::pair<Vec3,Vec3>> navigationObstacles(const std::vector<EntityId>& ignored) const;
    void restoreBlockEdits(BlockEdits edits);
    Vec3 origin() const { return {float(originX_),0,float(originZ_)}; }
    std::int64_t originX() const { return originX_; }
    std::int64_t originZ() const { return originZ_; }
    void restoreOrigin(std::int64_t x,std::int64_t z);
    size_t loadedChunks() const { return chunks_.size(); }
    const TerrainGenerator& generator() const { return generator_; }
    World& ecs() const { return world_; }
private:
    using Key=std::pair<std::int64_t,std::int64_t>;
    struct Chunk { std::vector<RenderItem> items; std::vector<TerrainBox> boxes; std::vector<EntityId> colliders; };
    void load(Key key);
    void install(Key key,ChunkData data);
    Chunk makeChunk(ChunkData data);
    void collide(Key key,Chunk& chunk,bool enabled);
    void unload(Chunk& chunk);
    void recenter();
    int doorFloor(Vec2 cell) const;
    void commitEdits(const BlockEdits& changes);
    Renderer& renderer_; AssetManager& assets_; World& world_; PhysicsSystem& physics_; PlayerController& player_;
    TerrainGenerator generator_;
    std::map<std::string,TextureHandle> textures_;
    std::map<std::string,ShaderInstanceHandle> shaders_;
    std::map<std::string,ModelHandle> blockModels_;
    std::map<Key,Chunk> chunks_;
    RegionEdits edits_;
    std::vector<BlockEdits> undo_,redo_;
    mutable std::map<Key,std::vector<StructureCell>> decorationCache_;
    std::set<EntityId> transient_;
    std::int64_t originX_=0,originZ_=0;
    struct Worker;
    std::unique_ptr<Worker> worker_;
    std::filesystem::path storageDirectory_;
    std::vector<std::pair<Vec2,Vec2>> doors_;
    MeshHandle doorMarker_;
    std::map<Key,std::vector<RenderItem>> props_;
};
}
