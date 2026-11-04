// Exact-r29 AVM GetURL FSCommand handler. Other commands retain the original ABI.
#include "ui_sound.h"
namespace ui_sound_native {
static bool enabled=false;static volatile LONG installed=0;static void* original=nullptr;
static unsigned suppressed=0;using Player=BOOL(WINAPI*)(DWORD);static Player player=nullptr;static DWORD lookupTick=0;
static DWORD Word(const void* p,unsigned at=0){return *reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(p)+at);}
static bool MovieRoot(DWORD root,DWORD slot,DWORD callback){
 auto* movie=*reinterpret_cast<BYTE**>(slot);if(!movie||Word(movie,12)!=callback||!movie[0x65])return false;
 auto* handle=reinterpret_cast<void*>(Word(movie,0x24));return handle&&Word(handle)==root;
}
static bool TrustedRoot(DWORD root){
 if(!root)return false;
 if(MovieRoot(root,0xfe1518,0xfe1520)||MovieRoot(root,0xfe15f0,0xfe15f8))return true;
 if(*reinterpret_cast<BYTE*>(0xff4b7f)==1&&Word(reinterpret_cast<void*>(0xff47a0))==0xc79974&&MovieRoot(root,0xff4790,0xff47a0))return true;
 const auto* generic=*reinterpret_cast<void**>(0xff4378);
 return *reinterpret_cast<BYTE*>(0xff437c)==1&&generic&&Word(generic)==root;
}
static Player FindPlayer(){
#ifdef AITD_UI_SOUND_TEST
 return TestPlayer;
#else
 if(player)return player;const DWORD now=GetTickCount();if(lookupTick&&now-lookupTick<1000)return nullptr;lookupTick=now;
 HMODULE module=GetModuleHandleW(L"d3d9.dll");if(!module)return nullptr;
 wchar_t own[MAX_PATH]{},other[MAX_PATH]{};
 if(!GetModuleFileNameW(selfModule,own,MAX_PATH)||!GetModuleFileNameW(module,other,MAX_PATH))return nullptr;
 auto* a=wcsrchr(own,L'\\');auto* b=wcsrchr(other,L'\\');if(!a||!b)return nullptr;*a=0;*b=0;if(_wcsicmp(own,other))return nullptr;
 auto candidate=reinterpret_cast<Player>(GetProcAddress(module,"AITDPlayUiSound"));if(!candidate)return nullptr;
 HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(candidate),&pinned))return nullptr;
 player=candidate;return player;
#endif
}
static BOOL __cdecl Replace(const char* command)noexcept{
 const DWORD error=GetLastError();BOOL result=FALSE;
 __try{
  bool terminated=false;if(command)for(unsigned i=0;i<80;++i)if(!command[i]){terminated=true;break;}
  if(enabled&&terminated&&!strncmp(command,"FSCommand:sndp:",15)&&TrustedRoot(*reinterpret_cast<DWORD*>(0xfd63e0))){
   const auto cue=ui_sound::NativeCue(command+15);auto target=FindPlayer();
   if(static_cast<unsigned>(cue)&&target&&target(static_cast<DWORD>(cue))){++suppressed;result=TRUE;}
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){result=FALSE;}
 SetLastError(error);return result;
}
static __declspec(naked) void Adapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push eax
  call Replace
  add esp,4
  test eax,eax
  jnz handled
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  jmp dword ptr [original]
 handled:
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  ret
 }
}
[[maybe_unused]] static void Install(){
 if(!enabled||InterlockedCompareExchange(&installed,1,0))return;
 constexpr DWORD site=0x923c10;const BYTE bytes[]={0x56,0x57,0x8b,0xf8,0xa1,0xe0,0x63,0xfd,0};
 if(memcmp(reinterpret_cast<void*>(site),bytes,sizeof bytes)){enabled=false;return;}
 auto* trampoline=static_cast<BYTE*>(VirtualAlloc(nullptr,14,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!trampoline){enabled=false;return;}
 memcpy(trampoline,bytes,sizeof bytes);trampoline[9]=0xe9;const DWORD back=site+9-static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(trampoline+14));memcpy(trampoline+10,&back,4);
 DWORD old{};if(!VirtualProtect(trampoline,14,PAGE_EXECUTE_READ,&old)){VirtualFree(trampoline,0,MEM_RELEASE);enabled=false;return;}
 original=trampoline;
 if(!VirtualProtect(reinterpret_cast<void*>(site),sizeof bytes,PAGE_EXECUTE_READWRITE,&old)){VirtualFree(trampoline,0,MEM_RELEASE);original=nullptr;enabled=false;return;}
 BYTE patch[9];memset(patch,0x90,sizeof patch);patch[0]=0xe9;const DWORD relative=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&Adapter)-site-5);memcpy(patch+1,&relative,4);
 memcpy(reinterpret_cast<void*>(site),patch,sizeof patch);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(site),sizeof patch);
 DWORD ignored{};VirtualProtect(reinterpret_cast<void*>(site),sizeof patch,old,&ignored);
}
}
