#include "favorites_state.h"
#include "favorites_policy.h"
namespace favorites {
using modern_combat::Pointer;using modern_combat::Word;
struct Item {DWORD definition{},kind{},hand{},low{},high{},components{},icon{};};
struct Set {DWORD valid{};Item items[3]{};};
static bool enabled=false,opened=false,editor=false,lastClick=false,loaded=false;
static bool closingFence=false;static DWORD closedTick=0;
static unsigned hover=~0u,selected=0,drag=~0u,status=0;
static DWORD statusTick=0;
static void Notify(unsigned code){status=code;statusTick=GetTickCount();}
static Set saved[6]{},working[6]{},current{};
static favorites_policy::Gesture gesture{};
static HWND window=nullptr;static WNDPROC previousWindowProc=nullptr;
static wchar_t settingsPath[MAX_PATH]{};
static SRWLOCK snapshotLock=SRWLOCK_INIT;
static FavoritesStateV1 published{};
// A native action defaults to count=1: always provide its zero-valued binding.
static modern_combat::InputValue blockedValue{};
static modern_combat::InputBinding blockedBinding{&blockedValue,0,0};
static modern_combat::NativeAction blockedAction{0,2,0,0,0,1,&blockedBinding,{}};
struct EquipPlan {
 bool active{};modern_combat::ItemKey hero{},inventory{};Set target{};DWORD began{};
 // Native stock equip can materialize a new actor. Adopt it only as the
 // acknowledgement of our own accepted op1 after the native queue drains.
 unsigned pendingSlot=~0u;Item pendingSource{},materialized[3]{};
};
static EquipPlan plan{};
static bool Active(){return enabled&&opened;}
static bool ConsumesInput(){
 if(!enabled)return false;if(opened)return true;if(!closingFence)return false;
 if(GetTickCount()-closedTick<80||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))return true;
 closingFence=false;return false;
}
static bool Focused(){DWORD pid=0;const HWND w=GetForegroundWindow();GetWindowThreadProcessId(w,&pid);return w&&pid==GetCurrentProcessId()&&!IsIconic(w);}
static DWORD Components(unsigned char* actor){
 auto* manager=Pointer(actor,0x510);if(!manager)return 0;
 const unsigned count=Word(manager,0xc);auto* list=Pointer(manager,0x10);if(count>64||(count&&!list))return 0;
 DWORD hash=2166136261u;
 for(unsigned n=0;n<count;++n){auto* part=Pointer(list,n*12+4);if(!part)return 0;
  const DWORD fields[]={Word(list,n*12),Word(part,0),Word(part,0x34)};
  for(auto v:fields)hash=(hash^v)*16777619u;
 }return hash^count;
}
static Item Describe(unsigned char* actor){
 if(!actor||(Word(actor,0x504)&0x80u))return {};Item i{Word(actor,0x4f8),Word(actor,0x58c),Word(actor,0x4fc),Word(actor,0x10),Word(actor,0x14),Components(actor),6};
 if(i.hand>2||!i.definition)return {};
 if(modern_combat::IsPistol(actor))i.icon=1;else if(modern_combat::IsLamp(actor))i.icon=2;
 else if((i.kind&0xffff)==2||(i.kind&0xffff)==8)i.icon=3;else if(i.hand==2)i.icon=4;
 return i;
}
static bool Same(const Item& a,const Item& b,bool identity){
 return a.definition&&a.definition==b.definition&&a.kind==b.kind&&a.hand==b.hand&&
  (identity?(a.low==b.low&&a.high==b.high):(a.components&&a.components==b.components));
}
static bool Context(unsigned char** heroOut,unsigned char** inventoryOut,bool* inInventory){
 if(!enabled||!Focused()||(Word(reinterpret_cast<void*>(0xfd8bd4),0)&15)||
    (Word(reinterpret_cast<void*>(0xfda5c8),0)&0x38)!=8)return false;
 auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);if(!hero||Word(hero,0)!=0xc834c0||hero[0xc19])return false;
 auto* inventory=Pointer(hero,0x810);auto* hands=Pointer(hero,0xf9c);auto* movie=Pointer(hero,0x280);
 if(!inventory||Pointer(inventory,0x754)!=hero||!hands||Pointer(hands,0)!=hero||(movie&&movie[0x204]))return false;
 const float t=*reinterpret_cast<float*>(hero+0x1d88),healing=*reinterpret_cast<float*>(hero+0x1e00);
 if(!std::isfinite(t)||!std::isfinite(healing)||healing>0)return false;
 *heroOut=hero;*inventoryOut=inventory;*inInventory=t>0;return true;
}
static bool CaptureSet(unsigned char* hero,unsigned char* inventory,Set& result){
 if(Word(inventory,0x81c))return false;auto* hands=Pointer(hero,0xf9c);if(!hands||Pointer(hands,0)!=hero)return false;
 result={};result.valid=1;
 for(unsigned n=0;n<3;++n){auto* actor=Pointer(hands,4+n*16);result.items[n]=Describe(actor);if(actor&&!result.items[n].definition)return false;}
 return !(result.items[2].definition&&(result.items[0].definition||result.items[1].definition));
}
// Native inventory stages chosen equipment until the inventory closes. Read
// the committed UI slots, never its merely highlighted/hovered item.
static bool CaptureInventorySet(unsigned char* hero,unsigned char* inventory,unsigned char* ui,Set& result){
 if(!ui||Word(ui,0)!=0xcbcfd8||Pointer(ui,0x74)!=inventory||Pointer(inventory,0x754)!=hero||Word(inventory,0x81c))return false;
 auto* stock=Pointer(inventory,0x4f0);if(!stock||Pointer(ui,0x78)!=stock)return false;
 const unsigned combining=Word(ui,0xb0),left=Word(ui,0xb4),right=Word(ui,0xb8);if(combining!=12||left>12||right>12)return false;
 result={};result.valid=1;
 auto capture=[&](unsigned slot,unsigned hand){if(slot==12)return true;auto* actor=Pointer(stock,4+slot*20);auto item=Describe(actor);if(!item.definition||item.hand!=hand)return false;result.items[hand]=item;return true;};
 return capture(left,0)&&capture(right,1);
}
static bool ValidSet(const Set& s){
 if(s.valid>1)return false;for(unsigned n=0;n<3;++n){const auto& i=s.items[n];if(i.definition&&(i.hand!=n||i.icon<1||i.icon>6))return false;}
 return !(s.items[2].definition&&(s.items[0].definition||s.items[1].definition));
}
static void Load(){
 if(loaded)return;loaded=true;
 if(GetPrivateProfileIntW(L"Favorites",L"Version",0,settingsPath)!=1)return;
 for(unsigned n=0;n<6;++n){wchar_t section[24];swprintf_s(section,L"Set%u",n+1);Set s{};s.valid=GetPrivateProfileIntW(section,L"Valid",0,settingsPath);
  for(unsigned h=0;h<3;++h){wchar_t key[16],line[180]{};swprintf_s(key,L"Hand%u",h);GetPrivateProfileStringW(section,key,L"",line,180,settingsPath);
   auto& i=s.items[h];if(line[0]&&swscanf_s(line,L"%lx,%lx,%lx,%lx,%lx,%lx,%lx",&i.definition,&i.kind,&i.hand,&i.low,&i.high,&i.components,&i.icon)!=7){s={};break;}}
  if(ValidSet(s))saved[n]=s;
 }
}
static bool Save(){
 wchar_t temp[MAX_PATH];if(swprintf_s(temp,L"%s.tmp",settingsPath)<0)return false;
 HANDLE file=CreateFileW(temp,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 char data[4096]{};int used=sprintf_s(data,"[Favorites]\r\nVersion=1\r\n");
 for(unsigned n=0;n<6;++n){used+=sprintf_s(data+used,sizeof(data)-used,"[Set%u]\r\nValid=%lu\r\n",n+1,working[n].valid);
  for(unsigned h=0;h<3;++h){const auto& i=working[n].items[h];used+=sprintf_s(data+used,sizeof(data)-used,"Hand%u=%lx,%lx,%lx,%lx,%lx,%lx,%lx\r\n",h,i.definition,i.kind,i.hand,i.low,i.high,i.components,i.icon);}}
 DWORD wrote=0;const bool ok=WriteFile(file,data,static_cast<DWORD>(used),&wrote,nullptr)&&wrote==static_cast<DWORD>(used)&&FlushFileBuffers(file);CloseHandle(file);
 if(!ok){DeleteFileW(temp);return false;}
 if(!MoveFileExW(temp,settingsPath,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp);return false;}memcpy(saved,working,sizeof(saved));return true;
}
static void Close(){if(opened){closingFence=true;closedTick=GetTickCount();}opened=false;editor=false;drag=~0u;favorites_native::EndSlow();}
static LRESULT CALLBACK WindowProc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_KILLFOCUS||(m==WM_ACTIVATEAPP&&!a)||m==WM_DESTROY){Close();plan={};gesture.blocked=true;}
 return CallWindowProcW(previousWindowProc,w,m,a,b);
}
static void RegisterWindow(HWND w){
 if(!enabled||!w||window)return;DWORD pid=0;GetWindowThreadProcessId(w,&pid);if(pid!=GetCurrentProcessId())return;
 SetLastError(0);const auto before=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(w,GWLP_WNDPROC));
 if(before&&SetWindowLongPtrW(w,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&WindowProc))){previousWindowProc=before;window=w;}
}
static void Configure(const wchar_t* config,bool supported){
 enabled=supported&&ConfigFlag(L"Controls",L"Favorites",true,config);if(!enabled)return;
 wcscpy_s(settingsPath,config);auto* slash=wcsrchr(settingsPath,L'\\');if(!slash){enabled=false;return;}wcscpy_s(slash,MAX_PATH-static_cast<size_t>(slash-settingsPath),L"\\enhanced-favorites.ini");
}
static unsigned char* Resolve(unsigned char* hero,unsigned char* inventory,const Item& desired){
 if(!desired.definition)return nullptr;unsigned char* exact=nullptr;unsigned char* fallback=nullptr;unsigned matches=0;
 auto visit=[&](unsigned char* actor){if(!actor)return;const auto value=Describe(actor);if(Same(desired,value,true))exact=actor;if(Same(desired,value,false)){fallback=actor;++matches;}};
 auto* hands=Pointer(hero,0xf9c);for(unsigned n=0;n<3;++n)visit(Pointer(hands,4+n*16));
 if(auto* stock=Pointer(inventory,0x4f0))for(unsigned n=0;n<12;++n){auto* p=Pointer(stock,4+n*20);if(p!=Pointer(hands,4)&&p!=Pointer(hands,0x14)&&p!=Pointer(hands,0x24))visit(p);}
 return exact?exact:(matches==1?fallback:nullptr);
}
static void EquipTick(unsigned char* hero,unsigned char* inventory,bool inInventory){
 if(!plan.active)return;
 if(inInventory||modern_combat::OwnedItemKey(hero)!=plan.hero||modern_combat::OwnedItemKey(inventory)!=plan.inventory||GetTickCount()-plan.began>5000){plan={};Notify(3);return;}
 if(Word(inventory,0x81c))return;
 auto* hands=Pointer(hero,0xf9c);unsigned char* targets[3]{};
 if(plan.pendingSlot<3){
  auto* held=Pointer(hands,4+plan.pendingSlot*16);const auto value=Describe(held);
  if(!held)return; // Native handler transition is still pending; timeout bounds it.
  if(!(Same(plan.pendingSource,value,true)||Same(plan.pendingSource,value,false))){plan={};Notify(3);return;}
  plan.materialized[plan.pendingSlot]=value;plan.pendingSlot=~0u;plan.pendingSource={};
 }
 for(unsigned n=0;n<3;++n)if(plan.target.items[n].definition){
  if(plan.materialized[n].definition){
   auto* held=Pointer(hands,4+n*16);
   if(!Same(plan.materialized[n],Describe(held),true)){plan={};Notify(3);return;}
   targets[n]=held;
  }else if(!(targets[n]=Resolve(hero,inventory,plan.target.items[n]))){plan={};Notify(3);return;}
 }
 for(unsigned n=0;n<3;++n){auto* held=Pointer(hands,4+n*16);if(held&&held!=targets[n]){
   if(!modern_combat::CallInventoryQueue(inventory,0,held,n,0)){plan={};Notify(3);}return;}}
 for(unsigned n=0;n<3;++n)if(targets[n]&&!Pointer(hands,4+n*16)){
  const auto source=Describe(targets[n]);
  if(!modern_combat::CallInventoryQueue(inventory,1,targets[n],n,8)){plan={};Notify(3);}
  else{plan.pendingSlot=n;plan.pendingSource=source;}return;}
 plan={};Notify(6);
}
static void Choose(unsigned n,unsigned char* hero,unsigned char* inventory){
 if(n>=6||!saved[n].valid)return;
 for(const auto& i:saved[n].items)if(i.definition&&!Resolve(hero,inventory,i)){Notify(3);return;}
 plan={true,modern_combat::OwnedItemKey(hero),modern_combat::OwnedItemKey(inventory),saved[n],GetTickCount()};Notify(5);
}
static void* ActionRoute(void* original,unsigned index){
 if(enabled&&((index>=39&&index<=44)||ConsumesInput()))return &blockedAction;return original;
}
static int HotkeyScan(){
#ifdef AITD_FAVORITES_BINDINGS_TEST
 auto* hero=testFavoritesHero;auto* table=testFavoritesTable;
#else
 auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);auto* table=*reinterpret_cast<unsigned char**>(0x1213570);
#endif
 auto* controller=hero?Pointer(hero,0x95c):nullptr;if(!controller)return -1;
 const DWORD base=Word(controller,8);if(!table||base>4096)return -1;
 auto* action=table+(base+39)*60;if(Word(action,0x14)!=1)return -1;auto* binding=Pointer(action,0x18);auto* value=binding?Pointer(binding,0):nullptr;
 auto* device=value?Pointer(value,0x50):nullptr;const unsigned scan=value?Word(value,4):256;
 if(!value||Word(value,0)!=0||Word(binding,4)||!scan||scan>=256||!device||Word(device,0)!=0xcd3d44||value!=Pointer(device,0x44)+(scan-1)*84)return -1;return static_cast<int>(scan);
}
static void Publish(float nx,float ny){
 FavoritesStateV1 s{};s.size=sizeof(s);s.version=1;s.tick=GetTickCount();s.flags=Active()?1u:((status&&s.tick-statusTick<2200)?2u:0u);s.mode=editor?1u:0u;s.hover=hover;s.selected=selected;s.status=status;s.cursorX=nx;s.cursorY=ny;
 for(unsigned h=0;h<3;++h)s.current[h]=current.items[h].icon;
 const auto* slots=editor?working:saved;for(unsigned n=0;n<6;++n){s.slots[n].valid=slots[n].valid;for(unsigned h=0;h<3;++h)s.slots[n].icons[h]=slots[n].items[h].icon;}
 AcquireSRWLockExclusive(&snapshotLock);published=s;ReleaseSRWLockExclusive(&snapshotLock);
}
static void Pulse(){if(opened&&!Focused())Close();favorites_native::Tick();}
static void Capture(void* data,DWORD bytes,bool keyboard){
 if(!enabled||!data)return;
 __try{
  Pulse();const bool wasOpen=opened;unsigned char *hero=nullptr,*inventory=nullptr;bool inInventory=false;const bool valid=Context(&hero,&inventory,&inInventory);
  if(!valid){Close();plan={};bool heldInvalid=gesture.previous;if(keyboard&&bytes==256){const int scan=HotkeyScan();heldInvalid=scan>=0&&(static_cast<unsigned char*>(data)[scan]&128)!=0;}gesture.Update(heldInvalid,GetTickCount(),false);Publish(.5f,.46f);return;}
  if(!opened)EquipTick(hero,inventory,inInventory);
  bool held=gesture.previous;
  if(keyboard&&bytes==256){auto* keys=static_cast<unsigned char*>(data);const int scan=HotkeyScan();held=scan>=0&&(keys[scan]&128)!=0;
   if(gesture.Update(held,GetTickCount(),!plan.active,inInventory)&&!opened){
    Load();Set set{};if(inInventory?CaptureInventorySet(hero,inventory,*reinterpret_cast<unsigned char**>(0x1081498),set):CaptureSet(hero,inventory,set)){current=set;opened=true;editor=inInventory;status=0;selected=0;hover=~0u;drag=~0u;lastClick=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;memcpy(working,saved,sizeof(saved));
     for(unsigned n=0;n<6;++n)if(!working[n].valid){selected=n;break;}
     if(window){RECT r{};GetClientRect(window,&r);POINT p{r.right/2,static_cast<LONG>(r.bottom*.46f)};ClientToScreen(window,&p);SetCursorPos(p.x,p.y);}if(!editor)favorites_native::BeginSlow();
    }
   }
  }
  POINT p{};RECT r{};float nx=.5f,ny=.46f,aspect=16.f/9.f;
  if(window&&GetClientRect(window,&r)&&r.right>0&&r.bottom>0&&GetCursorPos(&p)&&ScreenToClient(window,&p)){nx=static_cast<float>(p.x)/r.right;ny=static_cast<float>(p.y)/r.bottom;aspect=static_cast<float>(r.right)/r.bottom;}
  if(opened){
   if(editor!=inInventory){Close();Publish(nx,ny);if((keyboard&&bytes==256)||(!keyboard&&(bytes==sizeof(DIMOUSESTATE)||bytes==sizeof(DIMOUSESTATE2))))memset(data,0,bytes);return;}
   hover=favorites_policy::Sector(nx,ny,aspect);const bool click=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
   if(editor){
    if(click&&!lastClick){const auto button=favorites_policy::Button(nx,ny,aspect);
     if(button==0)Close();else if(button==1){working[selected]=current;Notify(1);}
     else if(button==2){if(Save()){Notify(2);Close();}else Notify(4);}
     else if(hover<6){selected=hover;drag=hover;}}
    if(!click&&lastClick&&drag<6){if(hover<6&&hover!=drag){const auto swap=working[hover];working[hover]=working[drag];working[drag]=swap;selected=hover;}drag=~0u;}
    if(GetAsyncKeyState(VK_ESCAPE)&0x8000)Close();
   }else if(keyboard&&!held){const auto choose=hover;Close();if(choose<6)Choose(choose,hero,inventory);}
   lastClick=click;
  }
  Publish(nx,ny);
  if(ConsumesInput()||wasOpen){if(keyboard&&bytes==256)memset(data,0,bytes);else if(!keyboard&&(bytes==sizeof(DIMOUSESTATE)||bytes==sizeof(DIMOUSESTATE2)))memset(data,0,bytes);}
 }__except(EXCEPTION_EXECUTE_HANDLER){Close();plan={};Publish(.5f,.46f);}
}
} // namespace favorites
extern "C" BOOL WINAPI AITDGetFavoritesState(FavoritesStateV1* output,DWORD bytes){
 if(!output||bytes<sizeof(FavoritesStateV1)||!favorites::enabled)return FALSE;
 if(!TryAcquireSRWLockShared(&favorites::snapshotLock))return FALSE;*output=favorites::published;ReleaseSRWLockShared(&favorites::snapshotLock);return TRUE;
}
