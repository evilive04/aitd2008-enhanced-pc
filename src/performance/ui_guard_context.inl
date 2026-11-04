// A renderer capability lease is distinct from route-specific input readiness.
// Full UI roots stay visually replaced while their original AVM keeps running.
#include "ui_guard_state.h"
namespace ui_guard_context {
struct Domain{DWORD movieSlot{},callbackSlot{},callback{},movie{},handle{},root{},ingame{};};
static SRWLOCK lock=SRWLOCK_INIT;static Domain domains[5]{};static bool configured=false,mainEnabled=false,phoneEnabled=false,inventoryEnabled=false;static DWORD stamp{};
static volatile LONG suppressed{},warmup{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static Domain Capture(DWORD movieSlot,DWORD callbackSlot,DWORD callback,bool mode){
 Domain d{};auto* movie=*reinterpret_cast<BYTE**>(movieSlot);if(!movie||Word(reinterpret_cast<void*>(callbackSlot))!=callback||Word(movie,12)!=callbackSlot||!movie[0x65])return d;
 auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return d;const auto root=Word(handle);DWORD pc{},ingame{};
 if(!root||!phone_context::OptionalZero(root,root+200,"fake")||!phone_context::Number(root,root+200,"WhatPlatform",pc,1)||pc!=1||(mode&&!phone_context::Number(root,root+200,"IsMainInGame",ingame,1)))return d;
 return {movieSlot,callbackSlot,callback,reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,ingame};
}
// A stale movie in one domain must not invalidate the other domains.
static Domain SafeCapture(DWORD movieSlot,DWORD callbackSlot,DWORD callback,bool mode){
 __try{return Capture(movieSlot,callbackSlot,callback,mode);}__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
static bool Live(const Domain& d){return d.root&&Word(reinterpret_cast<void*>(d.movieSlot))==d.movie&&Word(reinterpret_cast<void*>(d.callbackSlot))==d.callback&&Word(reinterpret_cast<void*>(d.movie),12)==d.callbackSlot&&*reinterpret_cast<BYTE*>(d.movie+0x65)&&Word(reinterpret_cast<void*>(d.movie),0x24)==d.handle&&Word(reinterpret_cast<void*>(d.handle))==d.root;}
static bool Active(unsigned n){
 const auto flags=Word(reinterpret_cast<void*>(0xff4290));
 // Flags1 alone is ordinary gameplay, even if the MainMenu film is retained.
 if(n==0)return flags>1&&flags<=255;
 if(n==1)return flags==33||flags==34;
 if(n==2)return *reinterpret_cast<BYTE*>(0xff4b7f)==1;
 if(n==4)return *reinterpret_cast<BYTE*>(0xff437c)!=0;
 auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0)return false;
 const auto inv=*reinterpret_cast<float*>(hero+0x1d88),heal=*reinterpret_cast<float*>(hero+0x1e00);return std::isfinite(inv)&&inv>0&&std::isfinite(heal)&&heal<=0;
}
static bool SafeActive(const Domain& domain,unsigned n){
 __try{return Live(domain)&&Active(n);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void Configure(bool value){ui_transition_timing::phoneValid=[](DWORD root){DWORD platform{};return phone_context::OptionalZero(root,root+200,"fake")&&phone_context::Number(root,root+200,"WhatPlatform",platform,1)&&platform==1;};AcquireSRWLockExclusive(&lock);configured=value;mainEnabled=value&&menu_context::enabled&&chapters_context::presenterEnabled&&controls_context::enabled&&recap_context::enabled&&calibration_context::enabled&&episode_end_context::enabled&&loading_context::enabled;phoneEnabled=value&&phone_context::enabled;inventoryEnabled=value&&inventory_ui_context::enabled&&inventory_popup_context::enabled;memset(domains,0,sizeof domains);stamp=0;ui_transition_timing::frontendEnabled=mainEnabled;ui_paint_owner::rendererAvailable=false;ReleaseSRWLockExclusive(&lock);}
static void Tick(){Domain next[5]{};if(configured){
 next[4]=SafeCapture(0xfe14e0,0xfe14e8,0xc77b34,false);
 if(mainEnabled){next[0]=SafeCapture(0xfe1518,0xfe1520,0xc77b68,true);next[1]=SafeCapture(0xfe15f0,0xfe15f8,0xc778f0,false);}
 if(phoneEnabled)next[2]=SafeCapture(0xff4790,0xff47a0,0xc79974,false);
 if(inventoryEnabled)next[3]=SafeCapture(0x1081490,0x1081494,0xcbe81c,false);
 }
 AcquireSRWLockExclusive(&lock);memcpy(domains,next,sizeof next);stamp=GetTickCount();ReleaseSRWLockExclusive(&lock);
}
// A fixed movie keeps its visual identity during lost input focus or a slow
// transition. Validate live class/movie/handle/root and UI activity on every
// call; expiring a keyboard-tick timestamp would reveal the original again.
static bool OwnsPaint(std::uintptr_t root)noexcept{
 if(!root||!configured)return false;bool result=false;AcquireSRWLockShared(&lock);
 for(unsigned n=0;n<5;++n)if(domains[n].root==root&&SafeActive(domains[n],n)){result=true;break;}
 ReleaseSRWLockShared(&lock);
 if(!result)return false;if(!ui_paint_owner::rendererAvailable){InterlockedIncrement(&warmup);return false;}InterlockedIncrement(&suppressed);return true;
}
static BOOL SetAvailable(BOOL value,DWORD version){if(version!=1||!configured)return FALSE;ui_paint_owner::rendererAvailable=value!=FALSE;return TRUE;}
static BOOL GetState(void* output,DWORD bytes){if(!output||bytes!=sizeof(UiGuardStateV1)||!configured)return FALSE;UiGuardStateV1 s{};s.tick=GetTickCount();
 AcquireSRWLockShared(&lock);for(unsigned n=0;n<5;++n)if(SafeActive(domains[n],n)){s.roots[n]=domains[n].root;s.flags|=UiGuardActive|UiGuardPointer;if(n==0&&!domains[n].ingame)s.flags|=UiGuardFrontend;}ReleaseSRWLockShared(&lock);
 // General confirmation movies have their own exact typed paint ownership.
 if(*reinterpret_cast<BYTE*>(0xff437c))s.flags|=UiGuardActive|UiGuardPointer;
 MenuStateV1 menu{};if(menu_context::GetState(&menu,sizeof menu)&&s.tick-menu.tick<=250){if(menu.flags&MenuTitleVisible){s.flags|=UiGuardTitle;s.flags&=~UiGuardPointer;}if((menu.flags&MenuRootVisible)&&InterlockedCompareExchange(&menu_context::presentationReady,0,0))s.flags|=UiGuardMenuPaint;}
 if(ui_paint_owner::rendererAvailable)s.flags|=UiGuardRenderer;s.suppressed=static_cast<DWORD>(InterlockedCompareExchange(&suppressed,0,0));s.warmup=static_cast<DWORD>(InterlockedCompareExchange(&warmup,0,0));
 __try{memcpy(output,&s,sizeof s);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;
}
}
