#include "favorites_art.h"
#include <algorithm>
#include <cstring>
namespace favorites_overlay {
static FavoritesStateGetter getter=nullptr;static DWORD nextLookup=0;
static bool State(FavoritesStateV1& s){
 if(!favoritesOverlayEnabled)return false;
 if(!getter&&(!nextLookup||GetTickCount()-nextLookup<0x80000000u)){nextLookup=GetTickCount()+1000;HMODULE input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input))getter=reinterpret_cast<FavoritesStateGetter>(GetProcAddress(input,"AITDGetFavoritesState"));}
 if(!getter||!getter(&s,sizeof(s))||s.size!=sizeof(s)||s.version!=1||!(s.flags&3)||GetTickCount()-s.tick>250)return false;
 DWORD pid=0;const HWND foreground=GetForegroundWindow();GetWindowThreadProcessId(foreground,&pid);return pid==GetCurrentProcessId()&&!IsIconic(foreground);
}

static favorites_art::Labels labels;static bool labelsReady=false;
static FavoritesStateV1 cached{};static ui_art::Scene baseArt;
static volatile DWORD meshBuilds=0;
static bool Frame(const FavoritesStateV1& state,unsigned width,unsigned height,ui_art::Scene& frame){
 frame={};if(width<320||height<200||!favorites_art::Valid(state)||GetTickCount()-state.tick>250)return false;
 if(!labelsReady){if(FAILED(favorites_art::Prepare(labels)))return false;labelsReady=true;}
 auto key=state;key.tick=0;key.cursorX=key.cursorY=0;
 if(baseArt.batches.empty()||memcmp(&cached,&key,sizeof key)){if(!favorites_art::Build(state,labels,baseArt))return false;cached=key;++meshBuilds;}
 frame=baseArt;favorites_art::Cursor(state,width,height,frame);return true;
}
static bool Draw(IDirect3DDevice9* device,const FfDevice& metadata,const FavoritesStateV1& state){try{
 if(!device||!metadata.usable||metadata.device!=device)return false;
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};
 if(FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(device->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc)))return false;
 for(UINT n=1;n<(std::min)(4u,metadata.targets);++n){mrt.Reset();if(SUCCEEDED(device->GetRenderTarget(n,&mrt))&&mrt)return false;}
 ui_art::Scene frame;if(!Frame(state,desc.Width,desc.Height,frame))return false;
 for(const auto& batch:frame.batches)if(batch.vertices.size()/3>metadata.maxPrimitives)return false;
 // CPU outline meshes only: no font atlas and no retained default-pool resource.
 // Both themed and fallback paths share this cache and exactly the same art.
 ui_art::Assets assets;const float scale=desc.Height/1080.f;
 return ui_art::Draw(device,frame,assets,scale,desc.Width*.5f-960*scale,0);
 }catch(...){return false;}
}
}
