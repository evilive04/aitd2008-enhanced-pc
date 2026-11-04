#pragma once
#include "ui_theme.h"
#include <wrl/client.h>
#include <string>
namespace ui_art {
inline volatile LONG completedScenes=0;
enum class Material { Solid, Slate, Roots, Image0, Image1, Image2, Image3, Image4, MouseLeft, MouseRight, CursorMetal, Logo, EpisodeCards, AmmoBullet, AmmoFlame };
struct Vertex { float x,y,z,rhw; D3DCOLOR color; float u,v; };
struct Batch { Material material; std::vector<Vertex> vertices; };
struct Bounds;
struct Scene {
    std::vector<Batch> batches;
    void Polygon(const std::vector<ui_theme::Point>&,D3DCOLOR,Material=Material::Solid);
    void Rect(float,float,float,float,D3DCOLOR);
    void Line(float,float,float,float,float,D3DCOLOR);
    void Arc(float,float,float,float,float,float,D3DCOLOR);
    void Text(const ui_theme::Label&,float,float,D3DCOLOR);
    void Frame(float,float,float,float);
    void Button(float,float,float,float,bool selected,bool enabled=true);
    // Uniform square placement preserves the authored root texture's proportions.
    void Roots(float,float,float,bool opposite=false);
    bool Image(unsigned slot,unsigned sourceWidth,unsigned sourceHeight,float x,float y,float w,float h);
    // Pixel region supplied by an authenticated source asset; fit its aspect,
    // preserving the original UV mapping rather than stretching unused padding.
    bool ImageRegion(unsigned slot,unsigned textureWidth,unsigned textureHeight,const Bounds& sourcePixels,float x,float y,float w,float h);
};
class Assets {
public:
    HRESULT Load(IDirect3DDevice9*,const std::wstring& directory);
    HRESULT LoadLocal(IDirect3DDevice9*);
    void Reset();
    IDirect3DTexture9* Texture(Material)const;
    HRESULT SetImage(IDirect3DDevice9*,unsigned slot,IDirect3DTexture9*);
    bool EnsureShared(IDirect3DDevice9*,Material)const;
private:
    Microsoft::WRL::ComPtr<IDirect3DTexture9> slate_,roots_;
    Microsoft::WRL::ComPtr<IDirect3DTexture9> images_[5];
    mutable Microsoft::WRL::ComPtr<IDirect3DTexture9> shared_[7];
    mutable bool sharedAttempted_[7]{};
};
// Outside BeginScene/EndScene, same complete native state restoration as ui_theme.
bool Draw(IDirect3DDevice9*,const Scene&,const Assets&,float scale,float ox=0,float oy=0);
struct OptionsLabels {
    ui_theme::Label title,rows[4],select,reset,back,enter,esc,x;
    HRESULT Build(const wchar_t* titleText=L"OPTIONS",const wchar_t* const* rowsText=nullptr,
                  const wchar_t* backText=L"Back",const wchar_t* resetText=L"Reset all settings");
};
struct Bounds { float x,y,w,h; bool Contains(float px,float py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;} };
Bounds OptionRow(unsigned);
void Options(Scene&,const OptionsLabels&,int selected=0,bool ingame=false);
}
