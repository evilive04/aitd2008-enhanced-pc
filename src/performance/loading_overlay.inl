#include "loading_art.h"
#include "loading_tip_content.h"
namespace loading_overlay {
static bool drawing=false,visible=false;static LoadingGetter getter=nullptr;static LoadingReady setter=nullptr;static LoadingImage acquireImage=nullptr;
static DWORD lookup{},readyGeneration{},lastStatus=~0u,started{};static IDirect3DDevice9* owner=nullptr;
static ui_art::Assets assets;static loading_art::Prepared prepared;static LoadingStateV3 cached{};
static void Invalidate(IDirect3DDevice9* d){if(drawing||owner!=d)return;owner=nullptr;visible=false;prepared={};assets.Reset();if(setter)setter(FALSE,readyGeneration);}
static bool Present(IDirect3DDevice9* d,const LoadingStateV3& s){
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};
 if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
 // Startup's own black-logo/reveal already owns this interval.
 MenuStateV1 menu{};if(menu_overlay::getter&&menu_overlay::getter(&menu,sizeof menu)&&(menu.flags&MenuTitleVisible))return false;
 const bool changed=!visible||owner!=d||memcmp(reinterpret_cast<const BYTE*>(&cached)+12,reinterpret_cast<const BYTE*>(&s)+12,sizeof s-12);
 if(changed){loading_art::Prepared next;if(FAILED(loading_art::Prepare(s,next)))return false;prepared=std::move(next);cached=s;if(!visible||owner!=d||readyGeneration!=s.generation)started=GetTickCount();owner=d;visible=true;}
 const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);ui_art::Scene art;
 if(prepared.full)art.Rect(-canvas.x/canvas.scale,-canvas.y/canvas.scale,desc.Width/canvas.scale,desc.Height/canvas.scale,0xff000000);
 ui_art::Scene panel;loading_art::Build(prepared,GetTickCount()-started,panel);for(auto& batch:panel.batches)art.batches.push_back(std::move(batch));
 if(prepared.picture){
  if(!acquireImage)return false;Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;if(FAILED(acquireImage(s.generation,&texture))||!texture)return false;
  D3DSURFACE_DESC picture{};const auto& spec=loading_tip_content::pictures[prepared.picture-1].image;
  if(FAILED(texture->GetLevelDesc(0,&picture))||picture.Width!=spec.width||picture.Height!=spec.height||FAILED(assets.SetImage(d,0,texture.Get())))return false;
  const auto& b=prepared.imageBounds;if(!art.Image(0,picture.Width,picture.Height,b.x,b.y,b.w,b.h))return false;
 }
 return ui_art::Draw(d,art,assets,canvas.scale,canvas.x,canvas.y);
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;LoadingStateV3 s{};bool ready=false;
 try{
  const auto now=GetTickCount();if((!getter||!setter||!acquireImage)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<LoadingGetter>(GetProcAddress(input,"AITDGetLoadingState"));setter=reinterpret_cast<LoadingReady>(GetProcAddress(input,"AITDSetLoadingReady"));acquireImage=reinterpret_cast<LoadingImage>(GetProcAddress(input,"AITDAcquireInformationImage"));}}
  if(!source&&!destination&&getter&&setter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==3&&(s.flags&LoadingVisible)&&now-s.tick<=250)ready=Present(d,s);
 }catch(...){ready=false;}
 if(ready)readyGeneration=s.generation;else{visible=false;prepared={};owner=nullptr;}if(setter)setter(ready?TRUE:FALSE,readyGeneration);
 const DWORD status=(getter?1u:0u)|(setter?2u:0u)|(ready?4u:0u)|(s.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Loading UI status=%08lx generation=%lu native_mode=%lu",status,s.generation,s.nativeMode);}
}
}
