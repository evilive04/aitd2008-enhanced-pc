#include "sss_settings.h"
#include "sss_capture.h"
#include "sss_diffusion.h"
#include "sss_native_passes.h"
#include "sss_skin_targets.h"
#include "sss_native_recipes.h"
#include "sss_timing.h"
#include "gtao_renderer.h"
#include "native_occlusion.h"
#include <wrl/client.h>
#include <array>
#include <vector>
namespace sss_pipeline {
using Microsoft::WRL::ComPtr;
static bool supported=false,recursive=false,enabled=false,eligible=false,applied=false,invalidOrder=false,maskView=false,waterCaptureRequested=false;
static IDirect3DDevice9* device=nullptr;
static sss_settings::Configuration configuration;
static sss_settings::DecisionLogGate logGate;
static sss_capture::Recorder capture;
static sss_diffusion::Renderer diffusion;
static sss_skin_targets::Targets targets;
static sss_timing::Meter timing,occlusionTiming;
static gtao_renderer::Renderer gtao;
static native_occlusion::Renderer occlusion;
static bool aoEnabled=false,contactsEnabled=false,occlusionWorld=false,lightingFinished=false,gtaoAttempted=false;
static unsigned occludedLights{},occlusionErrors{},gtaoFrames{};
static ULONGLONG occlusionRetryAfter{};
struct OcclusionOperation {bool measured;OcclusionOperation():measured(occlusionTiming.BeginOperation()){}~OcclusionOperation(){if(measured)occlusionTiming.EndOperation();}};
static ComPtr<IDirect3DTexture9> scratch,hdrSource,lightingSource;
static ComPtr<IDirect3DSurface9> scratchSurface;
static ComPtr<IDirect3DPixelShader9> debugShader,copyShader;
static UINT scratchWidth{},scratchHeight{};
static wchar_t configurationPath[MAX_PATH]{};
static ULONGLONG serial{},lastSummary{},lastCandidate{},lastFaceLog{},lastDebugCheck{},lastRejectLog{};
struct Counts {unsigned faces{},accepted{},rejected{},lights{},copied{},compositions{},errors{},orderRejects{};};
static Counts counts;
struct Shader {ComPtr<IDirect3DPixelShader9> object;sss_native_passes::Key key{};};
// 256 four-way sets keep lookup bounded to four COM identities per draw.
static std::array<Shader,1024> shaderCache;static std::array<BYTE,256> nextShader{};
struct Guard {bool previous;Guard():previous(recursive){recursive=true;}~Guard(){recursive=previous;}};
struct Operation {bool measured;Operation():measured(timing.BeginOperation()){}~Operation(){if(measured)timing.EndOperation();}};
static bool World() noexcept {
 __try{
  DWORD pid{};const auto window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);
  if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
  const auto* hero=*reinterpret_cast<const BYTE**>(0xfd61ec);
  return hero&&*reinterpret_cast<const DWORD*>(hero)==0xc834c0&&
   *reinterpret_cast<const DWORD*>(0xff4290)==0&&*reinterpret_cast<const DWORD*>(0xff4390)==0&&
   *reinterpret_cast<const DWORD*>(0xff5508)==35&&!*reinterpret_cast<const BYTE*>(0xff437c)&&
   !*reinterpret_cast<const BYTE*>(0xff4b7f);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void ReleaseOcclusion(){occlusion.Release();gtao.Release();occlusionTiming.Release();lightingSource.Reset();occlusionRetryAfter=0;}
static void OcclusionFailure(const char* stage,HRESULT hr){++occlusionErrors;occlusionRetryAfter=GetTickCount64()+5000;Log("Occlusion suspended for5s stage=%s hr=%08lx frame=%llu; native lighting retained",stage,hr,serial);}
static void ReleaseGpu(){
 targets.Release();diffusion.Release();timing.Release();scratchSurface.Reset();scratch.Reset();debugShader.Reset();copyShader.Reset();hdrSource.Reset();
 for(auto& s:shaderCache)s={};nextShader.fill(0);scratchWidth=scratchHeight=0;
}
static unsigned SampledSamplers(const DWORD* code,UINT bytes) noexcept {
 if(!code||bytes<8||bytes%4)return 0;
 unsigned mask=0;const UINT count=bytes/4;
 for(UINT at=1;at<count;){
  const DWORD token=code[at];const UINT op=token&0xffffu;if(op==0xffffu)return mask;
  const UINT length=op==0xfffeu?((token>>16)&0x7fffu):((token>>24)&15u);
  if(length>=count-at)return 0;
  if(op==66||op==93||op==95)for(UINT n=1;n<=length;++n){const DWORD operand=code[at+n];const UINT type=((operand>>28)&7u)|((operand>>8)&24u),slot=operand&0x7ffu;if((operand&0x80000000u)&&type==10&&slot<16)mask|=1u<<slot;}
  at+=length+1;
 }return 0;
}
static sss_native_passes::Key Program(IDirect3DDevice9* d) noexcept {
 try{
  ComPtr<IDirect3DPixelShader9> ps;if(FAILED(d->GetPixelShader(&ps))||!ps)return {};
  UINT hashPointer=static_cast<UINT>(reinterpret_cast<std::uintptr_t>(ps.Get())>>4);hashPointer^=hashPointer>>16;hashPointer*=0x7feb352du;hashPointer^=hashPointer>>15;
  const UINT bucket=hashPointer&255u;for(UINT way=0;way<4;++way){const auto& item=shaderCache[bucket*4+way];if(item.object.Get()==ps.Get())return item.key;}
  UINT bytes{};if(FAILED(ps->GetFunction(nullptr,&bytes))||bytes<8||bytes>65536||bytes%4)return {};
  std::vector<DWORD> code(bytes/4);if(FAILED(ps->GetFunction(code.data(),&bytes))||bytes!=code.size()*4)return {};
  std::uint64_t hash=0xcbf29ce484222325ull;for(UINT i=0;i<bytes;++i)hash=(hash^reinterpret_cast<const BYTE*>(code.data())[i])*0x100000001b3ull;
  auto& entry=shaderCache[bucket*4+(nextShader[bucket]++&3u)];entry.object=ps;entry.key=sss_native_passes::Lookup(hash,bytes);entry.key.sampledSamplers=SampledSamplers(code.data(),bytes);if(entry.key.role==sss_native_passes::Role::Unknown&&sss_native_recipes::KnownSkinColorSampler(hash,bytes)!=sss_native_recipes::MissingRegister)entry.key.role=sss_native_passes::Role::Face;return entry.key;
 }catch(...){return {};}
}
// ALL state block plus state excluded by D3D9 blocks / invalidated by UP draws.
struct Saved {
 IDirect3DDevice9* d;ComPtr<IDirect3DStateBlock9> all;ComPtr<IDirect3DSurface9> rt[4],ds;
 ComPtr<IDirect3DVertexBuffer9> vb;ComPtr<IDirect3DIndexBuffer9> ib;UINT offset{},stride{},frequency{};D3DVIEWPORT9 vp{};bool valid=false;
 explicit Saved(IDirect3DDevice9* current):d(current){
  if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&all))||FAILED(all->Capture()))return;
  for(UINT i=0;i<4;++i){const auto hr=d->GetRenderTarget(i,&rt[i]);if(FAILED(hr)&&!(i&&hr==D3DERR_NOTFOUND))return;}
  const auto hr=d->GetDepthStencilSurface(&ds);if(FAILED(hr)&&hr!=D3DERR_NOTFOUND)return;
  valid=SUCCEEDED(d->GetViewport(&vp))&&SUCCEEDED(d->GetStreamSource(0,&vb,&offset,&stride))&&SUCCEEDED(d->GetStreamSourceFreq(0,&frequency))&&SUCCEEDED(d->GetIndices(&ib));
 }
 bool Restore(){
  if(!valid)return false;valid=false;bool ok=true;auto check=[&](HRESULT hr){ok=SUCCEEDED(hr)&&ok;};
  for(UINT i=0;i<16;++i)check(d->SetTexture(i,nullptr));for(UINT i=0;i<4;++i){ComPtr<IDirect3DBaseTexture9> vertexTexture;const UINT slot=D3DVERTEXTEXTURESAMPLER0+i;if(SUCCEEDED(d->GetTexture(slot,&vertexTexture))&&vertexTexture)check(d->SetTexture(slot,nullptr));}check(d->SetDepthStencilSurface(nullptr));
  for(UINT i=1;i<4;++i)check(d->SetRenderTarget(i,nullptr));check(d->SetRenderTarget(0,rt[0].Get()));
  for(UINT i=1;i<4;++i)if(rt[i])check(d->SetRenderTarget(i,rt[i].Get()));check(d->SetDepthStencilSurface(ds.Get()));
  check(all->Apply());check(d->SetViewport(&vp));check(d->SetStreamSource(0,vb.Get(),offset,stride));check(d->SetStreamSourceFreq(0,frequency));check(d->SetIndices(ib.Get()));return ok;
 }
 ~Saved(){Restore();}
};
// Material catalog/UV identity and alpha guards live in Targets, not a single
// actor's shader/texture layout. Source bytecode semantics remain exact.
static bool ApprovedFace(IDirect3DDevice9* d,const sss_native_passes::Key& key,BYTE& group){
 if(sss_native_recipes::KnownSkinColorSampler(key.hash,key.bytes)==sss_native_recipes::MissingRegister)return false;
 return targets.ApproveKnownSkin(d,&group);
}
static ComPtr<IDirect3DTexture9> RenderTexture(IDirect3DDevice9* d){
 ComPtr<IDirect3DSurface9> rt;ComPtr<IDirect3DTexture9> texture;
 if(SUCCEEDED(d->GetRenderTarget(0,&rt))&&rt)rt->GetContainer(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(texture.GetAddressOf()));return texture;
}
static bool FloatScene(IDirect3DTexture9* t){D3DSURFACE_DESC desc{};return t&&SUCCEEDED(t->GetLevelDesc(0,&desc))&&desc.Width==targets.Width()&&desc.Height==targets.Height()&&desc.Format==D3DFMT_A16B16G16R16F&&desc.Pool==D3DPOOL_DEFAULT&&(desc.Usage&D3DUSAGE_RENDERTARGET);}
static bool Additive(IDirect3DDevice9* d){
 DWORD blend{},src{},dst{},op{};return SUCCEEDED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend))&&blend&&SUCCEEDED(d->GetRenderState(D3DRS_SRCBLEND,&src))&&src==D3DBLEND_ONE&&SUCCEEDED(d->GetRenderState(D3DRS_DESTBLEND,&dst))&&dst==D3DBLEND_ONE&&SUCCEEDED(d->GetRenderState(D3DRS_BLENDOP,&op))&&op==D3DBLENDOP_ADD;
}
static void RejectSummary(IDirect3DDevice9* d,const char* pass,const sss_native_passes::Key& key){
 const auto now=GetTickCount64();if(lastRejectLog&&now-lastRejectLog<2000)return;lastRejectLog=now;
 DWORD stencil{},writeMask{},fail{},zfail{},success{},alpha{},blend{},zwrite{};
 d->GetRenderState(D3DRS_STENCILENABLE,&stencil);d->GetRenderState(D3DRS_STENCILWRITEMASK,&writeMask);d->GetRenderState(D3DRS_STENCILFAIL,&fail);d->GetRenderState(D3DRS_STENCILZFAIL,&zfail);d->GetRenderState(D3DRS_STENCILPASS,&success);d->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha);d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);d->GetRenderState(D3DRS_ZWRITEENABLE,&zwrite);
 Log("SSS replayRejected pass=%s hash=%016llx bytes=%u status=%u alphaTest=%lu blend=%lu zWrite=%lu stencil=%lu stencilWriteMask=%08lx stencilOps=%lu/%lu/%lu frame=%llu",pass,key.hash,key.bytes,static_cast<unsigned>(targets.LastStatus()),alpha,blend,zwrite,stencil,writeMask,fail,zfail,success,serial);
}
static void FaceSummary(IDirect3DDevice9*,const sss_native_passes::Key& key,BYTE group){
 const auto now=GetTickCount64();if(lastFaceLog&&now-lastFaceLog<10000)return;lastFaceLog=now;
 Log("SSS material accepted frame=%llu shader=%016llx bytes=%u profile=%u scene=%ux%u depth=%p proj=(%.6g,%.6g) sign=%.0f radiusWorld=.007 fullResolution=1",serial,key.hash,key.bytes,group,targets.Width(),targets.Height(),targets.Depth(),targets.ProjectionScaleX(),targets.ProjectionScaleY(),targets.DepthSign());
}
// Own diagnostic HLSL: signed-depth-validated mask => green, background black.
// Samplers s0=mask,s1=sceneDepth; c0=(depthSign,.003,.002,0).
static const DWORD maskDebugProgram[]={
 0xffff0300u,0x0036fffeu,0x42415443u,0x0000001cu,0x000000abu,0xffff0300u,0x00000003u,0x0000001cu,
 0x00008100u,0x000000a4u,0x00000058u,0x00010003u,0x00060001u,0x00000064u,0x00000000u,0x00000074u,
 0x00000003u,0x00020001u,0x0000007cu,0x00000000u,0x0000008cu,0x00000002u,0x00020001u,0x00000094u,
 0x00000000u,0x74706564u,0x70614d68u,0xababab00u,0x000c0004u,0x00010001u,0x00000001u,0x00000000u,
 0x6b73616du,0x0070614du,0x000c0004u,0x00010001u,0x00000001u,0x00000000u,0x6974706fu,0x00736e6fu,
 0x00030001u,0x00040001u,0x00000001u,0x00000000u,0x335f7370u,0x4d00305fu,0x6f726369u,0x74666f73u,
 0x29522820u,0x534c4820u,0x6853204cu,0x72656461u,0x6d6f4320u,0x656c6970u,0x30312072u,0xab00312eu,
 0x05000051u,0xa00f0001u,0x3da3d70au,0x3f4ccccdu,0x3df5c28fu,0x3f800000u,0x05000051u,0xa00f0002u,
 0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x0200001fu,0x80000005u,0x90030000u,0x0200001fu,
 0x90000000u,0xa00f0800u,0x0200001fu,0x90000000u,0xa00f0801u,0x03000042u,0x800f0000u,0x90e40000u,
 0xa0e40801u,0x03000005u,0x80010000u,0x80000000u,0xa0000000u,0x03000005u,0x80020000u,0x8b000000u,
 0xa0aa0000u,0x0300000bu,0x80010001u,0xa0550000u,0x80550000u,0x03000042u,0x800f0002u,0x90e40000u,
 0xa0e40800u,0x04000004u,0x80020000u,0x80aa0002u,0xa0000000u,0x81000000u,0x03000002u,0x80020000u,
 0x8c550000u,0x80000001u,0x02000001u,0x80010001u,0xa0000002u,0x04000058u,0x800f0001u,0x80550000u,
 0xa0e40001u,0x80000001u,0x03000005u,0x80020000u,0x80aa0002u,0xa0000000u,0x04000058u,0x800f0001u,
 0x81550000u,0xa0000002u,0x80e40001u,0x04000058u,0x800f0000u,0x81000000u,0xa0000002u,0x80e40001u,
 0x04000058u,0x800f0000u,0x81550002u,0xa0000002u,0x80e40000u,0x04000058u,0x800f0800u,0x81000002u,
 0xa0000002u,0x80e40000u,0x0000ffffu,
};
static bool DrawMask(IDirect3DDevice9* d){
 if(!debugShader&&FAILED(d->CreatePixelShader(maskDebugProgram,&debugShader)))return false;
 bool ok=true;auto check=[&](HRESULT hr){ok=SUCCEEDED(hr)&&ok;};
 check(d->SetDepthStencilSurface(nullptr));for(UINT i=1;i<4;++i)check(d->SetRenderTarget(i,nullptr));check(d->SetRenderTarget(0,scratchSurface.Get()));
 D3DVIEWPORT9 vp{0,0,targets.Width(),targets.Height(),0,1};check(d->SetViewport(&vp));check(d->SetVertexShader(nullptr));check(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));check(d->SetPixelShader(debugShader.Get()));
 check(d->SetRenderState(D3DRS_ZENABLE,FALSE));check(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE));check(d->SetRenderState(D3DRS_STENCILENABLE,FALSE));check(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));check(d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE));check(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));check(d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE));check(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE));check(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));check(d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));check(d->SetRenderState(D3DRS_COLORWRITEENABLE,15));check(d->SetRenderState(D3DRS_CLIPPLANEENABLE,0));check(d->SetRenderState(D3DRS_FOGENABLE,FALSE));
 for(UINT i=0;i<2;++i){check(d->SetTexture(i,i?targets.Depth():targets.Mask()));check(d->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE));check(d->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));check(d->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));check(d->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_POINT));check(d->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_POINT));check(d->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_NONE));}
 const float options[]={targets.DepthSign(),.003f,.002f,0};check(d->SetPixelShaderConstantF(0,options,1));
 struct V{float x,y,z,rhw,u,v;};const float w=static_cast<float>(targets.Width()),h=static_cast<float>(targets.Height());const V vertices[]={{-.5f,-.5f,0,1,0,0},{w-.5f,-.5f,0,1,1,0},{-.5f,h-.5f,0,1,0,1},{w-.5f,h-.5f,0,1,1,1}};
 return ok&&SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(V)));
}
// Own diffusion changes only an authenticated linear component. Subsequent
// native lighting, shadows, transparency and post processing consume its delta.
static bool WritesRgb(IDirect3DDevice9* d){DWORD mask{};return SUCCEEDED(d->GetRenderState(D3DRS_COLORWRITEENABLE,&mask))&&(mask&7u);}
static void RejectOrder(const char* reason,const sss_native_passes::Key& key){
 if(invalidOrder)return;invalidOrder=true;++counts.orderRejects;
 Log("SSS order rejected frame=%llu reason=%s hash=%016llx bytes=%u",serial,reason,key.hash,key.bytes);
}
static bool CopyToHdr(IDirect3DDevice9* d,IDirect3DSurface9* destination){
 if(!copyShader){const DWORD code[]={0xffff0300u,0x0200001fu,0x80000005u,0x90030000u,
  0x0200001fu,0x90000000u,0xa00f0800u,0x03000042u,0x800f0000u,0x90e40000u,0xa0e40800u,
  0x02000001u,0x800f0800u,0x80e40000u,0x0000ffffu};if(FAILED(d->CreatePixelShader(code,&copyShader)))return false;}
 bool ok=true;auto check=[&](HRESULT hr){ok=SUCCEEDED(hr)&&ok;};
 for(UINT i=0;i<16;++i)check(d->SetTexture(i,nullptr));for(UINT i=0;i<4;++i){ComPtr<IDirect3DBaseTexture9> vertexTexture;const UINT slot=D3DVERTEXTEXTURESAMPLER0+i;if(SUCCEEDED(d->GetTexture(slot,&vertexTexture))&&vertexTexture)check(d->SetTexture(slot,nullptr));}check(d->SetDepthStencilSurface(nullptr));for(UINT i=1;i<4;++i)check(d->SetRenderTarget(i,nullptr));check(d->SetRenderTarget(0,destination));
 D3DVIEWPORT9 vp{0,0,targets.Width(),targets.Height(),0,1};check(d->SetViewport(&vp));check(d->SetVertexShader(nullptr));check(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));check(d->SetPixelShader(copyShader.Get()));
 check(d->SetRenderState(D3DRS_ZENABLE,FALSE));check(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE));check(d->SetRenderState(D3DRS_STENCILENABLE,FALSE));check(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));check(d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE));check(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));check(d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE));check(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE));check(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));check(d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));check(d->SetRenderState(D3DRS_COLORWRITEENABLE,15));check(d->SetRenderState(D3DRS_CLIPPLANEENABLE,0));check(d->SetRenderState(D3DRS_FOGENABLE,FALSE));
 check(d->SetTexture(0,scratch.Get()));check(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE));check(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));check(d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));check(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT));check(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT));check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
 struct V{float x,y,z,rhw,u,v;};const float w=static_cast<float>(targets.Width()),h=static_cast<float>(targets.Height());const V vertices[]={{-.5f,-.5f,0,1,0,0},{w-.5f,-.5f,0,1,1,0},{-.5f,h-.5f,0,1,0,1},{w-.5f,h-.5f,0,1,1,1}};
 return ok&&SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(V)));
}
// Compose before a real consumer, not merely a stale native texture binding.
// This also works in scenes with no particular PCF-shadow permutation.
static bool ConsumesHdr(IDirect3DDevice9* d,const sss_native_passes::Key& key,IDirect3DTexture9* source){
 if(!source||key.role==sss_native_passes::Role::DiffuseLight)return false;
 if(WritesRgb(d)&&RenderTexture(d).Get()==source)return true;
 for(UINT i=0;i<16;++i)if(key.sampledSamplers&(1u<<i)){ComPtr<IDirect3DBaseTexture9> bound;if(SUCCEEDED(d->GetTexture(i,&bound))&&bound.Get()==source)return true;}
 return false;
}
static bool ComposeBoundary(IDirect3DDevice9* d,const sss_native_passes::Key& key){
 if(!targets.Ready()||!hdrSource||applied||invalidOrder||(!maskView&&!targets.CombinedDiffuse()))return false;
 ComPtr<IDirect3DTexture9> scene=hdrSource;
 const bool matching=FloatScene(scene.Get())&&ConsumesHdr(d,key,scene.Get());
 const auto now=GetTickCount64();if(!lastCandidate||now-lastCandidate>=2000){lastCandidate=now;Log("SSS beforeHDRConsumer candidate hash=%016llx bytes=%u HDR=%p tracked=%p same=%d scene=%ux%u mask=%p diffuse=%p depth=%p frame=%llu diagnosticMask=%d",key.hash,key.bytes,scene.Get(),hdrSource.Get(),matching,targets.Width(),targets.Height(),targets.Mask(),targets.CombinedDiffuse(),targets.Depth(),serial,maskView);}
 if(!matching)return false;Guard guard;Operation op;Saved saved(d);if(!saved.valid){++counts.errors;return false;}
 ComPtr<IDirect3DSurface9> hdrSurface;if(FAILED(scene->GetSurfaceLevel(0,&hdrSurface))){++counts.errors;return false;}
 if(!scratch||scratchWidth!=targets.Width()||scratchHeight!=targets.Height()){
  scratchSurface.Reset();scratch.Reset();scratchWidth=scratchHeight=0;
  if(FAILED(d->CreateTexture(targets.Width(),targets.Height(),1,D3DUSAGE_RENDERTARGET,D3DFMT_A16B16G16R16F,D3DPOOL_DEFAULT,&scratch,nullptr))||FAILED(scratch->GetSurfaceLevel(0,&scratchSurface))){scratch.Reset();++counts.errors;return false;}
  scratchWidth=targets.Width();scratchHeight=targets.Height();
 }
 HRESULT hr=S_OK;if(maskView){if(!DrawMask(d))hr=E_FAIL;}else{
  sss_diffusion::Inputs inputs;inputs.sceneLinear=scene.Get();inputs.diffuseLinear=targets.CombinedDiffuse();inputs.linearDepth=targets.Depth();inputs.skinMask=targets.Mask();inputs.destination=scratchSurface.Get();inputs.projectionScaleX=targets.ProjectionScaleX();inputs.projectionScaleY=targets.ProjectionScaleY();
  sss_diffusion::Settings settings;settings.enabled=true;settings.maskDepth=true;settings.depthSign=targets.DepthSign();hr=diffusion.Apply(d,inputs,settings);
 }
 if(hr==S_OK&&!CopyToHdr(d,hdrSurface.Get()))hr=E_FAIL;
 const bool restored=saved.Restore();if(hr!=S_OK||!restored){++counts.errors;RejectOrder("composition_failure",key);Log("SSS compose failed frame=%llu hr=%08lx restored=%d",serial,hr,restored);return false;}
 applied=true;++counts.compositions;return true;
}
static void BeforeLight(IDirect3DDevice9* d,const sss_skin_targets::Draw& draw,const sss_native_passes::Key& key){
 if(key.role!=sss_native_passes::Role::DiffuseLight||!targets.Ready()||invalidOrder||applied)return;
 ++counts.lights;auto destination=RenderTexture(d);if(!FloatScene(destination.Get()))return;
 ComPtr<IDirect3DBaseTexture9> depth;if(FAILED(d->GetTexture(key.depthSampler,&depth))||depth.Get()!=targets.Depth())return;
 if(hdrSource&&destination.Get()!=hdrSource.Get()){RejectOrder("light_destination_changed",key);return;}hdrSource=destination;
 // Native lights destructively DECR stencil on passing pixels. Replay BEFORE
 // original with a read-only snapshot of the SAME stencil test, then restore.
 Guard guard;Operation op;if(targets.ReplayDiffuseLight(d,draw,true,key.depthSampler,key.zeroSpecularRgbRegister,true))++counts.copied;else RejectSummary(d,"light_before_native",key);
}
static void AfterDraw(IDirect3DDevice9* d,const sss_skin_targets::Draw& draw,const sss_native_passes::Key& key){
 if(!enabled||!eligible||recursive||applied||invalidOrder)return;
 if(key.role==sss_native_passes::Role::SceneInitialize&&targets.Ready()){
  ComPtr<IDirect3DBaseTexture9> base,depth;auto destination=RenderTexture(d);
  if(FloatScene(destination.Get())&&WritesRgb(d)&&SUCCEEDED(d->GetTexture(0,&base))&&targets.MatchesBaseRadiance(base.Get())&&SUCCEEDED(d->GetTexture(1,&depth))&&depth.Get()==targets.Depth()){
   if(hdrSource&&hdrSource.Get()!=destination.Get())RejectOrder("base_destination_changed",key);else hdrSource=destination;
  }
 }else if(key.role==sss_native_passes::Role::Face){
  if(!World())return;++counts.faces;
  Guard guard;BYTE group{};if(!ApprovedFace(d,key,group)){++counts.rejected;return;}
  if(hdrSource){RejectOrder("late_skin_material",key);return;}
  Operation op;
  if(targets.Replay(d,draw,true,group,true)){++counts.accepted;FaceSummary(d,key,group);}else {++counts.rejected;RejectSummary(d,"face",key);}
 }else if(hdrSource&&key.role!=sss_native_passes::Role::DiffuseLight&&WritesRgb(d)){
  auto destination=RenderTexture(d);if(destination.Get()==hdrSource.Get()&&!Additive(d))RejectOrder("unhandled_hdr_attenuation",key);
 }
}
static void BeginPresent(IDirect3DDevice9* current){
 const bool finished=capture.Active();capture.EndFrame();if(finished)Log("%s capture completed path=%ls error=%lu",capture.LastWasWater()?"Water":"SSS",capture.LastDirectory(),capture.LastError());
 timing.EndFrame();occlusionTiming.EndFrame();const auto now=GetTickCount64();const auto previous=configuration.Get();const bool had=enabled,hadOcclusion=aoEnabled||contactsEnabled;
 if(lastSummary==0||now-lastSummary>=5000){lastSummary=now;const auto aoStats=occlusionTiming.GetSummary();Log("Occlusion frame=%llu GTAO=%d contacts=%d world=%d prepared=%u patchedLights=%u contactLights=%u errors=%u GPUownMean=%.4f GPUownP95=%.4f GPUstatus=%u",serial,aoEnabled,contactsEnabled,occlusionWorld,gtaoFrames,occludedLights,occlusion.ContactLightsThisFrame(),occlusionErrors,aoStats.gpuMeanMs,aoStats.gpuP95Ms,static_cast<unsigned>(aoStats.gpuStatus));const auto stats=timing.GetSummary();Log("SSS frame=%llu enabled=%d world=%d faces=%u accepted=%u rejected=%u lights=%u copied=%u composed=%u errors=%u orderRejected=%u CPUownRenderPartialMean=%.4f CPUownRenderPartialP95=%.4f GPUownOpsMean=%.4f GPUownOpsP95=%.4f gpuStatus=%u samples=%u/%u pending=%u material=verified_humanoid_skin_catalog",serial,enabled,eligible,counts.faces,counts.accepted,counts.rejected,counts.lights,counts.copied,counts.compositions,counts.errors,counts.orderRejects,stats.cpuMeanMs,stats.cpuP95Ms,stats.gpuMeanMs,stats.gpuP95Ms,static_cast<unsigned>(stats.gpuStatus),stats.cpuSamples,stats.gpuSamples,stats.pendingFrames);}
 const auto oldDecision=sss_settings::Evaluate(previous,supported,applied);if(logGate.Changed(oldDecision,now))Log("SSS requested=%d enabled=%d active=%d capture=%d reason=%s",oldDecision.requested,oldDecision.enabled,oldDecision.active,oldDecision.capture,sss_settings::ReasonName(oldDecision.reason));
 configuration.Refresh();const auto requested=configuration.Get();enabled=supported&&requested.requested&&requested.requestValid;
 aoEnabled=supported&&requested.gtao&&requested.gtaoValid;contactsEnabled=supported&&requested.contacts&&requested.contactsValid;
 if(device!=current){ReleaseGpu();ReleaseOcclusion();}else {if((had&&!enabled)||(hadOcclusion&&!enabled&&!aoEnabled&&!contactsEnabled))ReleaseGpu();if(hadOcclusion&&!aoEnabled&&!contactsEnabled)ReleaseOcclusion();}device=current;
 if(!aoEnabled)gtao.Release();
 if(!lastDebugCheck||now-lastDebugCheck>=1000){lastDebugCheck=now;wchar_t text[32]{};GetPrivateProfileStringW(L"Diagnostics",L"SssMaskView",L"false",text,32,configurationPath);bool value{};maskView=sss_settings::ParseBoolean(text,value)&&value;GetPrivateProfileStringW(L"Diagnostics",L"WaterCapture",L"false",text,32,configurationPath);value=false;waterCaptureRequested=sss_settings::ParseBoolean(text,value)&&value;}
 const bool world=(enabled||aoEnabled||contactsEnabled)&&World();eligible=enabled&&world;occlusionWorld=(aoEnabled||contactsEnabled)&&world;
 lightingFinished=gtaoAttempted=false;occludedLights=occlusionErrors=gtaoFrames=0;
 applied=invalidOrder=false;++serial;counts={};native_occlusion::Settings occlusionSettings;occlusionSettings.gtaoEnabled=aoEnabled;occlusionSettings.contactsEnabled=contactsEnabled;occlusion.BeginFrame(current,serial,occlusionSettings);occlusionTiming.BeginFrame(current,aoEnabled||contactsEnabled);hdrSource.Reset();lightingSource.Reset();targets.BeginFrame(current,serial,enabled);timing.BeginFrame(current,enabled);
 DWORD foregroundPid{};const auto foregroundWindow=GetForegroundWindow();GetWindowThreadProcessId(foregroundWindow,&foregroundPid);
 const bool waterEligible=supported&&waterCaptureRequested&&foregroundPid==GetCurrentProcessId()&&!IsIconic(foregroundWindow);
 const bool wantsCapture=supported&&requested.capture&&requested.captureValid;capture.BeginFrame(current,wantsCapture,wantsCapture&&World(),waterEligible);capture.Suspend(true);
}
static void EndPresent(){capture.Suspend(false);}
static void Invalidate(IDirect3DDevice9* current){if(device==current){capture.EndFrame();ReleaseGpu();ReleaseOcclusion();device=nullptr;enabled=eligible=applied=aoEnabled=contactsEnabled=occlusionWorld=false;}}
static HRESULT Forward(IDirect3DDevice9* d,const sss_skin_targets::Draw& p){
 switch(p.kind){
 case 81:return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT)>(Original(d,81))(d,p.type,p.startVertex,p.primitiveCount);
 case 82:return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT)>(Original(d,82))(d,p.type,p.baseVertexIndex,p.minVertexIndex,p.numVertices,p.startIndex,p.primitiveCount);
 case 83:return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT)>(Original(d,83))(d,p.type,p.primitiveCount,p.vertexData,p.stride);
 case 84:return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT)>(Original(d,84))(d,p.type,p.minVertexIndex,p.numVertices,p.primitiveCount,p.indexData,p.indexFormat,p.vertexData,p.stride);
 default:return D3DERR_INVALIDCALL;
 }
}
static HRESULT Draw(IDirect3DDevice9* d,const sss_skin_targets::Draw& draw,const char* kind){
 if(recursive||(device&&device!=d))return Forward(d,draw);
 capture.BeforeDraw(d,kind,draw.type,draw.primitiveCount,draw.numVertices);
 sss_native_passes::Key key{};native_occlusion::Binding binding;
 const bool skinWork=enabled&&eligible&&!applied&&!invalidOrder;
 const bool occlusionWork=occlusionWorld&&!lightingFinished&&GetTickCount64()>=occlusionRetryAfter;
 if(skinWork||occlusionWork){
  key=Program(d);
  if(occlusionWork&&key.role==sss_native_passes::Role::DiffuseLight&&World()){
   native_occlusion::Light light;
   if(native_occlusion::InspectCurrentLight(d,key.hash,key.bytes,light)){
    if(!lightingSource)lightingSource=RenderTexture(d);
    Guard guard;OcclusionOperation measured;
    if(aoEnabled&&!gtaoAttempted){
     gtaoAttempted=true;gtao_renderer::Inputs input;input.linearViewDepth=light.depth.Get();input.viewNormals=light.normals.Get();input.projectionScaleX=light.projectionScaleX;input.projectionScaleY=light.projectionScaleY;input.depthSign=light.depthSign;input.normalEncoding=gtao_renderer::NormalEncoding::UnormPacked;input.viewSpaceVerified=input.centeredPerspectiveVerified=true;
     gtao_renderer::Settings settings;settings.enabled=true;
     HRESULT h=gtao.Render(d,input,settings);if(h==S_OK)h=occlusion.PrepareGtao(d,gtao,input);if(h==S_OK)++gtaoFrames;else if(FAILED(h))OcclusionFailure("GTAO",h);
    }
    if(GetTickCount64()>=occlusionRetryAfter){const auto h=occlusion.BindLight(d,light,binding);if(h==S_OK)++occludedLights;else if(FAILED(h))OcclusionFailure("light_binding",h);}
   }
  }
  // Visibility binding stays active through both the component replay and the
  // native draw, so SSS never subtracts unoccluded light from an occluded scene.
  if(skinWork&&World()){
   if(key.role==sss_native_passes::Role::DiffuseLight)BeforeLight(d,draw,key);
   else ComposeBoundary(d,key);
  }
  if(key.role==sss_native_passes::Role::Attenuation||ConsumesHdr(d,key,lightingSource.Get()))lightingFinished=true;
 }
 const HRESULT hr=Forward(d,draw); // Native draw exactly once.
 binding.Restore();
 if(SUCCEEDED(hr))AfterDraw(d,draw,key);
 else if(hr==D3DERR_DEVICELOST||hr==D3DERR_DEVICEREMOVED)Invalidate(d);
 else if(enabled&&eligible)RejectOrder("native_draw_failed",key);
 return hr;
}
}
static void ConfigureSubsurfaceScattering(const wchar_t* file,bool supported){sss_pipeline::supported=supported;wcsncpy_s(sss_pipeline::configurationPath,file?file:L"",_TRUNCATE);sss_pipeline::configuration.Configure(file);}
static bool SubsurfaceObservationEnabled(){return sss_pipeline::supported;}
static HRESULT STDMETHODCALLTYPE ObservedSssPrimitive(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,UINT start,UINT count){sss_skin_targets::Draw p;p.kind=81;p.type=type;p.startVertex=start;p.primitiveCount=count;return sss_pipeline::Draw(d,p,"primitive");}
static HRESULT STDMETHODCALLTYPE ObservedSssIndexed(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT vertices,UINT start,UINT count){sss_skin_targets::Draw p;p.kind=82;p.type=type;p.baseVertexIndex=base;p.minVertexIndex=minimum;p.numVertices=vertices;p.startIndex=start;p.primitiveCount=count;return sss_pipeline::Draw(d,p,"indexed");}
static HRESULT STDMETHODCALLTYPE ObservedSssPrimitiveUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,UINT count,const void* data,UINT stride){sss_skin_targets::Draw p;p.kind=83;p.type=type;p.primitiveCount=count;p.vertexData=data;p.stride=stride;return sss_pipeline::Draw(d,p,"primitiveUP");}
static HRESULT STDMETHODCALLTYPE ObservedSssIndexedUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,UINT minimum,UINT vertices,UINT count,const void* indices,D3DFORMAT format,const void* data,UINT stride){sss_skin_targets::Draw p;p.kind=84;p.type=type;p.minVertexIndex=minimum;p.numVertices=vertices;p.primitiveCount=count;p.indexData=indices;p.indexFormat=format;p.vertexData=data;p.stride=stride;return sss_pipeline::Draw(d,p,"indexedUP");}
