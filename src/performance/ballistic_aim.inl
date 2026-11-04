// Exact-r29 WPTK shot convergence after native muzzle/spread geometry and
// optional native callback. The native muzzle origin is never moved.
// Include after combat_controls.inl; its native ownership/context helpers are
// deliberately reused rather than inferring a second equipment state.
#include "ballistic_aim_math.h"
namespace ballistic_aim {
static bool enabled=false;
static volatile LONG installation=0,accepted=0,rejected=0,sampleSequence=0;
static DWORD site=0x6fe6bd;
static void* continuation=reinterpret_cast<void*>(0x6fe6c4);
struct Sample {
    DWORD tick{},reason{},hero{},weapon{},behavior{};
    float range{},camera[4]{},forward[4]{},muzzle[4]{},target[4]{},nativeDirection[4]{},direction[4]{};
    float centerHit[4]{},centerOrigin[4]{},centerDirection[4]{};
    DWORD centerHitValid{};
};
static Sample lastSample{};
// Reuse the gun's already constructed native query, including its actor filter,
// reference bookkeeping and bullet collision layers. The original call directly
// after our adapter replaces it with the actual physical muzzle query. This
// preliminary query never executes 6FF600 (damage/shot effects).
static __declspec(naked) void __cdecl NativeSceneQuery(void*,void*,const float*,const float*) {
    __asm {
        push ebp
        mov ebp,esp
        push [ebp+20]
        push [ebp+12]
        push [ebp+8]
        mov eax,[ebp+16]
        mov ecx,6fefb0h
        call ecx
        mov esp,ebp
        pop ebp
        ret
    }
}
static __declspec(noinline) float SceneRange(unsigned char* weapon,const float* eye,const float* forward) {
    auto* query=weapon+0xa80;
    const float maximum=*reinterpret_cast<const float*>(weapon+0x984);
    if(!std::isfinite(maximum)||maximum<=0)return -1;
    NativeSceneQuery(weapon,query,eye,forward);
    if(!query[0x720])return maximum;
    const auto* hit=reinterpret_cast<const float*>(query+0x600);
    float along=0;
    for(unsigned n=0;n<3;++n){
        if(!std::isfinite(hit[n]))return -1;
        along+=(hit[n]-eye[n])*forward[n];
    }
    return std::isfinite(along)&&along>0&&along<=maximum+.001f?along:-1;
}
static void Publish(const Sample& sample) {
    InterlockedIncrement(&sampleSequence);lastSample=sample;InterlockedIncrement(&sampleSequence);
    const LONG count=sample.reason==static_cast<DWORD>(Result::Applied)?InterlockedIncrement(&accepted):InterlockedIncrement(&rejected);
    if(count<=32)Log("Ballistic convergence reason=%lu hero=%08lx weapon=%08lx range=%.5f camera=(%.5f,%.5f,%.5f) muzzle=(%.5f,%.5f,%.5f) target=(%.5f,%.5f,%.5f) dir=(%.6f,%.6f,%.6f)",
        sample.reason,sample.hero,sample.weapon,sample.range,sample.camera[0],sample.camera[1],sample.camera[2],
        sample.muzzle[0],sample.muzzle[1],sample.muzzle[2],sample.target[0],sample.target[1],sample.target[2],
        sample.direction[0],sample.direction[1],sample.direction[2]);
}
static void __cdecl Converge(unsigned char* behavior,float* direction,const float* muzzle) {
    Sample sample{};
    __try {
        using namespace modern_combat;
        if(!ballistic_aim::enabled||!modern_combat::enabled||!freeThirdPerson||!actionRoutingActive||!decision.aim||
            !actionControls.active||GetTickCount()-actionControls.lastTick>100||!behavior||!direction||!muzzle)return;
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!hero||reinterpret_cast<unsigned>(hero)!=actionControls.owner||(Word(hero,0x968)&1)||Word(behavior,0)!=0xc94528||
            (Word(behavior,0xb0)!=0&&Word(behavior,0xb0)!=2))return;
        unsigned char *validatedHero=nullptr,*inventory=nullptr;
        if(!NativeContext(HeroController(),&validatedHero,&inventory)||validatedHero!=hero||
            !inventory||Word(inventory,0x81c)!=0)return;
        auto* hands=Pointer(hero,0xf9c);auto* weapon=hands?Pointer(hands,0x14):nullptr;
        if(!hands||Pointer(hands,0)!=hero||!NativePistolBehavior(weapon)||Pointer(weapon,0x25c)!=behavior||
            Pointer(behavior,0x20)!=weapon||!NativeGunPoseReady(hero))return;
        sample.tick=GetTickCount();sample.hero=reinterpret_cast<DWORD>(hero);sample.weapon=reinterpret_cast<DWORD>(weapon);
        sample.behavior=reinterpret_cast<DWORD>(behavior);
        memcpy(sample.muzzle,muzzle,16);memcpy(sample.nativeDirection,direction,16);
        if(!GetOrbitPistolAimRay(sample.camera,sample.forward)||!UnitForward(sample.forward,sample.forward)||
            !std::isfinite(sample.camera[0])||!std::isfinite(sample.camera[1])||!std::isfinite(sample.camera[2])) {
            sample.reason=static_cast<DWORD>(Result::InvalidRay);Publish(sample);return;
        }
        // Camera collision geometry has a five-centimetre wall clearance in the
        // measured scene. Use the actual bullet query for the reticle target;
        // retaining that clearance causes a visible parallax error at close range.
        sample.range=SceneRange(weapon,sample.camera,sample.forward);
        const auto* centerQuery=weapon+0xa80;
        memcpy(sample.centerHit,centerQuery+0x600,16);
        memcpy(sample.centerOrigin,centerQuery+0x10,16);
        memcpy(sample.centerDirection,centerQuery+0x20,16);
        sample.centerHitValid=centerQuery[0x720];
        sample.reason=static_cast<DWORD>(Convergence(sample.camera,sample.forward,sample.muzzle,sample.range,sample.target,sample.direction));
        if(sample.reason==static_cast<DWORD>(Result::Applied)) {
            memcpy(direction,sample.direction,16);
            memcpy(weapon+0xa60,sample.direction,16);
        }
        Publish(sample);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        sample.tick=GetTickCount();sample.reason=static_cast<DWORD>(Result::Exception);Publish(sample);
    }
}
static __declspec(naked) void Adapter() {
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        lea eax,[ebp+24h]
        push eax
        lea eax,[ebp+14h]
        push eax
        push edi
        call Converge
        add esp,12
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        // Replay both complete native instructions with original ESP/EDI.
        mov eax,[edi+20h]
        lea ecx,[esp+10h]
        jmp dword ptr [continuation]
    }
}
}
static void ConfigureBallisticAim(const wchar_t* config,bool supported) {
    ballistic_aim::enabled=supported&&ConfigFlag(L"Controls",L"ModernCombat",true,config)&&
        ConfigFlag(L"Controls",L"CorrectPistolConvergence",true,config);
}
static void InstallBallisticAim() {
    using namespace ballistic_aim;
    if(!ballistic_aim::enabled||InterlockedCompareExchange(&installation,1,0))return;
    const BYTE signature[]={0x8b,0x47,0x20,0x8d,0x4c,0x24,0x10,0x51,0x8d,0x90,0x80,0x0a,0,0,0x52,0x50,0x8d,0x44,0x24,0x2c,0xe8};
    auto* address=reinterpret_cast<BYTE*>(site);
    if(memcmp(address,signature,sizeof(signature))) {
        ballistic_aim::enabled=false;Log("Ballistic convergence signature mismatch; native trajectory retained");return;
    }
    DWORD old=0;
    if(!VirtualProtect(address,7,PAGE_EXECUTE_READWRITE,&old)){ballistic_aim::enabled=false;return;}
    const DWORD offset=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&Adapter)-reinterpret_cast<ULONG_PTR>(address+5));
    memcpy(address+1,&offset,4);address[5]=0x90;address[6]=0x90;address[0]=0xe9;
    FlushInstructionCache(GetCurrentProcess(),address,7);DWORD ignored=0;VirtualProtect(address,7,old,&ignored);
    Log("Ballistic convergence installed: exact WPTK native muzzle retained, scene-center target, native shot preparation/hit path");
}
