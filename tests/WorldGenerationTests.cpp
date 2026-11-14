#include "world/WorldStorage.h"
#include "world/ChunkWorld.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <set>
#include <cmath>
using namespace game;
using Json=nlohmann::json;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool rejected=false;try { f(); }catch(const std::exception&) { rejected=true; }check(rejected,"Invalid world content accepted"); }
Json fixture() {
    return Json::parse(R"({
      "version":1,
      "blocks":{"base:grass":{"texture":"base:grass"},"base:dirt":{"texture":"base:dirt"},"base:rock":{"texture":"base:rock"},"base:flower":{"texture":"base:flower","solid":false}},
      "structures":{"base:marker":[[0,0,0,"base:flower"]]},
      "generators":{"base:world":{"name":"Test","surface":"base:grass","fill":"base:dirt","rock":"base:rock","base_height":8,"relief":0,"period":64,"structures":[{"structure":"base:marker","chance":1}]}}
    })");
}
Json cells(const std::vector<StructureCell>& value) { auto result=Json::array();for(const auto& c:value) result.push_back({c.x,c.y,c.z,c.block});return result; }
void write(const std::filesystem::path& path,const Json& value) { std::filesystem::create_directories(path.parent_path());std::ofstream(path)<<value.dump(2); }
ModManifest mod(std::string id,const std::filesystem::path& root) { ModManifest result;result.id=id;result.version=Version::parse("1.0.0");result.root=root;return result; }
}
int main() {
    try {
        const auto old=fixture();const auto oldDefinition=WorldDefinition::parse(old);
        TerrainGenerator legacy(oldDefinition,"base:world","legacy");
        check(legacy.biome(0,0).empty() && legacy.decoration(0,0).empty(),"Legacy arrival point/biome changed");
        check(cells(legacy.decoration(-3,2))==Json::parse("[[8,8,8,\"base:flower\"]]"),"Legacy decoration changed");
        check(legacy.block(1,7,1)=="base:grass" && legacy.block(1,6,1)=="base:dirt" && legacy.block(1,4,1)=="base:rock","Legacy terrain layers changed");
        TerrainGenerator legacyRoundtrip(WorldDefinition::parse(oldDefinition.json()),"base:world","legacy");
        for(int z=-10;z<10;++z) for(int x=-10;x<10;++x) check(cells(legacy.decoration(x,z))==cells(legacyRoundtrip.decoration(x,z)),"Legacy snapshot changed after roundtrip");

        auto data=old;auto& g=data["generators"]["base:world"];
        g["structure_mode"]="scatter";g["safe_radius"]=-1;
        g["layers"]=Json::parse(R"([{"block":"base:dirt","depth":2},{"block":"base:grass","depth":1}])");
        g["structures"]=Json::parse(R"([{"id":"base:flowers","structure":"base:marker","chance":1,"attempts":8,"spacing":2,"salt":91}])");
        auto definition=WorldDefinition::parse(data);TerrainGenerator scatter(definition,"base:world","flowers");
        check(scatter.block(0,7,0)=="base:dirt" && scatter.block(0,5,0)=="base:grass" && scatter.block(0,4,0)=="base:rock","Custom soil depth/order ignored");
        auto decorations=scatter.decoration(-2,3);check(decorations.size()>1,"Scatter still limited to one feature per chunk");
        std::set<std::tuple<int,int,int>> positions;
        for(const auto& c:decorations) { check(c.x>=1 && c.x<=14 && c.z>=1 && c.z<=14 && c.y==8,"Scatter escaped chunk or terrain");check(positions.emplace(c.x,c.y,c.z).second,"Overlapping scatter cells");check(scatter.block(-32+c.x,c.y,48+c.z)==c.block,"Block query differs from generation"); }
        auto first=cells(decorations);(void)scatter.decoration(30,-51);check(cells(scatter.decoration(-2,3))==first,"Generation depends on visit order");
        TerrainGenerator another(definition,"base:world","different");check(cells(another.decoration(-2,3))!=first,"Scatter ignores seed");
        auto extra=data;extra["generators"]["base:world"]["structures"].push_back(Json{{"id","base:disabled"},{"structure","base:marker"},{"chance",0}});
        check(cells(TerrainGenerator(WorldDefinition::parse(extra),"base:world","flowers").decoration(-2,3))==first,"Unrelated disabled rule changes random samples");
        auto safe=data;safe["generators"]["base:world"]["safe_radius"]=1;TerrainGenerator safeGenerator(WorldDefinition::parse(safe),"base:world","flowers");
        check(safeGenerator.decoration(-1,1).empty() && !safeGenerator.decoration(2,0).empty(),"Configurable arrival radius ignored");

        auto restricted=data;restricted["generators"]["base:world"]["structures"][0]["min_height"]=9;
        check(TerrainGenerator(WorldDefinition::parse(restricted),"base:world","flowers").decoration(-2,3).empty(),"Altitude filter ignored");
        auto large=data;large["structures"]["base:large"]=Json::array({Json::array({-6,0,0,"base:flower"}),Json::array({6,0,0,"base:flower"})});
        large["generators"]["base:world"]["structures"]=Json::parse(R"([{"id":"base:large","structure":"base:large","chance":1,"rotations":[90],"y_offset":2}])");
        const auto rotated=TerrainGenerator(WorldDefinition::parse(large),"base:world","flowers").decoration(-1,-1);
        check(rotated.size()==2 && rotated[0].x==rotated[1].x && std::abs(rotated[0].z-rotated[1].z)==12 && rotated[0].y==10,"Large structure rotation/offset broken");

        data["biomes"]=Json::parse(R"({
          "base:meadow":{"name":"Meadow","rock":"base:rock","layers":[{"block":"base:grass","depth":1},{"block":"base:dirt","depth":2}]},
          "base:barren":{"name":"Barren","rock":"base:rock","layers":[{"block":"base:rock","depth":2}]}
        })");
        g["biomes"]=Json::parse(R"([{"biome":"base:meadow","weight":1},{"biome":"base:barren","weight":1}])");
        g["biome_period"]=32;g["biome_salt"]=123;g["structures"][0]["biomes"]=Json::array({"base:meadow"});
        g["noise"]=Json::parse(R"([{"period":64,"amplitude":0.75,"salt":17},{"period":16,"amplitude":0.25,"salt":42}])");
        definition=WorldDefinition::parse(data);TerrainGenerator mixed(definition,"base:world","biomes");
        std::set<std::string> seen;size_t filteredCells=0;
        for(int z=-128;z<=128;z+=7) for(int x=-128;x<=128;x+=7) {
            auto column=mixed.column(x,z);seen.insert(column.biome);
            check(column.height>=7 && column.height<=9 && std::abs(column.height-mixed.height(x+1,z))<=1,"Noise created excessive relief or a seam");
            check(column.blocks.back()==(column.biome=="base:meadow" ? "base:grass" : "base:rock"),"Biome palette ignored");
        }
        check(seen.size()==2,"Biome selection never changes");
        for(int z=-4;z<=4;++z) for(int x=-4;x<=4;++x) for(const auto& c:mixed.decoration(x,z)) {
            check(mixed.biome(x*16+c.x,z*16+c.z)=="base:meadow","Biome placement filter ignored");++filteredCells;
        }
        check(filteredCells>0,"Biome filter rejected all valid features");
        auto slope=old;slope["structures"]["base:pair"]=Json::parse("[[0,0,0,\"base:flower\"],[6,0,0,\"base:flower\"]]");
        slope["generators"]["base:world"].update(Json::parse(R"({"relief":1,"period":16,"structure_mode":"scatter","safe_radius":-1,"structures":[{"id":"base:pair","structure":"base:pair","chance":1,"rotations":[0],"alignment":"surface","max_slope":0}]})"));
        TerrainGenerator levelOnly(WorldDefinition::parse(slope),"base:world","slopes");
        slope["generators"]["base:world"]["structures"][0]["max_slope"]=8;TerrainGenerator followSurface(WorldDefinition::parse(slope),"base:world","slopes");
        size_t rejectedSlopes=0;
        for(int z=-6;z<=6;++z) for(int x=-6;x<=6;++x) {
            const auto level=levelOnly.decoration(x,z),following=followSurface.decoration(x,z);
            check(following.size()==2 && (level.empty() || level.size()==2),"Structure was partially placed");
            for(const auto& c:following) check(c.y==followSurface.height(x*16+c.x,z*16+c.z),"Surface alignment ignored");
            if(following[0].y!=following[1].y) { check(level.empty(),"Slope filter ignored");++rejectedSlopes; }
        }
        check(rejectedSlopes>0,"Slope test did not exercise an uneven footprint");
        const auto mesh=buildChunk(mixed,0,0);check(!mesh.meshes.empty(),"Biome terrain produced no mesh");
        auto roundtrip=WorldDefinition::parse(definition.json());check(roundtrip.json()==definition.json(),"Extended snapshot fields lost");
        TerrainGenerator restored(roundtrip,"base:world","biomes");
        for(auto coordinate:{-100000000000LL,-129LL,0LL,127LL,100000000000LL}) {
            check(restored.height(coordinate,-coordinate)==mixed.height(coordinate,-coordinate),"Distant noise unstable");
            check(restored.column(coordinate,-coordinate).blocks==mixed.column(coordinate,-coordinate).blocks,"Distant soil unstable");
            check(cells(restored.decoration(coordinate/16,-coordinate/16))==cells(mixed.decoration(coordinate/16,-coordinate/16)),"Distant features unstable");
        }
        for(const auto& key:{"biome_period","safe_radius"}) { auto bad=data;bad["generators"]["base:world"][key]=10000;rejects([&]{WorldDefinition::parse(bad);}); }
        auto bad=data;bad["generators"]["base:world"]["noise"][0]["amplitude"]=4;rejects([&]{WorldDefinition::parse(bad);});
        bad=data;bad["biomes"]["base:meadow"]["layers"][0]["block"]="base:flower";rejects([&]{WorldDefinition::parse(bad);});
        bad=data;bad["generators"]["base:world"]["structures"][0]["rotations"]=Json::array({45});rejects([&]{WorldDefinition::parse(bad);});
        bad=data;bad["generators"]["base:world"]["structures"][0]["biomes"]=Json::array({"base:missing"});rejects([&]{WorldDefinition::parse(bad);});
        bad=data;bad["generators"]["base:world"]["structures"].push_back(bad["generators"]["base:world"]["structures"][0]);rejects([&]{WorldDefinition::parse(bad);});
        bad=data;bad["generators"]["base:world"]["structures"][0]["attempts"]=100;rejects([&]{WorldDefinition::parse(bad);});

        const auto root=std::filesystem::path("user/world-generation-tests")/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto baseMod=mod("base",root/"base"),addon=mod("addon",root/"addon");addon.dependencies.push_back({"base",">=",Version::parse("1.0.0"),false});
        write(baseMod.root/"world.json",data);
        auto addonData=Json::parse(R"({
          "version":1,
          "structures":{"addon:marker":[[0,0,0,"base:flower"]]},
          "generators":{"addon:flat":{"extends":"base:world","name":"Flat","noise":[],"relief":0,"structures":[]}},
          "generator_extensions":{"addon:flowers":{"generator":"base:world","structures":[{"id":"addon:flowers","structure":"addon:marker","chance":1,"attempts":2}]}}
        })");
        write(addon.root/"world.json",addonData);
        auto loaded=WorldDefinition::load({baseMod,addon});auto reversed=WorldDefinition::load({addon,baseMod});
        check(loaded.json()==reversed.json(),"Mod discovery order changes world definition");
        check(loaded.generators.at("base:world").structures.size()==2 && loaded.generators.at("addon:flat").structures.empty(),"Inheritance/extensions modified the wrong preset");
        TerrainGenerator flat(loaded,"addon:flat","flat");check(flat.height(100,-120)==8,"Inherited override ignored");
        WorldStorage storage(root/"worlds");auto saved=storage.create("Snapshot","biomes","base:world",loaded,{{"base","1.0.0"},{"addon","1.0.0"}});
        addonData["generators"]["addon:flat"]["base_height"]=12;write(addon.root/"world.json",addonData);
        check(storage.read(saved.id).definition.json()==loaded.json(),"Mod edits mutated existing world snapshot");
        auto missingDependency=addon;missingDependency.dependencies.clear();rejects([&]{WorldDefinition::load({baseMod,missingDependency});});
        addonData["generators"]["addon:flat"]["extends"]="addon:flat";write(addon.root/"world.json",addonData);rejects([&]{WorldDefinition::load({baseMod,addon});});
        auto official=discoverMods("content");check(official.diagnostics.empty(),"Default mod discovery failed");
        official.mods.push_back(readManifest("examples/worldgen-addons/gardens"));const auto example=WorldDefinition::load(official.mods);
        check(example.generators.contains("gardens:quiet") && example.generators.at("plains:gentle").structures.back().id=="gardens:extra_flowers","Shipped addon failed to extend official content");
        TerrainGenerator garden(example,"gardens:quiet","example");check(garden.biome(0,0)=="gardens:flower_field" && garden.decoration(1,1).empty(),"Shipped addon preset is not usable");
        std::cout<<"PASS world generation: legacy snapshots, soil, biomes, noise, scatter, filters, rotations, stable seeds, inheritance, addons and validation\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL world generation: "<<e.what()<<'\n';return 1; }
}

