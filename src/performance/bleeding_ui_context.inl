// Whole inventory movie paint is claimed here only in an otherwise dormant world HUD.
#include "bleeding_ui_state.h"
namespace bleeding_ui_context{
static bool enabled=false,ready=false;static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{},readyGeneration{},readyTick{};static BleedingUiStateV1 state{};
struct Owner{DWORD hero{},movie{},handle{},root{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Hidden(BYTE* handle,const char* path,DWORD resource){DWORD b[2]{};if(!menu_context::Resolve(handle,path,b)||!b[0]||b[1]!=Word(handle)||Word(reinterpret_cast<void*>(Word(reinterpret_cast<void*>(b[0]),0x18)),8)!=resource)return false;return !(Word(reinterpret_cast<void*>(b[0]),0x6c)&1);}
static bool Capture(BleedingUiStateV1& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0x1081480))!=0xcbe850||Word(reinterpret_cast<void*>(0x1081494))!=0xcbe81c||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0||!Word(hero,0x95c))return false;const auto inv=*reinterpret_cast<float*>(hero+0x1d88);if(!std::isfinite(inv)||inv>0)return false;
  auto* movie=*reinterpret_cast<BYTE**>(0x1081490);if(!movie||Word(movie,12)!=0x1081494||!movie[0x65])return false;auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const auto root=Word(handle),scope=root+200,frame=root?Word(reinterpret_cast<void*>(scope),0x60):0;DWORD pc{};
  if(!root||(frame!=35&&frame!=36)||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",pc,1)||pc!=1||!inventory_ui_context::Dormant(handle,"/clip_general",435)||!inventory_ui_context::Dormant(handle,"/tips_inventory",47)||!inventory_ui_context::HandsOnly(handle)||!inventory_ui_context::Dormant(handle,"/helper",569)||!Hidden(handle,"/item",568)||!bleeding_timer::Read(handle,root,s.timer)||!s.timer.nativeFrame)return false;
  if(*reinterpret_cast<BYTE**>(0xfd61ec)!=hero||*reinterpret_cast<BYTE**>(0x1081490)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(scope),0x60)!=frame)return false;
  s.flags=1;owner={reinterpret_cast<DWORD>(hero),reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};ready=false;++generation;ReleaseSRWLockExclusive(&lock);}
static void Tick(){BleedingUiStateV1 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&state.flags&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,state.flags!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
}
