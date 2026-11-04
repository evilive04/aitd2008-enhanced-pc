// Native healing keeps its3D body, selection, held supplies and treatment input.
#include "healing_ui_state.h"
#include <cmath>
namespace healing_ui_context{
static bool enabled=false;static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{};static HealingUiStateV2 state{};
struct Owner{DWORD hero{},component{},entries{},count{};};static Owner previous{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Capture(HealingUiStateV2& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390)))||(Word(reinterpret_cast<void*>(0xfda5c8))&0x30))return false;
  auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0||!Word(hero,0x95c))return false;
  const float heal=*reinterpret_cast<float*>(hero+0x1e00),inv=*reinterpret_cast<float*>(hero+0x1d88);if(!std::isfinite(heal)||heal<=0||!std::isfinite(inv)||inv>0)return false;
  const auto component=Word(hero,0xf94);if(!component||Word(reinterpret_cast<void*>(component))!=0xc89088)return false;
  const auto count=Word(reinterpret_cast<void*>(component),0xc),entries=Word(reinterpret_cast<void*>(component),0x10),selected=Word(reinterpret_cast<void*>(component),0x13c);
  if(count>64||(count&&!entries)||selected>16)return false;s.selected=selected;
  bool found=false;for(DWORD n=0;n<count;++n){const auto at=entries+n*0x108;const auto parent=Word(reinterpret_cast<void*>(at),8),id=Word(reinterpret_cast<void*>(at),0x34),type=Word(reinterpret_cast<void*>(at),0x38);const auto inactive=*reinterpret_cast<BYTE*>(at+0x40);
   if(parent!=component||id>15||type>4||inactive>1)return false;if(type&&!inactive)++s.injuries;
   if(selected<16&&id==selected){if(found)return false;found=true;if(type&&!inactive){s.flags|=HealingUiSelected;s.severity=type;}}
  }
  // A valid selection is a bone ID, not an index into the configured wound list.
  // Native696920 searches entry+34 and may find no matching entry.
  const auto hands=Word(hero,0xf9c);if(!hands)return false;const auto held=Word(reinterpret_cast<void*>(hands),4);
  if(held){const auto metadata=Word(reinterpret_cast<void*>(held),0x28);if(!metadata)return false;if(Word(reinterpret_cast<void*>(metadata),8)&0x1000){const auto type=*reinterpret_cast<WORD*>(held+0x58c);if(type==14||type==15)s.medicine=type;}}
  const char* actions[]={"HEALING_SELECT_LEFT","HEALING_SELECT_RIGHT","HEALING_SELECT_UP","HEALING_SELECT_DOWN","HEALING_SELECT_OPPOSITE_LEFT","HEALING_SELECT_OPPOSITE_RIGHT","ACTION_LEAVE"};
  for(unsigned n=0;n<7;++n)if(!inventory_ui_context::Binding(actions[n],s.keys[n]))return false;
  // Native hands+4 is the left slot (the gun is+14). Show its actual
  // contextual binding, not a hard-coded mouse button or an inventory action.
  if(s.medicine&&!inventory_ui_context::Binding("LEFT_OBJECT_USE",s.keys[7]))return false;
  if(*reinterpret_cast<BYTE**>(0xfd61ec)!=hero||Word(hero,0xf94)!=component||Word(reinterpret_cast<void*>(component),0xc)!=count||Word(reinterpret_cast<void*>(component),0x10)!=entries||Word(reinterpret_cast<void*>(component),0x13c)!=selected||Word(hero,0xf9c)!=hands||Word(reinterpret_cast<void*>(hands),4)!=held)return false;
  s.flags|=HealingUiVisible;owner={reinterpret_cast<DWORD>(hero),component,entries,count};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};++generation;ReleaseSRWLockExclusive(&lock);}
static void Tick(){HealingUiStateV2 next{};Owner owner{};if(!Capture(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
// The miniature map shares the phone movie but is active even when the full
// phone is closed. Suppress its paint only during an authenticated, fresh
// healing state; no visibility flags, input, scripts or map state are changed.
static bool OwnsMinimapPaint(std::uintptr_t root)noexcept{
 AcquireSRWLockShared(&lock);const bool healing=enabled&&(state.flags&HealingUiVisible)&&GetTickCount()-state.tick<=250;ReleaseSRWLockShared(&lock);
 if(!healing||!root)return false;
 __try{
  if(*reinterpret_cast<BYTE*>(0xff4b7f)||Word(reinterpret_cast<void*>(0xff47a0))!=0xc79974)return false;
  auto* movie=*reinterpret_cast<BYTE**>(0xff4790);if(!movie||Word(movie,12)!=0xff47a0||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle||Word(handle)!=root)return false;
  DWORD minimap{};return phone_context::Resolve(handle,"/mini_map",585,minimap);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
