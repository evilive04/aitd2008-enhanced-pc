// PC Tips movie.54D750 formats c_infotext and polls native action13/QTE_A in phase2.
#include "tutorial_ui_state.h"
namespace tutorial_ui_context{
static bool enabled=false,ready=false;static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{},readyGeneration{},readyTick{};static TutorialUiStateV1 state{};
struct Owner{DWORD movie{},handle{},root{},message[2]{},phase{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Capture(TutorialUiStateV1& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0xff43c0))!=0xc79208||Word(reinterpret_cast<void*>(0xff4530))!=0xc792dc||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))||*reinterpret_cast<BYTE*>(0xff4568))return false;
  const auto phase=Word(reinterpret_cast<void*>(0xff4548));if(!phase||phase>3)return false;
  auto* movie=*reinterpret_cast<BYTE**>(0xff4528);if(!movie||Word(movie,12)!=0xff4530||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const auto root=Word(handle),scope=root+200,frame=root?Word(reinterpret_cast<void*>(scope),0x60):0;
  DWORD platform{},xbox{};if(!root||frame<2||frame>227||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||!phone_context::Number(root,scope,"pc_config_xbox",xbox,1)||xbox||!controls_context::Text(root,scope,"c_infotext",s.body,2048)||!s.body[0])return false;
  if(phase==2&&!inventory_ui_context::Binding("QTE_A",s.key))return false;
  if(*reinterpret_cast<BYTE**>(0xff4528)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(0xff4548))!=phase||Word(reinterpret_cast<void*>(scope),0x60)!=frame)return false;
  s.flags=TutorialUiVisible|(phase==2?TutorialUiBlocking:0)|((phase==3||(frame>=50&&frame<=63)||frame>=214)?TutorialUiClosing:0);s.phase=phase;s.nativeFrame=frame;
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,{Word(reinterpret_cast<void*>(0xff4558)),Word(reinterpret_cast<void*>(0xff455c))},phase};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};ready=false;++generation;ReleaseSRWLockExclusive(&lock);}
static void Tick(){TutorialUiStateV1 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&TutorialUiVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&TutorialUiVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
}
