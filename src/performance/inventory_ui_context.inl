// g_InventoryFlashManager1081480, constructor8394C0. This presenter does not
// replace the3D jacket, item selection, inventory actions, clock or input.
#include "inventory_ui_state.h"
#include "inventory_native_icons.h"
#include "inventory_native_paint.h"
#include "bleeding_timer.inl"
#include <cmath>
#include "inventory_icon_layout.h"
namespace inventory_ui_context {
static bool enabled=false,ready=false;static SRWLOCK lock=SRWLOCK_INIT;
static DWORD generation{},readyGeneration{},readyTick{};static InventoryUiStateV1 state{};
static DWORD captureStage{}; // Bounded diagnostic only; no persistent user data.
struct Owner{DWORD hero{},inventory{},movie{},handle{},root{},panel{},combination{};inventory_native_paint::Shapes equipment{};DWORD equipmentNodes[2]{},width{},height{};};static Owner previous{};
static inventory_native_paint::Cache equipmentCache{};
static DWORD Word(const void* p,unsigned at=0){return phone_context::Word(p,at);}
static bool Dormant(unsigned char* h,const char* path,DWORD resource){DWORD n{};return phone_context::Resolve(h,path,resource,n)&&Word(reinterpret_cast<void*>(n),0x60)==1;}
static bool Member(unsigned char* h,const char* path,DWORD resource,DWORD& n){DWORD binding[2]{};if(!menu_context::Resolve(h,path,binding)||!binding[0]||binding[1]!=Word(h))return false;n=binding[0];const auto definition=Word(reinterpret_cast<void*>(n),0x18);return definition&&Word(reinterpret_cast<void*>(definition),8)==resource;}
static bool HandsOnly(unsigned char* h){DWORD n{};if(!phone_context::Resolve(h,"/shortcuts",530,n))return false;const auto frame=Word(reinterpret_cast<void*>(n),0x60);return (frame==1||frame==16)&&Word(reinterpret_cast<void*>(n),0x64)==2&&!*reinterpret_cast<BYTE*>(n+0x68);}
static bool Binding(const char* command,wchar_t(&out)[256]){
#ifdef AITD_INVENTORY_UI_TEST
 return TestBinding(command,out);
#else
 if(!pc_prompts::enabled||!pc_prompts::nativeFormatter)return false;char text[512]{};const auto end=pc_prompts::Format(command,text,sizeof text);if(!end||end<=text||end>=text+sizeof text||*end||strnlen_s(text,sizeof text)!=static_cast<size_t>(end-text))return false;
 return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,out,256)!=0;
#endif
}
static bool Text(DWORD root,const char* key,wchar_t* out,int cap,bool optional=false){return controls_context::Text(root,root+200,key,out,cap,optional);}
// Native841D10 phase4 places an item: INV_SELECT confirms832720, INV_CANCEL
// returns to browsing. INV_EXIT deliberately does not close this native phase.
static unsigned DropPlacement(const BYTE* hero,const BYTE* inventory){
 __try{
  const auto* manager=*reinterpret_cast<const BYTE* const*>(0x1081498);
  if(!manager||Word(manager)!=0xcbcfd8||Word(manager,0x74)!=reinterpret_cast<DWORD>(inventory)||
   Word(inventory,0x754)!=reinterpret_cast<DWORD>(hero)||Word(manager,0xc0)!=4)return 0;
  return !manager[0xcb]&&manager[0xca]&&!manager[0xa5]?2u:1u;
 }__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
static bool FavoritesHint(){
#ifdef AITD_INVENTORY_UI_TEST
 return TestFavoritesHint();
#else
 // Only label the key we can identify; a changed binding is never mislabelled.
 return favorites::enabled&&favorites::HotkeyScan()==0x29;
#endif
}
static bool Hands(unsigned char* hero,unsigned char* inventory,DWORD& left,DWORD& right){
#ifdef AITD_INVENTORY_UI_TEST
 (void)hero;(void)inventory;return TestHands(left,right);
#else
 favorites::Set set{};auto* ui=*reinterpret_cast<unsigned char**>(0x1081498);
 if(!favorites::CaptureInventorySet(hero,inventory,ui,set))return false;
 left=set.items[0].icon;right=set.items[1].icon;
 return left<=6&&right<=6;
#endif
}
static bool AddPrompt(InventoryUiStateV1& s,DWORD root,const char* command,const char* caption){if(s.promptCount==4)return false;auto& p=s.prompts[s.promptCount];if(!Binding(command,p.key)||!Text(root,caption,p.label,128))return false;++s.promptCount;return true;}
#ifndef AITD_INVENTORY_UI_TEST
static void CaptureAmmo(BYTE* hero,InventoryUiStateV1& s){
 __try{
  auto* hands=modern_combat::Pointer(hero,0xf9c);auto* pistol=hands?modern_combat::Pointer(hands,0x14):nullptr;
  if(!pistol||modern_combat::Pointer(hands,0)!=hero||!modern_combat::NativePistolBehavior(pistol))return;
  auto* manager=modern_combat::Pointer(pistol,0x510);const DWORD count=manager?Word(manager,0xc):0;auto* entries=manager?modern_combat::Pointer(manager,0x10):nullptr;
  if(entries&&count<=256)for(DWORD n=0;n<count;++n){auto* component=modern_combat::Pointer(entries,n*12+4);if(component&&*reinterpret_cast<const short*>(component+0x34)==1){const DWORD rounds=Word(component,0x38);if(rounds<=9999){s.ammo=rounds;s.flags|=InventoryUiAmmoKnown;}break;}}
 }__except(EXCEPTION_EXECUTE_HANDLER){s.ammo=0;s.flags&=~InventoryUiAmmoKnown;}
}
static bool MouseLeftSelect(BYTE* hero){
 __try{auto* controller=modern_combat::Pointer(hero,0x95c);auto* table=*reinterpret_cast<BYTE**>(0x1213570);if(!controller||!table)return false;
  const DWORD base=Word(controller,8);if(base>4096)return false;auto* action=table+(base+31)*60;if(Word(action,0x14)!=1)return false;
  auto* binding=modern_combat::Pointer(action,0x18);auto* value=binding?modern_combat::Pointer(binding,0):nullptr;auto* device=value?modern_combat::Pointer(value,0x50):nullptr;
  return value&&Word(binding,4)==0&&Word(value,0)==0&&Word(value,4)==12&&device&&Word(device)==0xcd3d68;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
#endif
static bool Capture(InventoryUiStateV1& s,Owner& owner){s={};owner={};captureStage=1;if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0x1081480))!=0xcbe850||Word(reinterpret_cast<void*>(0x1081494))!=0xcbe81c||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  captureStage=2;auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0)return false;const float inventoryTime=*reinterpret_cast<float*>(hero+0x1d88),healing=*reinterpret_cast<float*>(hero+0x1e00);if(!std::isfinite(inventoryTime)||inventoryTime<=0||!std::isfinite(healing)||healing>0)return false;
  auto* inventory=reinterpret_cast<BYTE*>(Word(hero,0x810));if(!inventory||Word(inventory,0x754)!=reinterpret_cast<DWORD>(hero))return false;
  captureStage=3;auto* movie=*reinterpret_cast<BYTE**>(0x1081490);if(!movie||Word(movie,12)!=0x1081494||!movie[0x65])return false;auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const auto root=Word(handle),scope=root+200;if(!root)return false;
  const auto frame=Word(reinterpret_cast<void*>(scope),0x60);
  DWORD platform{},panel{},combine{},whatcase{},droppable{};
  captureStage=4;if(!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||(frame<23||frame>34)||
     !phone_context::Resolve(handle,"/clip_general",435,panel)||!phone_context::Resolve(handle,"/clip_general/ecran_combinaison",434,combine)||Word(reinterpret_cast<void*>(combine),0x60)!=1||
     !Dormant(handle,"/tips_inventory",47)||!HandsOnly(handle)||!bleeding_timer::Read(handle,root,s.bleeding)||
     !phone_context::Number(root,scope,"whatcase",whatcase,99)||(whatcase>10&&whatcase!=99)||
     !phone_context::Number(root,scope,"droppable",droppable,1)||!Binding("INV_EXIT",s.closeKey)||!Text(root,"m_exit_maj",s.closeLabel,128))return false;
  // The original311/310 movie publishes99 (ask) after choosing the first
  // combination item, before a valid second candidate produces9/10. Require
  // the native selection marker observed in this exact phase, not arbitrary
  // values between the ordinary cases and the authored sentinel.
  if(whatcase==99){DWORD selected{};if(!phone_context::Number(root,scope,"selected_item",selected,1)||selected!=1)return false;}
  // In the asking gap the last hovered item's detail fields can be stale or
  // empty. Do not present them as a new candidate, or let them erase Cancel.
  captureStage=5;if(whatcase!=99&&Word(reinterpret_cast<void*>(0x108149c))){
   if(!Text(root,"item_name",s.item,128)||!s.item[0]||!Text(root,"nb_item",s.quantity,64,true))return false;s.flags|=InventoryUiItem;
   char details[64]{};if(!phone_parameters_context::Raw(root,scope,"item_details",details,64))return false;
   const char* known[]={"default","bottle_liq_bul","bottle_liq","bottle_bul","bandage","blood_pack","tissue","red_stick","green_stick","bullets","gun","batteries","spray_medical","spray_rust","spray_mosquito","scotch","medical_alcohol","maglight"};bool valid=false;for(const auto* name:known)valid|=!strcmp(details,name);if(!valid)return false;
   if(!strcmp(details,"gun"))s.flags|=InventoryUiGunItem;
   // The native default detail layout (lighter and other unique items) has no
   // quantity field. nb_item can retain a stale "x0" from the prior selection.
   if(!strcmp(details,"default"))s.quantity[0]=0;
   if(!strcmp(details,"gun")||!strcmp(details,"bullets"))s.images[0]={73,128,32,0,0,128,32};
   else if(!strcmp(details,"bandage"))s.images[0]=inventory_native_icons::Find("bandages");
   else if(!strcmp(details,"maglight")||!strcmp(details,"batteries"))s.images[0]=inventory_native_icons::Find("batteries");
   else if(strcmp(details,"default"))s.images[0]=inventory_native_icons::Find(details);
   if(!strcmp(details,"maglight")||!strcmp(details,"batteries"))s.flags|=InventoryUiBatteryItem;
   if(!strcmp(details,"bullets"))s.flags|=InventoryUiBulletItem;
   if(!strcmp(details,"gun")||!strcmp(details,"bullets")){DWORD flame{};if(!phone_context::Number(root,scope,"bullet_flame",flame,1))return false;if(flame)s.flags|=InventoryUiFireAmmo;if(!strcmp(details,"bullets")&&!Text(root,"nb_boxes",s.boxes,64))return false;}
   if(!strcmp(details,"bottle_liq_bul")||!strcmp(details,"bottle_bul"))if(!Text(root,"nb_bul",s.bullets,64))return false;
   if(!strcmp(details,"bottle_liq_bul")||!strcmp(details,"bottle_liq")){char liquid[32]{};if(!Text(root,"nb_liq",s.liquid,64)||!phone_parameters_context::Raw(root,scope,"liquid_type",liquid,32))return false;if(!strcmp(liquid,"water"))s.liquidKind=1;else if(!strcmp(liquid,"inflammable"))s.liquidKind=2;else if(!strcmp(liquid,"blood"))s.liquidKind=3;else if(liquid[0])return false;}
   if(!strncmp(details,"bottle_",7)||!strncmp(details,"spray_",6)){DWORD power{};if(phone_context::Number(root,scope,"power_explosion",power,16384)){s.flags|=InventoryUiPowerKnown;if(power)s.flags|=InventoryUiPowerFull;}}
  }
  captureStage=6;const unsigned placement=DropPlacement(hero,inventory);
  if(placement){
   if(whatcase>=9||!Binding("INV_CANCEL",s.closeKey)||!Text(root,"m_cancel",s.closeLabel,128))return false;
   if(placement==2&&!AddPrompt(s,root,"INV_SELECT","m_drop"))return false;
  }
  else if(whatcase==9||whatcase==10||whatcase==99){
   // Native83B7F0 publishes9/10 only for a valid second combine/separate
   // candidate and binds action32 as combine_key. The localized instruction
   // is loaded separately from the movie's default fields. Prefer it when
   // present; retain the actual operation caption as a safe missing-text fallback.
   const char* operation=whatcase==99?"m_combine_uncombine":whatcase==9?"m_combine":"m_uncombine";
   s.flags|=InventoryUiCombining;
   if((!Text(root,"m_combine_with",s.instruction,512)||!s.instruction[0])&&
      !Text(root,operation,s.instruction,512))return false;
   //99 is waiting for a second item; native combine confirmation is meaningful
   // only after the engine reports a valid9/10 candidate.
   if(whatcase!=99&&!AddPrompt(s,root,"INV_COMBI",operation))return false;
   if(!AddPrompt(s,root,"INV_CANCEL","m_cancel"))return false;
  }
  else{
   if(whatcase>=1&&whatcase<=4)if(!AddPrompt(s,root,"INV_SELECT","m_handle"))return false;
   if(whatcase==2||whatcase==4||whatcase==5||whatcase==7)if(!AddPrompt(s,root,"INV_COMBI",whatcase==4||whatcase==7?"m_combine_uncombine":"m_combine"))return false;
   if(whatcase==3||whatcase==6)if(!AddPrompt(s,root,"INV_COMBI","m_uncombine"))return false;
   if(droppable||whatcase==8)if(!AddPrompt(s,root,"INV_DROP","m_drop"))return false;
  }
  if(*reinterpret_cast<BYTE**>(0xfd61ec)!=hero||Word(hero,0x810)!=reinterpret_cast<DWORD>(inventory)||*reinterpret_cast<BYTE**>(0x1081490)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(scope),0x60)!=frame)return false;
  s.flags|=InventoryUiVisible;if(FavoritesHint())s.flags|=InventoryUiFavorites;DWORD left{},right{};if(Hands(hero,inventory,left,right))s.flags|=InventoryUiHandsKnown|(left<<InventoryUiLeftShift)|(right<<InventoryUiRightShift);
  inventory_native_paint::Shapes nativeEquipment{};DWORD equipmentNodes[2]{},width{},height{};
#ifndef AITD_INVENTORY_UI_TEST
  RECT client{};DWORD pid{};const HWND window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);
  if(pid==GetCurrentProcessId()&&GetClientRect(window,&client)&&client.right>=320&&client.bottom>=200){width=client.right;height=client.bottom;}
  if(width&&height&&Member(handle,"/shortcuts/items_center/left",486,equipmentNodes[0])&&Member(handle,"/shortcuts/items_center/right",499,equipmentNodes[1])&&inventory_native_paint::Capture(root,inventory_native_content::equipment,nativeEquipment,equipmentCache))s.flags|=InventoryUiNativeEquipment;
#endif
  
  for(unsigned hand=0;hand<2;++hand){char name[96]{};if(phone_parameters_context::Raw(root,scope,hand?"right":"left",name,sizeof name)){
   const auto image=inventory_native_icons::Find(name);
   // Composite items keep their exact native movie art. Empty hands alone
   // retain the requested upright presentation from the same original bitmap.
   if(!(s.flags&InventoryUiNativeEquipment)||image.bitmap==484)s.images[hand+1]=image;
  }}
#ifndef AITD_INVENTORY_UI_TEST
  // Use the same authenticated pistol component as the ordinary ammunition HUD.
  // No value is shown if the equipped object or its magazine cannot be proven.
  CaptureAmmo(hero,s);
  if((placement||(whatcase>=1&&whatcase<=4))&&s.promptCount&&MouseLeftSelect(hero))s.flags|=InventoryUiSelectMouseLeft;
#endif
  s.nativeCase=whatcase;owner={reinterpret_cast<DWORD>(hero),reinterpret_cast<DWORD>(inventory),reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,combine,nativeEquipment,{equipmentNodes[0],equipmentNodes[1]},width,height};captureStage=100;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
// Inventory's separate root tips87..102 is not the Tips tutorial movie.
// Its localized tips_txt is the actual combining instruction; replacing only
// the ordinary inventory view leaves this old banner visible in the world.
static bool CaptureTip(InventoryUiStateV1& s,Owner& owner){s={};owner={};if(!enabled)return false;
 __try{
  if(Word(reinterpret_cast<void*>(0x1081480))!=0xcbe850||Word(reinterpret_cast<void*>(0x1081494))!=0xcbe81c||Word(reinterpret_cast<void*>(0xff4290))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<BYTE*>(0xff4b7f)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
  auto* hero=*reinterpret_cast<BYTE**>(0xfd61ec);if(!hero||Word(hero)!=0xc834c0)return false;
  const auto heal=*reinterpret_cast<float*>(hero+0x1e00),inv=*reinterpret_cast<float*>(hero+0x1d88);if(!std::isfinite(heal)||heal>0||!std::isfinite(inv))return false;
  auto* inventory=reinterpret_cast<BYTE*>(Word(hero,0x810));if(!inventory||Word(inventory,0x754)!=reinterpret_cast<DWORD>(hero))return false;
  auto* movie=*reinterpret_cast<BYTE**>(0x1081490);if(!movie||Word(movie,12)!=0x1081494||!movie[0x65])return false;
  auto* handle=reinterpret_cast<BYTE*>(Word(movie,0x24));if(!handle)return false;const auto root=Word(handle),scope=root+200;if(!root)return false;
  const auto frame=Word(reinterpret_cast<void*>(scope),0x60);DWORD pc{},tips{};
  if(frame<87||frame>102||!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",pc,1)||pc!=1||
     !phone_context::Resolve(handle,"/tips",588,tips)||!(Word(reinterpret_cast<void*>(tips),0x6c)&1)||!Word(reinterpret_cast<void*>(tips),0x60)||Word(reinterpret_cast<void*>(tips),0x60)>148||
     !Dormant(handle,"/tips_inventory",47)||!Dormant(handle,"/clip_general/ecran_combinaison",434)||!HandsOnly(handle)||!bleeding_timer::Read(handle,root,s.bleeding)||
     !Text(root,"tips_txt",s.instruction,512)||!s.instruction[0])return false;
  // The3D world/body are outside this Flash root. Other native helper/item
  // presentations must not be erased by a banner captured in an unknown overlap.
  for(const auto* path:{"/item","/helper"}){DWORD n{};const auto id=!strcmp(path,"/item")?568u:569u;if(!Member(handle,path,id,n)||((Word(reinterpret_cast<void*>(n),0x6c)&1)&&Word(reinterpret_cast<void*>(n),0x60)!=1))return false;}
  if(*reinterpret_cast<BYTE**>(0xfd61ec)!=hero||Word(hero,0x810)!=reinterpret_cast<DWORD>(inventory)||*reinterpret_cast<BYTE**>(0x1081490)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root||Word(reinterpret_cast<void*>(scope),0x60)!=frame)return false;
  s.flags=InventoryUiVisible|InventoryUiTip;owner={reinterpret_cast<DWORD>(hero),reinterpret_cast<DWORD>(inventory),reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,tips,0};return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){s={};owner={};return false;}
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;state={};previous={};ready=false;++generation;ReleaseSRWLockExclusive(&lock);}
static void Tick(){InventoryUiStateV1 next{};Owner owner{};if(!Capture(next,owner)&&!CaptureTip(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&previous,&owner,sizeof owner)){previous=owner;++generation;ready=false;}next.tick=GetTickCount();next.generation=generation;state=next;ReleaseSRWLockExclusive(&lock);}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;AcquireSRWLockShared(&lock);const auto copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&InventoryUiVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&InventoryUiVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool OwnsPointer(){AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Fresh(previous.root,previous.root,enabled,(state.flags&InventoryUiVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool OwnsNativeEquipment(std::uintptr_t root,std::uintptr_t shape,std::uintptr_t instance)noexcept{
 bool yes=false;AcquireSRWLockShared(&lock);
 __try{if((state.flags&InventoryUiNativeEquipment)&&ui_paint_owner::Fresh(root,previous.root,enabled,(state.flags&InventoryUiVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount())&&inventory_native_paint::Contains(previous.equipment,root,shape)){
  for(unsigned n=0;n<2&&!yes;++n)if(inventory_native_paint::Descendant(instance,previous.equipmentNodes[n])){float identity[6]{1,1,0,0,0,0};const auto& box=inventory_icon_layout::Equipment[n];yes=inventory_native_paint::Fit(previous.equipment,root,previous.equipmentNodes[n],identity,previous.width,previous.height,box.x,box.y,box.w,box.h);}
 }}
 __except(EXCEPTION_EXECUTE_HANDLER){yes=false;}
 ReleaseSRWLockShared(&lock);return yes;
}
static bool TransformNativeEquipment(std::uintptr_t root,std::uintptr_t shape,std::uintptr_t instance,float* matrix)noexcept{
 bool yes=false;AcquireSRWLockShared(&lock);
 __try{if((state.flags&InventoryUiNativeEquipment)&&ui_paint_owner::Fresh(root,previous.root,enabled,(state.flags&InventoryUiVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount())&&inventory_native_paint::Contains(previous.equipment,root,shape)){
  for(unsigned n=0;n<2&&!yes;++n)if(inventory_native_paint::Descendant(instance,previous.equipmentNodes[n])){const auto& box=inventory_icon_layout::Equipment[n];yes=inventory_native_paint::Fit(previous.equipment,root,previous.equipmentNodes[n],matrix,previous.width,previous.height,box.x,box.y,box.w,box.h);}
 }}__except(EXCEPTION_EXECUTE_HANDLER){yes=false;}
 ReleaseSRWLockShared(&lock);return yes;
}
}
