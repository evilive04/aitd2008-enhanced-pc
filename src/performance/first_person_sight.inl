// Shared native pistol sight replacement in first person and rope aiming.
// Rope movement/aiming remains native; only its beam/impact/dot is replaced.
namespace first_person_sight {
static bool enabled=false;
static volatile LONG attempted=0,suppressed=0;
static void *nativeBeam=reinterpret_cast<void*>(0x6ffc90),*nativeImpact=reinterpret_cast<void*>(0x7000b0),
    *nativeDotContinue=reinterpret_cast<void*>(0x6b0976);
static __declspec(naked) bool __cdecl NativeSightFlag(const void*){
 __asm {
  mov eax,[esp+4]
  push 80000000h
  push 80000h
  mov edx,4584b0h
  call edx
  ret
 }
}
static bool SightContext(unsigned char** outputHero,unsigned char** outputInventory){
 using namespace modern_combat;
 unsigned char *hero=nullptr,*inventory=nullptr;
 if(NativeContext(HeroController(),&hero,&inventory)&&(Word(hero,0x968)&1)){
  *outputHero=hero;*outputInventory=inventory;return true;
 }
 // 64DA00 case20 authenticates CTX_IS_ON_ROPE with the HRPA descriptor.
 // 6AF997 tests this same main-handler state flag before drawing its sight.
 if(!WorldInteractionContext(HeroController(),&hero))return false;
 auto* main=Pointer(hero,0x25c);
 if(!main||Pointer(main,0x20)!=hero||!HandlerClass(main,0x48525041)||!NativeSightFlag(main))return false;
 inventory=Pointer(hero,0x810);
 auto* overlay=Pointer(hero,0xf78);
 auto* hands=Pointer(hero,0xf9c);
 if(!inventory||Pointer(inventory,0x754)!=hero||!overlay||Word(overlay,0)!=0xc89e68||
    Pointer(overlay,0x20)!=hero||!hands||Pointer(hands,0)!=hero||
    !Pointer(hands,0x14)||Pointer(overlay,0x118)!=Pointer(hands,0x14))return false;
 const DWORD state=Word(overlay,0xb0);
 if(state==0||state==1||state==3||state==4)return false;
 *outputHero=hero;*outputInventory=inventory;return true;
}
static bool __cdecl Weapon(unsigned char* weapon){
 __try {
  using namespace modern_combat;
  if(!first_person_sight::enabled||!weapon)return false;
  unsigned char *hero=nullptr,*inventory=nullptr;
  if(!SightContext(&hero,&inventory)||Word(inventory,0x81c))return false;
  auto* hands=Pointer(hero,0xf9c);
  return hands&&Pointer(hands,0)==hero&&Pointer(hands,0x14)==weapon&&IsPistol(weapon)&&NativePistolBehavior(weapon);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool Visible(){
 __try {using namespace modern_combat;auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
  auto* hands=hero?Pointer(hero,0xf9c):nullptr;return hands&&Weapon(Pointer(hands,0x14));
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool __cdecl Overlay(unsigned char* overlay){
 __try {using namespace modern_combat;auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
  if(!hero||!overlay||Pointer(overlay,0x20)!=hero||Pointer(hero,0xf78)!=overlay)return false;
  return Visible();
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static __declspec(naked) void BeamAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push esi
  call Weapon
  add esp,4
  test al,al
  jz beamNative
  lock inc dword ptr [suppressed]
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  ret
 beamNative:
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  jmp dword ptr [nativeBeam]
 }
}
static __declspec(naked) void ImpactAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push [ebp+8]
  call Weapon
  add esp,4
  test al,al
  jz impactNative
  lock inc dword ptr [suppressed]
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  ret 4
 impactNative:
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  jmp dword ptr [nativeImpact]
 }
}
static __declspec(naked) void DotAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push eax
  call Overlay
  add esp,4
  test al,al
  jz dotNative
  lock inc dword ptr [suppressed]
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  ret
 dotNative:
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  // Original complete prologue; this site is a JMP, not a CALL.
  push ebp
  mov ebp,esp
  and esp,0fffffff0h
  jmp dword ptr [nativeDotContinue]
 }
}
static void Install(){
 if(!enabled||InterlockedCompareExchange(&attempted,1,0))return;
 struct Patch{DWORD address;unsigned size;BYTE signature[6];void* replacement;BYTE opcode;DWORD protection;};
 Patch patches[]={
  {0x6af9f7,5,{0xe8,0x94,0x02,0x05,0x00},reinterpret_cast<void*>(&BeamAdapter),0xe8,0},
  {0x6afa0c,5,{0xe8,0x9f,0x06,0x05,0x00},reinterpret_cast<void*>(&ImpactAdapter),0xe8,0},
  {0x6b0970,6,{0x55,0x8b,0xec,0x83,0xe4,0xf0},reinterpret_cast<void*>(&DotAdapter),0xe9,0}};
 for(const auto& p:patches)if(memcmp(reinterpret_cast<void*>(p.address),p.signature,p.size)){enabled=false;Log("First-person sight signature mismatch");return;}
 unsigned writable=0;for(auto& p:patches){if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.size,PAGE_EXECUTE_READWRITE,&p.protection))break;++writable;}
 if(writable!=3){for(int n=static_cast<int>(writable)-1;n>=0;--n){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[n].address),patches[n].size,patches[n].protection,&ignored);}enabled=false;return;}
 for(auto& p:patches){auto* at=reinterpret_cast<BYTE*>(p.address);at[0]=p.opcode;*reinterpret_cast<DWORD*>(at+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(p.replacement)-p.address-5);memset(at+5,0x90,p.size-5);FlushInstructionCache(GetCurrentProcess(),at,p.size);}
 // Beam and impact occupy the same page; unwind in reverse to restore its
 // original protection rather than the second acquisition's writable state.
 for(int n=2;n>=0;--n){auto& p=patches[n];DWORD ignored;VirtualProtect(reinterpret_cast<void*>(p.address),p.size,p.protection,&ignored);}
 modern_combat::firstPersonSightProvider=&Visible;
 Log("Native first-person and owned rope-aim pistol beam/impact/dot replaced by shared crosshair");
}
}
