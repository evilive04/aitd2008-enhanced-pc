#include "qte_ui_art.h"
namespace qte_ui_overlay {
static bool drawing=false,visible=false;static QteUiGetter getter=nullptr;static QteUiReady setter=nullptr;
static DWORD lookup{},readyGeneration{},lastStatus=~0u;static IDirect3DDevice9* owner=nullptr;static ui_art::Scene art;static ui_art::Assets assets;static QteUiStateV1 cached{};
static HRESULT buildResult=S_OK;
static void Hide(){visible=false;owner=nullptr;art={};}
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;Hide();assets.Reset();if(setter)setter(FALSE,readyGeneration);}
static bool Present(IDirect3DDevice9* d,const QteUiStateV1& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
 auto key=s;key.nativeFrame=key.phase=key.meter1=key.meter2=0;key.flags&=~QteUiTimingOpen;
 if(!visible||owner!=d||cached.generation!=s.generation||memcmp(reinterpret_cast<const BYTE*>(&cached)+12,reinterpret_cast<const BYTE*>(&key)+12,sizeof key-12)){ui_art::Scene next;buildResult=qte_ui_art::Build(s,next);if(FAILED(buildResult))return false;art=std::move(next);cached=key;owner=d;visible=true;}
 const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,1.f);
 ui_art::Scene frame=art;qte_ui_art::Animate(s,frame);return ui_art::Draw(d,frame,assets,canvas.scale,canvas.x,canvas.y);
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;QteUiStateV1 s{};bool ready=false;
 try{const auto now=GetTickCount();if((!getter||!setter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<QteUiGetter>(GetProcAddress(input,"AITDGetQteUiState"));setter=reinterpret_cast<QteUiReady>(GetProcAddress(input,"AITDSetQteUiReady"));}}
  if(!source&&!destination&&getter&&setter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&QteUiVisible)&&now-s.tick<=250)ready=Present(d,s);
 }catch(...){ready=false;}if(ready)readyGeneration=s.generation;else Hide();if(setter)setter(ready?TRUE:FALSE,readyGeneration);
 const DWORD status=(getter?1u:0u)|(setter?2u:0u)|(ready?4u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("QTE UI status=%08lx generation=%lu build=%08lx",status,s.generation,static_cast<DWORD>(buildResult));}
}
}
