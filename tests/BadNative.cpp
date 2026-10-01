#include "shardbound.h"
int SB_CALL invalidCallback(void*,const char*,char*,uint32_t) { return SB_ERROR; }
extern "C" SB_EXPORT int SB_CALL sb_mod_init(const SbHost* host,SbPlugin* plugin) {
    uint64_t token=0; host->subscribe(host->context,"test:bad",invalidCallback,nullptr,0,&token);
    plugin->abi=999; return SB_OK;
}
