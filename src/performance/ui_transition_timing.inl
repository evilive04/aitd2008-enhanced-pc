// Include after menu/options contexts. Exact r29 UI thread only.
#include "ui_transition_timing.h"
#include "phone_transition_rules.h"
#include "pause_transition_rules.h"
#include "ui_native_status.h"
namespace ui_transition_timing {
static std::uint32_t Word(const void* ptr,unsigned at=0){return *reinterpret_cast<const std::uint32_t*>(static_cast<const unsigned char*>(ptr)+at);}
static bool Node(unsigned char* handle,const char* path,std::uint32_t& out){
 DWORD binding[2]{};if(!menu_context::Resolve(handle,path,binding)||!binding[0]||binding[1]!=Word(handle))return false;
 out=binding[0];return (Word(reinterpret_cast<void*>(out),0x6c)&1)!=0;
}
static bool Blocked(unsigned char* handle){
 std::uint32_t node{};
 for(const auto* path:{"/options/pop_up","/mire"})if(Node(handle,path,node)&&
    (Word(reinterpret_cast<void*>(node),0x60)!=1||Word(reinterpret_cast<void*>(node),0x64)!=2||*reinterpret_cast<const BYTE*>(node+0x68)))return true;
 return false;
}
static void AdvanceNode(std::uint32_t node,const Rule& rule){
 const auto* object=reinterpret_cast<const void*>(node);const auto* resource=reinterpret_cast<const void*>(Word(object,0x18));if(!resource)return;
 if(!Eligible(rule,Word(resource,8),*reinterpret_cast<const WORD*>(static_cast<const BYTE*>(resource)+12),Word(object,0x60),Word(object,0x64),*reinterpret_cast<const BYTE*>(node+0x68)!=0))return;
 // Same deferred GotoAndPlay scheduling as native922550 +922420. The engine
 // reconstructs the destination display and runs its original action block.
 // No script or callback is synthesized; an existing pending jump is untouched.
 *reinterpret_cast<DWORD*>(node+0x70)=rule.target;
 *reinterpret_cast<BYTE*>(node+0x68)=1;
 *reinterpret_cast<DWORD*>(node+0x64)=1;
 ++advances;lastSprite=rule.sprite;lastFrom=Word(object,0x60);lastTarget=rule.target;
}
static void Advance(unsigned char* handle,const Rule& rule){std::uint32_t node{};if(Node(handle,rule.path,node))AdvanceNode(node,rule);}
static bool PauseSettled(unsigned char* handle){
 std::uint32_t node{};if(!Node(handle,"/menu",node))return false;
 const auto* object=reinterpret_cast<const void*>(node);const auto* resource=reinterpret_cast<const BYTE*>(Word(object,0x18));
 return resource&&Word(resource,8)==330&&*reinterpret_cast<const WORD*>(resource+12)==64&&
        Word(object,0x60)==28&&Word(object,0x64)==2&&!*reinterpret_cast<const BYTE*>(node+0x68);
}
static void FrontendTick(){
 if(!frontendEnabled)return;__try{
 const auto flags=Word(reinterpret_cast<void*>(0xff4290)),state=Word(reinterpret_cast<void*>(0xff5508))&0x7fffffffu;
 if((flags!=2&&flags!=6&&flags!=5&&flags!=9)||state!=35||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))||Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68)return;
 auto* movie=*reinterpret_cast<unsigned char**>(0xfe1518);if(!movie||Word(movie,12)!=0xfe1520||!movie[0x65])return;auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));if(!handle||!Word(handle)||Blocked(handle))return;
 for(const auto& rule:FrontendRules)Advance(handle,rule);
 if(flags==9){const auto frame=Word(reinterpret_cast<void*>(Word(handle)+200),0x60);if(frame>=131&&frame<=173)for(const auto& rule:PauseOpeningRules)Advance(handle,rule);}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void ChapterTick(){
 if(!frontendEnabled)return;__try{
 const auto flags=Word(reinterpret_cast<void*>(0xff4290));if((flags!=33&&flags!=34)||(Word(reinterpret_cast<void*>(0xff5508))&0x7fffffffu)!=35||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))||Word(reinterpret_cast<void*>(0xfe15f8))!=0xc778f0)return;
 auto* movie=*reinterpret_cast<unsigned char**>(0xfe15f0);if(!movie||Word(movie,12)!=0xfe15f8||!movie[0x65])return;auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));if(!handle||!Word(handle))return;
 for(const auto& rule:ChapterRules)Advance(handle,rule);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static bool (*phoneValid)(DWORD)=nullptr;
static void PhoneTick(){
 if(!frontendEnabled)return;__try{
 if(*reinterpret_cast<BYTE*>(0xff4b7f)!=1||Word(reinterpret_cast<void*>(0xff47a0))!=0xc79974||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return;
 auto* movie=*reinterpret_cast<BYTE**>(0xff4790);if(!movie||Word(movie,12)!=0xff47a0||!movie[0x65])return;
 auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle||!Word(handle))return;
 if(!phoneValid||!phoneValid(Word(handle)))return;
 // Resolve each shared path once, rather than once per timeline interval.
 const char* path=nullptr;std::uint32_t node{};
 for(const auto& rule:PhoneRules){if(!path||strcmp(path,rule.path)){path=rule.path;if(!Node(handle,path,node))node=0;}if(node)AdvanceNode(node,rule);}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void Tick(){
 FrontendTick();ChapterTick();PhoneTick();
 if(pending.kind==Request::None)return;
 __try{
  if(GetTickCount()-pending.started>2500||(!pending.inGame&&strcmp(reinterpret_cast<const char*>(0xfae538),"MainMenu"))||
     Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68||
     *reinterpret_cast<void**>(0xfe1518)!=pending.movie||!pending.movie||
     Word(pending.movie,12)!=0xfe1520||!*(static_cast<const BYTE*>(pending.movie)+0x65)||
     Word(pending.movie,0x24)!=reinterpret_cast<DWORD>(pending.handle)||!pending.handle||Word(pending.handle)!=pending.root||
     ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))){Reset();return;}
  if(*reinterpret_cast<BYTE*>(0xff437c)){
   // A genuine Options confirmation remains entirely native. Do not time out
   // while the human considers it, select a response, or accelerate the modal.
   if(pending.kind==Request::OptionsChoiceReturn)pending.started=GetTickCount();else Reset();
   return;
  }
  const auto flags=Word(reinterpret_cast<void*>(0xff4290)),state=Word(reinterpret_cast<void*>(0xff5508))&0x7fffffffu;
  if(pending.inGame&&(flags==1||flags==9)&&state==35&&pending.kind!=Request::ChildReturn){
   auto* handle=static_cast<unsigned char*>(pending.handle);if(Blocked(handle)){Reset();return;}
   const auto frame=Word(reinterpret_cast<void*>(pending.root+200),0x60);
   // Original opt_out at117 removes Options bit2. Before skipmenu at131 sets
   // bit3, native flags are1. That deferred flag request can commit AFTER
   // frame131 (observed132). Wait within this exact return through172, but
   // never advance until flags9. Other flags1 routes still reset.
   if(flags==1){if(frame<117||frame>=173)Reset();else Advance(handle,PauseBridgeRules[0]);return;}
   if(frame<131||frame>173){Reset();return;} // Never accelerate a later pause action.
   if(frame==173&&PauseSettled(handle)){Reset();return;}
   Advance(handle,PauseBridgeRules[1]);
   for(const auto& rule:PauseReturnRules)Advance(handle,rule);
   for(const auto& rule:PauseChildRules)Advance(handle,rule);
   return;
  }
  // Native paused Options use5. Closing load stage5 can remain latched for
  // the whole playable scene; neither should disable the original source-bound
  // return rules. Other modal/menu states and active loading still refuse them.
  if(pending.inGame?((flags!=5&&flags!=6)||state!=35):((flags!=2&&flags!=6)||(state!=34&&state!=35))){Reset();return;}
  auto* handle=static_cast<unsigned char*>(pending.handle);if(Blocked(handle)){Reset();return;}
  if(pending.kind==Request::ChildReturn){for(const auto& rule:ChildRules)Advance(handle,rule);}
  else {for(const auto& rule:OptionsRules)Advance(handle,rule);}
 }__except(EXCEPTION_EXECUTE_HANDLER){Reset();}
}
}
