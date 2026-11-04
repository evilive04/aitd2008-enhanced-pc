// Source MainMenu zero-based root: end_of_chapter423 -> Stop435 -> Play436/end -> actions444.
// Runtime current-frame uses one-based values: idle436, closing437..445.
// Preserve the original Continue/QUIT managers and validation timeline.
#include "episode_end_state.h"
namespace episode_end_context {
static bool enabled=false,ready=false,closing=false,continuous=false;static SRWLOCK lock=SRWLOCK_INIT;
static DWORD generation{},readyGeneration{},readyTick{},closingTick{},dispatches{},rejected{};
static EpisodeEndStateV1 state{};struct Owner{DWORD movie{},handle{},root{},buttons[2]{};};static Owner previous{};
struct Request{DWORD action{},row{},generation{};};static Request queued{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Capture(EpisodeEndStateV1& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68||Word(reinterpret_cast<void*>(0xff4290))!=128||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  auto* movie=*reinterpret_cast<unsigned char**>(0xfe1518);if(!movie||Word(movie,12)!=0xfe1520||!movie[0x65])return false;auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));if(!handle)return false;
  const auto root=Word(handle),scope=root+200;if(!root)return false;const auto frame=Word(reinterpret_cast<void*>(scope),0x60);DWORD platform{},ingame{},release{},selected{},buttons[2]{};
  if(frame<424||frame>445||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||!phone_context::Number(root,scope,"IsMainInGame",ingame,1)||ingame!=1||!phone_context::Number(root,scope,"release_button",release,1)||!phone_context::Number(root,scope,"num_b",selected,2)||!selected||!phone_context::Equal(root,scope,"whereto1","continue")||!phone_context::Equal(root,scope,"whereto2","quit"))return false;
  if(!phone_context::Resolve(handle,"/b_1",150,buttons[0])||!phone_context::Resolve(handle,"/b_2",150,buttons[1])||!controls_context::Text(root,scope,"m_Continue",s.buttons[0],128)||!controls_context::Text(root,scope,"m_Quit",s.buttons[1],128)||!controls_context::Text(root,scope,"m_Select",s.select,64))return false;
  if(!controls_context::Text(root,scope,"episode_title",s.title,128)&&!controls_context::Text(root,scope,"m_chapter",s.title,128))return false;
  if(*reinterpret_cast<unsigned char**>(0xfe1518)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(0xff4290))!=128)return false;
  if(continuous)wcscpy_s(s.buttons[1],L"MAIN MENU"); // Native quit returns to MainMenu; never terminates the process.
  s.flags=EpisodeEndVisible|((release||frame!=436||Word(reinterpret_cast<void*>(scope),0x64)!=2||*reinterpret_cast<BYTE*>(scope+0x68))?EpisodeEndBusy:0);s.selected=selected-1;
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,{buttons[0],buttons[1]}};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&EpisodeEndVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool Lease(DWORD now){return enabled&&ready&&(state.flags&EpisodeEndVisible)&&!(state.flags&EpisodeEndBusy)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;}
static BOOL Queue(DWORD action,DWORD row,DWORD epoch){if((action!=1&&action!=2)||row>=2||!options_context::Foreground())return FALSE;AcquireSRWLockExclusive(&lock);const bool yes=Lease(GetTickCount())&&state.generation==epoch&&!queued.action;if(yes)queued={action,row,epoch};ReleaseSRWLockExclusive(&lock);return yes?TRUE:FALSE;}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&EpisodeEndVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool OwnsPointer(){AcquireSRWLockShared(&lock);const bool yes=enabled&&ready&&(state.flags&EpisodeEndVisible)&&state.generation==readyGeneration&&GetTickCount()-state.tick<=250&&GetTickCount()-readyTick<=250;ReleaseSRWLockShared(&lock);return yes;}
static bool ConsumesInput(){if(!options_context::Foreground())return false;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();bool yes=enabled&&ready&&(state.flags&EpisodeEndVisible)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;if(!yes&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))yes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return yes;}
static bool Dispatch(const Owner& o,const Request& r){auto* handle=reinterpret_cast<unsigned char*>(o.handle);options_context::NativeInteger(handle,"/:num_b",r.row+1);
 if(r.action==static_cast<DWORD>(EpisodeEndAction::Focus))return true;
 // Exactly the source keyboard691 Enter path: release, then chosen button150
 // valid. Its original fla_sync and root end action deliver Continue/QUIT.
 options_context::NativeInteger(handle,"/:release_button",1);return menu_context::Timeline(reinterpret_cast<unsigned char*>(o.movie),r.row?"/b_2":"/b_1","valid");
}
static void Tick(){EpisodeEndStateV1 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}const auto request=queued;queued={};const bool allowed=request.action&&(next.flags&(EpisodeEndVisible|EpisodeEndBusy))==EpisodeEndVisible&&Lease(GetTickCount())&&request.generation==generation;if(request.action){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allowed&&options_context::Foreground()){__try{sent=Dispatch(owner,request);}__except(EXCEPTION_EXECUTE_HANDLER){}}
 if(request.action){sent?++dispatches:++rejected;if(!Capture(next,owner)){next={};owner={};}}
 AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}next.tick=GetTickCount();next.generation=generation;next.dispatches=dispatches;next.rejected=rejected;state=next;if(!(state.flags&EpisodeEndVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
static void Configure(bool value,bool continuousValue=false){AcquireSRWLockExclusive(&lock);enabled=value;continuous=continuousValue;state={};previous={};queued={};ready=closing=false;++generation;ReleaseSRWLockExclusive(&lock);}
}
