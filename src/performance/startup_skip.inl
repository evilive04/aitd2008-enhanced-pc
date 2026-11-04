// Exact native boot-video callsites only. Never synthesize PressAny input or
// modify title/menu presentation, profiles, warnings or story movie requests.
#include "startup_skip_policy.h"
namespace startup_skip {
static bool enabled=false,installed=false;
static volatile LONG installAttempt=0;
static unsigned skipped=0;
using MovieFunction=bool(WINAPI*)(const char*,DWORD);
static MovieFunction nativeMovie=reinterpret_cast<MovieFunction>(0x559890);
static void Configure(bool value){enabled=value;}
static bool Enabled(){return enabled;}
static bool StartupMovie(const char* path,DWORD flags){
    __try{
        return enabled&&installed&&*reinterpret_cast<DWORD*>(0xfaea54)==0&&
            !_stricmp(reinterpret_cast<const char*>(0xfae538),"MainMenu")&&IntroMovie(path,flags);
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool WINAPI Movie(const char* path,DWORD flags){
    // 454B20 ignores the default-list movie results; its config-list callback
    // likewise returns directly after559890. False is the native missing-film
    // result. Keep initialization before/after these requests authoritative.
    if(StartupMovie(path,flags)){
        ++skipped;Log("Startup videos: skipped native boot movie %s total=%u",path,skipped);return false;
    }
    return nativeMovie(path,flags);
}
static void Install(){
    if(!enabled||InterlockedCompareExchange(&installAttempt,1,0))return;
    struct Patch{DWORD address;unsigned char expected[5];DWORD protection;};
    Patch sites[]={
        {0x454b8e,{0xe8,0xfd,0x4c,0x10,0},0}, // default Atari
        {0x454b9a,{0xe8,0xf1,0x4c,0x10,0},0}, // default Eden
        {0x454ba6,{0xe8,0xe5,0x4c,0x10,0},0}, // default boot credits0
        {0x4557a7,{0xe8,0xe4,0x40,0x10,0},0}  // Movies/BootMovie callback only
    };
    for(const auto& s:sites)if(memcmp(reinterpret_cast<void*>(s.address),s.expected,5)){
        enabled=false;Log("Startup videos signature mismatch at %08lx",s.address);return;
    }
    unsigned writable=0;
    for(auto& s:sites){if(!VirtualProtect(reinterpret_cast<void*>(s.address),5,PAGE_EXECUTE_READWRITE,&s.protection))break;++writable;}
    if(writable!=4){
        for(int i=static_cast<int>(writable)-1;i>=0;--i){DWORD ignored{};VirtualProtect(reinterpret_cast<void*>(sites[i].address),5,sites[i].protection,&ignored);}
        enabled=false;Log("Startup videos native page access failed");return;
    }
    for(const auto& s:sites){auto* out=reinterpret_cast<unsigned char*>(s.address);out[0]=0xe8;
        *reinterpret_cast<DWORD*>(out+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&Movie)-s.address-5);
        FlushInstructionCache(GetCurrentProcess(),out,5);
    }
    for(int i=3;i>=0;--i){DWORD ignored{};VirtualProtect(reinterpret_cast<void*>(sites[i].address),5,sites[i].protection,&ignored);}
    installed=true;Log("Startup videos installed: Atari/Eden/credits0 boot requests only; PressAny title and story videos retained");
}
}
