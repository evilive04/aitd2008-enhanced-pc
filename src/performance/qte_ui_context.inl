// helperqt observers only. Original movie timelines, FS commands and native inputs run.
#include "qte_ui_state.h"
#include "qte_ui_content.h"
#include <cmath>
namespace qte_ui_context{
static bool enabled=false,ready=false;static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{},readyGeneration{},readyTick{};static QteUiStateV1 state{};
struct Owner{DWORD hero{},component{},movie{},handle{},root{},kind{},button{},message[2]{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Capture(QteUiStateV1& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0xf5b6d0))!=0xc8dd04||(Word(reinterpret_cast<void*>(0xfda5c8))&0x30)||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0||!Word(hero,0x95c)||!Word(hero,0xfe8))return false;
  const float inv=*reinterpret_cast<float*>(hero+0x1d88),heal=*reinterpret_cast<float*>(hero+0x1e00);if(!std::isfinite(inv)||!std::isfinite(heal)||inv>0||heal>0)return false;
  auto* movie=*reinterpret_cast<BYTE**>(0xfd6280);if(!movie||Word(movie,12)!=0xf5b6d0||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const auto root=Word(handle),scope=root+200,frame=root?Word(reinterpret_cast<void*>(scope),0x60):0;
  // Native549749..549774 copies FDA5C8 bits4/5 into pc_config_xbox/pc_pgen.
  // The keyboard guard above excludes BOTH bits, so both copied values are0.
  // Authored fake/debug defaults are not evidence of the runtime keyboard value.
  DWORD platform{},xbox{},pc{};if(!root||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||!phone_context::Number(root,scope,"pc_config_xbox",xbox,1)||xbox||!phone_context::Number(root,scope,"pc_pgen",pc,1)||pc!=0)return false;
  const auto component=Word(hero,0xfe8);DWORD node{},button{};
  if(frame>=10&&frame<=24){
   s.kind=QteUiButton;if(!phone_context::Resolve(handle,"/button",30,node))return false;char alias[16]{},status[16]{};
   if(!phone_parameters_context::Raw(root,scope,"what_button",alias,16)||!phone_parameters_context::Raw(root,scope,"btstatus",status,16)||!controls_context::Text(root,scope,"helper_txt",s.body,1024)||!s.body[0])return false;
   const char* names[]={"a","b","x","y","back","lt"};const char* actions[]={"QTE_A","QTE_B","QTE_X","QTE_Y"};for(unsigned n=0;n<6;++n)if(!strcmp(alias,names[n]))button=n+1;
   if(!button||Word(reinterpret_cast<void*>(component),0x38)!=button||!(Word(reinterpret_cast<void*>(component),4)&1))return false;
   if(button<=4){if(Word(reinterpret_cast<void*>(component),0x48)!=button+12||!inventory_ui_context::Binding(actions[button-1],s.key))return false;}
   else{
    if(Word(reinterpret_cast<void*>(component),0x48)!=~0u||!qte_ui_content::HasNativePcKey(s.body,1024)||
       (button==5?strcmp(status,"pump"):strcmp(status,"default")))return false;
    s.kind=QteUiCaption; // Native formatted instruction already contains its key.
   }
   if(!strcmp(status,"pump"))s.flags|=QteUiRepeat;else if(strcmp(status,"default"))return false;
   s.phase=Word(reinterpret_cast<void*>(node),0x60);if(!s.phase||s.phase>69)return false;
  }else if(frame>=25&&frame<=78){
   s.kind=QteUiCompression;if(!phone_context::Resolve(handle,"/stick_sarah",104,node))return false;s.phase=Word(reinterpret_cast<void*>(node),0x60);
   DWORD open{};if(s.phase<139||s.phase>266||!phone_context::Number(root,node,"btn_ok",open,1)||!phone_context::Number(root,scope,"num_pulse",s.pulse,10000)||!s.pulse)return false;
   if(open)s.flags|=QteUiTimingOpen;
  }else if(frame>=79&&frame<=106){
   s.kind=QteUiBreathing;if(!phone_context::Resolve(handle,"/breath",138,node))return false;s.phase=Word(reinterpret_cast<void*>(node),0x60);
   DWORD a{},b{},engine{};if(s.phase<180||s.phase>360||!phone_context::Resolve(handle,"/breath/lung1",134,a)||!phone_context::Resolve(handle,"/breath/lung2",134,b)||!phone_context::Resolve(handle,"/breath/engine",137,engine))return false;
   s.meter1=Word(reinterpret_cast<void*>(a),0x60);s.meter2=Word(reinterpret_cast<void*>(b),0x60);const auto e=Word(reinterpret_cast<void*>(engine),0x60);
   if(!s.meter1||s.meter1>300||!s.meter2||s.meter2>300||e<1||e>5)return false;
   // Inspiration/expiration are actual named PC timeline phases, not guessed health.
   if(s.phase>=190&&s.phase<=261)s.flags|=QteUiTimingOpen;
  }else if(frame>=107&&frame<=130){
   //677F20/678180 drive this exact PC left/right timeline.6763F0 reads the
   // FRID controller's actual horizontal movement axis; use its LEFT/RIGHT
   // bindings, not an Xbox stick icon or a fixed guess at A/D.
   s.kind=QteUiSteering;if(!phone_context::Resolve(handle,"/stick_ride",144,node))return false;s.phase=Word(reinterpret_cast<void*>(node),0x60);
   const auto behavior=Word(hero,0x25c);if(!behavior||Word(reinterpret_cast<void*>(behavior))!=0xc85c78||Word(reinterpret_cast<void*>(behavior),0x20)!=reinterpret_cast<DWORD>(hero)||s.phase<156||s.phase>310)return false;
   wchar_t right[256]{};if(!inventory_ui_context::Binding("LEFT",s.key)||!inventory_ui_context::Binding("RIGHT",right))return false;wcscpy_s(s.body,right);
   s.pulse=s.phase<165?0:s.phase<238?1:2;button=behavior;
  }else if(frame>=131&&frame<=146){
   //72C2B0 opens cables_in;72C5F0 selects on_green_pc and formats VCL_ACCEL.
   //85A1D0's PC branch uses mouse UP/DOWN and independent BUTTON_0/BUTTON_1.
   s.kind=QteUiCables;if(!phone_context::Resolve(handle,"/stick_cables",179,node))return false;s.phase=Word(reinterpret_cast<void*>(node),0x60);if(s.phase<218||s.phase>454)return false;
   wchar_t change[1024]{},contact[1024]{};if(!controls_context::Text(root,scope,"txt_wires",s.body,1024)||!s.body[0]||!controls_context::Text(root,scope,"m_switch_wires",change,1024)||!controls_context::Text(root,scope,"m_contact_wires",contact,1024)||(wcscmp(s.body,change)&&wcscmp(s.body,contact))||!inventory_ui_context::Binding("VCL_ACCEL",s.key))return false;
   const bool green=s.phase>=410&&s.phase<=423;DWORD gauge{};
   if(phone_context::Resolve(handle,"/stick_cables/gauge_cables",178,gauge)){s.meter1=Word(reinterpret_cast<void*>(gauge),0x60);if(!s.meter1||s.meter1>159)return false;}
   else if(s.phase>=414&&s.phase<=423)return false;
   if(green)s.flags|=QteUiTimingOpen;s.pulse=green||s.meter1?1u:0u;
  }else return false;
  if(Word(hero,0xfe8)!=component||*reinterpret_cast<BYTE**>(0xfd61ec)!=hero||*reinterpret_cast<BYTE**>(0xfd6280)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(scope),0x60)!=frame||Word(reinterpret_cast<void*>(node),0x60)!=s.phase)return false;
  if(s.kind==QteUiSteering&&Word(hero,0x25c)!=button)return false;
  const bool generic=s.kind==QteUiButton||s.kind==QteUiCaption;
  s.flags|=QteUiVisible;s.nativeFrame=frame;owner={reinterpret_cast<DWORD>(hero),Word(hero,0xfe8),reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,s.kind,button,{generic?Word(reinterpret_cast<void*>(component),0x40):0,generic?Word(reinterpret_cast<void*>(component),0x44):0}};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};ready=false;++generation;ReleaseSRWLockExclusive(&lock);}
static void Tick(){QteUiStateV1 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&QteUiVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&QteUiVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
}
