#pragma once
#include "mods/Manifest.h"
#include "renderer/Renderer.h"
#include <nlohmann/json.hpp>
#include <map>
#include <memory>
#include <set>
namespace game {
class FiniteMap;
struct BlockDefinition { std::string texture; bool solid=true; std::string shader,shaderTexture;ShaderParameters shaderParameters{};std::string model,name; };
struct StructureCell { int x=0,y=0,z=0; std::string block; };
struct StructureDefinition { std::vector<StructureCell> cells; };
struct SoilLayer { std::string block; int depth=1; };
struct BiomeDefinition { std::string name,rock; std::vector<SoilLayer> layers; };
struct BiomeChoice { std::string biome; double weight=1; };
struct NoiseLayer { int period=64; double amplitude=1; std::uint64_t salt=17; };
struct StructurePlacement {
    std::string structure; double chance=0;
    std::string id,alignment="anchor";
    int attempts=1,minHeight=1,maxHeight=47,maxSlope=8,spacing=0,yOffset=0;
    std::uint64_t salt=0;
    std::vector<int> rotations{0,90,180,270};
    std::vector<std::string> biomes;
};
struct VoxelRule {
    std::string id,block;
    std::vector<std::string> replace,biomes;
    int minY=0,maxY=47,minSurface=-48,maxSurface=48,period=16;
    double threshold=0;
    std::uint64_t salt=0;
    bool protectSpawn=true;
};
struct GeneratorDefinition {
    std::string id,name,surface,fill,rock;
    int baseHeight=4,relief=1,period=48;
    std::vector<StructurePlacement> structures;
    std::vector<SoilLayer> layers;
    std::vector<NoiseLayer> noise;
    std::vector<VoxelRule> voxelRules;
    std::vector<BiomeChoice> biomes;
    int biomePeriod=192,safeRadius=0;
    std::uint64_t biomeSalt=7919;
    bool scatter=false;
};
// Serializable content contract. No biome, item or structure is built into the engine.
struct WorldDefinition {
    std::map<std::string,BlockDefinition> blocks;
    std::map<std::string,StructureDefinition> structures;
    std::map<std::string,BiomeDefinition> biomes;
    std::map<std::string,GeneratorDefinition> generators;
    std::map<std::string,std::shared_ptr<const FiniteMap>> maps;
    std::set<std::string> mapPlacementMods;
    bool hasTerrain(const std::string& id) const { return generators.contains(id) || maps.contains(id); }
    nlohmann::json json() const;
    static WorldDefinition parse(const nlohmann::json& json);
    static WorldDefinition load(const std::vector<ModManifest>& mods);
};
std::uint64_t seedHash(std::string_view seed);
std::uint64_t coordinateHash(std::uint64_t seed,std::int64_t x,std::int64_t z,std::uint64_t salt=0);
std::int64_t floorDiv(std::int64_t value,int divisor);
double terrainNoise(std::uint64_t seed,std::int64_t x,std::int64_t z,int period,std::uint64_t salt);
struct TerrainColumn { int height=0; std::string biome; std::vector<std::string> blocks; };
class TerrainGenerator {
public:
    TerrainGenerator(WorldDefinition definition,std::string generator,std::string seed);
    int height(std::int64_t x,std::int64_t z) const;
    std::string biome(std::int64_t x,std::int64_t z) const;
    TerrainColumn column(std::int64_t x,std::int64_t z) const;
    std::string block(std::int64_t x,int y,std::int64_t z) const;
    std::vector<StructureCell> decoration(std::int64_t chunkX,std::int64_t chunkZ) const;
    const WorldDefinition& definition() const { return definition_; }
    const GeneratorDefinition& settings() const { return settings_; }
    const FiniteMap* finiteMap() const { return map_.get(); }
    std::uint64_t seed() const { return seed_; }
    static constexpr int chunkSize=16, maxHeight=48;
private:
    WorldDefinition definition_;
    GeneratorDefinition settings_;
    std::uint64_t seed_;
    std::shared_ptr<const FiniteMap> map_;
};
}
