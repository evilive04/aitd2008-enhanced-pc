#include "recap_art.h"
namespace recap_overlay {
static bool drawing=false,visible=false,haveMouse=false;static RecapGetter getter=nullptr;static RecapSender sender=nullptr;static RecapReady setter=nullptr;
static DWORD lookup{},epoch{},lastStatus=~0u;static unsigned selected=RecapPlay,pressed{};static POINT previous{};static menu_presentation_policy::KeyEdges keys;
static ui_art::Scene art;static ui_art::Assets assets;static RecapStateV1 cached{};static IDirect3DDevice9* owner=nullptr;static HRESULT buildResult=S_OK;
static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN,VK_PRIOR,VK_NEXT};unsigned held=0;for(unsigned n=0;n<9;++n)if(GetAsyncKeyState(codes[n])&0x8000)held|=1u<<n;return held;}
static void Hide(){visible=false;haveMouse=false;pressed=0;owner=nullptr;art={};keys.Reset(Keys());}
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;Hide();assets.Reset();if(setter)setter(FALSE,epoch);}
static bool Present(IDirect3DDevice9* d,const RecapStateV1& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 // Native video coordinates are source-bound. Other aspect ratios must retain
 // the original renderer until their native viewport transform is proven.
 if(std::abs(desc.Width/static_cast<float>(desc.Height)-16.f/9.f)>.01f)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
 const unsigned held=Keys();if(!visible||cached.generation!=s.generation||owner!=d){Hide();visible=true;owner=d;selected=RecapPlay;keys.Reset(held);}
 const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height);RECT client{};POINT pointer{};unsigned hit=0;
 if(ui_pointer::ReadClient(window,client,pointer)){const auto p=canvas.Inverse({pointer.x*desc.Width/static_cast<float>(client.right),pointer.y*desc.Height/static_cast<float>(client.bottom)});hit=recap_art::Hit(p.x,p.y);
  if(haveMouse&&(pointer.x!=previous.x||pointer.y!=previous.y)&&hit)selected=hit;previous=pointer;haveMouse=true;}
 const auto edges=keys.Read(held);unsigned action=0;
 if(!(s.flags&RecapBusy)){
  if(edges.pressed&(1u<<3))action=RecapPreviousChapter;if(edges.pressed&(1u<<4))action=RecapNextChapter;
  if(edges.pressed&(1u<<7))action=RecapPreviousSequence;if(edges.pressed&(1u<<8))action=RecapNextSequence;
  if(edges.pressed&((1u<<5)|(1u<<6)))selected=selected==RecapBack?RecapPlay:RecapBack;
  if(edges.pressed&1)pressed=hit;if(edges.released&1){if(hit&&hit==pressed)action=hit;pressed=0;}
  if(edges.released&(1u<<1))action=selected;if(edges.released&(1u<<2))action=RecapBack;
 }
 if(s.flags&RecapBusy){if(pressed!=RecapBack)pressed=0;if(edges.released&(1u<<2))action=RecapBack;if(edges.pressed&1)pressed=hit==RecapBack?hit:0;if(edges.released&1){if(hit==RecapBack&&pressed==RecapBack)action=RecapBack;pressed=0;}}
 auto key=s;key.tick=key.dispatches=key.rejected=key.videoLoaded=0;key.flags&=~RecapBusy;
 if(art.batches.empty()||memcmp(&key,&cached,sizeof key)){
  ui_art::Scene next;buildResult=recap_art::Build(key,0,next);
  if(FAILED(buildResult)){
   // Keep the last complete frame for this same owner while the next one is
   // being prepared. Never dispatch a click against stale displayed contents.
   if(art.batches.empty()||cached.generation!=s.generation)return false;
   return ui_art::Draw(d,art,assets,canvas.scale,canvas.x,canvas.y);
  }
  art=std::move(next);cached=key;
 }
 ui_art::Scene frame=art;recap_art::Highlight(frame,selected);if(!ui_art::Draw(d,frame,assets,canvas.scale,canvas.x,canvas.y))return false;
 if(action&&sender(action,s.generation))ui_sound::Emit(action==RecapBack?ui_sound::Cue::Back:ui_sound::Cue::Confirm);return true;
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;RecapStateV1 s{};bool okay=false;
 try{const auto now=GetTickCount();if((!getter||!sender||!setter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<RecapGetter>(GetProcAddress(input,"AITDGetRecapState"));sender=reinterpret_cast<RecapSender>(GetProcAddress(input,"AITDQueueRecapAction"));setter=reinterpret_cast<RecapReady>(GetProcAddress(input,"AITDSetRecapReady"));}}
  if(!source&&!destination&&getter&&sender&&setter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&RecapVisible)&&now-s.tick<=250)okay=Present(d,s);
 }catch(...){okay=false;}if(okay)epoch=s.generation;else Hide();if(setter)setter(okay?TRUE:FALSE,epoch);
 const DWORD status=(getter?1u:0u)|(sender?2u:0u)|(setter?4u:0u)|(okay?8u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Recap UI status=%08lx generation=%lu build=%08lx",status,s.generation,static_cast<DWORD>(buildResult));}
}
}
