#pragma once
#include <d3d9.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>
#include <vector>
namespace gtao_renderer {class Renderer;struct Inputs;}
namespace native_occlusion {
struct Settings {
 bool gtaoEnabled=false,contactsEnabled=false;
 float gtaoStrength=.45f,contactStrength=.7f;
 float contactDistance=.12f,contactThickness=.012f,receiverBias=.003f;
 unsigned maxWorkingDimension=1536,maxContactLights=2;
};
struct Light {
 std::uint64_t shaderHash{};UINT shaderBytes{},positionCoeffRegister{},specularRegister=~0u;
 Microsoft::WRL::ComPtr<IDirect3DPixelShader9> originalShader;
 Microsoft::WRL::ComPtr<IDirect3DTexture9> depth,normals;
 UINT width{},height{};float projectionScaleX{},projectionScaleY{},depthSign=-1;
 float viewPosition[3]{},worldPosition[4]{},diffuseRgb[3]{};
};
// Exact seven original native point/spot PS + two original native light VS.
// Full scene matching resources/viewport, negative-forward centered perspective,
// and homogeneous positional lights only. Owns COM references in output.
bool InspectCurrentLight(IDirect3DDevice9*,std::uint64_t hash,UINT bytes,Light&) noexcept;
struct PatchInfo {UINT extraSampler=15,controlRegister=223,positionCoeffRegister{},specularRegister=~0u;};
HRESULT BuildDiffusePatch(const DWORD*,UINT bytes,std::vector<DWORD>&,PatchInfo&) noexcept;
// Restore only touched shader/sampler/constant state AFTER the original draw.
// This preserves native Draw*UP stream/index side effects and stencil updates.
class Binding {
 struct State;std::unique_ptr<State> state_;
 friend class Renderer;
public:
 Binding();~Binding();Binding(const Binding&)=delete;Binding& operator=(const Binding&)=delete;
 void Restore() noexcept;bool Active() const noexcept;
};
class Renderer {
 struct State;std::unique_ptr<State> state_;
public:
 Renderer();~Renderer();Renderer(const Renderer&)=delete;Renderer& operator=(const Renderer&)=delete;
 void BeginFrame(IDirect3DDevice9*,std::uint64_t,const Settings&) noexcept;
 // Call after GTAO Render succeeded; creates one full-scene R16F visibility
 // using the GTAO module's depth/normal-aware reconstruction, once per frame.
 HRESULT PrepareGtao(IDirect3DDevice9*,gtao_renderer::Renderer&,const gtao_renderer::Inputs&) noexcept;
 // Caller holds its draw recursion guard around own rendering. Inspect BEFORE
 // this binding. Then keep it through SSS diffuse replay AND original native
 // light draw. The original PS's direct diffuse alone is multiplied; native
 // specular, alpha, material emission/base, texkill and stencil remain intact.
 HRESULT BindLight(IDirect3DDevice9*,const Light&,Binding&,bool allowContact=true) noexcept;
 void Release() noexcept;
 UINT ContactLightsThisFrame()const noexcept;
};
}
