#pragma once
#include <d3d9.h>
namespace gtao_renderer {
enum class NormalEncoding {DecodedSigned,UnormPacked};
struct Inputs {
 IDirect3DTexture9* linearViewDepth=nullptr; // Signed native linear Z in R.
 IDirect3DTexture9* viewNormals=nullptr; // XYZ in the same native VIEW basis.
 float projectionScaleX=0,projectionScaleY=0,depthSign=-1;
 NormalEncoding normalEncoding=NormalEncoding::UnormPacked;
 bool viewSpaceVerified=false,centeredPerspectiveVerified=false;
};
struct Settings {
 bool enabled=false;float radiusWorld=.35f,maxRadiusPixels=32.f,thicknessCompensation=.2f;
 unsigned maxWorkingDimension=1536; // <= half dimensions, hard maximum1920.
};
class Renderer {
 IDirect3DDevice9* owner_=nullptr;IDirect3DPixelShader9* horizon_=nullptr,*filter_=nullptr,*upsample_=nullptr;IDirect3DTexture9* working_[2]{};unsigned width_=0,height_=0,sourceWidth_=0,sourceHeight_=0;bool valid_=false;float radius_=0;
 HRESULT Prepare(IDirect3DDevice9*,unsigned,unsigned) noexcept;
public:
 Renderer()=default;~Renderer(){Release();}Renderer(const Renderer&)=delete;Renderer& operator=(const Renderer&)=delete;
 // Spatial-only GTAO:3 slice directions,4 horizon samples/side, analytic arc
 // integration, conservative thickness compensation and edge-aware denoising.
 // No temporal GI, hidden geometry, multibounce lighting, or scene composition.
 HRESULT Render(IDirect3DDevice9*,const Inputs&,const Settings&) noexcept;
 // AddRef'd working visibility: R=AO[0,1],G=positive Z,BA=oct normal in +Z basis.
 HRESULT AcquireVisibility(IDirect3DTexture9**) const noexcept;
 // Optional full-resolution VISIBILITY output only; parent supplies destination.
 // For native composition, gtao_upsample.hlsli shares the same4-tap reconstruction.
 HRESULT UpsampleVisibility(IDirect3DDevice9*,const Inputs&,IDirect3DSurface9*) noexcept;
 void Release() noexcept;
 unsigned WorkingWidth() const noexcept{return width_;}unsigned WorkingHeight() const noexcept{return height_;}
};
}
