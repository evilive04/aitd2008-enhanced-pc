#include "native_recap_cancel.h"
// Include after menu_context.inl in the input component. Tick only on the native
// UI thread; copied state, ready feedback and queue are safe across threads.
#include "options_context.h"
#include "dialog_text_capture.h"
#include "ui_paint_owner.h"
#include "options_native_values.h"
#include "options_route_policy.h"
#include "ui_transition_timing.h"
#include "ui_native_status.h"
namespace options_context {
struct Key {DWORD movie{},handle{},root{},options{},ingame{};};
static SRWLOCK lock=SRWLOCK_INIT;
static OptionsStateV1 state{};
static Key previous{};
static DWORD generation=0,queuedAction=0,queuedGeneration=0,readyGeneration=0,readyTick=0,closingTick=0;
static bool enabled=false,ready=false,closing=false;
static DWORD Word(const void* p,unsigned at=0){return *reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(p)+at);}
[[maybe_unused]] static bool Read(void*,std::uint32_t at,void* out,std::size_t bytes){__try{memcpy(out,reinterpret_cast<const void*>(at),bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool Foreground(){
#ifdef AITD_OPTIONS_CONTEXT_TEST
 return TestForeground();
#else
 DWORD pid{};const auto window=GetForegroundWindow();return window&&GetWindowThreadProcessId(window,&pid)&&pid==GetCurrentProcessId();
#endif
}
static bool Variable(DWORD root,const char* name,dialog_text::Value& value){
#ifdef AITD_OPTIONS_CONTEXT_TEST
 return TestVariable(root,name,value);
#else
 return options_native_values::Scoped({Read,nullptr},root,root+200,name,value);
#endif
}
static bool Integer(DWORD root,const char* name,DWORD& value){dialog_text::Value v{};std::uint32_t n{};if(!Variable(root,name,v)||!dialog_text::Unsigned(v,0xffffffffu,n))return false;value=n;return true;}
static bool Visible(unsigned char* handle,const char* path,DWORD* identity=nullptr){DWORD b[2]{};if(!menu_context::Resolve(handle,path,b)||!b[0]||b[1]!=Word(handle)||b[0]==b[1]+200)return false;if(identity)*identity=b[0];return (Word(reinterpret_cast<void*>(b[0]),0x6c)&1)!=0;}
static bool RouteValue(DWORD root,DWORD menu,const char* name,dialog_text::Value& out){
#ifdef AITD_OPTIONS_CONTEXT_TEST
 return TestRouteValue(root,menu,name,out);
#else
 return options_native_values::Scoped({Read,nullptr},root,menu,name,out);
#endif
}
static bool Routes(DWORD root,DWORD menu,bool ingame){
 dialog_text::Value v{};if(!RouteValue(root,menu,"nb_ic",v)||v.kind!=dialog_text::Kind::String||strcmp(v.text,ingame?"3":"4"))return false;
 const char* full[]={"audio","languages","display","configuration"};const char* paused[]={"audio","display","configuration"};
 for(unsigned n=0;n<options_route_policy::Count(ingame);++n){char name[24]{};sprintf_s(name,"whereto_%u",n+1);if(!RouteValue(root,menu,name,v)||v.kind!=dialog_text::Kind::String||strcmp(v.text,ingame?paused[n]:full[n]))return false;}return true;
}
static bool Capture(OptionsStateV1& s,Key& key,unsigned char*& movie,unsigned char*& handle){
 s={};key={};movie=nullptr;handle=nullptr;
 __try{
  if(!enabled||Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68)return false;
  movie=*reinterpret_cast<unsigned char**>(0xfe1518);if(!movie||Word(movie,12)!=0xfe1520||!movie[0x65])return false;
  handle=*reinterpret_cast<unsigned char**>(movie+0x24);if(!handle||!Word(handle))return false;
  DWORD ingame{};if(!Integer(Word(handle),"IsMainInGame",ingame)||ingame>1)return false;
  s.nativeFlags=Word(reinterpret_cast<void*>(0xff4290));s.nativeState=Word(reinterpret_cast<void*>(0xff5508))&0x7fffffff;s.flags=OptionsAuthenticated|(ingame?OptionsInGame:0);
  if((s.nativeFlags!=6&&(!ingame||s.nativeFlags!=5))||!native_recap_cancel::ReadyMenuState(s.nativeState)||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))||Word(reinterpret_cast<void*>(0xfe1790)))return true;
  DWORD options{},menu{};if(!Visible(handle,"/options",&options)||!Visible(handle,"/options/menu",&menu)||!Routes(Word(handle),menu,ingame!=0))return true;
  const char* excluded[]={"/options/audio","/options/languages","/options/display","/options/configuration","/options/config"};for(auto path:excluded)if(Visible(handle,path))return true;
  DWORD nativeLock{},platform{},released{},modified{};if(!Integer(Word(handle),"lockparam",nativeLock)||nativeLock||!Integer(Word(handle),"WhatPlatform",platform)||platform!=1||!Integer(Word(handle),"release_button",released)||released||!Integer(Word(handle),"options_modified",modified)||modified>1)return true;
  const char* names[]={"m_Options","m_audio","m_languages","m_display","m_configuration","m_Back","m_ResetAll"};
  for(unsigned i=0;i<7;++i){if(ingame&&i==2)continue;dialog_text::Value v{};if(!Variable(Word(handle),names[i],v)||v.kind!=dialog_text::Kind::String||!v.text[0])return true;int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.text,-1,s.labels[i],128);if(!n)n=MultiByteToWideChar(CP_ACP,0,v.text,-1,s.labels[i],128);if(!n)return true;}
  // Recheck ownership after the complete copy. No runtime pointers cross DLLs.
  if(*reinterpret_cast<unsigned char**>(0xfe1518)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle))return false;
  key={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),Word(handle),options,ingame};s.flags|=OptionsRootVisible;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};key={};return false;}
}
[[maybe_unused]] static __declspec(noinline) void NativeInteger(unsigned char* handle,const char* path,DWORD value){__asm{mov eax,handle
 push value
 push path
 mov ecx,091f4b0h
 call ecx
}}
static bool SetInteger(unsigned char* handle,const char* path,DWORD value){
#ifdef AITD_OPTIONS_CONTEXT_TEST
 return TestInteger(handle,path,value);
#else
 NativeInteger(handle,path,value);return true;
#endif
}
static bool Command(unsigned char* movie,const char* name){
 // Native549CF0 packs at most eight ASCII bytes, big endian, into EDX:EAX.
 ULONGLONG hash=0;for(unsigned i=0;i<8&&name[i];++i)hash=(hash<<8)|static_cast<BYTE>(name[i]);
#ifdef AITD_OPTIONS_CONTEXT_TEST
 return TestCommand(movie,name,static_cast<DWORD>(hash),static_cast<DWORD>(hash>>32));
#else
 using Native=unsigned char(__thiscall*)(void*,void*,DWORD,DWORD,const char*);
 return reinterpret_cast<Native>(0x53fa40)(reinterpret_cast<void*>(0xfe1520),movie,static_cast<DWORD>(hash),static_cast<DWORD>(hash>>32),name)!=0;
#endif
}
static bool Dispatch(unsigned char* movie,unsigned char* handle,DWORD action,bool ingame=false){
 __try{
  if(action>=OptionsAudio&&action<=OptionsConfiguration){const DWORD slot=options_route_policy::NativeSlot(action,ingame);if(!slot||!SetInteger(handle,"/options/menu/:num_ic",slot))return false;return menu_context::Timeline(movie,"/options/menu","valid");}
  if(action==OptionsResetAll){if(!Command(movie,"rstallpu"))return false;return menu_context::Timeline(movie,"/options","warning");}
  if(action==OptionsBack){DWORD modified{};if(!Integer(Word(handle),"options_modified",modified)||modified>1)return false;if(!SetInteger(handle,"/:release_button",1))return false;if(modified){if(!Command(movie,"pop_opt"))return false;const bool sent=menu_context::Timeline(movie,"/options","warning");if(sent)ui_transition_timing::Begin(ui_transition_timing::Request::OptionsChoiceReturn,movie,handle,ingame);return sent;}const bool sent=menu_context::Timeline(movie,"/options","close");if(sent)ui_transition_timing::Begin(ui_transition_timing::Request::OptionsReturn,movie,handle,ingame);return sent;}
  return false;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof(state))return FALSE;BOOL okay=TRUE;AcquireSRWLockShared(&lock);__try{memcpy(out,&state,sizeof state);}__except(EXCEPTION_EXECUTE_HANDLER){okay=FALSE;}ReleaseSRWLockShared(&lock);return okay;}
static void SetPresentationReady(BOOL value,DWORD expectedGeneration){AcquireSRWLockExclusive(&lock);if(value){if(enabled&&(state.flags&OptionsRootVisible)&&state.generation==expectedGeneration&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=expectedGeneration;readyTick=GetTickCount();}}else if(ready&&readyGeneration==expectedGeneration){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);}
static BOOL Queue(DWORD action,DWORD expectedGeneration){if(action<OptionsAudio||action>OptionsResetAll||!Foreground())return FALSE;AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();const bool accepted=!(action==OptionsLanguages&&(state.flags&OptionsInGame))&&enabled&&ready&&readyGeneration==expectedGeneration&&state.generation==expectedGeneration&&(state.flags&OptionsRootVisible)&&now-state.tick<=250&&now-readyTick<=250&&!queuedAction;if(accepted){queuedAction=action;queuedGeneration=expectedGeneration;}ReleaseSRWLockExclusive(&lock);return accepted?TRUE:FALSE;}
static bool ConsumesInput(){if(!Foreground())return false;AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();bool consumes=enabled&&ready&&(state.flags&OptionsRootVisible)&&readyGeneration==state.generation&&now-state.tick<=250&&now-readyTick<=250;if(!consumes&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))consumes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return consumes;}
static void Tick(){OptionsStateV1 next{};Key key{};unsigned char* movie=nullptr,*handle=nullptr;Capture(next,key,movie,handle);AcquireSRWLockExclusive(&lock);if(memcmp(&key,&previous,sizeof key)){previous=key;++generation;if(!generation)++generation;ready=false;}const DWORD action=queuedAction,expected=queuedGeneration;queuedAction=0;const bool allowed=action&&(next.flags&OptionsRootVisible)&&expected==generation&&ready&&readyGeneration==generation&&GetTickCount()-readyTick<=250;const DWORD dispatches=state.dispatches,rejected=state.rejected;if(action){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allowed&&Foreground())sent=Dispatch(movie,handle,action,(next.flags&OptionsInGame)!=0);if(action){Capture(next,key,movie,handle);AcquireSRWLockExclusive(&lock);if(memcmp(&key,&previous,sizeof key)){previous=key;++generation;if(!generation)++generation;}ReleaseSRWLockExclusive(&lock);}next.size=sizeof next;next.version=1;next.tick=GetTickCount();next.generation=generation;next.dispatches=dispatches+(sent?1:0);next.rejected=rejected+(action&&!sent?1:0);AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&OptionsRootVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
[[maybe_unused]] static bool OwnsPaint(std::uintptr_t root) noexcept {
 AcquireSRWLockShared(&lock);
 const bool owned=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&OptionsRootVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());
 ReleaseSRWLockShared(&lock);return owned;
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){ready=false;closing=false;queuedAction=0;previous={};++generation;}ReleaseSRWLockExclusive(&lock);}
}
