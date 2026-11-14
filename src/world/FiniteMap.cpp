#include "world/FiniteMap.h"
#include "world/WorldDefinition.h"
#include "core/AtomicFile.h"
#include "assets/AssetId.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>

namespace game {
namespace {
using Json = nlohmann::json;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("Finite map: ") + message);
}
int integer(const Json& value, int minimum, int maximum) {
    require(value.is_number_integer(), "expected integer");
    const auto n = value.get<double>();
    require(n >= minimum && n <= maximum, "integer outside permitted range");
    return static_cast<int>(n);
}
}

FiniteMap::FiniteMap(Bounds bounds) : bounds_(bounds) {
    require(bounds.width > 0 && bounds.width <= maxSide && bounds.depth > 0 && bounds.depth <= maxSide,
        "width and depth must be in 1..512");
    require(bounds.minX >= -coordinateLimit && bounds.minZ >= -coordinateLimit &&
        bounds.minX <= coordinateLimit - bounds.width && bounds.minZ <= coordinateLimit - bounds.depth,
        "bounds outside permitted coordinates");
    blocks_.resize(size_t(bounds.width) * bounds.depth * height);
    spawn_={bounds.minX+bounds.width/2,bounds.minZ+bounds.depth/2};
}

void FiniteMap::setSpawn(int x,int z) {
    require(contains(x,z),"spawn outside bounds");
    spawn_={x,z};
}

void FiniteMap::setExit(std::optional<Spawn> exit) {
    require(!exit || contains(exit->x,exit->z),"exit outside bounds");
    require(!exit || interiors_.empty(),"overworld cannot have an interior exit");
    exit_=exit;
}

void FiniteMap::addInterior(Interior interior) {
    require(interiors_.size()<32 && !exit_,"interiors require an overworld and are limited to 32");
    require(interior.map && interior.map->interiors().empty(),"nested interiors are not supported");
    require(interior.map->exit()==interior.exit,"interior exit differs from its map");
    require(contains(interior.entrance.x,interior.entrance.z) && contains(interior.returnPoint.x,interior.returnPoint.z),"door outside overworld bounds");
    require(interior.map->contains(interior.exit.x,interior.exit.z),"exit outside interior bounds");
    require(interior.entrance!=interior.returnPoint && interior.exit!=interior.map->spawn(),"arrival must be distinct from its door");
    for(const auto& existing:interiors_) require(existing.entrance!=interior.entrance && existing.returnPoint!=interior.entrance &&
        existing.entrance!=interior.returnPoint,"door overlaps another entrance or return point");
    size_t volume=blocks_.size()+interior.map->blocks_.size();
    for(const auto& existing:interiors_) volume+=existing.map->blocks_.size();
    require(volume<=size_t(maxSide)*maxSide*height,"combined map volume exceeds the map budget");
    interiors_.push_back(std::move(interior));
}

void FiniteMap::removeInterior() { interiors_.pop_back(); }

void FiniteMap::addProp(Prop prop) {
    (void)AssetId(prop.model);
    require(props_.size()<1024 && contains(prop.x,prop.z) && prop.y>=0 && prop.y<height,"model placement outside map limits");
    require(std::isfinite(prop.scale) && prop.scale>=.001f && prop.scale<=100 && std::isfinite(prop.yaw) && std::abs(prop.yaw)<=360,"invalid model transform");
    props_.push_back(std::move(prop));
}

void FiniteMap::removeProp() { props_.pop_back(); }

FiniteMap::Bounds FiniteMap::Area::bounds() const {
    auto result=map->bounds();result.minX+=offsetX;result.minZ+=offsetZ;return result;
}

FiniteMap::Area FiniteMap::interiorArea(size_t index) const {
    const auto* map=interiors_[index].map.get();
    const auto source=map->bounds();
    const int x=int(floorDiv(bounds_.minX,16))*16+1024+int(index%8)*1024;
    const int z=int(floorDiv(bounds_.minZ,16))*16+int(index/8)*1024;
    return {map,x-source.minX,z-source.minZ};
}

std::optional<FiniteMap::Area> FiniteMap::areaAt(std::int64_t x,std::int64_t z) const {
    if(contains(x,z)) return Area{this,0,0};
    for(size_t i=0;i<interiors_.size();++i) {
        const auto area=interiorArea(i);
        if(area.map->contains(x-area.offsetX,z-area.offsetZ)) return area;
    }
    return std::nullopt;
}

std::vector<FiniteMap::Area> FiniteMap::areas() const {
    std::vector<Area> result{{this,0,0}};
    for(size_t i=0;i<interiors_.size();++i) result.push_back(interiorArea(i));
    return result;
}

std::vector<FiniteMap::Door> FiniteMap::doors() const {
    std::vector<Door> result;
    for(size_t i=0;i<interiors_.size();++i) {
        const auto& interior=interiors_[i];const auto area=interiorArea(i);
        auto arrival=area.map->spawn();arrival.x+=area.offsetX;arrival.z+=area.offsetZ;
        auto exit=interior.exit;exit.x+=area.offsetX;exit.z+=area.offsetZ;
        result.push_back({interior.entrance,arrival});
        result.push_back({exit,interior.returnPoint});
    }
    return result;
}

void FiniteMap::validateDoors(const WorldDefinition& catalog) const {
    const auto floor=[&](const FiniteMap& map,Spawn position) {
        for(int y=height-3;y>=0;--y) {
            const auto id=map.block(position.x,y,position.z);
            if(!id.empty() && catalog.blocks.at(std::string(id)).solid &&
                map.block(position.x,y+1,position.z).empty() && map.block(position.x,y+2,position.z).empty()) return;
        }
        throw std::runtime_error("Door needs a solid floor and two clear blocks at "+std::to_string(position.x)+","+std::to_string(position.z));
    };
    for(const auto& interior:interiors_) {
        floor(*this,interior.entrance);floor(*this,interior.returnPoint);
        floor(*interior.map,interior.map->spawn());floor(*interior.map,interior.exit);
    }
}

bool FiniteMap::contains(std::int64_t x, std::int64_t z) const {
    return x >= bounds_.minX && x < bounds_.minX + bounds_.width &&
        z >= bounds_.minZ && z < bounds_.minZ + bounds_.depth;
}

size_t FiniteMap::index(int x, int y, int z) const {
    return (size_t(y) * bounds_.depth + (z - bounds_.minZ)) * bounds_.width + (x - bounds_.minX);
}

std::string_view FiniteMap::block(std::int64_t x, int y, std::int64_t z) const {
    if (!contains(x,z) || y < 0 || y >= height) return {};
    return palette_[blocks_[index(int(x),y,int(z))]];
}

std::uint16_t FiniteMap::paletteIndex(const std::string& id, const WorldDefinition& catalog) {
    if (!id.empty() && !catalog.blocks.contains(id)) throw std::runtime_error("Unknown map block: " + id);
    const auto found = std::find(palette_.begin(), palette_.end(), id);
    if (found != palette_.end()) return static_cast<std::uint16_t>(found - palette_.begin());
    require(palette_.size() <= std::numeric_limits<std::uint16_t>::max(), "palette is full");
    palette_.push_back(id);
    return static_cast<std::uint16_t>(palette_.size() - 1);
}

void FiniteMap::setBlock(int x, int y, int z, const std::string& id, const WorldDefinition& catalog) {
    require(contains(x,z) && y >= 0 && y < height, "block outside bounds");
    blocks_[index(x,y,z)] = paletteIndex(id,catalog);
}

std::string FiniteMap::biome(std::int64_t x, std::int64_t z) const {
    for (const auto& zone : zones_) {
        if (x >= zone.x && x < zone.x + zone.width && z >= zone.z && z < zone.z + zone.depth) return zone.id;
    }
    return {};
}

FiniteMap FiniteMap::parse(const Json& json, const WorldDefinition& catalog) {
    return parseMap(json,catalog,true,true);
}

FiniteMap FiniteMap::parseDraft(const Json& json,const WorldDefinition& catalog) {
    return parseMap(json,catalog,true,false);
}

FiniteMap FiniteMap::parseMap(const Json& json,const WorldDefinition& catalog,bool allowInteriors,bool playable) {
    require(json.at("format") == "shardbound-map" && json.at("version") == 1, "unsupported format or version");
    const auto& b = json.at("bounds");
    require(b.is_array() && b.size() == 4, "expected bounds [x,z,width,depth]");
    FiniteMap map({integer(b[0],-coordinateLimit,coordinateLimit), integer(b[1],-coordinateLimit,coordinateLimit),
        integer(b[2],1,maxSide), integer(b[3],1,maxSide)});
    const auto point=[](const Json& value) {
        require(value.is_array() && value.size()==2,"expected point [x,z]");
        return Spawn{integer(value[0],-coordinateLimit,coordinateLimit),integer(value[1],-coordinateLimit,coordinateLimit)};
    };
    if(json.contains("spawn")) {const auto spawn=point(json.at("spawn"));map.setSpawn(spawn.x,spawn.z);}
    if(json.contains("exit")) map.setExit(point(json.at("exit")));

    const auto& fills = json.value("fills", Json::array());
    require(fills.is_array() && fills.size() <= 4096, "invalid fill list");
    size_t painted = 0;
    for (const auto& fill : fills) {
        require(fill.is_array() && fill.size() == 7, "expected fill [x,y,z,width,height,depth,block]");
        const int x = integer(fill[0],-coordinateLimit,coordinateLimit), z = integer(fill[2],-coordinateLimit,coordinateLimit);
        const int y = integer(fill[1],0,height-1), rows = integer(fill[4],1,height-y);
        const int width = integer(fill[3],1,maxSide), depth = integer(fill[5],1,maxSide);
        require(map.contains(x,z) && map.contains(x+width-1,z+depth-1), "fill outside bounds");
        painted += size_t(width) * depth * rows;
        require(painted <= map.blocks_.size()*4, "fill volume exceeds map budget");
        const auto block = map.paletteIndex(fill[6].get<std::string>(),catalog);
        for (int dy=0; dy<rows; ++dy) for (int dz=0; dz<depth; ++dz)
            std::fill_n(map.blocks_.data()+map.index(x,y+dy,z+dz),width,block);
    }

    map.placeStructures(json.value("placements",Json::array()),catalog);

    const auto& runs = json.value("runs",Json::array());
    require(runs.is_array() && runs.size() <= map.blocks_.size(), "invalid run list");
    int previousEnd = 0;
    for (const auto& run : runs) {
        require(run.is_array() && run.size() == 3, "expected run [start,length,block]");
        const int start = integer(run[0],previousEnd,int(map.blocks_.size())-1);
        const int length = integer(run[1],1,int(map.blocks_.size())-start);
        const auto block = map.paletteIndex(run[2].get<std::string>(),catalog);
        std::fill_n(map.blocks_.data()+start,length,block);
        previousEnd = start+length;
    }

    const auto& zones = json.value("biomes",Json::array());
    require(zones.is_array() && zones.size() <= 256, "invalid biome region list");
    for (const auto& region : zones) {
        require(region.is_array() && region.size() == 5, "expected biome [x,z,width,depth,id]");
        Zone zone{integer(region[0],-coordinateLimit,coordinateLimit), integer(region[1],-coordinateLimit,coordinateLimit),
            integer(region[2],1,maxSide), integer(region[3],1,maxSide), region[4].get<std::string>()};
        require(map.contains(zone.x,zone.z) && map.contains(zone.x+zone.width-1,zone.z+zone.depth-1), "biome outside bounds");
        if (!catalog.biomes.contains(zone.id)) throw std::runtime_error("Unknown map biome: " + zone.id);
        for (const auto& previous : map.zones_) {
            require(zone.x+zone.width <= previous.x || previous.x+previous.width <= zone.x ||
                zone.z+zone.depth <= previous.z || previous.z+previous.depth <= zone.z, "overlapping biome regions");
        }
        map.zones_.push_back(std::move(zone));
    }
    const auto& rooms=json.value("interiors",Json::array());
    require(rooms.is_array() && rooms.size()<=32 && (allowInteriors || rooms.empty()),"invalid interior list");
    for(const auto& room:rooms) {
        auto inside=std::make_shared<FiniteMap>(parseMap(room.at("map"),catalog,false,playable));
        require(inside->exit().has_value(),"interior needs an exit");
        const auto exit=*inside->exit();
        map.addInterior({point(room.at("entrance")),point(room.at("return")),exit,std::move(inside)});
    }
    const auto& props=json.value("props",Json::array());
    require(props.is_array() && props.size()<=1024,"invalid model placements");
    for(const auto& prop:props) {
        require(prop.is_array() && prop.size()==6,"expected model placement [model,x,y,z,yaw,scale]");
        map.addProp({prop[0].get<std::string>(),integer(prop[1],-coordinateLimit,coordinateLimit),integer(prop[2],0,height-1),
            integer(prop[3],-coordinateLimit,coordinateLimit),prop[4].get<float>(),prop[5].get<float>()});
    }
    if(playable) map.validateDoors(catalog);
    return map;
}

void FiniteMap::placeStructures(const Json& placements,const WorldDefinition& catalog) {
    require(placements.is_array() && placements.size()<=4096,"invalid structure placement list");
    std::map<size_t,std::uint16_t> placedCells;
    for(const auto& placement:placements) {
        require(placement.is_array() && placement.size()==5,"expected placement [structure,x,y,z,rotation]");
        const auto id=placement[0].get<std::string>();
        const auto found=catalog.structures.find(id);
        if(found==catalog.structures.end()) throw std::runtime_error("Unknown placed structure: "+id);
        const int x=integer(placement[1],-coordinateLimit,coordinateLimit),y=integer(placement[2],0,height-1);
        const int z=integer(placement[3],-coordinateLimit,coordinateLimit),rotation=integer(placement[4],0,270);
        require(rotation%90==0,"expected rotation in steps of 90 degrees");
        for(const auto& cell:found->second.cells) {
            int dx=cell.x,dz=cell.z;
            for(int turn=0;turn<rotation/90;++turn) {const int oldX=dx;dx=-dz;dz=oldX;}
            const int bx=x+dx,by=y+cell.y,bz=z+dz;
            require(contains(bx,bz) && by>=0 && by<height,"structure outside map bounds");
            require(placedCells.emplace(index(bx,by,bz),paletteIndex(cell.block,catalog)).second,"structure placements overlap");
        }
    }
    for(const auto& [cell,id]:placedCells) blocks_[cell]=id;
}

Json FiniteMap::json() const {
    Json result = {{"format","shardbound-map"},{"version",1},
        {"bounds",{bounds_.minX,bounds_.minZ,bounds_.width,bounds_.depth}},
        {"spawn",{spawn_.x,spawn_.z}},
        {"runs",Json::array()},{"biomes",Json::array()}};
    auto& runs = result.at("runs");
    for (size_t start=0; start<blocks_.size();) {
        const auto block = blocks_[start];
        size_t end = start+1;
        while (end<blocks_.size() && blocks_[end]==block) ++end;
        if (block) runs.push_back({start,end-start,palette_[block]});
        start = end;
    }
    for (const auto& zone : zones_) result["biomes"].push_back({zone.x,zone.z,zone.width,zone.depth,zone.id});
    if(exit_) result["exit"]={exit_->x,exit_->z};
    if(!props_.empty()) {
        auto& props=result["props"]=Json::array();
        for(const auto& prop:props_) props.push_back({prop.model,prop.x,prop.y,prop.z,prop.yaw,prop.scale});
    }
    if(!interiors_.empty()) {
        auto& rooms=result["interiors"]=Json::array();
        for(const auto& room:interiors_) rooms.push_back({{"entrance",{room.entrance.x,room.entrance.z}},
            {"return",{room.returnPoint.x,room.returnPoint.z}},{"map",room.map->json()}});
    }
    return result;
}

FiniteMap FiniteMap::read(const std::filesystem::path& path, const WorldDefinition& catalog) {
    require(std::filesystem::file_size(path) <= 8*1024*1024, "file exceeds 8 MiB");
    std::ifstream input(path,std::ios::binary);
    require(bool(input), "cannot open file");
    return parse(Json::parse(input),catalog);
}

void FiniteMap::save(const std::filesystem::path& path) const {
    const auto text = json().dump();
    require(text.size() <= 8*1024*1024, "serialized file exceeds 8 MiB");
    atomicWrite(std::filesystem::absolute(path),text,true);
}
}
