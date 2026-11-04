// Deterministic control policy. Native adapters supply a proven equipment/context
// snapshot before applying decisions; this file never fabricates inventory state.
#include <cmath>
#include "throw_release_policy.h"
#include "interaction_input_policy.h"
#include "target_lock_policy.h"
#include "melee_locomotion_policy.h"
#ifdef AITD_COMBAT_NATIVE
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
#endif
namespace modern_combat {
enum Action : unsigned {
    ObjectHit=61, LockTarget=62, LeftObjectUse=63, RightObjectUse=64,
    ObjectLaunch=65, ObjectHandle=66, HandleUp=67, HandleDown=68,
    HandleLeft=69, HandleRight=70
};
enum class Equipment : unsigned { None, Unarmed, Pistol, PistolThrowable, PistolFlashlight, Melee, CarriedBody, Throwable, Flashlight };
struct Frame {
    unsigned tick{};
    unsigned owner{}; // Identity token, never dereferenced by the policy.
    bool eligible{},focused{},keyboardValid{},mouseValid{};
    bool e{},q{},left{},right{},interactionAvailable{};
    Equipment equipment{Equipment::None};
    bool cancelThrow{};
    bool firstPerson{};
    bool worldThrowable{};
    unsigned item{},itemLow{},itemHigh{};
    bool itemRight{};
    bool sprayLighter{}; // Authenticated opposite HLLI/HLSP hand pair.
};
struct Decision {
    unsigned overrideMask{};
    unsigned downMask{};
    bool aim{},directHands{},consumeE{},consumeQ{};
    bool toggleFlashlight{},toggleFlashlightEquip{};
    void Set(Action action,bool down){
        const unsigned bit=1u<<(static_cast<unsigned>(action)-ObjectHit);
        overrideMask|=bit;
        if(down)downMask|=bit;else downMask&=~bit;
    }
    bool Overrides(Action action)const{return (overrideMask&(1u<<(static_cast<unsigned>(action)-ObjectHit)))!=0;}
    bool Down(Action action)const{return (downMask&(1u<<(static_cast<unsigned>(action)-ObjectHit)))!=0;}
};
struct State {
    unsigned owner{},lastTick{},eStart{},qStart{};
    Equipment equipment{Equipment::None};
    bool active{},previousE{},previousQ{},eHeld{},qHeld{},latchE{},latchQ{};
    unsigned item{},itemLow{},itemHigh{};
};
// Identity tokens are compared only; the native bridge re-resolves an item
// through the current inventory before dereferencing or queuing it.
struct ItemKey {
    unsigned token{},low{},high{};
    bool operator==(const ItemKey& other)const{return token==other.token&&low==other.low&&high==other.high;}
    bool operator!=(const ItemKey& other)const{return !(*this==other);}
    explicit operator bool()const{return token!=0;}
};
struct LampRestore {
    enum Phase : unsigned { Idle, AwaitLamp, Held, AwaitEmpty } phase{Idle};
    unsigned owner{},inventory{},hand{},previousHand{},started{};
    ItemKey lamp{},previous{};
    void Clear(){*this={};}
    void Begin(unsigned hero,unsigned inv,ItemKey light,ItemKey displaced,unsigned slot,unsigned oldSlot,unsigned tick){
        *this={};phase=AwaitLamp;owner=hero;inventory=inv;lamp=light;previous=displaced;
        hand=slot;previousHand=oldSlot;started=tick;
    }
    // Returns true only after our lamp unequip completed with an empty hand.
    bool Observe(unsigned hero,unsigned inv,ItemKey item,unsigned queue,unsigned tick){
        if(phase==Idle)return false;
        if(hero!=owner||inv!=inventory||(phase!=Held&&tick-started>10000)){Clear();return false;}
        if(phase==AwaitLamp){
            if(item==lamp){phase=Held;return false;}
            // The native queue can finish before its hand animation publishes
            // the lamp. Only another item or the bounded timeout cancels it.
            if(item&&item!=previous)Clear();
        }else if(phase==Held){
            if((item&&item!=lamp)||(!item&&queue==0))Clear();
        }else if(phase==AwaitEmpty){
            if(item&&item!=lamp){Clear();return false;}
            return !item&&queue==0;
        }
        return false;
    }
    void Unequip(unsigned tick){phase=AwaitEmpty;started=tick;}
};
static bool WorldToActorPoint(const float* matrix,const float* world,float* local){
    for(unsigned n=0;n<16;++n)if(!std::isfinite(matrix[n]))return false;
    for(unsigned n=0;n<3;++n)if(!std::isfinite(world[n]))return false;
    // Row-vector affine convention, proven by native4B37B0: model bone pose
    // (4B3590) is multiplied by hero+1F0 to obtain a world-space pose. HLTO's
    // native6AC280 consumes the model-space point, so invert that transform.
    const float cross12[3]{matrix[5]*matrix[10]-matrix[6]*matrix[9],
        matrix[6]*matrix[8]-matrix[4]*matrix[10],matrix[4]*matrix[9]-matrix[5]*matrix[8]};
    const float cross20[3]{matrix[9]*matrix[2]-matrix[10]*matrix[1],
        matrix[10]*matrix[0]-matrix[8]*matrix[2],matrix[8]*matrix[1]-matrix[9]*matrix[0]};
    const float cross01[3]{matrix[1]*matrix[6]-matrix[2]*matrix[5],
        matrix[2]*matrix[4]-matrix[0]*matrix[6],matrix[0]*matrix[5]-matrix[1]*matrix[4]};
    const float determinant=matrix[0]*cross12[0]+matrix[1]*cross12[1]+matrix[2]*cross12[2];
    if(!std::isfinite(determinant)||std::fabs(determinant)<.00001f)return false;
    const float delta[3]{world[0]-matrix[12],world[1]-matrix[13],world[2]-matrix[14]};
    float output[3]{};const float* rows[3]{cross12,cross20,cross01};
    for(unsigned n=0;n<3;++n){
        output[n]=(delta[0]*rows[n][0]+delta[1]*rows[n][1]+delta[2]*rows[n][2])/determinant;
        if(!std::isfinite(output[n]))return false;
    }
    for(unsigned n=0;n<3;++n)local[n]=output[n];local[3]=1;
    return true;
}
static constexpr unsigned HoldThreshold=350;
static constexpr unsigned MaximumFrameGap=250;
static Decision Step(State& state,const Frame& frame){
    Decision result{};
    const bool supported=frame.equipment!=Equipment::None;
    const bool valid=frame.eligible&&frame.focused&&frame.keyboardValid&&frame.mouseValid&&frame.owner&&supported;
    if(!valid){
        // A press begun in a menu/another owner must never become a gameplay tap.
        state={};state.latchE=frame.e;state.latchQ=frame.q;
        return result;
    }
    const bool fresh=!state.active||state.owner!=frame.owner||state.equipment!=frame.equipment||
        state.item!=frame.item||state.itemLow!=frame.itemLow||state.itemHigh!=frame.itemHigh||
        frame.tick-state.lastTick>MaximumFrameGap;
    if(fresh){
        const bool latchE=state.latchE||frame.e,latchQ=state.latchQ||frame.q;
        state={};state.active=true;state.owner=frame.owner;state.equipment=frame.equipment;
        state.item=frame.item;state.itemLow=frame.itemLow;state.itemHigh=frame.itemHigh;
        state.latchE=latchE;state.latchQ=latchQ;
    }
    state.lastTick=frame.tick;
    const bool throwable=frame.equipment==Equipment::PistolThrowable||frame.equipment==Equipment::Throwable;
    if(frame.cancelThrow&&(throwable||frame.worldThrowable)){
        state.latchE=frame.e;state.latchQ=frame.q;state.previousE=state.previousQ=false;
        state.eHeld=state.qHeld=false;
    }
    if(!frame.e)state.latchE=false;
    if(!frame.q)state.latchQ=false;
    if(frame.interactionAvailable){state.latchE=frame.e;state.previousE=false;state.eHeld=false;}
    const bool e=frame.e&&!state.latchE&&!frame.interactionAvailable,q=frame.q&&!state.latchQ;
    if(e&&!state.previousE){state.eStart=frame.tick;state.eHeld=false;}
    if(q&&!state.previousQ){state.qStart=frame.tick;state.qHeld=false;}
    if(e&&frame.tick-state.eStart>=HoldThreshold)state.eHeld=true;
    if(q&&!state.qHeld&&frame.tick-state.qStart>=HoldThreshold){
        state.qHeld=true;result.toggleFlashlightEquip=true;
    }
    if(!q&&state.previousQ&&!state.qHeld)result.toggleFlashlight=true;
    const bool pistol=frame.equipment==Equipment::Pistol||frame.equipment==Equipment::PistolThrowable||
        frame.equipment==Equipment::PistolFlashlight;
    const bool carriedBody=frame.equipment==Equipment::CarriedBody;
    const bool melee=frame.equipment==Equipment::Melee;
    const bool heldObject=melee||carriedBody;
    const bool directed=heldObject||frame.equipment==Equipment::PistolFlashlight||frame.equipment==Equipment::Flashlight;
    const bool worldThrow=frame.worldThrowable&&!e&&state.previousE&&!state.eHeld&&frame.tick-state.eStart<HoldThreshold;
    if(carriedBody)result.toggleFlashlight=result.toggleFlashlightEquip=false;
    result.aim=pistol&&frame.right;
    result.directHands=directed&&e;
    result.consumeE=(throwable||directed||frame.equipment==Equipment::Flashlight)&&!frame.interactionAvailable;
    result.consumeQ=true;
    result.Set(ObjectHit,melee&&frame.left);
    // Pistol free aim stays independent. Melee retains its native bound lock action.
    if(!melee)result.Set(LockTarget,false);
    result.Set(LeftObjectUse,false); // Lamp toggle belongs to Q, not E.
    result.Set(RightObjectUse,pistol&&frame.left&&frame.right);
    if(frame.equipment==Equipment::Throwable){
        result.Set(LeftObjectUse,frame.itemRight?(frame.sprayLighter&&frame.left):(frame.sprayLighter?frame.right:frame.left));
        result.Set(RightObjectUse,frame.itemRight?(frame.sprayLighter?frame.right:frame.left):(frame.sprayLighter&&frame.left));
    }
    // Inventory E prepares while held and commits once on release.
    result.Set(ObjectLaunch,(heldObject&&frame.right)||(throwable&&!e&&state.previousE));
    // Native6B9640 reads keyboard66 PRESS and synthesizes event40000,
    // which commits its authored throw. A short E emits that press once on
    // release; a long E retains manual positioning without committing a throw.
    result.Set(ObjectHandle,(result.directHands&&!heldObject)||(heldObject&&frame.right)||(throwable&&e)||worldThrow);
    if(frame.firstPerson){
        // Native first-person gun aiming/shooting retain their own actions.
        // Held objects use the same explicit throw/use/hit inputs as third person.
        result.overrideMask=result.downMask=0;
        result.aim=false;result.directHands=heldObject&&e;
        if(throwable){result.Set(ObjectHandle,e);result.Set(ObjectLaunch,!e&&state.previousE);}
        // Native left-use shares E in first person. Letting it leak here enters
        // mode1 before HLLU can enter throw preparation; only on E release did
        // the queued throw become visible. Keep the right gun input native.
        if(frame.equipment==Equipment::PistolThrowable)result.Set(LeftObjectUse,false);
        if(heldObject){result.Set(ObjectHandle,worldThrow);result.Set(ObjectLaunch,false);}
        if(melee)result.Set(ObjectHit,frame.left);
        if(frame.equipment==Equipment::Throwable){
            result.Set(LeftObjectUse,frame.itemRight?(frame.sprayLighter&&frame.left):(frame.sprayLighter?frame.right:frame.left));
            result.Set(RightObjectUse,frame.itemRight?(frame.sprayLighter?frame.right:frame.left):(frame.sprayLighter&&frame.left));
        }
        if(frame.equipment==Equipment::Flashlight){
            // Q is dispatched through the authenticated lamp toggle below.
            // Native E/primary-use inputs must not toggle a standalone lamp.
            result.Set(LeftObjectUse,false);result.Set(RightObjectUse,false);
            result.Set(ObjectHandle,false);result.Set(ObjectLaunch,false);
        }
    }
    // Axis values remain native when directing the hand; otherwise disable only
    // hand axes. Camera axis routing is handled independently by the orbit code.
    if(!result.directHands&&!frame.firstPerson){for(unsigned n=HandleUp;n<=HandleRight;++n)result.Set(static_cast<Action>(n),false);}
    state.previousE=e;state.previousQ=q;
    if(!e)state.eHeld=false;
    if(!q)state.qHeld=false;
    return result;
}
}

#ifdef AITD_COMBAT_NATIVE
#include "combat_aim_state.h"
namespace modern_combat {
static bool enabled=false,meleeMouseFacing=true;
static unsigned meleeFacingTurns=0;
static unsigned meleeStrafeRequests=0;
static target_lock_policy::State targetLock;
static bool(*externalInputBlocked)()=nullptr;
static void*(*externalActionRoute)(void*,unsigned)=nullptr;
static volatile LONG installAttempt=0;
static State controls{},actionControls{};
static Decision decision{};
static bool actionRoutingActive=false;
static bottle_throw::Queue bottleRelease{};
// Native42CF50/639A60 tracked weak reference, automatically cleared by native
// destruction. Ordinary world melee objects need not exist in inventory stock.
static DWORD displacedReference[4]{};
static void* nativeAssignReference=reinterpret_cast<void*>(0x42cf50);
static unsigned gestureSampleBlocks=0,gestureEventBlocks=0;
static void* nativeGestureSample=reinterpret_cast<void*>(0x581110);
static void* nativeGestureEventContinue=reinterpret_cast<void*>(0x6b96c3);
static void* nativeGestureMatchContinue=reinterpret_cast<void*>(0x6b5e2f);
// Native6B2CA0 uses -strength*x,0,strength*y,0; strength0 is zero.
static float neutralHandSample[4]{};
static ItemKey firingWeapon{},firingHero{};
static bool nativeTriggerHeld=false;
static unsigned nativeFireMessages=0,automaticFirstPersonBlocks=0;
static bool(*firstPersonSightProvider)()=nullptr;
static volatile LONG immediateAimFaces=0;
static volatile float lastImmediateAimDirection[4]{};
struct GunPoseOwnership {
    unsigned char *hero{},*overlay{},*animation{},*descriptor{};
    ItemKey heroIdentity{},weaponIdentity{};
    DWORD layer{},aimA{},aimB{},previousA{},previousB{},requested{};
    bool active{};
};
static GunPoseOwnership gunPose{};
static unsigned nativeGunPoseRequests=0;
static bool NativeGunPoseReady(unsigned char* hero);
static bool EnsureNativeGunPose(unsigned char* hero,unsigned char* weapon);
namespace scoped_lamp_aim { static bool Apply(unsigned char*,unsigned char*); }
namespace scoped_gun_aim { static bool UpdatePitch(unsigned char*,unsigned char*,float); static bool NativeAimBound(unsigned char*,unsigned char*); static bool ApplyGrip(unsigned char*,unsigned char*); }
static void ReleaseNativeGunPose();
static void* nativeCancelHandRequest=reinterpret_cast<void*>(0x6ba3c0);
static void* nativeFirstPersonContinue=reinterpret_cast<void*>(0x6b9889);
static void* nativeFirstPersonEnd=reinterpret_cast<void*>(0x6b9904);
static void* nativeActionAccessor=reinterpret_cast<void*>(0x41b610);
static void* nativeShouldEnterFirstPerson=reinterpret_cast<void*>(0x691060);
struct InputValue {DWORD padding[12]{};float value{};};
struct InputBinding {InputValue* value{};WORD inverted{},kind{};};
struct NativeAction {DWORD operation{},combine{2},flags{};float duration{};DWORD unused{},count{1};InputBinding* bindings{};DWORD remaining[8]{};};
static NativeAction targetLockAction;
static InputValue targetLockValue;
static InputBinding targetLockBinding;
static bool readingOriginalLock=false,targetLockRouting=false,actualTargetLocked=false;
static bool(*targetCapture)(bool,bool)=nullptr;
static_assert(sizeof(NativeAction)==60,"Native action stride");
static InputValue actionValues[10]{};
static InputBinding actionBindings[10]{};
static NativeAction actionRecords[10]{};
static InputValue interactionValue{};
static InputBinding interactionBinding{};
static NativeAction interactionAction{},interactionLeaveAction{};
static InputValue interactionLeaveValue{};
static InputBinding interactionLeaveBinding{};
static interaction_input::State interactionControls{};
static NativeAction blockedInteraction{};
static InputValue blockedInteractionValue{};
static InputBinding blockedInteractionBinding{};
static unsigned interactionTick{},interactionOwner{};
static bool interactionActive=false;
static bool interactionLeaving=false;
static bool interactionVehicle=false;
using VehicleKeyFocusQuery=bool(*)()noexcept;
static VehicleKeyFocusQuery vehicleKeyFocus=nullptr;
static bool interactionVehicleEnterAliases=false;
static void* nativeLampToggle=reinterpret_cast<void*>(0x6fc330);
static void* nativeInventoryQueue=reinterpret_cast<void*>(0x830960);
static void* nativeControllerContinue=reinterpret_cast<void*>(0x41bce6);
static void* nativePoseYes=reinterpret_cast<void*>(0x6a9b68);
static void* nativePoseNo=reinterpret_cast<void*>(0x6a9d76);
static void* nativeApplyPose=reinterpret_cast<void*>(0x605c90);
static void* nativePoseFinished=reinterpret_cast<void*>(0x6a9d76);
static volatile LONG aimPublished=0,aimPublishedTick=0;
static bool NativeContext(void*,unsigned char**,unsigned char**);
static bool WorldInteractionContext(void*,unsigned char**);
static unsigned char* ownedLook=nullptr;
static unsigned char* ownedHero=nullptr;
static float previousAimDirection[4]{},previousAimPoint[4]{},lastAimDirection[4]{},lastAimPoint[4]{};
static bool faceOwned=false;
static unsigned char* Pointer(const void* p,unsigned o){return *reinterpret_cast<unsigned char*const*>(static_cast<const unsigned char*>(p)+o);}
static DWORD Word(const void* p,unsigned o){return *reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(p)+o);}
// Native HHML can leave FDA7C4 null while its hero-owned controller remains
// active. Identity is the exact supported hero and its authoritative +95C link.
static unsigned char* HeroController(){
    auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
    return hero&&Word(hero,0)==0xc834c0?Pointer(hero,0x95c):nullptr;
}
static bool ActorClass(const void* actor,DWORD kind){
    if(!actor)return false;
    const unsigned char* type=Pointer(actor,0x28);
    for(unsigned n=0;type&&n<32;++n,type=Pointer(type,0x1c))if(Word(type,0)==kind)return true;
    return false;
}
static bool IsLamp(const void* actor){return actor&&ActorClass(actor,0x414c4d50)&&
    (Word(actor,0x58c)&0xffffu)==0&&(Word(actor,0x58c)>>16)<=1;}
static bool IsPistol(const void* actor){return actor&&ActorClass(actor,0x414f5750)&&
    (Word(actor,0x58c)&0xffffu)==1&&((Word(actor,0x58c)>>16)==0||(Word(actor,0x58c)>>16)==3);}
static bool HandlerClass(const void* handler,DWORD kind){
    if(!handler)return false;
    const auto* type=Pointer(handler,0x18);
    for(unsigned n=0;type&&n<32;++n,type=Pointer(type,0x10))if(Word(type,4)==kind)return true;
    return false;
}
// Native69AB00 selects HLLI for type4 and HLSP for type14. Factories
// F5A4A8/F5AAC8 construct these exact hand handlers and bind +118.
static bool OwnedLighter(unsigned char* hero,unsigned char* item,unsigned slot){
    if(!hero||!item||slot>1||(Word(item,0x58c)&0xffffu)!=4||Word(item,0x4fc)!=slot)return false;
    auto* hands=Pointer(hero,0xf9c);auto* handler=Pointer(hero,slot?0xf78:0xf7c);
    return hands&&Pointer(hands,0)==hero&&Pointer(hands,4+slot*16)==item&&!Pointer(hands,0x24)&&
        handler&&Word(handler,0)==0xc8c318&&Pointer(handler,0x20)==hero&&
        Pointer(handler,0x118)==item&&HandlerClass(handler,0x484c4c49);
}
static bool SprayLighterInHands(unsigned char* hero,unsigned char* spray){
    if(!hero||!spray||(Word(spray,0x58c)&0xffffu)!=14||!ActorClass(spray,0x414f5350))return false;
    const unsigned slot=Word(spray,0x4fc);if(slot>1)return false;
    auto* hands=Pointer(hero,0xf9c);auto* handler=Pointer(hero,slot?0xf78:0xf7c);
    return hands&&Pointer(hands,0)==hero&&Pointer(hands,4+slot*16)==spray&&
        handler&&Word(handler,0)==0xc8c7c8&&Pointer(handler,0x20)==hero&&Pointer(handler,0x118)==spray&&
        HandlerClass(handler,0x484c5350)&&OwnedLighter(hero,Pointer(hands,4+(1-slot)*16),1-slot);
}
static __declspec(noinline) bool ThrowableInHand(unsigned char* hero,unsigned char* actor,unsigned slot){
    if(!actor||slot>1)return false;
    const auto* handler=Pointer(hero,slot==0?0xf7c:0xf78);
    if(!handler||Pointer(handler,0x20)!=hero||IsLamp(actor)||IsPistol(actor))return false;
    if(HandlerClass(handler,0x4848444c)&&Pointer(handler,0x468)==actor){
        const auto* properties=Pointer(handler,0x458);
        // IsThrowable is a BYTE loaded at4BB45B, consumed by6B6090.
        return properties&&properties[0x138]!=0;
    }
    // Native HLLU owns prepare66/launch65 states10/11 for every inventory
    // throwable: HLBO, HLES (glowstick), HLSP, HLAB and HLBP. _HLA alone does
    // not imply throw capability. Keep the actual hand/handler item identity.
    return HandlerClass(handler,0x484c4c55)&&Pointer(handler,0x118)==actor;
}
static unsigned char* InventoryThrowHandler(unsigned char* hero,unsigned char* item){
    if(!hero||!item)return nullptr;
    const auto* hands=Pointer(hero,0xf9c);if(!hands||Pointer(hands,0)!=hero)return nullptr;
    for(unsigned slot=0;slot<2;++slot){
        auto* handler=Pointer(hero,slot==0?0xf7c:0xf78);
        if(Pointer(hands,4+slot*16)==item&&handler&&Pointer(handler,0x20)==hero&&
           HandlerClass(handler,0x484c4c55)&&Pointer(handler,0x118)==item)return handler;
    }
    return nullptr;
}
static unsigned char* WorldThrowable(unsigned char* hero,unsigned char* hands){
    for(unsigned slot=0;slot<3;++slot){
        auto* item=Pointer(hands,4+slot*16);auto* handler=Pointer(hero,slot==0?0xf7c:0xf78);
        if(!item||!handler||Pointer(handler,0x20)!=hero||!HandlerClass(handler,0x4848444c)||
           Pointer(handler,0x468)!=item)continue;
        auto* properties=Pointer(handler,0x458);
        if(properties&&properties[0x138])return item;
    }
    return nullptr;
}
static __declspec(noinline) bool HeldMelee(unsigned char* hero,unsigned char* hands){
    for(unsigned slot=0;slot<3;++slot){
        auto* actor=Pointer(hands,4+slot*16);
        if(!actor||IsLamp(actor)||IsPistol(actor))continue;
        auto* handler=Pointer(hero,slot==0?0xf7c:0xf78);
        if(HandlerClass(handler,0x4848444c)&&Pointer(handler,0x20)==hero&&Pointer(handler,0x468)==actor)return true;
    }
    return false;
}
#include "aim_range.h"
static __declspec(noinline) float AimRange(unsigned char* component,const float* eye,const float* forward){
    return aim_range::SceneDistance(OrbitRay(component,eye,forward,aim_range::query));
}
static __declspec(noinline) bool FaceDirection(unsigned char* hero,void* node,const float* direction,float step=-1.f){
    return safe_facing::Turn(hero,static_cast<unsigned char*>(node),direction,step);
}
static void RestoreAim(){
    InterlockedExchange(&aimPublished,0);
    if(!ownedLook)return;
    __try {
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(hero==ownedHero&&Pointer(hero,0xf74)==ownedLook&&Pointer(ownedLook,0x20)==hero){
            if(!memcmp(ownedLook+0x230,lastAimDirection,16))memcpy(ownedLook+0x230,previousAimDirection,16);
            if(!memcmp(ownedLook+0x240,lastAimPoint,16))memcpy(ownedLook+0x240,previousAimPoint,16);
            // Stop only the angular command we own, through the same native API.
            if(faceOwned){auto* node=Pointer(hero,0x20);if(node)FaceDirection(hero,node,reinterpret_cast<const float*>(hero+0x210));}
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    ownedLook=ownedHero=nullptr;faceOwned=false;
}
// 0: native decision, 1: legacy native correction, 2: original125E pose complete.
static unsigned __cdecl PrepareAimPose(unsigned char* look){
    __try {
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!enabled||!actionRoutingActive||!decision.aim||!hero||reinterpret_cast<unsigned>(hero)!=actionControls.owner||
            GetTickCount()-actionControls.lastTick>100||(Word(hero,0x968)&1)||Pointer(hero,0xf74)!=look||
            !look||Pointer(look,0x20)!=hero||Word(look,0xb0)!=10)return false;
        unsigned char *validatedHero=nullptr,*inventory=nullptr;
        if(!NativeContext(HeroController(),&validatedHero,&inventory)||validatedHero!=hero||Word(inventory,0x81c)!=0)return false;
        auto* hands=Pointer(hero,0xf9c);
        if(!hands||!IsPistol(Pointer(hands,0x14)))return false;
        const auto* animation=Pointer(hero,0x3b4);
        if(Word(look,0x4a0)>1||!animation||!Word(animation,0x18)||!Word(animation,0x1c)||
            !Pointer(animation,0x10)||!*reinterpret_cast<const WORD*>(Pointer(animation,0x10)))return false;
        // Animation buffers are phase-owned. Acquire here as well as the input
        // phase; do not require ephemeral pose matrices to exist during Tick.
        if(!EnsureNativeGunPose(hero,Pointer(hands,0x14)))return false;
        // The lamp must follow the first authenticated aim pose. Waiting for
        // the reticle/fire readiness delay leaves its native side-facing beam
        // visible during that delay. Its independent item/rig/IK guards remain.
        scoped_lamp_aim::Apply(hero,Pointer(hero,0x3b4));
        if(!NativeGunPoseReady(hero))return false;
        float eye[4]{0,0,0,1},forward[4]{};
        if(!GetOrbitPistolAimRay(eye,forward))return false;
        for(unsigned n=0;n<3;++n)if(!std::isfinite(eye[n])||!std::isfinite(forward[n]))return false;
        const float norm=forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2];
        if(norm<.5f||norm>1.5f)return false;
        if(gunPose.aimA==0x125e&&!scoped_gun_aim::UpdatePitch(hero,Pointer(hero,0x3b4),
            atan2f(forward[1],sqrtf(forward[0]*forward[0]+forward[2]*forward[2]))))return false;
        const float scale=1.0f/sqrtf(norm);for(unsigned n=0;n<3;++n)forward[n]*=scale;
        float distance=AimRange(orbit.component,eye,forward);
        if(!std::isfinite(distance)||distance<=0)return false;
        distance=std::clamp(distance,1.0f,100.0f);
        float target[4]{0,0,0,1},planar[4]{};
        for(unsigned n=0;n<3;++n){target[n]=eye[n]+forward[n]*distance;planar[n]=target[n]-*reinterpret_cast<const float*>(hero+0x220+4*n);}
        planar[1]=0;const float planarLength=sqrtf(planar[0]*planar[0]+planar[2]*planar[2]);
        if(!std::isfinite(planarLength)||planarLength<.1f)return false;
        planar[0]/=planarLength;planar[2]/=planarLength;
        float localTarget[4]{};
        if(!WorldToActorPoint(reinterpret_cast<const float*>(hero+0x1f0),target,localTarget))return false;
        if(ownedLook!=look||ownedHero!=hero){RestoreAim();ownedLook=look;ownedHero=hero;memcpy(previousAimDirection,look+0x230,16);memcpy(previousAimPoint,look+0x240,16);}
        memcpy(lastAimDirection,forward,16);memcpy(lastAimPoint,localTarget,16);
        memcpy(look+0x230,forward,16);memcpy(look+0x240,localTarget,16);
        // Authenticated original125E clips already supply authored body/grip.
        // Do not stack the forced first-person trunk correction over that pose.
        const bool nativePose=scoped_gun_aim::NativeAimBound(hero,Pointer(hero,0x3b4));
        if(!nativePose)return false;
        scoped_gun_aim::ApplyGrip(hero,Pointer(hero,0x3b4));
        if(auto* node=Pointer(hero,0x20);node&&FaceDirection(hero,node,planar)){faceOwned=true;InterlockedIncrement(&immediateAimFaces);for(unsigned i=0;i<4;++i)lastImmediateAimDirection[i]=planar[i];}
        InterlockedExchange(&aimPublishedTick,static_cast<LONG>(GetTickCount()));InterlockedExchange(&aimPublished,1);return 2;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool __cdecl SkipFirstPersonExtras(unsigned char* look,unsigned char* animation){
    __try {
        const bool own=actionRoutingActive&&decision.aim&&ownedLook==look&&ownedHero&&Pointer(ownedHero,0xf74)==look&&
            (Word(ownedHero,0x968)&1)==0&&GetTickCount()-actionControls.lastTick<=100;
        if(own&&NativeGunPoseReady(ownedHero)&&animation&&Pointer(ownedHero,0x3b4)==animation&&Word(animation,0x18)&&Word(animation,0x1c)&&
            Pointer(animation,0x10)&&*reinterpret_cast<const WORD*>(Pointer(animation,0x10))){
            // Require both the owned native gun base-pose request and the fresh
            // camera-oriented upper-body look pass. Render thread reads POD.
            InterlockedExchange(&aimPublishedTick,static_cast<LONG>(GetTickCount()));InterlockedExchange(&aimPublished,1);
        }
        return own;
    }
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static __declspec(naked) void ApplyAimPoseAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        push [ebp+0ch]
        push [ebp+8]
        call dword ptr [nativeApplyPose]
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+8]
        push edi
        call SkipFirstPersonExtras
        add esp,8
        test al,al
        jz keepNativePoseReturn
        mov eax,nativePoseFinished
        mov [ebp+4],eax
    keepNativePoseReturn:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret 8
    }
}
static __declspec(naked) void AimPoseAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push edi
        call PrepareAimPose
        add esp,4
        cmp eax,2
        je aimPosePrivateComplete
        test al,al
        jnz aimPoseOwn
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        test byte ptr [eax+968h],1
        jnz aimPoseContinue
        jmp dword ptr [nativePoseNo]
    aimPoseOwn:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        test byte ptr [eax+968h],1
    aimPoseContinue:
        jmp dword ptr [nativePoseYes]
    aimPosePrivateComplete:
        // Skip6AC930/605C90 too: original125E supplies its complete pose.
        // Actual FP takes the original native branch and matrix solver.
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [nativePoseFinished]
    }
}
static void UpdateActions(const Decision& next,DWORD elapsed){
    for(unsigned n=0;n<10;++n){
        auto& record=actionRecords[n];const bool before=(record.flags&1)!=0;
        const bool down=(next.downMask&(1u<<n))!=0,changed=before!=down;
        record.flags=(down?3u:0u)|(changed?4u:0u)|((down&&changed)?8u:0u)|((!down&&changed)?16u:0u);
        record.duration=changed?0.0f:record.duration+static_cast<float>(elapsed)*.001f;
        actionValues[n].value=down?1.0f:0.0f;
        actionBindings[n].value=&actionValues[n];record.bindings=&actionBindings[n];record.count=1;record.combine=2;
    }
}
static void* __cdecl RouteAction(void* original,DWORD offset){
    if(!enabled||offset%60)return original;
    __try {
        auto* controller=HeroController();
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!hero||!controller||Pointer(hero,0x95c)!=controller)return original;
        const DWORD index=offset/60,baseIndex=Word(controller,8);
        if(index>=baseIndex&&index-baseIndex==LockTarget){
            if(readingOriginalLock)return original;
            if(targetLockRouting&&targetLock.owner==reinterpret_cast<unsigned>(hero)&&
               GetTickCount()-targetLock.tick<=100)return &targetLockAction;
        }
        if(index>=baseIndex&&externalActionRoute){
            void* routed=externalActionRoute(original,index-baseIndex);if(routed!=original)return routed;
        }
        if(index>=baseIndex&&(index-baseIndex==7||index-baseIndex==8||
            (interactionVehicle&&(index-baseIndex==17||index-baseIndex==26)))){
            unsigned char* verified=nullptr;
            if(interactionActive&&interactionOwner==reinterpret_cast<unsigned>(hero)&&GetTickCount()-interactionTick<=100&&
                WorldInteractionContext(controller,&verified)&&verified==hero)
                return index-baseIndex==7||((index-baseIndex==17||index-baseIndex==26)&&interactionVehicleEnterAliases)?&interactionAction:&interactionLeaveAction;
            return original;
        }
        if(!actionRoutingActive||!actionControls.active||GetTickCount()-actionControls.lastTick>100||
            reinterpret_cast<unsigned>(hero)!=actionControls.owner)return original;
        if(index<baseIndex||index-baseIndex<ObjectHit||index-baseIndex>HandleRight)return original;
        unsigned char *currentHero=nullptr,*inventory=nullptr;
        const bool firstPerson=(Word(hero,0x968)&1)!=0;
        if((!freeThirdPerson&&!firstPerson)||!NativeContext(controller,&currentHero,&inventory)||currentHero!=hero||Word(inventory,0x81c)!=0)return original;
        const auto action=static_cast<Action>(index-baseIndex);
        if(firstPerson&&action!=ObjectHandle&&action!=ObjectLaunch&&
           !(actionControls.equipment==Equipment::Melee&&action==ObjectHit)&&
           !(actionControls.equipment==Equipment::PistolThrowable&&action==LeftObjectUse)&&
           !((actionControls.equipment==Equipment::Throwable||actionControls.equipment==Equipment::Flashlight)&&
             (action==LeftObjectUse||action==RightObjectUse)))return original;
        if(!decision.Overrides(action))return original;
        return &actionRecords[static_cast<unsigned>(action)-ObjectHit];
    } __except(EXCEPTION_EXECUTE_HANDLER){return original;}
}
static __declspec(naked) void ActionReturnAdapter(){
    __asm {
        // Replay MOV ECX,[1213570], and select the original return record.
        mov ecx,dword ptr ds:[01213570h]
        lea eax,[esi+ecx]
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push esi
        push eax
        call RouteAction
        add esp,8
        mov [ebp-8],eax
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        // Original41B610 epilogue: savedESI and one callee-cleaned actionarg.
        pop esi
        ret 4
    }
}
// Native code uses a custom EAX parameter. A separate call boundary prevents
// the compiler retaining XMM scratch values across a native assembly call.
static __declspec(naked) void __cdecl CallLampToggle(void*){
    __asm {
        mov eax,[esp+4]
        jmp dword ptr [nativeLampToggle]
    }
}
static __declspec(noinline) bool CallInventoryQueue(void* inventory,unsigned operation,void* item,unsigned slot,unsigned flags){
    unsigned char accepted=0;
    __asm {
        push flags
        push slot
        push item
        push operation
        mov ecx,inventory
        xor eax,eax
        call dword ptr [nativeInventoryQueue]
        mov accepted,al
    }
    return accepted!=0;
}
static __declspec(noinline) void AssignDisplacedReference(void* actor){
    void* record=displacedReference;
    __asm {
        push record
        mov eax,actor
        call dword ptr [nativeAssignReference]
    }
}
namespace inventory_assist {static void Reset();}
static void ClearLampRestore(){inventory_assist::Reset();}
static __declspec(noinline) bool WorldInteractionContext(void* controller,unsigned char** outputHero){
    if(!enabled||(externalInputBlocked&&externalInputBlocked())||!controller||controller!=HeroController()||
        (Word(controller,4)&6)!=6||(Word(reinterpret_cast<void*>(0xfda5c8),0)&0x38)!=8||
        (Word(reinterpret_cast<void*>(0xfd8bd4),0)&15))return false;
    auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
    if(!hero||Word(hero,0)!=0xc834c0||Pointer(hero,0x95c)!=controller||hero[0xc19])return false;
    const auto* cinematic=Pointer(hero,0x280);
    if(cinematic&&cinematic[0x204])return false;
    // Native64DA00 condition39 DECIMAL checks HHEL; condition0x39 is an
    // unrelated hero+600 condition. Match inventory safety at hero+1E00.
    const float inventory=*reinterpret_cast<const float*>(hero+0x1d64+9*4);
    const float healing=*reinterpret_cast<const float*>(hero+0x1e00);
    if(!std::isfinite(inventory)||!std::isfinite(healing)||inventory>0||healing>0)return false;
    DWORD process=0;HWND window=GetForegroundWindow();GetWindowThreadProcessId(window,&process);
    if(process!=GetCurrentProcessId()||IsIconic(window))return false;
    *outputHero=hero;return true;
}
static void SetInteractionAction(NativeAction& record,InputValue& value,InputBinding& binding,bool down,unsigned elapsed){
    const bool before=(record.flags&1)!=0,changed=down!=before;
    record.flags=(down?3u:0u)|(changed?4u:0u)|((down&&changed)?8u:0u)|((!down&&changed)?16u:0u);
    record.duration=changed?0.f:record.duration+std::min<unsigned>(elapsed,250)*.001f;
    value.value=down?1.f:0.f;binding.value=&value;record.bindings=&binding;record.count=1;record.combine=2;
}
static __declspec(noinline) bool CabinetInteractionContext(unsigned char* hero){
    const auto* handler=Pointer(hero,0x25c);
    if(!handler||Pointer(handler,0x20)!=hero)return false;
    // HDIG retains its real ADIG target after the initial entry selector changes.
    if(Word(handler,0)==0xc83d10&&HandlerClass(handler,0x48444947))
        return ActorClass(Pointer(handler,0x214),0x41444947);
    // Medical cabinets use COPH/COPN, not HDIG. The native context owner is
    // CTNR and links back through378; A64/A70 become the highlighted loot.
    if(Word(handler,0)!=0xca11b8||!HandlerClass(handler,0x434f5048)||
       Word(handler,0xb0)>=12||Word(handler,0x4e8)!=1)return false;
    const auto* context=Pointer(handler,0x4d4);
    const auto* container=context?Pointer(context,4):nullptr;
    return ActorClass(container,0x43544e52)&&Pointer(container,0x378)==context;
}
static __declspec(noinline) bool VehicleInteractionContext(unsigned char* hero,interaction_input::Mode& mode){
    const auto* handler=Pointer(hero,0x25c);
    // Exact native HVCL ownership; unrecognised/special states keep their native
    // input instead of inheriting the generic "Other -> F means LEAVE" route.
    if(!handler||Word(handler,0)!=0xc84b68||Pointer(handler,0x20)!=hero||
       !HandlerClass(handler,0x4856434c))return false;
    mode=interaction_input::Mode::VehicleNative;
    const auto state=Word(handler,0xb0);const auto* descriptor=Pointer(handler,0x18);
    const auto* vehicle=Pointer(handler,0x8e8);
    if(!vehicle||Pointer(hero,0x28c)!=vehicle||descriptor!=reinterpret_cast<void*>(0xf55da8)||Word(descriptor,0x1c)!=59||Pointer(descriptor,0x30)!=reinterpret_cast<void*>(0xf55de0))return true;
    // Native1D/1E (animations52F/530) explicitly query ENTER7 held/repress.
    // Do not relabel them as TakeKey: the key's actual focus is helper action18.
    if((state==0x1d||state==0x1e)&&Word(reinterpret_cast<void*>(0xf55de0),state*24+0x10)!=(state==0x1d?0x52fu:0x530u))return true;
    const bool keyFocus=(state==5||state==15)&&vehicleKeyFocus&&vehicleKeyFocus();
    mode=interaction_input::VehicleMode(state,keyFocus);return true;
}
static void UpdateInteraction(void* controller,unsigned tick){
    unsigned char* hero=nullptr;const bool held=(GetAsyncKeyState('F')&0x8000)!=0;
    const bool valid=WorldInteractionContext(controller,&hero);
    interaction_input::Frame frame{};frame.tick=tick;frame.owner=reinterpret_cast<unsigned>(hero);frame.eligible=valid;frame.held=held;
    if(valid){
        unsigned char *ordinaryHero=nullptr,*inventory=nullptr;
        frame.mode=NativeContext(controller,&ordinaryHero,&inventory)?interaction_input::Mode::Ordinary:interaction_input::Mode::Other;
        // HHDL's original6B97B2 consumes ACTION_LEAVE8 and emits100000
        // for putting down its authenticated world object. Ordinary ENTER7
        // emits80000 instead; do not erase LEAVE merely because HHMV is valid.
        if(frame.mode==interaction_input::Mode::Ordinary&&HeldMelee(hero,Pointer(hero,0xf9c)))
            frame.mode=interaction_input::Mode::HeldObject;
        if(CabinetInteractionContext(hero))frame.mode=interaction_input::Mode::Cabinet;
        interaction_input::Mode vehicleMode{};if(VehicleInteractionContext(hero,vehicleMode)){frame.mode=vehicleMode;if(vehicleMode==interaction_input::Mode::VehicleNative)frame.eligible=false;}
    }
    const auto next=interactionControls.Step(frame);
    SetInteractionAction(interactionAction,interactionValue,interactionBinding,next.enter,tick-interactionTick);
    SetInteractionAction(interactionLeaveAction,interactionLeaveValue,interactionLeaveBinding,next.leave,tick-interactionTick);
    interactionLeaving=frame.mode!=interaction_input::Mode::Ordinary;
    interactionVehicle=valid&&(frame.mode==interaction_input::Mode::Vehicle||frame.mode==interaction_input::Mode::VehicleInteractive||frame.mode==interaction_input::Mode::VehicleHeld);
    // Stable cabin interactions consume17/26 pressed edges, not ENTER7. Only
    // the authenticated focused key gets those edges immediately; ordinary
    // driving still receives exit aliases exclusively after a500ms hold.
    interactionVehicleEnterAliases=valid&&frame.mode==interaction_input::Mode::VehicleInteractive;
    interactionTick=tick;interactionOwner=frame.owner;interactionActive=valid&&frame.eligible;
}
static bool NativeContext(void* controller,unsigned char** outputHero,unsigned char** outputInventory){
    unsigned char* hero=nullptr;if(!WorldInteractionContext(controller,&hero))return false;
    const auto* movement=Pointer(hero,0x25c);
    if(!movement)return false;
    const DWORD vt=Word(movement,0),state=Word(movement,0xb0);
    // Native66F1F0 is the ordinary landing-to-walk variant47, established by
    // the camera audit. Retain input ownership during that on-foot blend.
    if(vt==0xc857b8){if(state>7&&state!=47)return false;}
    else if(vt==0xc86c08||vt==0xc86e78){if(state>6)return false;}
    // Original HHMC descriptorF56FFC has exactly eight carried-character
    // states. Its factory682E40/680170 derives the same HHMV input family.
    else if(vt==0xc874c0){if(state>7)return false;}
    else return false;
    auto* inv=Pointer(hero,0x810);
    auto* hands=Pointer(hero,0xf9c);
    if(!inv||Pointer(inv,0x754)!=hero||!hands||Pointer(hands,0)!=hero)return false;
    *outputHero=hero;*outputInventory=inv;return true;
}
static unsigned char* HeldLamp(unsigned char* hands,unsigned* slot){
    for(unsigned n=0;n<3;++n){auto* item=Pointer(hands,4+n*16);if(IsLamp(item)){*slot=n;return item;}}
    return nullptr;
}
static bool HandGestureScope(unsigned char* handler){
    if(!handler||!enabled)return false;
    unsigned char *hero=nullptr,*inventory=nullptr;
    if(!NativeContext(HeroController(),&hero,&inventory)||
        Pointer(handler,0x20)!=hero)return false;
    if((Word(hero,0x968)&1)!=0){
        if((actionControls.equipment!=Equipment::Melee&&actionControls.equipment!=Equipment::CarriedBody)||
           !HandlerClass(handler,0x4848444c))return false;
    }else if(!freeThirdPerson)return false;
    return (Pointer(hero,0xf78)==handler||Pointer(hero,0xf7c)==handler)&&
        actionRoutingActive&&reinterpret_cast<unsigned>(hero)==actionControls.owner&&GetTickCount()-actionControls.lastTick<=100;
}
static ItemKey OwnedItemKey(const unsigned char* actor);
struct RetainedHandSample {
    ItemKey hero{},left{},right{},both{};
    unsigned char* handler{};unsigned char* descriptor{};
    float sample[4]{};
    unsigned sampleTick{},attackTick{},prepareStarted{};bool sampleFresh{},attackEmitted{},positioned{},prepareAttack{};
};
static RetainedHandSample retainedHandSamples[2];
static void ClearRetainedHandSamples(){for(auto& entry:retainedHandSamples)entry={};}
static const float* RetainMeleeHandSample(unsigned char* handler,const float* original,bool direct){
    auto* hero=Pointer(handler,0x20);auto* hands=hero?Pointer(hero,0xf9c):nullptr;
    if(!hands||Pointer(hands,0)!=hero)return nullptr;
    auto& entry=retainedHandSamples[Pointer(hero,0xf78)==handler?0:1];
    const auto h=OwnedItemKey(hero),l=OwnedItemKey(Pointer(hands,4)),r=OwnedItemKey(Pointer(hands,0x14)),b=OwnedItemKey(Pointer(hands,0x24));
    auto* descriptor=Pointer(handler,0x18);
    if(entry.handler!=handler||entry.descriptor!=descriptor||entry.hero!=h||entry.left!=l||entry.right!=r||entry.both!=b){
        entry={};entry.hero=h;entry.left=l;entry.right=r;entry.both=b;entry.handler=handler;entry.descriptor=descriptor;
    }
    bool valid=original!=nullptr;
    if(valid)for(unsigned i=0;i<4;++i)if(!std::isfinite(original[i])||fabsf(original[i])>4.f)valid=false;
    if(valid){entry.sampleTick=actionControls.lastTick;entry.sampleFresh=true;}
    if(direct){
        if(valid){memcpy(entry.sample,original,sizeof(entry.sample));entry.positioned=true;entry.prepareAttack=false;return original;}
        return entry.sample;
    }
    return entry.sample;
}
static const float* __cdecl RouteHandSample(unsigned char* handler,const float* original){
    __try {
        const bool scoped=HandGestureScope(handler);
        // The producer supplies an absolute manipulation sample, not a mouse
        // delta. Retain the last E-owned sample for this exact held object.
        // Publishing neutral every frame would gradually straighten it again.
        if(scoped&&(actionControls.equipment==Equipment::Melee||actionControls.equipment==Equipment::CarriedBody)){
            if(const auto* retained=RetainMeleeHandSample(handler,original,decision.directHands)){
                if(!decision.directHands)++gestureSampleBlocks;
                return retained;
            }
        }
        if(scoped&&!decision.directHands){++gestureSampleBlocks;return neutralHandSample;}
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    return original;
}
// Native581110 writes an eight-sector mask at record+1C. Native5818E0's
// keyboard hit chooses the lowest tied sector and attacks its opposite (+4).
static DWORD NativeMeleeHitPacket(DWORD sectorMask){
    sectorMask&=0xffu;
    if(!sectorMask)return 0x14021u; // Existing native forward hit for neutral pose.
    unsigned sector=0;while((sectorMask&(1u<<sector))==0)++sector;
    return 0x14001u|((1u<<((sector+4u)&7u))<<5);
}
static DWORD RetainedMeleeHit(unsigned char* handler,bool pressed){
    const auto* sample=RetainMeleeHandSample(handler,nullptr,false);
    if(!sample)return 0;
    auto* hero=Pointer(handler,0x20);
    auto& entry=retainedHandSamples[Pointer(hero,0xf78)==handler?0:1];
    if(entry.attackEmitted&&entry.attackTick==actionControls.lastTick)return 0;
    const DWORD state=Word(handler,0xb0);
    if(entry.prepareAttack){
        // The native stance/animation owns readiness. Re-publish the explicit
        // click only while still cocking; a native attack state consumes it.
        if(state==12||state==14){entry.prepareAttack=false;return 0;}
        if(actionControls.lastTick-entry.prepareStarted>1000||decision.directHands||
           decision.Down(ObjectHandle)||decision.Down(ObjectLaunch)||
           (state!=10&&state!=11&&state!=13)){entry.prepareAttack=false;return 0;}
    }
    if(!pressed&&!entry.prepareAttack)return 0;
    if(!entry.positioned){
        const auto* held=Pointer(handler,0x468);
        if((state!=10&&state!=11)||!HandlerClass(handler,0x4848444c)||!held||
           (reinterpret_cast<unsigned>(held)!=entry.left.token&&reinterpret_cast<unsigned>(held)!=entry.right.token&&
            reinterpret_cast<unsigned>(held)!=entry.both.token))return 0;
        // Actual r107 trace: neutral sample plus LMB only entered state13,
        // never the swing state14. Give the native producer the same valid
        // absolute cocking direction proven by actual E positioning. This
        // happens solely for an explicit click, never passive mouse movement.
        entry.sample[0]=-1;entry.sample[1]=entry.sample[2]=entry.sample[3]=0;
        entry.positioned=true;entry.prepareAttack=true;entry.prepareStarted=actionControls.lastTick;
        entry.sampleFresh=false;
        // Native state11 needs the original explicit LMB event to enter13.
        // Suppressing this initiating packet deadlocks the readiness queue.
        // It is emitted only on this first physical click; the native consumer
        // still owns cocking, and only fresh state13 history permits the swing.
        entry.attackEmitted=true;entry.attackTick=actionControls.lastTick;
        return NativeMeleeHitPacket(0);
    }
    if(entry.prepareAttack&&state!=13)return 0;
    if(!entry.sampleFresh||entry.sampleTick!=actionControls.lastTick)return 0;
    const auto* history=handler+0x56c;
    const DWORD capacity=Word(history,4),count=Word(history,8),next=Word(history,12);
    const auto* records=Pointer(history,0);
    if(!records||!capacity||capacity>4096||!count||count>capacity||next>=capacity)return 0;
    const auto* latest=records+(next?next-1:capacity-1)*32;
    for(unsigned i=0;i<4;++i){
        const float actual=*reinterpret_cast<const float*>(latest+i*4);
        if(!std::isfinite(actual)||fabsf(actual-sample[i])>.001f)return 0;
    }
    const DWORD mask=Word(latest,0x1c);
    if(mask&~0xffu)return 0;
    entry.attackEmitted=true;entry.attackTick=actionControls.lastTick;
    return NativeMeleeHitPacket(mask);
}
static void __cdecl FilterHandGesture(unsigned char* handler,DWORD* event){
    __try {
        if(!event||!HandGestureScope(handler))return;
        if(actionControls.equipment==Equipment::CarriedBody){
            // Body positioning uses the same native hand sampler. E must not
            // turn a fast positioning motion into a synthesized melee hit.
            if(!decision.directHands||(*event&0x1fu)!=0x10u)*event=0;
            ++gestureEventBlocks;return;
        }
        if(actionControls.equipment==Equipment::Melee){
            const bool pressed=(actionRecords[0].flags&8)!=0;
            const DWORD hit=RetainedMeleeHit(handler,pressed);
            if(hit||pressed)*event=hit;
            else if(!decision.directHands||(*event&0x1fu)!=0x10u)*event=0;
            // E permits positioning/cocking, never a mouse-generated hit.
            // The original matcher, animation, collision and damage still own
            // every explicit LMB hit, including native dangerous self contact.
            ++gestureEventBlocks;return;
        }
        if(decision.directHands)return;
        *event=0;++gestureEventBlocks;
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
static __declspec(naked) void GestureSampleAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push eax
        push edi
        call RouteHandSample
        add esp,8
        mov [ebp-8],eax
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [nativeGestureSample]
    }
}
static __declspec(naked) void GestureEventAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        // Native EBP is the event pointer at this convergence point.
        push dword ptr [ebp]
        push esi
        call FilterHandGesture
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov ecx,[esi+20h]
        cmp dword ptr [ecx+95ch],0
        jmp dword ptr [nativeGestureEventContinue]
    }
}
static __declspec(naked) void GestureMatchAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        // This second consumer holds the event in EDI, including the branch
        // that skipped the native history evaluator altogether.
        lea eax,[ebp-36]
        push eax
        push esi
        call FilterHandGesture
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov eax,[esi+20h]
        mov ebx,[eax+95ch]
        jmp dword ptr [nativeGestureMatchContinue]
    }
}
// Native6A5570 has already applied look pitch, authored arc340 and charge
// speed when its local velocity reaches6A5927. Change only its aim frame;
// native6F3D00 and the subsequent collision/ballistic solvers remain unchanged.
struct ThrowAimSelection {
    ItemKey hero{},item{},pistol{};unsigned char* handler{};unsigned char* look{};
    unsigned gesture{};float target[4]{};bool valid{};
};
static ThrowAimSelection throwAimSelection{};
static volatile LONG throwAimUpdates=0;
static volatile float lastNativeThrowVelocity[4]{},lastCameraThrowVelocity[4]{},lastCameraThrowTarget[4]{};
static void* nativeProjectileVelocity=reinterpret_cast<void*>(0x6f3d00);
static ItemKey throwFacingHero{};
static unsigned char* throwFacingNode=nullptr;
static DWORD throwFacingTick=0;
static volatile LONG throwFacingRequests=0;
static void ReleaseThrowFacing(){
    __try {
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(throwFacingNode&&hero&&OwnedItemKey(hero)==throwFacingHero&&Pointer(hero,0x20)==throwFacingNode)
            FaceDirection(hero,throwFacingNode,reinterpret_cast<const float*>(hero+0x210));
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    throwFacingHero={};throwFacingNode=nullptr;throwFacingTick=0;orbitThrowFacing=false;
}

static __declspec(noinline) bool RotateNativeThrowVelocity(const float* velocity,float oldAimPitch,const float* aim,float* output){
    float speed2=0,aim2=0;
    for(unsigned i=0;i<3;++i){if(!std::isfinite(velocity[i])||!std::isfinite(aim[i]))return false;speed2+=velocity[i]*velocity[i];aim2+=aim[i]*aim[i];}
    if(!std::isfinite(oldAimPitch)||!std::isfinite(speed2)||speed2<.0001f||speed2>1000000.f||
       !std::isfinite(aim2)||aim2<.0001f||!std::isfinite(velocity[3]))return false;
    const float horizontal=sqrtf(aim[0]*aim[0]+aim[2]*aim[2]);if(horizontal<.0001f)return false;
    const float oldPitch=atan2f(velocity[1],sqrtf(velocity[0]*velocity[0]+velocity[2]*velocity[2]));
    // The old velocity already includes native authored arc. Replace only the
    // native look pitch with the actual camera target elevation.
    const float pitch=oldPitch-oldAimPitch+atan2f(aim[1],horizontal),speed=sqrtf(speed2);
    const float planar=cosf(pitch)*speed;
    output[0]=aim[0]/horizontal*planar;output[1]=sinf(pitch)*speed;output[2]=aim[2]/horizontal*planar;output[3]=velocity[3];
    return std::isfinite(output[0])&&std::isfinite(output[1])&&std::isfinite(output[2]);
}
static void __cdecl RouteThrowVelocity(unsigned char* handler,const float* origin,float* velocity,unsigned char* properties){
    __try {
        if(!enabled||!freeThirdPerson||!handler||!origin||!velocity||!properties||!actionRoutingActive||
           (actionControls.equipment!=Equipment::PistolThrowable&&actionControls.equipment!=Equipment::Throwable)||(!decision.Down(ObjectHandle)&&!decision.Down(ObjectLaunch))||
           GetTickCount()-actionControls.lastTick>100)return;
        unsigned char *hero=nullptr,*inventory=nullptr;
        if(!NativeContext(HeroController(),&hero,&inventory)||reinterpret_cast<unsigned>(hero)!=actionControls.owner||
           (Word(hero,0x968)&1)||Word(inventory,0x81c)||
           Pointer(handler,0x20)!=hero||Word(handler,0xb0)!=10||!HandlerClass(handler,0x484c4c55))return;
        auto* hands=Pointer(hero,0xf9c);auto* item=Pointer(handler,0x118);
        auto* pistol=hands&&IsPistol(Pointer(hands,0x14))?Pointer(hands,0x14):nullptr;
        auto* look=Pointer(hero,0xf74);
        if(!item||InventoryThrowHandler(hero,item)!=handler||!look||Pointer(look,0x20)!=hero)return;
        float speed2=0;for(unsigned i=0;i<3;++i){if(!std::isfinite(origin[i])||!std::isfinite(velocity[i]))return;speed2+=velocity[i]*velocity[i];}
        if(!std::isfinite(speed2)||speed2<.0001f||speed2>1000000.f)return;
        const auto h=OwnedItemKey(hero),k=OwnedItemKey(item),p=OwnedItemKey(pistol);
        auto& own=throwAimSelection;
        if(own.hero!=h||own.item!=k||own.pistol!=p||own.handler!=handler||own.look!=look||own.gesture!=actionControls.eStart){
            own={};own.hero=h;own.item=k;own.pistol=p;own.handler=handler;own.look=look;own.gesture=actionControls.eStart;
        }
        if(actionControls.previousE||!own.valid){
            float eye[4]{0,0,0,1},forward[4]{};
            if(!GetOrbitThrowAimRay(eye,forward))return;
            float norm2=0;
            for(unsigned i=0;i<3;++i){if(!std::isfinite(eye[i])||!std::isfinite(forward[i]))return;norm2+=forward[i]*forward[i];}
            if(norm2<.5f||norm2>1.5f||!std::isfinite(norm2))return;
            const float inv=1.f/sqrtf(norm2);for(unsigned i=0;i<3;++i)forward[i]*=inv;
            // Ray length comes from native velocity and gravity, not a new
            // weapon range. Bound it to one native ballistic flight diameter.
            const auto* physics=*reinterpret_cast<unsigned char**>(0xff67bc);
            const float gravity=physics?fabsf(*reinterpret_cast<const float*>(physics+0x14)):0.f;
            if(!std::isfinite(gravity)||gravity<.01f)return;
            const float range=std::min(100.f,speed2/gravity);
            const float eyeDistance=sqrtf((eye[0]-origin[0])*(eye[0]-origin[0])+(eye[1]-origin[1])*(eye[1]-origin[1])+(eye[2]-origin[2])*(eye[2]-origin[2]));
            const float length=std::max(.25f,range+eyeDistance);
            const float hit=OrbitRay(orbit.component,eye,forward,length);
            if(!std::isfinite(hit)||hit<=.01f||hit>length+.01f)return;
            for(unsigned i=0;i<3;++i)own.target[i]=eye[i]+forward[i]*hit;
            own.target[3]=1;own.valid=true;
        }
        float oldAimY=0;
        for(unsigned i=0;i<4;++i){
            const float component=*reinterpret_cast<const float*>(look+0x270+i*4);
            const float worldY=*reinterpret_cast<const float*>(hero+0x1f4+i*16);
            if(!std::isfinite(component)||!std::isfinite(worldY))return;oldAimY+=component*worldY;
        }
        if(!std::isfinite(oldAimY)||fabsf(oldAimY)>1.001f)return;
        float aim[3]{own.target[0]-origin[0],own.target[1]-origin[1],own.target[2]-origin[2]},adjusted[4]{};
        if(!RotateNativeThrowVelocity(velocity,asinf(std::clamp(oldAimY,-1.f,1.f)),aim,adjusted))return;
        for(unsigned i=0;i<4;++i){lastNativeThrowVelocity[i]=velocity[i];lastCameraThrowVelocity[i]=adjusted[i];lastCameraThrowTarget[i]=own.target[i];}
        memcpy(velocity,adjusted,16);InterlockedIncrement(&throwAimUpdates);
        if(actionControls.previousE&&!decision.aim){
            float planar[4]{own.target[0]-*reinterpret_cast<const float*>(hero+0x220),0,
                own.target[2]-*reinterpret_cast<const float*>(hero+0x228),0};
            const float length=sqrtf(planar[0]*planar[0]+planar[2]*planar[2]);
            if(std::isfinite(length)&&length>.1f){
                planar[0]/=length;planar[2]/=length;auto* node=Pointer(hero,0x20);
                if(node&&FaceDirection(hero,node,planar)){
                    throwFacingHero=h;throwFacingNode=node;throwFacingTick=GetTickCount();
                    orbitThrowFacing=true;InterlockedIncrement(&throwFacingRequests);
                }
            }
        }

    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
static __declspec(naked) void ThrowVelocityAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+8]
        push edx
        push ecx
        push ebx
        call RouteThrowVelocity
        add esp,16
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        // Exact custom native ECX-origin / EDX-velocity / stack-properties API.
        jmp dword ptr [nativeProjectileVelocity]
    }
}
static ItemKey OwnedItemKey(const unsigned char* actor){
    return actor?ItemKey{reinterpret_cast<unsigned>(actor),Word(actor,0x10),Word(actor,0x14)}:ItemKey{};
}
// The native bottle prepare animation can defer state10 beyond a short key tap.
// Queue only physical input; readiness and the throw remain owned by the engine.
static __declspec(noinline) void UpdateBottleThrow(const Frame& frame,unsigned char* hero,
                                                  unsigned char* inventory,unsigned char* left,bool equipQueued){
    auto* handler=InventoryThrowHandler(hero,left);
    const bool supported=hero&&inventory&&left&&
        HandlerClass(handler,0x484c4c55)&&
        Pointer(handler,0x20)==hero&&Pointer(handler,0x118)==left;
    if(!supported){
        const bool ownedGesture=bottleRelease.armed||bottleRelease.pending;
        bottleRelease.Clear();
        // Do not transfer an in-flight bottle gesture to another handler/item.
        // An unrelated HHDL gesture retains its existing native policy.
        if(ownedGesture&&frame.eligible){decision.Set(ObjectHandle,false);decision.Set(ObjectLaunch,false);}
        return;
    }
    bottle_throw::Frame sample{};sample.tick=frame.tick;sample.e=frame.e;sample.cancel=frame.cancelThrow;
    const ItemKey item=OwnedItemKey(left);
    sample.identity={reinterpret_cast<unsigned>(hero),item.token,item.low,item.high,reinterpret_cast<unsigned>(handler)};
    auto* hands=Pointer(hero,0xf9c);
    sample.eligible=frame.eligible&&frame.focused&&frame.keyboardValid&&frame.mouseValid&&
        frame.owner==reinterpret_cast<unsigned>(hero)&&hero==*reinterpret_cast<unsigned char**>(0xfd61ec)&&
        (frame.equipment==Equipment::PistolThrowable||frame.equipment==Equipment::Throwable)&&hands&&
        (Pointer(hands,4)==left||Pointer(hands,0x14)==left)&&
        Word(inventory,0x81c)==0&&!equipQueued;
    sample.ready=Word(handler,0xb0)==10&&Word(handler,0x10c)==0;
    const auto output=bottleRelease.Step(sample);
    if(frame.eligible){decision.Set(ObjectHandle,output.prepare);decision.Set(ObjectLaunch,output.launch);}
}
static bool NativePistolBehavior(unsigned char* weapon);
static volatile LONG postThrowAimFrames=0,postThrowFireRequests=0;
static __declspec(noinline) bool NativeThrownTargetAim(unsigned char* hero,unsigned char* pistol){
 if(!hero||!freeThirdPerson||(Word(hero,0x968)&1)||!NativePistolBehavior(pistol))return false;
 const auto& own=throwAimSelection;
 if(!own.valid||OwnedItemKey(hero)!=own.hero||OwnedItemKey(pistol)!=own.pistol)return false;
 auto* hands=Pointer(hero,0xf9c);auto* target=Pointer(hero,0xc6c);
 if(!hands||Pointer(hands,0)!=hero||Pointer(hands,0x14)!=pistol||Pointer(hands,4)||Pointer(hands,0x24)||
    !target||OwnedItemKey(target)!=own.item||!ActorClass(target,0x414f424a))return false;
 auto* overlay=Pointer(hero,0xf78);const DWORD state=overlay?Word(overlay,0xb0):~0u;
 if(!overlay||Word(overlay,0)!=0xc89e68||Pointer(overlay,0x20)!=hero||Pointer(overlay,0x118)!=pistol||
    (state!=14&&state!=11))return false;
 const unsigned kind=Word(target,0x58c)&0xffffu;
 // Native64FEE0 rejects destroyed projectile targets through actor588 bit1.
 return (kind==2||kind==8||kind==0xe||kind==0x16||kind==0x1b||kind==0x1c)&&(Word(target,0x588)&2u)==0;
}
static volatile LONG throwCancelRequests=0;
static __declspec(noinline) bool CanCancelBottleThrow(const Frame& frame,unsigned char* hero,unsigned char* inventory,unsigned char* left){
 if(!enabled||(!freeThirdPerson&&!frame.firstPerson)||
    (frame.equipment!=Equipment::PistolThrowable&&frame.equipment!=Equipment::Throwable)||!frame.q||controls.previousQ||
    !bottleRelease.active||(!bottleRelease.armed&&!bottleRelease.pending)||!hero||!inventory||Word(inventory,0x81c)||!left)return false;
 auto* handler=InventoryThrowHandler(hero,left);const auto key=OwnedItemKey(left);
 const bottle_throw::Identity live{reinterpret_cast<unsigned>(hero),key.token,key.low,key.high,reinterpret_cast<unsigned>(handler)};
 return bottleRelease.identity==live&&handler&&Pointer(handler,0x20)==hero&&Pointer(handler,0x118)==left&&
  HandlerClass(handler,0x484c4c55)&&Word(handler,0xb0)!=11;
}
static bool NativePistolBehavior(unsigned char* weapon){
    const auto* behavior=weapon?Pointer(weapon,0x25c):nullptr;
    return IsPistol(weapon)&&behavior&&Word(behavior,0)==0xc94528&&Pointer(behavior,0x20)==weapon;
}
#include "scoped_gun_aim.inl"
#include "scoped_lamp_aim.inl"
static __declspec(noinline) void PlayNativeGunPose(unsigned char* overlay,DWORD first,DWORD second,DWORD layer){
    using Play=void(__thiscall*)(void*,DWORD,DWORD,DWORD);
    reinterpret_cast<Play>(0x4b5bd0)(overlay,first,second,layer);
}
static bool GunPoseMatches(unsigned char* hero){
    if(!gunPose.active||hero!=gunPose.hero||OwnedItemKey(hero)!=gunPose.heroIdentity||
        Pointer(hero,0xf78)!=gunPose.overlay||Pointer(hero,0x3b4)!=gunPose.animation)return false;
    const auto* overlay=gunPose.overlay;const auto* animation=gunPose.animation;
    auto* hands=Pointer(hero,0xf9c);auto* weapon=hands?Pointer(hands,0x14):nullptr;
    if(!hands||Pointer(hands,0)!=hero||OwnedItemKey(weapon)!=gunPose.weaponIdentity||
        Word(overlay,0)!=0xc89e68||Pointer(overlay,0x20)!=hero||Pointer(overlay,0x118)!=weapon||
        Pointer(overlay,0x18)!=gunPose.descriptor||Word(overlay,0xb0)!=2||Word(overlay,0xe4)!=gunPose.layer||
        Word(animation,0xc)!=1)return false;
    const unsigned offset=gunPose.layer*0x68;
    return (Word(animation,offset+0x118)&4)==0&&Word(animation,offset+0x130)==gunPose.aimA&&
        Word(animation,offset+0x134)==gunPose.aimB;
}
static bool NativeGunPoseReady(unsigned char* hero){
    // The request is made before the native animation update. Allow its normal
    // blend to run, rather than showing a reticle/firing on the request frame.
    return GunPoseMatches(hero)&&scoped_gun_aim::Bound(hero,gunPose.animation)&&GetTickCount()-gunPose.requested>=100;
}
static void ReleaseNativeGunPose(){
    if(!gunPose.active)return;
    __try {
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        // Restore only our still-current request. A native reload, equip, hit,
        // script, perspective change or another layer request keeps priority.
        if(hero==gunPose.hero&&OwnedItemKey(hero)==gunPose.heroIdentity&&Pointer(hero,0x3b4)==gunPose.animation)
            scoped_gun_aim::RestoreWeights(hero,gunPose.animation);
        if(hero&&(Word(hero,0x968)&1)==0&&GunPoseMatches(hero))
            PlayNativeGunPose(gunPose.overlay,gunPose.previousA,gunPose.previousB,gunPose.layer);
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    gunPose={};
}
static bool EnsureNativeGunPose(unsigned char* hero,unsigned char* weapon){
    if(!decision.aim||!hero||(Word(hero,0x968)&1)){ReleaseNativeGunPose();return false;}
    if(gunPose.active){if(GunPoseMatches(hero)){if(gunPose.aimA==0x125e&&!scoped_gun_aim::PrepareWeights(hero,gunPose.animation)){ReleaseNativeGunPose();return false;}return scoped_gun_aim::Bind(hero,gunPose.animation);}ReleaseNativeGunPose();}
    auto* overlay=Pointer(hero,0xf78);auto* animation=Pointer(hero,0x3b4);
    if(!NativePistolBehavior(weapon)||!overlay||Word(overlay,0)!=0xc89e68||
        Pointer(overlay,0x20)!=hero||Pointer(overlay,0x118)!=weapon||Word(overlay,0xb0)!=2||
        !animation||Word(animation,0xc)!=1||!Word(animation,0x8c4))return false;
    const DWORD layer=Word(overlay,0xe4);if(layer>=16)return false;
    const unsigned offset=layer*0x68;if(Word(animation,offset+0x118)&4)return false;
    auto* descriptor=Pointer(overlay,0x18);
    if(!descriptor||Word(descriptor,4)!=0x484c4445||Word(descriptor,0x1c)<=14||Word(descriptor,0x1c)>256)return false;
    const auto* states=Pointer(descriptor,0x30);if(!states)return false;
    const DWORD carryA=Word(states,2*24+0x10),carryB=Word(states,2*24+0x14);
    const DWORD candidateA=Word(states,14*24+0x10),candidateB=Word(states,14*24+0x14);
    if(candidateA!=0x125e||candidateB!=0||Word(states,13*24+0x10)!=0x125d||Word(states,13*24+0x14)!=0)return false;
    if(Word(animation,offset+0x130)!=carryA||Word(animation,offset+0x134)!=carryB)return false;
    if(!scoped_gun_aim::PrepareWeights(hero,animation))return false;
    const DWORD aimA=candidateA,aimB=0;
    if((aimA==carryA&&aimB==carryB)||aimA==0xffffffffu||aimA==0xfffffffeu||
        Word(animation,offset+0x130)!=carryA||Word(animation,offset+0x134)!=carryB)return false;
    GunPoseOwnership owned{};owned.hero=hero;owned.overlay=overlay;owned.animation=animation;owned.descriptor=descriptor;
    owned.heroIdentity=OwnedItemKey(hero);owned.weaponIdentity=OwnedItemKey(weapon);owned.layer=layer;
    owned.aimA=aimA;owned.aimB=aimB;owned.previousA=carryA;owned.previousB=carryB;
    owned.requested=GetTickCount();owned.active=true;
    // State14 / selector125E resolves the native down/middle/up gun aim blend.
    // Use its untouched original resources. No private files or graph clones.
    // Same native animation API/pairs as4B5EA0, without changing behavior B0,
    // player perspective, mesh geometry or physics. Native layer blending wins.
    PlayNativeGunPose(overlay,aimA,aimB,layer);gunPose=owned;
    if(!GunPoseMatches(hero)){scoped_gun_aim::RestoreWeights(hero,animation);gunPose={};return false;}
    ++nativeGunPoseRequests;return true;
}
static __declspec(noinline) void* CallNativeAction(void* controller,unsigned index){
    void* result=nullptr;
    __asm {
        mov eax,controller
        push index
        call dword ptr [nativeActionAccessor]
        mov result,eax
    }
    return result;
}
// Query the authentic action so custom keyboard/gamepad bindings remain native.
static bool NativeMeleeLock(void* controller){
    return controller==HeroController()&&targetLockRouting&&targetLock.latched&&actualTargetLocked&&
        GetTickCount()-targetLock.tick<=100;
}
static void UpdateTargetLock(unsigned char* hero,void* controller,bool eligible,DWORD now){
    bool down=false;
    readingOriginalLock=true;
    __try {const auto* action=CallNativeAction(controller,LockTarget);down=action&&(Word(action,8)&1)!=0;}
    __finally {readingOriginalLock=false;}
    const bool before=(targetLockAction.flags&1)!=0;
    const DWORD elapsed=now-targetLock.tick;
    const bool effective=targetLock.Step(reinterpret_cast<unsigned>(hero),now,eligible,down);
    const bool changed=before!=effective;
    targetLockAction.flags=(effective?3u:0u)|(changed?4u:0u)|((effective&&changed)?8u:0u)|((!effective&&changed)?16u:0u);
    targetLockAction.duration=changed?0.f:targetLockAction.duration+std::min<DWORD>(elapsed,250)*.001f;
    targetLockValue.value=effective?1.f:0.f;targetLockBinding.value=&targetLockValue;
    targetLockAction.bindings=&targetLockBinding;targetLockAction.count=1;targetLockAction.combine=2;
    targetLockRouting=eligible;
    if(before!=effective)Log("Native target lock toggle active=%d",effective);
    actualTargetLocked=targetCapture&&targetCapture(effective,eligible);
}
static bool RetainTargetLockDuringHit(void* controller,DWORD now){
    unsigned char* hero=nullptr;
    if(!freeThirdPerson||!targetLockRouting||!WorldInteractionContext(controller,&hero)||
       targetLock.owner!=reinterpret_cast<unsigned>(hero)||(Word(hero,0x968)&1))return false;
    const auto* movement=Pointer(hero,0x25c);
    // Observed ordinary HMOV hit reaction. Preserve only existing lock input;
    // no melee facing, attack, movement or animation ownership is broadened.
    if(!movement||Word(movement,0)!=0xc857b8||Word(movement,0xb0)!=32)return false;
    auto* inventory=Pointer(hero,0x810);auto* hands=Pointer(hero,0xf9c);
    if(!inventory||Pointer(inventory,0x754)!=hero||Word(inventory,0x81c)||
       !hands||Pointer(hands,0)!=hero||IsPistol(Pointer(hands,0x14)))return false;
    UpdateTargetLock(hero,controller,true,now);return true;
}
// Called after native HHMV physics input. Use only the cached planar camera heading;
// keep the existing E gesture, attack animation and collision/damage machinery.
static void ApplyMeleeCamera(unsigned char* expectedHero,const float* forward,float step){
    orbitMeleeAim=false;
    if(!enabled||!meleeMouseFacing||!actionRoutingActive||!actionControls.active||
       actionControls.equipment!=Equipment::Melee||decision.directHands||
       decision.Down(ObjectLaunch)||GetTickCount()-actionControls.lastTick>100||!forward)return;
    __try {
        unsigned char *hero=nullptr,*inventory=nullptr;
        auto* controller=HeroController();
        if(!NativeContext(controller,&hero,&inventory)||hero!=expectedHero||
           reinterpret_cast<unsigned>(hero)!=actionControls.owner||(Word(hero,0x968)&1)||
           Word(inventory,0x81c)||!HeldMelee(hero,Pointer(hero,0xf9c))||
           Word(Pointer(hero,0x25c),0)==0xc874c0||NativeMeleeLock(controller)||
           (GetAsyncKeyState('E')&0x8000)||(GetAsyncKeyState(VK_RBUTTON)&0x8000))return;
        float planar[4]={forward[0],0,forward[2],0};
        const float length2=planar[0]*planar[0]+planar[2]*planar[2];
        if(!std::isfinite(planar[0])||!std::isfinite(planar[2])||
           !std::isfinite(length2)||length2<.0001f)return;
        // Native571EF0 uses acos(dot), requiring a unit direction. Removing
        // camera pitch shortens the vector; without normalization the motor
        // keeps turning even at the requested yaw and oscillates around it.
        const float inverseLength=1.f/sqrtf(length2);
        planar[0]*=inverseLength;planar[2]*=inverseLength;
        auto* node=Pointer(hero,0x20);
        if(node&&FaceDirection(hero,node,planar,step)){
            orbitMeleeAim=true;++meleeFacingTurns;
            static DWORD lastLog=0;const auto now=GetTickCount();
            if(!lastLog||now-lastLog>=2000){lastLog=now;
                Log("Melee mouse facing physics total=%u requested=%.5f actual=%.5f physics_dt=%.6f render_dt=%.6f",meleeFacingTurns,
                    atan2f(planar[0],planar[2]),atan2f(*reinterpret_cast<float*>(hero+0x210),*reinterpret_cast<float*>(hero+0x218)),
                    step,*reinterpret_cast<float*>(0xf503c8));}
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){orbitMeleeAim=false;}
}
// HHMV's ordinary movement writes zero angular velocity when WASD is idle,
// and its own facing motor when moving. Publish camera intent here, then apply
// it after virtual+234, immediately before physics consumes the angular motor.
static unsigned char* meleeCameraHero=nullptr;
static float meleeCameraForward[4]{};
static DWORD meleeCameraTick=0;
static void FollowMeleeCamera(unsigned char* hero,const float* forward){
    orbitMeleeAim=false;
    if(!forward){meleeCameraHero=nullptr;return;}
    memcpy(meleeCameraForward,forward,sizeof(meleeCameraForward));
    meleeCameraHero=hero;meleeCameraTick=GetTickCount();
}
static void __cdecl PrepareMeleeLocomotion(unsigned char* movement,float* nativeForward,const float* desired){
    if(!enabled||!meleeMouseFacing||!actionRoutingActive||!actionControls.active||
       actionControls.equipment!=Equipment::Melee||decision.directHands||decision.Down(ObjectLaunch)||
       GetTickCount()-actionControls.lastTick>100||GetTickCount()-meleeCameraTick>100)return;
    __try {
        unsigned char *hero=nullptr,*inventory=nullptr;auto* controller=HeroController();
        if(!NativeContext(controller,&hero,&inventory)||hero!=meleeCameraHero||
           reinterpret_cast<unsigned>(hero)!=actionControls.owner||Pointer(hero,0x25c)!=movement||
           Word(movement,0)!=0xc86c08||(Word(hero,0x968)&1)||Word(inventory,0x81c)||
           !HeldMelee(hero,Pointer(hero,0xf9c))||NativeMeleeLock(controller)||
           (GetAsyncKeyState('E')&0x8000)||(GetAsyncKeyState(VK_RBUTTON)&0x8000))return;
        const float lateral=OrbitRead(controller,0x1d0);
        if(!std::isfinite(lateral)||fabsf(lateral)<=.25f||!nativeForward)return;
        float forward[4]{};
        if(melee_locomotion::Forward(desired,meleeCameraForward,forward)){
            memcpy(nativeForward,forward,sizeof(forward));
            if(++meleeStrafeRequests<=4)Log("Melee directional gait applied lateral=%.3f desired=%.4f/%.4f basis=%.4f/%.4f",
                lateral,desired[0],desired[2],forward[0],forward[2]);
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void* nativeMeleeMovementAngle=reinterpret_cast<void*>(0x6433e0);
static __declspec(naked) void MeleeLocomotionAngleAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        // Original67C951: EBX=HHMV, EAX=desired local vector,
        // stack+4=temporary native forward. Keep original float return/ABI.
        push eax
        push dword ptr [ebp+8]
        push ebx
        call PrepareMeleeLocomotion
        add esp,12
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [nativeMeleeMovementAngle]
    }
}
static void AfterMeleeMovement(unsigned char* movement,DWORD context){
    __try {
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!hero||hero!=meleeCameraHero||Pointer(hero,0x25c)!=movement||
           GetTickCount()-meleeCameraTick>100)return;
        const auto table=Word(movement,0);
        if((table!=0xc86c08&&table!=0xc86e78)||Word(movement,0xb0)>6)return;
        if(!context)return;
        const float step=*reinterpret_cast<const float*>(context+8);
        if(!std::isfinite(step)||step<=0||step>.1f)return;
        // Match the native virtual+234 motor's own physics clock.
        ApplyMeleeCamera(hero,meleeCameraForward,step);
    }__except(EXCEPTION_EXECUTE_HANDLER){orbitMeleeAim=false;}
}
static void* nativeMeleePhysicsContinue=reinterpret_cast<void*>(0x67c826);
static void* nativeMeleePhysicsVariantContinue=reinterpret_cast<void*>(0x67d5e6);
static __declspec(naked) void OriginalMeleePhysics(){
    __asm {
        push ebp
        mov ebp,esp
        and esp,0fffffff0h
        jmp dword ptr [nativeMeleePhysicsContinue]
    }
}
static __declspec(naked) void OriginalMeleePhysicsVariant(){
    __asm {
        push ebp
        mov ebp,esp
        and esp,0fffffff0h
        jmp dword ptr [nativeMeleePhysicsVariantContinue]
    }
}
static void __fastcall MeleePhysicsAdapter(unsigned char* movement,void*,DWORD context,DWORD phase,const float* direction,float strength){
    using Native=void(__thiscall*)(unsigned char*,DWORD,DWORD,const float*,float);
    reinterpret_cast<Native>(&OriginalMeleePhysics)(movement,context,phase,direction,strength);
    AfterMeleeMovement(movement,context);
}
static void __fastcall MeleePhysicsVariantAdapter(unsigned char* movement,void*,DWORD context,DWORD phase,const float* direction,float strength){
    using Native=void(__thiscall*)(unsigned char*,DWORD,DWORD,const float*,float);
    reinterpret_cast<Native>(&OriginalMeleePhysicsVariant)(movement,context,phase,direction,strength);
    AfterMeleeMovement(movement,context);
}
static unsigned gunPhysicsFaces=0;
static void __cdecl AfterGunMovement(unsigned char* node,unsigned char* movement,unsigned char* context){
    if(!enabled||!freeThirdPerson||!actionRoutingActive||!decision.aim||!faceOwned||
       !InterlockedCompareExchange(&aimPublished,0,0)||GetTickCount()-actionControls.lastTick>100||
       GetTickCount()-static_cast<DWORD>(InterlockedCompareExchange(&aimPublishedTick,0,0))>100)return;
    __try {
        unsigned char *hero=nullptr,*inventory=nullptr;
        if(!context||!NativeContext(HeroController(),&hero,&inventory)||hero!=ownedHero||
           reinterpret_cast<unsigned>(hero)!=actionControls.owner||Pointer(hero,0x20)!=node||
           Pointer(node,0x40)!=hero||Pointer(hero,0x25c)!=movement||Pointer(movement,0x20)!=hero||
           (Word(hero,0x968)&1)||Word(inventory,0x81c)||!NativeGunPoseReady(hero)||
           !NativePistolBehavior(Pointer(Pointer(hero,0xf9c),0x14)))return;
        const float step=*reinterpret_cast<const float*>(context+8);
        if(!std::isfinite(step)||step<=0||step>.1f)return;
        float direction[4]{};for(unsigned n=0;n<4;++n)direction[n]=lastImmediateAimDirection[n];
        // Native locomotion just wrote its angular command. Reapply only the
        // authenticated gun heading using the very clock about to consume it.
        if(FaceDirection(hero,node,direction,step)&&++gunPhysicsFaces<=4)
            Log("Gun physics facing applied dt=%.6f direction=%.4f/%.4f",step,direction[0],direction[2]);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void __cdecl BeforeGunAngularCopy(unsigned char* node,unsigned char* context){
    __try {
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(hero&&Pointer(hero,0x20)==node)
            AfterGunMovement(node,Pointer(hero,0x25c),context);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void* nativeGunPhysicsContinue=reinterpret_cast<void*>(0x56efaa);
static __declspec(naked) void GunPhysicsAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push edi
        push esi
        call BeforeGunAngularCopy
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        // C83830's native phase0 is about to copy the actual angular command
        // to its backend. HHMV also reaches this common consumer. Preserve
        // all native state and replay its exact MOVSS, not an HHMV-only step.
        movss xmm0,dword ptr [esi+310h]
        jmp dword ptr [nativeGunPhysicsContinue]
    }
}
static __declspec(noinline) bool SendNativeWeaponAction(unsigned char* hero,unsigned char* weapon,DWORD action){
    if(!hero||!NativePistolBehavior(weapon))return false;
    // Authentic native APHA and secondary messages, as emitted by6AE4E0.
    // Both helpers are thiscall(message,sourceHero,weapon), RET8. Preserve the
    // native dispatch nesting so scripts, ammo, recoil, FX and sound stay native.
    struct Message {DWORD vtable,action;};
    Message primary{0xc62c2c,action},secondary{0xc6ee74,action};
    using Dispatch=int(__thiscall*)(Message*,void*,void*);
    auto* nesting=reinterpret_cast<volatile LONG*>(0xfd621c);
    ++*nesting;
    __try {
        reinterpret_cast<Dispatch>(0x4b85a0)(&primary,hero,weapon);
        reinterpret_cast<Dispatch>(0x4b85d0)(&secondary,hero,weapon);
    } __finally {--*nesting;}
    ++nativeFireMessages;return true;
}
static void ReleaseNativeTrigger(){
    if(!nativeTriggerHeld)return;
    nativeTriggerHeld=false;
    __try {
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!hero||Word(hero,0)!=0xc834c0||OwnedItemKey(hero)!=firingHero)return;
        auto* hands=Pointer(hero,0xf9c);auto* inventory=Pointer(hero,0x810);
        if(!hands||Pointer(hands,0)!=hero||!inventory||Pointer(inventory,0x754)!=hero)return;
        for(unsigned n=0;n<3;++n){auto* item=Pointer(hands,4+n*16);
            if(OwnedItemKey(item)==firingWeapon){SendNativeWeaponAction(hero,item,3);return;}}
        auto* stock=Pointer(inventory,0x4f0);
        for(unsigned n=0;stock&&n<12;++n){auto* item=Pointer(stock,4+n*20);
            if(OwnedItemKey(item)==firingWeapon){SendNativeWeaponAction(hero,item,3);return;}}
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
static bool __cdecl SuppressAutomaticFirstPerson(unsigned char* handler){
    __try {
        if(!HandGestureScope(handler))return false;
        auto* hero=Pointer(handler,0x20);auto* controller=Pointer(hero,0x95c);
        auto* hands=Pointer(hero,0xf9c);
        if(!hands||Pointer(hands,0)!=hero||!IsPistol(Pointer(hands,0x14)))return false;
        // The explicit perspective action45 is always native. Only the gun's
        // automatic first-person request is intercepted in our proven3p scope.
        const auto* perspective=CallNativeAction(controller,45);
        if(!perspective||(Word(perspective,8)&8))return false;
        // Native650F40 explicitly consumes this pending request bit. Rejecting
        // it must not defer an unwanted camera switch until equipment changes.
        *reinterpret_cast<DWORD*>(hero+0x968)&=~8u;
        ++automaticFirstPersonBlocks;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool __cdecl FilterFirstPersonRequest(unsigned char* hero,unsigned nativeResult){
    __try {
        if((nativeResult&255)==0)return false;
        if(hero==*reinterpret_cast<unsigned char**>(0xfd61ec)&&
            SuppressAutomaticFirstPerson(Pointer(hero,0xf78)))return false;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return (nativeResult&255)!=0;}
}
static __declspec(naked) void FirstPersonPredicateAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        push [ebp+8]
        call dword ptr [nativeShouldEnterFirstPerson]
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push eax
        push [ebp+8]
        call FilterFirstPersonRequest
        add esp,8
        mov [ebp-8],al
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret 4
    }
}
static __declspec(naked) void AutomaticFirstPersonAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push esi
        call SuppressAutomaticFirstPerson
        add esp,4
        test al,al
        jz nativeRequest
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [nativeFirstPersonEnd]
    nativeRequest:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov eax,esi
        call dword ptr [nativeCancelHandRequest]
        jmp dword ptr [nativeFirstPersonContinue]
    }
}
#include "inventory_assist.inl"
static void TraceCombat(unsigned char* hero,const Frame& frame,bool eligible){
    static unsigned count=0,lastTick=0,lastTag=~0u;
    if(count>=256||!hero)return;
    const unsigned tag=(eligible?1u:0u)|(frame.left?2u:0u)|(frame.right?4u:0u)|(frame.e?8u:0u)|(frame.q?16u:0u)|
        (static_cast<unsigned>(frame.equipment)<<5)|((interactionAction.flags&1)?128u:0u)|(static_cast<unsigned>(inventory_assist::state.phase)<<24)|
        (Word(hero,0x968)&0xffffu)<<8;
    if(tag==lastTag&&(!(frame.left||frame.right||frame.e)||frame.tick-lastTick<250))return;
    lastTag=tag;lastTick=frame.tick;++count;
    const auto* move=Pointer(hero,0x25c);const auto* look=Pointer(hero,0xf74);const auto* hand=Pointer(hero,0xf78);
    Log("combat r104 ctx=%u buttons=%u/%u/%u perspective=%08lx move=%08lx/%lu hand=%08lx/%lu look=%lu weights=%g/%g aim=%u pose=%ld fire=%u native=%u fpBlocked=%u gestures=%u/%u equipment=%u q=%u restore=%u weak=%08lx F=%u leave=%u",
        eligible?1u:0u,frame.left?1u:0u,frame.right?1u:0u,frame.e?1u:0u,Word(hero,0x968),
        move?Word(move,0):0,move?Word(move,0xb0):0,hand?Word(hand,0):0,hand?Word(hand,0xb0):0,
        look?Word(look,0xb0):0,look?*reinterpret_cast<const float*>(look+0x290):0.0f,
        look?*reinterpret_cast<const float*>(look+0x294):0.0f,decision.aim?1u:0u,
        InterlockedCompareExchange(&aimPublished,0,0),nativeTriggerHeld?1u:0u,nativeFireMessages,
        automaticFirstPersonBlocks,gestureSampleBlocks,gestureEventBlocks,static_cast<unsigned>(frame.equipment),frame.q?1u:0u,
        static_cast<unsigned>(inventory_assist::state.phase),displacedReference[0],(interactionAction.flags&1)?1u:0u,interactionLeaving?1u:0u);
}
static void __cdecl Tick(void* controller){
    if(controller!=HeroController())return;
    Frame frame{};frame.tick=GetTickCount();
    frame.q=(GetAsyncKeyState('Q')&0x8000)!=0;
    frame.e=(GetAsyncKeyState('E')&0x8000)!=0;
    __try {
        UpdateInteraction(controller,frame.tick);
        auto* observedHero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        inventory_assist::Tick(observedHero,observedHero?Pointer(observedHero,0x810):nullptr);
        unsigned char *hero=nullptr,*inventory=nullptr;
        if(!NativeContext(controller,&hero,&inventory)){
            if(!RetainTargetLockDuringHit(controller,frame.tick)){
                targetLock.Reset();targetLockRouting=actualTargetLocked=false;targetLockAction.flags=0;
                if(targetCapture)targetCapture(false,false);
            }
            ClearRetainedHandSamples();ReleaseThrowFacing();
            TraceCombat(observedHero,frame,false);
            Step(controls,frame);decision=Step(actionControls,frame);actionRoutingActive=false;
            bottle_throw::Frame invalid{};invalid.tick=frame.tick;invalid.e=frame.e;bottleRelease.Step(invalid);
            UpdateActions(decision,0);orbitHandManipulation=false;orbitPistolAim=false;orbitMeleeAim=orbitMeleeLocked=false;RestoreAim();ReleaseNativeTrigger();ReleaseNativeGunPose();return;
        }
        frame.owner=reinterpret_cast<unsigned>(hero);frame.eligible=frame.focused=frame.keyboardValid=frame.mouseValid=true;
        frame.firstPerson=(Word(hero,0x968)&1)!=0;
        auto* hands=Pointer(hero,0xf9c);auto* right=Pointer(hands,0x14);auto* left=Pointer(hands,4);
        auto* throwable=InventoryThrowHandler(hero,left)?left:InventoryThrowHandler(hero,right)?right:nullptr;
        frame.equipment=IsPistol(right)?(IsLamp(left)?Equipment::PistolFlashlight:
            (ThrowableInHand(hero,left,0)?Equipment::PistolThrowable:Equipment::Pistol)):Equipment::Unarmed;
        if(!IsPistol(right)&&HeldMelee(hero,hands))frame.equipment=Word(Pointer(hero,0x25c),0)==0xc874c0?Equipment::CarriedBody:Equipment::Melee;
        if(!IsPistol(right)&&throwable)frame.equipment=Equipment::Throwable;
        if(frame.equipment==Equipment::Unarmed&&!Pointer(hands,0x24)&&
           ((IsLamp(left)&&!right)||(IsLamp(right)&&!left)))frame.equipment=Equipment::Flashlight;
        auto* worldThrowable=WorldThrowable(hero,hands);
        frame.worldThrowable=worldThrowable&&(frame.equipment==Equipment::Melee||frame.equipment==Equipment::CarriedBody);
        auto* gestureItem=throwable?throwable:frame.worldThrowable?worldThrowable:nullptr;
        if(gestureItem){frame.item=reinterpret_cast<unsigned>(gestureItem);frame.itemLow=Word(gestureItem,0x10);frame.itemHigh=Word(gestureItem,0x14);}
        if(throwable){frame.itemRight=Word(throwable,0x4fc)!=0;frame.sprayLighter=SprayLighterInHands(hero,throwable);}
        UpdateTargetLock(hero,controller,freeThirdPerson&&(Word(hero,0x968)&1)==0&&Word(inventory,0x81c)==0&&
            (frame.equipment==Equipment::Melee||frame.equipment==Equipment::Unarmed),frame.tick);
        if(frame.equipment!=Equipment::Melee&&frame.equipment!=Equipment::CarriedBody)ClearRetainedHandSamples();
        frame.left=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
        frame.right=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
        frame.cancelThrow=CanCancelBottleThrow(frame,hero,inventory,throwable);
        if(frame.worldThrowable&&controls.active&&controls.previousE&&frame.e&&frame.q&&!controls.previousQ)frame.cancelThrow=true;
        if(frame.cancelThrow)InterlockedIncrement(&throwCancelRequests);
        const Decision q=Step(controls,frame);
        const DWORD elapsed=frame.tick-actionControls.lastTick;
        // Input ownership must not disappear during a transient camera-only
        // guard failure. Pose/fire still require the actual fresh camera ray.
        frame.eligible=frame.firstPerson?
            (frame.equipment==Equipment::PistolThrowable||frame.equipment==Equipment::Throwable||
             frame.equipment==Equipment::Melee||frame.equipment==Equipment::CarriedBody||
             frame.equipment==Equipment::Flashlight):
            freeThirdPerson&&frame.equipment!=Equipment::Unarmed;
        decision=Step(actionControls,frame);actionRoutingActive=frame.eligible&&actionControls.active;
        UpdateBottleThrow(frame,hero,inventory,throwable,q.toggleFlashlightEquip);
        if(actionRoutingActive&&decision.aim&&IsPistol(right)&&Word(inventory,0x81c)==0)EnsureNativeGunPose(hero,right);else ReleaseNativeGunPose();
        const bool poseFresh=InterlockedCompareExchange(&aimPublished,0,0)!=0&&
            frame.tick-static_cast<DWORD>(InterlockedCompareExchange(&aimPublishedTick,0,0))<=100&&NativeGunPoseReady(hero);
        if(!NativeGunPoseReady(hero))InterlockedExchange(&aimPublished,0);
        const bool nativeThrownAim=actionRoutingActive&&NativeThrownTargetAim(hero,right);
        if(nativeThrownAim)InterlockedIncrement(&postThrowAimFrames);
        const bool fireIntent=!nativeThrownAim&&decision.aim&&decision.Down(RightObjectUse)&&poseFresh;
        if(nativeThrownAim&&frame.left)InterlockedIncrement(&postThrowFireRequests);
        // Ordinary modern RMB fire uses the message bridge. In the original
        // flying-item autoaim state14, native6B0270 action2 instead requests
        // its genuine firing state11 without FP; preserve that entire path.
        if(nativeThrownAim)decision.Set(RightObjectUse,frame.left);
        else if(IsPistol(right)&&decision.Overrides(RightObjectUse))decision.Set(RightObjectUse,false);
        UpdateActions(decision,std::min<DWORD>(elapsed,250));
        if(!(actionRoutingActive&&(frame.equipment==Equipment::PistolThrowable||frame.equipment==Equipment::Throwable)&&actionControls.previousE&&
             !decision.aim&&OwnedItemKey(hero)==throwFacingHero&&Pointer(hero,0x20)==throwFacingNode&&
             frame.tick-throwFacingTick<=100))ReleaseThrowFacing();
        const bool nativeLock=(Word(hero,0x968)&1)==0&&
            (frame.equipment==Equipment::Melee||frame.equipment==Equipment::Unarmed)&&NativeMeleeLock(controller);
        if(nativeLock!=orbitMeleeLocked)Log("Native melee/unarmed target lock held=%d equipment=%u",nativeLock,static_cast<unsigned>(frame.equipment));
        orbitMeleeLocked=nativeLock;
        if(!meleeMouseFacing||!actionRoutingActive||frame.equipment!=Equipment::Melee||
           decision.directHands||frame.e||frame.right||orbitMeleeLocked)orbitMeleeAim=false;
        orbitHandManipulation=actionRoutingActive&&decision.directHands;
        orbitPistolAim=actionRoutingActive&&decision.aim;
        if(!orbitPistolAim)RestoreAim();
        const bool fire=actionRoutingActive&&fireIntent&&Word(inventory,0x81c)==0&&NativePistolBehavior(right);
        if(fire){
            const ItemKey weaponIdentity=OwnedItemKey(right),heroIdentity=OwnedItemKey(hero);
            if(nativeTriggerHeld&&(firingWeapon!=weaponIdentity||firingHero!=heroIdentity))ReleaseNativeTrigger();
            if(!nativeTriggerHeld&&SendNativeWeaponAction(hero,right,2)){
                // Message handlers may run scripts; capture identity before
                // dispatch and do not dereference that item after dispatch.
                nativeTriggerHeld=true;firingWeapon=weaponIdentity;firingHero=heroIdentity;
            }
        }else ReleaseNativeTrigger();
        if(q.toggleFlashlight&&Word(inventory,0x81c)==0){unsigned slot=0;if(auto* lamp=HeldLamp(hands,&slot))CallLampToggle(lamp);}
        if(q.toggleFlashlightEquip)inventory_assist::Toggle(hero,inventory);
        TraceCombat(hero,frame,actionRoutingActive);
    } __except(EXCEPTION_EXECUTE_HANDLER){targetLock.Reset();targetLockRouting=readingOriginalLock=actualTargetLocked=false;if(targetCapture)targetCapture(false,false);controls={};actionControls={};bottleRelease.Clear();decision={};actionRoutingActive=false;interactionActive=false;orbitHandManipulation=false;orbitPistolAim=false;orbitMeleeAim=orbitMeleeLocked=false;RestoreAim();ReleaseNativeTrigger();ReleaseNativeGunPose();ClearLampRestore();ReleaseThrowFacing();}
}
static __declspec(naked) void ControllerAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ecx
        call Tick
        add esp,4
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        // Replay exactly the overwritten native prologue.
        push ebp
        mov ebp,esp
        and esp,0fffffff0h
        jmp dword ptr [nativeControllerContinue]
    }
}
static void Configure(const wchar_t* path,bool supported){
    enabled=supported&&ConfigFlag(L"Controls",L"ModernCombat",true,path);
    meleeMouseFacing=ConfigFlag(L"Controls",L"MeleeMouseFacing",true,path);
    orbitMeleePose=enabled?&FollowMeleeCamera:nullptr;
    Log("Melee camera controls configured mouse_facing=%d native_lock=preserved",enabled&&meleeMouseFacing);
}
static void Install(){
    if(!enabled||InterlockedCompareExchange(&installAttempt,1,0))return;
    const unsigned char solverPrologue[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x8b,0x0d,0x90,0x5d,0xfd,0,
        0x81,0xec,0x8c,0,0,0,0x56,0x8b,0xf0};
    const unsigned char solverBody[]={0x8d,0x44,0x24,0x50,0x50};
    const unsigned char solverEpilogue[]={0x5e,0x8b,0xe5,0x5d,0xc3};
    if(memcmp(reinterpret_cast<void*>(0x6ac040),solverPrologue,sizeof(solverPrologue))||
        memcmp(reinterpret_cast<void*>(0x6ac06b),solverBody,sizeof(solverBody))||
        memcmp(reinterpret_cast<void*>(0x6ac19a),solverEpilogue,sizeof(solverEpilogue))){enabled=false;Log("Modern combat native aim solver signature mismatch");return;}
    struct Patch {DWORD address;unsigned size;unsigned char original[13];void* replacement;DWORD protection;};
    Patch patches[]={
        {0x56efa2,8,{0xf3,0x0f,0x10,0x86,0x10,0x03,0,0},reinterpret_cast<void*>(&GunPhysicsAdapter),0},
        {0x67c951,5,{0xe8,0x8a,0x6a,0xfc,0xff},reinterpret_cast<void*>(&MeleeLocomotionAngleAdapter),0},
        {0x67c820,6,{0x55,0x8b,0xec,0x83,0xe4,0xf0},reinterpret_cast<void*>(&MeleePhysicsAdapter),0},
        {0x67d5e0,6,{0x55,0x8b,0xec,0x83,0xe4,0xf0},reinterpret_cast<void*>(&MeleePhysicsVariantAdapter),0},
        {0x41bce0,6,{0x55,0x8b,0xec,0x83,0xe4,0xf0},reinterpret_cast<void*>(&ControllerAdapter),0},
        {0x41b644,6,{0x8b,0x0d,0x70,0x35,0x21,0x01},reinterpret_cast<void*>(&ActionReturnAdapter),0},
        {0x6a9b5b,13,{0xf6,0x80,0x68,0x09,0,0,1,0x0f,0x84,0x0e,2,0,0},reinterpret_cast<void*>(&AimPoseAdapter),0},
        {0x6a9c4b,5,{0xe8,0x40,0xc0,0xf5,0xff},reinterpret_cast<void*>(&ApplyAimPoseAdapter),0},
        {0x6a5927,5,{0xe8,0xd4,0xe3,0x4,0x0},reinterpret_cast<void*>(&ThrowVelocityAdapter),0},
        {0x653c54,5,{0xe8,0x07,0xcd,0x1d,0x00},reinterpret_cast<void*>(&inventory_assist::WorldPickupAdapter),0},
        {0x6b2d35,5,{0xe8,0xd6,0xe3,0xec,0xff},reinterpret_cast<void*>(&GestureSampleAdapter),0},
        {0x6b96b9,10,{0x8b,0x4e,0x20,0x83,0xb9,0x5c,0x09,0,0,0},reinterpret_cast<void*>(&GestureEventAdapter),0},
        {0x6b5e26,9,{0x8b,0x46,0x20,0x8b,0x98,0x5c,0x09,0,0},reinterpret_cast<void*>(&GestureMatchAdapter),0},
        {0x6b9882,7,{0x8b,0xc6,0xe8,0x37,0x0b,0,0},reinterpret_cast<void*>(&AutomaticFirstPersonAdapter),0},
        {0x6aa0dd,5,{0xe8,0x7e,0x6f,0xfe,0xff},reinterpret_cast<void*>(&FirstPersonPredicateAdapter),0}};
    constexpr unsigned patchCount=static_cast<unsigned>(sizeof(patches)/sizeof(patches[0]));
    for(const auto& p:patches)if(memcmp(reinterpret_cast<void*>(p.address),p.original,p.size)){enabled=false;Log("Modern controls signature mismatch at%08lx",p.address);return;}
    unsigned writable=0;
    for(auto& p:patches){if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.size,PAGE_EXECUTE_READWRITE,&p.protection))break;++writable;}
    if(writable!=patchCount){for(int n=static_cast<int>(writable)-1;n>=0;--n){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[n].address),patches[n].size,patches[n].protection,&ignored);}enabled=false;return;}
    for(auto& p:patches){auto* site=reinterpret_cast<unsigned char*>(p.address);site[0]=(p.address==0x67c951||p.address==0x6a9c4b||p.address==0x6b2d35||p.address==0x6a5927||p.address==0x653c54||p.address==0x6aa0dd)?0xe8:0xe9;*reinterpret_cast<DWORD*>(site+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(p.replacement)-p.address-5);memset(site+5,0x90,p.size-5);FlushInstructionCache(GetCurrentProcess(),site,p.size);}
    for(int n=static_cast<int>(patchCount)-1;n>=0;--n){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[n].address),patches[n].size,patches[n].protection,&ignored);}
    Log("Modern controls native Q tap/hold and scoped combat action bridge installed");
}
}
extern "C" BOOL WINAPI AITDGetCombatAimState(CombatAimStateV1* output,DWORD bytes){
    if(!output||bytes<sizeof(CombatAimStateV1))return FALSE;
    const DWORD tick=static_cast<DWORD>(InterlockedCompareExchange(&modern_combat::aimPublishedTick,0,0));
    const DWORD now=GetTickCount();
    DWORD foregroundProcess=0;const HWND window=GetForegroundWindow();GetWindowThreadProcessId(window,&foregroundProcess);
    const bool focused=foregroundProcess==GetCurrentProcessId()&&!IsIconic(window);
    const bool thirdPerson=InterlockedCompareExchange(&modern_combat::aimPublished,0,0)!=0&&now-tick<=100&&
        (GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
    bool firstPerson=false;
    __try {if(focused&&modern_combat::firstPersonSightProvider)firstPerson=modern_combat::firstPersonSightProvider();}
    __except(EXCEPTION_EXECUTE_HANDLER){firstPerson=false;}
    const bool active=focused&&(thirdPerson||firstPerson);
    const CombatAimStateV1 snapshot{sizeof(CombatAimStateV1),1,active?1u:0u,firstPerson?now:tick,.5f,.5f,{0,0}};
    __try {memcpy(output,&snapshot,sizeof(snapshot));return TRUE;}
    __except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}
}
#endif
