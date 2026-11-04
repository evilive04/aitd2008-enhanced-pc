// General's real waiting/loading film. Presentation only: no timeline writes,
// fake percentages, input capture, game-clock changes or modal responses.
#include "loading_state.h"
#include "loading_tip_content.h"
namespace loading_context {
static bool enabled=false,ready=false;static SRWLOCK lock=SRWLOCK_INIT;
static DWORD generation{},readyGeneration{},readyTick{};static LoadingStateV3 state{};
struct Owner{DWORD movie{},handle{},root{},panel{},full{},helperMovie{},helperHandle{},helperRoot{},picture{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Dormant(unsigned char* h,const char* path,DWORD id){DWORD node{};return phone_context::Resolve(h,path,id,node)&&Word(reinterpret_cast<void*>(node),0x60)==1;}
// Native loading-tip controller FF54F0: 554A00 updates its +40 state only
// in flow32.555840 invalidates it when the scene loader leaves phases9/10;
// 5558B0 clears +41/+44 on close. Timeline/content alone outlive this owner.
static bool TipStage(){
 const auto loader=Word(reinterpret_cast<void*>(0xfddcfc));
 return *reinterpret_cast<const BYTE*>(0xfddf48)&&loader&&
   (Word(reinterpret_cast<void*>(loader),0x40)==9||Word(reinterpret_cast<void*>(loader),0x40)==10);
}
static bool TipOwner(DWORD kind){return TipStage()&&(Word(reinterpret_cast<void*>(0xff5508))&0x7fffffff)==32&&
 *reinterpret_cast<const BYTE*>(0xff5530)&&*reinterpret_cast<const BYTE*>(0xff5531)&&Word(reinterpret_cast<void*>(0xff5534))==kind;}
static bool CaptureHelper(LoadingStateV3& s,Owner& owner){
 if(Word(reinterpret_cast<void*>(0xff4510))!=0xc791c4)return false;auto* movie=*reinterpret_cast<BYTE**>(0xff4508);
 if(!movie||Word(movie,12)!=0xff4510||!movie[0x65])return false;auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;
 const auto root=Word(handle),scope=root+200;DWORD platform{},count{},tips{};
 if(!root||Word(reinterpret_cast<void*>(scope),0x60)<24||Word(reinterpret_cast<void*>(scope),0x60)>84||
    !phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
    !phone_context::Resolve(handle,"/tips",98,tips)||!phone_context::Number(root,scope,"nb_tips",count,2)||!count)return false;
 char first[32]{},second[32]{};wchar_t body[2][1024]{};
 if(!phone_parameters_context::Raw(root,scope,"tips_1",first,32)||!phone_parameters_context::Raw(root,scope,"tips_2",second,32))return false;
 //555C20's exact ten-entry source table, including its single-helper sentinel.
 const bool pair=count==2&&((!strcmp(first,"v_glove")&&!strcmp(second,"v_sun_visor"))||(!strcmp(first,"v_pierce")&&!strcmp(second,"v_fill_bottle")));
 const bool single=count==1&&!strcmp(second,"nothing____")&&(!strcmp(first,"v_cables")||!strcmp(first,"i_burn_petrol")||!strcmp(first,"handle")||!strcmp(first,"handle_f")||!strcmp(first,"i_rope"));
 if((!pair&&!single)||!controls_context::Text(root,scope,"txt_tips_1",body[0],1024)||!body[0][0])return false;
 if(pair&&(!controls_context::Text(root,scope,"txt_tips_2",body[1],1024)||!body[1][0]))return false;
 if(wcslen(body[0])+wcslen(body[1])+(pair?2:0)>=2048)return false;
 wcscpy_s(s.body,body[0]);if(pair){wcscat_s(s.body,L"\n\n");wcscat_s(s.body,body[1]);}
 if(*reinterpret_cast<BYTE**>(0xff4508)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root)return false;
 owner.helperMovie=reinterpret_cast<DWORD>(movie);owner.helperHandle=reinterpret_cast<DWORD>(handle);owner.helperRoot=root;return true;
}
static bool CaptureMain(LoadingStateV3& s,Owner& owner){
 // MainMenu's old loading frame can remain resident after control returns to
 // the world. A live world with no modal or progress owner cannot own it.
 const auto mode=Word(reinterpret_cast<void*>(0xff4390));
 if(*reinterpret_cast<const BYTE*>(0xfd8be5)&&!Word(reinterpret_cast<void*>(0xff4290))&&
    (!mode||mode==5)&&!TipStage())return false;
 if(Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68)return false;auto* movie=*reinterpret_cast<unsigned char**>(0xfe1518);
 if(!movie||Word(movie,12)!=0xfe1520||!movie[0x65])return false;auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));if(!handle)return false;
 const auto root=Word(handle),scope=root+200;if(!root)return false;const auto frame=Word(reinterpret_cast<void*>(scope),0x60);DWORD panel{},platform{};
 if(frame<496||frame>625||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||!phone_context::Resolve(handle,"/loading",700,panel)||!controls_context::Text(root,scope,"m_loading",s.message,1024))return false;
 if(*reinterpret_cast<unsigned char**>(0xfe1518)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(scope),0x60)!=frame||*reinterpret_cast<BYTE*>(0xff437c))return false;
 s.flags=LoadingVisible|LoadingFullCanvas;s.nativeMode=6;owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,1};return true;
}
static bool Capture(LoadingStateV3& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  if(*reinterpret_cast<BYTE*>(0xff437c))return false;const auto mode=Word(reinterpret_cast<void*>(0xff4390));if(mode>5)return false;
  if(mode!=4&&CaptureMain(s,owner))return true;s={};owner={};
  if(Word(reinterpret_cast<void*>(0xfe14e8))!=0xc77b34)return false;
  auto* movie=*reinterpret_cast<unsigned char**>(0xfe14e0);if(!movie||Word(movie,12)!=0xfe14e8||!movie[0x65])return false;
  auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));if(!handle||reinterpret_cast<DWORD>(handle)!=Word(reinterpret_cast<void*>(0xff4378)))return false;
  const auto root=Word(handle),scope=root+200;if(!root||!phone_context::OptionalZero(root,scope,"fake"))return false;
  const auto frame=Word(reinterpret_cast<void*>(scope),0x60);DWORD panel{};const char* message="msg_loading";bool full=false,information=false;
  if(frame>=30&&frame<=46){
   //555B00/555FC0 populate these original translated fields before opening
   // loading_tips. This is also used outside an active loading progress mode.
   // The helper/picture routes begin47/62 and must retain their own content.
   if(!TipOwner(1)&&!TipOwner(4))return false;
   DWORD text{},platform{};
   if(!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
      !phone_context::Resolve(handle,"/bkgd",156,panel)||!phone_context::Resolve(handle,"/txt",158,text)||
      !controls_context::Text(root,scope,"title_tips",s.title,128)||!s.title[0]||
      !controls_context::Text(root,scope,"tips_txt",s.body,2048)||!s.body[0])return false;
   information=true;
  }else if(frame>=47&&frame<=61){
   if(!TipOwner(2))return false;
   DWORD platform{};
   if(!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||!phone_context::Resolve(handle,"/bkgd",156,panel)||
      !controls_context::Text(root,scope,"title_tips",s.title,128)||!s.title[0]||!CaptureHelper(s,owner))return false;
   information=true;
  }else if(frame>=62&&frame<=75){
   if(!TipOwner(3))return false;
   DWORD platform{},picture{};char alias[16]{};
   if(!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||!phone_context::Resolve(handle,"/tips",173,picture)||
      !phone_parameters_context::Raw(root,scope,"tipType",alias,16)||!controls_context::Text(root,scope,"title_tips",s.title,128)||!s.title[0]||
      !controls_context::Text(root,scope,"tips_txt",s.body,2048)||!s.body[0])return false;
   s.picture=loading_tip_content::Select(alias,Word(reinterpret_cast<void*>(picture),0x60));if(!s.picture)return false;panel=picture;owner.picture=s.picture;information=true;
  }else if(!mode||mode==4)return false;
  else if(frame==10||frame==18){
   if(mode==5||!phone_context::Resolve(handle,"/load_scr_popup",138,panel))return false;full=true;message="m_msg_loading";
  }else if(frame==1){
   if(!phone_context::Resolve(handle,"/waiting",17,panel))return false;
   const auto waitFrame=Word(reinterpret_cast<void*>(panel),0x60);if(waitFrame<=1||waitFrame>348)return false;
   // Owning the General root must never erase another active General feature.
   if(!Dormant(handle,"/subtitle",7)||!Dormant(handle,"/chrono",118)||!Dormant(handle,"/avert",102)||!Dormant(handle,"/b_exit",109))return false;
  }else return false; // Tips pictures/helpers require a separate content bridge.
  if(!information&&(!controls_context::Text(root,scope,message,s.message,1024)||!s.message[0]))return false;
  if(*reinterpret_cast<unsigned char**>(0xfe14e0)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(0xff4390))!=mode||*reinterpret_cast<BYTE*>(0xff437c)||Word(reinterpret_cast<void*>(scope),0x60)!=frame)return false;
  s.flags=LoadingVisible|(full?LoadingFullCanvas:0)|(information?LoadingInformation:0);s.nativeMode=information?(s.picture?9:owner.helperRoot?8:7):mode;
  owner.movie=reinterpret_cast<DWORD>(movie);owner.handle=reinterpret_cast<DWORD>(handle);owner.root=root;owner.panel=panel;owner.full=information?2u:full?1u:0u;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};ready=false;++generation;ReleaseSRWLockExclusive(&lock);}
static void Tick(){LoadingStateV3 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&LoadingVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const auto owned=root==previous.helperRoot?previous.helperRoot:previous.root;const bool owns=ui_paint_owner::Visual(root,owned,enabled,(state.flags&LoadingVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return owns;}
}
