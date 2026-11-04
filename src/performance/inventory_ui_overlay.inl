#include "inventory_ui_art.h"
namespace inventory_ui_overlay {
static bool drawing=false,visible=false;static InventoryUiGetter getter=nullptr;static InventoryUiReady setter=nullptr;
static DWORD lookup{},readyGeneration{},lastStatus=~0u;static IDirect3DDevice9* owner=nullptr;static ui_art::Scene art;static ui_art::Assets assets;static InventoryUiStateV1 cached{};static bool handIcon=false,bulletIcon=false,mouseIcon=false;
static HRESULT buildResult=S_OK;static InventoryUiImageGetter imageGetter=nullptr;static unsigned imageMask=0;
static void Hide(){visible=false;owner=nullptr;art={};}
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;Hide();assets.Reset();if(setter)setter(FALSE,readyGeneration);}
static bool Present(IDirect3DDevice9* d,const InventoryUiStateV1& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
 FavoritesStateV1 fav{};if(favorites_overlay::State(fav)&&(fav.flags&1)&&fav.mode==1)return true;
 auto key=s;key.bleeding.nativeFrame=key.bleeding.nativeFrame?2:0;
 if(owner!=d){assets.Reset();handIcon=phone_presentation::LoadUiImage(d,assets,L"inventory_hands.png",132,118);bulletIcon=phone_presentation::LoadUiImage(d,assets,L"inventory_bullet.png",88,32,1);mouseIcon=phone_presentation::LoadUiImage(d,assets,L"inventory_mouse_left.png",48,51,2);}
 unsigned nativeImages=0;if(imageGetter)for(unsigned n=0;n<3;++n)if(s.images[n].bitmap){Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;if(imageGetter(n,s.generation,&texture)==S_OK&&texture&&SUCCEEDED(assets.SetImage(d,n?n+2:1,texture.Get())))nativeImages|=1u<<n;}
 if(!(nativeImages&1)&&(imageMask&1))bulletIcon=phone_presentation::LoadUiImage(d,assets,L"inventory_bullet.png",88,32,1);
 if(!visible||imageMask!=nativeImages||owner!=d||cached.generation!=s.generation||memcmp(reinterpret_cast<const BYTE*>(&cached)+12,reinterpret_cast<const BYTE*>(&key)+12,sizeof key-12)){ui_art::Scene next;buildResult=inventory_ui_art::Build(s,next,handIcon,bulletIcon,mouseIcon,nativeImages);if(FAILED(buildResult))return false;art=std::move(next);cached=key;owner=d;visible=true;imageMask=nativeImages;}
 const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,1.f);RECT client{};POINT point{};const bool mouse=ui_pointer::ReadClient(window,client,point);const float x=mouse?(point.x*static_cast<float>(desc.Width)/client.right-canvas.x)/canvas.scale:0,y=mouse?(point.y*static_cast<float>(desc.Height)/client.bottom-canvas.y)/canvas.scale:0;
 if(ui_pointer::centralCursor)return ui_art::Draw(d,art,assets,canvas.scale,canvas.x,canvas.y);
 ui_art::Scene frame=art;ui_pointer::AppendHiddenCursor(frame,mouse,x,y);return ui_art::Draw(d,frame,assets,canvas.scale,canvas.x,canvas.y);
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;InventoryUiStateV1 s{};bool ready=false;
 try{const auto now=GetTickCount();if((!getter||!setter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){imageGetter=reinterpret_cast<InventoryUiImageGetter>(GetProcAddress(input,"AITDAcquireInventoryImage"));getter=reinterpret_cast<InventoryUiGetter>(GetProcAddress(input,"AITDGetInventoryUiState"));setter=reinterpret_cast<InventoryUiReady>(GetProcAddress(input,"AITDSetInventoryUiReady"));}}
  if(!source&&!destination&&getter&&setter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&InventoryUiVisible)&&now-s.tick<=250)ready=Present(d,s);
 }catch(...){ready=false;}if(ready)readyGeneration=s.generation;else Hide();if(setter)setter(ready?TRUE:FALSE,readyGeneration);
 const DWORD status=(getter?1u:0u)|(setter?2u:0u)|(ready?4u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Inventory UI status=%08lx generation=%lu case=%lu prompts=%lu build=%08lx",status,s.generation,s.nativeCase,s.promptCount,static_cast<DWORD>(buildResult));}
}
}
