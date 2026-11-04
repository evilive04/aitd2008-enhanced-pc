// Helper manager54D030 / action presenter54C970; paint only.
#include "interaction_ui_state.h"
#include <cmath>
#include "interaction_native_icons.h"
namespace qte_ui_context{static bool OwnsPaint(std::uintptr_t)noexcept;}
namespace interaction_ui_context{
static bool enabled=false,ready=false;static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{},readyGeneration{},readyTick{};static InteractionUiStateV1 state{};
struct Owner{DWORD movie{},handle{},root{},action{},target{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool TemplateReason(const wchar_t* text){
 // The shipped helper's unavailable edit field contains a width-test run of
 // W glyphs before its real localized reason is assigned. It is not a reason.
 unsigned count=0;
 for(unsigned n=0;text[n];++n){
  if(text[n]==L'W'||text[n]==L'w')++count;
  else if(text[n]!=L' '&&text[n]!=L'\t'&&text[n]!=L'\r'&&text[n]!=L'\n')return false;
 }
 return count>=8;
}
static bool Member(BYTE* handle,const char* path,DWORD resource,DWORD& out){DWORD b[2]{};if(!menu_context::Resolve(handle,path,b)||b[1]!=Word(handle)||!b[0]||Word(reinterpret_cast<void*>(Word(reinterpret_cast<void*>(b[0]),0x18)),8)!=resource)return false;out=b[0];return true;}
// Original helperqt root frame9 moves generic /button30 to y12081 twips
// on its 1280x720 canvas (y906 in our 1920x1080 canvas). Its lock/hold
// instruction therefore shares the ordinary interaction prompt's lower lane.
// Detect visual ownership, independently of the QTE component's input flags:
// a useful native caption can remain visible when that typed presenter refuses it.
static bool NativeLowerHint(){
 __try{
  if(Word(reinterpret_cast<void*>(0xf5b6d0))!=0xc8dd04)return false;
  auto* movie=*reinterpret_cast<BYTE**>(0xfd6280);
  if(!movie||Word(movie,12)!=0xf5b6d0||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;
  const auto root=Word(handle),scope=root+200;if(!root||qte_ui_context::OwnsPaint(root))return false;
  const auto frame=Word(reinterpret_cast<void*>(scope),0x60);DWORD platform{},node{};
  if(frame<10||frame>24||!phone_context::OptionalZero(root,scope,"fake")||
   !phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
   !Member(handle,"/button",30,node)||(Word(reinterpret_cast<void*>(node),0x6c)&1)==0)return false;
  const auto phase=Word(reinterpret_cast<void*>(node),0x60);wchar_t text[1024]{};
  if(phase<1||phase>69||!controls_context::Text(root,scope,"helper_txt",text,1024)||!text[0])return false;
  return *reinterpret_cast<BYTE**>(0xfd6280)==movie&&Word(movie,0x24)==reinterpret_cast<DWORD>(handle)&&Word(handle)==root;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool Capture(InteractionUiStateV1& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0xff43c0))!=0xc79208||Word(reinterpret_cast<void*>(0xff4510))!=0xc791c4||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  const auto phase=Word(reinterpret_cast<void*>(0xff43d0)),action=Word(reinterpret_cast<void*>(0xff43f8));if(!phase||phase>3||action>24)return false;
  auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0||!Word(hero,0x95c))return false;
  const auto inv=*reinterpret_cast<float*>(hero+0x1d88),heal=*reinterpret_cast<float*>(hero+0x1e00);if(!std::isfinite(inv)||!std::isfinite(heal)||inv>0||heal>0)return false;
  auto* movie=*reinterpret_cast<BYTE**>(0xff4508);if(!movie||Word(movie,12)!=0xff4510||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const auto root=Word(handle),scope=root+200,frame=root?Word(reinterpret_cast<void*>(scope),0x60):0;
  DWORD platform{},valid{},shape{};if(!root||frame<2||frame>23||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||!phone_context::Number(root,scope,"actionable_valid",valid,1)||!phone_context::Resolve(handle,"/shape",70,shape))return false;
  const char* aliases[]={"action","handle","take","v_radio","v_starter","v_headlights","v_light","v_sun_visor","v_door","v_cables","v_towtruck","v_glove","v_horn","handle_f","action","v_pierce","v_fill_bottle","i_burn_petrol","v_take_key","v_drive","i_rope","i_extinguisher","i_gas_bottle","i_cadaver","i_bash_in"};
  const wchar_t* labels[]={L"Interact",L"Pick up",L"Take",L"Radio",L"Start engine",L"Headlights",L"Interior light",L"Sun visor",L"Door",L"Ignition wires",L"Tow hook",L"Glove compartment",L"Horn",L"Pick up",L"Interact",L"Pierce",L"Fill bottle",L"Burn fuel",L"Take key",L"Drive",L"Use rope",L"Use extinguisher",L"Gas bottle",L"Move body",L"Break open"};
  char alias[40]{},button[16]{};if(!phone_parameters_context::Raw(root,scope,"action",alias,40)||strcmp(alias,aliases[action])||!phone_parameters_context::Raw(root,scope,"bt",button,16)||strcmp(button,"a")||!inventory_ui_context::Binding("ACTION_ENTER",s.key))return false;
  DWORD icon{};if(!Member(handle,"/shape/ic",60,icon))return false;
  s.bitmap=interaction_native_icons::Bitmap(Word(reinterpret_cast<void*>(icon),0x60));if(!s.bitmap)return false;
  wcscpy_s(s.label,labels[action]);DWORD forbidden{};
  if(!Member(handle,"/forbidden",73,forbidden))return false;
  const auto forbiddenFrame=Word(reinterpret_cast<void*>(forbidden),0x60);if(forbiddenFrame<1||forbiddenFrame>7)return false;
  // Helper frame1 still contains authored template text before its off action.
    // Only a genuinely forbidden native action owns this reason field.
    if(!valid&&forbiddenFrame<7&&(Word(reinterpret_cast<void*>(forbidden),0x6c)&1)&&!controls_context::Text(root,scope,"txt_action_impossible",s.unavailable,512,true))return false;
    if(TemplateReason(s.unavailable))s.unavailable[0]=0;
  const char* paths[]={"/shape/i_heal","/shape/i_inflammable","/shape/i_damage1","/shape/i_damage2","/shape/i_damage3"};
  const DWORD ids[]={66,69,63,63,63};for(unsigned n=0;n<5;++n){DWORD node{};if(!Member(handle,paths[n],ids[n],node))return false;if(Word(reinterpret_cast<void*>(node),0x6c)&1)s.traits|=1u<<n;}
  if(*reinterpret_cast<BYTE**>(0xff4508)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(0xff43f8))!=action||Word(reinterpret_cast<void*>(0xff43d0))!=phase)return false;
  s.flags=InteractionUiVisible|(valid?InteractionUiAllowed:0)|(NativeLowerHint()?InteractionUiAboveNativeHint:0);s.action=action;s.nativeFrame=frame;owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,action,Word(reinterpret_cast<void*>(0xff43d8))};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};ready=false;++generation;ReleaseSRWLockExclusive(&lock);}
static void Tick(){InteractionUiStateV1 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
// Native focus is an input fact even when the replacement HUD is disabled.
// Do not make taking a car key depend on copied text, icon assets or paint Ready.
static bool VehicleKeyFocus()noexcept{
 __try{
  if(Word(reinterpret_cast<void*>(0xff43c0))!=0xc79208||Word(reinterpret_cast<void*>(0xff4510))!=0xc791c4||
     Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||
     ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  const DWORD phase=Word(reinterpret_cast<void*>(0xff43d0)),target=Word(reinterpret_cast<void*>(0xff43d8));
  if(!phase||phase>3||!target||Word(reinterpret_cast<void*>(0xff43f8))!=18)return false;
  auto* movie=*reinterpret_cast<BYTE**>(0xff4508);if(!movie||Word(movie,12)!=0xff4510||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;
  const DWORD root=Word(handle),scope=root+200,frame=root?Word(reinterpret_cast<void*>(scope),0x60):0;
  DWORD platform{},valid{};char alias[40]{},button[16]{};
  if(!root||frame<2||frame>23||!phone_context::OptionalZero(root,scope,"fake")||
     !phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     !phone_context::Number(root,scope,"actionable_valid",valid,1)||valid!=1||
     !phone_parameters_context::Raw(root,scope,"action",alias,40)||strcmp(alias,"v_take_key")||
     !phone_parameters_context::Raw(root,scope,"bt",button,16)||strcmp(button,"a"))return false;
  return *reinterpret_cast<BYTE**>(0xff4508)==movie&&Word(movie,0x24)==reinterpret_cast<DWORD>(handle)&&Word(handle)==root&&
    Word(reinterpret_cast<void*>(0xff43d0))==phase&&Word(reinterpret_cast<void*>(0xff43d8))==target&&Word(reinterpret_cast<void*>(0xff43f8))==18;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&InteractionUiVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Fresh(root,previous.root,enabled,(state.flags&InteractionUiVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
}
