#include "shardbound.h"
#include <cstring>
#include <string>
#include <vector>
namespace {
const SbHost* host=nullptr;unsigned ticks=0;
int mutate(const char* operation,const std::string& input) {char output[32]{};uint32_t required=0;return host->call(host->context,operation,input.c_str(),output,sizeof(output),&required);}
int SB_CALL physics(void*,const char*,char* output,uint32_t capacity) {
    if(capacity<5 || mutate("engine.default","{}")!=SB_OK) return SB_ERROR;
    ++ticks;if(mutate("state.set","{\"native_physics_ticks\":"+std::to_string(ticks)+"}")!=SB_OK) return SB_ERROR;
    std::memcpy(output,"true",5);return SB_OK;
}
}
extern "C" SB_EXPORT int SB_CALL sb_mod_init(const SbHost* api,SbPlugin* plugin) {
    if(!api || !plugin || api->abi!=SB_ABI_VERSION || api->size<sizeof(SbHost) || plugin->size<sizeof(SbPlugin)) return SB_ERROR;host=api;ticks=0;plugin->abi=SB_ABI_VERSION;plugin->size=sizeof(SbPlugin);
    uint32_t required=0;if(host->call(host->context,"engine.info","null",nullptr,0,&required)!=SB_BUFFER_TOO_SMALL || required>1024*1024 || required<2) return SB_ERROR;
    std::vector<char> information(required);if(host->call(host->context,"engine.info","null",information.data(),required,&required)!=SB_OK || std::string(information.data()).find("streaming")==std::string::npos) return SB_ERROR;
    uint64_t token=0;if(host->subscribe(host->context,"engine_probe:physics",physics,nullptr,1,&token)!=SB_OK) return SB_ERROR;
    return mutate("engine.register",R"({"id":"engine_probe:physics","stage":"physics","handler":"engine_probe:physics","options":{"api":1,"mode":"replace","replaces":"core:default"}})");
}
