#pragma once
#include <d3d9.h>

// Explicit material inputs only. No framebuffer/hue inference or temporal history.
namespace sss_diffusion {
struct Settings {
    bool enabled=false;
    float radiusWorld=.007f,amount=1.f;
    float depthAbsolute=.003f,depthRelative=.002f,maxRadiusPixels=24.f;
    unsigned maxWorkingDimension=1024; // Only the explicit legacy separable path uses this bound.
    bool fullResolution=true; // One direct 2D pass, no intermediate allocations or resampling.
    bool maskDepth=false;
    float depthSign=1.f; // Exactly +1 or -1; converts both source Z channels to positive distance.
};
struct Inputs {
    // All inputs use matching full-scene UVs, dimensions and linear RGB.
    // diffuse is the additive diffuse component already included in scene.
    IDirect3DTexture9* sceneLinear=nullptr;
    IDirect3DTexture9* diffuseLinear=nullptr;
    IDirect3DTexture9* linearDepth=nullptr; // R * depthSign = positive view-space distance.
    IDirect3DTexture9* skinMask=nullptr; // R=strength; G=nonzero group ID /255; optional B=face view Z.
    // With maskDepth, B * depthSign must be finite, positive and match linearDepth.R * depthSign.
    IDirect3DSurface9* destination=nullptr; // Must not alias any input.
    float projectionScaleX=0,projectionScaleY=0; // abs(projection._11/_22).
};
class Renderer {
public:
    Renderer()=default;~Renderer(){Release();}
    Renderer(const Renderer&)=delete;Renderer& operator=(const Renderer&)=delete;
    // Caller invokes inside its rendering scene, before tone mapping and HUD.
    // S_FALSE: off/zero strength, no device or destination modification.
    HRESULT Apply(IDirect3DDevice9*,const Inputs&,const Settings&) noexcept;
    // Required before Reset and before owner device destruction; lazily rebuilds.
    void Release() noexcept;
    unsigned WorkingWidth()const noexcept{return width_;}
    unsigned WorkingHeight()const noexcept{return height_;}
private:
    HRESULT Prepare(IDirect3DDevice9*,unsigned,unsigned,bool) noexcept;
    IDirect3DDevice9* owner_=nullptr;
    IDirect3DPixelShader9* filter_=nullptr,*compose_=nullptr,*direct_=nullptr;
    IDirect3DTexture9* temporary_[3]{}; // horizontal, vertical, identically sampled unblurred reference
    unsigned width_=0,height_=0;
};
}
