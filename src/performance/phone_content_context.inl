// Native message widget and text lifecycle. No private game text/assets shipped.
#include "phone_content_state.h"
#include <algorithm>
namespace phone_content_context {
using Action=PhoneContentAction;using Kind=PhoneContentKind;
static SRWLOCK lock=SRWLOCK_INIT;static bool enabled=false,ready=false,closing=false;
static DWORD generation{},readyGeneration{},readyTick{},closingTick{},queued{},queuedRow{},queuedGeneration{};
static PhoneContentStateV1 state{};
static LONG wheel{};
// Read-only diagnostics in the component map; no persistent user data.
[[maybe_unused]] static DWORD wheelCaptureEvents{},wheelReadEvents{};
[[maybe_unused]] static LONG wheelLastCaptured{},wheelLastRead{};
struct Owner{DWORD movie{},handle{},root{},panel{},kind{},mapShapes[8]{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
#include "phone_telephony_context.inl"
#include "phone_map_context.inl"
static bool CaptureNative(PhoneContentStateV1& out,Owner& owner,unsigned char*& movie){
 out={};owner={};movie=nullptr;
 __try{
  PhoneStateV1 common{};phone_context::Key key{};
  if(!enabled||!phone_context::Capture(common,key,movie)||!(common.flags&PhoneAuthenticated)||
     *reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))||Word(reinterpret_cast<void*>(0xff4290)))return false;
  if(common.nativePage==4||common.nativePage==5)return CaptureTelephony(common.nativePage,movie,out,owner);
  if(common.nativePage==1)return CaptureMap(movie,out,owner);
  if(common.nativePage!=2)return false;
  auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const DWORD root=Word(handle),scope=root+200;
  DWORD platform{},released{},panel{},list{},total{},line{},selected{},offset{};char page[32]{};
  if(!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     !phone_context::Number(root,scope,"release_button",released,1)||!phone_parameters_context::Raw(root,scope,"whatpage",page,32)||
     !phone_parameters_context::Route(page)||!strcmp(page,"no_one")||
     !phone_context::Resolve(handle,"/menu_messages",414,panel)||!phone_timeline_policy::MessageParentPaint(Word(reinterpret_cast<void*>(panel),0x60))||
     !phone_context::Resolve(handle,"/menu_messages/messages",360,list))return false;
  const auto sub=Word(reinterpret_cast<void*>(0xff4b8c)),frame=Word(reinterpret_cast<void*>(list),0x60);
  auto& v=out.view;
  if(sub==2&&phone_timeline_policy::MessageListPaint(frame))v.kind=Kind::List;
  else if(sub==3&&phone_timeline_policy::MessageReaderPaint(frame))v.kind=Kind::Reader;else return false;
  if(Word(reinterpret_cast<void*>(0xff4bf4))!=0xc799a8||Word(reinterpret_cast<void*>(0xff4bf8))!=reinterpret_cast<DWORD>(handle)||
     strcmp(reinterpret_cast<const char*>(0xff4c00),"/menu_messages/messages")||Word(reinterpret_cast<void*>(0xff4c44))!=10||
     !phone_context::Number(root,list,"nb_lines",line,10)||line!=10||!phone_context::Number(root,list,"total_lines",total,4096)||
     !phone_context::Number(root,list,"cur_line",selected,10)||!phone_context::Number(root,list,"start_line",offset,4096))return false;
  if(total){if(!selected||!offset||selected>offset||offset>total)return false;v.offset=offset-selected;v.count=(std::min<DWORD>)(10,total-v.offset);if(selected>v.count)return false;v.selected=selected-1;}
  // Native548600 publishes cur_line/start_line as zero-based selection +1
  // even for an empty list. Both the initial VMF zeros and native1/1 are
  // authentic empty states; neither denotes a selectable message.
  else if(v.kind!=Kind::List||selected>1||offset>1)return false;
  v.total=total;
  if(!phone_parameters_context::Text(root,scope,"m_messages",v.title,96)||!phone_parameters_context::Text(root,scope,"m_subtitle",v.subtitle,256)||
     !phone_parameters_context::Text(root,scope,"m_Back",v.back,64)||!phone_parameters_context::Text(root,scope,"m_read",v.open,64))return false;
  if(v.kind==Kind::Reader){
   // Native read_msg must have populated the selected message, not the VMF's
   // design-time Lorem ipsum placeholder. Supported PC552CF0 always selects
   // "sms", including documents. No guessed image/MMS owner is synthesized.
   char actual[4096]{};const auto* nativeBody=reinterpret_cast<const char*>(0xff4ce4);const auto nativeBytes=strnlen_s(nativeBody,2048);
   if(!Word(reinterpret_cast<void*>(0xff4c74))||!nativeBytes||nativeBytes==2048||
      !phone_parameters_context::Raw(root,scope,"txt_msg",actual,4096)||strlen(actual)!=nativeBytes||memcmp(nativeBody,actual,nativeBytes)||!phone_context::Equal(root,scope,"msg_type","sms")||
      !phone_parameters_context::Text(root,scope,"txt_msg",v.body,4096))return false;
   if(frame==99){if(!phone_parameters_context::Text(root,scope,"title_msg",v.subtitle,256))return false;}
   else{
    // Native frame99 copies this selected original row's title. Before that
    // action root/:title_msg may still belong to the previous message.
    if(!selected)return false;char path[96]{};DWORD title{};sprintf_s(path,"/menu_messages/messages/line%lu/texte",selected);
    if(!phone_context::Resolve(handle,path,230,title)||!phone_parameters_context::Text(root,title,"title_msg",v.subtitle,256))return false;
   }
  }else if(total){
   for(unsigned n=0;n<v.count;++n){DWORD row{},text{},read{};char path[96]{};sprintf_s(path,"/menu_messages/messages/line%u",n+1);
    if(!phone_context::Resolve(handle,path,232,row)||!phone_context::Number(root,row,"read",read,1))return false;
    sprintf_s(path,"/menu_messages/messages/line%u/texte",n+1);
    if(!phone_context::Resolve(handle,path,230,text)||!phone_parameters_context::Text(root,text,"title_msg",v.rows[n].title,256))return false;v.rows[n].read=read;
   }
  }else if(!phone_parameters_context::Text(root,scope,"m_no_message",v.body,4096))return false;
  const bool stable=v.kind==Kind::Reader?frame==99:phone_timeline_policy::MessageListInteractive(frame);
  out.flags=PhoneContentVisible|((released||!stable||Word(reinterpret_cast<void*>(panel),0x60)!=97||
      Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68)||
      Word(reinterpret_cast<void*>(list),0x64)!=2||*reinterpret_cast<BYTE*>(list+0x68))?PhoneContentBusy:0);
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,list,static_cast<DWORD>(v.kind)};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){out={};owner={};return false;}
}
static bool Capture(PhoneContentStateV1& out,Owner& owner,unsigned char*& movie){
 const bool result=CaptureNative(out,owner,movie);if(result&&(out.flags&PhoneContentVisible)){DWORD network{};if(phone_context::Number(owner.root,owner.root+200,"network",network,1)){out.flags|=PhoneContentNetworkKnown;if(network)out.flags|=PhoneContentNetwork;}}return result;
}
static bool Dispatch(unsigned char* movie,const PhoneContentStateV1& s,DWORD raw,DWORD row){
 const auto a=static_cast<Action>(raw);auto* h=reinterpret_cast<unsigned char*>(Word(movie,0x24));const auto& v=s.view;
 if(v.kind==Kind::Call||v.kind==Kind::Dialer)return DispatchTelephony(movie,v,a,row);
 if(v.kind==Kind::Gps)return a==Action::Back&&menu_context::Timeline(movie,"/","back_home");
 if(a==Action::Back)return menu_context::Timeline(movie,v.kind==Kind::Reader?"/menu_messages/messages":"/menu_messages",v.kind==Kind::Reader?"back_from_reading":"back_from_list");
 if(v.kind==Kind::Reader){if(a!=Action::Previous&&a!=Action::Next)return false;return menu_context::Timeline(movie,"/menu_messages/messages",a==Action::Previous?"previous_reading":"next_reading");}
 if(a==Action::Focus||a==Action::Open){
  if(row>=v.count)return false;
  const LONG delta=static_cast<LONG>(row)-static_cast<LONG>(v.selected);
  for(LONG n=0;n<std::abs(delta);++n)if(!phone_parameters_context::Command(movie,delta<0?"widg:lims:selu":"widg:lims:seld"))return false;
  options_context::NativeInteger(h,"/menu_messages/messages/:cur_line",row+1);options_context::NativeInteger(h,"/menu_messages/messages/:start_line",v.offset+row+1);
  options_context::NativeInteger(h,"/:read",v.rows[row].read);
  if(a==Action::Focus)return true;
  options_context::NativeInteger(h,"/:num_valided",row+1);
  if(!phone_context::String(movie,"/:from",""))return false;
  return menu_context::Timeline(movie,"/menu_messages/messages","reading_msg");
 }
 if(a==Action::Previous||a==Action::Next){
  const DWORD steps=a==Action::Previous?(std::min<DWORD>)(10,v.offset):(std::min<DWORD>)(10,v.total-v.offset-v.count);
  if(!steps)return false;
  for(DWORD n=0;n<steps;++n)if(!phone_parameters_context::Command(movie,a==Action::Previous?"widg:lims:scru":"widg:lims:scrd"))return false;
  return true;
 }return false;
}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&PhoneContentVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool WheelLease(DWORD now){return enabled&&ready&&(state.flags&PhoneContentVisible)&&!(state.flags&PhoneContentBusy)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250&&(state.view.kind==Kind::List||state.view.kind==Kind::Reader);}
static bool CaptureWheel(LONG delta){
 if(!phone_context::Foreground())return false;AcquireSRWLockExclusive(&lock);const bool owns=WheelLease(GetTickCount());
 if(owns){wheel=static_cast<LONG>((std::max)(-3840LL,(std::min)(3840LL,static_cast<long long>(wheel)+delta)));if(delta){++wheelCaptureEvents;wheelLastCaptured=delta;}}else wheel=0;
 ReleaseSRWLockExclusive(&lock);return owns;
}
static LONG TakeWheel(DWORD epoch){
 const bool focused=phone_context::Foreground();AcquireSRWLockExclusive(&lock);const LONG value=focused&&epoch==state.generation&&WheelLease(GetTickCount())?wheel:0;wheel=0;if(value){++wheelReadEvents;wheelLastRead=value;}ReleaseSRWLockExclusive(&lock);return value;
}
static BOOL Queue(DWORD action,DWORD row,DWORD epoch){
 if(action<1||action>8||!phone_context::Foreground())return FALSE;
 AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();const auto& v=state.view;
 bool target=action==3||((action==1||action==2)&&v.kind==Kind::List&&row<v.count);
 if(action==4||action==5){const DWORD current=v.offset+v.selected;target=v.kind==Kind::Reader?(action==4?current>0:current+1<v.total):v.kind==Kind::List&&(action==4?v.offset>0:v.offset+v.count<v.total);}
 if(action>=6)target=v.kind==Kind::Dialer&&(action==6?row<10&&wcslen(v.body)<v.offset:v.body[0]!=0);
 const bool accept=target&&enabled&&ready&&(state.flags&PhoneContentVisible)&&!(state.flags&PhoneContentBusy)&&epoch==state.generation&&epoch==readyGeneration&&now-state.tick<=250&&now-readyTick<=250&&!queued;
 if(accept){queued=action;queuedRow=row;queuedGeneration=epoch;}ReleaseSRWLockExclusive(&lock);return accept?TRUE:FALSE;
}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&PhoneContentVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool ConsumesInput(){if(!phone_context::Foreground())return false;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();bool yes=enabled&&ready&&(state.flags&PhoneContentVisible)&&state.view.kind!=Kind::Gps&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;
 if(!yes&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))yes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return yes;}
static bool OwnsMapContent(std::uintptr_t root,std::uintptr_t shape)noexcept{
 AcquireSRWLockShared(&lock);bool yes=state.view.kind==Kind::Gps&&ui_paint_owner::Fresh(root,previous.root,enabled,(state.flags&PhoneContentVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());
 if(yes){yes=false;for(const auto p:previous.mapShapes)if(p&&p==shape){yes=true;break;}}
 ReleaseSRWLockShared(&lock);return yes;
}
static void Tick(){PhoneContentStateV1 next{};Owner owner{};unsigned char* movie{};Capture(next,owner,movie);if(!(next.flags&PhoneContentVisible)||next.view.kind!=Kind::Gps)mapCache={};AcquireSRWLockExclusive(&lock);
 if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;ready=false;wheel=0;}
 const DWORD action=queued,row=queuedRow;queued=0;const bool allowed=action&&(next.flags&PhoneContentVisible)&&!(next.flags&PhoneContentBusy)&&ready&&queuedGeneration==generation&&readyGeneration==generation&&GetTickCount()-readyTick<=250;
 const auto dispatches=state.dispatches,rejected=state.rejected;if(action){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allowed&&phone_context::Foreground()){__try{sent=Dispatch(movie,next,action,row);}__except(EXCEPTION_EXECUTE_HANDLER){sent=false;}}
 if(action){Capture(next,owner,movie);AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;}ReleaseSRWLockExclusive(&lock);}
 next.size=sizeof next;next.version=1;next.tick=GetTickCount();next.generation=generation;next.dispatches=dispatches+(sent?1:0);next.rejected=rejected+(action&&!sent?1:0);
 AcquireSRWLockExclusive(&lock);state=next;if(!(state.flags&PhoneContentVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){ready=false;closing=false;queued=0;wheel=0;previous={};++generation;}ReleaseSRWLockExclusive(&lock);}
}
