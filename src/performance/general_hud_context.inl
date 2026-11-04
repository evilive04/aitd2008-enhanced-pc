// Passive General root replacement. No game-clock, input, subtitle or timer writes.
#include "general_hud_state.h"
#include "general_countdown_content.h"
#include <cmath>
// Inventory formatter implementation is included later by volume_input.cpp.
namespace inventory_ui_context{static bool Binding(const char*,wchar_t(&)[256]);}
namespace general_hud_context{
static bool enabled=false,ready=false;static SRWLOCK lock=SRWLOCK_INIT;
static DWORD generation{},readyGeneration{},readyTick{};static GeneralHudStateV1 state{};
struct Owner{DWORD movie{},handle{},root{},subtitle{},chrono{},flags{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Countdown(DWORD root,DWORD scope,const char* key,char suffix,DWORD maximum,DWORD& out){char text[8]{};unsigned value{};if(!phone_parameters_context::Raw(root,scope,key,text,8)||!general_countdown_content::Parse(text,suffix,maximum,value))return false;out=value;return true;}
// Reuse the same exact COPH/CTNR or HDIG/ADIG authority as the actual
// tap/hold input route; no unrelated General exit prompt is relabelled.
static bool CabinetPrompt(BYTE* hero,wchar_t(&key)[256]){
#ifdef AITD_GENERAL_HUD_TEST
 return TestCabinetPrompt(hero,key);
#else
 return modern_combat::enabled&&modern_combat::interactionActive&&
  modern_combat::interactionOwner==reinterpret_cast<DWORD>(hero)&&
  GetTickCount()-modern_combat::interactionTick<=100&&
  modern_combat::CabinetInteractionContext(hero)&&
  inventory_ui_context::Binding("ACTION_LEAVE",key);
#endif
}
static bool Capture(GeneralHudStateV1& s,Owner& owner){
 s={};owner={};if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0xfe14e8))!=0xc77b34||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f))return false;
  const auto load=Word(reinterpret_cast<void*>(0xff4390));if(load&&load!=5)return false;
  auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0||!Word(hero,0x95c))return false;
  const auto inv=*reinterpret_cast<float*>(hero+0x1d88),heal=*reinterpret_cast<float*>(hero+0x1e00);
  if(!std::isfinite(inv)||!std::isfinite(heal)||inv>0||heal>0)return false;
  auto* movie=*reinterpret_cast<BYTE**>(0xfe14e0);if(!movie||Word(movie,12)!=0xfe14e8||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const auto root=Word(handle),scope=root+200;
  DWORD subtitle{},chrono{};if(!root||Word(reinterpret_cast<void*>(scope),0x60)!=1||!phone_context::OptionalZero(root,scope,"fake")||
   !phone_context::Resolve(handle,"/subtitle",7,subtitle)||!phone_context::Resolve(handle,"/chrono",118,chrono)||
   !loading_context::Dormant(handle,"/waiting",17)||!loading_context::Dormant(handle,"/avert",102))return false;
  DWORD exit{};if(!phone_context::Resolve(handle,"/b_exit",109,exit))return false;
  const auto exitFrame=Word(reinterpret_cast<void*>(exit),0x60);
  if(exitFrame!=1){
   // Original General109 open is frames9..16; its content108 PC branch
   // retains the old Escape artwork even when modern input uses hold-F.
   if(exitFrame<9||exitFrame>16||!CabinetPrompt(hero,s.exitKey)||
      !controls_context::Text(root,scope,"m_exit",s.exitLabel,128)||!s.exitLabel[0])return false;
   s.flags|=GeneralHudCabinetExit;
  }
  const auto subFrame=Word(reinterpret_cast<void*>(subtitle),0x60),timerFrame=Word(reinterpret_cast<void*>(chrono),0x60);
  if(subFrame<1||subFrame>2||timerFrame<1||timerFrame>25)return false;
  if(subFrame==2){if(!controls_context::Text(root,scope,"Subtitle",s.subtitle,2048)||!s.subtitle[0])return false;s.flags|=GeneralHudSubtitle;}
  if(timerFrame>1){if(!Countdown(root,scope,"chrono_min",'\'',999,s.minutes)||!Countdown(root,scope,"chrono_sec",'"',59,s.seconds))return false;s.flags|=GeneralHudTimer;}
  if(!s.flags||*reinterpret_cast<BYTE**>(0xfe14e0)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(subtitle),0x60)!=subFrame||Word(reinterpret_cast<void*>(chrono),0x60)!=timerFrame||Word(reinterpret_cast<void*>(0xff4390))!=load)return false;
  s.flags|=GeneralHudVisible;owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,subtitle,chrono,s.flags};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};ready=false;++generation;ReleaseSRWLockExclusive(&lock);}
static void Tick(){GeneralHudStateV1 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&GeneralHudVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&GeneralHudVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
}
