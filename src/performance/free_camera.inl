// Own ordinary-on-foot orbit implementation; native addresses require the exact r29 image.
#include "phone_camera_policy.h"
#include "native_camera_ray.h"
#include <cmath>
#include <algorithm>
#include "safe_facing.inl"
static bool freeThirdPerson=false;
static bool orbitSettingsValid=true;
namespace fixed_camera {static bool Active();}
namespace vehicle_camera {static unsigned char* Select(unsigned char*,unsigned char*);}
static bool companionFollowNormally=false;
static float companionCatchUpDistance=25.0f;
static float orbitGainX=0.0015f,orbitGainY=0.0012f;
static bool orbitInvertY=false;
static bool orbitMeleeAim=false,orbitMeleeLocked=false;
static void(*orbitMeleePose)(unsigned char*,const float*)=nullptr;
static bool orbitPistolAim=false; // Set by the guarded third-person pistol input bridge.
static bool orbitHandManipulation=false; // Updated by the guarded combat input bridge.
static volatile LONG orbitRawX=0,orbitRawY=0;
static DWORD orbitLastMouse=0;
struct OrbitState {
    unsigned char* component{};
    unsigned char* frame{};
    unsigned char* player{};
    float yaw{},pitch{},radius{},distance{},aimBlend{};
    float pivot[4]{};
    float root[3]{};
    DWORD tick{},lastLog{};
    bool active{};
};
static OrbitState orbit;
static unsigned orbitContextReason=0,orbitLastDenial=~0u;
static DWORD orbitLastDenialLog=0;
static bool orbitHistoryUsable=false;
static bool orbitLandingContinuity=false;
static bool orbitStopContinuity=false; // Already-owned ordinary walk/run -> idle/start.
static float orbitAimOrigin[4]{},orbitAimForward[4]{};
static DWORD orbitAimRayTick=0;
static unsigned char* orbitAimRayComponent=nullptr;
static unsigned char* orbitAimRayPlayer=nullptr;
static bool GetNativeGameplayAimRay(float*,float*);
[[maybe_unused]] static __declspec(noinline) bool GetOrbitThrowAimRay(float* origin,float* direction){
    if(!origin||!direction||!orbit.active||GetTickCount()-orbitAimRayTick>100||
        orbitAimRayComponent!=orbit.component||orbitAimRayPlayer!=orbit.player)return false;
    for(unsigned i=0;i<3;++i){origin[i]=orbitAimOrigin[i];direction[i]=orbitAimForward[i];}
    return true;
}
[[maybe_unused]] static __declspec(noinline) bool GetOrbitPistolAimRay(float* origin,float* direction){
    if(!orbitPistolAim)return false;
    return GetOrbitThrowAimRay(origin,direction)||GetNativeGameplayAimRay(origin,direction);
}

static float orbitNativePivot[4]{};
static unsigned char* orbitPivotFrame=nullptr;
static unsigned char* orbitPivotComponent=nullptr;
static float OrbitRead(const unsigned char* p,unsigned o){return *reinterpret_cast<const float*>(p+o);}
// Native64D4A0 owns the inventory/healing camera from these context clocks.
// The opening operation can precede the first positive clock value, so also
// respect the hero-owned inventory's native mode. Neither camera feature may
// replace that view merely because HMOV still looks like ordinary walking.
static bool OrbitNativeMenu(const unsigned char* hero){
    if(!hero)return true;
    const float inventory=OrbitRead(hero,0x1d88),healing=OrbitRead(hero,0x1e00);
    if(!std::isfinite(inventory)||!std::isfinite(healing)||inventory>0||healing>0)return true;
    const auto* owner=*reinterpret_cast<unsigned char* const*>(hero+0x810);
    return !owner||*reinterpret_cast<unsigned char* const*>(owner+0x754)!=hero||
        *reinterpret_cast<const DWORD*>(owner+0x81c)!=0;
}
//830960 appends24-byte operations;81C is their COUNT, not an inventory-camera
// mode. Actual unequip/equip and the narrow native pickup-to-stock operation
// below may keep an already-owned ordinary orbit alive.
// The coat/healing camera clocks and every other gameplay guard still win.
static bool OrbitEquipmentTransition(const unsigned char* hero){
    if(!hero||*reinterpret_cast<const DWORD*>(hero)!=0xc834c0)return false;
    const float inventory=OrbitRead(hero,0x1d88),healing=OrbitRead(hero,0x1e00);
    if(!std::isfinite(inventory)||!std::isfinite(healing)||inventory>0||healing>0)return false;
    const auto* owner=*reinterpret_cast<unsigned char* const*>(hero+0x810);
    if(!owner||*reinterpret_cast<unsigned char* const*>(owner+0x754)!=hero)return false;
    const DWORD queue=*reinterpret_cast<const DWORD*>(owner+0x81c);
    if(!queue||queue>7)return false;
    for(DWORD n=0;n<queue;++n){
        const auto* operation=owner+0x75c+n*24;
        const DWORD action=*reinterpret_cast<const DWORD*>(operation),slot=*reinterpret_cast<const DWORD*>(operation+8);
        if(action<=1&&slot<=2)continue;
        // Native830B50 case2 stores a picked world item through831AE0, then
        //831D80. It is not the coat UI (cases3/4). The actual pickup capture
        // used exactly op2/item/-1/flags0/metadata(-1,0), briefly dropping the
        // orbit and reinitializing yaw by0.45rad. Retain only prior ownership;
        // full OrbitContext still checks script/input/top/component identity.
        const auto* camera=*reinterpret_cast<unsigned char* const*>(0xfd5d98);
        const bool pickupStore=action==2&&slot==~0u&&
            *reinterpret_cast<const DWORD*>(operation+4)!=0&&*reinterpret_cast<const DWORD*>(operation+12)==0&&
            *reinterpret_cast<const DWORD*>(operation+16)==~0u&&*reinterpret_cast<const DWORD*>(operation+20)==0&&
            (orbit.active||orbitHistoryUsable)&&orbit.player==hero&&orbit.component&&camera&&
            *reinterpret_cast<const DWORD*>(camera)==0xc67670&&
            *reinterpret_cast<unsigned char* const*>(camera+0x25c)==orbit.component&&
            *reinterpret_cast<const DWORD*>(orbit.component)==0xc68228&&
            *reinterpret_cast<unsigned char* const*>(orbit.component+0x20)==camera&&
            *reinterpret_cast<const unsigned char* const*>(camera+0x3e8)==hero;
        if(!pickupStore)return false;
    }
    return true;
}
static bool GetNativeGameplayAimRay(float* origin,float* direction){
    if(!origin||!direction||!freeThirdPerson||!orbitPistolAim)return false;
    __try {
        const auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!hero||*reinterpret_cast<const DWORD*>(hero)!=0xc834c0||OrbitNativeMenu(hero)||
            (*reinterpret_cast<const DWORD*>(hero+0x968)&1)||hero[0xc19]||
            *reinterpret_cast<DWORD*>(0xff4290)||*reinterpret_cast<BYTE*>(0xff4b7f)||
            (*reinterpret_cast<DWORD*>(0xfd8bd4)&15u)||(*reinterpret_cast<DWORD*>(0xfda5c8)&0x38u)!=8u)return false;
        const auto* controller=*reinterpret_cast<unsigned char* const*>(hero+0x95c);
        const auto* cinematic=*reinterpret_cast<unsigned char* const*>(hero+0x280);
        if(!controller||(*reinterpret_cast<const DWORD*>(controller+4)&6u)!=6u||(cinematic&&cinematic[0x204]))return false;
        const auto* world=*reinterpret_cast<unsigned char**>(0xff7164);
        const auto* camera=world?*reinterpret_cast<unsigned char* const*>(world+0x60):nullptr;
        if(!camera)return false;
        float view[16]{};memcpy(view,camera,sizeof view);
        const DWORD perspective=*reinterpret_cast<const DWORD*>(camera+0xe0);
        if(!native_camera_ray::Center(view,perspective,origin,direction))return false;
        if(*reinterpret_cast<unsigned char**>(0xff7164)!=world||
            *reinterpret_cast<unsigned char* const*>(world+0x60)!=camera||memcmp(view,camera,sizeof view))return false;
        static unsigned reported=0;
        if(reported++<4)Log("Pistol aim uses displayed native gameplay camera; own orbit inactive=%d",!orbit.active);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static float OrbitWrap(float a){return std::remainder(a,6.28318530718f);}
static void OrbitDiscardMouse(){InterlockedExchange(&orbitRawX,0);InterlockedExchange(&orbitRawY,0);}
static void ConfigureFreeCamera(const wchar_t* config,bool supported){
    freeThirdPerson=supported&&ConfigFlag(L"Camera",L"FreeThirdPerson",false,config);
    companionFollowNormally=supported&&ConfigFlag(L"Companions",L"FollowNormally",true,config);
    wchar_t companionValue[64]{};wchar_t* companionEnd=nullptr;
    GetPrivateProfileStringW(L"Companions",L"CatchUpDistance",L"25",companionValue,64,config);
    const double catchUp=wcstod(companionValue,&companionEnd);
    if(companionEnd==companionValue||*companionEnd||!std::isfinite(catchUp)||catchUp<15||catchUp>100)companionFollowNormally=false;
    else companionCatchUpDistance=static_cast<float>(catchUp);
    orbitInvertY=ConfigFlag(L"Camera",L"InvertY",false,config);
    wchar_t value[64]{};wchar_t* end=nullptr;
    GetPrivateProfileStringW(L"Camera",L"SensitivityX",L"1.0",value,64,config);
    const double x=wcstod(value,&end);
    if(end==value||*end||!std::isfinite(x)||x<0.1||x>4.0){freeThirdPerson=false;orbitSettingsValid=false;return;}
    GetPrivateProfileStringW(L"Camera",L"SensitivityY",L"1.0",value,64,config);
    const double y=wcstod(value,&end);
    if(end==value||*end||!std::isfinite(y)||y<0.1||y>4.0){freeThirdPerson=false;orbitSettingsValid=false;return;}
    orbitGainX*=static_cast<float>(x);orbitGainY*=static_cast<float>(y);
}
static bool OrbitMouseControlsHands();
static void CaptureOrbitMouse(const void* data,DWORD bytes){
    if((!freeThirdPerson&&!fixed_camera::Active())||!data||(bytes!=sizeof(DIMOUSESTATE)&&bytes!=sizeof(DIMOUSESTATE2)))return;
    // Menus use the mouse as a pointer, never as world-camera input. Drop
    // accumulated deltas as well so closing a menu cannot replay motion.
    __try {if(*reinterpret_cast<BYTE*>(0xff4b7f)||*reinterpret_cast<DWORD*>(0xff4290)||
        (OrbitNativeMenu(*reinterpret_cast<unsigned char**>(0xfd61ec))&&
         !OrbitEquipmentTransition(*reinterpret_cast<unsigned char**>(0xfd61ec)))){OrbitDiscardMouse();return;}}
    __except(EXCEPTION_EXECUTE_HANDLER){OrbitDiscardMouse();return;}
    if(OrbitMouseControlsHands()){OrbitDiscardMouse();return;}
    const auto* state=static_cast<const DIMOUSESTATE*>(data);
    // Device output is untouched. Consume actual deltas once, never native stick accumulation.
    InterlockedExchangeAdd(&orbitRawX,std::clamp(state->lX,-4096L,4096L));
    InterlockedExchangeAdd(&orbitRawY,std::clamp(state->lY,-4096L,4096L));
    orbitLastMouse=GetTickCount();
}
// Exact HMOV dispatch66D644:11..19 are takeoff/airborne/landing animation gates,
// with671D60 integrating vertical movement, followed by22(fall) and23(landing).
// Kick/death/interaction states stay native;47 is the ordinary landing walk variant.
static __declspec(noinline) bool OrbitJumpState(unsigned state){return (state>=11&&state<=19)||state==22;}
static __declspec(noinline) bool OrbitOrdinaryState(unsigned state){return state<=7||OrbitJumpState(state)||state==23||state==47;}
static __declspec(noinline) bool OrbitHandMovement(DWORD type,unsigned state){
    // Exact HHMC factory682E40/680170, descriptorF56FFC: eight states.
    // Carried characters use this HHMV child instead of HHML/HHMH.
    return ((type==0xc86c08||type==0xc86e78)&&state<=6)||(type==0xc874c0&&state<=7);
}
static __declspec(noinline) bool OrbitBehaviorEligible(DWORD type,unsigned state){
    if(type==0xc857b8)return OrbitOrdinaryState(state); // HMOV
    return OrbitHandMovement(type,state);
}
#include "fixed_camera.inl"
static bool OrbitMouseControlsHands(){
    if(orbitHandManipulation)return true;
    if(!(GetAsyncKeyState('E')&0x8000))return false;
    __try {
        const auto player=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!player||player!=orbit.player||!orbit.active)return false;
        const auto behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
        if(!behavior)return false;
        const DWORD type=*reinterpret_cast<DWORD*>(behavior);
        return OrbitHandMovement(type,*reinterpret_cast<unsigned*>(behavior+0xb0));
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static float OrbitRay(unsigned char*,const float*,const float*,float);
static void OrbitLook(float*,const float*,const float*,const float*);
#include "vehicle_camera.inl"
// Native horizontal target/collision remains immediate. Filter only camera height;
// a normal jump keeps the preceding target altitude rather than following its animation.
static __declspec(noinline) float OrbitStableHeight(float previous,float target,float dt,bool jump){
    if(!std::isfinite(previous)||!std::isfinite(target))return target;
    if(!std::isfinite(dt)||dt<=0)return previous;
    if(jump&&fabsf(target-previous)<=2.5f)return previous;
    const float gain=1.0f-expf(-10.0f*std::min(dt,.1f));
    return previous+(target-previous)*gain;
}
static __declspec(noinline) float OrbitStableOffset(float pivot,float root,float target,float targetRoot,float dt){
    // Follow genuine actor translation immediately; damp only animated/configured offsets.
    return targetRoot+OrbitStableHeight(pivot-root,target-targetRoot,dt,false);
}
static __declspec(noinline) bool OrbitContinuousMotion(DWORD type,unsigned state){
    // PID15772 running-look traverses 4->5; mode2 can persist through 5->4 landing.
    // Explicit cinematic/forced-angle/input/top-camera guards remain in OrbitContext.
    if(type==0xc857b8)return state==4||state==5||OrbitJumpState(state)||state==23||state==47;
    return OrbitHandMovement(type,state);
}
static __declspec(noinline) bool OrbitKeepHistory(unsigned reason){return reason==4||reason==5||reason==10;}
static void LogOrbitDenial(unsigned char* component,DWORD now){
    if(orbitContextReason==orbitLastDenial&&now-orbitLastDenialLog<2000)return;
    orbitLastDenial=orbitContextReason;orbitLastDenialLog=now;
    __try {
        const auto hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        const auto behavior=hero?*reinterpret_cast<unsigned char**>(hero+0x25c):nullptr;
        const auto controller=hero?*reinterpret_cast<unsigned char**>(hero+0x95c):nullptr;
        const auto inventory=hero?*reinterpret_cast<unsigned char**>(hero+0x810):nullptr;
        const auto top=*reinterpret_cast<unsigned char***>(0xfda8f0);
        Log("Orbit blocked reason=%u behavior=%08lx state=%ld camera_mode=%ld camera_flags=%08lx controller=%08lx context=%08lx top=%p mouse_age=%lu hands=%d queue=%lu inventoryTime=%.5f healingTime=%.5f perspective=%08lx",
            orbitContextReason,behavior?*reinterpret_cast<DWORD*>(behavior):0,behavior?*reinterpret_cast<LONG*>(behavior+0xb0):-1,
            component?*reinterpret_cast<LONG*>(component+0xb0):-1,component?*reinterpret_cast<DWORD*>(component+0x438):0,
            controller?*reinterpret_cast<DWORD*>(controller+4):0,*reinterpret_cast<DWORD*>(0xfda5c8),top?*top:nullptr,now-orbitLastMouse,orbitHandManipulation,
            inventory?*reinterpret_cast<DWORD*>(inventory+0x81c):0,hero?OrbitRead(hero,0x1d88):0,hero?OrbitRead(hero,0x1e00):0,hero?*reinterpret_cast<DWORD*>(hero+0x968):0);
    } __except(EXCEPTION_EXECUTE_HANDLER){Log("Orbit blocked reason=%u; incomplete context snapshot",orbitContextReason);}
}
static bool OrbitContext(unsigned char* component,unsigned char** outputPlayer){
    const bool stopWasOwned=orbitStopContinuity;
    orbitStopContinuity=false; // Every failed guard, including exceptions, drops stop ownership.
    orbitContextReason=13;
    if(orbitMeleeLocked)return false; // Native enemy lock owns the camera until its bound action is released.
    orbitContextReason=1;
    if((!freeThirdPerson&&!fixed_camera::Active())||!component||*reinterpret_cast<DWORD*>(component)!=0xc68228)return false;
    const auto camera=*reinterpret_cast<unsigned char**>(0xfd5d98);
    const auto player=*reinterpret_cast<unsigned char**>(0xfd61ec);
    orbitContextReason=2;
    if(!camera||!player||*reinterpret_cast<unsigned char**>(component+0x20)!=camera||
        *reinterpret_cast<DWORD*>(camera)!=0xc67670||*reinterpret_cast<DWORD*>(player)!=0xc834c0||
        *reinterpret_cast<unsigned char**>(camera+0x3e8)!=player)return false;
    orbitContextReason=12;
    const auto cinematic=*reinterpret_cast<unsigned char**>(player+0x280);
    const DWORD contextFlags=*reinterpret_cast<DWORD*>(0xfd8bd4);
    const bool phoneActive=*reinterpret_cast<BYTE*>(0xff4b7f)==1;
    const bool phoneManager=phoneActive&&*reinterpret_cast<DWORD*>(0xff47a0)==0xc79974;
    const auto phoneMovie=phoneManager&&((contextFlags&15u)==8u)?*reinterpret_cast<unsigned char**>(0xff4790):nullptr;
    const bool phoneMovieValid=phoneMovie&&*reinterpret_cast<DWORD*>(phoneMovie+12)==0xff47a0&&phoneMovie[0x65];
    if(!phone_camera_policy::Allow(contextFlags,cinematic&&cinematic[0x204],phoneActive,phoneManager,phoneMovieValid))return false;
    orbitContextReason=3;
    if((*reinterpret_cast<DWORD*>(player+0x968)&1)||player[0xc19])return false;
    if(OrbitNativeMenu(player)&&!OrbitEquipmentTransition(player))return false;
    orbitContextReason=6;
    const auto behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
    if(!behavior)return false;
    const DWORD type=*reinterpret_cast<DWORD*>(behavior);const unsigned state=*reinterpret_cast<unsigned*>(behavior+0xb0);
    if(!OrbitBehaviorEligible(type,state))return false;
    const bool ownedStop=stopWasOwned&&orbit.active&&orbit.component==component&&
        orbit.player==player&&type==0xc857b8&&state<=3;
    // Native equipment presentation temporarily places this SAME camera in
    // its camera stack. Continue only an orbit we already owned, for verified
    // unequip/equip work; all cinematic, menu and forced-angle guards remain.
    const bool ownedEquipment=OrbitEquipmentTransition(player)&&(orbit.active||orbitHistoryUsable)&&
        orbit.component==component&&orbit.player==player;
    const bool continuous=OrbitContinuousMotion(type,state)||ownedStop||ownedEquipment;
    orbitContextReason=4;
    if((*reinterpret_cast<DWORD*>(component+0x438)&1)&&!fixed_camera::Active())return false;
    orbitContextReason=5;
    const bool mode2=*reinterpret_cast<int*>(component+0xb0)==2;
    orbitContextReason=7;
    const auto controller=*reinterpret_cast<unsigned char**>(player+0x95c);
    // Native hero input is owned by player+95C (64B839 / 68FC70).
    // FDA7C4 is context-selected state, not a persistent hero owner;
    // native6B161D clears it, and valid HHML gameplay can leave it null.
    // The exact current hero/camera identity
    // above and native controller enable flags retain the ownership boundary.
    if(!controller||(*reinterpret_cast<DWORD*>(controller+4)&6)!=6)return false;
    orbitContextReason=8;
    if((*reinterpret_cast<DWORD*>(0xfda5c8)&0x38)!=8)return false;
    orbitContextReason=9;
    const auto top=*reinterpret_cast<unsigned char***>(0xfda8f0);
    if(!top||*top!=camera)return false;
    orbitContextReason=10;
    const auto end=*reinterpret_cast<unsigned char**>(0xfda8fc);
    auto node=*reinterpret_cast<unsigned char**>(0xfda8f8);unsigned seen=0;
    while(node&&node!=end){
        if(++seen>32)return false;
        if(*reinterpret_cast<unsigned char**>(node)==camera&&!continuous&&!fixed_camera::Active())return false;
        node=*reinterpret_cast<unsigned char**>(node+0x14);
    }
    orbitContextReason=11;
    DWORD pid=0;const HWND foreground=GetForegroundWindow();GetWindowThreadProcessId(foreground,&pid);
    if(pid!=GetCurrentProcessId()||IsIconic(foreground))return false;
    // Only our already owned landing may continue into this otherwise locked idle.
    // All script, transition, input and active-camera guards above must still pass.
    orbitContextReason=5;
    const bool landingIdle=orbitLandingContinuity&&orbit.active&&orbit.component==component&&
        orbit.player==player&&type==0xc857b8&&state==0;
    if(mode2&&!continuous&&!landingIdle&&!fixed_camera::Active())return false;
    orbitContextReason=0;*outputPlayer=player;orbitStopContinuity=stopWasOwned;return true;
}
// Native46AEA0 can return from its mode-flag0x10 focus path before either
// orbit pose hook executes. Preserve its genuine focus lifecycle, but let an
// already owned, authenticated equipment-only transition use the ordinary path.
static bool __cdecl ShouldSkipEquipmentFocus(unsigned char* component,DWORD original){
    const bool stopBefore=orbitStopContinuity;
    const unsigned reasonBefore=orbitContextReason;
    bool skip=false;
    __try {
        const auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(component&&hero&&OrbitEquipmentTransition(hero)){
            unsigned char* allowedHero=nullptr;
            const bool owned=(orbit.active||orbitHistoryUsable)&&orbit.component==component&&orbit.player==hero;
            if((original&0xffu)&&owned)skip=OrbitContext(component,&allowedHero)&&allowedHero==hero;
            static volatile LONG reports=0;
            if(InterlockedIncrement(&reports)<=48){
                const auto* inventory=*reinterpret_cast<unsigned char* const*>(hero+0x810);
                const DWORD count=*reinterpret_cast<const DWORD*>(inventory+0x81c);
                const auto* first=inventory+0x75c;
                const auto* second=count>1?first+24:nullptr;
                Log("Orbit equipment focus original=%lu skip=%d owned=%d component=%p queue=%lu op0=%lu/%lu op1=%lu/%lu inventoryTime=%.5f healingTime=%.5f orbitTick=%lu aimTick=%lu now=%lu reason=%u",
                    original&0xffu,skip,owned,component,count,*reinterpret_cast<const DWORD*>(first),*reinterpret_cast<const DWORD*>(first+8),
                    second?*reinterpret_cast<const DWORD*>(second):~0u,second?*reinterpret_cast<const DWORD*>(second+8):~0u,
                    OrbitRead(hero,0x1d88),OrbitRead(hero,0x1e00),orbit.tick,orbitAimRayTick,GetTickCount(),orbitContextReason);
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){skip=false;}
    // This is only an admission predicate. PrepareOrbit retains ownership of
    // continuity transitions and of the context reason used by its diagnostics.
    orbitStopContinuity=stopBefore;orbitContextReason=reasonBefore;
    return skip;
}
static void* orbitNativeEquipmentFocus=reinterpret_cast<void*>(0x468ec0);
static __declspec(naked) void OrbitEquipmentFocusAdapter(){
    __asm {
        //468EC0 uses EAX=component and has NO stack arguments (plain RET).
        // EBX is the native46AEA0 component. Call the native predicate ONCE,
        // then preserve its actual post-call GPR/flags/FX state completely.
        call dword ptr [orbitNativeEquipmentFocus]
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        mov eax,[ebp-8]
        and eax,0ffh
        push eax
        push ebx
        call ShouldSkipEquipmentFocus
        add esp,8
        test al,al
        jz orbitKeepEquipmentFocus
        // Only AL is the branch result. Keep native upper EAX bits untouched.
        mov byte ptr [ebp-8],0
    orbitKeepEquipmentFocus:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret
    }
}
static void __cdecl PrepareOrbit(unsigned char* component,unsigned char* frame,unsigned char* fx){
    if(vehicle_camera::Prepare(component,frame,fx)){
        orbit.active=false;orbitHistoryUsable=false;orbitLandingContinuity=false;orbitStopContinuity=false;orbitAimRayTick=0;return;
    }
    const DWORD now=GetTickCount();
    __try {
        unsigned char* player=nullptr;
        if(!OrbitContext(component,&player)){
            orbitLandingContinuity=false;orbitStopContinuity=false;
            if(orbit.active||orbitContextReason!=orbitLastDenial)LogOrbitDenial(component,now);
            orbitHistoryUsable=(orbit.active||orbitHistoryUsable)&&OrbitKeepHistory(orbitContextReason);
            orbit.active=false;
            if(!orbitHistoryUsable)OrbitDiscardMouse();
            return;
        }
        const bool landingContinuity=orbitLandingContinuity;
        const bool stopContinuity=orbitStopContinuity;
        orbitLandingContinuity=false; // Invalid data below must never retain ownership.
        const float bx=OrbitRead(frame,0xe0),by=OrbitRead(frame,0xe4),bz=OrbitRead(frame,0xe8);
        const float baseYaw=atan2f(bx,bz);
        const float basePitch=asinf(std::clamp(by,-1.0f,1.0f));
        if(orbitPivotFrame!=frame||orbitPivotComponent!=component){orbit.active=false;return;}
        // Native obstruction can collapse eye onto target. It is not our orbit radius.
        for(unsigned i=0;i<3;++i)if(!std::isfinite(orbitNativePivot[i])){orbit.active=false;OrbitDiscardMouse();return;}
        const float root[3]={OrbitRead(player,0x220),OrbitRead(player,0x224),OrbitRead(player,0x228)};
        for(float value:root)if(!std::isfinite(value)){orbit.active=false;return;}
        float rootDistance2=0;
        for(unsigned i=0;i<3;++i){const float difference=root[i]-orbit.root[i];rootDistance2+=difference*difference;}
        // An idle native camera need not run this hook every frame. A genuine
        // equipment queue must not mistake that idle age for a scene change.
        const bool queuedContinuity=OrbitEquipmentTransition(player)&&(orbit.active||orbitHistoryUsable)&&
            orbit.component==component&&orbit.player==player&&rootDistance2<=64.0f;
        const bool fresh=(!orbit.active&&!orbitHistoryUsable)||orbit.component!=component||orbit.player!=player||
            (now-orbit.tick>1500&&!queuedContinuity)||rootDistance2>64.0f;
        const auto behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
        const unsigned motionState=*reinterpret_cast<unsigned*>(behavior+0xb0);
        const bool jump=OrbitJumpState(motionState);
        const float previousPivot[3]={orbit.pivot[0],orbit.pivot[1],orbit.pivot[2]};
        if(fresh&&(!std::isfinite(baseYaw)||!std::isfinite(basePitch))){orbit.active=false;OrbitDiscardMouse();return;}
        if(fresh){
            const float nominal=OrbitRead(component,0x43c);
            if(!std::isfinite(nominal)||nominal<.2f||nominal>10){orbit.active=false;return;}
            orbit.yaw=baseYaw;orbit.pitch=basePitch;orbit.radius=std::clamp(nominal,1.5f,6.0f);
            orbit.distance=orbit.radius;orbit.aimBlend=0;OrbitDiscardMouse();
            Log("Orbit begin base_yaw=%.5f pitch=%.5f radius=%.4f component=%p hero=%p history=%d age=%lu queued=%d rootDistance2=%.5f",
                baseYaw,basePitch,orbit.radius,component,player,orbitHistoryUsable,now-orbit.tick,queuedContinuity,rootDistance2);
        }
        const LONG dx=InterlockedExchange(&orbitRawX,0),dy=InterlockedExchange(&orbitRawY,0);
        if(now-orbitLastMouse<100){
            orbit.yaw=OrbitWrap(orbit.yaw-static_cast<float>(std::clamp(dx,-2048L,2048L))*orbitGainX);
            orbit.pitch-=static_cast<float>(std::clamp(dy,-2048L,2048L))*orbitGainY*(orbitInvertY?-1.0f:1.0f);
            orbit.pitch=std::clamp(orbit.pitch,-1.22173048f,1.13446401f);
        }
        float dt=*reinterpret_cast<float*>(0xf503c8);
        if(!std::isfinite(dt)||dt<0||dt>.1f)dt=0;
        orbit.aimBlend=OrbitStableHeight(orbit.aimBlend,orbitPistolAim?1.0f:0.0f,dt,false);
        orbit.component=component;orbit.frame=frame;orbit.player=player;orbit.tick=now;orbit.active=true;orbitHistoryUsable=true;
        for(unsigned i=0;i<4;++i)orbit.pivot[i]=orbitNativePivot[i];
        if(!fresh){
            orbit.pivot[0]=OrbitStableOffset(previousPivot[0],orbit.root[0],orbitNativePivot[0],root[0],dt);
            orbit.pivot[1]=OrbitStableHeight(previousPivot[1],orbitNativePivot[1],dt,jump);
            orbit.pivot[2]=OrbitStableOffset(previousPivot[2],orbit.root[2],orbitNativePivot[2],root[2],dt);
        }
        for(unsigned i=0;i<3;++i)orbit.root[i]=root[i];
        orbitStopContinuity=*reinterpret_cast<DWORD*>(behavior)==0xc857b8&&
            ((motionState>=3&&motionState<=7)||motionState==47||
            (stopContinuity&&!fresh&&motionState<=3));
        orbitLandingContinuity=*reinterpret_cast<DWORD*>(behavior)==0xc857b8&&
            (jump||motionState==23||(landingContinuity&&!fresh&&motionState==0&&
            *reinterpret_cast<int*>(component+0xb0)==2));
        // This mode computes an actual orbit pose later, instead of legacy look-at offsets.
        *reinterpret_cast<float*>(frame+0x5c)=0;
        *reinterpret_cast<float*>(frame+0x10)=0;
        *reinterpret_cast<float*>(fx+160+4*16)=0; // preserved XMM4 scalar used by the native branch
    } __except(EXCEPTION_EXECUTE_HANDLER){orbitLandingContinuity=false;orbitStopContinuity=false;orbit.active=false;OrbitDiscardMouse();}
}
static void* orbitNativeLook=reinterpret_cast<void*>(0x644c10);
static void* orbitNativeRay=reinterpret_cast<void*>(0x46d970);
// Capture the actual target before the engine reuses its stack slot for the eye.
// At entry there are three native arguments and the call return below the frame.
static __declspec(naked) void OrbitCapturePivot(){
    __asm {
        push ecx
        mov orbitPivotComponent,ebx
        lea ecx,[esp+20]
        mov orbitPivotFrame,ecx
        mov ecx,[eax]
        mov dword ptr [orbitNativePivot],ecx
        mov ecx,[eax+4]
        mov dword ptr [orbitNativePivot+4],ecx
        mov ecx,[eax+8]
        mov dword ptr [orbitNativePivot+8],ecx
        mov ecx,[eax+12]
        mov dword ptr [orbitNativePivot+12],ecx
        pop ecx
        jmp dword ptr [orbitNativeLook]
    }
}
static float OrbitRay(unsigned char* component,const float* origin,const float* direction,float distance){
    float result=distance;
    __asm {
        push distance
        push origin
        push component
        mov eax,direction
        call dword ptr [orbitNativeRay]
        movss result,xmm0
    }
    return result;
}
static void OrbitLook(float* matrix,const float* pivot,const float* eye,const float* up){
    __asm {
        push up
        push eye
        push matrix
        mov eax,pivot
        call dword ptr [orbitNativeLook]
        add esp,12
    }
}
static void __cdecl FinishOrbit(unsigned char* component,unsigned char* frame){
    if(vehicle_camera::Finish(component,frame))return;
    if(!orbit.active||orbit.component!=component||orbit.frame!=frame)return;
    const float cp=cosf(orbit.pitch),sp=sinf(orbit.pitch),sy=sinf(orbit.yaw),cy=cosf(orbit.yaw);
    const float forward[4]={sy*cp,sp,cy*cp,0};
    const float right[3]={-cy,0,sy};
    const float up[4]={-sy*sp,cp,-cy*sp,0};
    float pivot[4]={orbit.pivot[0],orbit.pivot[1],orbit.pivot[2],orbit.pivot[3]};
    const float radius=orbit.radius+(std::min(orbit.radius,1.8f)-orbit.radius)*orbit.aimBlend;
    if(orbit.aimBlend>.0001f){
        float shift[4]={right[0]*.45f*orbit.aimBlend,.08f*orbit.aimBlend,right[2]*.45f*orbit.aimBlend,0};
        const float length=sqrtf(shift[0]*shift[0]+shift[1]*shift[1]+shift[2]*shift[2]);
        float direction[4]={shift[0]/length,shift[1]/length,shift[2]/length,0};
        // Sweep the shoulder target itself before tracing the eye, so it cannot begin inside a wall.
        const float query=length+.15f,result=OrbitRay(component,orbit.pivot,direction,query);
        if(!std::isfinite(result)){orbit.active=false;return;}
        const float fraction=result<query-.0001f?std::clamp((result-.10f)/length,0.0f,1.0f):1.0f;
        for(unsigned i=0;i<3;++i)pivot[i]+=shift[i]*fraction;
    }
    float eye[4]={pivot[0]-forward[0]*radius,pivot[1]-forward[1]*radius,pivot[2]-forward[2]*radius,1};
    float safe=radius;
    // Center and four lens corners, using the engine's own player/held-object exclusions.
    const float corners[5][2]={{0,0},{-.18f,-.12f},{.18f,-.12f},{-.18f,.12f},{.18f,.12f}};
    for(const auto& corner:corners){
        float dir[4]{};float length2=0;
        for(unsigned i=0;i<3;++i){dir[i]=eye[i]+corner[0]*right[i]+corner[1]*up[i]-pivot[i];length2+=dir[i]*dir[i];}
        const float length=sqrtf(length2);if(!std::isfinite(length)||length<.001f){orbit.active=false;return;}
        for(unsigned i=0;i<3;++i)dir[i]/=length;
        const float query=length+1.0f;
        const float result=OrbitRay(component,pivot,dir,query);
        if(!std::isfinite(result)){orbit.active=false;return;}
        if(result<query-.0001f){
            const float fraction=std::clamp(result/query,0.0f,1.0f);
            safe=std::min(safe,std::max(0.05f,fraction*radius-.20f));
        }
    }
    float dt=*reinterpret_cast<float*>(0xf503c8);if(!std::isfinite(dt)||dt<0||dt>.1f)dt=0;
    if(safe<orbit.distance)orbit.distance=safe;
    else orbit.distance=std::min(safe,orbit.distance+(safe-orbit.distance)*(1.0f-expf(-7.0f*dt)));
    for(unsigned i=0;i<3;++i)eye[i]=pivot[i]-forward[i]*orbit.distance;
    const float worldUp[4]={0,1,0,0};
    OrbitLook(reinterpret_cast<float*>(frame+0xc0),pivot,eye,worldUp);
    for(unsigned i=0;i<4;++i){orbitAimOrigin[i]=eye[i];orbitAimForward[i]=forward[i];}
    orbitAimRayComponent=component;orbitAimRayPlayer=orbit.player;orbitAimRayTick=GetTickCount();
    if(orbitMeleePose)orbitMeleePose(orbit.player,forward);

}
static void __cdecl RotateOrbitMovement(float* direction,unsigned char* controller,const float* nativeBasis){
    if(!orbit.active||GetTickCount()-orbit.tick>100||!orbit.player||!direction||!nativeBasis)return;
    __try {
        auto* player=orbit.player;
        if(*reinterpret_cast<unsigned char**>(player+0x95c)!=controller)return;
        const auto behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
        if(!behavior)return;
        const DWORD type=*reinterpret_cast<DWORD*>(behavior);
        if(type!=0xc857b8&&!OrbitHandMovement(type,*reinterpret_cast<unsigned*>(behavior+0xb0)))return;
        // Exact41C15F..196 selects hero+1F0 or controller+440. Both feed the
        // same native41C600 vector producer; preserve its chosen basis and input.
        const auto* actorBasis=reinterpret_cast<const float*>(player+0x1f0);
        if(nativeBasis!=actorBasis&&nativeBasis!=reinterpret_cast<const float*>(controller+0x440))return;
        const float ax=actorBasis[0],az=actorBasis[2],bx=nativeBasis[0],bz=nativeBasis[2];
        const float fx=OrbitRead(player,0x210),fz=OrbitRead(player,0x218);
        if(!std::isfinite(ax)||!std::isfinite(az)||!std::isfinite(bx)||!std::isfinite(bz)||
            !std::isfinite(fx)||!std::isfinite(fz)||ax*ax+az*az<.0001f||bx*bx+bz*bz<.0001f||fx*fx+fz*fz<.0001f)return;
        const float actor=atan2f(fx,fz);
        const float basisDelta=OrbitWrap(atan2f(bx,bz)-atan2f(ax,az));
        const float delta=OrbitWrap(orbit.yaw-actor-basisDelta),c=cosf(delta),q=sinf(delta);
        const float x=direction[0],z=direction[2];direction[0]=x*c+z*q;direction[2]=z*c-x*q;
    } __except(EXCEPTION_EXECUTE_HANDLER){orbit.active=false;}
}

static DWORD orbitPreYes=0x46cbcc,orbitPreNo=0x46c7fa,orbitFinalNext=0x46cc9e;
static void* orbitMovementOriginal=reinterpret_cast<void*>(0x41c600);
static void* orbitNativeLocomotion=reinterpret_cast<void*>(0x4b5ea0);
static __declspec(noinline) int OrbitSelectLocomotionState(int requested,float localX,float localZ){
    if(requested<4||requested>7||!std::isfinite(localX)||!std::isfinite(localZ))return requested;
    // Use the native local movement channel, not the camera-rotated world sector.
    // Native670AD7 applies the same lateral limit before backward movement.
    if(localZ<-.25f&&fabsf(localX)<.9f)return 6;
    return requested==6?4:(requested==7?5:requested);
}
static __declspec(noinline) int __cdecl OrbitLocomotionState(unsigned char* behavior,int requested){
    if(requested<4||requested>7)return requested;
    __try {
        unsigned char* player=nullptr;
        if(!OrbitContext(orbitPivotComponent,&player)||
            *reinterpret_cast<unsigned char**>(player+0x25c)!=behavior)return requested;
        const auto controller=*reinterpret_cast<unsigned char**>(player+0x95c);
        // Original6/7's CL=0 preserves body facing and backward animation metadata.
        // Backward sprint uses the supported third-person backward-walk state6.
        return OrbitSelectLocomotionState(requested,OrbitRead(controller,0x1d0),OrbitRead(controller,0x1d8));
    } __except(EXCEPTION_EXECUTE_HANDLER){return requested;}
}
static bool __cdecl CanOrbitBackpedal(unsigned char* player){
    __try {
        unsigned char* current=nullptr;
        if(!OrbitContext(orbitPivotComponent,&current)||current!=player)return false;
        const auto behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
        if(!behavior||*reinterpret_cast<DWORD*>(behavior)!=0xc857b8||*reinterpret_cast<unsigned*>(behavior+0xb0)!=6)return false;
        const auto controller=*reinterpret_cast<unsigned char**>(player+0x95c);
        const float x=OrbitRead(controller,0x1d0),z=OrbitRead(controller,0x1d8);
        const float dx=OrbitRead(player,0x9e0),dz=OrbitRead(player,0x9e8);
        const float norm2=dx*dx+dz*dz;
        return std::isfinite(x)&&std::isfinite(z)&&z<-.25f&&fabsf(x)<.9f&&
            std::isfinite(dx)&&std::isfinite(dz)&&std::isfinite(norm2)&&norm2>.0001f;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void* orbitNativeChannelTurn=reinterpret_cast<void*>(0x66ac20);
// Animation state and physical intent can disagree for several transition frames.
// Facing ownership must cover both native66B000 branches throughout that interval.
static __declspec(noinline) bool __cdecl CanOrbitMovementFacing(unsigned char* player){
    __try {
        unsigned char* current=nullptr;
        if(!OrbitContext(orbitPivotComponent,&current)||current!=player||!orbit.active||
            orbit.player!=player||orbit.component!=orbitPivotComponent||GetTickCount()-orbit.tick>100)return false;
        const auto behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
        if(!behavior||*reinterpret_cast<DWORD*>(behavior)!=0xc857b8)return false;
        const unsigned state=*reinterpret_cast<unsigned*>(behavior+0xb0);
        return (state>=3&&state<=7)||state==47;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Reuse native directional velocity while the scoped selector below keeps the
// authentic third-person animation graph. Global perspective stays unchanged.
static bool __cdecl CanOrbitStrafe(unsigned char* player){
    __try {
        unsigned char* current=nullptr;
        if(!OrbitContext(orbitPivotComponent,&current)||current!=player)return false;
        const auto behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
        if(!behavior||*reinterpret_cast<DWORD*>(behavior)!=0xc857b8)return false;
        const unsigned state=*reinterpret_cast<unsigned*>(behavior+0xb0);
        if(state!=3&&state!=4&&state!=5&&state!=47)return false;
        const auto controller=*reinterpret_cast<unsigned char**>(player+0x95c);
        const float x=OrbitRead(controller,0x1d0),z=OrbitRead(controller,0x1d8);
        const float dx=OrbitRead(player,0x980),dz=OrbitRead(player,0x988),norm2=dx*dx+dz*dz;
        return std::isfinite(x)&&std::isfinite(z)&&fabsf(x)>.25f&&z>=-.25f&&
            std::isfinite(norm2)&&norm2>.0001f;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// 44C is the authentic STD/GUN seven-child third-person locomotion blend.
// 49C/49D select subjective-only assets even when only its velocity path is reused.
// Keep native66A2D0 direction weights and every first-person/script fallback.
static __declspec(noinline) unsigned __cdecl OrbitLateralAnimation(unsigned char* behavior,unsigned requested){
    if(requested!=0x49c&&requested!=0x49d)return requested;
    __try {
        if(!behavior)return requested;
        auto* player=*reinterpret_cast<unsigned char**>(behavior+0x20);
        if(!player||(*reinterpret_cast<DWORD*>(player+0x968)&1)||
            *reinterpret_cast<unsigned char**>(player+0x25c)!=behavior||!CanOrbitStrafe(player))return requested;
        return 0x44c;
    } __except(EXCEPTION_EXECUTE_HANDLER){return requested;}
}
static DWORD orbitLateralAnimationNext=0x66b1af;
static __declspec(naked) void OrbitLateralAnimationAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+4]
        push esi
        call OrbitLateralAnimation
        add esp,8
        mov [ebp+4],eax
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        // This hook is a JMP: original animation arguments are at ESP, not ESP+4.
        mov edx,[esi]
        mov eax,[edx+1dch]
        jmp dword ptr [orbitLateralAnimationNext]
    }
}
static DWORD orbitForwardBranch=0x66b022,orbitDirectionalBranch=0x66b168;
static __declspec(naked) void OrbitDirectionalBranchAdapter(){
    __asm {
        jnz orbitNativeDirectional
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push eax
        call CanOrbitStrafe
        add esp,4
        test al,al
        jz orbitKeepForwardChannel
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [orbitDirectionalBranch]
    orbitKeepForwardChannel:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [orbitForwardBranch]
    orbitNativeDirectional:
        jmp dword ptr [orbitDirectionalBranch]
    }
}
static __declspec(naked) void OrbitStrafeTurnAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+8]
        call CanOrbitMovementFacing
        add esp,4
        test al,al
        jz orbitKeepStrafeTurn
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret 8
    orbitKeepStrafeTurn:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [orbitNativeChannelTurn]
    }
}
static __declspec(noinline) bool OrbitBackpedalDirection(float* velocity,const float* direction){
    const float speed2=velocity[0]*velocity[0]+velocity[2]*velocity[2];
    const float length2=direction[0]*direction[0]+direction[2]*direction[2];
    if(!std::isfinite(speed2)||!std::isfinite(length2)||length2<.0001f)return false;
    const float scale=sqrtf(speed2/length2);
    velocity[0]=direction[0]*scale;velocity[2]=direction[2]*scale;
    return true;
}
static volatile LONG orbitForwardRedirects=0,orbitForwardZeroSpeed=0,orbitImmediateFacingRequests=0;
// Native66AA30 otherwise translates along old actor-forward while571EF0 turns
// toward the requested heading. Own only its exact ordinary-HMOV caller.
static __declspec(noinline) bool OrbitImmediateFace(unsigned char* player,unsigned char* node,const float* direction){
    return safe_facing::Turn(player,node,direction);
}
static __declspec(noinline) bool OrbitFaceCameraMovement(unsigned char* player,unsigned char* node){
    if(orbitPistolAim||orbitThrowFacing||orbitMeleeAim)return false;
    if(!orbit.active||orbit.player!=player||orbit.component!=orbitPivotComponent||
        GetTickCount()-orbit.tick>100||!std::isfinite(orbit.yaw))return false;
    const float heading[4]={sinf(orbit.yaw),0,cosf(orbit.yaw),0};
    return OrbitImmediateFace(player,node,heading);
}
static void OrbitZeroAngularVelocity(unsigned char* node);
// True means this exact owned call also owns/suppresses the later native turn.
static bool __cdecl RewriteOrbitForwardVelocity(float* velocity,unsigned char* node,const float* desired,DWORD caller){
    if(caller!=0x66b153||!velocity||!node||!desired)return false;
    __try {
        unsigned char* player=nullptr;
        if(!OrbitContext(orbitPivotComponent,&player)||!player||*reinterpret_cast<unsigned char**>(player+0x20)!=node)return false;
        auto* behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
        if(!behavior||*reinterpret_cast<DWORD*>(behavior)!=0xc857b8)return false;
        const unsigned state=*reinterpret_cast<unsigned*>(behavior+0xb0);
        if(!CanOrbitMovementFacing(player))return false;
        auto* controller=*reinterpret_cast<unsigned char**>(player+0x95c);
        const float x=OrbitRead(controller,0x1d0),z=OrbitRead(controller,0x1d8);
        if(!std::isfinite(x)||!std::isfinite(z))return false;
        if(x*x+z*z<=.0625f){
            if(!orbitPistolAim&&!orbitThrowFacing&&!orbitMeleeAim)OrbitZeroAngularVelocity(node);
            return true; // Release cannot hand facing back to an old animation branch.
        }
        const float desiredLength2=desired[0]*desired[0]+desired[2]*desired[2];
        if(!std::isfinite(desiredLength2)||desiredLength2<.0001f)return false;
        // Body faces camera-forward while native linear velocity keeps the
        // actual W/A/S/D diagonal. Aim and throw have their own facing owner.
        const float nativeSpeed2=velocity[0]*velocity[0]+velocity[2]*velocity[2];
        if(!std::isfinite(nativeSpeed2))return false;
        if(!orbitPistolAim&&!orbitThrowFacing&&!orbitMeleeAim){
            if(!OrbitFaceCameraMovement(player,node))return false;
            InterlockedIncrement(&orbitImmediateFacingRequests);
        }
        if(nativeSpeed2<.00000001f){
            if(InterlockedIncrement(&orbitForwardZeroSpeed)<=4)Log("Orbit forward native planar speed zero; state=%u input_z=%.4f",state,z);
            return true; // Still finish the one-frame turn; keep native zero linear speed.
        }
        if(OrbitBackpedalDirection(velocity,desired))InterlockedIncrement(&orbitForwardRedirects);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static DWORD orbitForwardVelocityNext=0x66ab41;
static __declspec(naked) void OrbitForwardVelocityAdapter(){
    __asm {
        // Replay before creating our frame: native argument/vector offsets stay exact.
        push eax
        mov ecx,ebx
        movss [esp+40h],xmm0
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        mov eax,[ebp]
        push [eax+4]
        push [eax+10h]
        push ebx
        push [ebp+4]
        call RewriteOrbitForwardVelocity
        add esp,16
        test al,al
        jz orbitKeepNativeForwardTurn
        // Original local S+18: skip66AB94 after our immediate/aim-owned turn.
        mov dword ptr [ebp+20h],0
    orbitKeepNativeForwardTurn:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [orbitForwardVelocityNext]
    }
}
static __declspec(noinline) void OrbitZeroAngularVelocity(unsigned char* node){
    const float zero[4]={0,0,0,0};
    using Setter=void(__thiscall*)(void*,const float*);
    const auto table=*reinterpret_cast<Setter**>(node);
    table[0x2c/4](node,zero);
}
static void __cdecl RewriteOrbitBackpedal(float* velocity,unsigned char* player,unsigned char* node){
    if(!CanOrbitMovementFacing(player))return;
    __try {
        if(*reinterpret_cast<unsigned char**>(player+0x20)!=node)return;
        const auto controller=*reinterpret_cast<unsigned char**>(player+0x95c);
        const float x=OrbitRead(controller,0x1d0),z=OrbitRead(controller,0x1d8);
        if(!std::isfinite(x)||!std::isfinite(z))return;
        const bool intent=x*x+z*z>.0625f;
        if(intent)OrbitBackpedalDirection(velocity,reinterpret_cast<const float*>(player+(CanOrbitBackpedal(player)?0x9e0:0x980)));
        if(!orbitPistolAim&&!orbitThrowFacing&&!orbitMeleeAim&&(!intent||!OrbitFaceCameraMovement(player,node)))OrbitZeroAngularVelocity(node);
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}

// Native66B3F5 receives(player,false), RET8. Keep ordinary facing ownership even
// when input has already changed but the native backward animation is finishing.
static __declspec(naked) void OrbitBackpedalTurnAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+8]
        call CanOrbitMovementFacing
        add esp,4
        test al,al
        jz orbitKeepBackwardTurn
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret 8
    orbitKeepBackwardTurn:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [orbitNativeChannelTurn]
    }
}
// Replace only LEA EDX,[ESP+30];PUSH EDX;CALL EAX at66B4EA. The native
// node setter still receives its original pointer and performs collision/physics.
static __declspec(naked) void OrbitBackpedalVelocityAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ecx
        push esi
        lea eax,[ebp+38h]
        push eax
        call RewriteOrbitBackpedal
        add esp,12
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        lea edx,[esp+34h]
        push edx
        call eax
        ret
    }
}
static __declspec(naked) void OrbitLocomotionAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+8]
        push ecx
        call OrbitLocomotionState
        add esp,8
        cmp eax,[ebp+8]
        je orbitKeepStateArguments
        mov [ebp+8],eax
        // Resolve the new state's own default action metadata, not backward overrides.
        mov dword ptr [ebp+0ch],-1
        mov dword ptr [ebp+10h],-1
        mov dword ptr [ebp+14h],-1
        mov dword ptr [ebp+18h],-1
    orbitKeepStateArguments:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [orbitNativeLocomotion]
    }
}
static __declspec(naked) void OrbitPreAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push esp
        lea eax,[ebp+4]
        push eax
        push ebx
        call PrepareOrbit
        add esp,12
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        comiss xmm5,xmm4
        jbe orbitPreNegative
        jmp dword ptr [orbitPreNo]
    orbitPreNegative:
        jmp dword ptr [orbitPreYes]
    }
}
static __declspec(naked) void OrbitFinalAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        lea eax,[ebp+4]
        push eax
        push ebx
        call FinishOrbit
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov ecx,[ebx+20h]
        mov eax,[ecx]
        jmp dword ptr [orbitFinalNext]
    }
}
static __declspec(naked) void OrbitMovementAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        push edx // Preserve the actual selected native basis across41C600.
        push [ebp+0ch]
        push [ebp+8]
        call dword ptr [orbitMovementOriginal]
        lea esp,[esp+8]
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp-4]
        push ebx
        push esi
        call RotateOrbitMovement
        add esp,12
        fxrstor [esp]
        lea esp,[ebp-40]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret
    }
}
#include "companion_camera.inl"
static volatile LONG orbitDirectKeyboardSamples=0;
static __declspec(noinline) bool OrbitKeyboardBindings(unsigned char* controller){
    const auto* table=*reinterpret_cast<unsigned char**>(0x1213570);
    const unsigned base=*reinterpret_cast<unsigned*>(controller+8);
    if(!table||base>4096)return false;
    for(unsigned action=0;action<4;++action){
        const auto* record=table+(base+action)*60;
        const unsigned count=*reinterpret_cast<const unsigned*>(record+0x14);
        const auto* bindings=*reinterpret_cast<unsigned char* const*>(record+0x18);
        if(!bindings||count<1||count>8)return false;
        for(unsigned b=0;b<count;++b){
            const auto* binding=bindings+b*8;
            const auto* value=*reinterpret_cast<unsigned char* const*>(binding);
            if(!value||*reinterpret_cast<const WORD*>(binding+6)!=0||*reinterpret_cast<const DWORD*>(value)!=0)return false;
            const auto* device=*reinterpret_cast<unsigned char* const*>(value+0x50);
            // Actual native KEYBOARD constructor975B9D and its 84-byte key
            // array (native updater9770EF). Reject analog/mixed device bindings.
            if(!device||*reinterpret_cast<const DWORD*>(device)!=0xcd3d44)return false;
            const auto* keys=*reinterpret_cast<unsigned char* const*>(device+0x44);
            const unsigned index=*reinterpret_cast<const unsigned*>(value+0x2c);
            if(!keys||index>=128||value!=keys+index*84)return false;
        }
    }
    return true;
}
static __declspec(noinline) bool OrbitDirectKeyboard(unsigned char* controller){
    __try {
        unsigned char* player=nullptr;
        if(!controller||!(*reinterpret_cast<DWORD*>(0xfda5c8)&8)||
            !OrbitContext(orbitPivotComponent,&player)||
            *reinterpret_cast<unsigned char**>(player+0x95c)!=controller||!OrbitKeyboardBindings(controller))return false;
        auto* behavior=*reinterpret_cast<unsigned char**>(player+0x25c);
        if(!behavior)return false;
        const DWORD type=*reinterpret_cast<DWORD*>(behavior);
        const unsigned state=*reinterpret_cast<unsigned*>(behavior+0xb0);
        const bool own=(type==0xc857b8&&(state<=7||state==47))||
            OrbitHandMovement(type,state);
        if(own)InterlockedIncrement(&orbitDirectKeyboardSamples);
        return own;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void* orbitNativePolarFilter=reinterpret_cast<void*>(0x41c7e0);
static void* orbitNativeWorldFilter=reinterpret_cast<void*>(0x41ca50);
// Only select the native unfiltered-input branch. It copies the new direction
// AND refreshes its filter history (41F520); physics and animation stay native.
static __declspec(naked) void OrbitKeyboardPolarAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ebx
        call OrbitDirectKeyboard
        add esp,4
        test al,al
        jz orbitKeepPolarFilter
        mov dword ptr [ebp+0ch],0
    orbitKeepPolarFilter:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [orbitNativePolarFilter]
    }
}
static __declspec(naked) void OrbitKeyboardWorldAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ebx
        call OrbitDirectKeyboard
        add esp,4
        test al,al
        jz orbitKeepWorldFilter
        mov dword ptr [ebp+14h],0
    orbitKeepWorldFilter:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [orbitNativeWorldFilter]
    }
}
static volatile LONG orbitInstallAttempt=0;
static void InstallFreeCamera(){
    if((!freeThirdPerson&&!companionFollowNormally&&!fixed_camera::available)||InterlockedCompareExchange(&orbitInstallAttempt,1,0))return;
    struct Patch {DWORD address;unsigned size;unsigned char expected[9];void* replacement;unsigned char opcode;DWORD protection;};
    Patch patches[]={
        {0x46c7f1,9,{0x0f,0x2f,0xec,0x0f,0x86,0xd2,0x03,0,0},reinterpret_cast<void*>(&OrbitPreAdapter),0xe9,0},
        {0x46cc99,5,{0x8b,0x4b,0x20,0x8b,0x01},reinterpret_cast<void*>(&OrbitFinalAdapter),0xe9,0},
        {0x41c1ab,5,{0xe8,0x50,0x04,0,0},reinterpret_cast<void*>(&OrbitMovementAdapter),0xe8,0},
        {0x46c4b2,5,{0xe8,0x59,0x87,0x1d,0},reinterpret_cast<void*>(&OrbitCapturePivot),0xe8,0},
        {0xc85998,4,{0xa0,0x5e,0x4b,0},reinterpret_cast<void*>(&OrbitLocomotionAdapter),0,0},
        {0x66b3f5,5,{0xe8,0x26,0xf8,0xff,0xff},reinterpret_cast<void*>(&OrbitBackpedalTurnAdapter),0xe8,0},
        {0x66b4ea,7,{0x8d,0x54,0x24,0x30,0x52,0xff,0xd0},reinterpret_cast<void*>(&OrbitBackpedalVelocityAdapter),0xe8,0},
        {0x66b01c,6,{0x0f,0x85,0x46,0x01,0,0},reinterpret_cast<void*>(&OrbitDirectionalBranchAdapter),0xe9,0},
        {0x66ab38,9,{0x50,0x8b,0xcb,0xf3,0x0f,0x11,0x44,0x24,0x40},reinterpret_cast<void*>(&OrbitForwardVelocityAdapter),0xe9,0},
        {0x66b1a7,8,{0x8b,0x16,0x8b,0x82,0xdc,0x01,0,0},reinterpret_cast<void*>(&OrbitLateralAnimationAdapter),0xe9,0},
        {0x66b1b8,5,{0xe8,0x63,0xfa,0xff,0xff},reinterpret_cast<void*>(&OrbitStrafeTurnAdapter),0xe8,0},
        {0x41be46,5,{0xe8,0x95,0x09,0,0},reinterpret_cast<void*>(&OrbitKeyboardPolarAdapter),0xe8,0},
        {0x41c25d,5,{0xe8,0xee,0x07,0,0},reinterpret_cast<void*>(&OrbitKeyboardWorldAdapter),0xe8,0},
        {0x46aedb,5,{0xe8,0xe0,0xdf,0xff,0xff},reinterpret_cast<void*>(&OrbitEquipmentFocusAdapter),0xe8,0},
        {0xca8b20,4,{0,0xdb,0x79,0},reinterpret_cast<void*>(&CameraFollowerUpdate),0,0},
        {0x46d047,5,{0xe8,0x44,0xff,0xff,0xff},reinterpret_cast<void*>(&CameraExclusionsAdapter),0xe8,0},
        {0x46d9a8,5,{0xe8,0xe3,0xf5,0xff,0xff},reinterpret_cast<void*>(&CameraExclusionsAdapter),0xe8,0},
        {0x477932,5,{0x50,0x8b,0xcb,0xff,0xd2},reinterpret_cast<void*>(&CompanionPlacementAdapter),0xe8,0},
        {0x47799c,5,{0x50,0x8b,0xcb,0xff,0xd2},reinterpret_cast<void*>(&CompanionPlacementAdapter),0xe8,0},
        {0x4779fb,5,{0x50,0x8b,0xcb,0xff,0xd2},reinterpret_cast<void*>(&CompanionPlacementAdapter),0xe8,0},
        {0x47741e,5,{0x57,0x8b,0xce,0xff,0xd0},reinterpret_cast<void*>(&CompanionFinalPlacementAdapter),0xe8,0},
        {0x79dcc8,6,{0x8b,0x82,0x04,0x01,0,0},reinterpret_cast<void*>(&CompanionRecoverySelector),0xe8,0}};
    const unsigned patchCount=static_cast<unsigned>(sizeof(patches)/sizeof(patches[0]));
    const unsigned first=(freeThirdPerson||fixed_camera::available)?0:14;
    for(unsigned i=first;i<patchCount;++i){const auto& p=patches[i];if(memcmp(reinterpret_cast<void*>(p.address),p.expected,p.size)){Log("Camera/follower signature mismatch at%08lx",p.address);freeThirdPerson=false;fixed_camera::available=false;companionFollowNormally=false;return;}}
    unsigned writable=0;
    for(unsigned i=first;i<patchCount;++i){auto& p=patches[i];if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.size,PAGE_EXECUTE_READWRITE,&p.protection))break;++writable;}
    if(writable!=patchCount-first){
        for(int i=static_cast<int>(first+writable)-1;i>=static_cast<int>(first);--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[i].address),patches[i].size,patches[i].protection,&ignored);}
        freeThirdPerson=false;fixed_camera::available=false;companionFollowNormally=false;Log("Camera/follower page access failed");return;
    }
    for(unsigned i=first;i<patchCount;++i){auto& p=patches[i];
        auto* site=reinterpret_cast<unsigned char*>(p.address);
        if(!p.opcode){*reinterpret_cast<void**>(site)=p.replacement;FlushInstructionCache(GetCurrentProcess(),site,p.size);continue;}
        site[0]=p.opcode;
        *reinterpret_cast<DWORD*>(site+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(p.replacement)-p.address-5);
        memset(site+5,0x90,p.size-5);FlushInstructionCache(GetCurrentProcess(),site,p.size);
    }
    // Restore in reverse order: two hooks can share a native memory page.
    for(int i=static_cast<int>(patchCount)-1;i>=static_cast<int>(first);--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[i].address),patches[i].size,patches[i].protection,&ignored);}
    Log("Camera/follower hooks installed; free_orbit=%d normal_follow=%d catchup_min=%.1fm; native world collision and locomotion",freeThirdPerson,companionFollowNormally,companionCatchUpDistance);
    vehicle_camera::Install();
}
