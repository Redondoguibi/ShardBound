#include "mods/Manifest.h"
#include "core/PathText.h"
#include "scripting/ScriptCheck.h"
#include "persistence/SaveGame.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string_view>
#include <fstream>
#include "assets/StudioPackage.h"
#include "assets/ShaderCompiler.h"
#include "world/WorldDefinition.h"
#include "world/FiniteMap.h"
int wmain(int argc,wchar_t** argv) {
    if(argc==3 && std::wstring_view(argv[1])==L"--studio") {
        try { const auto p=game::loadStudioPackage(argv[2],std::filesystem::temp_directory_path()/"shardbound-studio-validation");std::cout<<nlohmann::json{{"valid",true},{"project",p->manifest["project"]},{"models",p->manifest["models"].size()},{"clips",p->events["clips"].size()},{"effects",p->effects["definitions"].size()},{"sounds",p->events["sounds"].size()}}.dump(2)<<'\n';return 0; }
        catch(const std::exception& e){std::cout<<nlohmann::json{{"valid",false},{"error",e.what()}}.dump(2)<<'\n';return 1;}
    }
    if(argc>=2 && std::wstring_view(argv[1])==L"--save") {
        if(argc<3 || argc>4 || (argc==4 && std::wstring_view(argv[3])!=L"--json")) { std::cerr<<"Usage: modcheck --save FILE [--json]\n"; return 2; }
        try {
            const auto data=game::SaveGame::inspect(argv[2]);
            const auto json=nlohmann::json::parse(game::encodeData(data));
            const nlohmann::json summary={{"valid_envelope",true},{"format",json.at("format")},{"version",json.at("version")},{"mods",json.at("mods")},{"entities",json.at("entities").size()}};
            std::cout<<summary.dump(2)<<'\n'; return 0;
        } catch(const std::exception& error) {
            std::cout<<nlohmann::json{{"valid_envelope",false},{"error",error.what()}}.dump(2)<<'\n'; return 1;
        }
    }
    if(argc<2 || argc>3 || (argc==3 && std::wstring_view(argv[2])!=L"--json")) {
        std::cerr<<"Usage: modcheck MODS_DIRECTORY [--json]\n"; return 2;
    }
    const bool json=argc==3;
    std::vector<game::Diagnostic> diagnostics; size_t mods=0,files=0;
    try {
        if(!std::filesystem::is_directory(argv[1])) throw std::runtime_error("Mods directory does not exist");
        auto report=game::discoverMods(argv[1]); diagnostics=std::move(report.diagnostics); mods=report.mods.size();
        game::VirtualFileSystem shaderFiles;
        for(const auto& mod:report.mods) shaderFiles.mount(mod.id,mod.root);
        for(const auto& mod:report.mods) {
            if(!mod.native.empty()) {
                try { (void)game::VirtualFileSystem::contained(mod.root,mod.native); }
                catch(const std::exception& error) { diagnostics.push_back({"CHECK103",error.what(),mod.native,mod.id}); }
            }
            try {
                if(std::filesystem::exists(mod.root/"assets.json")){std::ifstream input(mod.root/"assets.json");const auto registry=nlohmann::json::parse(input);
                    if(registry.contains("shaders")) {
                        const auto library=game::readShaderText(std::filesystem::absolute(argv[0]).parent_path()/"assets/core/shaders/forward.hlsl");
                        if(!registry["shaders"].is_object()) throw std::runtime_error("Shader registry must be an object");
                        for(const auto& [id,entry]:registry["shaders"].items()) {
                            try {
                                if(game::AssetId(id).nameSpace()!=mod.id) throw std::runtime_error("Shader asset must use its mod namespace");
                                (void)game::compileShader(shaderFiles,game::shaderSource(id,entry),library);
                            } catch(const game::DiagnosticError& e) {auto diagnostic=e.diagnostic;diagnostic.mod=mod.id;diagnostic.resource=id;diagnostics.push_back(std::move(diagnostic));}
                              catch(const std::exception& e) {diagnostics.push_back({"CHECK106",e.what(),"assets.json",mod.id,id});}
                        }
                    }
                    if(registry.contains("studio"))for(const auto& [id,entry]:registry["studio"].items()){const auto p=game::loadStudioPackage(game::VirtualFileSystem::contained(mod.root,entry.at("source")),std::filesystem::temp_directory_path()/"shardbound-studio-validation",mod.id);if(p->manifest["project"]!=id)throw std::runtime_error("Studio registry ID differs from project ID");}}
            }catch(const std::exception& e){diagnostics.push_back({"CHECK104",e.what(),"assets.json",mod.id});}
            auto checked=game::script::checkScripts(mod.root,mod.entry); files+=checked.files;
            for(auto& diagnostic:checked.diagnostics) { diagnostic.mod=mod.id; diagnostics.push_back(std::move(diagnostic)); }
        }
        bool hasWorld=false;for(const auto& mod:report.mods) hasWorld|=std::filesystem::exists(mod.root/"world.json");
        if(hasWorld) {
            try { const auto world=game::WorldDefinition::load(report.mods);
                auto asset=[&](const std::string& category,const std::string& id) -> nlohmann::json {
                    const game::AssetId key(id);
                    for(const auto& mod:report.mods) if(mod.id==key.nameSpace()) {
                        std::ifstream file(mod.root/"assets.json");const auto registry=nlohmann::json::parse(file);
                        const auto entry=registry.at(category).at(id);
                        (void)game::VirtualFileSystem::contained(mod.root,entry.at("source").get<std::string>());return entry;
                    }
                    throw std::runtime_error("World asset is not provided by a mod: "+id);
                };
                for(const auto& [id,block]:world.blocks) {
                    (void)id;(void)asset("textures",block.texture);
                    if(!block.model.empty()) (void)asset("models",block.model);
                    if(!block.shader.empty()) {
                        const auto shader=game::shaderSource(block.shader,asset("shaders",block.shader));
                        if(shader.kind!=game::ShaderKind::Surface) throw std::runtime_error("Block requires a surface shader: "+block.shader);
                        if(!block.shaderTexture.empty()) (void)asset("textures",block.shaderTexture);
                    }
                }
                for(const auto& [id,map]:world.maps) {
                    (void)id;
                    for(const auto& area:map->areas()) for(const auto& prop:area.map->props()) {
                        bool registered=false;
                        for(const auto& mod:report.mods) {
                            if(mod.id!=game::AssetId(prop.model).nameSpace()) continue;
                            std::ifstream input(mod.root/"assets.json");
                            if(!input) break;
                            const auto registry=nlohmann::json::parse(input);
                            if(registry.contains("models") && registry["models"].contains(prop.model)) {
                                (void)asset("models",prop.model);registered=true;break;
                            }
                            if(registry.contains("studio")) for(const auto& entry:registry["studio"]) {
                                const auto source=game::VirtualFileSystem::contained(mod.root,entry.at("source").get<std::string>());
                                const auto package=game::loadStudioPackage(source,std::filesystem::temp_directory_path()/"shardbound-studio-validation",mod.id);
                                for(const auto& model:package->manifest.at("models")) if(model.at("id")==prop.model) registered=true;
                            }
                        }
                        if(!registered) throw std::runtime_error("Unknown placed model: "+prop.model);
                    }
                }
            } catch(const std::exception& e){diagnostics.push_back({"CHECK105",e.what(),"world.json"});}
        }
    } catch(const std::exception& error) {
        diagnostics.push_back({"CHECK100",error.what(),game::pathText(argv[1])});
    }
    bool valid=true; nlohmann::json entries=nlohmann::json::array();
    for(const auto& d:diagnostics) {
        const bool warning=d.code=="MOD106"; valid&=warning;
        if(json) entries.push_back({{"code",d.code},{"severity",warning ? "warning" : "error"},{"message",d.message},
            {"file",d.file},{"mod",d.mod},{"resource",d.resource},{"line",d.line},{"column",d.column}});
        else std::cout<<(warning ? "warning " : "error ")<<game::DiagnosticError(d).what()<<'\n';
    }
    if(json) std::cout<<nlohmann::json{{"valid",valid},{"mods",mods},{"scripts",files},{"diagnostics",entries}}.dump(2,' ',false,nlohmann::json::error_handler_t::replace)<<'\n';
    else std::cout<<(valid ? "PASS" : "FAIL")<<": "<<mods<<" mods, "<<files<<" scripts; manifests, syntax, imports, world content, shaders and Studio packages\n";
    return valid ? 0 : 1;
}
