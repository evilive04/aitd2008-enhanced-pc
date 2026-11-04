// Included inside modern_combat after NativeContext and CallInventoryQueue.
#include "inventory_assist_policy.h"
namespace inventory_assist {
static State state{};
static void* nativeQueueEntry=reinterpret_cast<void*>(0x830960);
static unsigned traceCount=0,pickupBeforeCount=0,pickupAfterCount=0,lastPickupReason=0;
static void Trace(const char* event,const Frame* f=nullptr,const Command* command=nullptr){
 if(traceCount++>=160)return;
 Log("inventory_assist event=%s phase=%u prev=%08x/%08x/%08x sig=%08x/%08x/%u lamp=%08x/%08x/%u live=%08x weak=%08x pickup=%u reject=%u calls=%u/%u eligible=%u queue=%u hands=%08x/%08x/%08x cmd=%u/%u/%08x",event,
  static_cast<unsigned>(state.phase),state.previousInstance.token,state.previousInstance.low,state.previousInstance.high,state.previous.definition,state.previous.kind,state.previous.hand,
  state.lamp.definition,state.lamp.kind,state.lamp.hand,state.lampInstance.token,displacedReference[0],state.pickup.valid?1u:0u,lastPickupReason,pickupBeforeCount,pickupAfterCount,
  f&&f->eligible?1u:0u,f?f->queue:~0u,f?f->left.key.token:0u,f?f->right.key.token:0u,f?f->both.key.token:0u,command?command->operation:~0u,command?command->slot:~0u,command?command->item.token:0u);
 if(f)for(unsigned n=0;n<12;++n){const auto& i=f->stock[n];if(i&&(i.lamp||state.previous.Matches(i)||state.pickup.Matches(i)))
  Log("inventory_assist stock=%u key=%08x/%08x/%08x def=%08x kind=%08x hand=%u rev=%08x lamp=%u throwable=%u",n,i.key.token,i.key.low,i.key.high,i.definition,i.kind,i.hand,i.revision,i.lamp?1u:0u,i.throwable?1u:0u);}
}
static Key Identity(const unsigned char* actor){const auto k=OwnedItemKey(actor);return {k.token,k.low,k.high};}
static Item CaptureItem(unsigned char* actor){
 if(!actor||(Word(actor,0x504)&0x80u))return {};Item i{};i.key=Identity(actor);i.definition=Word(actor,0x4f8);i.kind=Word(actor,0x58c);i.hand=Word(actor,0x4fc);
 i.lamp=IsLamp(actor);i.pistol=IsPistol(actor);
 // Bounded native composite-list metadata detects stock additions. Quantity
 // changes invisible to this snapshot conservatively do not trigger auto-equip.
 if(auto* parts=Pointer(actor,0x510)){const unsigned count=Word(parts,0xc);auto* list=Pointer(parts,0x10);if(count<=64&&(!count||list)){
  unsigned hash=2166136261u;for(unsigned k=0;k<count*12;++k)hash=(hash^list[k])*16777619u;i.revision=hash^count;
 }}
 // Authenticated HLBO/HLLU bottle classes. Two-hand HHDL objects cannot share
 // a hand with a pistol; no speculative hand rewrite or class conversion.
 const unsigned kind=i.kind&0xffffu;i.throwable=i.hand==0&&(kind==2||kind==8)&&!i.lamp&&!i.pistol;return i;
}
static bool Snapshot(unsigned char* hero,unsigned char* inventory,Frame& out){
 if(!hero||hero!=*reinterpret_cast<unsigned char**>(0xfd61ec)||Word(hero,0)!=0xc834c0||!inventory||Pointer(hero,0x810)!=inventory||Pointer(inventory,0x754)!=hero)return false;
 auto* hands=Pointer(hero,0xf9c);if(!hands||Pointer(hands,0)!=hero)return false;
 out={};out.tick=GetTickCount();out.hero=Identity(hero);out.inventory=Identity(inventory);out.queue=Word(inventory,0x81c);if(out.queue>7)return false;
 out.left=CaptureItem(Pointer(hands,4));out.right=CaptureItem(Pointer(hands,0x14));out.both=CaptureItem(Pointer(hands,0x24));
 if(auto* stock=Pointer(inventory,0x4f0))for(unsigned i=0;i<12;++i)out.stock[i]=CaptureItem(Pointer(stock,4+i*20));
 // Native42CF50 registration clears this weak reference on actor destruction.
 // Its pointer is never taken from a remembered token or speculative registry.
 auto* remembered=reinterpret_cast<unsigned char*>(displacedReference[0]);
 if(remembered&&state.previousInstance.token==reinterpret_cast<unsigned>(remembered)){
  auto item=CaptureItem(remembered);if(item.key==state.previousInstance&&state.previous.Matches(item))out.remembered=item;
 }
 unsigned char *validatedHero=nullptr,*validatedInventory=nullptr;out.eligible=NativeContext(HeroController(),&validatedHero,&validatedInventory)&&validatedHero==hero&&validatedInventory==inventory;
 return true;
}
static unsigned char* Resolve(unsigned char* hero,unsigned char* inventory,Key item){
 if(!item)return nullptr;Frame fresh{};if(!Snapshot(hero,inventory,fresh)||!fresh.eligible||fresh.queue)return nullptr;
 if(fresh.remembered.key==item&&fresh.remembered)return reinterpret_cast<unsigned char*>(displacedReference[0]);
 auto* hands=Pointer(hero,0xf9c);for(unsigned h=0;h<3;++h){auto* actor=Pointer(hands,4+h*16);if(actor&&Identity(actor)==item)return actor;}
 if(auto* stock=Pointer(inventory,0x4f0))for(unsigned s=0;s<12;++s){auto* actor=Pointer(stock,4+s*20);if(actor&&Identity(actor)==item)return actor;}
 return nullptr;
}
static bool Execute(unsigned char* hero,unsigned char* inventory,const Frame& observed,const Command& command){
 Frame fresh{};if(!command||!Snapshot(hero,inventory,fresh)||!fresh.eligible||fresh.queue||fresh.hero!=observed.hero||fresh.inventory!=observed.inventory)return false;
 if(fresh.left.key!=observed.left.key||fresh.right.key!=observed.right.key||fresh.both.key!=observed.both.key)return false;
 auto* actor=Resolve(hero,inventory,command.item);if(!actor||Word(actor,0x4fc)!=command.slot)return false;
 auto* hands=Pointer(hero,0xf9c);
 if(command.operation==0){if(command.slot!=0||Pointer(hands,4)!=actor||!IsLamp(actor))return false;}
 else if(command.operation==1){if(Pointer(hands,0x24)||(command.slot==0&&Pointer(hands,4))||(command.slot==1&&Pointer(hands,0x14))||(command.slot==2&&(Pointer(hands,4)||Pointer(hands,0x14)))){
   // Only the explicit Q equip may replace the remembered hand/two-hand item.
   if(!IsLamp(actor)||state.phase!=State::AwaitLamp)return false;
  }}else return false;
 // Native830B50 sees the old empty-hand handler for several frames after
 // unequip. Its ordinary equip request otherwise fails with status4 before
 // the handler is retired. Native transition bit8 retains the queued request
 // until that handler is ready/removed, then runs the original69AE60 equip.
 // This never bypasses the owner/empty-slot/current-item checks above.
 return CallInventoryQueue(inventory,command.operation,actor,command.slot,command.operation==1?8u:0u);
}
static void Reset(){if(state.phase!=State::Idle||state.pickup.valid)Trace("reset");state.Clear();AssignDisplacedReference(nullptr);}
static void Tick(unsigned char* hero,unsigned char* inventory){
 __try{Frame f{};if(!Snapshot(hero,inventory,f)){Reset();return;}const State before=state;const Command command=state.Update(f);const bool accepted=!command||Execute(hero,inventory,f,command);if(command&&!accepted)state=before;
  if(command||before.phase!=state.phase||before.pickup.valid!=state.pickup.valid)Trace(accepted?"tick":"execute-rejected",&f,&command);
  if(before.phase!=State::Idle&&state.phase==State::Idle)AssignDisplacedReference(nullptr);
 }__except(EXCEPTION_EXECUTE_HANDLER){Reset();}
}
static void Toggle(unsigned char* hero,unsigned char* inventory){
 __try{Tick(hero,inventory);Frame f{};if(!Snapshot(hero,inventory,f)){Trace("toggle-snapshot-failed");return;}const State before=state;Trace("toggle-before",&f);const Command command=state.Toggle(f);
  if(command&&command.operation==1&&state.phase==State::AwaitLamp){auto* hands=Pointer(hero,0xf9c);auto* previous=Pointer(hands,0x24);if(!previous)previous=Pointer(hands,4);AssignDisplacedReference(previous);}
  const bool accepted=!command||Execute(hero,inventory,f,command);if(command&&!accepted){state=before;if(before.phase==State::Idle)AssignDisplacedReference(nullptr);}Trace(accepted?"toggle-after":"toggle-execute-rejected",&f,&command);
 }__except(EXCEPTION_EXECUTE_HANDLER){Reset();}
}
// Only the authentic world-acquisition CALL653C54 uses this adapter. Native
//830B50 operation2 performs storage and schedules operation1 only on success.
static volatile LONG worldPickupSeen=0,worldPickupEnabled=0,lastWorldPickupReason=0;
static volatile DWORD lastWorldPickup[8]{};
static unsigned __cdecl WorldPickupFlags(unsigned char* inventory,unsigned operation,unsigned char* actor,unsigned slot,unsigned flags){
 InterlockedIncrement(&worldPickupSeen);lastWorldPickupReason=0;
 __try{
  auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);Frame f{};
  if(!Snapshot(hero,inventory,f)){lastWorldPickupReason=1;return flags;}
  lastWorldPickup[0]=operation;lastWorldPickup[1]=reinterpret_cast<DWORD>(actor);lastWorldPickup[2]=slot;lastWorldPickup[3]=flags;
  lastWorldPickup[4]=f.left.key.token;lastWorldPickup[5]=f.right.key.token;lastWorldPickup[6]=f.both.key.token;lastWorldPickup[7]=flags;
  unsigned char* ordinary=nullptr;
  // This callsite authenticates a real selected-world-item acquisition, not
  // an inventory equip. The common world guard includes cabinet interactions
  // while retaining inventory/healing/cinematic/focus/menu/owner restrictions.
  if(!WorldInteractionContext(HeroController(),&ordinary)||ordinary!=hero||hero[0xc13]||
     Word(hero,0xa64)!=0||Pointer(hero,0xa70)!=actor||!actor||!Word(actor,0x4f8)){lastWorldPickupReason=2;return flags;}
  if(state.phase!=State::Idle){lastWorldPickupReason=3;return flags;}
  f.eligible=true;const Item item=CaptureItem(actor);
  if(!AutoEquipAcquisition(f,item,operation,slot,flags)){lastWorldPickupReason=4;return flags;}
  const unsigned result=flags|0x20u;lastWorldPickup[7]=result;lastWorldPickupReason=5;InterlockedIncrement(&worldPickupEnabled);
  Log("inventory_assist world-acquire op=%u item=%p def=%u kind=%08x hand=%u flags=%08x->%08x hands=%08x/%08x/%08x",operation,actor,item.definition,item.kind,item.hand,flags,result,f.left.key.token,f.right.key.token,f.both.key.token);
  return result;
 }__except(EXCEPTION_EXECUTE_HANDLER){lastWorldPickupReason=6;return flags;}
}
[[maybe_unused]] static __declspec(naked) void WorldPickupAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push [ebp+14h]
  push [ebp+10h]
  push [ebp+0ch]
  push [ebp+8]
  push ecx
  call WorldPickupFlags
  add esp,20
  mov [ebp+14h],eax
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  jmp dword ptr [nativeQueueEntry]
 }
}
struct PickupObservation {Frame before{};Item item{};bool valid{};};
static PickupObservation pending{};
static void __cdecl BeforePickup(unsigned char* inventory,unsigned operation,unsigned char* item,unsigned slot,unsigned flags){
 ++pickupBeforeCount;lastPickupReason=0;pending={};state.ClearPickup();__try{
  if(traceCount<160)Log("inventory_assist native-before op=%u item=%p slot=%u flags=%08x raw=%08x/%08x/%u/%08x",operation,item,slot,flags,item?Word(item,0x4f8):0,item?Word(item,0x58c):0,item?Word(item,0x4fc):~0u,item?Word(item,0x504):0);
  if(operation!=1||!item||slot!=0||(flags&~0x1120u)){lastPickupReason=1;Trace("pickup-arguments");return;}
  auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);Frame f{};if(!Snapshot(hero,inventory,f)){lastPickupReason=2;Trace("pickup-snapshot");return;}if(!f.eligible||f.queue>=7||hero[0xc13]){lastPickupReason=3;Trace("pickup-context",&f);return;}
  const auto candidate=CaptureItem(item);if(candidate.hand!=slot||!candidate.throwable){lastPickupReason=4;Trace("pickup-type",&f);return;}pending={f,candidate,true};Trace("pickup-before",&f);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void __cdecl AfterPickup(unsigned accepted){
 ++pickupAfterCount;const auto observation=pending;pending={};if(!(accepted&255u)||!observation.valid){lastPickupReason=5;Trace("pickup-native-rejected");return;}
 __try{auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);Frame current{};auto* inventory=hero?Pointer(hero,0x810):nullptr;
  if(!Snapshot(hero,inventory,current)||current.hero!=observation.before.hero||current.inventory!=observation.before.inventory||!current.eligible){lastPickupReason=6;Trace("pickup-owner-after");return;}
  // Queue acceptance alone is not successful acquisition. State.Update also
  // requires a new stock identity/revision, so rejected/full pickups cannot
  // equip a matching bottle that was already present before the request.
  state.Picked(observation.before,observation.item);
  lastPickupReason=state.pickup.valid?7u:8u;Trace("pickup-after",&current);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
// Redirect only CALL83342C (E8 2F D5 FF FF), never the830960 entry.
// Custom EAX metadata,ECX inventory, four stack arguments and ret16 preserved.
[[maybe_unused]] static __declspec(naked) void QueueAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push [ebp+14h]
  push [ebp+10h]
  push [ebp+0ch]
  push [ebp+8]
  push ecx
  call BeforePickup
  add esp,20
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  push [ebp+14h]
  push [ebp+10h]
  push [ebp+0ch]
  push [ebp+8]
  call dword ptr [nativeQueueEntry]
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push eax
  call AfterPickup
  add esp,4
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  ret 16
 }
}
} // namespace inventory_assist
