#include "mods/Manifest.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        game::ModManifest a,b,c; a.id="a"; b.id="b"; c.id="c";
        a.version=b.version=c.version=game::Version::parse("1.0.0");
        a.dependencies.push_back({"b",">=",game::Version::parse("1.0")});
        c.dependencies.push_back({"missing","",{},true});
        std::vector<game::Diagnostic> diagnostics;
        auto sorted=game::resolveMods({a,b,c},diagnostics);
        require(sorted.size()==3 && sorted[0].id=="b" && sorted[1].id=="a","Dependency ordering failed");
        require(diagnostics.size()==1 && diagnostics[0].code=="MOD106","Missing optional dependency not reported");
        b.dependencies.push_back({"a"}); diagnostics.clear();
        sorted=game::resolveMods({a,b,c},diagnostics);
        require(sorted.size()==3 && sorted[0].dependencyCycle && sorted[1].dependencyCycle,"Cycle not isolated");
        sorted=game::resolveMods({a,a,c},diagnostics);
        require(sorted.size()==1 && sorted[0].id=="c","Duplicate namespace silently selected");
        bool rejected=false; try { game::Version::parse("1.2.3.4"); } catch (...) { rejected=true; }
        require(rejected,"Malformed version accepted");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
