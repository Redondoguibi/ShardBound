#include "scripting/ScriptCheck.h"
#include "assets/VirtualFileSystem.h"
#include "core/PathText.h"
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
namespace game::script {
CheckResult checkScripts(const std::filesystem::path& root,const std::string& entry) {
    CheckResult result;
    std::map<std::filesystem::path,int> states;
    std::function<void(const std::string&,const Location&,unsigned)> visit;
    std::function<void(const Block&,unsigned)> walk;
    walk=[&](const Block& nodes,unsigned depth) {
        for(const auto& node:nodes) {
            if(node->kind==Kind::Import) visit(node->text,node->at,depth);
            walk(node->body,depth); walk(node->alternative,depth);
        }
    };
    visit=[&](const std::string& file,const Location& at,unsigned depth) {
        std::filesystem::path path;
        try {
            if(depth>=64) fail(at,"CHECK101","Import nesting limit exceeded");
            path=VirtualFileSystem::contained(root,file);
            if(path.extension()!=".sl") fail(at,"SCRIPT308","Imports must use .sl");
            if(states[path]==1) fail(at,"SCRIPT309","Circular import: "+file);
            if(states[path]==2) return;
            states[path]=1;
            std::ifstream input(path); std::ostringstream source; source<<input.rdbuf();
            if(!input) fail(at,"SCRIPT310","Cannot read script: "+file);
            const auto ast=parse(source.str(),pathText(path)); ++result.files;
            walk(ast,depth+1);
            states[path]=2;
        } catch(const DiagnosticError& error) {
            auto diagnostic=error.diagnostic;
            if(diagnostic.file.empty()) { diagnostic.file=at.file; diagnostic.line=at.line; diagnostic.column=at.column; }
            result.diagnostics.push_back(std::move(diagnostic));
            if(!path.empty()) states[path]=2;
        } catch(const std::exception& error) {
            result.diagnostics.push_back({"CHECK102",error.what(),at.file,{},{},at.line,at.column});
            if(!path.empty()) states[path]=2;
        }
    };
    if(!entry.empty()) visit(entry,{pathText(root/entry),1,1},0);
    return result;
}
}
