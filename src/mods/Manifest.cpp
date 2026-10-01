#include "mods/Manifest.h"
#include "assets/AssetId.h"
#include "assets/VirtualFileSystem.h"
#include <nlohmann/json.hpp>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <functional>
namespace game {
Version Version::parse(std::string_view text) {
    Version result; size_t start=0; unsigned count=0;
    while (start<text.size() && count<3) {
        auto end=text.find('.',start); if (end==std::string_view::npos) end=text.size();
        auto [ptr,error]=std::from_chars(text.data()+start,text.data()+end,result.parts[count++]);
        if (error!=std::errc{} || ptr!=text.data()+end) throw DiagnosticError({"MOD101","Invalid numeric version: "+std::string(text)});
        start=end+1;
    }
    if (count<2 || start<text.size() || text.empty() || text.back()=='.') throw DiagnosticError({"MOD101","Version requires two or three numeric components"});
    return result;
}
bool Dependency::accepts(Version candidate) const {
    if (operation.empty()) return true;
    if (operation==">=") return candidate>=version;
    if (operation=="<=") return candidate<=version;
    if (operation==">") return candidate>version;
    if (operation=="<") return candidate<version;
    return candidate==version;
}
ModManifest readManifest(const std::filesystem::path& root) {
    const auto file=root/"mod.json";
    try {
        std::ifstream input(file); if (!input) throw std::runtime_error("Cannot read manifest");
        auto json=nlohmann::json::parse(input);
        ModManifest mod;
        mod.root=std::filesystem::canonical(root);
        mod.id=json.at("id").get<std::string>(); mod.name=json.at("name").get<std::string>();
        mod.version=Version::parse(json.at("version").get<std::string>());
        if (!validNamespace(mod.id) || mod.id=="core" || mod.name.empty()) throw std::runtime_error("Invalid/reserved mod ID or empty name");
        if (json.at("api").get<std::string>()!="0.1") throw std::runtime_error("Unsupported runtime API (expected 0.1)");
        mod.entry=json.value("entry",std::string{});
        mod.native=json.value("native",std::string{});
        if(!mod.native.empty()) {
            const std::filesystem::path native(mod.native);
            if(native.is_absolute() || native.has_root_name() || mod.native.find(':')!=std::string::npos || native.extension()!=L".dll") throw std::runtime_error("Native entry must be a relative .dll path");
            for(const auto& part:native) if(part==L"..") throw std::runtime_error("Native entry cannot traverse directories");
        }
        if (!mod.entry.empty()) (void)VirtualFileSystem::contained(root,mod.entry);
        for (auto key:{"dependencies","optional_dependencies"}) {
            if (!json.contains(key)) continue;
            if (!json[key].is_array()) throw std::runtime_error("Dependencies must be arrays of strings");
            for (const auto& item:json[key]) {
                std::istringstream text(item.get<std::string>()); Dependency dep; std::string version,extra;
                dep.optional=std::string_view(key)=="optional_dependencies"; text>>dep.id;
                if (!validNamespace(dep.id)) throw std::runtime_error("Invalid dependency ID");
                if (text>>dep.operation) {
                    if (!(text>>version) || (text>>extra) || (dep.operation!=">=" && dep.operation!="<=" && dep.operation!=">" && dep.operation!="<" && dep.operation!="=="))
                        throw std::runtime_error("Use dependency 'id' or 'id >= 1.0.0' with ==, <, <=, >, >=");
                    dep.version=Version::parse(version);
                }
                mod.dependencies.push_back(std::move(dep));
            }
        }
        return mod;
    } catch (const std::exception& e) { throw DiagnosticError({"MOD102",e.what(),file.string()}); }
}
Discovery discoverMods(const std::filesystem::path& directory) {
    Discovery result;
    if (!std::filesystem::exists(directory)) return result;
    std::vector<std::filesystem::path> roots;
    for (const auto& entry:std::filesystem::directory_iterator(directory)) if (entry.is_directory() && std::filesystem::exists(entry.path()/"mod.json")) roots.push_back(entry.path());
    std::sort(roots.begin(),roots.end());
    for (const auto& root:roots) {
        try { result.mods.push_back(readManifest(root)); }
        catch (const DiagnosticError& e) { result.diagnostics.push_back(e.diagnostic); }
    }
    result.mods=resolveMods(std::move(result.mods),result.diagnostics); return result;
}
std::vector<ModManifest> resolveMods(std::vector<ModManifest> mods,std::vector<Diagnostic>& diagnostics) {
    std::sort(mods.begin(),mods.end(),[](const auto& a,const auto& b) { return a.id<b.id; });
    std::map<std::string,unsigned> counts;
    for (const auto& mod:mods) ++counts[mod.id];
    std::erase_if(mods,[&](const auto& mod) {
        if (counts[mod.id]<2) return false;
        diagnostics.push_back({"MOD103","Duplicate mod ID; all colliding packages excluded",(mod.root/"mod.json").string(),mod.id}); return true;
    });
    std::map<std::string,size_t> indices;
    for (size_t i=0;i<mods.size();++i) indices[mods[i].id]=i;
    std::vector<int> states(mods.size()); std::vector<size_t> stack,order;
    std::function<void(size_t)> visit=[&](size_t i) {
        if (states[i]==2) return;
        if (states[i]==1) {
            const auto begin=std::find(stack.begin(),stack.end(),i);
            for (auto it=begin;it!=stack.end();++it) mods[*it].dependencyCycle=true;
            diagnostics.push_back({"MOD104","Dependency cycle; scripts in cycle cannot initialize",{},mods[i].id}); return;
        }
        states[i]=1; stack.push_back(i);
        for (const auto& dep:mods[i].dependencies) {
            const auto found=indices.find(dep.id);
            if (found==indices.end()) {
                diagnostics.push_back({dep.optional ? "MOD106" : "MOD105","Missing dependency: "+dep.id+"; independent content will still load",{},mods[i].id}); continue;
            }
            if (!dep.accepts(mods[found->second].version)) {
                diagnostics.push_back({"MOD107","Incompatible dependency version: "+dep.id,{},mods[i].id}); continue;
            }
            visit(found->second);
        }
        stack.pop_back(); states[i]=2; order.push_back(i);
    };
    for (size_t i=0;i<mods.size();++i) visit(i);
    std::vector<ModManifest> sorted; for (auto i:order) sorted.push_back(std::move(mods[i])); return sorted;
}
}
