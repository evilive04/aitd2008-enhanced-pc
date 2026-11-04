// Include after ff_overlay.inl. This optional vector renderer consumes the same
// authenticated providers and radial coordinates; old renderer remains fallback.
#include "ui_theme.h"
#include "ui_art.h"
#include <wrl/client.h>
#include <map>
#include <string>
#include <utility>
namespace hud_presentation {
using Microsoft::WRL::ComPtr;
static bool enabled=false;
static volatile DWORD radialDraws=0,radialDrawFailures=0; // Read-only runtime diagnostics in the component map.
static ui_theme::Label ammoLabel;static DWORD cachedRounds=~0u;
static ui_art::Assets materials;static IDirect3DDevice9* materialOwner=nullptr;
static void Invalidate(IDirect3DDevice9* device){if(materialOwner==device){materials.Reset();materialOwner=nullptr;}}
static void MaterialPanel(ui_art::Scene& scene,float x,float y,float w,float h){scene.Frame(x,y,w,h);}
static void AppendSolid(ui_art::Scene& scene,const ui_theme::Mesh& mesh){if(mesh.empty())return;scene.batches.push_back({ui_art::Material::Solid,{}});auto& v=scene.batches.back().vertices;v.reserve(mesh.size());for(const auto& p:mesh)v.push_back({p.x,p.y,p.z,p.rhw,p.color,0,0});}
static void HudPanel(ui_theme::Mesh& mesh,float x,float y,float w,float h){
    ui_theme::Panel(mesh,x,y,w,h,false);

}
static bool BuildFavorites(ui_theme::Mesh& mesh,UINT width,UINT height,const FavoritesStateV1& state,ui_art::Scene* surface=nullptr){
    ui_art::Scene art;if(!favorites_overlay::Frame(state,width,height,art))return false;
    const float offset=width/(height/1080.f)*.5f-960;
    for(auto& batch:art.batches)for(auto& v:batch.vertices)v.x+=offset;
    if(surface)*surface=std::move(art);
    else for(const auto& batch:art.batches)for(const auto& v:batch.vertices)mesh.push_back({v.x,v.y,v.z,v.rhw,v.color});
    return true;
}
static bool BuildHud(ui_theme::Mesh& mesh,UINT width,UINT height,bool fastForward,bool aiming,const WeaponHudStateV1* ammo,ui_art::Scene* surface=nullptr){
    if(width<320||height<200)return false;const float s=height/1080.f,w=width/s;constexpr auto cream=0xffe8e4d5u;
    if(fastForward){const float x=w*.95f-104,y=1080*.95f-56;if(surface)MaterialPanel(*surface,x,y,104,56);else HudPanel(mesh,x,y,104,56);
        for(unsigned n=0;n<2;++n){const float a=x+25+n*26;ui_theme::Triangle(mesh,{a,y+15},{a,y+41},{a+19,y+28},cream);}}
    else if(ammo){if(ammo->rounds>9999)return false;
        if(cachedRounds!=ammo->rounds){wchar_t value[16]{};swprintf_s(value,L"%02lu",ammo->rounds);ui_theme::Label pending;if(FAILED(ui_theme::BuildLabel(value,28,96,42,pending,L"Times New Roman")))return false;ammoLabel=std::move(pending);cachedRounds=ammo->rounds;}
        // Original inventory ammunition artwork, rotated upright without changing
        // its proportions. The compact counter has no empty separator column.
        const bool fire=(ammo->flags&WeaponHudFireAmmo)!=0;
        const float flameSpace=fire?20.f:0.f;
        const float panelWidth=(std::max)(82.f,ammoLabel.mesh.advanceWidth+46.f)+flameSpace;
        const float x=w*.95f-panelWidth,y=1080*.95f-50;
        if(surface){MaterialPanel(*surface,x,y,panelWidth,50);surface->Image(10,31,88,x+10,y+9,10,32);}else HudPanel(mesh,x,y,panelWidth,50);
        // Original inventory shape61/bitmap60: exact authored crop, aspect and
        // alpha. Its presence follows the actual next loaded round, not hover.
        if(fire&&surface)surface->ImageRegion(11,32,64,{0,0,27,44},x+26,y+13,15,24);
        ui_theme::Text(mesh,ammoLabel,x+32+flameSpace,y+8,ammo->rounds?cream:0xffce9a83);
    }else if(!aiming)return false;
    // Return pixel coordinates. Native sight is copied after scaling so its
    // half-pixel convention and exact vertices survive theme Draw(scale=1).
    for(auto& v:mesh){v.x*=s;v.y*=s;}
    if(surface)for(auto& batch:surface->batches)for(auto& v:batch.vertices){v.x*=s;v.y*=s;}
    if(aiming&&!fastForward){const auto sight=FfBuildAimMesh(width,height);for(unsigned i=0;i<sight.count;++i){const auto& v=sight.vertices[i];mesh.push_back({v.x+.5f,v.y+.5f,v.z,v.rhw,v.color});}}
    return true;
}
static bool Target(IDirect3DDevice9* device,const FfDevice& metadata,D3DSURFACE_DESC& desc){
    if(!device||!metadata.usable||!metadata.targets||metadata.targets>4)return false;
    ComPtr<IDirect3DSurface9> back,target,extra;
    if(FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(back->GetDesc(&desc))||FAILED(device->GetRenderTarget(0,&target))||back.Get()!=target.Get())return false;
    for(unsigned i=1;i<metadata.targets;++i){extra.Reset();const auto hr=device->GetRenderTarget(i,&extra);if(extra||(FAILED(hr)&&hr!=D3DERR_NOTFOUND))return false;}return true;
}
// Draw before inventory/HUD chrome so the modal veil dims the world only.
static void DrawFavoritesBackdrop(IDirect3DDevice9* device,const RECT* source,const RECT* destination)noexcept{
    if(!enabled||source||destination)return;FavoritesStateV1 state{};
    if(!favorites_overlay::State(state)||!(state.flags&1))return;
    try{FfDevice metadata{};AcquireSRWLockShared(&ffDeviceLock);if(const auto* known=FfFind(device))metadata=*known;ReleaseSRWLockShared(&ffDeviceLock);
        D3DSURFACE_DESC desc{};if(!Target(device,metadata,desc))return;
        ui_art::Scene veil;veil.Rect(0,0,static_cast<float>(desc.Width),static_cast<float>(desc.Height),0xb8000000);
        ui_art::Assets solid;ui_art::Draw(device,veil,solid,1.f);
    }catch(...){/* Do not take input ownership if a renderer cannot draw. */}
}
}
// true means rendered successfully (or another own overlay is currently drawing).
// false requests the unchanged DrawFastForwardOverlay fallback for this frame.
static bool DrawThemedGameOverlay(IDirect3DDevice9* device,const RECT* source,const RECT* destination)noexcept{
    using namespace hud_presentation;
    if(!hud_presentation::enabled||source||destination)return false;
    const bool fastForward=ffOverlayEnabled&&FfActive(),aiming=!fastForward&&AimActive();WeaponHudStateV1 ammo{};const bool ammunition=!fastForward&&AmmoActive(&ammo);
    FavoritesStateV1 favorites{};const bool radial=favorites_overlay::State(favorites);
    if(!fastForward&&!ammunition&&!aiming&&!radial){Invalidate(device);return false;}
    if(InterlockedCompareExchange(&ffDrawing,1,0))return true;
    bool drawn=false;
    try{FfDevice metadata{};AcquireSRWLockShared(&ffDeviceLock);if(const auto* known=FfFind(device))metadata=*known;ReleaseSRWLockShared(&ffDeviceLock);
        D3DSURFACE_DESC desc{};if(Target(device,metadata,desc)){ui_theme::Mesh mesh;mesh.reserve(50000);ui_art::Scene surface;
            const bool built=radial?BuildFavorites(mesh,desc.Width,desc.Height,favorites,&surface):BuildHud(mesh,desc.Width,desc.Height,fastForward,aiming,ammunition?&ammo:nullptr,&surface);
            if(built&&mesh.size()/3<=metadata.maxPrimitives){
                if(radial){ui_art::Assets solid;AppendSolid(surface,mesh);drawn=ui_art::Draw(device,surface,solid,desc.Height/1080.f);}
                else{
                    if(materialOwner!=device){materials.Reset();materialOwner=nullptr;if(SUCCEEDED(materials.LoadLocal(device)))materialOwner=device;}
                    if(materialOwner==device){AppendSolid(surface,mesh);drawn=ui_art::Draw(device,surface,materials,1.f);}
                }
            }}
    }catch(...){drawn=false;}
    if(radial){if(drawn)++radialDraws;else ++radialDrawFailures;}
    InterlockedExchange(&ffDrawing,0);return drawn;
}
