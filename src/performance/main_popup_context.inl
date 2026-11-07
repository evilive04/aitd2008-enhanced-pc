#include "native_recap_cancel.h"
// Native main-menu Options popup320/bind319. Responses follow original button
// scripts299/300/301/305/315/316/317; native scripts retain every side effect.
#include "main_popup_state.h"
namespace main_popup_context {
static bool enabled=false,ready=false,closing=false,dispatching=false;static SRWLOCK lock=SRWLOCK_INIT;
static MainPopupStateV1 state{};static DWORD generation{},readyGeneration{},readyTick{},closingTick{},queued=~0u,queuedGeneration{};
struct Owner{DWORD movie{},handle{},root{},popup{},bind{},type{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Capture(MainPopupStateV1& s,Owner& owner){
 s={};owner={};if(!enabled)return false;
 __try{
  const auto flags=Word(reinterpret_cast<void*>(0xff4290)),mode=Word(reinterpret_cast<void*>(0xfe1790));
  if((flags!=5&&flags!=6)||mode>6||mode==5||Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68||
     !native_recap_cancel::ReadyMenuState(Word(reinterpret_cast<void*>(0xff5508))&0x7fffffff)||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  auto* movie=*reinterpret_cast<unsigned char**>(0xfe1518);if(!movie||Word(movie,12)!=0xfe1520||!movie[0x65])return false;
  auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const DWORD root=Word(handle),scope=root+200;DWORD platform{},ingame{},popup{},bind{},locked{};char type[32]{};
  if(!root||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     !phone_context::Number(root,scope,"IsMainInGame",ingame,1)||flags!=(ingame?5u:6u)||
     !phone_context::Resolve(handle,"/options/pop_up",320,popup)||Word(reinterpret_cast<void*>(popup),0x60)==1||
     !phone_context::Resolve(handle,"/options/pop_up/bind",319,bind)||!phone_parameters_context::Raw(root,scope,"whatpopup",type,32)||
     !phone_context::Number(root,scope,"lockbt_pop",locked,1))return false;
  DWORD stop{};
  if(!strcmp(type,"warning")){s.type=1;stop=87;}
  else if(!strcmp(type,"warningchoice")){s.type=2;stop=69;}
  else if(!strcmp(type,"conflict")){s.type=3;stop=107;}
  else return false;
  if(!controls_context::Text(root,scope,"title_warning",s.title,96)||!controls_context::Text(root,scope,"txt_warning",s.body,1024))return false;
  if(s.type==1){if(!controls_context::Text(root,scope,"m_ok",s.labels[1],64))return false;}
  else if(s.type==2){if(!controls_context::Text(root,scope,"m_no",s.labels[0],64)||!controls_context::Text(root,scope,"m_yes",s.labels[1],64))return false;}
  else {
   // Some authored variants provide contextual captions on popup320. The PC
   // key-conflict route actually observed at bind319/frame107 has neither:
   // its native A/B buttons315/316 answer no/yes, using root m_no/m_yes.
   // Do not treat absent optional captions as an absent interactive dialog.
   const bool contextual=controls_context::Text(root,popup,"name_1",s.labels[0],64)&&controls_context::Text(root,popup,"name_2",s.labels[1],64);
   if(!contextual){
    DWORD xbox{};wchar_t conflictingKey[96]{};
    if(mode!=6||!phone_context::Number(root,scope,"pc_config_xbox",xbox,1)||xbox||
       !controls_context::Text(root,scope,"key_in_conflict",conflictingKey,96)||
       !controls_context::Text(root,scope,"m_no",s.labels[0],64)||!controls_context::Text(root,scope,"m_yes",s.labels[1],64))return false;
   }
  }
  s.flags=MainPopupVisible;
  if(locked||Word(reinterpret_cast<void*>(popup),0x60)!=28||Word(reinterpret_cast<void*>(popup),0x64)!=2||*reinterpret_cast<BYTE*>(popup+0x68)||
     Word(reinterpret_cast<void*>(bind),0x60)!=stop||Word(reinterpret_cast<void*>(bind),0x64)!=2||*reinterpret_cast<BYTE*>(bind+0x68))s.flags|=MainPopupBusy;
  if(*reinterpret_cast<unsigned char**>(0xfe1518)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root)return false;
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,popup,bind,s.type};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static bool Dispatch(unsigned char* movie,DWORD type,DWORD choice){
 if(type==1)return choice==1&&menu_context::Timeline(movie,"/options/pop_up","close");
 if(type!=2&&type!=3)return false;auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));
 if(!options_children_context::NativeString(movie,"/options/pop_up/bind/:validation",choice?"yes":"no"))return false;
 options_context::NativeInteger(handle,"/:lockbt_pop",1);options_context::NativeInteger(handle,"/:release_button",1);
 return menu_context::Timeline(movie,"/options/pop_up/bind",type==2?"end":"end_conflict");
}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;MainPopupStateV1 copy{};AcquireSRWLockShared(&lock);copy=state;if(dispatching||queued!=~0u)copy.flags|=MainPopupBusy;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&MainPopupVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static BOOL Queue(DWORD choice,DWORD epoch){
 if(choice>1||!phone_context::Foreground())return FALSE;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();
 const bool accept=enabled&&ready&&!dispatching&&(state.flags&MainPopupVisible)&&!(state.flags&MainPopupBusy)&&(state.type!=1||choice==1)&&state.generation==epoch&&readyGeneration==epoch&&now-state.tick<=250&&now-readyTick<=250&&queued==~0u;
 if(accept){queued=choice;queuedGeneration=epoch;}ReleaseSRWLockExclusive(&lock);return accept?TRUE:FALSE;
}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&MainPopupVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool OwnsPointer()noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Fresh(previous.root,previous.root,enabled,(state.flags&MainPopupVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool ConsumesInput(){
 if(!phone_context::Foreground())return false;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();
 bool yes=enabled&&ready&&(state.flags&MainPopupVisible)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;
 if(!yes&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))yes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return yes;
}
static void Tick(){
 MainPopupStateV1 next{};Owner owner{};Capture(next,owner);AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;ready=false;}
 const auto choice=queued;queued=~0u;const bool allowed=choice<=1&&(next.flags&MainPopupVisible)&&!(next.flags&MainPopupBusy)&&ready&&queuedGeneration==generation&&readyGeneration==generation&&GetTickCount()-readyTick<=250;const auto sentBefore=state.dispatches,rejectedBefore=state.rejected;
 if(choice<=1){dispatching=true;ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allowed&&phone_context::Foreground()){__try{sent=Dispatch(reinterpret_cast<unsigned char*>(owner.movie),next.type,choice);}__except(EXCEPTION_EXECUTE_HANDLER){sent=false;}}
 if(choice<=1){Capture(next,owner);AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;}ReleaseSRWLockExclusive(&lock);}
 next.tick=GetTickCount();next.generation=generation;next.dispatches=sentBefore+(sent?1:0);next.rejected=rejectedBefore+(choice<=1&&!sent?1:0);AcquireSRWLockExclusive(&lock);state=next;dispatching=false;if(!(state.flags&MainPopupVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){state={};previous={};ready=closing=dispatching=false;queued=~0u;++generation;}ReleaseSRWLockExclusive(&lock);}
}

