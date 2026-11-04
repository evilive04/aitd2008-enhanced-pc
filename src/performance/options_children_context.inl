#include "native_recap_cancel.h"
// Include after menu_context.inl and options_context.inl in the input module.
// Native reads/calls belong exclusively to Tick on the original UI thread.
#include "options_children_context.h"
#include "options_native_values.h"
#include "ui_paint_owner.h"
#include "ui_native_status.h"
#include "ui_transition_timing.h"
#include <cmath>
namespace options_children_context {
using Action=options_children_art::Action;using Page=options_children_art::Page;using Kind=options_children_art::Kind;
struct Owner {DWORD movie{},handle{},root{},page{},mode{},ingame{};};
struct Pending {DWORD action{},generation{};LONG row{};double normalized{};};
static SRWLOCK lock=SRWLOCK_INIT;static bool enabled=false,ready=false,closing=false;
static bool calibrationEnabled=false;
static DWORD generation=0,readyGeneration=0,readyTick=0,closingTick=0;static Owner previous{};static Pending pending{};static OptionsChildrenStateV1 state{};
static DWORD Word(const void* p,unsigned offset=0){return *reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(p)+offset);}
[[maybe_unused]] static bool Read(void*,std::uint32_t at,void* out,std::size_t bytes){__try{memcpy(out,reinterpret_cast<const void*>(at),bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool Value(DWORD root,DWORD node,const char* name,dialog_text::Value& out,bool optional=false){
#ifdef AITD_OPTIONS_CHILDREN_TEST
 const bool found=TestValue(root,node,name,out);return found&&(optional||out.kind!=dialog_text::Kind::Missing);
#else
 return options_native_values::Scoped({Read,nullptr},root,node,name,out,optional);
#endif
}
static bool Number(DWORD root,DWORD node,const char* name,DWORD& out,bool optional=false){dialog_text::Value v{};if(!Value(root,node,name,v,optional))return false;if(optional&&v.kind==dialog_text::Kind::Missing){out=0;return true;}std::uint32_t n{};if(!dialog_text::Unsigned(v,16384,n))return false;out=n;return true;}
static bool Text(DWORD root,DWORD node,const char* name,wchar_t* out,unsigned cap){dialog_text::Value v{};if(!Value(root,node,name,v)||v.kind!=dialog_text::Kind::String||!v.text[0])return false;int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.text,-1,out,cap);if(!n)n=MultiByteToWideChar(CP_ACP,0,v.text,-1,out,cap);return n!=0;}
static DWORD Node(unsigned char* handle,const char* path){DWORD b[2]{};if(!menu_context::Resolve(handle,path,b)||!b[0]||b[1]!=Word(handle)||b[0]==b[1]+200||!(Word(reinterpret_cast<void*>(b[0]),0x6c)&1))return 0;return b[0];}
static bool BlockingMovie(DWORD node){
 // Native frames are one-based. Popup/calibration instances exist with the
 // visible bit even when idle: stopped frame1, no pending timeline change.
 // Every transition and later frame revokes ownership until it settles back.
 return node&&(Word(reinterpret_cast<void*>(node),0x60)!=1||Word(reinterpret_cast<void*>(node),0x64)!=2||*reinterpret_cast<BYTE*>(node+0x68));
}
static bool Gauge(DWORD at,options_native_values::Gauge& value){
#ifdef AITD_OPTIONS_CHILDREN_TEST
 return TestGauge(at,value);
#else
 return options_native_values::ReadGauge({Read,nullptr},at,value);
#endif
}
static bool Capture(OptionsChildrenStateV1& s,Owner& owner,unsigned char*& movie){s={};owner={};movie=nullptr;
 __try{
  if(!enabled||Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68)return false;
  movie=*reinterpret_cast<unsigned char**>(0xfe1518);if(!movie||Word(movie,12)!=0xfe1520||!movie[0x65])return false;auto* handle=*reinterpret_cast<unsigned char**>(movie+0x24);if(!handle||!Word(handle))return false;
  const DWORD root=Word(handle),rootNode=root+200;DWORD ingame{};if(!Number(root,rootNode,"IsMainInGame",ingame)||ingame>1)return false;
  if((Word(reinterpret_cast<void*>(0xff4290))!=6&&(!ingame||Word(reinterpret_cast<void*>(0xff4290))!=5))||!native_recap_cancel::ReadyMenuState(Word(reinterpret_cast<void*>(0xff5508))&0x7fffffff)||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  const DWORD mode=Word(reinterpret_cast<void*>(0xfe1790));if(mode<1||mode>3||(ingame&&mode==2))return false;const char* prefix=mode==1?"/options/audio":mode==2?"/options/languages":"/options/display";const DWORD page=Node(handle,prefix);if(!page)return false;if(BlockingMovie(Node(handle,"/options/pop_up"))||BlockingMovie(Node(handle,"/mire")))return false;
  DWORD platform{},released{},nativeLock{};if(!Number(root,rootNode,"WhatPlatform",platform)||platform!=1||!Number(root,rootNode,"release_button",released)||released>1||!Number(root,rootNode,"lockparam",nativeLock)||nativeLock>1)return false;
  auto& view=s.view;view.version=1;view.page=static_cast<Page>(mode);view.count=mode==1?3:mode==2?1:7;view.selected=0;const char* title=mode==1?"m_audio":mode==2?"m_languages":"m_display";if(!Text(root,rootNode,title,view.title,96))return false;
  DWORD selection=1;if(mode!=2&&(!Number(root,page,"num_param",selection)||!selection||selection>view.count))return false;view.selected=static_cast<LONG>(selection-1);
  for(unsigned n=0;n<view.count;++n){char rowPath[128]{};if(mode==2)strcpy_s(rowPath,"/options/languages/param");else sprintf_s(rowPath,"%s/param_%u",prefix,n+1);const DWORD row=Node(handle,rowPath);if(!row)return false;auto& out=view.rows[n];out.enabled=1;
   // Native AVM leaves lock undefined on unlocked rows. Only this optional
   // boolean defaults to zero; missing required values/corrupt tables reject.
   DWORD locked=0;if(mode!=2&&(!Number(root,row,"lock",locked,true)||locked>1))return false;out.locked=locked;
   if(mode==2){if(!Text(root,rootNode,"m_languages",out.label,128))return false;}else if(!Text(root,row,"title_param",out.label,128))return false;
   const bool gauge=mode==1?n==0:mode==3&&(n==2||n==3);out.kind=gauge?Kind::Gauge:Kind::Selector;char valuePath[160]{};if(mode==2)strcpy_s(valuePath,rowPath);else sprintf_s(valuePath,"%s/reglage/%s",rowPath,gauge?"gauge":"param");const DWORD valueNode=Node(handle,valuePath);if(!valueNode)return false;
   if(gauge){const DWORD widget=0xfe1790+(mode==1?0x18:n==2?0x12728:0x127e8);options_native_values::Gauge native{};if(!Gauge(widget,native))return false;out.current=native.current;out.minimum=native.minimum;out.maximum=native.maximum;}
   else {DWORD index{},count{};if(!Number(root,valueNode,"num_param",index)||!Number(root,valueNode,"nb_param",count)||!count||!index||index>count||!Text(root,valueNode,"param_txt",out.value,128))return false;out.current=index;out.minimum=1;out.maximum=count;}
  }
  if(mode==3&&(!Text(root,rootNode,"m_informations",view.infoTitle,96)||!Text(root,rootNode,"t_txt_infos_opt_gr",view.information,768)))return false;
  const char* captions[]={"m_OK","m_Cancel","m_Reset","m_calibrage"};const wchar_t* keys[]={L"Enter",L"Esc",L"X",L"C"};const Action actions[]={Action::Confirm,Action::Cancel,Action::Reset,Action::Calibration};const unsigned footers=mode==2?2:mode==1?3:4;
  for(unsigned n=0;n<footers;++n){if(!Text(root,rootNode,captions[n],view.footer[n].label,64))return false;wcscpy_s(view.footer[n].key,keys[n]);view.footer[n].action=actions[n];view.footer[n].enabled=0;}
  s.flags=OptionsChildVisible|(ingame?OptionsChildInGame:0);s.nativeMode=mode;
  // Languages has one permanently focused native selector. Its complete
  // previous/next/confirm/cancel paths are authenticated from original410/406.
  // Audio/Display retain native presentation until their focus/gauge lifecycle
  // is independently proven; a successful snapshot is not action permission.
  if(mode==2){const DWORD selector=Node(handle,"/options/languages/param");s.flags|=OptionsChildActionsProven;s.actions=(1u<<static_cast<unsigned>(Action::FocusRow))|(1u<<static_cast<unsigned>(Action::Previous))|(1u<<static_cast<unsigned>(Action::Next))|(1u<<static_cast<unsigned>(Action::Confirm))|(1u<<static_cast<unsigned>(Action::Cancel));
   if(released||Word(reinterpret_cast<void*>(selector),0x64)!=2||*reinterpret_cast<BYTE*>(selector+0x68))s.flags|=OptionsChildBusy;const bool usable=!(s.flags&OptionsChildBusy);view.rows[0].enabled=1;view.footer[0].enabled=view.footer[1].enabled=1;(void)usable;
  }
  if(mode!=2){
   s.flags|=OptionsChildActionsProven;s.actions=(1u<<static_cast<unsigned>(Action::FocusRow))|(1u<<static_cast<unsigned>(Action::Previous))|(1u<<static_cast<unsigned>(Action::Next))|(1u<<static_cast<unsigned>(Action::Gauge))|(1u<<static_cast<unsigned>(Action::Confirm))|(1u<<static_cast<unsigned>(Action::Cancel))|(1u<<static_cast<unsigned>(Action::Reset));
   char selectedPath[128]{};sprintf_s(selectedPath,"%s/param_%u",prefix,selection);const DWORD selectedRow=Node(handle,selectedPath);DWORD active{};
   if(!Number(root,selectedRow,"line_active",active)||active>1)return false;
   if(released||nativeLock||Word(reinterpret_cast<void*>(page),0x64)!=2||*reinterpret_cast<BYTE*>(page+0x68)||active!=1)s.flags|=OptionsChildBusy;
   if(view.rows[selection-1].kind==Kind::Selector){char selectorPath[160]{};sprintf_s(selectorPath,"%s/param_%u/reglage/param",prefix,selection);const DWORD selector=Node(handle,selectorPath);if(!selector)return false;if(Word(reinterpret_cast<void*>(selector),0x64)!=2||*reinterpret_cast<BYTE*>(selector+0x68))s.flags|=OptionsChildBusy;}
   for(unsigned n=0;n<view.count;++n){char path[128]{};sprintf_s(path,"%s/param_%u",prefix,n+1);const DWORD node=Node(handle,path);dialog_text::Value in{},out{};
    if(!Value(root,node,"msgin",in)||in.kind!=dialog_text::Kind::String||!in.text[0]||strlen(in.text)>31||!Value(root,node,"msgout",out)||out.kind!=dialog_text::Kind::String||!out.text[0]||strlen(out.text)>31)return false;
    view.rows[n].enabled=1;
   }
   for(unsigned n=0;n<footers;++n)view.footer[n].enabled=1;
   if(mode==3){DWORD xbox{},pgen{};view.footer[3].enabled=0;if(Number(root,rootNode,"pc_config_xbox",xbox)&&xbox<=1&&Number(root,rootNode,"pc_pgen",pgen)&&pgen<=1&&(xbox||pgen||calibrationEnabled)&&Node(handle,"/mire")){s.actions|=1u<<static_cast<unsigned>(Action::Calibration);view.footer[3].enabled=1;}}
  }
  if(*reinterpret_cast<unsigned char**>(0xfe1518)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(0xfe1790))!=mode)return false;
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,page,mode,ingame};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
// Native91F430 retains string storage. Call only with static/module-owned strings.
static bool NativeString(unsigned char* movie,const char* path,const char* text){
#ifdef AITD_OPTIONS_CHILDREN_TEST
 return TestString(movie,path,text);
#else
 auto* handle=*reinterpret_cast<unsigned char**>(movie+0x24);
 __asm {mov eax,handle
        mov ecx,text
        push path
        mov edx,91f430h
        call edx}
 return true;
#endif
}
static bool NativeGauge(DWORD widget,float value){
#ifdef AITD_OPTIONS_CHILDREN_TEST
 return TestSetGauge(widget,value);
#else
 __asm {mov esi,widget
        movss xmm1,value
        mov eax,54c3f0h
        call eax}
 return true;
#endif
}
static bool RawText(DWORD root,DWORD node,const char* name,char* out,unsigned cap){dialog_text::Value v{};if(!Value(root,node,name,v)||v.kind!=dialog_text::Kind::String||!v.text[0]||strlen(v.text)>=cap)return false;strcpy_s(out,cap,v.text);return true;}
// These are stable AVM-owned values;91F430 does not duplicate the supplied text.
static char infoAdjustment[64]{},infoText[4096]{};
static bool Focus(unsigned char* movie,const OptionsChildrenStateV1& current,LONG selected){
 if(selected<0||static_cast<DWORD>(selected)>=current.view.count||current.view.rows[selected].locked)return false;
 const LONG old=current.view.selected;if(old==selected)return true;if(old<0||static_cast<DWORD>(old)>=current.view.count)return false;
 auto* handle=*reinterpret_cast<unsigned char**>(movie+0x24);const DWORD root=Word(handle);const char* page=current.nativeMode==1?"/options/audio":"/options/display";
 char oldPath[128]{},newPath[128]{},selectionPath[128]{},msgOut[32]{},msgIn[32]{},adjustment[64]{},information[4096]{};
 sprintf_s(oldPath,"%s/param_%ld",page,old+1);sprintf_s(newPath,"%s/param_%ld",page,selected+1);sprintf_s(selectionPath,"%s/:num_param",page);
 const DWORD oldNode=Node(handle,oldPath),newNode=Node(handle,newPath);if(!oldNode||!newNode||!RawText(root,oldNode,"msgout",msgOut,32)||!RawText(root,newNode,"msgin",msgIn,32))return false;
 if(current.nativeMode==3){char key[96]{};if(!RawText(root,newNode,"adjustment",adjustment,64))return false;sprintf_s(key,"m_infos_%s",adjustment);if(!RawText(root,root+200,key,information,4096)||!Node(handle,"/options/display/infos"))return false;}
 // These two strings are native row FSCommand notifications, not acknowledged
 // requests. Native AVM ignores the callback's return. E.g. mucg_in/qual_in
 // legitimately return0 from53FA40; stopping there leaves num_param changed
 // but the new row inactive. Exceptions still propagate to the dispatch guard.
 options_context::Command(movie,msgOut);
 if(!menu_context::Timeline(movie,oldPath,current.view.rows[old].locked?"out_lock":"out"))return false;
 options_context::NativeInteger(handle,selectionPath,selected+1);
 options_context::Command(movie,msgIn);
 if(!menu_context::Timeline(movie,newPath,"on"))return false;
 if(current.nativeMode==3){strcpy_s(infoAdjustment,adjustment);strcpy_s(infoText,information);if(!NativeString(movie,"/:infos_adjustment",infoAdjustment)||!NativeString(movie,"/:new_info_op",infoText)||!menu_context::Timeline(movie,"/options/display/infos","change"))return false;}
 return true;
}
static bool Dispatch(unsigned char* movie,const OptionsChildrenStateV1& current,const Pending& action){const Action kind=static_cast<Action>(action.action);
 if(current.nativeMode==1||current.nativeMode==3){
  const char* page=current.nativeMode==1?"/options/audio":"/options/display";auto* handle=*reinterpret_cast<unsigned char**>(movie+0x24);char rowPath[128]{};sprintf_s(rowPath,"%s/param_%ld",page,current.view.selected+1);
  if(kind==Action::FocusRow)return Focus(movie,current,action.row);
  if(kind==Action::Previous||kind==Action::Next||kind==Action::Gauge){if(action.row!=current.view.selected||action.row<0||static_cast<DWORD>(action.row)>=current.view.count||current.view.rows[action.row].locked)return false;
   const auto& row=current.view.rows[action.row];if(kind==Action::Gauge){if(row.kind!=Kind::Gauge||action.normalized<0||action.normalized>1)return false;const DWORD widget=0xfe1790+(current.nativeMode==1?0x18:action.row==2?0x12728:action.row==3?0x127e8:0);if(widget==0xfe1790)return false;
#ifndef AITD_OPTIONS_CHILDREN_TEST
    if(*reinterpret_cast<BYTE*>(widget+8)!=1||Word(reinterpret_cast<void*>(widget),0x30)!=reinterpret_cast<DWORD>(handle))return false;
#endif
    options_native_values::Gauge actual{};if(!Gauge(widget,actual))return false;const float target=static_cast<float>(actual.minimum+action.normalized*(actual.maximum-actual.minimum));return NativeGauge(widget,target)&&options_context::Command(movie,"newparam");}
   if(row.kind==Kind::Gauge){const DWORD widget=0xfe1790+(current.nativeMode==1?0x18:action.row==2?0x12728:action.row==3?0x127e8:0);if(widget==0xfe1790)return false;
#ifndef AITD_OPTIONS_CHILDREN_TEST
    if(*reinterpret_cast<BYTE*>(widget+8)!=1||Word(reinterpret_cast<void*>(widget),0x30)!=reinterpret_cast<DWORD>(handle))return false;
#endif
    return options_context::Command(movie,kind==Action::Previous?"slid:arwd":"slid:arwu");}
   if(row.kind!=Kind::Selector)return false;char selector[160]{};sprintf_s(selector,"%s/reglage/param",rowPath);return menu_context::Timeline(movie,selector,kind==Action::Previous?"left":"right");
  }
  if(action.row!=-1)return false;
  if(kind==Action::Confirm){if(!options_context::Command(movie,"opvalid"))return false;if(current.nativeMode==1&&!menu_context::Timeline(movie,rowPath,"valid"))return false;return menu_context::Timeline(movie,page,current.nativeMode==1?"close_valid":"close");}
  if(kind==Action::Cancel)return options_context::Command(movie,"opcancel")&&menu_context::Timeline(movie,page,"close");
  if(kind==Action::Reset){if(current.nativeMode==3&&!menu_context::Timeline(movie,rowPath,"valid"))return false;return NativeString(movie,"/:key","reset")&&NativeString(movie,"/:whatpopup","warningchoice")&&menu_context::Timeline(movie,"/options/pop_up","open")&&menu_context::Timeline(movie,page,current.nativeMode==1?"block":"warning");}
  if(kind==Action::Calibration&&current.nativeMode==3){options_context::NativeInteger(handle,"/:release_button",1);options_context::NativeInteger(handle,"/:mire_options",1);return menu_context::Timeline(movie,"/mire","open")&&NativeString(movie,"/:help_frame","empty")&&menu_context::Timeline(movie,"/options/help","change")&&menu_context::Timeline(movie,page,"mire_in");}
  return false;
 }
 if(current.nativeMode!=2)return false;const bool footer=kind==Action::Confirm||kind==Action::Cancel;if((!footer&&action.row!=0)||(footer&&action.row!=-1&&action.row!=0))return false;const char* selector="/options/languages/param";
 if(kind==Action::FocusRow)return true;
 if(kind==Action::Previous||kind==Action::Next)return menu_context::Timeline(movie,selector,kind==Action::Previous?"left":"right");
 if(kind==Action::Confirm){if(!options_context::Command(movie,"opvalid"))return false;return menu_context::Timeline(movie,selector,"valid");}
 if(kind==Action::Cancel){if(!menu_context::Timeline(movie,selector,"block")||!options_context::Command(movie,"opcancel"))return false;return menu_context::Timeline(movie,"/options/languages","exit");}
 return false;
}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;BOOL good=TRUE;AcquireSRWLockShared(&lock);__try{memcpy(out,&state,sizeof state);}__except(EXCEPTION_EXECUTE_HANDLER){good=FALSE;}ReleaseSRWLockShared(&lock);return good;}
static void SetPresentationReady(BOOL value,DWORD expected){AcquireSRWLockExclusive(&lock);if(value){if(enabled&&(state.flags&3)==3&&state.generation==expected&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=expected;readyTick=GetTickCount();}}else if(ready&&readyGeneration==expected){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);}
static bool CanAct(const OptionsChildrenStateV1& s,DWORD action,LONG row,double normalized){if(action>=32||!(s.actions&(1u<<action)))return false;const auto kind=static_cast<Action>(action);if(kind==Action::Confirm||kind==Action::Cancel||kind==Action::Reset||kind==Action::Calibration){if(row!=-1&&!(s.nativeMode==2&&row==0))return false;for(const auto& footer:s.view.footer)if(footer.action==kind)return footer.enabled!=0;return false;}
 if(row<0||static_cast<DWORD>(row)>=s.view.count||!s.view.rows[row].enabled||s.view.rows[row].locked)return false;if(kind==Action::FocusRow)return true;if(row!=s.view.selected)return false;if(kind==Action::Gauge)return s.view.rows[row].kind==Kind::Gauge&&normalized>=0&&normalized<=1;return (kind==Action::Previous||kind==Action::Next)&&(s.view.rows[row].kind==Kind::Selector||s.view.rows[row].kind==Kind::Gauge);}
static BOOL Queue(DWORD action,LONG row,double normalized,DWORD expected){if(!options_context::Foreground()||!std::isfinite(normalized))return FALSE;AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();const bool accepted=enabled&&ready&&readyGeneration==expected&&state.generation==expected&&(state.flags&3)==3&&!(state.flags&OptionsChildBusy)&&now-state.tick<=250&&now-readyTick<=250&&!pending.action&&CanAct(state,action,row,normalized);if(accepted)pending={action,expected,row,normalized};ReleaseSRWLockExclusive(&lock);return accepted?TRUE:FALSE;}
static bool ConsumesInput(){if(!options_context::Foreground())return false;AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();bool consume=enabled&&ready&&(state.flags&3)==3&&readyGeneration==state.generation&&now-state.tick<=250&&now-readyTick<=250;if(!consume&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))consume=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return consume;}
static void Tick(){OptionsChildrenStateV1 next{};Owner owner{};unsigned char* movie=nullptr;if(!Capture(next,owner,movie)){next={};owner={};movie=nullptr;}AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;if(!generation)++generation;ready=false;}const auto action=pending;pending={};const bool allowed=action.action&&action.generation==generation&&ready&&readyGeneration==generation&&(next.flags&3)==3&&!(next.flags&OptionsChildBusy)&&CanAct(next,action.action,action.row,action.normalized)&&GetTickCount()-readyTick<=250;const DWORD successes=state.dispatches,failures=state.rejected;if(action.action&&(action.action==static_cast<DWORD>(Action::Confirm)||action.action==static_cast<DWORD>(Action::Cancel)||action.action==static_cast<DWORD>(Action::Reset)||action.action==static_cast<DWORD>(Action::Calibration))){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);bool sent=false;if(allowed&&options_context::Foreground())sent=Dispatch(movie,next,action);if(sent&&(action.action==static_cast<DWORD>(Action::Confirm)||action.action==static_cast<DWORD>(Action::Cancel)))ui_transition_timing::Begin(ui_transition_timing::Request::ChildReturn,movie,*reinterpret_cast<unsigned char**>(movie+0x24),(next.flags&OptionsChildInGame)!=0);if(action.action){if(!Capture(next,owner,movie)){next={};owner={};movie=nullptr;}AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;if(!generation)++generation;ready=false;}ReleaseSRWLockExclusive(&lock);}next.size=sizeof next;next.version=1;next.tick=GetTickCount();next.generation=generation;next.dispatches=successes+(sent?1:0);next.rejected=failures+(action.action&&!sent?1:0);AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&OptionsChildVisible))ready=false;ReleaseSRWLockExclusive(&lock);}
[[maybe_unused]] static bool OwnsPaint(std::uintptr_t root) noexcept {
 AcquireSRWLockShared(&lock);
 const bool owned=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&3)==3,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());
 ReleaseSRWLockShared(&lock);return owned;
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){ready=false;closing=false;pending={};previous={};++generation;}ReleaseSRWLockExclusive(&lock);}
}
