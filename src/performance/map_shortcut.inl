#include "map_shortcut_policy.h"
namespace map_shortcut {
static map_shortcut_policy::State gesture{};
// Action75 is CELLPHONE_TOGGLE in the authenticated r29 native table. The
// exact keyboard device and single non-inverted physical binding are required.
static int PhoneScan(){
 using modern_combat::Pointer;using modern_combat::Word;
 auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);auto* table=*reinterpret_cast<unsigned char**>(0x1213570);
 auto* controller=hero?Pointer(hero,0x95c):nullptr;if(!controller||!table)return -1;const DWORD base=Word(controller,8);if(base>4096)return -1;
 auto* action=table+(base+75)*60;if(Word(action,0x14)!=1)return -1;auto* binding=Pointer(action,0x18);auto* value=binding?Pointer(binding,0):nullptr;
 auto* device=value?Pointer(value,0x50):nullptr;const unsigned scan=value?Word(value,4):256;
 if(!value||Word(value,0)!=0||Word(binding,4)||!scan||scan>=256||!device||Word(device,0)!=0xcd3d44||value!=Pointer(device,0x44)+(scan-1)*84)return -1;
 return static_cast<int>(scan);
}
static bool Gameplay(){
 using modern_combat::Word;using modern_combat::Pointer;
 if(!phone_context::enabled||!phone_context::Foreground()||Word(reinterpret_cast<void*>(0xff4290),0)||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390),0))||(Word(reinterpret_cast<void*>(0xfda5c8),0)&0x38)!=8||(Word(reinterpret_cast<void*>(0xfd8bd4),0)&15))return false;
 auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);if(!hero||Word(hero,0)!=0xc834c0||hero[0xc19]||!Pointer(hero,0x95c))return false;
 const auto inventory=*reinterpret_cast<float*>(hero+0x1d88),heal=*reinterpret_cast<float*>(hero+0x1e00);auto* cinematic=Pointer(hero,0x280);
 return std::isfinite(inventory)&&inventory<=0&&std::isfinite(heal)&&heal<=0&&(!cinematic||!cinematic[0x204]);
}
static void Capture(void* data){
 if(!data)return;__try{
  auto* keys=static_cast<BYTE*>(data);const bool down=(keys[0x32]&0x80)!=0;
  const int scan=PhoneScan();const bool available=scan>0&&scan!=0x32&&!(keys[scan]&0x80);
  PhoneStateV1 phone{};const bool fresh=phone_context::GetState(&phone,sizeof phone)!=FALSE;
  const bool ready=fresh&&(phone.flags&PhoneHome)&&!(phone.flags&PhoneBusy);
  const bool gps=ready&&phone.count>=1&&phone.items[0].kind==static_cast<DWORD>(PhoneItemKind::Gps)&&phone.items[0].enabled;
  const bool other=fresh&&(phone.flags&PhoneNativeSubpage);
  const bool mapOpen=available&&fresh&&phone.nativePage==1&&(phone.flags&PhoneNativeSubpage);
  const auto action=gesture.Update(down,available&&Gameplay(),ready,gps,other,GetTickCount(),mapOpen);
  if(action==map_shortcut_policy::Result::OpenPhone){phone_context::RequestDirectMap();keys[scan]|=0x80;}
  else if(action==map_shortcut_policy::Result::CloseMap){phone_context::RequestClosePhone();keys[0x32]=0;}
 }__except(EXCEPTION_EXECUTE_HANDLER){gesture={};}
}
}
