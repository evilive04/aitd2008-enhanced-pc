// Shared native phone-page bridge: Parameters and message/contact rosters. All game calls run on
// the input/UI thread; the D3D component receives only copied, bounded values.
#include "options_children_context.h"
#include "ui_paint_owner.h"
#include <cmath>
namespace phone_parameters_context {
using Action=options_children_art::Action;
struct Owner {DWORD movie{},handle{},root{},panel{},mode{};};
struct Request {DWORD action{},generation{};LONG row{-1};double value{};};
static SRWLOCK lock=SRWLOCK_INIT;
static OptionsChildrenStateV1 state{};static Owner previous{};static Request queued{};
static bool enabled=false,ready=false,closing=false,haveEntry=false;
static DWORD generation{},readyGeneration{},readyTick{},closingTick{};
static double entryGauge{};
static DWORD Word(const void* p,unsigned n=0){return *reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(p)+n);}
static bool Gauge(options_native_values::Gauge& out){
 return options_native_values::ReadGauge({phone_context::Read,nullptr},0xff4abc,out);
}
static bool Node(unsigned char* handle,const char* path,DWORD id,DWORD& node){return phone_context::Resolve(handle,path,id,node);}
static bool Text(DWORD root,DWORD node,const char* name,wchar_t* out,int cap){
 dialog_text::Value v{};if(!phone_context::Value(root,node,name,v)||v.kind!=dialog_text::Kind::String||!v.text[0])return false;
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.text,-1,out,cap);
 if(!n)n=MultiByteToWideChar(CP_ACP,0,v.text,-1,out,cap);return n!=0;
}
static bool Widget(unsigned char* handle,options_native_values::Gauge& out){
 if(Word(reinterpret_cast<void*>(0xff4aec))!=reinterpret_cast<DWORD>(handle)||
    strcmp(reinterpret_cast<const char*>(0xff4af0),"/parameters/m_4/gauge:num_level")||
    strcmp(reinterpret_cast<const char*>(0xff4b30),"/parameters/m_4/gauge/gauge"))return false;
 return Gauge(out);
}
#include "phone_roster_context.inl"
static bool CaptureNative(OptionsChildrenStateV1& s,Owner& owner,unsigned char*& movie){
 s={};owner={};movie=nullptr;
 __try{
  if(!enabled)return false;
  PhoneStateV1 common{};phone_context::Key key{};
  if(!phone_context::Capture(common,key,movie)||!(common.flags&PhoneAuthenticated)||!movie)return false;
  if(*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))||
     Word(reinterpret_cast<void*>(0xff4290)))return false;
  if(common.nativePage==2||common.nativePage==5)return CaptureRoster(common.nativePage,movie,s,owner);
  if(common.nativePage!=3)return false;
  auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const DWORD root=Word(handle),scope=root+200;
  DWORD platform{},release{},panel{},count{},selected{};
  if(!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     !phone_context::Equal(root,scope,"whatpage","parameters")||!phone_context::Number(root,scope,"release_button",release,1)||
     !Node(handle,"/parameters",486,panel)||!phone_context::Number(root,panel,"nb_m",count,4)||count!=4||
     !phone_context::Number(root,panel,"num_m",selected,4)||!selected)return false;
  const auto frame=Word(reinterpret_cast<void*>(panel),0x60);
  if(!phone_timeline_policy::ParametersPaint(frame))return false;
  auto& v=s.view;v.version=1;v.page=options_children_art::Page::PhoneParameters;v.count=4;v.selected=static_cast<LONG>(selected-1);
  if(!Text(root,scope,"m_Parameters",v.title,96))return false;
  bool busy=release||!phone_timeline_policy::ParametersInteractive(frame)||Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68);
  for(unsigned n=0;n<3;++n){
   char path[48]{},value[24]{};sprintf_s(path,"/parameters/m_%u",n+1);DWORD row{},current{},maximum{};
   if(!Node(handle,path,469,row)||!phone_context::Number(root,row,"num_p",current,3)||!current||
      !phone_context::Number(root,row,"nb_p",maximum,3)||maximum!=(n?2u:3u)||current>maximum)return false;
   auto& r=v.rows[n];r.kind=options_children_art::Kind::Selector;r.current=current;r.minimum=1;r.maximum=maximum;
   sprintf_s(value,"param_%u",current);
   if(!Text(root,row,"title_param",r.label,128)||!Text(root,row,value,r.value,128))return false;
   busy|=Word(reinterpret_cast<void*>(row),0x64)!=2||*reinterpret_cast<BYTE*>(row+0x68);
  }
  DWORD row{},gaugeNode{};options_native_values::Gauge actual{};
  if(!Node(handle,"/parameters/m_4",455,row)||!Node(handle,"/parameters/m_4/gauge",451,gaugeNode)||
     !Widget(handle,actual)||!Text(root,row,"title_param",v.rows[3].label,128))return false;
  auto& g=v.rows[3];g.kind=options_children_art::Kind::Gauge;g.current=actual.current;g.minimum=actual.minimum;g.maximum=actual.maximum;
  busy|=Word(reinterpret_cast<void*>(row),0x64)!=2||*reinterpret_cast<BYTE*>(row+0x68);
  const char* names[]={"m_ok","m_Cancel"};const wchar_t* keys[]={L"Enter",L"Esc"};
  for(unsigned n=0;n<2;++n){auto& f=v.footer[n];if(!Text(root,scope,names[n],f.label,64))return false;wcscpy_s(f.key,keys[n]);f.action=n?Action::Cancel:Action::Confirm;f.enabled=busy?0:1;}
  for(auto& r:v.rows)r.enabled=busy?0:1;
  s.nativeMode=3;s.flags=OptionsChildVisible|OptionsChildActionsProven|OptionsChildInGame|(busy?OptionsChildBusy:0);
  for(unsigned a=1;a<=6;++a)s.actions|=1u<<a;
  if(*reinterpret_cast<unsigned char**>(0xff4790)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root)return false;
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,3};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static bool Capture(OptionsChildrenStateV1& out,Owner& owner,unsigned char*& movie){
 const bool result=CaptureNative(out,owner,movie);if(result&&(out.flags&OptionsChildVisible)){DWORD network{};if(phone_context::Number(owner.root,owner.root+200,"network",network,1)){out.flags|=OptionsChildPhoneNetworkKnown;if(network)out.flags|=OptionsChildPhoneNetwork;}}return result;
}
static bool Foreground(){return phone_context::Foreground();}
static __declspec(noinline) bool Command(unsigned char* movie,const char* name){
#ifdef AITD_PHONE_PARAMETERS_TEST
 return TestCommand(movie,name);
#else
 ULONGLONG hash=0;for(unsigned n=0;n<8&&name[n];++n)hash=(hash<<8)|static_cast<BYTE>(name[n]);
 using Native=void(__thiscall*)(void*,void*,DWORD,DWORD,const char*);
 // Native callbacks are notifications; valparam/outgauge legitimately return0.
 reinterpret_cast<Native>(0x5535a0)(reinterpret_cast<void*>(0xff47a0),movie,static_cast<DWORD>(hash),static_cast<DWORD>(hash>>32),name);
 return true;
#endif
}
static bool NativeGauge(float value){
#ifdef AITD_PHONE_PARAMETERS_TEST
 return TestSetGauge(value);
#else
 __asm{mov esi,0ff4abch
       movss xmm1,value
       mov eax,54c3f0h
       call eax}
 return true;
#endif
}
static bool Dispatch(unsigned char* movie,const OptionsChildrenStateV1& s,const Request& request){
 if(s.nativeMode==2||s.nativeMode==5)return DispatchRoster(movie,s,request);
 if(s.nativeMode!=3)return false;
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const auto a=static_cast<Action>(request.action);
 const auto selected=s.view.selected;char path[48]{};
 if(a==Action::FocusRow){
  if(request.row<0||request.row>=4||selected<0||selected>=4)return false;if(request.row==selected)return true;
  sprintf_s(path,"/parameters/m_%ld",selected+1);if(!menu_context::Timeline(movie,path,"out"))return false;
  options_context::NativeInteger(handle,"/parameters/:num_m",request.row+1);
  sprintf_s(path,"/parameters/m_%ld",request.row+1);return menu_context::Timeline(movie,path,"on");
 }
 if(a==Action::Previous||a==Action::Next||a==Action::Gauge){
  if(request.row!=selected||selected<0||selected>=4)return false;
  if(selected<3){if(a==Action::Gauge)return false;sprintf_s(path,"/parameters/m_%ld",selected+1);return menu_context::Timeline(movie,path,a==Action::Previous?"left":"right");}
  options_native_values::Gauge actual{};if(!Widget(handle,actual))return false;
  double value=actual.current;
  if(a==Action::Gauge){if(!std::isfinite(request.value)||request.value<0||request.value>1)return false;value=actual.minimum+request.value*(actual.maximum-actual.minimum);}
  else value+=(a==Action::Previous?-1.:1.)*(actual.maximum-actual.minimum)/actual.steps;
  value=std::fmax(actual.minimum,std::fmin(actual.maximum,value));return NativeGauge(static_cast<float>(value));
 }
 if(request.row!=-1)return false;
 if(a==Action::Confirm){
  const char* vars[]={"/:ring_parameter","/:map_parameter","/:map_rotation"};
  for(unsigned n=0;n<3;++n)options_context::NativeInteger(handle,vars[n],static_cast<DWORD>(s.view.rows[n].current));
  return Command(movie,"valparam")&&Command(movie,"outgauge")&&menu_context::Timeline(movie,"/","back_home");
 }
 if(a==Action::Cancel){
  options_native_values::Gauge actual{};if(!haveEntry||!Widget(handle,actual)||entryGauge<actual.minimum||entryGauge>actual.maximum)return false;
  return NativeGauge(static_cast<float>(entryGauge))&&Command(movie,"outgauge")&&menu_context::Timeline(movie,"/","back_home");
 }
 return false;
}
static BOOL GetState(void* out,DWORD bytes){
 if(!out||bytes!=sizeof state)return FALSE;OptionsChildrenStateV1 copy{};
 AcquireSRWLockShared(&lock);copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;
 __try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;
}
static void SetPresentationReady(BOOL value,DWORD expected){
 AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&OptionsChildVisible)&&state.generation==expected&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=expected;readyTick=GetTickCount();}
 else if(readyGeneration==expected)ready=false;ReleaseSRWLockExclusive(&lock);
}
static BOOL Queue(DWORD action,LONG row,double value,DWORD expected){
 if(action<1||action>9||action==7||action==8||!std::isfinite(value)||!Foreground()||row< -1||row>=7||(action==4&&(value<0||value>1)))return FALSE;
 AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();
 bool target=false;
 if(row==-1){for(const auto& f:state.view.footer)if(static_cast<DWORD>(f.action)==action&&f.enabled)target=true;}
 else if(static_cast<DWORD>(row)<state.view.count&&state.view.rows[row].enabled&&!state.view.rows[row].locked){
  target=action==1||action==9||((action==2||action==3||action==4)&&row==state.view.selected);
  if(action==4&&state.view.rows[row].kind!=options_children_art::Kind::Gauge)target=false;
 }
 const bool accept=target&&(state.actions&(1u<<action))&&enabled&&ready&&(state.flags&OptionsChildVisible)&&!(state.flags&OptionsChildBusy)&&state.generation==expected&&readyGeneration==expected&&now-state.tick<=250&&now-readyTick<=250&&!queued.action;
 if(accept)queued={action,expected,row,value};ReleaseSRWLockExclusive(&lock);return accept?TRUE:FALSE;
}
static bool OwnsPaint(std::uintptr_t root)noexcept{
 AcquireSRWLockShared(&lock);const bool owns=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&OptionsChildVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return owns;
}
static bool ConsumesInput(){
 if(!Foreground())return false;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();
 bool owns=enabled&&ready&&(state.flags&OptionsChildVisible)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;
 if(!owns&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))owns=true;else closing=false;}
 ReleaseSRWLockExclusive(&lock);return owns;
}
static void Tick(){
 OptionsChildrenStateV1 next{};Owner owner{};unsigned char* movie{};Capture(next,owner,movie);
 AcquireSRWLockExclusive(&lock);
 if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;if(!generation)++generation;ready=false;haveEntry=false;}
 // Opening may expose complete labels before the native gauge has finished
 // setup. Cancel's baseline belongs to the first interactive snapshot only.
 if(!haveEntry&&(next.flags&OptionsChildVisible)&&!(next.flags&OptionsChildBusy)&&next.nativeMode==3){haveEntry=true;entryGauge=next.view.rows[3].current;}
 const auto request=queued;queued={};
 const bool allowed=request.action&&(next.flags&OptionsChildVisible)&&!(next.flags&OptionsChildBusy)&&ready&&request.generation==generation&&readyGeneration==generation&&GetTickCount()-readyTick<=250;
 const auto sentBefore=state.dispatches,rejectedBefore=state.rejected;
 if(request.action){ready=false;closing=true;closingTick=GetTickCount();}
 ReleaseSRWLockExclusive(&lock);
 bool sent=false;
 if(allowed&&Foreground()){__try{sent=Dispatch(movie,next,request);}__except(EXCEPTION_EXECUTE_HANDLER){sent=false;}}
 if(request.action){Capture(next,owner,movie);AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;if(!generation)++generation;haveEntry=false;}ReleaseSRWLockExclusive(&lock);}
 next.size=sizeof next;next.version=1;next.tick=GetTickCount();next.generation=generation;next.dispatches=sentBefore+(sent?1:0);next.rejected=rejectedBefore+(request.action&&!sent?1:0);
 AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&OptionsChildVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){ready=false;closing=false;haveEntry=false;queued={};previous={};++generation;}ReleaseSRWLockExclusive(&lock);}
}
