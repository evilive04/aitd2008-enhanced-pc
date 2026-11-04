// Exact-r29 phone home. UI-thread-only native actions; renderer gets copied data.
#include "phone_state.h"
#include "options_native_values.h"
#include "ui_paint_owner.h"
#include "ui_native_status.h"
#include "phone_timeline_policy.h"
namespace phone_context {
struct Key {DWORD movie,handle,root,menu,page;};
static SRWLOCK lock=SRWLOCK_INIT;static PhoneStateV1 state{};static Key previous{};
static bool enabled=false,ready=false,closing=false,escapeHeld=false;
static bool (*subpageInputOwned)()=nullptr;
static DWORD generation{},readyGeneration{},readyTick{},queued{},queuedGeneration{},closingTick{};
static DWORD directMapStarted{};static bool directMapSent=false;
static DWORD closeStarted{};static bool closeBackSent=false,closeExitSent=false;
static void RequestDirectMap(){closeStarted=0;closeBackSent=closeExitSent=false;directMapStarted=GetTickCount();directMapSent=false;}
static void RequestClosePhone(){directMapStarted=0;directMapSent=false;closeStarted=GetTickCount();closeBackSent=closeExitSent=false;}
static DWORD Word(const void* p,unsigned offset=0){return *reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(p)+offset);}
[[maybe_unused]] static bool Read(void*,std::uint32_t at,void* out,std::size_t bytes){__try{memcpy(out,reinterpret_cast<void*>(at),bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool Foreground(){
#ifdef AITD_PHONE_CONTEXT_TEST
 return TestForeground();
#else
 return options_context::Foreground();
#endif
}
static bool Value(DWORD root,DWORD node,const char* name,dialog_text::Value& out,bool optional=false){
#ifdef AITD_PHONE_CONTEXT_TEST
 (void)optional;
 return TestValue(root,node,name,out);
#else
 return options_native_values::Scoped({Read,nullptr},root,node,name,out,optional);
#endif
}
static bool Number(DWORD root,DWORD node,const char* name,DWORD& out,DWORD maximum=65535){dialog_text::Value v{};std::uint32_t n{};if(!Value(root,node,name,v)||!dialog_text::Unsigned(v,maximum,n))return false;out=n;return true;}
static bool OptionalZero(DWORD root,DWORD node,const char* name){
 dialog_text::Value v{};if(!Value(root,node,name,v,true))return false;if(v.kind==dialog_text::Kind::Missing)return true;
 std::uint32_t n{};return dialog_text::Unsigned(v,1,n)&&!n;
}
static bool Equal(DWORD root,DWORD node,const char* name,const char* expected){dialog_text::Value v{};return Value(root,node,name,v)&&v.kind==dialog_text::Kind::String&&!strcmp(v.text,expected);}
static bool SubpageRoute(DWORD root,DWORD scope,DWORD page){
 static const char* pages[]={"home","gps","messages","parameters","phone","index"};
 if(!page||page>5)return false;
 if(Equal(root,scope,"whatpage",pages[page]))return true;
 // Message lists/readers retain the selected original contact/category as
 // whatpage. Requiring the parent literal "messages" removed the fallback
 // Escape route exactly when one of these content captures failed.
 if(page==2)for(const char* route:{"bungle","hartford","sarah","tips","no_one","documents"})if(Equal(root,scope,"whatpage",route))return true;
 return false;
}
static bool Text(DWORD root,const char* name,wchar_t* out,int capacity){
 dialog_text::Value v{};if(!Value(root,root+200,name,v)||v.kind!=dialog_text::Kind::String||!v.text[0])return false;
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.text,-1,out,capacity);if(!n)n=MultiByteToWideChar(CP_ACP,0,v.text,-1,out,capacity);return n!=0;
}
static bool Resolve(unsigned char* handle,const char* path,DWORD expected,DWORD& node){
 DWORD binding[2]{};if(!menu_context::Resolve(handle,path,binding)||!binding[0]||binding[1]!=Word(handle))return false;
 node=binding[0];auto* resource=reinterpret_cast<void*>(Word(reinterpret_cast<void*>(node),0x18));
 return resource&&Word(resource,8)==expected&&(Word(reinterpret_cast<void*>(node),0x6c)&1)!=0;
}
static bool Capture(PhoneStateV1& s,Key& key,unsigned char*& movie){
 s={};key={};movie=nullptr;
 __try{
  if(!enabled||*reinterpret_cast<BYTE*>(0xff4b7f)!=1||Word(reinterpret_cast<void*>(0xff47a0))!=0xc79974)return false;
  movie=*reinterpret_cast<unsigned char**>(0xff4790);
  if(!movie||Word(movie,12)!=0xff47a0||!movie[0x65])return false;
  auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));if(!handle||!Word(handle))return false;
  const DWORD root=Word(handle),scope=root+200;s.flags=PhoneAuthenticated;s.nativePage=Word(reinterpret_cast<void*>(0xff4b88));
  if(*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return true;
  if(s.nativePage){
   DWORD platform{},release{};
   if(s.nativePage<=5&&OptionalZero(root,scope,"fake")&&Number(root,scope,"WhatPlatform",platform,1)&&platform==1&&
      Number(root,scope,"release_button",release,1)&&!release&&SubpageRoute(root,scope,s.nativePage)){
    key={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,0,s.nativePage};s.flags|=PhoneNativeSubpage;
   }
   return true;
  }
  DWORD platform{},release{},menu{},count{},objective{},network{};
  if(!Equal(root,scope,"whatpage","home")||!OptionalZero(root,scope,"fake")||
     !Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     !Number(root,scope,"release_button",release,1)||!Resolve(handle,"/menu",166,menu)||
     !Number(root,menu,"nb_ic",count,6)||count!=5)return true;
  const auto frame=Word(reinterpret_cast<void*>(menu),0x60);
  if(!phone_timeline_policy::HomePaint(frame))return true;
  const char* routes[]={"gps","messages","parameters","phone","index"};
  const char* labels[]={"m_gps","m_messages","m_Parameters","m_phone","m_index"};
  unsigned enabledCount=0;
  for(unsigned n=0;n<count;++n){
   char route[24]{},locked[24]{};sprintf_s(route,"whereto%u",n+1);sprintf_s(locked,"lock%u",n+1);DWORD nativeLock{};
   if(!Equal(root,menu,route,routes[n])||!Number(root,menu,locked,nativeLock,1)||!Text(root,labels[n],s.items[n].label,128))return true;
   s.items[n].nativeSlot=n+1;s.items[n].kind=n+1;s.items[n].enabled=nativeLock?0:1;enabledCount+=s.items[n].enabled;
  }
  if(!enabledCount||!Text(root,"m_home",s.title,128)||!Text(root,"m_exit",s.closeText,96)||!Text(root,"m_Valid",s.openText,96)||
     !Number(root,scope,"is_objective",objective,1))return true;
  if(objective){if(!Text(root,"m_objective",s.objective,768))return true;
   // Keep the actual localized objective body. The short UI caption is ours;
   // never substitute a placeholder objective if native capture is incomplete.
   if(!Text(root,"m_objective_title",s.objectiveTitle,96))return true;s.flags|=PhoneObjective;
  }
  if(Number(root,scope,"network",network,1)){s.flags|=PhoneNetworkKnown;if(network)s.flags|=PhoneNetwork;}
  s.count=count;DWORD selected{};if(!Number(root,menu,"num_ic",selected,5)||!selected)return true;s.selected=selected-1;
  if(release||!phone_timeline_policy::HomeInteractive(frame)||
     Word(reinterpret_cast<void*>(menu),0x64)!=2||*reinterpret_cast<BYTE*>(menu+0x68))s.flags|=PhoneBusy;
  if(*reinterpret_cast<unsigned char**>(0xff4790)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root)return false;
  key={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,menu,0};s.flags|=PhoneHome;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};key={};return false;}
}
static __declspec(noinline) bool String(unsigned char* movie,const char* path,const char* text){
#ifdef AITD_PHONE_CONTEXT_TEST
 return TestString(movie,path,text);
#else
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));
 // Same retained-string ABI as the proven Options bridge. Static strings only.
 __asm{mov eax,handle
       mov ecx,text
       push path
       mov edx,91f430h
       call edx}
 return true;
#endif
}
static bool Dispatch(unsigned char* movie,DWORD action,DWORD page){
 __try{
  if(action==PhoneClose){
   // The original exit_* labels close the complete phone. Back_home instead
   // rebuilds its home surface; its homestop callback performs5517B0(0),
   // including the previous map/message/call mode's native cleanup.
   return page<=5&&menu_context::Timeline(movie,"/",page?"back_home":"exit_home");
  }
  if(action<1||action>5)return false;
  static const char* routes[]={"gps","messages","parameters","phone","index"};
  auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));char icon[40]{};sprintf_s(icon,"/menu/ic_%u",action);
  // Original home buttons152..156/Enter160 set these same variables, then play
  // the selected icon's valid timeline. Its final frame advances the parent
  // exit, homestop/home_out callbacks and the native target page.
  options_context::NativeInteger(handle,"/menu/:num_ic",action);
  options_context::NativeInteger(handle,"/:ic_tmp",action);
  if(!String(movie,"/:where_to",routes[action-1]))return false;
  options_context::NativeInteger(handle,"/:release_button",1);
  return menu_context::Timeline(movie,icon,"valid");
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static BOOL GetState(void* out,DWORD bytes){
 if(!out||bytes<sizeof state||!enabled)return FALSE;PhoneStateV1 copy{};AcquireSRWLockShared(&lock);copy=state;ReleaseSRWLockShared(&lock);
 if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;
}
static void SetPresentationReady(BOOL value,DWORD expected){
 AcquireSRWLockExclusive(&lock);if(value&&(state.flags&PhoneHome)&&state.generation==expected&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=expected;readyTick=GetTickCount();}else if(readyGeneration==expected)ready=false;ReleaseSRWLockExclusive(&lock);
}
static BOOL Queue(DWORD action,DWORD expected){
 if(action<1||action>PhoneClose||action==6||!Foreground())return FALSE;
 AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();
 const bool slot=action==PhoneClose||(action<=state.count&&state.items[action-1].enabled);
 const bool accepted=enabled&&ready&&(state.flags&PhoneHome)&&!(state.flags&PhoneBusy)&&slot&&state.generation==expected&&readyGeneration==expected&&now-state.tick<=250&&now-readyTick<=250&&!queued;
 if(accepted){queued=action;queuedGeneration=expected;}ReleaseSRWLockExclusive(&lock);return accepted?TRUE:FALSE;
}
static bool OwnsPaint(std::uintptr_t root)noexcept{
 AcquireSRWLockShared(&lock);const bool owns=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&PhoneHome)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return owns;
}
static bool ConsumesInput(){
 if(!Foreground())return false;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();
 bool owns=enabled&&ready&&(state.flags&PhoneHome)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;
 if(!owns&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))owns=true;else closing=false;}
 ReleaseSRWLockExclusive(&lock);return owns;
}
static void Tick(){
 PhoneStateV1 next{};Key key{};unsigned char* movie{};Capture(next,key,movie);
 if(closeStarted&&(!(next.flags&PhoneAuthenticated)||GetTickCount()-closeStarted>3000)){closeStarted=0;closeBackSent=closeExitSent=false;}
 const bool closeSubpage=closeStarted&&!closeBackSent&&!closeExitSent&&(next.flags&PhoneNativeSubpage);
 const bool closeHome=closeStarted&&!closeExitSent&&(next.flags&PhoneHome)&&!(next.flags&PhoneBusy);
 if(directMapStarted&&(GetTickCount()-directMapStarted>3000||next.nativePage==1)){directMapStarted=0;directMapSent=false;}
 const bool directMap=directMapStarted&&!directMapSent&&(next.flags&PhoneHome)&&!(next.flags&PhoneBusy)&&next.count&&next.items[0].kind==static_cast<DWORD>(PhoneItemKind::Gps)&&next.items[0].enabled;
 AcquireSRWLockExclusive(&lock);if(memcmp(&key,&previous,sizeof key)){previous=key;++generation;if(!generation)++generation;ready=false;}
 const bool escape=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;
 const bool nativeBack=escape&&!escapeHeld&&(next.flags&PhoneNativeSubpage)&&(!subpageInputOwned||!subpageInputOwned());escapeHeld=escape;
 const auto action=closeSubpage||closeHome?PhoneClose:directMap?1u:nativeBack?PhoneClose:queued,expected=queuedGeneration;queued=0;
 const bool slot=action==PhoneClose||(action&&action<=next.count&&next.items[action-1].enabled);
 const bool allowed=closeSubpage||closeHome||directMap||nativeBack||(action&&slot&&(next.flags&PhoneHome)&&!(next.flags&PhoneBusy)&&expected==generation&&ready&&readyGeneration==generation&&GetTickCount()-readyTick<=250);
 const auto before=state.dispatches,rejected=state.rejected;if(action){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
 const bool sent=allowed&&Foreground()&&Dispatch(movie,action,key.page);
 if(sent&&closeSubpage)closeBackSent=true;if(sent&&closeHome)closeExitSent=true;
 if(directMap&&sent)directMapSent=true;
 if(action){Capture(next,key,movie);AcquireSRWLockExclusive(&lock);if(memcmp(&key,&previous,sizeof key)){previous=key;++generation;if(!generation)++generation;}ReleaseSRWLockExclusive(&lock);}
 next.size=sizeof next;next.version=1;next.tick=GetTickCount();next.generation=generation;next.dispatches=before+(sent?1:0);next.rejected=rejected+(action&&!sent?1:0);
 if(directMapStarted||closeStarted)next.flags|=PhoneDirectMap;
 AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&PhoneHome))ready=false;ReleaseSRWLockExclusive(&lock);
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){ready=false;closing=false;queued=0;previous={};directMapStarted=closeStarted=0;directMapSent=closeBackSent=closeExitSent=false;++generation;}ReleaseSRWLockExclusive(&lock);}
}
