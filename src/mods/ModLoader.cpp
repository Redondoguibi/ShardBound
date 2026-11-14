#include "mods/ModLoader.h"
#include "core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <set>
#include <algorithm>
#include "scripting/ScriptCheck.h"
#include "audio/AudioSystem.h"
namespace game {
LoadReport ModLoader::load(const std::filesystem::path& directory,PreparationProgress progress) {
    auto discovery=discoverMods(directory);
    LoadReport report{std::move(discovery.mods),std::move(discovery.diagnostics)};
    std::set<std::string> failed;
    for (const auto& mod:report.mods) {
        bool blocked=mod.dependencyCycle;
        for(const auto& dep:mod.dependencies) if(!dep.optional) {
            const auto found=std::find_if(report.mods.begin(),report.mods.end(),[&](const auto& candidate){return candidate.id==dep.id && dep.accepts(candidate.version);});
            blocked|=failed.contains(dep.id) || found==report.mods.end();
        }
        if(blocked) {failed.insert(mod.id);report.diagnostics.push_back({"MOD204","Package excluded: required dependency unavailable or cycle",{},mod.id});continue;}
        const auto scripts=script::checkScripts(mod.root,mod.entry);
        if(!scripts.diagnostics.empty()) {for(auto d:scripts.diagnostics) {d.mod=mod.id;report.diagnostics.push_back(std::move(d));}failed.insert(mod.id);continue;}
        const auto diagnosticsBefore=report.diagnostics.size();size_t registered=0;
        VirtualFileSystem stagedFiles=vfs_;auto staged=assets_.stage(stagedFiles);
        try {
            stagedFiles.mount(mod.id,mod.root);
            if (!std::filesystem::exists(mod.root/"assets.json")) { vfs_.swap(stagedFiles);assets_.publish(*staged);LOG_INFO("MODS","Discovered "+mod.id+" (no assets)");continue; }
            const auto file=stagedFiles.resolve(mod.id,"assets.json");
            std::ifstream input(file); auto json=nlohmann::json::parse(input);
            if (!json.is_object()) throw std::runtime_error("assets.json must be an object");
            size_t total=0,done=0; for (const auto& [type,entries]:json.items()) { (void)type; total+=entries.size(); }
            for (const auto& [type,entries]:json.items()) {
                if (!entries.is_object()) { report.diagnostics.push_back({"MOD202","Asset registry must be an object",file.string(),mod.id,type}); continue; }
                for (const auto& [id,entry]:entries.items()) {
                    if (progress) progress(mod.id,done++,total);
                    try {
                        AssetId assetId(id);
                        if (assetId.nameSpace()!=mod.id) throw std::runtime_error("New assets must use their own mod namespace; implicit replacements are forbidden");
                        const auto path=entry.at("source").get<std::string>();
                        (void)stagedFiles.resolve(mod.id,path);
                        if (type=="shaders") {
                            staged->registerShader(shaderSource(id,entry));
                        } else if (type=="studio") {
                            staged->registerStudioPackage(assetId,path);
                        } else if (type=="models") {
                            if(entry.contains("import") && (!entry["import"].is_object() || !entry["import"].empty())) throw std::runtime_error("Model import settings are not supported yet");
                            staged->registerModel({assetId,path}); (void)staged->model(assetId);
                        } else if(type=="sounds") {
                            if(entry.contains("import")) throw std::runtime_error("Sound import settings are not supported yet");
                            validateAudio(stagedFiles.resolve(mod.id,path));
                            staged->registerSound({assetId,path,entry.value("streaming",false)});
                        } else if (type=="textures") {
                            bool srgb=true,nearest=true;
                            if (entry.contains("import")) {
                                const auto& settings=entry.at("import"); srgb=settings.value("srgb",true);
                                const auto filter=settings.value("filter",std::string("nearest"));
                                if (filter!="nearest" && filter!="linear") throw std::runtime_error("Unknown texture filter");
                                nearest=filter=="nearest";
                                for (const auto& [name,value]:settings.items()) { (void)value; if (name!="srgb" && name!="filter") throw std::runtime_error("Unsupported texture import setting: "+name); }
                            }
                            staged->registerTexture({assetId,path,srgb,nearest}); (void)staged->texture(assetId);
                        } else throw std::runtime_error("Unsupported asset registry in v0: "+type);
                        ++registered;
                    } catch(const DiagnosticError& e) { auto diagnostic=e.diagnostic;diagnostic.mod=mod.id;diagnostic.resource=id;report.diagnostics.push_back(std::move(diagnostic)); }
                    catch (const std::exception& e) { report.diagnostics.push_back({"MOD203",e.what(),file.string(),mod.id,id}); }
                }
            }
            if (progress) progress(mod.id,total,total);
            if(report.diagnostics.size()==diagnosticsBefore) {
                vfs_.swap(stagedFiles);assets_.publish(*staged);report.registeredAssets+=registered;
                LOG_INFO("MODS","Loaded asset registry: "+mod.id);
            }
        } catch (const PreparationCancelled&) { throw; }
        catch (const std::exception& e) { report.diagnostics.push_back({"MOD201",e.what(),(mod.root/"assets.json").string(),mod.id}); }
        if(report.diagnostics.size()!=diagnosticsBefore) failed.insert(mod.id);
    }
    std::erase_if(report.mods,[&](const auto& mod){return failed.contains(mod.id);});
    for (const auto& d:report.diagnostics) LOG_WARN("MODS",d.code+" ["+d.mod+"] "+d.message);
    return report;
}
}
