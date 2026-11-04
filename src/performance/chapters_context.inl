// Authentic native chapter/sequence content. The diagnostic observer alone
// never acquires paint/input; the separate presenter feature requires readiness.
#include "chapters_state.h"
#include "chapters_art.h"
namespace chapters_context {
static bool enabled=false;static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{};
static bool presenterEnabled=false,ready=false,interactiveReady=false,closing=false;static DWORD readyGeneration{},readyTick{},closingTick{};
struct Request{DWORD action{},generation{};LONG row=-1;};static Request queued{};
static ChaptersStateV1 state{};
struct Owner{DWORD movie{},handle{},root{},panel{},list{},chapter{};};static Owner previous{};
static ChaptersStateV1 lastStable{};static Owner stableOwner{};static DWORD stableTick{},transitionTick{},transitionRoot{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Capture(ChaptersStateV1& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  const auto flags=Word(reinterpret_cast<void*>(0xff4290));
  if((flags!=34&&flags!=33)||Word(reinterpret_cast<void*>(0xfe15f8))!=0xc778f0||
     (Word(reinterpret_cast<void*>(0xff5508))&0x7fffffff)!=35||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  auto* movie=*reinterpret_cast<unsigned char**>(0xfe15f0);if(!movie||Word(movie,12)!=0xfe15f8||!movie[0x65])return false;
  auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const auto root=Word(handle),scope=root+200;DWORD platform{},release{},panel{},list{},title{};
  if(!root||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
     !phone_context::Number(root,scope,"release_button",release,1)||!phone_context::Resolve(handle,"/menu",121,panel)||!phone_context::Resolve(handle,"/menu/sequences",106,list)||!phone_context::Resolve(handle,"/menu/title",69,title)||
     !phone_context::Number(root,scope,"c_NbChapters",s.chapters,64)||!s.chapters||!phone_context::Number(root,scope,"c_ChapterIndex",s.chapter,s.chapters)||!s.chapter||
     !controls_context::Text(root,scope,"title_chapter",s.title,128)||
     !controls_context::Text(root,scope,"info_lock",s.information,512,true)||!controls_context::Text(root,scope,"m_Back",s.back,64)||!controls_context::Text(root,scope,"m_Select",s.open,64))return false;
  // chapternumber/title_chapter_var are delayed copies in sprite69. Build the
  // number from the current native index and use the canonical native title.
  wchar_t prefix[80]{};if(!controls_context::Text(root,scope,"m_chapter",prefix,80)||swprintf_s(s.number,L"%s %lu",prefix,s.chapter)<0)return false;
  DWORD line{};if(!phone_context::Number(root,list,"nb_lines",s.count,5)||!phone_context::Number(root,list,"total_lines",s.total,4096)||
     !phone_context::Number(root,list,"start_line",s.offset,4096)||!phone_context::Number(root,list,"cur_line",line,4096)||!s.total||s.offset>=s.total||
     s.count!=(std::min<DWORD>)(5,s.total-s.offset)||line<=s.offset||line>s.offset+s.count)return false;s.selected=line-s.offset-1;
  for(DWORD n=0;n<s.count;++n){char path[100]{};DWORD row{},text{};sprintf_s(path,"/menu/sequences/line%lu",n+1);
   if(!phone_context::Resolve(handle,path,95,row)||!phone_context::Number(root,row,"lock",s.rows[n].locked,1))return false;
   sprintf_s(path,"/menu/sequences/line%lu/txt",n+1);if(!phone_context::Resolve(handle,path,81,text)||!controls_context::Text(root,text,"name",s.rows[n].title,128))return false;
  }
  // ChangeChapter is written by original navigation buttons, not a permission.
  // It is legitimately absent at first entry (actual PC v54).
  s.flags=ChaptersVisible|(flags==33?ChaptersInGame:0)|(s.chapters>1?ChaptersCanChange:0);
  if(release||Word(reinterpret_cast<void*>(title),0x60)!=37||Word(reinterpret_cast<void*>(title),0x64)!=2||*reinterpret_cast<BYTE*>(title+0x68)||Word(reinterpret_cast<void*>(panel),0x60)!=32||Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68)||
     Word(reinterpret_cast<void*>(list),0x60)!=9||Word(reinterpret_cast<void*>(list),0x64)!=2||*reinterpret_cast<BYTE*>(list+0x68))s.flags|=ChaptersBusy;
  if(*reinterpret_cast<unsigned char**>(0xfe15f0)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root)return false;
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,list,s.chapter};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static bool TransitionOwner(Owner& owner){owner={};if(!presenterEnabled||!enabled)return false;
 __try{
  const auto flags=Word(reinterpret_cast<void*>(0xff4290));if((flags!=34&&flags!=33)||Word(reinterpret_cast<void*>(0xfe15f8))!=0xc778f0||(Word(reinterpret_cast<void*>(0xff5508))&0x7fffffff)!=35||*reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  auto* movie=*reinterpret_cast<unsigned char**>(0xfe15f0);if(!movie||Word(movie,12)!=0xfe15f8||!movie[0x65])return false;auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const auto root=Word(handle);DWORD platform{},panel{};
  if(!root||!phone_context::OptionalZero(root,root+200,"fake")||!phone_context::Number(root,root+200,"WhatPlatform",platform,1)||platform!=1||!phone_context::Resolve(handle,"/menu",121,panel))return false;
  owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,0,0};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){owner={};return false;}
}
static bool CapturePresentation(ChaptersStateV1& out,Owner& owner){
 const bool captured=Capture(out,owner);if(!presenterEnabled)return captured;const auto now=GetTickCount();
 if(captured&&!(out.flags&ChaptersBusy)){lastStable=out;stableOwner=owner;stableTick=now;transitionTick=now;transitionRoot=owner.root;return true;}
 Owner transition{};if(!TransitionOwner(transition)){lastStable={};stableOwner={};stableTick=transitionRoot=transitionTick=0;return captured;}
 if(transitionRoot!=transition.root){transitionRoot=transition.root;transitionTick=now;}
 if(stableTick&&now-stableTick<=1000&&stableOwner.root==transition.root&&stableOwner.movie==transition.movie&&stableOwner.handle==transition.handle){out=lastStable;out.flags|=ChaptersBusy|ChaptersTransition;owner=stableOwner;return true;}
 if(now-transitionTick<=2000){out={};out.flags=ChaptersVisible|ChaptersBusy|ChaptersTransition|(Word(reinterpret_cast<void*>(0xff4290))==33?ChaptersInGame:0);owner=transition;return true;}
 return captured;
}
#ifndef AITD_CHAPTERS_CONTEXT_TEST
#include "chapters_actions.inl"
#else
static void Tick(){ChaptersStateV1 next{};Owner owner{};if(!CapturePresentation(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
#endif
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){state={};previous={};queued={};lastStable={};stableOwner={};stableTick=transitionTick=transitionRoot=0;ready=interactiveReady=closing=false;++generation;}ReleaseSRWLockExclusive(&lock);}
}
