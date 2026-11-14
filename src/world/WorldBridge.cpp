#include "world/WorldBridge.h"
#include "world/FiniteMap.h"
#include "world/WorldTools.h"
#include <cmath>
#include <charconv>
namespace game {
namespace {
std::pair<std::int64_t,std::int64_t> readOrigin(const Data& state) {
    const auto json=nlohmann::json::parse(encodeData(state));std::int64_t coordinates[2];size_t i=0;
    for(const auto* field:{"origin_x","origin_z"}) {
        const auto text=json.at(field).get<std::string>();const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),coordinates[i]);
        if(error!=std::errc{} || end!=text.data()+text.size() || coordinates[i]%16 || std::abs(double(coordinates[i]))>1e12) throw std::runtime_error("Invalid saved world origin");++i;
    }
    return {coordinates[0],coordinates[1]};
}
}
BlockEdits readLegacyBlockEdits(const Data& state,const WorldDefinition& definition) {
    const auto json=nlohmann::json::parse(encodeData(state));
    BlockEdits edits;
    if(!json.contains("blocks")) return edits; // Compatible with pre-edit checkpoints.
    const auto& rows=json.at("blocks");
    if(!rows.is_array() || rows.size()>4096) throw std::runtime_error("Invalid saved block edits count");
    const auto integer=[](const nlohmann::json& value) {
        if(!value.is_string()) throw std::runtime_error("Invalid saved block coordinate type");
        const auto str=value.get<std::string>();std::int64_t n=0;
        const auto [end,error]=std::from_chars(str.data(),str.data()+str.size(),n);
        if(error!=std::errc{} || end!=str.data()+str.size() || std::abs(double(n))>1e12) throw std::runtime_error("Invalid saved block coordinate");
        return n;
    };
    for(const auto& row:rows) {
        if(!row.is_array() || row.size()!=4 || !row[2].is_number() || !row[3].is_string()) throw std::runtime_error("Invalid saved block edit");
        const double height=row[2].get<double>();
        if(!std::isfinite(height) || height<0 || height>=TerrainGenerator::maxHeight || height!=std::floor(height)) throw std::runtime_error("Invalid saved block height");
        const auto x=integer(row[0]),z=integer(row[1]);const int y=int(height);const auto block=row[3].get<std::string>();
        if(y<0 || y>=TerrainGenerator::maxHeight || (!block.empty() && !definition.blocks.contains(block))) throw std::runtime_error("Invalid saved block ID/height");
        if(!edits.emplace(std::tuple{x,y,z},block).second) throw std::runtime_error("Duplicate saved block edit");
    }
    return edits;
}
void bindWorldPersistence(SaveGame& saves,ChunkWorld& world) {
    world.storageDirectory(saves.directory()/"terrain");
    const auto staged=std::make_shared<std::optional<RegionEdits>>();
    const auto previous=saves.validateState;
    saves.validateState=[previous,&world,staged](const Data& state){
        staged->reset();const auto& root=std::get<Data::Object>(state.value);const auto found=root.find("core");if(found==root.end()) throw std::runtime_error("Saved world origin missing");
        (void)readOrigin(found->second);auto legacy=readLegacyBlockEdits(found->second,world.generator().definition());
        const auto json=nlohmann::json::parse(encodeData(found->second));
        if(json.contains("regions")) *staged=world.readEditSnapshot(json.at("regions"));
        else *staged=world.readLegacyEdits(legacy);
        if(previous) previous(state);
    };
    saves.captureCore=[&world]{
        const auto x=world.originX(),z=world.originZ();auto edits=world.editStore();
        return [x,z,edits=std::move(edits)]() mutable {return decodeData(nlohmann::json{{"origin_x",std::to_string(x)},{"origin_z",std::to_string(z)},{"regions",edits.checkpoint()}}.dump());};
    };
    saves.didLoad=[&saves,&world,staged]{
        const auto [x,z]=readOrigin(saves.state("core"));world.restoreOrigin(x,z);
        if(*staged) { world.restoreEditSnapshot(std::move(**staged));staged->reset(); }
        else throw std::logic_error("Terrain load was not validated");
    };
    saves.includeEntity=[&world](EntityId id){return !world.transient(id);};
}
void bindWorldApi(script::Runtime& runtime,ChunkWorld& world,const WorldRecord& record) {
    using namespace script;
    bindWorldTools(runtime,world);
    runtime.bind("world_spawn",native([&world](const auto& a,const auto& at){
        if(!a.empty()) fail(at,"TERRAIN101","world_spawn takes no arguments");
        const auto* map=world.generator().finiteMap();
        const auto spawn=map?map->spawn():FiniteMap::Spawn{8,8};
        auto position=object();
        position.setMember("x",Value(double(spawn.x-world.originX())+.5),at);
        position.setMember("y",Value(double(world.generator().height(spawn.x,spawn.z))),at);
        position.setMember("z",Value(double(spawn.z-world.originZ())+.5),at);
        return position;
    }));
    const auto coordinates=[&world](const auto& a,const auto& at) {
        if(a.size()!=2) fail(at,"TERRAIN101","Expected local x, z");
        const double x=a[0].number(at),z=a[1].number(at);
        if(!std::isfinite(x+z) || std::abs(x)>100000 || std::abs(z)>100000) fail(at,"TERRAIN101","Invalid local coordinates");
        return std::pair{world.originX()+std::int64_t(std::floor(x)),world.originZ()+std::int64_t(std::floor(z))};
    };
    runtime.bind("terrain_biome",native([&world,coordinates](const auto& a,const auto& at){
        const auto [x,z]=coordinates(a,at);return Value(world.generator().biome(x,z)); }));
    runtime.bind("terrain_info",native([&world,coordinates](const auto& a,const auto& at){
        const auto [x,z]=coordinates(a,at);const auto column=world.generator().column(x,z);auto info=object();
        info.setMember("height",Value(double(column.height)),at);info.setMember("biome",Value(column.biome),at);
        info.setMember("surface",Value(column.height>0 ? column.blocks[size_t(column.height-1)] : std::string{}),at);return info; }));
    runtime.bind("terrain_block",native([&world,coordinates](const auto& a,const auto& at){
        if(a.size()!=3) fail(at,"TERRAIN101","terrain_block needs local x, y, z");
        const auto [x,z]=coordinates(std::vector<Value>{a[0],a[2]},at);const double y=a[1].number(at);
        if(!std::isfinite(y) || std::abs(y)>100000) fail(at,"TERRAIN101","Invalid height");
        return Value(world.blockAt(x,int(std::floor(y)),z)); }));
    runtime.bind("terrain_set_block",native([&world,coordinates](const auto& a,const auto& at){
        if(a.size()!=4) fail(at,"TERRAIN101","terrain_set_block needs local x, y, z, block id (empty for air)");
        const auto [x,z]=coordinates(std::vector<Value>{a[0],a[2]},at);const double y=a[1].number(at);
        if(!std::isfinite(y) || y!=std::floor(y) || y<0 || y>=TerrainGenerator::maxHeight) fail(at,"TERRAIN101","Invalid block height");
        try { return Value(world.setBlock(x,int(y),z,a[3].string(at))); }
        catch(const std::exception& error) { fail(at,"TERRAIN102",error.what()); }
    }));
    runtime.bind("world_noise",native([&world](const auto& a,const auto& at){
        if(a.size()!=4) fail(at,"TERRAIN101","world_noise needs global x, z, period, salt");
        std::int64_t n[4];for(size_t i=0;i<4;++i) { const double v=a[i].number(at);
            if(!std::isfinite(v) || std::abs(v)>1e12 || v!=std::floor(v)) fail(at,"TERRAIN101","Expected integer coordinate or noise parameter");n[i]=std::int64_t(v); }
        if(n[2]<16 || n[2]>4096 || n[3]<0 || n[3]>2147483647) fail(at,"TERRAIN101","Invalid noise period or salt");
        return Value(terrainNoise(world.generator().seed(),n[0],n[1],int(n[2]),std::uint64_t(n[3]))); }));
    runtime.bind("terrain_height",native([&world](const auto& a,const auto& at){
        if(a.size()!=2) fail(at,"TERRAIN101","terrain_height needs two local coordinates");
        auto x=a[0].number(at),z=a[1].number(at);if(!std::isfinite(x+z) || std::abs(x)>100000 || std::abs(z)>100000) fail(at,"TERRAIN101","Invalid local terrain coordinates");
        return Value(double(world.localHeight(float(x),float(z)))); }));
    runtime.bind("world_origin",native([&world](const auto& a,const auto& at){
        if(!a.empty()) fail(at,"TERRAIN101","world_origin takes no arguments");auto result=object();result.setMember("x",Value(double(world.originX())),at);result.setMember("z",Value(double(world.originZ())),at);return result; }));
    runtime.bind("terrain_clear",native([&world](const auto& a,const auto& at){
        if(a.size()!=2) fail(at,"TERRAIN101","terrain_clear needs local x, z");
        const double x=a[0].number(at),z=a[1].number(at);if(!std::isfinite(x+z) || std::abs(x)>100000 || std::abs(z)>100000) fail(at,"TERRAIN101","Invalid local coordinates");
        const auto gx=world.originX()+std::int64_t(std::floor(x)),gz=world.originZ()+std::int64_t(std::floor(z));const auto& generator=world.generator();const int height=generator.height(gx,gz);
        for(int y=height;y<height+3;++y){const auto block=world.blockAt(gx,y,gz);if(!block.empty() && generator.definition().blocks.at(block).solid)return Value(false);}return Value(true); }));
    runtime.bind("world_seed",native([seed=record.seed](const auto& a,const auto& at){if(!a.empty()) fail(at,"TERRAIN101","world_seed takes no arguments");return Value(seed);}));
    runtime.bind("world_generator",native([id=record.generator](const auto& a,const auto& at){if(!a.empty()) fail(at,"TERRAIN101","world_generator takes no arguments");return Value(id);}));
    runtime.bind("world_random",native([&world](const auto& a,const auto& at){
        if(a.size()!=3) fail(at,"TERRAIN101","world_random needs global x, z, salt");
        std::int64_t values[3];for(size_t i=0;i<3;++i){double n=a[i].number(at);if(!std::isfinite(n) || std::abs(n)>1e12 || n!=std::floor(n)) fail(at,"TERRAIN101","Expected integer coordinate");values[i]=std::int64_t(n);}
        return Value(double(coordinateHash(world.generator().seed(),values[0],values[1],std::uint64_t(values[2]))>>11)/9007199254740992.0); }));
}
}
