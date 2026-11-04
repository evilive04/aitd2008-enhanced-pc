#include "episode_end_art.h"
namespace episode_end_overlay {
static bool drawing=false,visible=false,mouseSeen=false;static EpisodeEndGetter getter=nullptr;static EpisodeEndSender sender=nullptr;static EpisodeEndReady setter=nullptr;
static DWORD lookup{},readyGeneration{},lastStatus=~0u,generation{};static IDirect3DDevice9* owner=nullptr;static ui_art::Scene art;static ui_art::Assets assets;static EpisodeEndStateV1 cached{};
static menu_presentation_policy::KeyEdges keys;static POINT lastMouse{};static unsigned pressed=~0u;
static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT};unsigned held=0;for(unsigned n=0;n<6;++n)if(GetAsyncKeyState(codes[n])&0x8000)held|=1u<<n;return held;}
static void Hide(){visible=mouseSeen=false;pressed=~0u;keys.Reset(Keys());art={};owner=nullptr;}
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;Hide();assets.Reset();if(setter)setter(FALSE,readyGeneration);}
static bool Present(IDirect3DDevice9* d,const EpisodeEndStateV1& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
 const auto held=Keys();if(!visible||generation!=s.generation||owner!=d){Hide();visible=true;owner=d;generation=s.generation;keys.Reset(held);}
 if(art.batches.empty()||memcmp(reinterpret_cast<const BYTE*>(&cached)+12,reinterpret_cast<const BYTE*>(&s)+12,sizeof s-12)){if(FAILED(episode_end_art::Build(s,art)))return false;cached=s;}
 const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT point{};const bool mouse=ui_pointer::ReadClient(window,client,point);float x{},y{};unsigned hit=~0u;
 if(mouse){x=(point.x*static_cast<float>(desc.Width)/client.right-canvas.x)/canvas.scale;y=(point.y*static_cast<float>(desc.Height)/client.bottom-canvas.y)/canvas.scale;for(unsigned n=0;n<2;++n)if(episode_end_art::Row(n).Contains(x,y))hit=n;}
 const bool moved=mouse&&mouseSeen&&(point.x!=lastMouse.x||point.y!=lastMouse.y);mouseSeen=mouse;lastMouse=point;const auto edges=keys.Read(held);EpisodeEndAction action{};unsigned row=s.selected;
 if(!(s.flags&EpisodeEndBusy)){
  if((edges.pressed&1)&&mouse)pressed=hit;
  if(edges.released&1){if(hit<2&&hit==pressed){action=EpisodeEndAction::Activate;row=hit;}pressed=~0u;}
  if(edges.released&2)action=EpisodeEndAction::Activate;
  else if(edges.pressed&60){action=EpisodeEndAction::Focus;row=1-s.selected;}
  else if(moved&&!(held&1)&&hit<2&&hit!=s.selected){action=EpisodeEndAction::Focus;row=hit;}
 }else{pressed=~0u;keys.Reset(held);}
 ui_art::Scene frame=art;ui_pointer::AppendHiddenCursor(frame,mouse,x,y);if(!ui_art::Draw(d,frame,assets,canvas.scale,canvas.x,canvas.y))return false;
 if(static_cast<DWORD>(action)&&sender(static_cast<DWORD>(action),row,s.generation))ui_sound::Emit(action==EpisodeEndAction::Activate?ui_sound::Cue::Confirm:ui_sound::Cue::Hover);return true;
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;EpisodeEndStateV1 s{};bool ready=false;
 try{const auto now=GetTickCount();if((!getter||!setter||!sender)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<EpisodeEndGetter>(GetProcAddress(input,"AITDGetEpisodeEndState"));sender=reinterpret_cast<EpisodeEndSender>(GetProcAddress(input,"AITDQueueEpisodeEndAction"));setter=reinterpret_cast<EpisodeEndReady>(GetProcAddress(input,"AITDSetEpisodeEndReady"));}}
  if(!source&&!destination&&getter&&setter&&sender&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&EpisodeEndVisible)&&now-s.tick<=250)ready=Present(d,s);
 }catch(...){ready=false;}if(ready)readyGeneration=s.generation;else Hide();if(setter)setter(ready?TRUE:FALSE,readyGeneration);
 const DWORD status=(getter?1u:0u)|(sender?2u:0u)|(setter?4u:0u)|(ready?8u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Episode end UI status=%08lx generation=%lu dispatches=%lu rejected=%lu",status,s.generation,s.dispatches,s.rejected);}
}
}
