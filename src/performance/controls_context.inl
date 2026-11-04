#include "native_recap_cancel.h"
// Native Configuration capture and generation-scoped UI action boundary.
#include "controls_state.h"
namespace controls_context {
using Phase=controls_art::Phase;using Kind=controls_art::Kind;
static bool enabled=false;static SRWLOCK lock=SRWLOCK_INIT;
static ControlsStateV1 state{};static DWORD generation{};
static DWORD captureStage{};
static LONG wheel{};
static bool ready=false,closing=false;static DWORD readyGeneration{},readyTick{},closingTick{};
struct Request{DWORD action{},generation{};LONG row=-1;double normalized{};};static Request queued{};
struct Owner{DWORD movie{},handle{},root{},panel{},phase{},category{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Number(DWORD root,DWORD node,const char* name,DWORD& out,DWORD cap=4096){return phone_context::Number(root,node,name,out,cap);}
static bool Text(DWORD root,DWORD node,const char* name,wchar_t* out,int cap,bool optional=false){
 dialog_text::Value v{};if(!phone_context::Value(root,node,name,v)||v.kind!=dialog_text::Kind::String||(!optional&&!v.text[0]))return false;
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.text,-1,out,cap);if(!n)n=MultiByteToWideChar(CP_ACP,0,v.text,-1,out,cap);return n!=0;
}
static bool Node(unsigned char* handle,const char* path,DWORD resource,DWORD& out){return phone_context::Resolve(handle,path,resource,out);}
static bool Capture(ControlsStateV1& out,Owner& owner){
 out={};owner={};captureStage=0;if(!enabled)return false;captureStage=1;
 __try{
  // Measured PC configuration uses mode6. Mode5 is a different native route;
  // the shared panel name alone is not sufficient authority to edit bindings.
  if(Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68||Word(reinterpret_cast<void*>(0xfe1790))!=6||Word(reinterpret_cast<void*>(0xfe1938))!=0xc789f0||
     !native_recap_cancel::ReadyMenuState(Word(reinterpret_cast<void*>(0xff5508))&0x7fffffff)||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  captureStage=2;auto* movie=*reinterpret_cast<unsigned char**>(0xfe1518);if(!movie||Word(movie,12)!=0xfe1520||!movie[0x65])return false;
  auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const auto root=Word(handle),scope=root+200;
  captureStage=3;DWORD main{},platform{},released{},locked{},panel{},category{},maximum{};
  if(!root||!phone_context::OptionalZero(root,scope,"fake")||!Number(root,scope,"IsMainInGame",main,1)||!Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     Word(reinterpret_cast<void*>(0xff4290))!=(main?5u:6u)||!Number(root,scope,"release_button",released,1)||!Number(root,scope,"lockparam",locked,1)||
     !Node(handle,"/options/configuration",567,panel)||!Number(root,panel,"maxhinge",maximum,4)||maximum!=4||!Number(root,panel,"cur_hinge",category,4)||!category)return false;
  captureStage=4;auto& v=out.view;v.category=category-1;v.phase=category==4?Phase::Settings:Phase::Bindings;
  if(!Text(root,scope,"m_configuration",v.title,96))return false;
  captureStage=5;for(unsigned n=0;n<4;++n){char path[80]{};sprintf_s(path,"/options/configuration/o_%u",n+1);DWORD tab{},binding[2]{};
   if(!menu_context::Resolve(handle,path,binding)||!binding[0]||binding[1]!=root)return false;tab=binding[0];
   if(!Text(root,tab,"title_hinge",v.categories[n],64))return false;
  }
  if(category<4){
   captureStage=6;DWORD list{},line{},selected{},start{},total{};if(!Node(handle,"/options/configuration/array",520,list)||!Number(root,list,"nb_lines",line,10)||line!=10||
      !Number(root,list,"cur_line",selected,10)||!Number(root,list,"start_line",start)||!Number(root,list,"total_lines",total)||!total||!selected||selected>start||start>total)return false;
   v.total=total;v.offset=start-selected;v.count=(std::min<DWORD>)(10,total-v.offset);if(selected>v.count)return false;v.selected=selected-1;
   for(unsigned n=0;n<v.count;++n){captureStage=100+n;char path[80]{};sprintf_s(path,"/options/configuration/array/line%u",n+1);DWORD row{},warning{};
    if(!Node(handle,path,501,row)||!Text(root,row,"action",v.rows[n].caption,128)||!Text(root,row,"key",v.rows[n].value,96,true)||
       !Text(root,row,"controller",v.rows[n].device,64,true)||!Number(root,row,"red",warning,1))return false;v.rows[n].warning=warning;
   }
  }else{
   DWORD count{},selected{};if(!Number(root,panel,"nb_param",count,5)||count!=5||!Number(root,panel,"num_param",selected,5)||!selected)return false;v.count=v.total=count;v.selected=selected-1;
   for(unsigned n=0;n<5;++n){char path[112]{},adjustment[24]{};sprintf_s(path,"/options/configuration/param_%u",n+1);DWORD row{},blocked{},binding[2]{};
    if(!menu_context::Resolve(handle,path,binding)||!binding[0]||binding[1]!=root)return false;row=binding[0];
    if(!Text(root,row,"title_param",v.rows[n].caption,128)||!Number(root,row,"lock",blocked,1)||!phone_parameters_context::Raw(root,row,"adj",adjustment,24))return false;
    auto& r=v.rows[n];r.locked=blocked;
    if(!strcmp(adjustment,"gauge")){
     if(n!=2&&n!=3)return false;options_native_values::Gauge gauge{};
     if(!options_children_context::Gauge(0xfe1790+(n==2?0xe8:0x12318),gauge))return false;r.kind=Kind::Gauge;r.current=gauge.current;r.minimum=gauge.minimum;r.maximum=gauge.maximum;
    }else{
     if(strcmp(adjustment,"param"))return false;sprintf_s(path,"/options/configuration/param_%u/reglage/param",n+1);DWORD value{},index{},choices{};if(!menu_context::Resolve(handle,path,binding)||binding[1]!=root||!binding[0])return false;value=binding[0];
     if(!Number(root,value,"num_param",index)||!Number(root,value,"nb_param",choices)||!choices||!index||index>choices||!Text(root,value,"param_txt",r.value,96))return false;r.kind=Kind::Selector;r.current=index;r.minimum=1;r.maximum=choices;
    }
   }
  }
  captureStage=7;DWORD popup{};
  if(Node(handle,"/options/pop_up",320,popup)&&Word(reinterpret_cast<void*>(popup),0x60)!=1){
   char type[32]{};if(!phone_parameters_context::Raw(root,scope,"whatpopup",type,32))return false;
   // Interactive conflict/choice dialogs are owned by main_popup_context.
   if(!strcmp(type,"calibration"))v.phase=Phase::ReleaseKeys;else if(!strcmp(type,"config"))v.phase=Phase::CaptureKey;else if(!strcmp(type,"forbidden"))v.phase=Phase::Forbidden;else return false;
   if(!Text(root,scope,"title_warning",v.modalTitle,96)||!Text(root,scope,"txt_warning",v.modalBody,512))return false;
   out.flags|=ControlsCaptureInput;
  }
  captureStage=8;const char* captions[]={"m_Cancel","m_OK","m_Reset"};const wchar_t* keys[]={L"Esc",L"Enter",L"X"};const controls_art::Action actions[]={controls_art::Action::Cancel,controls_art::Action::Confirm,controls_art::Action::Reset};
  const unsigned footers=v.phase==Phase::Bindings||v.phase==Phase::Settings?3:1;
  for(unsigned n=0;n<footers;++n){if(!Text(root,scope,captions[n],v.footer[n].caption,64))return false;wcscpy_s(v.footer[n].key,keys[n]);v.footer[n].action=actions[n];}
  out.flags|=ControlsVisible;if(released||locked||*reinterpret_cast<BYTE*>(panel+0x68)||Word(reinterpret_cast<void*>(panel),0x64)!=2)out.flags|=ControlsBusy;
  if(*reinterpret_cast<unsigned char**>(0xfe1518)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root)return false;
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,static_cast<DWORD>(v.phase),category};captureStage=9;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){out={};owner={};return false;}
}
#ifndef AITD_CONTROLS_CONTEXT_TEST
#include "controls_actions.inl"
#else
static void Tick(){ControlsStateV1 next{};Owner owner{};Capture(next,owner);AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
#endif
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){state={};previous={};queued={};wheel=0;ready=closing=false;readyGeneration=readyTick=closingTick=0;++generation;}ReleaseSRWLockExclusive(&lock);}
}
