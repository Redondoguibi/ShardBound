#include "world/WorldDefinition.h"
#include "world/FiniteMap.h"
#include "assets/VirtualFileSystem.h"
#include "assets/AssetId.h"
#include <fstream>
#include <cmath>
#include <set>
#include <algorithm>
#include <functional>
namespace game {
namespace {
using Json=nlohmann::json;
void check(bool value,const std::string& message) { if(!value) throw std::runtime_error("World content: "+message); }
void object(const Json& j) { check(j.is_object(),"expected an object"); }
int integer(const Json& j,int low,int high) {
    check(j.is_number_integer(),"expected integer"); const auto n=j.get<std::int64_t>();
    check(n>=low && n<=high,"integer outside permitted range"); return int(n);
}
double number(const Json& j,double low,double high) {
    check(j.is_number(),"expected number");const double n=j.get<double>();
    check(std::isfinite(n) && n>=low && n<=high,"number outside permitted range");return n;
}
std::uint64_t salt(const Json& j) { return std::uint64_t(integer(j,0,2147483647)); }
std::uint64_t mix(std::uint64_t v) { v=(v^(v>>30))*0xbf58476d1ce4e5b9ULL; v=(v^(v>>27))*0x94d049bb133111ebULL; return v^(v>>31); }
void solid(const WorldDefinition& d,const std::string& id) { check(d.blocks.contains(id) && d.blocks.at(id).solid,"terrain layers require solid registered blocks: "+id); }
std::vector<SoilLayer> layers(const Json& j,const WorldDefinition& d) {
    check(j.is_array() && j.size()<=8,"soil requires at most 8 layers");std::vector<SoilLayer> result;int depth=0;
    for(const auto& entry:j) { object(entry);SoilLayer layer{entry.at("block").get<std::string>(),integer(entry.at("depth"),1,32)};solid(d,layer.block);depth+=layer.depth;result.push_back(layer); }
    check(depth<=48,"total soil depth exceeds 48");return result;
}
Json layerJson(const std::vector<SoilLayer>& layers) { auto result=Json::array();for(const auto& layer:layers) result.push_back({{"block",layer.block},{"depth",layer.depth}});return result; }
}
std::uint64_t seedHash(std::string_view seed) { std::uint64_t h=14695981039346656037ULL; for(unsigned char c:seed) { h^=c; h*=1099511628211ULL; } return h; }
std::uint64_t coordinateHash(std::uint64_t seed,std::int64_t x,std::int64_t z,std::uint64_t salt) { return mix(seed^mix(std::uint64_t(x))^mix(std::uint64_t(z)+0x9e3779b97f4a7c15ULL)^mix(salt)); }
std::int64_t floorDiv(std::int64_t value,int divisor) { auto q=value/divisor; if(value%divisor<0) --q; return q; }
double terrainNoise(std::uint64_t seed,std::int64_t x,std::int64_t z,int period,std::uint64_t salt) {
    const auto gx=floorDiv(x,period),gz=floorDiv(z,period);
    const auto smooth=[](double a) { return a*a*(3-2*a); };
    const double tx=smooth(double(x-gx*period)/period),tz=smooth(double(z-gz*period)/period);
    const auto sample=[&](auto a,auto b) { return double(coordinateHash(seed,a,b,salt)>>11)/9007199254740992.0; };
    const double a=sample(gx,gz)*(1-tx)+sample(gx+1,gz)*tx,b=sample(gx,gz+1)*(1-tx)+sample(gx+1,gz+1)*tx;
    return (a*(1-tz)+b*tz)*2-1;
}
WorldDefinition WorldDefinition::parse(const Json& j) {
    object(j); check(j.at("version")==1,"unsupported version"); WorldDefinition d;
    object(j.at("blocks"));object(j.at("structures"));object(j.at("generators"));
    for(const auto& [id,b]:j.at("blocks").items()) {
        (void)AssetId(id); object(b); auto texture=b.at("texture").get<std::string>(); (void)AssetId(texture);
        BlockDefinition block{texture,b.value("solid",true)};
        block.model=b.value("model",std::string{});
        block.name=b.value("name",std::string{});
        if(!block.model.empty()) (void)AssetId(block.model);
        if(b.contains("shader")) {
            const auto& shader=b.at("shader");object(shader);
            for(const auto& [key,value]:shader.items()) { (void)value;check(key=="id" || key=="parameters" || key=="texture","unknown block shader field: "+key); }
            block.shader=shader.at("id").get<std::string>();(void)AssetId(block.shader);
            block.shaderTexture=shader.value("texture",std::string{});if(!block.shaderTexture.empty()) (void)AssetId(block.shaderTexture);
            if(shader.contains("parameters")) {const auto& params=shader.at("parameters");check(params.is_array() && params.size()<=16,"shader parameters require at most 16 numbers");
                for(size_t i=0;i<params.size();++i) block.shaderParameters[i]=float(number(params[i],-std::numeric_limits<float>::max(),std::numeric_limits<float>::max()));}
        }
        d.blocks.emplace(id,std::move(block));
    }
    check(d.blocks.size()<=16384,"block catalog exceeds 16384 definitions");
    for(const auto& [id,s]:j.at("structures").items()) {
        (void)AssetId(id); check(s.is_array() && s.size()<=512,"structure cell limit is 512"); StructureDefinition structure; std::set<std::tuple<int,int,int>> occupied;
        for(const auto& cell:s) {
            check(cell.is_array() && cell.size()==4,"cell must be [x,y,z,block]");
            StructureCell c{integer(cell[0],-6,6),integer(cell[1],0,15),integer(cell[2],-6,6),cell[3].get<std::string>()};
            check(d.blocks.contains(c.block),"unknown structure block: "+c.block);
            check(occupied.emplace(c.x,c.y,c.z).second,"duplicate structure cell"); structure.cells.push_back(c);
        }
        d.structures.emplace(id,std::move(structure));
    }
    check(d.structures.size()<=64,"structure limit is 64");
    if(j.contains("biomes")) {
        object(j.at("biomes"));check(j.at("biomes").size()<=64,"biome limit is 64");
        for(const auto& [id,b]:j.at("biomes").items()) {
            (void)AssetId(id);object(b);
            BiomeDefinition biome{b.at("name").get<std::string>(),b.at("rock").get<std::string>(),layers(b.at("layers"),d)};
            check(!biome.name.empty() && biome.name.size()<=96 && !biome.layers.empty(),"biome requires a name and soil layers");solid(d,biome.rock);
            d.biomes.emplace(id,std::move(biome));
        }
    }
    for(const auto& [id,g]:j.at("generators").items()) {
        (void)AssetId(id); object(g);check(!g.contains("extends"),"generator inheritance must be resolved by the mod loader");
        GeneratorDefinition spec; spec.id=id; spec.name=g.at("name").get<std::string>();
        check(!spec.name.empty() && spec.name.size()<=96,"invalid generator name");
        spec.surface=g.at("surface"); spec.fill=g.at("fill"); spec.rock=g.at("rock");
        for(const auto& b:{spec.surface,spec.fill,spec.rock}) solid(d,b);
        spec.baseHeight=integer(g.at("base_height"),2,24); spec.relief=integer(g.at("relief"),0,4); spec.period=integer(g.at("period"),16,1024);
        spec.layers=layers(g.value("layers",Json::array()),d);
        spec.biomePeriod=integer(g.value("biome_period",Json(192)),16,4096);spec.biomeSalt=salt(g.value("biome_salt",Json(7919)));
        spec.safeRadius=integer(g.value("safe_radius",Json(0)),-1,8);
        const auto mode=g.value("structure_mode",std::string("legacy"));check(mode=="legacy" || mode=="scatter","unknown structure_mode");spec.scatter=mode=="scatter";
        const auto& noise=g.value("noise",Json::array());check(noise.is_array() && noise.size()<=8,"noise layer limit is 8");double relief=0;
        for(const auto& n:noise) { object(n);NoiseLayer layer{integer(n.at("period"),16,4096),number(n.at("amplitude"),0,4),salt(n.at("salt"))};relief+=layer.amplitude;spec.noise.push_back(layer); }
        if(noise.empty()) relief=spec.relief;
        check(relief<=4 && spec.baseHeight>relief,"total relief must be at most 4 and terrain must remain above zero");
        const auto& choices=g.value("biomes",Json::array());check(choices.is_array() && choices.size()<=32,"biome choice limit is 32");std::set<std::string> biomeIds;
        for(const auto& choice:choices) {
            object(choice);BiomeChoice b{choice.at("biome").get<std::string>(),number(choice.value("weight",Json(1)),0.001,1000)};
            check(d.biomes.contains(b.biome),"unknown biome: "+b.biome);check(biomeIds.insert(b.biome).second,"duplicate biome choice");spec.biomes.push_back(b);
        }
        const auto& rules=g.value("voxel_rules",Json::array());check(rules.is_array() && rules.size()<=8,"voxel rule limit is 8");std::set<std::string> voxelIds;
        for(const auto& entry:rules) {
            object(entry);VoxelRule rule;rule.id=entry.at("id");(void)AssetId(rule.id);check(voxelIds.insert(rule.id).second,"duplicate voxel rule ID");
            rule.block=entry.at("block");check(rule.block.empty() || d.blocks.contains(rule.block),"unknown voxel rule output block");
            const auto& replace=entry.at("replace");check(replace.is_array() && !replace.empty() && replace.size()<=128,"voxel rule requires replacement filters");
            for(const auto& value:replace) { auto b=value.get<std::string>();check(b.empty() || d.blocks.contains(b),"unknown voxel replacement block");rule.replace.push_back(b); }
            rule.minY=integer(entry.value("min_y",Json(0)),0,47);rule.maxY=integer(entry.value("max_y",Json(47)),0,47);
            rule.minSurface=integer(entry.value("min_surface",Json(-48)),-48,48);rule.maxSurface=integer(entry.value("max_surface",Json(48)),-48,48);
            check(rule.minY<=rule.maxY && rule.minSurface<=rule.maxSurface,"inverted voxel rule interval");
            rule.period=integer(entry.value("period",Json(16)),4,256);rule.threshold=number(entry.value("threshold",Json(0)),-1,1);rule.salt=salt(entry.value("salt",Json(0)));
            rule.protectSpawn=entry.value("protect_spawn",true);
            const auto& filters=entry.value("biomes",Json::array());check(filters.is_array() && filters.size()<=32,"invalid voxel biome filters");
            for(const auto& value:filters) { auto b=value.get<std::string>();check(biomeIds.contains(b),"voxel biome not selected by generator");rule.biomes.push_back(b); }
            spec.voxelRules.push_back(std::move(rule));
        }
        const auto& placements=g.at("structures"); check(placements.is_array() && placements.size()<=64,"placement limit is 64");std::set<std::string> ruleIds;int attempts=0;
        for(const auto& p:placements) {
            object(p);StructurePlacement place;place.structure=p.at("structure");place.chance=number(p.at("chance"),0,1);
            check(d.structures.contains(place.structure),"unknown structure: "+place.structure);
            place.id=p.value("id",std::string{});if(spec.scatter) { (void)AssetId(place.id);check(ruleIds.insert(place.id).second,"duplicate placement id: "+place.id); }
            place.attempts=integer(p.value("attempts",Json(1)),1,8);attempts+=place.attempts;
            place.salt=salt(p.value("salt",Json(0)));place.minHeight=integer(p.value("min_height",Json(1)),1,47);place.maxHeight=integer(p.value("max_height",Json(47)),1,47);
            check(place.minHeight<=place.maxHeight,"inverted placement height interval");
            place.maxSlope=integer(p.value("max_slope",Json(8)),0,8);place.spacing=integer(p.value("spacing",Json(0)),0,32);place.yOffset=integer(p.value("y_offset",Json(0)),0,8);
            place.alignment=p.value("alignment",std::string("anchor"));check(place.alignment=="anchor" || place.alignment=="surface","unknown structure alignment");
            const auto& rotations=p.value("rotations",Json::array({0,90,180,270}));check(rotations.is_array() && !rotations.empty() && rotations.size()<=4,"requires 1..4 rotations");place.rotations.clear();std::set<int> uniqueRotations;
            for(const auto& r:rotations) { int angle=integer(r,0,270);check(angle%90==0 && uniqueRotations.insert(angle).second,"invalid/duplicate rotation");place.rotations.push_back(angle); }
            const auto& filters=p.value("biomes",Json::array());check(filters.is_array() && filters.size()<=32,"invalid biome filter");
            for(const auto& b:filters) { auto biome=b.get<std::string>();check(biomeIds.contains(biome),"placement biome is not selected by generator: "+biome);place.biomes.push_back(biome); }
            if(!spec.scatter) {
                check(place.id.empty() && place.attempts==1 && place.salt==0 && place.minHeight==1 && place.maxHeight==47 && place.maxSlope==8 && place.spacing==0 && place.yOffset==0 && place.alignment=="anchor" && place.rotations==std::vector<int>({0,90,180,270}) && place.biomes.empty(),"advanced placement rules require structure_mode scatter");
                for(const auto& c:d.structures.at(place.structure).cells) check(std::abs(c.x)<=3 && std::abs(c.z)<=3,"large structures require scatter mode");
            }
            spec.structures.push_back(std::move(place));
        }
        check(attempts<=128,"total placement attempts exceed 128");d.generators.emplace(id,std::move(spec));
    }
    check(d.generators.size()<=32,"generator limit is 32");
    if(j.contains("maps")) {
        object(j.at("maps"));check(j.at("maps").size()<=1,"only one overworld may be registered");
        for(const auto& [id,map]:j.at("maps").items()) {
            (void)AssetId(id);check(!d.generators.contains(id),"duplicate terrain ID: "+id);
            d.maps.emplace(id,std::make_shared<FiniteMap>(FiniteMap::parse(map,d)));
        }
    }
    return d;
}
nlohmann::json WorldDefinition::json() const {
    Json j={{"version",1},{"blocks",Json::object()},{"structures",Json::object()},{"biomes",Json::object()},{"generators",Json::object()}};
    for(const auto& [id,b]:blocks) {
        j["blocks"][id]={{"texture",b.texture},{"solid",b.solid}};
        if(!b.model.empty()) j["blocks"][id]["model"]=b.model;
        if(!b.name.empty()) j["blocks"][id]["name"]=b.name;
        if(!b.shader.empty()) j["blocks"][id]["shader"]={{"id",b.shader},{"parameters",b.shaderParameters},{"texture",b.shaderTexture}};
    }
    for(const auto& [id,s]:structures) { auto cells=Json::array(); for(const auto& c:s.cells) cells.push_back({c.x,c.y,c.z,c.block}); j["structures"][id]=cells; }
    for(const auto& [id,b]:biomes) j["biomes"][id]={{"name",b.name},{"rock",b.rock},{"layers",layerJson(b.layers)}};
    for(const auto& [id,g]:generators) {
        auto p=Json::array();for(const auto& s:g.structures) {
            Json rule={{"structure",s.structure},{"chance",s.chance}};
            if(g.scatter) rule.update(Json{{"id",s.id},{"attempts",s.attempts},{"salt",s.salt},{"min_height",s.minHeight},{"max_height",s.maxHeight},{"max_slope",s.maxSlope},{"spacing",s.spacing},{"y_offset",s.yOffset},{"alignment",s.alignment},{"rotations",s.rotations},{"biomes",s.biomes}});
            p.push_back(rule);
        }
        auto noise=Json::array();for(const auto& n:g.noise) noise.push_back({{"period",n.period},{"amplitude",n.amplitude},{"salt",n.salt}});
        auto choices=Json::array();for(const auto& b:g.biomes) choices.push_back({{"biome",b.biome},{"weight",b.weight}});
        auto voxels=Json::array();for(const auto& r:g.voxelRules) voxels.push_back({{"id",r.id},{"block",r.block},{"replace",r.replace},{"biomes",r.biomes},
            {"min_y",r.minY},{"max_y",r.maxY},{"min_surface",r.minSurface},{"max_surface",r.maxSurface},{"period",r.period},{"threshold",r.threshold},{"salt",r.salt},{"protect_spawn",r.protectSpawn}});
        j["generators"][id]={{"name",g.name},{"surface",g.surface},{"fill",g.fill},{"rock",g.rock},{"base_height",g.baseHeight},{"relief",g.relief},{"period",g.period},{"structures",p},{"layers",layerJson(g.layers)},{"noise",noise},{"voxel_rules",voxels},{"biomes",choices},{"biome_period",g.biomePeriod},{"biome_salt",g.biomeSalt},{"safe_radius",g.safeRadius},{"structure_mode",g.scatter ? "scatter" : "legacy"}};
    }
    if(!maps.empty()) {
        j["maps"]=Json::object();
        for(const auto& [id,map]:maps) j["maps"][id]=map->json();
    }
    return j;
}
WorldDefinition WorldDefinition::load(const std::vector<ModManifest>& mods) {
    Json combined={{"version",1},{"blocks",Json::object()},{"structures",Json::object()},{"biomes",Json::object()},{"generators",Json::object()}};
    Json extensions=Json::object();
    std::map<std::string,std::filesystem::path> maps;
    struct MapOverride { std::filesystem::path path;std::set<std::string> baked; };
    std::map<std::string,MapOverride> mapOverrides;
    std::vector<std::pair<std::string,Json>> mapPlacements;
    auto dependency=[&](const std::string& owner,const std::string& target) {
        const auto targetMod=AssetId(target).nameSpace();if(owner==targetMod) return;
        bool allowed=false;for(const auto& mod:mods) if(mod.id==owner) for(const auto& dep:mod.dependencies) if(dep.id==targetMod)
            for(const auto& installed:mods) if(installed.id==targetMod && !installed.dependencyCycle && dep.accepts(installed.version)) allowed=true;
        check(allowed,"declare an installed dependency before extending "+target);
    };
    for(const auto& mod:mods) {
        if(mod.dependencyCycle || !std::filesystem::exists(mod.root/"world.json")) continue;
        const auto path=VirtualFileSystem::contained(mod.root,"world.json"); check(std::filesystem::file_size(path)<=1024*1024,"world.json exceeds 1 MiB");
        std::ifstream in(path); const auto j=Json::parse(in); object(j); check(j.at("version")==1,"unsupported mod world version");
        if(j.contains("maps")) {
            object(j.at("maps"));
            for(const auto& [id,source]:j.at("maps").items()) {
                check(AssetId(id).nameSpace()==mod.id,"map must use its own mod namespace: "+id);
                check(maps.empty(),"only one overworld may be registered");
                maps.emplace(id,VirtualFileSystem::contained(mod.root,source.get<std::string>()));
            }
        }
        if(j.contains("map_overrides")) {
            object(j.at("map_overrides"));
            for(const auto& [id,source]:j.at("map_overrides").items()) {
                dependency(mod.id,id);
                check(!mapOverrides.contains(id),"multiple mods replace the same overworld: "+id);
                MapOverride entry;
                if(source.is_string()) entry.path=VirtualFileSystem::contained(mod.root,source.get<std::string>());
                else {
                    entry.path=VirtualFileSystem::contained(mod.root,source.at("source").get<std::string>());
                    const auto& baked=source.at("baked_placements");
                    check(baked.is_array() && baked.size()<=1024,"invalid baked placement mod list");
                    for(const auto& value:baked) {
                        const auto owner=value.get<std::string>();check(validNamespace(owner),"invalid baked placement mod ID");
                        dependency(mod.id,owner+":placements");entry.baked.insert(owner);
                    }
                }
                mapOverrides.emplace(id,std::move(entry));
            }
        }
        if(j.contains("map_placements")) {
            const auto& entries=j.at("map_placements");
            check(entries.is_array() && entries.size()+mapPlacements.size()<=4096,"invalid map placements");
            for(const auto& entry:entries) {
                check(entry.is_array() && entry.size()==5,"expected placement [structure,x,y,z,rotation]");
                dependency(mod.id,entry.at(0).get<std::string>());
                mapPlacements.emplace_back(mod.id,entry);
            }
        }
        for(const std::string category:{"blocks","structures","biomes","generators","generator_extensions"}) if(j.contains(category)) {
            object(j.at(category));auto& destination=category=="generator_extensions" ? extensions : combined[category];
            for(const auto& [id,value]:j.at(category).items()) {
                check(AssetId(id).nameSpace()==mod.id,"definition must use its own mod namespace: "+id);
                check(!destination.contains(id),"duplicate definition: "+id);destination[id]=value;
            }
        }
    }
    // Resolve inheritance before extensions, so each preset has an independent base.
    std::set<std::string> visiting,resolved;
    std::function<void(const std::string&)> resolve=[&](const std::string& id) {
        check(combined["generators"].contains(id),"unknown base generator: "+id);
        if(resolved.contains(id)) return;
        check(visiting.size()<32 && visiting.insert(id).second,"generator inheritance cycle: "+id);
        auto value=combined["generators"][id];object(value);
        if(value.contains("extends")) {
            const auto base=value.at("extends").get<std::string>();dependency(AssetId(id).nameSpace(),base);resolve(base);
            value.erase("extends");auto inherited=combined["generators"][base];inherited.update(value);value=std::move(inherited);
        }
        combined["generators"][id]=std::move(value);visiting.erase(id);resolved.insert(id);
    };
    for(const auto& [id,g]:combined["generators"].items()) { (void)g;resolve(id); }
    // JSON object keys are sorted: extension order does not depend on discovery order.
    for(const auto& [id,extension]:extensions.items()) {
        object(extension);const auto target=extension.at("generator").get<std::string>();dependency(AssetId(id).nameSpace(),target);
        check(combined["generators"].contains(target),"unknown extension target: "+target);auto& g=combined["generators"][target];
        check(g.value("structure_mode",std::string("legacy"))=="scatter","extensions require a scatter generator");
        for(const std::string category:{"structures","biomes","voxel_rules"}) if(extension.contains(category)) {
            const auto& entries=extension.at(category);check(entries.is_array(),"extension entries must be arrays");
            if(!g.contains(category)) g[category]=Json::array();
            check(g[category].is_array(),"generator entries must be arrays");
            for(const auto& entry:entries) {
                if(category=="structures" || category=="voxel_rules") check(AssetId(entry.at("id").get<std::string>()).nameSpace()==AssetId(id).nameSpace(),"extension placement IDs must use the extension namespace");
                g[category].push_back(entry);
            }
        }
    }
    auto definition=parse(combined);
    check(mapPlacements.empty() || !maps.empty(),"map placements require the overworld");
    for(const auto& [id,path]:mapOverrides) { (void)path;check(maps.contains(id),"missing overworld for map override: "+id); }
    for(const auto& [id,path]:maps) {
        check(!definition.generators.contains(id),"duplicate terrain ID: "+id);
        const auto replacement=mapOverrides.find(id);
        auto map=FiniteMap::read(replacement==mapOverrides.end()?path:replacement->second.path,definition);
        auto placements=Json::array();
        for(const auto& [owner,placement]:mapPlacements) {
            definition.mapPlacementMods.insert(owner);
            if(replacement!=mapOverrides.end() && replacement->second.baked.contains(owner)) continue;
            placements.push_back(placement);
        }
        map.placeStructures(placements,definition);
        map.validateDoors(definition);
        const auto spawn=map.spawn();
        bool hasGround=false;
        for(int y=0;y<FiniteMap::height;++y) {
            const auto block=map.block(spawn.x,y,spawn.z);
            if(!block.empty() && definition.blocks.at(std::string(block)).solid) hasGround=true;
        }
        check(hasGround,"the overworld spawn needs solid ground");
        definition.maps.emplace(id,std::make_shared<FiniteMap>(std::move(map)));
    }
    return definition;
}
TerrainGenerator::TerrainGenerator(WorldDefinition definition,std::string generator,std::string seed)
    :definition_(std::move(definition)),seed_(seedHash(seed)) {
    if(const auto found=definition_.maps.find(generator);found!=definition_.maps.end()) {
        map_=found->second;settings_.id=std::move(generator);settings_.name="ShardBound";
    } else settings_=definition_.generators.at(generator);
}
int TerrainGenerator::height(std::int64_t x,std::int64_t z) const {
    if(map_) {
        const auto area=map_->areaAt(x,z);
        if(!area) return 0;
        for(int y=maxHeight-1;y>=0;--y) {
            const auto id=area->map->block(x-area->offsetX,y,z-area->offsetZ);
            if(!id.empty() && definition_.blocks.at(std::string(id)).solid) return y+1;
        }
        return 0;
    }
    double offset=0;
    if(settings_.noise.empty()) offset=terrainNoise(seed_,x,z,settings_.period,17)*settings_.relief;
    else for(const auto& n:settings_.noise) offset+=terrainNoise(seed_,x,z,n.period,n.salt)*n.amplitude;
    return settings_.baseHeight+int(std::round(offset));
}
std::string TerrainGenerator::biome(std::int64_t x,std::int64_t z) const {
    if(map_) {
        const auto area=map_->areaAt(x,z);
        return area?area->map->biome(x-area->offsetX,z-area->offsetZ):std::string{};
    }
    if(settings_.biomes.empty()) return {};
    double total=0;for(const auto& b:settings_.biomes) total+=b.weight;
    double selected=(terrainNoise(seed_,x,z,settings_.biomePeriod,settings_.biomeSalt)+1)*.5*total;
    for(const auto& b:settings_.biomes) { selected-=b.weight;if(selected<0) return b.biome; }
    return settings_.biomes.back().biome;
}
TerrainColumn TerrainGenerator::column(std::int64_t x,std::int64_t z) const {
    TerrainColumn result;result.height=height(x,z);result.biome=biome(x,z);
    if(map_) {
        if(const auto area=map_->areaAt(x,z)) {
            result.blocks.resize(maxHeight);
            for(int y=0;y<maxHeight;++y) result.blocks[size_t(y)]=area->map->block(x-area->offsetX,y,z-area->offsetZ);
        }
        return result;
    }
    const auto* soil=&settings_.layers;auto rock=settings_.rock;
    if(!result.biome.empty()) { const auto& b=definition_.biomes.at(result.biome);soil=&b.layers;rock=b.rock; }
    result.blocks.assign(size_t(result.height),rock);
    if(soil->empty()) {
        for(int y=0;y<result.height;++y) result.blocks[size_t(y)]=y==result.height-1 ? settings_.surface : y>=result.height-3 ? settings_.fill : settings_.rock;
    } else {
        int y=result.height-1;for(const auto& layer:*soil) for(int i=0;i<layer.depth && y>=0;++i) result.blocks[size_t(y--)]=layer.block;
    }
    for(const auto& rule:settings_.voxelRules) {
        if(rule.protectSpawn && settings_.safeRadius>=0 && std::abs(floorDiv(x,16))<=settings_.safeRadius && std::abs(floorDiv(z,16))<=settings_.safeRadius) continue;
        if(!rule.biomes.empty() && std::find(rule.biomes.begin(),rule.biomes.end(),result.biome)==rule.biomes.end()) continue;
        const int low=std::max(rule.minY,result.height+rule.minSurface),high=std::min(rule.maxY,result.height+rule.maxSurface);
        for(int y=low;y<=high;++y) {
            const auto old=size_t(y)<result.blocks.size()?result.blocks[size_t(y)]:std::string{};
            if(std::find(rule.replace.begin(),rule.replace.end(),old)==rule.replace.end()) continue;
            const auto gy=floorDiv(y,rule.period);const double fraction=double(y-gy*rule.period)/rule.period,t=fraction*fraction*(3-2*fraction);
            // Interpolate independent XZ noise planes for a continuous 3D field.
            const double first=terrainNoise(seed_,x,z,rule.period,mix(rule.salt^mix(std::uint64_t(gy))));
            const double second=terrainNoise(seed_,x,z,rule.period,mix(rule.salt^mix(std::uint64_t(gy+1))));
            if(first*(1-t)+second*t<rule.threshold) continue;
            if(size_t(y)>=result.blocks.size()) result.blocks.resize(size_t(y)+1);
            result.blocks[size_t(y)]=rule.block;
        }
    }
    return result;
}
std::vector<StructureCell> TerrainGenerator::decoration(std::int64_t cx,std::int64_t cz) const {
    std::vector<StructureCell> cells;
    if(map_) return cells;
    if(settings_.safeRadius>=0 && std::abs(cx)<=settings_.safeRadius && std::abs(cz)<=settings_.safeRadius) return cells;
    if(!settings_.scatter) {
        std::uint64_t index=0;
        for(const auto& placement:settings_.structures) {
            auto h=coordinateHash(seed_,cx,cz,101+index++);
            if(double(h>>11)/9007199254740992.0>=placement.chance) continue;
            const int ax=4+int((h>>8)%8),az=4+int((h>>24)%8),rotation=int((h>>40)%4);
            const int base=height(cx*16+ax,cz*16+az);
            for(auto c:definition_.structures.at(placement.structure).cells) {
                for(int i=0;i<rotation;++i) { int old=c.x; c.x=-c.z; c.z=old; }
                c.x+=ax; c.z+=az; c.y+=base;
                if(c.y>=height(cx*16+c.x,cz*16+c.z)) cells.push_back(c);
            }
            break;
        }
        return cells;
    }
    std::set<std::tuple<int,int,int>> occupied;
    struct Anchor { int x,z,spacing; };std::vector<Anchor> anchors;
    for(const auto& p:settings_.structures) {
        const auto& structure=definition_.structures.at(p.structure);int radius=0;
        for(const auto& c:structure.cells) radius=std::max({radius,std::abs(c.x),std::abs(c.z)});
        for(int attempt=0;attempt<p.attempts;++attempt) {
            // A stable rule ID/salt isolates random samples from unrelated addon rules.
            auto h=coordinateHash(seed_,cx,cz,seedHash(p.id)^mix(p.salt)^mix(std::uint64_t(attempt)+1));
            if(double(h>>11)/9007199254740992.0>=p.chance) continue;
            const int span=14-2*radius,ax=1+radius+int((h>>8)%span),az=1+radius+int((h>>24)%span);
            const int rotation=p.rotations[size_t((h>>40)%p.rotations.size())]/90;
            const auto wx=cx*16+ax,wz=cz*16+az;const int base=height(wx,wz);
            if(base<p.minHeight || base>p.maxHeight) continue;
            if(!p.biomes.empty() && std::find(p.biomes.begin(),p.biomes.end(),biome(wx,wz))==p.biomes.end()) continue;
            bool valid=true;for(const auto& anchor:anchors) {
                const int distance=std::max(anchor.spacing,p.spacing),dx=ax-anchor.x,dz=az-anchor.z;
                if(dx*dx+dz*dz<distance*distance) valid=false;
            }
            if(!valid) continue;
            int low=base,high=base;std::vector<StructureCell> candidate;
            for(auto c:structure.cells) {
                for(int i=0;i<rotation;++i) { int old=c.x;c.x=-c.z;c.z=old; }c.x+=ax;c.z+=az;
                const int floor=height(cx*16+c.x,cz*16+c.z);low=std::min(low,floor);high=std::max(high,floor);
                c.y+=(p.alignment=="surface" ? floor : base)+p.yOffset;
                if(c.y<floor || c.y>=maxHeight || occupied.contains({c.x,c.y,c.z})) { valid=false;break; }candidate.push_back(c);
            }
            // Atomic placement: never cut a ruin/tree in half or overwrite an earlier feature.
            if(!valid || high-low>p.maxSlope || candidate.empty() || cells.size()+candidate.size()>512) continue;
            for(const auto& c:candidate) { occupied.emplace(c.x,c.y,c.z);cells.push_back(c); }anchors.push_back({ax,az,p.spacing});
        }
    }
    return cells;
}
std::string TerrainGenerator::block(std::int64_t x,int y,std::int64_t z) const {
    if(map_) {
        const auto area=map_->areaAt(x,z);
        return area?std::string(area->map->block(x-area->offsetX,y,z-area->offsetZ)):std::string{};
    }
    if(y<0 || y>=maxHeight) return {};
    const auto terrain=column(x,z);if(y<terrain.height) return terrain.blocks[size_t(y)];
    const auto cx=floorDiv(x,16),cz=floorDiv(z,16);const int lx=int(x-cx*16),lz=int(z-cz*16);
    for(const auto& c:decoration(cx,cz)) if(c.x==lx && c.z==lz && c.y==y) return c.block;
    return size_t(y)<terrain.blocks.size()?terrain.blocks[size_t(y)]:std::string{};
}
}

