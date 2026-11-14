#include "tools/MapPackage.h"
#include "assets/AssetId.h"
#include "assets/VirtualFileSystem.h"
#include "core/AtomicFile.h"
#include "core/PathText.h"
#include "tools/EditorProcess.h"
#include "world/FiniteMap.h"
#include "world/WorldDefinition.h"
#include <Windows.h>
#include <atomic>
#include <fstream>
#include <set>

namespace game {
namespace {
using Json=nlohmann::json;
bool within(const std::filesystem::path& path,const std::filesystem::path& root) {
    auto part=path.begin();
    for(const auto& name:root) {
        if(part==path.end() || CompareStringOrdinal(name.c_str(),-1,part->c_str(),-1,TRUE)!=CSTR_EQUAL) return false;
        ++part;
    }
    return true;
}
}

void exportMapPackage(const FiniteMap& map,const std::filesystem::path& content,
    const std::filesystem::path& destination,const std::filesystem::path& modcheck) {
    const auto source=std::filesystem::canonical(content);
    const auto output=std::filesystem::weakly_canonical(destination);
    if(within(output,source)) throw std::runtime_error("Escolha um destino fora da pasta de origem");
    if(std::filesystem::exists(output)) throw std::runtime_error("A pasta do pacote já existe");
    const auto id=pathText(output.filename());
    if(!validNamespace(id) || id=="core" || id.size()>48) throw std::runtime_error("Use um ID com letras minúsculas, números, hífen ou sublinhado");
    const auto active=discoverMods(source);
    for(const auto& diagnostic:active.diagnostics) if(diagnostic.code!="MOD106") throw DiagnosticError(diagnostic);
    const auto catalog=WorldDefinition::load(active.mods);
    if(catalog.maps.empty()) throw std::runtime_error("Carregue os mods do mapa oficial primeiro");
    if(map.exit()) throw std::runtime_error("Escolha um mapa principal para exportar. Interiores são publicados com a porta que os conecta ao mapa.");
    map.validateDoors(catalog);
    const auto mapId=catalog.maps.begin()->first;
    const auto document=map.json();
    const auto encoded=document.dump();
    if(encoded.size()>8*1024*1024) throw std::runtime_error("O mapa exportado excede 8 MiB");

    auto dependencies=catalog.mapPlacementMods;
    dependencies.insert(AssetId(mapId).nameSpace());
    const auto collect=[&](const Json& area) {
        for(const auto& run:area.at("runs")) {
            const auto blockId=run.at(2).get<std::string>();
            dependencies.insert(AssetId(blockId).nameSpace());
            const auto& block=catalog.blocks.at(blockId);
            for(const auto& asset:{block.texture,block.shader,block.shaderTexture,block.model})
                if(!asset.empty()) dependencies.insert(AssetId(asset).nameSpace());
        }
        for(const auto& zone:area.at("biomes")) dependencies.insert(AssetId(zone.at(4).get<std::string>()).nameSpace());
        for(const auto& prop:area.value("props",Json::array())) dependencies.insert(AssetId(prop.at(0).get<std::string>()).nameSpace());
    };
    collect(document);
    for(const auto& room:document.value("interiors",Json::array())) collect(room.at("map"));
    dependencies.erase("core");

    static std::atomic<unsigned> serial{};
    const auto stage=output.parent_path()/(".editor-map-"+std::to_string(GetCurrentProcessId())+"-"+
        std::to_string(GetTickCount64())+"-"+std::to_string(++serial));
    for(const auto& mod:active.mods) {
        if(mod.id==id) throw std::runtime_error("Escolha um novo ID para o mod de mapa");
        if(within(stage,mod.root)) throw std::runtime_error("Escolha um destino fora da pasta de origem dos mods");
    }
    if(!std::filesystem::create_directory(stage)) throw std::runtime_error("Não foi possível preparar o pacote de mapa");
    try {
        for(const auto& mod:active.mods) {
            const auto copy=stage/mod.id;
            std::filesystem::copy(mod.root,copy,std::filesystem::copy_options::recursive|std::filesystem::copy_options::skip_symlinks);
            if(!std::filesystem::exists(copy/"world.json")) continue;
            const auto worldPath=VirtualFileSystem::contained(copy,"world.json");
            Json world;
            {std::ifstream input(worldPath,std::ios::binary);world=Json::parse(input);}
            if(world.contains("map_overrides") && world.at("map_overrides").erase(mapId)>0)
                atomicWrite(worldPath,world.dump(2));
        }
        atomicWrite(stage/id/"map.json",encoded);
        atomicWrite(stage/id/"world.json",Json{{"version",1},{"map_overrides",{{mapId,
            {{"source","map.json"},{"baked_placements",catalog.mapPlacementMods}}}}}}.dump(2));
        atomicWrite(stage/id/"mod.json",Json{{"id",id},{"name",id},{"api","0.1"},{"version","1.0.0"},
            {"dependencies",dependencies}}.dump(2));
        const auto checked=runEditorProcess(modcheck.wstring(),{stage.wstring(),L"--json"},stage,{},{});
        if(checked.exitCode) throw std::runtime_error("Validação do pacote falhou (código "+std::to_string(checked.exitCode)+").\n"+checked.output.substr(0,10000));
        renameDirectory(stage,output);
    } catch(...) {
        std::error_code ignored;
        if(std::filesystem::weakly_canonical(stage,ignored)==stage && !ignored)
            std::filesystem::remove_all(stage,ignored);
        throw;
    }
}
}
