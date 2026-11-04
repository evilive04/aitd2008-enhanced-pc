// Install once after successful CreateDevice and before native shadow pools exist.
// Exact-build signatures cover every size selector, allocator and bias tier.
#include "native_shadow_sites.h"
namespace native_shadows {
static bool enabled=false;
static volatile LONG installation=0;
static bool installed=false;
struct Change { BYTE* address; unsigned size; BYTE before[16]; BYTE after[16]; DWORD protection; bool writable; };
static bool Apply(Change* changes,unsigned count) {
    for(unsigned i=0;i<count;++i)
        if(memcmp(changes[i].address,changes[i].before,changes[i].size))return false;
    // Acquire all protection changes before writing even one operand. Each
    // range is contained in a single page; duplicate pages unwind in reverse.
    unsigned ready=0;
    for(;ready<count;++ready) {
        auto& c=changes[ready];
        if(!VirtualProtect(c.address,c.size,PAGE_EXECUTE_READWRITE,&c.protection))break;
        c.writable=true;
    }
    if(ready!=count) {
        while(ready){auto& c=changes[--ready];DWORD ignored;VirtualProtect(c.address,c.size,c.protection,&ignored);c.writable=false;}
        return false;
    }
    bool complete=true;
    __try {
        for(unsigned i=0;i<count;++i)memcpy(changes[i].address,changes[i].after,changes[i].size);
        for(unsigned i=0;i<count;++i)
            if(memcmp(changes[i].address,changes[i].after,changes[i].size)){complete=false;break;}
    } __except(EXCEPTION_EXECUTE_HANDLER) {complete=false;}
    if(!complete) {
        // Protections are still writable, so restore the entire transaction.
        __try {for(unsigned i=0;i<count;++i)memcpy(changes[i].address,changes[i].before,changes[i].size);}
        __except(EXCEPTION_EXECUTE_HANDLER) {Log("Native shadow rollback write failed");}
    }
    for(unsigned i=count;i>0;--i) {
        auto& c=changes[i-1];DWORD ignored;
        VirtualProtect(c.address,c.size,c.protection,&ignored);c.writable=false;
        FlushInstructionCache(GetCurrentProcess(),c.address,c.size);
    }
    return complete;
}
static bool Patch() {
    Change changes[siteCount+1]{};
    static_assert(siteCount==14,"audit native shadow operands including both bias producers");
    for(unsigned i=0;i<siteCount;++i) {
        const auto& s=sites[i];auto& c=changes[i];
        c.address=reinterpret_cast<BYTE*>(s.address);c.size=5;
        c.before[0]=s.opcode;c.after[0]=s.opcode;
        memcpy(c.before+1,&s.original,4);const DWORD doubled=s.original*2;
        memcpy(c.after+1,&doubled,4);
    }
    auto& table=changes[siteCount];table.address=reinterpret_cast<BYTE*>(tableAddress);table.size=16;
    memcpy(table.before,sizes,16);
    const DWORD doubled[]={2048,1024,512,256};memcpy(table.after,doubled,16);
    return Apply(changes,siteCount+1);
}
}
static void ConfigureNativeShadowQuality(const wchar_t* config,bool supported) {
    native_shadows::enabled=supported&&ConfigFlag(L"Graphics",L"HighResolutionShadows",false,config);
}
static void InstallNativeShadowQuality(IDirect3DDevice9* device) {
    using namespace native_shadows;
    if(!enabled||!device||InterlockedCompareExchange(&installation,1,0))return;
    D3DCAPS9 caps{};
    if(FAILED(device->GetDeviceCaps(&caps))||caps.MaxTextureWidth<2048||caps.MaxTextureHeight<2048) {
        enabled=false;Log("Native shadow quality skipped: device lacks 2048 texture support");return;
    }
    bool applied=false;
    __try {applied=Patch();} __except(EXCEPTION_EXECUTE_HANDLER) {applied=false;}
    installed=applied;if(!applied)enabled=false;
    Log("Native shadow quality x2 installed=%d tiers=2048,1024,512,256 native pool counts retained",applied);
}
