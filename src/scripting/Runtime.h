#pragma once
#include "scripting/Registry.h"
#include <filesystem>
namespace game::script {
class Runtime {
public:
    Runtime();
    ~Runtime();
    void bind(std::string name,Value value);
    void registerSymbol(std::string name);
    void load(const std::string& mod,const std::filesystem::path& root,const std::string& entry,const std::vector<std::string>& dependencies={});
    void execute(const std::string& mod,std::string_view source,const std::string& file="<memory>");
    Value global(const std::string& mod,const std::string& name) const;
    std::vector<Diagnostic> emit(const std::string& event,const std::vector<Value>& args={});
    Registry& registry();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
