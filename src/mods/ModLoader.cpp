#include "mods/ModLoader.h"
#include "core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
namespace game {
LoadReport ModLoader::load(const std::filesystem::path& directory,PreparationProgress progress) {
    auto discovery=discoverMods(directory);
    LoadReport report{std::move(discovery.mods),std::move(discovery.diagnostics)};
    for (const auto& mod:report.mods) {
        try {
            vfs_.mount(mod.id,mod.root);
            if (!std::filesystem::exists(mod.root/"assets.json")) { LOG_INFO("MODS","Discovered "+mod.id+" (no assets)"); continue; }
            const auto file=vfs_.resolve(mod.id,"assets.json");
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
                        (void)vfs_.resolve(mod.id,path);
                        if (type=="models") {
                            if(entry.contains("import") && (!entry["import"].is_object() || !entry["import"].empty())) throw std::runtime_error("Model import settings are not supported yet");
                            assets_.registerModel({assetId,path}); (void)assets_.model(assetId);
                        } else if(type=="sounds") {
                            if(entry.contains("import")) throw std::runtime_error("Sound import settings are not supported yet");
                            assets_.registerSound({assetId,path,entry.value("streaming",false)});
                        } else if (type=="textures") {
                            bool srgb=true,nearest=true;
                            if (entry.contains("import")) {
                                const auto& settings=entry.at("import"); srgb=settings.value("srgb",true);
                                const auto filter=settings.value("filter",std::string("nearest"));
                                if (filter!="nearest" && filter!="linear") throw std::runtime_error("Unknown texture filter");
                                nearest=filter=="nearest";
                                for (const auto& [name,value]:settings.items()) { (void)value; if (name!="srgb" && name!="filter") throw std::runtime_error("Unsupported texture import setting: "+name); }
                            }
                            assets_.registerTexture({assetId,path,srgb,nearest}); (void)assets_.texture(assetId);
                        } else throw std::runtime_error("Unsupported asset registry in v0: "+type);
                        ++report.registeredAssets;
                    } catch (const std::exception& e) { report.diagnostics.push_back({"MOD203",e.what(),file.string(),mod.id,id}); }
                }
            }
            if (progress) progress(mod.id,total,total);
            LOG_INFO("MODS","Loaded asset registry: "+mod.id);
        } catch (const PreparationCancelled&) { throw; }
        catch (const std::exception& e) { report.diagnostics.push_back({"MOD201",e.what(),(mod.root/"assets.json").string(),mod.id}); }
    }
    for (const auto& d:report.diagnostics) LOG_WARN("MODS",d.code+" ["+d.mod+"] "+d.message);
    return report;
}
}
