// Included after modern_combat. Exact native inventory tips and world clock.
namespace favorites_native {
static bool enabled=false;
static bool (*uiActiveProvider)()=nullptr;
static volatile LONG installAttempt=0,tipBlocks=0,headerHidden=0,headerMissing=0;
struct ClockState {float current,target,slew;};
static ClockState previous{},written{};static bool slowOwned=false,foreignRate=false;
static DWORD Word(const void* p,unsigned offset){return *reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(p)+offset);}
static unsigned char* Pointer(const void* p,unsigned offset){return *reinterpret_cast<unsigned char*const*>(static_cast<const unsigned char*>(p)+offset);}
static bool Scope(unsigned char* inventory){
 if(!enabled||!inventory)return false;auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
 return hero&&Word(hero,0)==0xc834c0&&Pointer(hero,0x810)==inventory&&Pointer(inventory,0x754)==hero;
}
static ClockState ReadClock(){ClockState state{};memcpy(&state,reinterpret_cast<void*>(0xf503a8),sizeof(state));return state;}
static bool ValidClock(const ClockState& s){return std::isfinite(s.current)&&std::isfinite(s.target)&&std::isfinite(s.slew)&&s.current>=0&&s.current<=100&&s.target>=0&&s.target<=100&&s.slew>=0&&s.slew<=10000;}
static bool Focused(){
#ifdef AITD_FAVORITES_NATIVE_TEST
 return TestFocused();
#else
 DWORD pid=0;const HWND window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);return pid==GetCurrentProcessId()&&!IsIconic(window);
#endif
}
static void SetRate(float rate){
#ifdef AITD_FAVORITES_NATIVE_TEST
 TestNativeRate(rate,0.f);
#else
 reinterpret_cast<void(__stdcall*)(float,float)>(0x41a6f0)(rate,0.f);
#endif
}
static void EndSlow(){
 __try{if(slowOwned){const auto actual=ReadClock();if(!memcmp(&actual,&written,sizeof(actual)))memcpy(reinterpret_cast<void*>(0xf503a8),&previous,sizeof(previous));}slowOwned=false;foreignRate=false;}
 __except(EXCEPTION_EXECUTE_HANDLER){slowOwned=false;foreignRate=false;}
}
static bool BeginSlow(float rate=.15f){
 __try{
  if(!enabled||!uiActiveProvider||!uiActiveProvider()||!Focused()||!std::isfinite(rate)||rate<.05f||rate>.5f){EndSlow();return false;}
  auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);if(!hero||!Scope(Pointer(hero,0x810))){EndSlow();return false;}
  if(foreignRate)return false;
  if(slowOwned){const auto actual=ReadClock();if(!memcmp(&actual,&written,sizeof(actual)))return true;slowOwned=false;foreignRate=true;return false;}
  previous=ReadClock();if(!ValidClock(previous))return false;SetRate(rate);written=ReadClock();
  slowOwned=written.current==rate&&written.target==rate&&written.slew==0;return slowOwned;
 }__except(EXCEPTION_EXECUTE_HANDLER){EndSlow();return false;}
}
static void DisableOriginalFavorites(){
 __try{
  auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);if(!enabled||!hero||!Scope(Pointer(hero,0x810)))return;
  // Original configuration: UseFavorites / UseFavoritesInGameFlash. The
  // inventory's forced tutorial type 2 uses its own completion byte.
  auto* options=reinterpret_cast<volatile unsigned char*>(0xfaf224);
  auto* tutorial=reinterpret_cast<volatile unsigned char*>(0xfd96a2);
  if(options[0]<=1&&options[1]<=1&&*tutorial<=1){options[0]=0;options[1]=0;*tutorial=1;}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void Tick(){DisableOriginalFavorites();if((slowOwned||foreignRate)&&(!enabled||!uiActiveProvider||!uiActiveProvider()||!Focused()))EndSlow();}
static bool __cdecl SuppressTip(unsigned char* inventory){__try{if(Scope(inventory)){InterlockedIncrement(&tipBlocks);return true;}}__except(EXCEPTION_EXECUTE_HANDLER){}return false;}
static void* tipContinue=reinterpret_cast<void*>(0x8301d9);
static void* tipFinished=reinterpret_cast<void*>(0x830211);
static void* nativeHeaderText=reinterpret_cast<void*>(0x91f430);
static bool ResolveDisplay(unsigned char* handle,const char* path,DWORD (&binding)[2]){
#ifdef AITD_FAVORITES_NATIVE_TEST
 return TestResolveDisplay(handle,path,binding);
#else
 unsigned result=0;DWORD* bindingOut=binding;
 __asm {
  mov ecx,handle
  mov edx,path
  push bindingOut
  mov eax,91f860h
  call eax
  mov result,eax
 }
 return (result&255)!=0;
#endif
}
static void DisableFavoritesVariable(unsigned char* handle){
#ifdef AITD_FAVORITES_NATIVE_TEST
 TestFavoritesVariable(handle);
#else
 const char* path="/:is_favorites";
 __asm {
  mov eax,handle
  push 0
  push path
  mov ecx,91f4b0h
  call ecx
 }
#endif
}
static void __cdecl HeaderAfter(unsigned char* ui,unsigned char* handle){
 __try{
  auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);if(!hero||!Scope(Pointer(hero,0x810))||!ui||!handle)return;
  auto* movie=Pointer(ui,0x10);if(!movie||!movie[0x65]||Pointer(movie,0x24)!=handle||!Pointer(handle,0))return;
  DisableFavoritesVariable(handle);
  // Colon paths name Flash VARIABLES. A display node must separately resolve
  // by its actual instance path; never treat the variable as a sprite pointer.
  DWORD binding[2]{};if(!ResolveDisplay(handle,"/clip_general/barre_haut/help/help/favorites",binding)||!binding[0]||binding[1]!=Word(handle,0)||binding[0]==Word(handle,0)+200){InterlockedIncrement(&headerMissing);return;}
  auto* node=reinterpret_cast<unsigned char*>(binding[0]);*reinterpret_cast<DWORD*>(node+0x6c)&=~1u;InterlockedIncrement(&headerHidden);
 }__except(EXCEPTION_EXECUTE_HANDLER){InterlockedIncrement(&headerMissing);}
}
static __declspec(naked) void TipAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push ebx
  call SuppressTip
  add esp,4
  test al,al
  jnz suppress
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  mov esi,dword ptr ds:[0fde050h]
  jmp dword ptr [tipContinue]
 suppress:
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  jmp dword ptr [tipFinished]
 }
}
static __declspec(naked) void HeaderAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  push eax
  push [ebp+8]
  call dword ptr [nativeHeaderText]
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push [ebp-4]
  push esi
  call HeaderAfter
  add esp,8
  fxrstor [esp]
  lea esp,[ebp-40]
  popad
  popfd
  mov esp,ebp
  pop ebp
  ret 4
 }
}
static void Install(){
 if(!enabled||InterlockedCompareExchange(&installAttempt,1,0))return;
 struct Patch {DWORD address;unsigned size;unsigned char original[6];void* replacement;DWORD protection;};
 Patch patches[]={{0x8301d3,6,{0x8b,0x35,0x50,0xe0,0xfd,0},reinterpret_cast<void*>(&TipAdapter),0},
  {0x839afb,5,{0xe8,0x30,0x59,0x0e,0},reinterpret_cast<void*>(&HeaderAdapter),0}};
 const unsigned char clock[]={0x51,0xf3,0x0f,0x10,0x44,0x24,0x0c,0x0f,0x57};
 if(memcmp(reinterpret_cast<void*>(0x41a6f0),clock,sizeof(clock))){enabled=false;return;}
 for(const auto& p:patches)if(memcmp(reinterpret_cast<void*>(p.address),p.original,p.size)){enabled=false;return;}
 unsigned count=0;for(auto& p:patches){if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.size,PAGE_EXECUTE_READWRITE,&p.protection))break;++count;}
 if(count==2){for(auto& p:patches){auto* site=reinterpret_cast<unsigned char*>(p.address);site[0]=p.address==0x839afb?0xe8:0xe9;*reinterpret_cast<DWORD*>(site+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(p.replacement)-p.address-5);if(p.size>5)site[5]=0x90;FlushInstructionCache(GetCurrentProcess(),site,p.size);}}
 else enabled=false;
 while(count){auto& p=patches[--count];DWORD ignored;VirtualProtect(reinterpret_cast<void*>(p.address),p.size,p.protection,&ignored);}
}
} // namespace favorites_native
