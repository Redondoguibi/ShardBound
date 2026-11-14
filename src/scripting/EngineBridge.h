#pragma once
#include "extensions/EnginePipeline.h"
#include "scripting/Runtime.h"
namespace game {
class EngineBridge {
public:
    explicit EngineBridge(script::Runtime& runtime);
    EnginePipeline pipeline;
    std::vector<std::string> fixedOrder={"fixed","movement","physics"};
    // Host-owned service: validates and commits physics/render configuration.
    std::function<Data(const std::string&,const Data&)> service;
    Data request(const std::string& owner,const std::string& operation,const Data& input);
private:
    script::Runtime& runtime_;
    std::map<std::string,std::vector<std::pair<std::string,Data>>> writes_;
    void configure(const std::string& owner,const std::string& name,const Data& input);
};
}
