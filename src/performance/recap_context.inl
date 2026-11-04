// Native replay preview remains on its original decoder, audio and material.
// Only source-bound video shape673 survives the complete chrome replacement.
#include "recap_state.h"
#include "recap_native_actions.h"
#include "dialog_context.h"
#include "native_recap_cancel.h"
namespace recap_context {
static constexpr DWORD VideoShapeFnv=0x50d70ad2u;
struct Owner{DWORD movie{},handle{},root{},panel{},videoShape{};};
static volatile DWORD acceptedChoiceEvents{},lastChoiceMode{},lastChoiceAction{},lastChoiceResult{},lastChoiceOwner{},lastClearReason{};
static Owner playOwner{};static DWORD priorDialogGeneration{};static bool returnedNo=false;
static bool enabled=false,ready=false,closing=false;static SRWLOCK lock=SRWLOCK_INIT;
static RecapStateV1 state{};static Owner previous{};static DWORD generation{},readyGeneration{},readyTick{},closingTick{},queued{},queuedGeneration{},captureStage{};
static DWORD Word(DWORD p,unsigned at=0){return *reinterpret_cast<const DWORD*>(p+at);}
static void ClearPlay(DWORD reason=0){if(playOwner.movie)lastClearReason=reason;playOwner={};priorDialogGeneration=0;returnedNo=false;}
static bool Dialog(DialogStateV1& dialog){
#ifdef AITD_RECAP_CONTEXT_TEST
 return TestDialog(dialog);
#else
 return dialog_context::GetState(&dialog,sizeof dialog)!=FALSE;
#endif
}
// Record accepted original choices before CloseNative can clear its owner/result.
static void ObserveAcceptedChoice(DWORD dialogGeneration,DWORD mode,DWORD action,DWORD result,DWORD nativeOwner)noexcept{
 ++acceptedChoiceEvents;lastChoiceMode=mode;lastChoiceAction=action;lastChoiceResult=result;lastChoiceOwner=nativeOwner;
 if(!enabled||!playOwner.movie||!dialogGeneration||dialogGeneration==priorDialogGeneration||mode!=3||action!=5||nativeOwner!=0xff5500)return;
 __try{
  if(Word(0xfe1518)!=playOwner.movie||Word(playOwner.movie,0x24)!=playOwner.handle||Word(playOwner.handle)!=playOwner.root||!*reinterpret_cast<BYTE*>(playOwner.movie+0x65)){ClearPlay();return;}
  if(result==1){native_recap_cancel::Clear();ClearPlay(2);}else if(result==2){returnedNo=true;native_recap_cancel::Bind(playOwner.movie,playOwner.handle,playOwner.root);}
 }__except(EXCEPTION_EXECUTE_HANDLER){ClearPlay();}
}
// Native555430 action5 returns No through back_warning_to_skip and state30;
// original movie360..373 then returns to interactive skip_stop199, not state35.
static void ObserveNoReturn(){if(!playOwner.movie)return;
 __try{
  const DWORD system=Word(0xff5508)&0x7fffffff;
  if(!enabled||Word(0xff4290)!=9||Word(0xfe1518)!=playOwner.movie||
     Word(playOwner.movie,0x24)!=playOwner.handle||Word(playOwner.handle)!=playOwner.root||!*reinterpret_cast<BYTE*>(playOwner.movie+0x65)){ClearPlay();return;}
  const bool modal=*reinterpret_cast<BYTE*>(0xff437c)!=0;const DWORD frame=Word(playOwner.root+200,0x60);
  // Original end_skip_close334 emits skpopval then STOP. Its native prompt
  // initialization passes through6 before34; do not discard the armed Play.
  // This retains provenance only: Capture still rejects all state6 surfaces.
  if(system!=30&&system!=34&&system!=35&&!(system==6&&frame==334&&!returnedNo)){ClearPlay(0x20000u|system);return;}

  // warning_to_skip starts before the generic modal becomes active. Preserve
  // this Play only through that original322..359 skip_close/opening and360..373 No return.
  if(!modal&&frame!=199&&!(frame>=322&&frame<=359)&&!(returnedNo&&frame>=360&&frame<=373))ClearPlay(0x10000u|frame);
 }__except(EXCEPTION_EXECUTE_HANDLER){ClearPlay();}
}
static bool Number(DWORD root,DWORD node,const char* name,DWORD& value,DWORD maximum=64){return phone_context::Number(root,node,name,value,maximum);}
static bool Capture(RecapStateV1& out,Owner& owner){out={};owner={};captureStage=1;if(!enabled)return false;
 __try{
  const DWORD system=Word(0xff5508)&0x7fffffff;
  if(Word(0xfe1520)!=0xc77b68||Word(0xff4290)!=9||(system!=35&&system!=30)||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(0xff4390)))return false;
  const DWORD movie=Word(0xfe1518);if(!movie||Word(movie,12)!=0xfe1520||!*reinterpret_cast<BYTE*>(movie+0x65))return false;
  const DWORD handle=Word(movie,0x24),root=Word(handle),scope=root+200;captureStage=2;DWORD platform{},released{},zoomed{},panel{},video{},image{};
  if(system==30&&!native_recap_cancel::Matches(movie,handle,root))return false;
  if(!root||Word(scope,0x60)!=199||!phone_context::OptionalZero(root,scope,"fake")||!Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     !phone_context::OptionalZero(root,scope,"pc_config_xbox")||!phone_context::OptionalZero(root,scope,"pc_pgen")||
     !Number(root,scope,"release_button",released,1)||!Number(root,scope,"zoomed",zoomed,1)||zoomed)return false;
  captureStage=3;auto* h=reinterpret_cast<unsigned char*>(handle);
  if(!phone_context::Resolve(h,"/pannel",626,panel)||!phone_context::Resolve(h,"/video",679,video)||!phone_context::Resolve(h,"/video/video",678,image))return false;
  // Original buttons are dispatched directly; never play appended adapter frames.
  const DWORD panelFrames=Word(Word(panel,0x18),12)&0xffff;
  if(panelFrames!=26&&panelFrames!=31)return false;
  bool clickBusy=false;const char* clickPaths[]={"/pannel/previous","/pannel/backward","/pannel/play","/pannel/forward","/pannel/next"};
  for(const auto* path:clickPaths){DWORD click{};if(!phone_context::Resolve(h,path,620,click))return false;clickBusy=clickBusy||*reinterpret_cast<BYTE*>(click+0x68)||Word(click,0x64)!=2;}
  captureStage=4;
  if(!Number(root,scope,"num_episode",out.episode)||!Number(root,scope,"nb_episode",out.episodes)||!out.episode||out.episode>out.episodes||
     !Number(root,scope,"num_sequence",out.sequence,6)||!Number(root,scope,"nb_sequences",out.sequences,6)||!out.sequence||out.sequence>out.sequences||
     !Number(root,scope,"num_chapter",out.chapter,6)||!out.chapter||!Number(root,scope,"video_loaded",out.videoLoaded,1))return false;
  char name[48]{};sprintf_s(name,"nb_chapters_%lu",out.sequence);if(!Number(root,scope,name,out.chapters,6)||!out.chapters||out.chapter>out.chapters)return false;
  DWORD gauge{},sequence{},locked{};if(!phone_context::Resolve(h,"/video/gauge",671,gauge))return false;
  sprintf_s(name,"/video/gauge/seq_%lu",out.sequence);if(!phone_context::Resolve(h,name,667,sequence))return false;
  sprintf_s(name,"chlk_%lu",out.chapter);if(!Number(root,sequence,name,locked,1))return false;
  captureStage=5;
  if(!controls_context::Text(root,scope,"m_Chapters",out.title,128)||!controls_context::Text(root,scope,"episode_title",out.episodeTitle,128)||
     !controls_context::Text(root,scope,"sequence_title",out.sequenceTitle,128)||!controls_context::Text(root,scope,"seq_summary",out.summary,768,true)||
     !controls_context::Text(root,scope,"m_Back",out.back,64)||!controls_context::Text(root,scope,"m_Select",out.play,64))return false;
  // Resource673 is the original textured video quad, never generic UI art.
  const DWORD dictionary=Word(root,0x74),shape=dictionary?Word(dictionary,673*4):0;
  if(!shape||Word(shape,8)!=673||Word(shape,4)-shape!=140)return false;
  DWORD fnv=2166136261u;for(unsigned n=8;n<140;++n)fnv=(fnv^*reinterpret_cast<const BYTE*>(shape+n))*16777619u;
  if(fnv!=VideoShapeFnv)return false;
  if(Word(0xfe1518)!=movie||Word(movie,0x24)!=handle||Word(handle)!=root)return false;
  // Original sprite679 frame38 explicitly clears release_button and STOPs,
  // just as frame16 is a completed preview. Episode-boundary previews settle
  // at38; treating that stopped phase as loading leaves all arrows blocked.
  const DWORD previewFrame=Word(video,0x60);
  const bool previewReady=(previewFrame==16||previewFrame==38)&&Word(video,0x64)==2&&!*reinterpret_cast<BYTE*>(video+0x68);
  out.flags=RecapVisible|(locked?RecapLocked:0)|((released||clickBusy||*reinterpret_cast<BYTE*>(panel+0x68)||Word(panel,0x64)!=2||!out.videoLoaded||!previewReady||Word(image,0x60)!=34)?RecapBusy:0);
  owner={movie,handle,root,panel,shape};captureStage=100;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){out={};owner={};return false;}
}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;BOOL ok=TRUE;AcquireSRWLockShared(&lock);__try{memcpy(out,&state,bytes);}__except(EXCEPTION_EXECUTE_HANDLER){ok=FALSE;}ReleaseSRWLockShared(&lock);return ok;}
static bool Fresh(){const DWORD now=GetTickCount();return enabled&&ready&&(state.flags&RecapVisible)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();if(value&&enabled&&(state.flags&RecapVisible)&&epoch==state.generation&&now-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=now;}else if(!value&&ready&&epoch==readyGeneration){ready=false;closing=true;closingTick=now;}ReleaseSRWLockExclusive(&lock);}
static BOOL Queue(DWORD action,DWORD epoch){if(action<1||action>6||!phone_context::Foreground())return FALSE;AcquireSRWLockExclusive(&lock);const bool accepted=Fresh()&&(action==RecapBack||!(state.flags&RecapBusy))&&!(action==RecapPlay&&(state.flags&RecapLocked))&&epoch==state.generation&&!queued;if(accepted){queued=action;queuedGeneration=epoch;}ReleaseSRWLockExclusive(&lock);return accepted?TRUE:FALSE;}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=root&&root==previous.root&&Fresh();ReleaseSRWLockShared(&lock);return yes;}
static bool OwnsVideo(std::uintptr_t root,std::uintptr_t shape)noexcept{AcquireSRWLockShared(&lock);const bool yes=root==previous.root&&shape==previous.videoShape&&Fresh();ReleaseSRWLockShared(&lock);return yes;}
static bool ConsumesInput(){if(!phone_context::Foreground())return false;AcquireSRWLockExclusive(&lock);bool yes=Fresh();if(!yes&&closing){if(GetTickCount()-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))yes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return yes;}
// Native928310 dispatches DefineButton conditions with EAX=resource, DI=event.
// Native9250c0 scopes root+6c to the button's parent before calling it.
#ifndef AITD_RECAP_CONTEXT_TEST
static __declspec(noinline) void NativeRelease(DWORD button){
 __asm {
  push edi
  mov eax,button
  mov edi,8
  mov ecx,928310h
  call ecx
  pop edi
 }
}
#endif
static DWORD ReleaseResource(const Owner& o,DWORD action){
 if(action<2||action>6)return 0;
 const auto& spec=recap_native_actions::specs[action-2];
 const DWORD dict=Word(o.root,0x74),button=dict?Word(dict,spec.id*4):0;
 if(!button||Word(button)!=recap_native_actions::ButtonVtable||Word(button,8)!=spec.id||Word(button,4)-button!=spec.bytes)return 0;
 const DWORD relative=Word(button,0x10);if(relative>spec.bytes-10)return 0;
 unsigned cursor=relative+4,releases=0;bool ended=false;
 for(unsigned n=0;n<16&&cursor+6<=spec.bytes;++n){
  const auto* record=reinterpret_cast<const WORD*>(button+cursor);const unsigned size=record[0],condition=record[1];
  const unsigned end=size?cursor+size:spec.bytes;if(end<cursor+6||end>spec.bytes)return 0;
  if(condition&8){if(condition!=8||end-cursor-4!=spec.releaseBytes)return 0;
   DWORD fnv=2166136261u;for(unsigned i=cursor+4;i<end;++i)fnv=(fnv^*reinterpret_cast<const BYTE*>(button+i))*16777619u;
   if(fnv!=spec.fnv)return 0;++releases;}
  if(!size){ended=true;break;}cursor=end;
 }
 return ended&&releases==1?button:0;
}
static bool Dispatch(const Owner& o,DWORD action){
 auto* movie=reinterpret_cast<unsigned char*>(o.movie);
 if(action==RecapBack){ClearPlay();return menu_context::Timeline(movie,"/","to_icon");}
 const DWORD button=ReleaseResource(o,action);if(!button)return false;
 if(action==RecapPlay){native_recap_cancel::Clear();ClearPlay();playOwner=o;DialogStateV1 dialog{};if(Dialog(dialog))priorDialogGeneration=dialog.generation;}
 const DWORD oldRoot=Word(0xfd63e0),oldMovie=Word(0xfd63e4),oldTarget=Word(o.root,0x6c);
 __try{
  *reinterpret_cast<DWORD*>(0xfd63e0)=o.root;*reinterpret_cast<DWORD*>(0xfd63e4)=o.root;*reinterpret_cast<DWORD*>(o.root+0x6c)=o.panel;
#ifdef AITD_RECAP_CONTEXT_TEST
  TestNativeRelease(button);
#else
  NativeRelease(button);
#endif
 }__finally{
  *reinterpret_cast<DWORD*>(o.root+0x6c)=oldTarget;*reinterpret_cast<DWORD*>(0xfd63e0)=oldRoot;*reinterpret_cast<DWORD*>(0xfd63e4)=oldMovie;
 }
 return true;
}
static void Tick(){native_recap_cancel::Reconcile();ObserveNoReturn();RecapStateV1 next{};Owner owner{};Capture(next,owner);AcquireSRWLockExclusive(&lock);
 if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}
 const DWORD action=queued;queued=0;const bool allowed=action&&(next.flags&RecapVisible)&&(action==RecapBack||!(next.flags&RecapBusy))&&!(action==RecapPlay&&(next.flags&RecapLocked))&&queuedGeneration==generation&&Fresh();const DWORD sentBefore=state.dispatches,rejectedBefore=state.rejected;
 if(action){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allowed&&phone_context::Foreground()){__try{sent=Dispatch(owner,action);}__except(EXCEPTION_EXECUTE_HANDLER){sent=false;}}
 if(action==RecapPlay&&!sent)ClearPlay();
 if(action){Capture(next,owner);AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;}ReleaseSRWLockExclusive(&lock);}
 next.size=sizeof next;next.version=1;next.tick=GetTickCount();next.generation=generation;next.dispatches=sentBefore+(sent?1:0);next.rejected=rejectedBefore+(action&&!sent?1:0);
 AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&RecapVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){ready=false;closing=false;queued=0;previous={};native_recap_cancel::Clear();ClearPlay();++generation;}ReleaseSRWLockExclusive(&lock);dialog_context::SetChoiceObserver(value?ObserveAcceptedChoice:nullptr);}
}
