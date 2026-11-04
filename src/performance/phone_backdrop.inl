// Live scene dimming only: no downsampling, blur or per-frame capture.
#include "phone_state.h"
namespace phone_backdrop {
static IDirect3DDevice9* owner{};static ui_art::Assets assets;
static UINT width{},height{};static PhoneGetter getter{};static DWORD lookup{};
static void Invalidate(IDirect3DDevice9* d){if(owner==d){assets.Reset();owner=nullptr;width=height=0;}}
static bool Render(IDirect3DDevice9* d,bool gps){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target;D3DSURFACE_DESC desc{};
 if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||target.Get()!=back.Get()||FAILED(back->GetDesc(&desc)))return false;
 for(DWORD n=1;n<4;++n){Microsoft::WRL::ComPtr<IDirect3DSurface9> mrt;if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
 if(owner!=d){if(owner)Invalidate(owner);owner=d;}width=desc.Width;height=desc.Height;
 ui_art::Scene scene;
 const auto region=[&](float l,float t,float r,float b){scene.Rect(l*width,t*height,(r-l)*width,(b-t)*height,0xa0000000);};
 // The native map is already rendered: veil only the surrounding world.
 if(gps){region(0,0,1,.214f);region(0,.8453f,1,1);region(0,.214f,.056f,.8453f);region(.9458f,.214f,1,.8453f);}
 else region(0,0,1,1);
 return ui_art::Draw(d,scene,assets,1.f);
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(source||destination||!options_overlay::enabled)return;
 const auto now=GetTickCount();if(!getter&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto module=GetModuleHandleW(L"dinput8.dll");if(module&&FfSameDirectory(module))getter=reinterpret_cast<PhoneGetter>(GetProcAddress(module,"AITDGetPhoneState"));}
 PhoneStateV1 s{};if(!getter||!getter(&s,sizeof s)||s.size!=sizeof s||s.version!=1||now-s.tick>250||!(s.flags&PhoneAuthenticated)){Invalidate(d);return;}
 try{Render(d,s.nativePage==1);}catch(...){Invalidate(d);}
}
}

