// Final mixed PCM suppression for exact r29. No OS/session volume or clock changes.
// The caller supplies a thread-safe freshness/key/focus snapshot; audio never reads game ownership.
static bool CinematicOutputSilenced();
namespace cinematic_audio {
static volatile LONG installAttempt=0;
static void* nativeConverter=reinterpret_cast<void*>(0x961450);
static void* nativeClearRing=reinterpret_cast<void*>(0x961490);
static void* observedBuffer=nullptr;
static bool ringSilenced=false; // Owned only by the original software-mixer thread.
static __declspec(naked) DWORD __cdecl ClearNativeRing(void*){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        mov esi,[ebp+8]
        call dword ptr [nativeClearRing]
        mov [ebp-8],eax
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret
    }
}
static void __cdecl ClearQueuedAudio(void* owner){
    const DWORD error=GetLastError();
    const auto buffer=*reinterpret_cast<void**>(static_cast<unsigned char*>(owner)+0x20bc);
    if(buffer!=observedBuffer){observedBuffer=buffer;ringSilenced=false;}
    const bool activeSilence=CinematicOutputSilenced();
    if(!activeSilence)ringSilenced=false;
    else if(buffer&&!ringSilenced){
        // This executes BEFORE the normal block Lock. Never nest DirectSound locks.
        // The existing engine routine locks/clears/unlocks the entire mixed ring.
        // Its final HRESULT is0 on successful native Unlock; retry failures next block.
        if(ClearNativeRing(owner)==0)ringSilenced=true;
    }
    SetLastError(error);
}
static void __cdecl SuppressPcm(short* output,int samples){
    const DWORD error=GetLastError();
    // Original9612F7 always supplies1024*channels, with at most8 channels.
    // The converter just wrote this exact locked region; preserve it unless FF is actual.
    if(output&&samples>0&&samples<=8192&&CinematicOutputSilenced())
        memset(output,0,static_cast<size_t>(samples)*sizeof(*output));
    SetLastError(error);
}
// Original961450 has custom EDI=float input, ESI=int16 output, EDX=sample count.
// Convert normally first, preserving decoder/mixer behavior and its exact post-call CPU state.
static __declspec(naked) void PcmAdapter(){
    __asm {
        call dword ptr [nativeConverter]
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push edx
        push esi
        call SuppressPcm
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret
    }
}
// Original9612B1 is MOV EAX,[EBX+20BC], before DirectSound's per-block Lock.
static __declspec(naked) void BeforeLockAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ebx
        call ClearQueuedAudio
        add esp,4
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov eax,[ebx+20bch]
        ret
    }
}
}
static void InstallCinematicAudioOutputSilence(bool supportedAndEnabled){
    using namespace cinematic_audio;
    if(!supportedAndEnabled||InterlockedCompareExchange(&installAttempt,1,0))return;
    struct Patch {DWORD address;unsigned size;unsigned char expected[6];void* replacement;DWORD protection;};
    Patch patches[]={
        {0x9612b1,6,{0x8b,0x83,0xbc,0x20,0,0},reinterpret_cast<void*>(&BeforeLockAdapter),0},
        {0x9612f7,5,{0xe8,0x54,0x01,0,0},reinterpret_cast<void*>(&PcmAdapter),0}};
    for(const auto& patch:patches)if(memcmp(reinterpret_cast<void*>(patch.address),patch.expected,patch.size)){
        Log("Cinematic PCM signature mismatch at %08lx",patch.address);return;
    }
    unsigned writable=0;
    for(auto& patch:patches){if(!VirtualProtect(reinterpret_cast<void*>(patch.address),patch.size,PAGE_EXECUTE_READWRITE,&patch.protection))break;++writable;}
    if(writable!=2){
        for(int i=static_cast<int>(writable)-1;i>=0;--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[i].address),patches[i].size,patches[i].protection,&ignored);}
        Log("Cinematic PCM page access failed");return;
    }
    bool installed=true;
    for(auto& patch:patches){
        const DWORD relative=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(patch.replacement)-patch.address-5);
        if(patch.size==6){
            //9612B1's six bytes lie wholly inside aligned block9612B0..B7.
            // Keep both neighbor bytes and commit the entire instruction atomically.
            auto* block=reinterpret_cast<volatile LONG64*>(patch.address-1);
            LONG64 original=0,desired=0;
            memcpy(&original,reinterpret_cast<const void*>(patch.address-1),sizeof(original));desired=original;
            auto* bytes=reinterpret_cast<unsigned char*>(&desired);bytes[1]=0xe8;
            memcpy(bytes+2,&relative,sizeof(relative));bytes[6]=0x90;
            if(InterlockedCompareExchange64(block,desired,original)!=original){installed=false;break;}
        }else{
            // Existing E8 remains unchanged; its rel32 operand is naturally aligned.
            InterlockedExchange(reinterpret_cast<volatile LONG*>(patch.address+1),static_cast<LONG>(relative));
        }
        FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(patch.address),patch.size);
    }
    for(int i=1;i>=0;--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[i].address),patches[i].size,patches[i].protection,&ignored);}
    if(!installed){Log("Cinematic PCM concurrent patch conflict; installation stopped");return;}
    Log("Cinematic FF final PCM silence installed; native queued-ring clear, decode and audio clock preserved");
}
