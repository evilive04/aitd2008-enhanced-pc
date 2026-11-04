#include "calibration_art.h"
namespace calibration_overlay {
static bool drawing=false,visible=false,haveMouse=false,dragging=false;static CalibrationGetter getter=nullptr;static CalibrationSender sender=nullptr;static CalibrationReady setter=nullptr;
static DWORD lookup{},epoch{},lastStatus=~0u,repeatAt{};static unsigned dragRow{},pressed{};static POINT previous{};static menu_presentation_policy::KeyEdges keys;
static IDirect3DDevice9* owner=nullptr;static calibration_art::Prepared prepared;static CalibrationStateV1 cached{};static HRESULT buildResult=S_OK;
static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_UP,VK_DOWN,VK_TAB,VK_LEFT,VK_RIGHT};unsigned held=0;for(unsigned n=0;n<8;++n)if(GetAsyncKeyState(codes[n])&0x8000)held|=1u<<n;return held;}
static void Hide(){visible=false;haveMouse=dragging=false;pressed=repeatAt=0;owner=nullptr;prepared={};keys.Reset(Keys());}
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;Hide();if(setter)setter(FALSE,epoch);}
static bool Present(IDirect3DDevice9* d,const CalibrationStateV1& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}const auto window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
 const unsigned held=Keys();if(!visible||cached.generation!=s.generation||owner!=d){Hide();visible=true;owner=d;keys.Reset(held);}
 auto key=s;key.tick=key.dispatches=key.rejected=key.selected=0;key.opacity=1;for(auto& g:key.gauges)g={};
 if(prepared.scene.batches.empty()||memcmp(&key,&cached,sizeof key)){calibration_art::Prepared next;buildResult=calibration_art::Prepare(s,next);if(FAILED(buildResult))return false;prepared=std::move(next);cached=key;}
 const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT pointer{};ui_theme::Point pos{};bool pointerRead=ui_pointer::ReadClient(window,client,pointer);
 if(pointerRead){const auto p=canvas.Inverse({pointer.x*desc.Width/static_cast<float>(client.right),pointer.y*desc.Height/static_cast<float>(client.bottom)});pos={p.x,p.y};}
 const auto edge=keys.Read(held);DWORD action=0,row=s.selected;double normalized=0;
 if(!(s.flags&CalibrationBusy)){
  if(edge.released&(1u<<1))action=CalibrationConfirm;if(edge.released&(1u<<2))action=CalibrationCancel;
  if(edge.pressed&(1u<<5)){action=CalibrationFocus;row=s.page?0u:1u;}
  if((edge.pressed&((1u<<3)|(1u<<4)))&&s.page){action=CalibrationFocus;row=1+(s.selected-1+((edge.pressed&(1u<<3))?2u:1u))%3;}
  const unsigned arrows=held&((1u<<6)|(1u<<7));const DWORD now=GetTickCount();bool adjust=(edge.pressed&arrows)!=0;
  if(adjust)repeatAt=now+300;else if(arrows&&repeatAt&&static_cast<LONG>(now-repeatAt)>=0){adjust=true;repeatAt=now+16;}if(!arrows)repeatAt=0;
  if(adjust)action=(arrows&(1u<<6))?CalibrationPrevious:CalibrationNext;
  unsigned hit=0;if(pointerRead){
   for(unsigned n=0;n<2;++n)if(calibration_art::Tab(n).Contains(pos.x,pos.y))hit=n+1;
   for(unsigned n=s.page?1u:0u;n<(s.page?4u:1u);++n)if(calibration_art::Slider(n,s.page).Contains(pos.x,pos.y))hit=n+3;
   if(calibration_art::Footer(true).Contains(pos.x,pos.y))hit=7;if(calibration_art::Footer(false).Contains(pos.x,pos.y))hit=8;
  }
  if(edge.pressed&1){pressed=hit;if(hit>=3&&hit<=6){dragging=true;dragRow=hit-3;}}
  if(dragging&&(held&1)&&pointerRead){const auto b=calibration_art::Slider(dragRow,s.page);if(b.w>0){const auto& g=s.gauges[dragRow];normalized=(std::max)(0.0,(std::min)(1.0,(pos.x-b.x)/static_cast<double>(b.w)));const double current=(g.current-g.minimum)/static_cast<double>(g.maximum-g.minimum);
    if(s.selected!=dragRow||std::abs(normalized-current)>.00002){action=CalibrationGauge;row=dragRow;}
  }}
  if(edge.released&1){dragging=false;if(hit&&hit==pressed){if(hit<=2){action=CalibrationFocus;row=hit==1?0u:1u;}else if(hit>=7)action=hit==7?CalibrationConfirm:CalibrationCancel;}pressed=0;}
  if(!action&&!dragging&&pointerRead&&haveMouse&&(pointer.x!=previous.x||pointer.y!=previous.y)&&hit>=3&&hit<=6&&hit-3!=s.selected){action=CalibrationFocus;row=hit-3;}
 }
 if(pointerRead){previous=pointer;haveMouse=true;}ui_art::Scene art;if(!calibration_art::Build(s,prepared,art))return false;if(!menu_overlay::DrawBackdrop(d))return false;ui_art::Assets assets;if(!ui_art::Draw(d,art,assets,canvas.scale,canvas.x,canvas.y))return false;
 if(action&&sender(action,row,normalized,s.generation)&&(action==CalibrationConfirm||action==CalibrationCancel))ui_sound::Emit(action==CalibrationConfirm?ui_sound::Cue::Confirm:ui_sound::Cue::Back);return true;
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;CalibrationStateV1 s{};bool okay=false;
 try{const auto now=GetTickCount();if((!getter||!sender||!setter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<CalibrationGetter>(GetProcAddress(input,"AITDGetCalibrationState"));sender=reinterpret_cast<CalibrationSender>(GetProcAddress(input,"AITDQueueCalibrationAction"));setter=reinterpret_cast<CalibrationReady>(GetProcAddress(input,"AITDSetCalibrationReady"));}}
  if(!source&&!destination&&getter&&sender&&setter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&CalibrationVisible)&&now-s.tick<=250)okay=Present(d,s);
 }catch(...){okay=false;}if(okay)epoch=s.generation;else Hide();if(setter)setter(okay?TRUE:FALSE,epoch);const DWORD status=(getter?1u:0u)|(sender?2u:0u)|(setter?4u:0u)|(okay?8u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Calibration UI status=%08lx generation=%lu build=%08lx",status,s.generation,static_cast<DWORD>(buildResult));}
}
}
