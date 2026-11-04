// Own exact-build story-cinematic fast-forward. No global timer/rate changes.
#include "fast_forward_state.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace cinematic_ff {
constexpr DWORD EntityVtable=0xc686b8,TimelineVtable=0xc689e8,PlayerVtable=0xc834c0;
constexpr DWORD CinematicCameraVtable=0xc664b0,PlainEventCameraVtable=0xc66070;
constexpr float MaxStep=1.0f/120.0f;
constexpr unsigned MaxCalls=64;
static bool enabled=false;
static bool eventsEnabled=false;
static float maximumSpeed=20.0f;
static void* nativeUpdate=reinterpret_cast<void*>(0x470ab0);
static void* nativeTimeline=reinterpret_cast<void*>(0x91d2e0);
static volatile LONG consumedSpace=0,publishedActor=0;
static volatile LONG stateSequence=0,stateFlags=0,stateRate=65536,stateTick=0;
static volatile LONG installAttempt=0;
#ifdef AITD_CINEMATIC_TEST
static bool testForeground=true,testSpace=true;
#endif

static DWORD Word(const void* object,unsigned offset){return *reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(object)+offset);}
static float Number(const void* object,unsigned offset){return *reinterpret_cast<const float*>(static_cast<const unsigned char*>(object)+offset);}
static void Number(void* object,unsigned offset,float value){*reinterpret_cast<float*>(static_cast<unsigned char*>(object)+offset)=value;}
static bool SpaceHeld(){
#ifdef AITD_CINEMATIC_TEST
    return testSpace;
#else
    return (GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
#endif
}
static bool Foreground(){
#ifdef AITD_CINEMATIC_TEST
    return testForeground;
#else
    DWORD pid=0;const HWND window=GetForegroundWindow();
    if(!window||IsIconic(window))return false;
    GetWindowThreadProcessId(window,&pid);return pid==GetCurrentProcessId();
#endif
}
// A camera-only timeline is not necessarily a cutscene: many run during normal
// movement. Native41B610 returns the dummy action table when controller bit2 is
// clear;41BCE0 also exits before movement updates unless bits2 and4 are both set.
// Require the native action table disabled, an exact top cinematic camera and
// reciprocal active context ownership. Bit4 alone may remain set during
// a noninteractive reveal:41B610 still returns only dummy actions and41BCE0
// still exits before movement. Bit2 alone is NOT sufficient evidence of a
// noninteractive event, because its action table remains live.
// The captured C66070 reveal removes the controller instead; that case is
// considered only after its exact camera class and reciprocal context checks.
static unsigned EventOwnership(void* actor,const unsigned char* player){
    if(!eventsEnabled)return 1;
    const auto heroContext=*reinterpret_cast<unsigned char* const*>(player+0x280);
    if(heroContext&&heroContext[0x204])return 2;
    const auto controller=*reinterpret_cast<unsigned char* const*>(player+0x95c);
    if(controller&&(Word(controller,4)&2)!=0)return 3;
    if(*reinterpret_cast<DWORD*>(0xff5508)!=35||*reinterpret_cast<DWORD*>(0xff4390)||
       *reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<DWORD*>(0xff4290)||
       *reinterpret_cast<BYTE*>(0xff4b7f))return 4;
    const auto top=*reinterpret_cast<unsigned char***>(0xfda8f0);
    const auto camera=top?*top:nullptr;
    if(!camera)return 5;
    const DWORD cameraType=Word(camera,0);
    if(cameraType!=CinematicCameraVtable&&cameraType!=PlainEventCameraVtable)return 5;
    const auto context=*reinterpret_cast<unsigned char* const*>(camera+0x280);
    if(!context||!context[0x204]||*reinterpret_cast<void* const*>(context)!=actor)return 6;
    // Native47A420 binds this active primary context even when the camera is
    // absent from actor+338/+33C: that list only supplies optional context flags.
    // Actual v164 capture/log proved this exact C66070 owner relationship with
    // no controller and no matching participant. Keep the old family stricter.
    if(cameraType==PlainEventCameraVtable)return 0;
    if(!controller)return 3;
    const DWORD count=Word(actor,0x338);
    const auto participants=*reinterpret_cast<unsigned char* const*>(static_cast<unsigned char*>(actor)+0x33c);
    if(!participants||!count||count>256)return 7;
    for(DWORD i=0;i<count;++i)
        if(*reinterpret_cast<unsigned char* const*>(participants+i*0x20)==camera){
            return 0;
        }
    return 8;
}
static void LogEventOwnership(void* actor,unsigned reason,const unsigned char* player){
    if(!eventsEnabled||!SpaceHeld())return;
    static DWORD previousTick=0;
    const DWORD now=GetTickCount();
    // Multiple ambient timelines may be evaluated between two camera updates.
    // Bound all diagnostic attempts together rather than per actor/reason.
    if(previousTick&&now-previousTick<2000)return;
    DWORD inputFlags=~0u,cameraType=0,cameraOwner=0,actorFlags=0,participantCount=0;
    float actorTime=0.0f;
    __try{
        const auto controller=player?*reinterpret_cast<unsigned char* const*>(player+0x95c):nullptr;
        if(controller)inputFlags=Word(controller,4);
        if(actor&&Word(actor,0)==EntityVtable&&Word(actor,0x360)==TimelineVtable){
            actorFlags=Word(actor,0x434);actorTime=Number(actor,0x424);
            participantCount=Word(actor,0x338);
        }
        const auto top=*reinterpret_cast<unsigned char***>(0xfda8f0);
        const auto camera=top?*top:nullptr;
        if(camera){cameraType=Word(camera,0);
            if(cameraType==CinematicCameraVtable||cameraType==PlainEventCameraVtable){const auto context=*reinterpret_cast<unsigned char* const*>(camera+0x280);
                if(context&&context[0x204])cameraOwner=Word(context,0);}}
    }__except(EXCEPTION_EXECUTE_HANDLER){}
    // When the camera identifies its owner, unrelated ambient timelines must
    // not consume the bounded diagnostic slot before that owner is evaluated.
    if(actor&&cameraOwner&&cameraOwner!=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(actor)))return;
    previousTick=now;
    Log("Scripted event fast-forward actor=%p ownership=%u input_flags=%08lx top_camera_type=%08lx camera_owner=%08lx actor_flags=%08lx actor_time=%.4f participants=%lu (0=eligible,2=hero-cinematic,3=input-ownership,4=UI/loading,5=top-camera,6=camera-context,7=participant-data,8=not-camera-participant,9=missing-or-unsupported-story-owner)",actor,reason,inputFlags,cameraType,cameraOwner,actorFlags,actorTime,participantCount);
}
static bool Ownership(void* actor){
    if(!actor||Word(actor,0)!=EntityVtable||Word(actor,0x360)!=TimelineVtable)return false;
    const auto player=*reinterpret_cast<unsigned char**>(0xfd61ec);
    if(!player||Word(player,0)!=PlayerVtable)return false;
    const auto context=*reinterpret_cast<unsigned char**>(player+0x280);
    if(context&&context[0x204]&&*reinterpret_cast<void**>(context)==actor)return true;
    const unsigned reason=EventOwnership(actor,player);
    LogEventOwnership(actor,reason,player);
    return reason==0;
}
static bool Running(void* actor){
    return Ownership(actor)&&(Word(actor,0x434)&0x26)==4;
}
static bool Eligible(void* actor){
    return enabled&&maximumSpeed>1.0f&&Running(actor)&&
        (*reinterpret_cast<DWORD*>(0xfd8bd4)&15)==0&&Foreground()&&SpaceHeld();
}
static bool SafeEligible(void* actor){
    __try{return Eligible(actor);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool RecentApplied(void* actor){
    return InterlockedCompareExchange(&publishedActor,0,0)==static_cast<LONG>(reinterpret_cast<ULONG_PTR>(actor))&&
        (InterlockedCompareExchange(&stateFlags,0,0)&1)!=0&&
        static_cast<DWORD>(GetTickCount()-static_cast<DWORD>(InterlockedCompareExchange(&stateTick,0,0)))<250;
}
static void Publish(void* actor,bool applied,float rate=1.0f){
    // Only the simulation thread writes this snapshot. Unrelated cinematic
    // entities must not erase the current story cinematic's heartbeat.
    if(!applied&&InterlockedCompareExchange(&publishedActor,0,0)!=static_cast<LONG>(reinterpret_cast<ULONG_PTR>(actor)))return;
    InterlockedIncrement(&stateSequence);
    InterlockedExchange(&stateFlags,applied?1:0);
    const float bounded=std::clamp(rate,1.0f,20.0f);
    InterlockedExchange(&stateRate,static_cast<LONG>(bounded*65536.0f+0.5f));
    InterlockedExchange(&stateTick,static_cast<LONG>(GetTickCount()));
    InterlockedExchange(&publishedActor,applied?static_cast<LONG>(reinterpret_cast<ULONG_PTR>(actor)):0);
    InterlockedIncrement(&stateSequence);
}

struct Frame {
    void* actor{};
    Frame* previous{};
    float rate{},delta{},globalTime{},originalTime{},originalOffset{};
    float base{},requested{},last{},applied{};
    DWORD clockFlags{};
    unsigned calls{};
    bool eligible{},globalClock{},enteredBackend{},interrupted{},zeroTickCarry{};
};
static __declspec(thread) Frame* current=nullptr;

static bool Compatible(const Frame& frame){
    return Word(frame.actor,0)==EntityVtable&&Word(frame.actor,0x360)==TimelineVtable&&Number(frame.actor,0x428)==frame.rate&&
        (Word(frame.actor,0x2e0)&0x4800)==frame.clockFlags;
}
static bool KeepZeroTick(const Frame& frame){
    return frame.zeroTickCarry&&RecentApplied(frame.actor)&&Eligible(frame.actor)&&Compatible(frame)&&
        (Word(frame.actor,0x434)&0x800)==0&&Number(frame.actor,0x430)==frame.originalOffset&&
        Number(frame.actor,0x424)==frame.base;
}
static void SetTime(Frame& frame,float time){
    Number(frame.actor,0x424,time);
    if(frame.globalClock)Number(frame.actor,0x430,*reinterpret_cast<float*>(0xf503bc)-time);
}
// Native code clobbers volatile XMM registers. Keep an actual compiler-visible
// call boundary: inlined MSVC inline-asm CALL does not describe those clobbers.
// Otherwise the caller can retain `next` in XMM0 and read a native scratch value.
static __declspec(noinline) bool NativeTimeline(void* timeline,float time,void* target){
    unsigned char result=0;
    __asm {
        push time
        mov ecx,timeline
        call dword ptr [target]
        mov result,al
    }
    return result!=0;
}
static __declspec(noinline) void NativeUpdate(void* actor){
    __asm {
        push actor
        call dword ptr [nativeUpdate]
    }
}

static bool Prepare(Frame& frame){
    if(frame.previous||!Eligible(frame.actor))return false;
    frame.rate=Number(frame.actor,0x428);
    frame.delta=*reinterpret_cast<float*>((Word(frame.actor,0x2e0)&0x4000)?0xf503c0:0xf503c8);
    const float realDelta=*reinterpret_cast<float*>(0xf503c0);
    frame.clockFlags=Word(frame.actor,0x2e0)&0x4800;
    frame.globalClock=(frame.clockFlags&0x800)&&frame.rate==1.0f;
    // Global-clock playback follows F503BC/F503C0, which can advance in ~21ms
    // quanta while the render/manual game delta remains 1/120. Credit the clock
    // actually driving this cinematic; using game delta here under-accelerates.
    if(frame.globalClock)frame.delta=realDelta;
    frame.globalTime=*reinterpret_cast<float*>(0xf503bc);
    frame.originalTime=Number(frame.actor,0x424);frame.originalOffset=Number(frame.actor,0x430);
    const auto resource=*reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(frame.actor)+0x364);
    const float duration=resource?Number(resource,0):0;
    if(!std::isfinite(frame.rate)||frame.rate<=0||frame.rate>4||
        !std::isfinite(frame.delta)||frame.delta<0||(!frame.globalClock&&frame.delta==0)||frame.delta>.1f||
        !std::isfinite(realDelta)||realDelta<0||realDelta>.1f||
        !std::isfinite(frame.globalTime)||!std::isfinite(frame.originalTime)||
        !std::isfinite(frame.originalOffset)||!std::isfinite(duration)||duration<=0)return false;
    // The native explicit-clock override is a suspension, not another rate.
    if(frame.globalClock&&(Word(frame.actor,0x434)&0x800))return false;
    frame.base=frame.globalClock?frame.globalTime-frame.originalOffset:frame.originalTime;
    if(!std::isfinite(frame.base)||frame.base<0||frame.base>=duration)return false;
    if(frame.globalClock&&frame.delta==0){
        // No new credit or heartbeat: bridge only a fresh, already applied
        // owner's quantized clock gap. Initial idle ticks cannot start FF.
        frame.zeroTickCarry=RecentApplied(frame.actor);return false;
    }
    // One ordinary evaluation plus at most 63 small additional evaluations.
    const float extra=std::min({(maximumSpeed-1)*frame.delta*frame.rate,
        MaxStep*static_cast<float>(MaxCalls-1),duration-frame.base});
    if(!std::isfinite(extra)||extra<=.000001f)return false;
    frame.requested=frame.base+extra;frame.last=frame.base;
    if(frame.globalClock)Number(frame.actor,0x430,frame.originalOffset-extra);
    else Number(frame.actor,0x424,frame.originalTime+extra);
    frame.eligible=true;return true;
}
static float RollbackBeforeBackend(Frame& frame,float requested){
    const float extra=frame.requested-frame.base;
    float ordinary=requested;
    if(Word(frame.actor,0)==EntityVtable&&Word(frame.actor,0x360)==TimelineVtable){
        if(frame.globalClock){
            const float creditedOffset=frame.originalOffset-extra;
            if(std::fabs(Number(frame.actor,0x430)-creditedOffset)<.0001f){
                const float creditedTime=*reinterpret_cast<float*>(0xf503bc)-creditedOffset;
                Number(frame.actor,0x430,frame.originalOffset);
                if(std::fabs(requested-creditedTime)<.0001f){
                    ordinary=*reinterpret_cast<float*>(0xf503bc)-frame.originalOffset;
                    Number(frame.actor,0x424,ordinary);
                }
            }
        }else if(std::fabs(requested-frame.requested)<.0001f){
            ordinary=frame.originalTime;Number(frame.actor,0x424,ordinary);
        }
    }
    frame.eligible=false;frame.interrupted=true;return ordinary;
}

// Called only by the exact embedded timeline callsite. Intermediate evaluations
// see the correct actor time, so callbacks never read the final credited time
// while evaluating an earlier event. AL is the sole native return value.
static bool __cdecl Timeline(void* timeline,float requested,void* target){
    Frame* frame=current;
    if(!frame||!frame->eligible||timeline!=static_cast<unsigned char*>(frame->actor)+0x360||target!=nativeTimeline)
        return NativeTimeline(timeline,requested,target);
    frame->enteredBackend=true;
    if(!Compatible(*frame)||!Eligible(frame->actor))
        return NativeTimeline(timeline,RollbackBeforeBackend(*frame,requested),target);
    SetTime(*frame,frame->base);
    bool continuing=NativeTimeline(timeline,frame->base,target);++frame->calls;
    if(!continuing)return false;
    if(!Compatible(*frame)||!Running(frame->actor)||Number(frame->actor,0x424)!=frame->base){frame->interrupted=true;return continuing;}
    while(frame->last<frame->requested&&frame->calls<MaxCalls){
        if(!Eligible(frame->actor)||!Compatible(*frame)){frame->interrupted=true;break;}
        const float next=std::min(frame->requested,frame->last+MaxStep);
        if(!(next>frame->last)){frame->interrupted=true;break;}
        SetTime(*frame,next);
        continuing=NativeTimeline(timeline,next,target);++frame->calls;
        // The actual call has evaluated this interval. A script can end or
        // seek the cinematic during it; never overwrite that script's state.
        frame->last=next;frame->applied=std::max(0.0f,next-frame->base);
        if(!continuing)break;
        if(!Compatible(*frame)||!Running(frame->actor)||Number(frame->actor,0x424)!=next){frame->interrupted=true;break;}
    }
    return continuing;
}

static void __cdecl Update(void* actor){
    Frame frame{};frame.actor=actor;frame.previous=current;current=&frame;
    __try {
        Prepare(frame);
        NativeUpdate(actor);
        if(frame.eligible&&!frame.enteredBackend){
            // No timeline evaluation consumed the credit. Restore only our
            // fields while this is still the same live cinematic.
            if(Compatible(frame)){
                Number(actor,0x424,frame.originalTime);
                if(frame.globalClock)Number(actor,0x430,frame.originalOffset);
            }
            frame.applied=0;
        }
        if(frame.applied>0&&SafeEligible(actor)){
            InterlockedExchange(&consumedSpace,1);
            Publish(actor,true,1.0f+frame.applied/(frame.delta*frame.rate));
        }else if(!KeepZeroTick(frame))Publish(actor,false);
    } __finally {current=frame.previous;}
}
static bool __cdecl SuppressCue(void* actor){
    const Frame* frame=current;
    // The native per-frame cue cleanup remains in place. Suppressed cues are
    // not marked used, so existing voice instances expire through that path.
    return frame&&frame->actor==actor&&(KeepZeroTick(*frame)||
        (frame->eligible&&frame->enteredBackend&&frame->requested>frame->base&&Eligible(actor)&&Compatible(*frame)));
}
static DWORD cueContinuation=0x471638;
static __declspec(naked) void CueAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ecx
        call SuppressCue
        add esp,4
        test al,al
        jz originalCue
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret
    originalCue:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        push ecx
        push ebx
        lea ebx,[ecx+504h]
        jmp dword ptr [cueContinuation]
    }
}

static __declspec(naked) void UpdateAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+8]
        call Update
        add esp,4
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret 4
    }
}
static DWORD timelineContinuation=0x470b62;
static __declspec(naked) void TimelineAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push edx
        push [ebp+4]
        push ecx
        call Timeline
        add esp,12
        mov byte ptr [ebp-8],al
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        // This is a JMP site: consume the original float argument, then
        // replay TEST BL,BL / POP EBX displaced with CALL EDX.
        lea esp,[esp+4]
        test bl,bl
        pop ebx
        jmp dword ptr [timelineContinuation]
    }
}
} // namespace cinematic_ff

static void ConfigureCinematicFastForward(const wchar_t* config,bool supported){
    cinematic_ff::enabled=supported&&ConfigFlag(L"General",L"Enabled",true,config)&&
        ConfigFlag(L"Cutscenes",L"FastForward",true,config);
    cinematic_ff::maximumSpeed=static_cast<float>(std::clamp(GetPrivateProfileIntW(L"Cutscenes",L"MaxSpeed",20,config),1u,20u));
    cinematic_ff::eventsEnabled=cinematic_ff::enabled&&ConfigFlag(L"Cutscenes",L"FastForwardEvents",true,config);
}
static void FilterFastForwardKeyboard(void* state,DWORD bytes){
    if(!state||bytes!=256||!cinematic_ff::enabled)return;
    if(!cinematic_ff::SpaceHeld()){InterlockedExchange(&cinematic_ff::consumedSpace,0);return;}
    bool consume=InterlockedCompareExchange(&cinematic_ff::consumedSpace,0,0)!=0;
    if(!consume){
        __try {
            const auto player=*reinterpret_cast<unsigned char**>(0xfd61ec);
            const auto context=player?*reinterpret_cast<unsigned char**>(player+0x280):nullptr;
            auto actor=context?*reinterpret_cast<void**>(context):nullptr;
            // Reserve the key before the first accelerated update; otherwise
            // the native skip/jump binding can consume the same first press.
            consume=cinematic_ff::SafeEligible(actor);
            if(!consume&&cinematic_ff::eventsEnabled){
                const auto top=*reinterpret_cast<unsigned char***>(0xfda8f0);
                const auto camera=top?*top:nullptr;
                const DWORD cameraType=camera?*reinterpret_cast<DWORD*>(camera):0;
                const auto cameraContext=camera&&(cameraType==cinematic_ff::CinematicCameraVtable||cameraType==cinematic_ff::PlainEventCameraVtable)?
                    *reinterpret_cast<unsigned char**>(camera+0x280):nullptr;
                actor=cameraContext&&cameraContext[0x204]?*reinterpret_cast<void**>(cameraContext):nullptr;
                consume=cinematic_ff::SafeEligible(actor);
                // Also diagnose a rejected physical Space press when no Story
                // update is reached. This remains read-only and never reserves
                // gameplay Space or treats an arbitrary camera as an event.
                if(!actor||cinematic_ff::Word(actor,0)!=cinematic_ff::EntityVtable||
                   cinematic_ff::Word(actor,0x360)!=cinematic_ff::TimelineVtable)
                    cinematic_ff::LogEventOwnership(nullptr,9,player);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER){consume=false;}
    }
    if(consume){InterlockedExchange(&cinematic_ff::consumedSpace,1);static_cast<unsigned char*>(state)[0x39]=0;}
}
extern "C" BOOL WINAPI AITDGetCutsceneFastForwardState(FastForwardStateV1* output,DWORD bytes){
    if(!output||bytes<sizeof(FastForwardStateV1))return FALSE;
    FastForwardStateV1 copy{};copy.size=sizeof(copy);copy.version=1;
    for(unsigned attempt=0;attempt<16;++attempt){
        const LONG before=InterlockedCompareExchange(&cinematic_ff::stateSequence,0,0);
        if(before&1)continue;
        copy.flags=static_cast<DWORD>(InterlockedCompareExchange(&cinematic_ff::stateFlags,0,0));
        copy.rateQ16=static_cast<DWORD>(InterlockedCompareExchange(&cinematic_ff::stateRate,0,0));
        copy.tick=static_cast<DWORD>(InterlockedCompareExchange(&cinematic_ff::stateTick,0,0));
        if(before!=InterlockedCompareExchange(&cinematic_ff::stateSequence,0,0))continue;
        __try{*output=copy;return TRUE;}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}
    }
    return FALSE;
}

// Audio thread reads only the published simulation snapshot. Never dereference
// cinematic/hero objects here: their lifetime belongs to the simulation thread.
static bool CinematicOutputSilenced(){
    if(!cinematic_ff::enabled||!cinematic_ff::SpaceHeld()||!cinematic_ff::Foreground())return false;
    FastForwardStateV1 state{};
    return AITDGetCutsceneFastForwardState(&state,sizeof(state))&&(state.flags&1)!=0&&
        static_cast<DWORD>(GetTickCount()-state.tick)<250;
}
static void InstallCinematicFastForward(){
    if(!cinematic_ff::enabled||InterlockedCompareExchange(&cinematic_ff::installAttempt,1,0))return;
    struct Patch{DWORD address;unsigned size;unsigned char expected[8];void* adapter;unsigned char opcode;DWORD protection;};
    Patch patches[]={
        {0x47051f,5,{0xe8,0x8c,0x05,0,0},reinterpret_cast<void*>(&cinematic_ff::UpdateAdapter),0xe8,0},
        {0x470b5d,5,{0xff,0xd2,0x84,0xdb,0x5b},reinterpret_cast<void*>(&cinematic_ff::TimelineAdapter),0xe9,0},
        {0x471630,8,{0x51,0x53,0x8d,0x99,0x04,0x05,0,0},reinterpret_cast<void*>(&cinematic_ff::CueAdapter),0xe9,0}};
    // The second site is CALL EDX / TEST BL,BL / POP EBX. Its adapter includes
    // these displaced instructions below; its continuation remains native.
    for(const auto& p:patches)if(memcmp(reinterpret_cast<void*>(p.address),p.expected,p.size)){
        cinematic_ff::enabled=false;Log("Cinematic fast-forward signature mismatch at %08lx",p.address);return;
    }
    unsigned writable=0;
    for(auto& p:patches){if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.size,PAGE_EXECUTE_READWRITE,&p.protection))break;++writable;}
    if(writable!=3){for(int i=static_cast<int>(writable)-1;i>=0;--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[i].address),patches[i].size,patches[i].protection,&ignored);}cinematic_ff::enabled=false;return;}
    for(auto& p:patches){auto* site=reinterpret_cast<unsigned char*>(p.address);site[0]=p.opcode;
        *reinterpret_cast<DWORD*>(site+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(p.adapter)-p.address-5);
        memset(site+5,0x90,p.size-5);FlushInstructionCache(GetCurrentProcess(),site,p.size);}
    for(int i=2;i>=0;--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[i].address),patches[i].size,patches[i].protection,&ignored);}
    Log("Story cinematic fast-forward installed; maximum %.1fx, bounded timeline steps; noninteractive camera events=%d",cinematic_ff::maximumSpeed,cinematic_ff::eventsEnabled);
}
