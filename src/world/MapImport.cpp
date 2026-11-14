#include "world/MapImport.h"
#include "world/WorldBridge.h"
#include <fstream>

namespace game {
FiniteMap importMapFromSave(const std::filesystem::path& checkpoint, FiniteMap::Bounds selection) {
    FiniteMap map(selection);
    const auto directory=std::filesystem::absolute(checkpoint).parent_path();
    const auto metadataPath=directory/"world.json";
    if(std::filesystem::file_size(metadataPath)>16*1024*1024) throw std::runtime_error("World metadata exceeds 16 MiB");
    std::ifstream input(metadataPath,std::ios::binary);
    const auto metadata=nlohmann::json::parse(input);
    if(metadata.at("format")!="shardbound-world" || metadata.at("version")!=1) throw std::runtime_error("Unsupported world metadata");
    auto catalog=WorldDefinition::parse(metadata.at("definition"));
    const TerrainGenerator terrain(catalog,metadata.at("generator"),metadata.at("seed"));
    if(const auto* source=terrain.finiteMap()) {
        const auto area=source->areaAt(selection.minX,selection.minZ);
        if(!area || !area->map->contains(selection.minX+selection.width-1-area->offsetX,
            selection.minZ+selection.depth-1-area->offsetZ)) throw std::runtime_error("Selection outside saved map area");
    }

    const auto saved=nlohmann::json::parse(encodeData(SaveGame::inspect(checkpoint)));
    const auto& core=saved.at("state").at("core");
    RegionEdits edits;
    if(core.contains("regions")) edits=RegionEdits::restore(directory/"terrain",core.at("regions"),catalog);
    const auto legacy=readLegacyBlockEdits(decodeData(core.dump()),catalog);

    const int maxX=selection.minX+selection.width-1,maxZ=selection.minZ+selection.depth-1;
    for(auto cz=floorDiv(selection.minZ,16);cz<=floorDiv(maxZ,16);++cz) {
        for(auto cx=floorDiv(selection.minX,16);cx<=floorDiv(maxX,16);++cx) {
            const auto decoration=terrain.decoration(cx,cz);
            for(int z=int(std::max<std::int64_t>(selection.minZ,cz*16));z<=std::min<std::int64_t>(maxZ,cz*16+15);++z) {
                for(int x=int(std::max<std::int64_t>(selection.minX,cx*16));x<=std::min<std::int64_t>(maxX,cx*16+15);++x) {
                    const auto column=terrain.column(x,z);
                    for(int y=0;y<int(column.blocks.size());++y) {
                        if(!column.blocks[size_t(y)].empty()) map.setBlock(x,y,z,column.blocks[size_t(y)],catalog);
                    }
                }
            }
            for(const auto& cell:decoration) {
                const int x=int(cx*16)+cell.x,z=int(cz*16)+cell.z;
                if(map.contains(x,z)) map.setBlock(x,cell.y,z,cell.block,catalog);
            }
            for(const auto& [cell,id]:edits.chunk({cx,cz})) {
                const auto [x,y,z]=cell;
                if(map.contains(x,z)) map.setBlock(int(x),y,int(z),id,catalog);
            }
        }
    }
    if(!core.contains("regions")) for(const auto& [cell,id]:legacy) {
        const auto [x,y,z]=cell;
        if(map.contains(x,z)) map.setBlock(int(x),y,int(z),id,catalog);
    }
    return map;
}
}
