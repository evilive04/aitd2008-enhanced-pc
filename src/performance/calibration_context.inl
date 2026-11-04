#include "native_recap_cancel.h"
#include "calibration_state.h"
#include <cmath>
namespace calibration_context {
struct Owner{DWORD movie{},handle{},root{},panel{};};struct Request{DWORD action{},row{},epoch{};double normalized{};};
static SRWLOCK lock=SRWLOCK_INIT;static bool enabled=false,ready=false,closing=false;
static CalibrationStateV1 state{},lastStable{};static Owner previous{};static Request queued{};static DWORD generation{},readyGeneration{},readyTick{},closingTick{},captureStage{};
static DWORD Word(DWORD p,unsigned at=0){return *reinterpret_cast<const DWORD*>(p+at);}
static DWORD Widget(unsigned row){return row<4?0xff3ba8+row*0xc0:0;}
static bool Capture(CalibrationStateV1& out,Owner& owner){out={};owner={};captureStage=1;if(!enabled)return false;
 __try{
  const DWORD nativeFlags=Word(0xff4290),nativeState=Word(0xff5508)&0x7fffffff,mode=Word(0xfe1790);
  if(Word(0xfe1520)!=0xc77b68||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(0xff4390)))return false;
  if(!((native_recap_cancel::ReadyMenuState(nativeState)&&(nativeFlags==5||nativeFlags==6))||(nativeState==3&&nativeFlags==2)))return false;
  const DWORD movie=Word(0xfe1518);if(!movie||Word(movie,12)!=0xfe1520||!*reinterpret_cast<BYTE*>(movie+0x65))return false;
  const DWORD handle=Word(movie,0x24),root=Word(handle),scope=root+200;DWORD platform{},released{},mireOptions{},panel{};captureStage=2;
  if(!root||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     !phone_context::Number(root,scope,"release_button",released,1)||!phone_context::Number(root,scope,"mire_options",mireOptions,1)||
     !phone_context::Resolve(reinterpret_cast<unsigned char*>(handle),"/mire",128,panel))return false;
  const DWORD frame=Word(panel,0x60);const bool pending=*reinterpret_cast<BYTE*>(panel+0x68)!=0;
  const bool pendingOpen=frame==1&&pending&&mode==3&&mireOptions&&released;
  const bool pendingClose=frame==34&&pending&&mode==0&&mireOptions&&previous.root==root&&(lastStable.flags&CalibrationLevelsKnown);
  const bool opening=(frame>=10&&frame<34)||pendingOpen,closingFrame=(frame>=63&&frame<=74)||pendingClose;
  if(frame!=34&&!opening&&!closingFrame)return false;
  const bool transition=frame!=34||pendingClose;
  if(mode!=4&&!(mireOptions&&transition&&(mode==0||mode==3)))return false;
  captureStage=3;
  if(!controls_context::Text(root,scope,"m_calibrage",out.title,128)||!controls_context::Text(root,scope,"m_brightness",out.brightness,64)||
     !controls_context::Text(root,scope,"m_infos_brightness",out.instructions,512)||!controls_context::Text(root,scope,"m_OK",out.accept,64)||!controls_context::Text(root,scope,"m_Cancel",out.cancel,64))return false;
  out.flags=CalibrationVisible|(nativeFlags==5?CalibrationInGame:0);
  if(transition){out.flags|=CalibrationBusy|CalibrationTransition;out.opacity=pendingOpen?0.f:pendingClose?1.f:opening?(frame-10)/24.f:(75-frame)/12.f;
   if(previous.root==root&&previous.movie==movie&&(lastStable.flags&CalibrationLevelsKnown)){out.selected=lastStable.selected;out.page=lastStable.page;for(unsigned n=0;n<4;++n)out.gauges[n]=lastStable.gauges[n];out.flags|=CalibrationLevelsKnown;}
  }
  else {
   const DWORD selected=Word(0xff3ea8);if(selected>3)return false;out.selected=selected;out.page=selected?1u:0u;
   for(unsigned n=0;n<4;++n){if(Word(Widget(n),0x30)!=handle||*reinterpret_cast<BYTE*>(Widget(n)+8)>1||!options_children_context::Gauge(Widget(n),out.gauges[n]))return false;}
   if(*reinterpret_cast<BYTE*>(Widget(selected)+8)!=1)return false;
   out.flags|=CalibrationLevelsKnown;
   if(released||Word(panel,0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68))out.flags|=CalibrationBusy;
  }
  if(Word(0xfe1518)!=movie||Word(movie,0x24)!=handle||Word(handle)!=root)return false;
  owner={movie,handle,root,panel};captureStage=100;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){out={};owner={};return false;}
}
static bool Fresh(){const DWORD now=GetTickCount();return enabled&&ready&&(state.flags&CalibrationVisible)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;BOOL ok=TRUE;AcquireSRWLockShared(&lock);__try{memcpy(out,&state,bytes);}__except(EXCEPTION_EXECUTE_HANDLER){ok=FALSE;}ReleaseSRWLockShared(&lock);return ok;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();if(value&&enabled&&(state.flags&CalibrationVisible)&&state.generation==epoch&&now-state.tick<=250){ready=true;readyTick=now;readyGeneration=epoch;}else if(!value&&ready&&readyGeneration==epoch){ready=false;closing=true;closingTick=now;}ReleaseSRWLockExclusive(&lock);}
static BOOL Queue(DWORD action,DWORD row,double normalized,DWORD epoch){if(action<1||action>6||row>3||!std::isfinite(normalized)||normalized<0||normalized>1||!phone_context::Foreground())return FALSE;
 AcquireSRWLockExclusive(&lock);const bool okay=Fresh()&&!(state.flags&CalibrationBusy)&&epoch==state.generation&&!queued.action;if(okay)queued={action,row,epoch,normalized};ReleaseSRWLockExclusive(&lock);return okay?TRUE:FALSE;
}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=root&&root==previous.root&&Fresh();ReleaseSRWLockShared(&lock);return yes;}
static bool ConsumesInput(){if(!phone_context::Foreground())return false;AcquireSRWLockExclusive(&lock);bool yes=Fresh();if(!yes&&closing){if(GetTickCount()-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))yes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return yes;}
static bool Focus(const Owner& o,DWORD row){
 auto* movie=reinterpret_cast<unsigned char*>(o.movie);auto* handle=reinterpret_cast<unsigned char*>(o.handle);
 if(Word(0xff3ea8)!=row){options_context::Command(movie,"outgauge");options_context::NativeInteger(handle,"/mire/gamma/:num_gauge",row+1);options_context::NativeInteger(handle,"/mire/:num_menu",row+1);if(!options_context::Command(movie,"ingauge"))return false;}
 return Word(0xff3ea8)==row&&Word(Widget(row),0x30)==o.handle&&*reinterpret_cast<BYTE*>(Widget(row)+8)==1;
}
static bool Dispatch(const Owner& o,const Request& request){auto* movie=reinterpret_cast<unsigned char*>(o.movie);
 if(request.action==CalibrationConfirm||request.action==CalibrationCancel)return options_context::Command(movie,request.action==CalibrationConfirm?"opvalid":"opcancel")&&menu_context::Timeline(movie,"/mire","close");
 if(!Focus(o,request.row))return false;if(request.action==CalibrationFocus)return true;
 if(request.action==CalibrationPrevious||request.action==CalibrationNext){
  // These two original FSCommand notifications deliberately return0 after
  // applying the slider (540D30, actual v79). AVM ignores that return. Verify
  // the surviving native owner/domain instead of reporting a false rejection.
  options_context::Command(movie,request.action==CalibrationPrevious?"slid:arwd":"slid:arwu");options_native_values::Gauge after{};
  return Word(0xfe1790)==4&&Word(0xff3ea8)==request.row&&Word(Widget(request.row),0x30)==o.handle&&options_children_context::Gauge(Widget(request.row),after);
 }
 options_native_values::Gauge actual{};if(request.action!=CalibrationGauge||!options_children_context::Gauge(Widget(request.row),actual))return false;
 const float target=static_cast<float>(actual.minimum+request.normalized*(actual.maximum-actual.minimum));return options_children_context::NativeGauge(Widget(request.row),target)&&options_context::Command(movie,"newparam");
}
static void Tick(){CalibrationStateV1 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}if(!(next.flags&CalibrationVisible))lastStable={};else if(!(next.flags&CalibrationTransition))lastStable=next;AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;ready=false;}
 const auto request=queued;queued={};const bool allowed=request.action&&(next.flags&CalibrationVisible)&&!(next.flags&CalibrationBusy)&&request.epoch==generation&&Fresh();const DWORD sentBefore=state.dispatches,rejectedBefore=state.rejected;
 if(request.action>=CalibrationConfirm){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allowed&&phone_context::Foreground()){__try{sent=Dispatch(owner,request);}__except(EXCEPTION_EXECUTE_HANDLER){sent=false;}}
 if(request.action){if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;}ReleaseSRWLockExclusive(&lock);}
 next.tick=GetTickCount();next.generation=generation;next.dispatches=sentBefore+(sent?1:0);next.rejected=rejectedBefore+(request.action&&!sent?1:0);AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&CalibrationVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;options_children_context::calibrationEnabled=value;if(!value){ready=false;closing=false;queued={};previous={};lastStable={};++generation;}ReleaseSRWLockExclusive(&lock);}
}
