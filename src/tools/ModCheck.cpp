#include "mods/Manifest.h"
#include "scripting/ScriptCheck.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string_view>
int main(int argc,char** argv) {
    if(argc<2 || argc>3 || (argc==3 && std::string_view(argv[2])!="--json")) {
        std::cerr<<"Usage: modcheck MODS_DIRECTORY [--json]\n"; return 2;
    }
    const bool json=argc==3;
    std::vector<game::Diagnostic> diagnostics; size_t mods=0,files=0;
    try {
        if(!std::filesystem::is_directory(argv[1])) throw std::runtime_error("Mods directory does not exist");
        auto report=game::discoverMods(argv[1]); diagnostics=std::move(report.diagnostics); mods=report.mods.size();
        for(const auto& mod:report.mods) {
            auto checked=game::script::checkScripts(mod.root,mod.entry); files+=checked.files;
            for(auto& diagnostic:checked.diagnostics) { diagnostic.mod=mod.id; diagnostics.push_back(std::move(diagnostic)); }
        }
    } catch(const std::exception& error) { diagnostics.push_back({"CHECK100",error.what(),argv[1]}); }
    bool valid=true; nlohmann::json entries=nlohmann::json::array();
    for(const auto& d:diagnostics) {
        const bool warning=d.code=="MOD106"; valid&=warning;
        if(json) entries.push_back({{"code",d.code},{"severity",warning ? "warning" : "error"},{"message",d.message},
            {"file",d.file},{"mod",d.mod},{"resource",d.resource},{"line",d.line},{"column",d.column}});
        else std::cout<<(warning ? "warning " : "error ")<<game::DiagnosticError(d).what()<<'\n';
    }
    if(json) std::cout<<nlohmann::json{{"valid",valid},{"mods",mods},{"scripts",files},{"diagnostics",entries}}.dump(2)<<'\n';
    else std::cout<<(valid ? "PASS" : "FAIL")<<": "<<mods<<" mods, "<<files<<" scripts; manifests, syntax and imports only\n";
    return valid ? 0 : 1;
}
