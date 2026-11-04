// Real (fake0) combination choice. Original button349 broadcasts valchoi1/2
// or b_button; its fake-only valid_choice shortcut must not be used here.
#include "inventory_popup_state.h"
#include "inventory_native_paint.h"
#ifndef AITD_INVENTORY_POPUP_TEST
#include "native_ui_command.inl"
#endif
namespace inventory_popup_context {
static bool enabled=false,ready=false,closing=false;static SRWLOCK lock=SRWLOCK_INIT;
static DWORD generation{},readyGeneration{},readyTick{},closingTick{},dispatches{},rejected{};static InventoryPopupStateV1 state{};
struct Owner{DWORD hero{},inventory{},movie{},handle{},root{},panel{},actors[2]{},choices[2]{},kind{},capacity{};inventory_native_paint::Shapes animation{};};static Owner previous{};
static inventory_native_paint::Cache splashCache{},scotchCache{},rechargeCache{},decantCache{};
// Original noninteractive graphics do not borrow a copied dialog's input lease.
// The general UI guard owns the movie even when that dialog cannot be copied.
struct NativeAnimationOwner{DWORD movie{},handle{},root{},panel{},clip{},group{},tick{};inventory_native_paint::Shapes shapes{};};
static NativeAnimationOwner nativeAnimationOwner{};static DWORD captureStage{},nativeCaptureStage{},nativeFrame{},nativeRootFrame{},nativeShortcutsFrame{};
static volatile LONG nativeAdmitted{},nativeRejected{};
struct Request{DWORD action{},row{},generation{};};static Request queued{};static DWORD draftAmount{};static bool draftSet=false;
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Send(BYTE* movie,const char* command){
#ifdef AITD_INVENTORY_POPUP_TEST
 return TestSend(movie,command);
#else
 return native_ui_command::Send(movie,command);
#endif
}
static DWORD ChoiceId(const char* value){const char* names[]={"scotch_items","decant_liquids","decant_bullets","decant_batteries","splash_bullets","scotch"};for(unsigned n=0;n<6;++n)if(!strcmp(names[n],value))return n+1;return 0;}
static bool ChoiceName(const char* value){return ChoiceId(value)!=0;}
static DWORD AnimationGroup(DWORD frame){return frame>=125&&frame<=206?1:frame>=213&&frame<=246?2:frame>=247&&frame<=310?3:frame>=78&&frame<=123?4:0;}
static bool CaptureNativeAnimation(NativeAnimationOwner& out){
 out={};nativeCaptureStage=1;nativeFrame=nativeRootFrame=nativeShortcutsFrame=0;if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0x1081480))!=0xcbe850||Word(reinterpret_cast<void*>(0x1081494))!=0xcbe81c||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  nativeCaptureStage=2;auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0)return false;
  const auto inv=*reinterpret_cast<float*>(hero+0x1d88),heal=*reinterpret_cast<float*>(hero+0x1e00);if(!std::isfinite(inv)||inv<=0||!std::isfinite(heal)||heal>0)return false;
  auto* inventory=reinterpret_cast<BYTE*>(Word(hero,0x810));if(!inventory||Word(inventory,0x754)!=reinterpret_cast<DWORD>(hero))return false;
  nativeCaptureStage=3;auto* movie=*reinterpret_cast<BYTE**>(0x1081490);if(!movie||Word(movie,12)!=0x1081494||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const DWORD root=Word(handle),scope=root+200;if(!root)return false;
  nativeRootFrame=Word(reinterpret_cast<void*>(scope),0x60);DWORD pc{},panel{},clip{},shortcuts{};
  // Authored root inventory23..34 only. Retained clips in gameplay/menu/helper
  // timelines must not regain visibility through this content exception.
  nativeCaptureStage=4;if(nativeRootFrame<23||nativeRootFrame>34||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",pc,1)||pc!=1)return false;
  if(phone_context::Resolve(handle,"/shortcuts",530,shortcuts))nativeShortcutsFrame=Word(reinterpret_cast<void*>(shortcuts),0x60);
  nativeCaptureStage=5;if(!phone_context::Resolve(handle,"/clip_general/ecran_combinaison",434,panel))return false;
  nativeFrame=Word(reinterpret_cast<void*>(panel),0x60);const DWORD group=AnimationGroup(nativeFrame);nativeCaptureStage=6;if(!group)return false;
  static const char* paths[]={"/clip_general/ecran_combinaison/splash","/clip_general/ecran_combinaison/scotch","/clip_general/ecran_combinaison/recharge","/clip_general/ecran_combinaison/decant"};
  static const DWORD definitions[]={337,427,430,417};nativeCaptureStage=7;
  if(!phone_context::Resolve(handle,paths[group-1],definitions[group-1],clip)||!inventory_native_paint::Descendant(clip,panel))return false;
  inventory_native_paint::Shapes shapes{};nativeCaptureStage=8;bool authentic=false;
  if(group==1)authentic=inventory_native_paint::Capture(root,inventory_native_content::splash,shapes,splashCache);
  else if(group==2)authentic=inventory_native_paint::Capture(root,inventory_native_content::scotch,shapes,scotchCache);
  else if(group==3)authentic=inventory_native_paint::Capture(root,inventory_native_content::recharge,shapes,rechargeCache);
  else authentic=inventory_native_paint::Capture(root,inventory_native_content::decant,shapes,decantCache);
  if(!authentic)return false;
  nativeCaptureStage=9;if(*reinterpret_cast<BYTE**>(0x1081490)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||AnimationGroup(Word(reinterpret_cast<void*>(panel),0x60))!=group)return false;
  out={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,clip,group,GetTickCount(),shapes};nativeCaptureStage=100;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){nativeCaptureStage|=0x80000000u;out={};return false;}
}
static bool Capture(InventoryPopupStateV1& s,Owner& owner){s={};owner={};captureStage=1;if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0x1081480))!=0xcbe850||Word(reinterpret_cast<void*>(0x1081494))!=0xcbe81c||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  captureStage=2;auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0)return false;const auto time=*reinterpret_cast<float*>(hero+0x1d88),heal=*reinterpret_cast<float*>(hero+0x1e00);if(!std::isfinite(time)||time<=0||!std::isfinite(heal)||heal>0)return false;auto* inventory=reinterpret_cast<BYTE*>(Word(hero,0x810));if(!inventory||Word(inventory,0x754)!=reinterpret_cast<DWORD>(hero))return false;
  captureStage=3;auto* movie=*reinterpret_cast<BYTE**>(0x1081490);if(!movie||Word(movie,12)!=0x1081494||!movie[0x65])return false;auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const auto root=Word(handle),scope=root+200;if(!root)return false;
  DWORD pc{},panel{},selected{};captureStage=4;if(Word(reinterpret_cast<void*>(scope),0x60)!=34||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",pc,1)||pc!=1||!phone_context::Resolve(handle,"/clip_general/ecran_combinaison",434,panel)||!inventory_ui_context::HandsOnly(handle)||!inventory_ui_context::Dormant(handle,"/tips_inventory",47)||!bleeding_timer::Read(handle,root,s.bleeding))return false;
  const auto frame=Word(reinterpret_cast<void*>(panel),0x60);
  DWORD choiceIds[2]{};bool busy=Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68);
  captureStage=5;if(!controls_context::Text(root,scope,"m_cancel",s.cancel,64)||!controls_context::Text(root,scope,"m_valid",s.confirm,64))return false;captureStage=6;
  if(frame>=8&&frame<=47){
   wcscpy_s(s.title,L"COMBINATION");
   if(!phone_context::Number(root,scope,"num_menu",selected,2)||!selected)return false;
   for(unsigned n=0;n<2;++n){char key[40]{},alias[40]{};sprintf_s(key,"choice_%u",n+1);if(!phone_parameters_context::Raw(root,scope,key,alias,40))return false;if(!alias[0]){if(!n)return false;break;}choiceIds[n]=ChoiceId(alias);if(!choiceIds[n])return false;sprintf_s(key,"m_%s",alias);if(!controls_context::Text(root,scope,key,s.choices[n],512))return false;++s.count;}
   if(selected>s.count)return false;s.selected=selected-1;busy|=frame!=18;
  }else if(frame>=57&&frame<=123){
   char alias[40]{},from[40]{},key[48]{};
   if(!phone_parameters_context::Raw(root,scope,"choice_combinaison",alias,40)||!phone_parameters_context::Raw(root,scope,"what_from",from,40))return false;
   choiceIds[0]=ChoiceId(alias);if(choiceIds[0]!=2&&choiceIds[0]!=3)return false;
   if(strcmp(from,"bottle")&&strcmp(from,"charger")&&strcmp(from,"munition_box"))return false;
   s.kind=2;s.liquid=choiceIds[0]==2;wcscpy_s(s.title,L"TRANSFER");sprintf_s(key,"m_%s",alias);
   if(!controls_context::Text(root,scope,key,s.choices[0],512)||!phone_context::Number(root,scope,s.liquid?"liquid_possible":"bullets_possible",s.capacity,10000)||!phone_context::Number(root,scope,"nb_decant",s.amount,10000)||s.amount>s.capacity)return false;
   s.count=1;busy|=frame!=77;
  }else if(frame>=48&&frame<=56){
   // Original434 pop_exit contains only the fading background. It is shared by
   // Cancel and completed operations; their old choice fields may be unset or
   // stale. Own only that authenticated empty closing panel, never an old item.
   s.kind=3;busy=true;s.flags|=InventoryPopupClosing;wcscpy_s(s.title,L"COMBINATION");
  }else if((frame>=125&&frame<=206)||(frame>=213&&frame<=310)){
   char alias[40]{},key[48]{};
   if(!phone_parameters_context::Raw(root,scope,"choice_combinaison",alias,40))return false;
   choiceIds[0]=ChoiceId(alias);
   if(frame<=206?choiceIds[0]!=5:frame<=246?(choiceIds[0]!=1&&choiceIds[0]!=6):choiceIds[0]!=4)return false;
   sprintf_s(key,"m_%s",alias);if(!controls_context::Text(root,scope,key,s.choices[0],512))return false;
   s.kind=3;s.count=1;busy=true;wcscpy_s(s.title,frame>=247?L"RECHARGING":L"COMBINING");
  }else return false; //207..212 is the original debug ERROR branch, not a numeric dialog.
  if(*reinterpret_cast<BYTE**>(0x1081490)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(panel),0x60)!=frame)return false;
  inventory_native_paint::Shapes animation{};bool nativeAnimation=false;
#ifndef AITD_INVENTORY_POPUP_TEST
  if(busy&&!(s.flags&InventoryPopupClosing)){
   if(s.kind==3&&frame>=125&&frame<=206)nativeAnimation=inventory_native_paint::Capture(root,inventory_native_content::splash,animation,splashCache);
   else if(s.kind==3&&frame>=213&&frame<=246)nativeAnimation=inventory_native_paint::Capture(root,inventory_native_content::scotch,animation,scotchCache);
   else if(s.kind==3&&frame>=247&&frame<=310)nativeAnimation=inventory_native_paint::Capture(root,inventory_native_content::recharge,animation,rechargeCache);
   else if(s.kind==2&&frame>=78&&frame<=123)nativeAnimation=inventory_native_paint::Capture(root,inventory_native_content::decant,animation,decantCache);
  }
#endif
  s.flags|=InventoryPopupVisible|(busy?InventoryPopupBusy:0)|(nativeAnimation?InventoryPopupNativeAnimation:0);s.nativeFrame=frame;owner={reinterpret_cast<DWORD>(hero),reinterpret_cast<DWORD>(inventory),reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,{Word(reinterpret_cast<void*>(0x108149c)),Word(reinterpret_cast<void*>(0x10814b0))},{choiceIds[0],choiceIds[1]},s.kind,s.capacity,animation};captureStage=100;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&InventoryPopupVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool Lease(DWORD now){return enabled&&ready&&(state.flags&InventoryPopupVisible)&&!(state.flags&InventoryPopupBusy)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;}
static BOOL Queue(DWORD action,DWORD row,DWORD epoch){if(action<1||action>4||!options_context::Foreground())return FALSE;AcquireSRWLockExclusive(&lock);const bool yes=Lease(GetTickCount())&&state.generation==epoch&&!queued.action&&(action==3?row==0:state.kind==2?(action==4?row<=state.capacity:action==2&&row>0&&row<=state.capacity):action!=4&&row<state.count);if(yes)queued={action,row,epoch};ReleaseSRWLockExclusive(&lock);return yes?TRUE:FALSE;}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&InventoryPopupVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool OwnsPointer(){AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Fresh(previous.root,previous.root,enabled,(state.flags&InventoryPopupVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool NativeAnimationLive(const NativeAnimationOwner& n,std::uintptr_t root,DWORD now){
 if(!enabled||!n.group||n.group>4||root!=n.root||now-n.tick>250||Word(reinterpret_cast<void*>(0x1081490))!=n.movie||Word(reinterpret_cast<void*>(0x1081494))!=0xcbe81c||!(*reinterpret_cast<BYTE*>(n.movie+0x65))||Word(reinterpret_cast<void*>(n.movie),12)!=0x1081494||Word(reinterpret_cast<void*>(n.movie),0x24)!=n.handle||Word(reinterpret_cast<void*>(n.handle))!=root)return false;
 const auto rootFrame=Word(reinterpret_cast<void*>(root+200),0x60);if(rootFrame<23||rootFrame>34||AnimationGroup(Word(reinterpret_cast<void*>(n.panel),0x60))!=n.group)return false;
 static const DWORD definitions[]={337,427,430,417};const DWORD definition=Word(reinterpret_cast<void*>(n.clip),0x18);if(!definition||Word(reinterpret_cast<void*>(definition),8)!=definitions[n.group-1])return false;
 auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0)return false;const auto inv=*reinterpret_cast<float*>(hero+0x1d88),heal=*reinterpret_cast<float*>(hero+0x1e00);
 return !Word(reinterpret_cast<void*>(0xff4290))&&!*reinterpret_cast<BYTE*>(0xff437c)&&!*reinterpret_cast<BYTE*>(0xff4b7f)&&!ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))&&std::isfinite(inv)&&inv>0&&std::isfinite(heal)&&heal<=0&&(Word(reinterpret_cast<void*>(n.clip),0x6c)&1)&&inventory_native_paint::Descendant(n.clip,n.panel);
}
static bool OwnsNativeAnimation(std::uintptr_t root,std::uintptr_t shape,std::uintptr_t instance)noexcept{
 bool yes=false;AcquireSRWLockShared(&lock);
 __try{yes=NativeAnimationLive(nativeAnimationOwner,root,GetTickCount())&&inventory_native_paint::Contains(nativeAnimationOwner.shapes,root,shape)&&inventory_native_paint::Descendant(instance,nativeAnimationOwner.clip);
  if(root==nativeAnimationOwner.root&&nativeAnimationOwner.group)InterlockedIncrement(yes?&nativeAdmitted:&nativeRejected);
 }__except(EXCEPTION_EXECUTE_HANDLER){yes=false;}
 ReleaseSRWLockShared(&lock);return yes;
}
static bool ConsumesInput(){if(!options_context::Foreground())return false;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();bool yes=enabled&&ready&&(state.flags&InventoryPopupVisible)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;if(!yes&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))yes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return yes;}
static bool Dispatch(const Owner& owner,const Request& request){auto* movie=reinterpret_cast<BYTE*>(owner.movie);if(request.action==3)return Send(movie,"b_button");auto* handle=reinterpret_cast<BYTE*>(owner.handle);
 if(owner.kind==2){
  if(request.action==4)return true;
  if(request.action!=2||!request.row||request.row>owner.capacity)return false;
  options_context::NativeInteger(handle,"/:nb_decant",request.row);
  return Send(movie,owner.choices[0]==2?"liqvalid":"bulvalid");
 }
 char key[32]{},alias[40]{};static char subtitles[2][4096]{};sprintf_s(key,"choice_%lu",request.row+1);if(!phone_parameters_context::Raw(owner.root,owner.root+200,key,alias,40)||!ChoiceName(alias))return false;sprintf_s(key,"m_%s",alias);if(!phone_parameters_context::Raw(owner.root,owner.root+200,key,subtitles[request.row],4096))return false;
 options_context::NativeInteger(handle,"/:num_menu",request.row+1);if(!options_children_context::NativeString(movie,"/:subtitle",subtitles[request.row]))return false;
 return request.action==1||Send(movie,request.row?"valchoi2":"valchoi1");
}
static void Tick(){InventoryPopupStateV1 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;draftSet=false;}const auto r=queued;queued={};const bool allow=r.action&&(next.flags&(InventoryPopupVisible|InventoryPopupBusy))==InventoryPopupVisible&&Lease(GetTickCount())&&r.generation==generation&&(r.action==3||(next.kind==2?(r.action==4?r.row<=next.capacity:r.action==2&&r.row>0&&r.row<=next.capacity):r.action!=4&&r.row<next.count));if(r.action){ready=false;if(allow&&(r.action==2||r.action==3)){closing=true;closingTick=GetTickCount();}}ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allow&&options_context::Foreground()){__try{sent=Dispatch(owner,r);}__except(EXCEPTION_EXECUTE_HANDLER){}}
 if(r.action){sent?++dispatches:++rejected;if(!Capture(next,owner)){next={};owner={};}}
 NativeAnimationOwner nextNative{};CaptureNativeAnimation(nextNative);
 AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;draftSet=false;}
 if(sent&&r.action==4&&owner.kind==2){draftSet=true;draftAmount=r.row;}
 if(next.kind==2&&draftSet)next.amount=draftAmount;
 next.tick=GetTickCount();next.generation=generation;state=next;nativeAnimationOwner=nextNative;if(!(state.flags&InventoryPopupVisible))ready=false;ReleaseSRWLockExclusive(&lock);
#ifndef AITD_INVENTORY_POPUP_TEST
 // One line per admission/rejection phase change, never per animation frame.
 static DWORD lastDiagnostic=~0u;const DWORD diagnostic=(nativeCaptureStage&0xffu)|(captureStage<<8)|(nextNative.group<<16)|(nativeCaptureStage&0x80000000u);
 if(diagnostic!=lastDiagnostic){lastDiagnostic=diagnostic;Log("Native combination paint stage=%08lx copied_stage=%lu root_frame=%lu panel_frame=%lu shortcuts_frame=%lu group=%lu clip=%08lx admitted=%ld rejected=%ld",nativeCaptureStage,captureStage,nativeRootFrame,nativeFrame,nativeShortcutsFrame,nextNative.group,nextNative.clip,InterlockedCompareExchange(&nativeAdmitted,0,0),InterlockedCompareExchange(&nativeRejected,0,0));}
#endif
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};nativeAnimationOwner={};queued={};ready=closing=draftSet=false;++generation;ReleaseSRWLockExclusive(&lock);}
}
