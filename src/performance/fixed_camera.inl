// Change only the ordinary gameplay camera ranking. Native lists, activation,
// blend callbacks, timeline state and cinematic cameras retain their lifecycle.
#include "fixed_camera_policy.h"
namespace fixed_camera {
static bool available=false,ignore=false,installed=false;
static volatile LONG installAttempt=0;
static wchar_t path[MAX_PATH]{};
static fixed_camera_policy::KeyEdge key;
static void* nativeSelect=reinterpret_cast<void*>(0x45ebe0);
static void Configure(const wchar_t* config,bool supported){
 available=supported;ignore=available&&ConfigFlag(L"Camera",L"DisableFixedAngles",false,config);
 if(config)wcsncpy_s(path,config,_TRUNCATE);
}
static bool Gameplay(){
 __try {
  const auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
  const auto* camera=*reinterpret_cast<unsigned char**>(0xfd5d98);
  if(!hero||!camera||*reinterpret_cast<const DWORD*>(hero)!=0xc834c0||
   *reinterpret_cast<const DWORD*>(camera)!=0xc67670||
   *reinterpret_cast<unsigned char* const*>(camera+0x3e8)!=hero)return false;
  const auto* controller=*reinterpret_cast<unsigned char* const*>(hero+0x95c);
  const auto* behavior=*reinterpret_cast<unsigned char* const*>(hero+0x25c);
  const auto* cinematic=*reinterpret_cast<unsigned char* const*>(hero+0x280);
  if(!controller||!behavior||OrbitNativeMenu(hero))return false;
  return fixed_camera_policy::Gameplay(*reinterpret_cast<DWORD*>(0xff4290),
   *reinterpret_cast<DWORD*>(0xff4390),*reinterpret_cast<DWORD*>(0xff5508),
   *reinterpret_cast<DWORD*>(0xfd8bd4),*reinterpret_cast<DWORD*>(0xfda5c8),
   *reinterpret_cast<const DWORD*>(controller+4),cinematic&&cinematic[0x204],
   *reinterpret_cast<BYTE*>(0xff4b7f)!=0,hero[0xc19]||(*reinterpret_cast<const DWORD*>(hero+0x968)&1),
   OrbitBehaviorEligible(*reinterpret_cast<const DWORD*>(behavior),*reinterpret_cast<const unsigned*>(behavior+0xb0)));
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool Active(){return available&&installed&&ignore&&Gameplay();}
static void Keyboard(void* data){
 if(!available||!installed||!data)return;
 auto* keys=static_cast<unsigned char*>(data);
 DWORD pid{};const HWND window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);
 const bool allowed=pid==GetCurrentProcessId()&&!IsIconic(window)&&Gameplay();
 const bool down=(keys[DIK_HOME]&0x80)!=0;
 const bool modified=((keys[DIK_LSHIFT]|keys[DIK_RSHIFT]|keys[DIK_LCONTROL]|keys[DIK_RCONTROL]|
  keys[DIK_LMENU]|keys[DIK_RMENU]|keys[DIK_LWIN]|keys[DIK_RWIN])&0x80)!=0;
 if(allowed&&down&&!modified)keys[DIK_HOME]=0;
 if(!key.Update(down,allowed,modified))return;
 ignore=!ignore;
 const bool saved=path[0]&&WritePrivateProfileStringW(L"Camera",L"DisableFixedAngles",ignore?L"true":L"false",path);
 Log("Fixed gameplay camera Home ignore=%d saved=%d error=%lu",ignore,saved,saved?0:GetLastError());
}
static unsigned char* __cdecl Select(unsigned char* start,unsigned char* original){
 if(auto* vehicle=vehicle_camera::Select(start,original))return vehicle;
 if(!Active())return original;
 __try {
  const auto* camera=*reinterpret_cast<unsigned char**>(0xfd5d98);
  unsigned char* selected=nullptr;unsigned count=0;
  for(auto* node=start;node;node=*reinterpret_cast<unsigned char**>(node+0x18)){
   if(++count>64)return original;
   const auto* actor=*reinterpret_cast<unsigned char**>(node);
   if(!actor)return original;
   if(!(node[0xc]&1))continue;
   if(actor==camera){selected=node;continue;}
   const auto* definition=*reinterpret_cast<unsigned char* const*>(actor+0x28);
   const auto* cinematic=*reinterpret_cast<unsigned char* const*>(actor+0x280);
   const auto* settings=*reinterpret_cast<unsigned char* const*>(actor+0x38c);
   // Native DisableScenaricCameras preserves definition flag8. Protect the
   // same special family and any camera explicitly owned by a live timeline.
   //45EC40 dispatches settings+0C bit100 through native events6/7;
   //650710 clears/restores hero+95C for that actual input-disabled camera.
   // Retain its ranking before the entry callback runs, not only afterwards
   // when Gameplay already sees the disabled controller. Never clear the lock.
   if(!definition||!settings||(*reinterpret_cast<const DWORD*>(settings+0xc)&0x100)||
    (*reinterpret_cast<const DWORD*>(definition+0x20)&8)||
    (cinematic&&cinematic[0x204]))return original;
  }
  return selected?selected:original;
 }__except(EXCEPTION_EXECUTE_HANDLER){return original;}
}
static __declspec(naked) void SelectAdapter(){__asm {
 push ebp
 mov ebp,esp
 push edx
 call dword ptr [nativeSelect]
 pushfd
 pushad
 sub esp,528
 and esp,0fffffff0h
 fxsave [esp]
 push eax
 push [ebp-4]
 call Select
 add esp,8
 mov [ebp-12],eax
 mov [ebp-20],eax
 fxrstor [esp]
 lea esp,[ebp-40]
 popad
 popfd
 mov esp,ebp
 pop ebp
 ret
}}
static void Install(){
 if(!available||InterlockedCompareExchange(&installAttempt,1,0))return;
 auto* site=reinterpret_cast<unsigned char*>(0x45ecc0);
 const unsigned char expected[]={0xe8,0x1b,0xff,0xff,0xff};
 if(memcmp(site,expected,sizeof expected)){available=false;Log("Fixed camera ranking signature mismatch");return;}
 DWORD protection{},ignored{};
 if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&protection)){available=false;return;}
 site[0]=0xe8;*reinterpret_cast<DWORD*>(site+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&SelectAdapter)-0x45ecc5);
 FlushInstructionCache(GetCurrentProcess(),site,5);VirtualProtect(site,5,protection,&ignored);installed=true;
 Log("Fixed gameplay camera ranking installed ignore=%d Home=toggle; scripted/input-locked cameras native",ignore);
}
}
