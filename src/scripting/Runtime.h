#pragma once
#include "scripting/Registry.h"
#include "core/Data.h"
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
    std::string currentMod() const;
    std::filesystem::path moduleRoot(const std::string& mod) const;
    std::uint64_t subscribe(std::string event,Value handler,bool hook=false,std::string owner={});
    bool unsubscribe(std::uint64_t token);
    Value filter(const std::string& hook,Value value);
    Value invoke(const Value& function,const std::vector<Value>& args);
    Value call(const std::string& builtin,const std::vector<Value>& args={});
    // Cooperative main-thread tasks. Delta is game time; no wall-clock waits.
    std::vector<Diagnostic> advanceTasks(double delta);
    void cancelTasks();
    Data debugSnapshot() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
