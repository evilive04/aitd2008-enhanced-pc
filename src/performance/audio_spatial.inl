// Exact r29 native panner; install only after the parent's executable identity
// gate. No speaker configuration, system effects, PCM volume or source writes.
#include "stereo_panner_policy.h"
namespace stereo_audio {
static volatile LONG installation=0;
static unsigned requestedOutputChannels=0;
static bool pannerEnabled=false;
static volatile LONG outputNegotiations=0,observedNativeConfig=0;
static LONG reportedNegotiations=0;
static void* nativeOutputSelectorContinue=reinterpret_cast<void*>(0x960e0d);
static __declspec(naked) void OutputSelectorAdapter(){
    __asm {
        // EAX is the actual GetSpeakerConfig result. Feed the native selector
        // its authentic stereo enum; all format/ring/engine setup stays native.
        mov dword ptr [observedNativeConfig],eax
        lock inc dword ptr [outputNegotiations]
        cmp dword ptr [requestedOutputChannels],2
        jne native_layout
        mov eax,4
    native_layout:
        add eax,-1
        xor edi,edi
        jmp dword ptr [nativeOutputSelectorContinue]
    }
}
static bool InstallOutputSelector(){
    // A caller arriving after native output creation cannot repair that stream
    // by changing the next negotiation. Report the missed startup boundary.
    if(*reinterpret_cast<const DWORD*>(0x120f040)!=0){
        Log("Audio output selector arrived after native output creation; restart with earlier startup initialization required");return false;
    }
    const BYTE expected[8]{0x83,0xc0,0xff,0x33,0xff,0x83,0xf8,0x07};
    if(memcmp(reinterpret_cast<const void*>(0x960e08),expected,8)){
        Log("Audio output selector signature mismatch");return false;
    }
    LONG64 before=0,after=0;memcpy(&before,expected,8);after=before;
    auto* bytes=reinterpret_cast<BYTE*>(&after);bytes[0]=0xe9;
    const DWORD displacement=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&OutputSelectorAdapter)-0x960e08-5);
    memcpy(bytes+1,&displacement,4);
    DWORD protection=0;if(!VirtualProtect(reinterpret_cast<void*>(0x960e08),8,PAGE_EXECUTE_READWRITE,&protection)){
        Log("Audio output selector page access failed");return false;
    }
    const bool installed=InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(0x960e08),after,before)==before;
    if(installed)FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(0x960e08),8);
    DWORD ignored;VirtualProtect(reinterpret_cast<void*>(0x960e08),8,protection,&ignored);
    Log(installed?"Audio output selector installed; requested stereo, native format/ring setup preserved":"Audio output selector concurrent signature conflict");
    return installed;
}
static void* nativePannerContinue=reinterpret_cast<void*>(0x95c3b6);
static __declspec(noinline) const float* __cdecl SelectDirection(const float* source,const void* table,float* copy){
    __try {
        const unsigned channels=*reinterpret_cast<const unsigned*>(static_cast<const unsigned char*>(table)+4);
        const unsigned mask=*reinterpret_cast<const unsigned*>(0x120ef9c);
        if(FoldDirection(source,channels,mask,copy))return copy;
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    return source;
}
static __declspec(naked) void NativePanner(){
    __asm {
        push ebp
        mov ebp,esp
        and esp,0fffffff0h
        jmp dword ptr [nativePannerContinue]
    }
}
static __declspec(naked) void PannerAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        sub esp,16
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        lea eax,[ebp-16]
        push eax
        push edi
        push ecx
        call SelectDirection
        add esp,12
        mov [ebp-28],eax
        fxrstor [esp]
        lea esp,[ebp-52]
        popad
        popfd
        // Copy survives below our frame for the full native call. The original
        // RET12 is consumed here; caller receives its identical stack contract.
        push [ebp+16]
        push [ebp+12]
        push [ebp+8]
        call NativePanner
        mov esp,ebp
        pop ebp
        ret 12
    }
}
}
static void ConfigureStereoSpatialAudio(const wchar_t* path,bool supportedAndEnabled){
    using namespace stereo_audio;
    requestedOutputChannels=0;pannerEnabled=false;if(!supportedAndEnabled)return;
    pannerEnabled=ConfigFlag(L"Audio",L"FixRearStereoVoices",true,path);
    wchar_t text[16]{};unsigned requested=0;
    const DWORD n=GetPrivateProfileStringW(L"Audio",L"OutputChannels",L"0",text,16,path);
    if(n>=15||!ParseOutputChannels(text,requested)){
        Log("Audio.OutputChannels invalid; retaining native output negotiation");return;
    }
    requestedOutputChannels=requested;
}
static bool StereoSpatialAudioRequested(){return stereo_audio::pannerEnabled||stereo_audio::requestedOutputChannels==2;}
static void TickStereoSpatialAudio(){
    using namespace stereo_audio;
    const LONG count=InterlockedCompareExchange(&outputNegotiations,0,0);
    if(count==reportedNegotiations)return;
    static DWORD next=0;const DWORD now=GetTickCount();
    if(next&&static_cast<LONG>(now-next)<0)return;next=now+1000;
    __try {
        const auto* owner=*reinterpret_cast<const unsigned char*const*>(0x120f040);
        const unsigned channels=owner?*reinterpret_cast<const unsigned*>(owner+8):0;
        const unsigned samples=owner?*reinterpret_cast<const unsigned*>(owner+0x20c0):0;
        const unsigned bytes=owner?*reinterpret_cast<const unsigned*>(owner+0x20c4):0;
        const unsigned mask=*reinterpret_cast<const unsigned*>(0x120ef9c);
        const bool ready=channels==requestedOutputChannels&&mask==3&&samples==4096&&bytes==16384&&
            *reinterpret_cast<const DWORD*>(owner+0x20bc)!=0;
        Log("Audio output negotiations=%ld originalConfig=%08lx requestedChannels=%u actualChannels=%u mixerMask=%08x ringSamples=%u ringBytes=%u",count,static_cast<DWORD>(InterlockedCompareExchange(&observedNativeConfig,0,0)),requestedOutputChannels,channels,mask,samples,bytes);
        if(ready)reportedNegotiations=count;
    } __except(EXCEPTION_EXECUTE_HANDLER){Log("Audio output negotiation snapshot unavailable");}
}
static void InstallStereoSpatialAudio(bool supportedAndEnabled){
    using namespace stereo_audio;
    if(!supportedAndEnabled||InterlockedCompareExchange(&installation,1,0))return;
    if(requestedOutputChannels==2)InstallOutputSelector();
    if(!pannerEnabled)return;
    const BYTE expectedBytes[8]{0x55,0x8b,0xec,0x83,0xe4,0xf0,0xf3,0x0f};
    auto* address=reinterpret_cast<volatile LONG64*>(0x95c3b0);
    LONG64 original=0,desired=0;memcpy(&original,expectedBytes,8);desired=original;
    auto* bytes=reinterpret_cast<BYTE*>(&desired);bytes[0]=0xe9;bytes[5]=0x90;
    const DWORD relative=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&PannerAdapter)-0x95c3b0-5);
    memcpy(bytes+1,&relative,4);
    if(memcmp(reinterpret_cast<const void*>(0x95c3b0),expectedBytes,8)){
        Log("Stereo spatial panner signature mismatch");return;
    }
    DWORD protection=0;if(!VirtualProtect(reinterpret_cast<void*>(0x95c3b0),8,PAGE_EXECUTE_READWRITE,&protection)){
        Log("Stereo spatial panner page access failed");return;
    }
    const bool installed=InterlockedCompareExchange64(address,desired,original)==original;
    if(installed)FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(0x95c3b0),8);
    DWORD ignored;VirtualProtect(reinterpret_cast<void*>(0x95c3b0),8,protection,&ignored);
    Log(installed?"Stereo spatial panner installed; rear directions retained, native gains and surround preserved":"Stereo spatial panner concurrent signature conflict");
}
