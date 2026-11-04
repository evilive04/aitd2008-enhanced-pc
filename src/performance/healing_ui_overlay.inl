#include "healing_ui_art.h"
namespace healing_ui_overlay {
static bool drawing=false,visible=false;static HealingUiGetter getter=nullptr;
static DWORD lookup{},lastStatus=~0u;static IDirect3DDevice9* owner=nullptr;
static HRESULT buildResult=S_OK;
static void Hide(){visible=false;owner=nullptr;}
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;Hide();}
static bool Present(IDirect3DDevice9* d,const HealingUiStateV2& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
 // Healing is deliberately unobstructed: keep the native 3D treatment view
 // and inputs, while the independent healing context still hides the minimap.
 (void)s;owner=d;visible=true;return true;
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;HealingUiStateV2 s{};bool ready=false;
 try{const auto now=GetTickCount();if(!getter&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<HealingUiGetter>(GetProcAddress(input,"AITDGetHealingUiState"));}}
  if(!source&&!destination&&getter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==2&&(s.flags&HealingUiVisible)&&now-s.tick<=250)ready=Present(d,s);
 }catch(...){ready=false;}if(!ready)Hide();
 const DWORD status=(getter?1u:0u)|(ready?4u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Healing UI status=%08lx generation=%lu build=%08lx",status,s.generation,static_cast<DWORD>(buildResult));}
}
}
