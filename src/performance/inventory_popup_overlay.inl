#include "inventory_popup_art.h"
namespace inventory_popup_overlay {
static bool drawing=false,visible=false,mouseSeen=false;static InventoryPopupGetter getter=nullptr;static InventoryPopupSender sender=nullptr;static InventoryPopupReady setter=nullptr;
static DWORD lookup{},readyGeneration{},lastStatus=~0u,generation{};static IDirect3DDevice9* owner=nullptr;static ui_art::Scene art;static ui_art::Assets assets;static InventoryPopupStateV1 cached{};static HRESULT buildResult=S_OK;
static menu_presentation_policy::KeyEdges keys;static POINT lastMouse{};static inventory_popup_art::Hit pressed{};
static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT};unsigned held=0;for(unsigned n=0;n<7;++n)if(GetAsyncKeyState(codes[n])&0x8000)held|=1u<<n;return held;}
static void Hide(){visible=mouseSeen=false;pressed={};keys.Reset(Keys());art={};owner=nullptr;}
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;Hide();assets.Reset();if(setter)setter(FALSE,readyGeneration);}
static bool Present(IDirect3DDevice9* d,const InventoryPopupStateV1& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
 const auto held=Keys();if(!visible||owner!=d||generation!=s.generation){Hide();owner=d;generation=s.generation;visible=true;keys.Reset(held);}
 auto key=s;key.nativeFrame=0;key.bleeding.nativeFrame=key.bleeding.nativeFrame?2:0;
 if(art.batches.empty()||memcmp(reinterpret_cast<const BYTE*>(&cached)+12,reinterpret_cast<const BYTE*>(&key)+12,sizeof key-12)){ui_art::Scene next;buildResult=inventory_popup_art::Build(s,next);if(FAILED(buildResult))return false;art=std::move(next);cached=key;}
 const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT point{};const bool mouse=ui_pointer::ReadClient(window,client,point);float x{},y{};if(mouse){x=(point.x*static_cast<float>(desc.Width)/client.right-canvas.x)/canvas.scale;y=(point.y*static_cast<float>(desc.Height)/client.bottom-canvas.y)/canvas.scale;}
 const bool moved=mouse&&mouseSeen&&(point.x!=lastMouse.x||point.y!=lastMouse.y);mouseSeen=mouse;lastMouse=point;const auto hit=inventory_popup_art::HitTest(s,x,y);const auto edges=keys.Read(held);inventory_popup_art::Hit action{};using A=InventoryPopupAction;
 if(!(s.flags&InventoryPopupBusy)){
  if((edges.pressed&1)&&mouse)pressed=hit;
  if(edges.released&1){if(hit.action==pressed.action&&hit.row==pressed.row){action=hit;if(action.action==A::Focus)action.action=A::Confirm;}pressed={};}
  if(edges.released&4)action={A::Cancel,0};else if(edges.released&2){if(s.kind==1||s.amount)action={A::Confirm,s.kind==2?s.amount:s.selected};}
  else if(edges.pressed&120){if(s.kind==1)action={A::Focus,s.count==2?1-s.selected:0};else{const bool more=(edges.pressed&72)!=0;action={A::Amount,more?(s.amount<s.capacity?s.amount+1:s.capacity):(s.amount?s.amount-1:0)};}}
  else if(moved&&!(held&1)&&hit.action==A::Focus&&hit.row!=s.selected)action=hit;
 }else{pressed={};keys.Reset(held);}
 ui_art::Scene frame=art;
 if(s.kind==3&&!(s.flags&(InventoryPopupClosing|InventoryPopupNativeAnimation))){constexpr float pi=3.14159265358979323846f;const float angle=(GetTickCount()%1800)*(2*pi/1800.f);frame.Arc(1360,662,16,0,2*pi,1,0x457e888e);frame.Arc(1360,662,16,angle,angle+pi*.9f,2,0xffe2b66f);}
 if(s.flags&InventoryPopupClosing){const DWORD opacity=(56-s.nativeFrame)*255/8;for(auto& batch:frame.batches)for(auto& vertex:batch.vertices)vertex.color=(vertex.color&0x00ffffffu)|((((vertex.color>>24)*opacity+127)/255)<<24);}
 ui_pointer::AppendHiddenCursor(frame,mouse,x,y);if(!ui_art::Draw(d,frame,assets,canvas.scale,canvas.x,canvas.y))return false;
 if(static_cast<DWORD>(action.action)&&sender(static_cast<DWORD>(action.action),action.row,s.generation))ui_sound::Emit(action.action==A::Cancel?ui_sound::Cue::Back:action.action==A::Confirm?ui_sound::Cue::Confirm:ui_sound::Cue::Hover);return true;
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;InventoryPopupStateV1 s{};bool ready=false;
 try{const auto now=GetTickCount();if((!getter||!setter||!sender)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<InventoryPopupGetter>(GetProcAddress(input,"AITDGetInventoryPopupState"));sender=reinterpret_cast<InventoryPopupSender>(GetProcAddress(input,"AITDQueueInventoryPopupAction"));setter=reinterpret_cast<InventoryPopupReady>(GetProcAddress(input,"AITDSetInventoryPopupReady"));}}
  if(!source&&!destination&&getter&&setter&&sender&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&InventoryPopupVisible)&&now-s.tick<=250)ready=Present(d,s);
 }catch(...){ready=false;}if(ready)readyGeneration=s.generation;else Hide();if(setter)setter(ready?TRUE:FALSE,readyGeneration);
 const DWORD status=(getter?1u:0u)|(sender?2u:0u)|(setter?4u:0u)|(ready?8u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Inventory choice UI status=%08lx generation=%lu frame=%lu build=%08lx",status,s.generation,s.nativeFrame,static_cast<DWORD>(buildResult));}
}
}
