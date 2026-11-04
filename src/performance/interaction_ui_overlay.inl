#include "interaction_ui_art.h"
namespace interaction_ui_overlay {
static bool drawing=false,visible=false;static InteractionUiGetter getter=nullptr;static InteractionUiReady setter=nullptr;
static DWORD lookup{},readyGeneration{},lastStatus=~0u;static IDirect3DDevice9* owner=nullptr;static ui_art::Scene art;static ui_art::Assets assets;static InteractionUiStateV1 cached{};
static HRESULT buildResult=S_OK;static InteractionUiImageGetter imageGetter=nullptr;
static void Hide(){visible=false;owner=nullptr;art={};}
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;Hide();assets.Reset();if(setter)setter(FALSE,readyGeneration);}
static bool Present(IDirect3DDevice9* d,const InteractionUiStateV1& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
 if(!imageGetter)return false;
 for(unsigned n=0;n<4;++n){if(n==1&&!(s.traits&1))continue;if(n==2&&!(s.traits&2))continue;if(n==3&&!(s.traits&28))continue;Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;if(imageGetter(n,s.generation,&texture)!=S_OK||!texture||FAILED(assets.SetImage(d,n,texture.Get())))return false;}
 auto key=s;key.nativeFrame=0;
 if(!visible||owner!=d||cached.generation!=s.generation||memcmp(reinterpret_cast<const BYTE*>(&cached)+12,reinterpret_cast<const BYTE*>(&key)+12,sizeof key-12)){ui_art::Scene next;buildResult=interaction_ui_art::Build(s,next);if(FAILED(buildResult))return false;art=std::move(next);cached=key;owner=d;visible=true;}
 const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,1.f);
 return ui_art::Draw(d,art,assets,canvas.scale,canvas.x,canvas.y);
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;InteractionUiStateV1 s{};bool ready=false;
 try{const auto now=GetTickCount();if((!getter||!setter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){imageGetter=reinterpret_cast<InteractionUiImageGetter>(GetProcAddress(input,"AITDAcquireInteractionImage"));getter=reinterpret_cast<InteractionUiGetter>(GetProcAddress(input,"AITDGetInteractionUiState"));setter=reinterpret_cast<InteractionUiReady>(GetProcAddress(input,"AITDSetInteractionUiReady"));}}
  if(!source&&!destination&&getter&&setter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&InteractionUiVisible)&&now-s.tick<=250)ready=Present(d,s);
 }catch(...){ready=false;}if(ready)readyGeneration=s.generation;else Hide();if(setter)setter(ready?TRUE:FALSE,readyGeneration);
 const DWORD status=(getter?1u:0u)|(setter?2u:0u)|(ready?4u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Interaction UI status=%08lx generation=%lu build=%08lx",status,s.generation,static_cast<DWORD>(buildResult));}
}
}
