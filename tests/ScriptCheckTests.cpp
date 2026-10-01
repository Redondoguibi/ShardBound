#include "scripting/ScriptCheck.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    try {
        const std::filesystem::path root="user/check-tests"; std::filesystem::create_directories(root);
        auto write=[&](const char* file,const char* source) { std::ofstream output(root/file); output<<source; };
        write("main.sl","import \"lib.sl\"\nimport \"lib.sl\"\nundefined_function()\n");
        write("lib.sl","func twice(x):\n    return x * 2\n");
        auto result=game::script::checkScripts(root,"main.sl");
        require(result.diagnostics.empty() && result.files==2,"Checker executed code or repeated imports");
        write("lib.sl","import \"main.sl\"\n");
        result=game::script::checkScripts(root,"main.sl");
        require(result.diagnostics.size()==1 && result.diagnostics[0].code=="SCRIPT309","Circular import was not diagnosed");
        write("main.sl","import \"../outside.sl\"\n");
        result=game::script::checkScripts(root,"main.sl");
        require(!result.diagnostics.empty(),"Import escaped mod root");
        write("main.sl","import \"library.txt\"\n"); write("library.txt","x = 1\n");
        result=game::script::checkScripts(root,"main.sl");
        require(result.diagnostics.size()==1 && result.diagnostics[0].code=="SCRIPT308","Non-script import accepted");
        write("main.sl","func broken(:\n    return 1\n");
        result=game::script::checkScripts(root,"main.sl");
        require(!result.diagnostics.empty() && result.diagnostics[0].line>0,"Syntax diagnostic lost location");
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
