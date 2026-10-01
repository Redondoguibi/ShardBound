#include "shardbound.h"
#include <cstring>
#include <string>
namespace {
const SbHost* host=nullptr;
int SB_CALL start(void*,const char*,char*,uint32_t) {
    char output[256]; uint32_t required=0;
    if(host->call(host->context,"state.set","{\"native_started\":true}",output,sizeof(output),&required)!=SB_OK) return SB_ERROR;
    if(host->call(host->context,"entity.create","{\"name\":\"native-counter\"}",output,sizeof(output),&required)!=SB_OK) return SB_ERROR;
    const std::string entity(output);
    const auto args="{\"entity\":"+entity+",\"type\":\"native_sample:counter\"}";
    return host->call(host->context,"component.add",args.c_str(),output,sizeof(output),&required);
}
void SB_CALL stop(void*) {
    char output[256]; uint32_t required=0;
    host->call(host->context,"log","{\"message\":\"Native sample shut down\"}",output,sizeof(output),&required);
}
}
extern "C" SB_EXPORT int SB_CALL sb_mod_init(const SbHost* api,SbPlugin* plugin) {
    if(!api || !plugin || api->abi!=SB_ABI_VERSION || api->size<sizeof(SbHost) || plugin->size<sizeof(SbPlugin)) return SB_ERROR;
    host=api; plugin->abi=SB_ABI_VERSION; plugin->size=sizeof(SbPlugin); plugin->shutdown=stop;
    char output[256]; uint32_t required=0;
    if(host->call(host->context,"component.define","{\"type\":\"native_sample:counter\",\"version\":1,\"defaults\":{\"count\":1}}",output,sizeof(output),&required)!=SB_OK) return SB_ERROR;
    uint64_t token=0;
    return host->subscribe(host->context,"on_start",start,nullptr,0,&token);
}
