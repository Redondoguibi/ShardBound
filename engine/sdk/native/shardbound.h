#ifndef SHARDBOUND_NATIVE_H
#define SHARDBOUND_NATIVE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SB_ABI_VERSION 1u
#define SB_EXPORT __declspec(dllexport)
#define SB_CALL __cdecl
enum { SB_OK=0, SB_ERROR=1, SB_BUFFER_TOO_SMALL=2 };
/* JSON UTF-8, NUL terminated. Output sizes include NUL. All calls on runtime thread.
   A callback cannot retain input/output pointers. No exceptions across the ABI. */
typedef int (SB_CALL *SbCallback)(void* user,const char* input,char* output,uint32_t capacity);
typedef struct SbHost {
    uint32_t size,abi;
    void* context;
    int (SB_CALL *call)(void* context,const char* operation,const char* input,char* output,uint32_t capacity,uint32_t* required);
    int (SB_CALL *subscribe)(void* context,const char* event,SbCallback callback,void* user,int hook,uint64_t* token);
    int (SB_CALL *unsubscribe)(void* context,uint64_t token);
} SbHost;
typedef struct SbPlugin {
    uint32_t size,abi;
    void* user;
    void (SB_CALL *shutdown)(void* user);
} SbPlugin;
typedef int (SB_CALL *SbInit)(const SbHost* host,SbPlugin* plugin);
/* Export: int SB_CALL sb_mod_init(const SbHost*, SbPlugin*). Host pointer lives
   until shutdown. API 1 is Windows x64. No STL, allocator or engine types cross. */
#ifdef __cplusplus
}
#endif
#endif
