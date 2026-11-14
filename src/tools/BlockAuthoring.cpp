#include "tools/BlockAuthoring.h"
#include "assets/AssetId.h"
#include "assets/ModelCompiler.h"
#include "core/AtomicFile.h"
#include "world/WorldDefinition.h"
#include <atomic>
#include <windows.h>

namespace game {
void publishBlock(const std::filesystem::path& mods,const AuthoredBlock& block) {
    if(block.mod.empty() || block.mod.size()>48 || !validNamespace(block.mod) || block.mod=="core")
        throw std::runtime_error("Use um ID de mod com letras minúsculas, números, hífen ou sublinhado");
    if(!std::filesystem::is_directory(mods)) throw std::runtime_error("A pasta de mods precisa existir");
    for(const auto& mod:discoverMods(mods).mods) if(mod.id==block.mod) throw std::runtime_error("Já existe um mod com esse ID");
    const auto destination=mods/block.mod;
    if(std::filesystem::exists(destination)) throw std::runtime_error("Já existe uma pasta com esse ID");
    if(!block.model.empty() && (!std::filesystem::is_regular_file(block.model) || block.model.extension()!=L".glb" ||
        std::filesystem::file_size(block.model)>32*1024*1024))
        throw std::runtime_error("Escolha um modelo GLB de até 32 MiB");
    static std::atomic<unsigned> serial{};
    const auto stage=mods/ (".editor-block-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++serial));
    if(!std::filesystem::create_directory(stage)) throw std::runtime_error("Não foi possível preparar o pacote do bloco");

    std::string bmp;
    const auto field=[&](std::uint32_t value,int bytes) {
        for(int i=0;i<bytes;++i) bmp.push_back(static_cast<char>((value>>(8*i))&255));
    };
    bmp="BM";field(54+16*16*3,4);field(0,4);field(54,4);
    field(40,4);field(16,4);field(16,4);field(1,2);field(24,2);
    field(0,4);field(16*16*3,4);for(int i=0;i<4;++i) field(0,4);
    for(int y=15;y>=0;--y) for(int x=0;x<16;++x) field(block.pixels[size_t(y)*16+x],3);

    using Json=nlohmann::json;
    const auto id=block.mod+":block";
    try {
        atomicWrite(stage/"block.bmp",bmp);
        Json assets={{"textures",{{id,{{"source","block.bmp"}}}}}};
        Json definition={{"texture",id},{"solid",block.solid}};
        if(!block.name.empty()) definition["name"]=block.name;
        if(!block.project.empty()) atomicWrite(stage/"block.bbmodel",block.project);
        if(!block.model.empty()) {
            std::filesystem::copy_file(block.model,stage/"block.glb");
            const auto cache=stage/"validate";
            const auto model=compileModel(stage/"block.glb",cache);
            if(model.parts.empty()) throw std::runtime_error("O arquivo GLB não tem geometria");
            for(const auto& file:std::filesystem::directory_iterator(cache)) std::filesystem::remove(file.path());
            std::filesystem::remove(cache);
            assets["models"][id]={{"source","block.glb"}};
            definition["model"]=id;
        }
        atomicWrite(stage/"assets.json",assets.dump(2));
        atomicWrite(stage/"world.json",Json{{"version",1},{"blocks",{{id,definition}}}}.dump(2));
        atomicWrite(stage/"mod.json",Json{{"id",block.mod},{"name",block.name.empty()?block.mod:block.name},
            {"version","1.0.0"},{"api","0.1"}}.dump(2));
        renameDirectory(stage,destination);
    } catch(...) {
        std::error_code ignored;
        const auto cache=stage/"validate";
        if(std::filesystem::is_directory(cache,ignored)) {
            for(const auto& file:std::filesystem::directory_iterator(cache,ignored)) std::filesystem::remove(file.path(),ignored);
            std::filesystem::remove(cache,ignored);
        }
        for(const auto* file:{"block.bmp","block.glb","block.bbmodel","assets.json","world.json","mod.json"})
            std::filesystem::remove(stage/file,ignored);
        std::filesystem::remove(stage,ignored);
        throw;
    }
}
}
