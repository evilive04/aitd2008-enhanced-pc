#define NOMINMAX
#include "native_occlusion.h"
#include "gtao_renderer.h"
#include "native_occlusion_shaders.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace native_occlusion {
using Microsoft::WRL::ComPtr;
namespace {
struct Recipe {std::uint64_t hash;UINT bytes,declarations,tail,coeff,specular,diffuse;};
constexpr Recipe recipes[]={
 {0x56e154ecbef2ed2bull,2424,213,597,11,10,9},
 {0x7b3cb41c9a707dbfull,2176,192,535,7,6,5},
 {0xa6d55f08f6f36944ull,2000,192,492,6,~0u,5},
 {0x53da5717f76db999ull,944,112,227,3,2,1},
 {0x68d6abc3cc961e74ull,784,112,188,2,~0u,1},
 {0xefbfaec8102415d6ull,928,112,223,3,2,1},
 {0x278e6a6d7e7c7ec7ull,768,112,184,2,~0u,1}
};
std::uint64_t Hash(const void* data,UINT bytes){auto p=static_cast<const BYTE*>(data);std::uint64_t h=0xcbf29ce484222325ull;for(UINT n=0;n<bytes;++n)h=(h^p[n])*0x100000001b3ull;return h;}
const Recipe* Find(std::uint64_t hash,UINT bytes){for(const auto& r:recipes)if(r.hash==hash&&r.bytes==bytes)return &r;return nullptr;}
template<class T>bool Code(T* shader,std::vector<DWORD>& words){UINT size{};if(!shader||FAILED(shader->GetFunction(nullptr,&size))||size<8||size>65536||size%4)return false;words.resize(size/4);return SUCCEEDED(shader->GetFunction(words.data(),&size))&&size==words.size()*4;}
bool Finite(float v){return std::isfinite(v);}
bool ValidSettings(const Settings& s){return Finite(s.gtaoStrength)&&s.gtaoStrength>=0&&s.gtaoStrength<=1&&Finite(s.contactStrength)&&s.contactStrength>=0&&s.contactStrength<=1&&Finite(s.contactDistance)&&s.contactDistance>0&&s.contactDistance<=.5f&&Finite(s.contactThickness)&&s.contactThickness>0&&s.contactThickness<=.1f&&Finite(s.receiverBias)&&s.receiverBias>0&&s.receiverBias<=s.contactThickness&&s.maxWorkingDimension>=16&&s.maxWorkingDimension<=1920&&s.maxContactLights<=2;}
struct Saved {
 IDirect3DDevice9* d;ComPtr<IDirect3DStateBlock9> all;ComPtr<IDirect3DSurface9> rt[4],ds;
 ComPtr<IDirect3DVertexBuffer9> vb;ComPtr<IDirect3DIndexBuffer9> ib;D3DVIEWPORT9 vp{};UINT offset{},stride{},frequency{},count{};bool valid=false;
 explicit Saved(IDirect3DDevice9* current):d(current){
  D3DCAPS9 caps{};if(FAILED(d->GetDeviceCaps(&caps)))return;count=(std::min)(4u,static_cast<UINT>(caps.NumSimultaneousRTs));
  if(!count||FAILED(d->CreateStateBlock(D3DSBT_ALL,&all))||FAILED(all->Capture()))return;
  for(UINT i=0;i<count;++i){const auto hr=d->GetRenderTarget(i,&rt[i]);if(FAILED(hr)&&!(i&&hr==D3DERR_NOTFOUND))return;}
  const auto depth=d->GetDepthStencilSurface(&ds);if(FAILED(depth)&&depth!=D3DERR_NOTFOUND)return;
  valid=SUCCEEDED(d->GetViewport(&vp))&&SUCCEEDED(d->GetStreamSource(0,&vb,&offset,&stride))&&SUCCEEDED(d->GetStreamSourceFreq(0,&frequency))&&SUCCEEDED(d->GetIndices(&ib));
 }
 bool Restore(){if(!valid)return false;valid=false;bool ok=true;auto check=[&](HRESULT hr){ok=SUCCEEDED(hr)&&ok;};for(UINT i=0;i<3;++i)check(d->SetTexture(i,nullptr));check(d->SetDepthStencilSurface(nullptr));for(UINT i=1;i<count;++i)check(d->SetRenderTarget(i,nullptr));check(d->SetRenderTarget(0,rt[0].Get()));for(UINT i=1;i<count;++i)if(rt[i])check(d->SetRenderTarget(i,rt[i].Get()));check(d->SetDepthStencilSurface(ds.Get()));check(all->Apply());check(d->SetViewport(&vp));check(d->SetStreamSource(0,vb.Get(),offset,stride));check(d->SetStreamSourceFreq(0,frequency));check(d->SetIndices(ib.Get()));return ok;}
 ~Saved(){Restore();}
};
bool Common(IDirect3DDevice9* d,UINT count){bool ok=true;auto check=[&](HRESULT h){ok=SUCCEEDED(h)&&ok;};for(UINT i=0;i<3;++i)check(d->SetTexture(i,nullptr));check(d->SetDepthStencilSurface(nullptr));for(UINT i=1;i<count;++i)check(d->SetRenderTarget(i,nullptr));check(d->SetVertexShader(nullptr));check(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
 const D3DRENDERSTATETYPE states[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_STENCILENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_CLIPPLANEENABLE,D3DRS_FOGENABLE};for(auto s:states)check(d->SetRenderState(s,FALSE));check(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));check(d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));check(d->SetRenderState(D3DRS_COLORWRITEENABLE,15));
 for(UINT i=0;i<3;++i){check(d->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE));check(d->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));check(d->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));check(d->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_POINT));check(d->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_POINT));check(d->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_NONE));}return ok;}
HRESULT Quad(IDirect3DDevice9* d,UINT w,UINT h){D3DVIEWPORT9 vp{0,0,w,h,0,1};auto hr=d->SetViewport(&vp);if(FAILED(hr))return hr;struct V{float x,y,z,rhw,u,v;};const float wf=static_cast<float>(w),hf=static_cast<float>(h);const V v[]={{-.5f,-.5f,0,1,0,0},{wf-.5f,-.5f,0,1,1,0},{-.5f,hf-.5f,0,1,0,1},{wf-.5f,hf-.5f,0,1,1,1}};return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(V));}
bool Texture(IDirect3DDevice9* d,UINT w,UINT h,D3DFORMAT format,ComPtr<IDirect3DTexture9>& t,ComPtr<IDirect3DSurface9>& s){ComPtr<IDirect3DTexture9> made;ComPtr<IDirect3DSurface9> surface;if(FAILED(d->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,format,D3DPOOL_DEFAULT,&made,nullptr))||FAILED(made->GetSurfaceLevel(0,&surface)))return false;t=made;s=surface;return true;}
constexpr D3DSAMPLERSTATETYPE samplerStates[]={D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_ADDRESSW,D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_SRGBTEXTURE};
}
HRESULT BuildDiffusePatch(const DWORD* original,UINT bytes,std::vector<DWORD>& clone,PatchInfo& info) noexcept {
 clone.clear();info={};if(!original||bytes<8||bytes%4)return E_INVALIDARG;if(bytes>65536||original[0]!=0xffff0300u)return S_FALSE;
 const auto* recipe=Find(Hash(original,bytes),bytes);if(!recipe)return S_FALSE;
 try{
  const UINT n=bytes/4;if(recipe->tail+4>=n||recipe->declarations>=recipe->tail||original[n-1]!=0xffffu)return E_INVALIDARG;
  const UINT op=original[recipe->tail]&0xffffu,len=(original[recipe->tail]>>24)&15u;
  if((op!=4&&op!=5)||len!=(op==4?4u:3u)||(original[recipe->tail+1]&0x7ffu)||((original[recipe->tail+1]>>16)&15u)!=7)return E_INVALIDARG;
  clone.reserve(n+16);clone.insert(clone.end(),original,original+recipe->declarations);
  const DWORD declaration[]={0x0200001fu,0x90000000u,0xa00f080fu};clone.insert(clone.end(),std::begin(declaration),std::end(declaration));
  clone.insert(clone.end(),original+recipe->declarations,original+recipe->tail);
  const DWORD extra[]={0x03000042u,0x800f001fu,0x90e40000u,0xa0e4080fu,
   0x04000004u,0x8011001fu,0x8000001fu,0xa00000dfu,0xa05500dfu,
   0x03000005u,0x8007001eu,original[recipe->tail+2],0x8000001fu};
  clone.insert(clone.end(),std::begin(extra),std::end(extra));
  const auto tailStart=clone.size();clone.insert(clone.end(),original+recipe->tail,original+n);clone[tailStart+2]=0x80e4001eu;
  info.positionCoeffRegister=recipe->coeff;info.specularRegister=recipe->specular;return S_OK;
 }catch(...){clone.clear();info={};return E_OUTOFMEMORY;}
}
bool InspectCurrentLight(IDirect3DDevice9* d,std::uint64_t hash,UINT bytes,Light& out) noexcept {
 out={};const auto* r=Find(hash,bytes);if(!d||!r)return false;
 try{
  ComPtr<IDirect3DPixelShader9> ps;ComPtr<IDirect3DVertexShader9> vs;std::vector<DWORD> code;
  if(FAILED(d->GetPixelShader(&ps))||!Code(ps.Get(),code)||code.size()*4!=bytes||Hash(code.data(),bytes)!=hash||FAILED(d->GetVertexShader(&vs))||!Code(vs.Get(),code))return false;
  const UINT size=static_cast<UINT>(code.size()*4);const auto vh=Hash(code.data(),size);if(!((vh==0x9cfe6a213808b66eull&&size==608)||(vh==0xe1410b2b8c357619ull&&size==524))||(r->coeff>=6&&vh!=0x9cfe6a213808b66eull)||(r->coeff<6&&vh!=0xe1410b2b8c357619ull))return false;
  float coefficient[4]{},world[4]{},view[12]{},diffuse[4]{};
  if(FAILED(d->GetPixelShaderConstantF(r->coeff,coefficient,1))||FAILED(d->GetVertexShaderConstantF(4,world,1))||FAILED(d->GetVertexShaderConstantF(0,view,3))||FAILED(d->GetPixelShaderConstantF(r->diffuse,diffuse,1)))return false;
  for(float x:coefficient)if(!Finite(x))return false;for(float x:world)if(!Finite(x))return false;for(float x:view)if(!Finite(x))return false;for(float x:diffuse)if(!Finite(x)||x<0)return false;
  if(coefficient[0]>=-.01f||coefficient[1]>=-.01f||std::abs(coefficient[2])>1e-5f||std::abs(coefficient[3])>1e-5f||std::abs(world[3]-1)>1e-5f)return false;
  const float px=-1/coefficient[0],py=-1/coefficient[1];if(px<.05f||px>100||py<.05f||py>100)return false;
  for(UINT row=0;row<3;++row){float length=0;for(UINT n=0;n<3;++n)length+=view[row*4+n]*view[row*4+n];if(std::abs(length-1)>.02f)return false;for(UINT other=0;other<row;++other){float dot=0;for(UINT n=0;n<3;++n)dot+=view[row*4+n]*view[other*4+n];if(std::abs(dot)>.02f)return false;}}
  ComPtr<IDirect3DBaseTexture9> normal,depth;if(FAILED(d->GetTexture(0,&normal))||!normal||FAILED(d->GetTexture(2,&depth))||!depth||FAILED(normal.As(&out.normals))||FAILED(depth.As(&out.depth)))return false;
  D3DSURFACE_DESC a{},b{};D3DVIEWPORT9 vp{};if(FAILED(out.normals->GetLevelDesc(0,&a))||FAILED(out.depth->GetLevelDesc(0,&b))||FAILED(d->GetViewport(&vp)))return false;
  if(a.Format!=D3DFMT_A8R8G8B8||(b.Format!=D3DFMT_G16R16F&&b.Format!=D3DFMT_G32R32F)||a.Width!=b.Width||a.Height!=b.Height||!a.Width||!a.Height||std::uint64_t(a.Width)*a.Height>8294400ull||vp.X||vp.Y||vp.Width!=a.Width||vp.Height!=a.Height||vp.MinZ!=0||vp.MaxZ!=1)return false;
  out.shaderHash=hash;out.shaderBytes=bytes;out.positionCoeffRegister=r->coeff;out.specularRegister=r->specular;out.originalShader=ps;out.width=a.Width;out.height=a.Height;out.projectionScaleX=px;out.projectionScaleY=py;
  std::copy(world,world+4,out.worldPosition);std::copy(diffuse,diffuse+3,out.diffuseRgb);for(UINT row=0;row<3;++row){float v=0;for(UINT n=0;n<4;++n)v+=view[row*4+n]*world[n];if(!Finite(v))return false;out.viewPosition[row]=v;}return true;
 }catch(...){out={};return false;}
}
struct Binding::State {IDirect3DDevice9* device{};ComPtr<IDirect3DPixelShader9> shader;ComPtr<IDirect3DBaseTexture9> texture;DWORD samplers[std::size(samplerStates)]{};float control[4]{};bool active=false;};
Binding::Binding()=default;Binding::~Binding(){Restore();}
bool Binding::Active()const noexcept{return state_&&state_->active;}
void Binding::Restore()noexcept{if(!state_)return;auto& s=*state_;if(!s.active)return;s.active=false;s.device->SetPixelShader(s.shader.Get());s.device->SetTexture(15,s.texture.Get());for(UINT n=0;n<std::size(samplerStates);++n)s.device->SetSamplerState(15,samplerStates[n],s.samplers[n]);s.device->SetPixelShaderConstantF(223,s.control,1);s.shader.Reset();s.texture.Reset();s.device=nullptr;}
struct Renderer::State {
 struct Program{std::uint64_t hash{};ComPtr<IDirect3DPixelShader9> clone;};std::array<Program,7> programs;
 ComPtr<IDirect3DDevice9> device;ComPtr<IDirect3DTexture9> ao,working,contact,aoDepth,aoNormals;float aoPx{},aoPy{},aoSign{};ComPtr<IDirect3DSurface9> aoSurface,workingSurface,contactSurface;
 ComPtr<IDirect3DPixelShader9> trace,upsample;Settings settings;UINT width{},height{},ww{},wh{},contacts{};std::uint64_t serial{};bool aoReady=false;
};
Renderer::Renderer():state_(std::make_unique<State>()){}Renderer::~Renderer(){Release();}
void Renderer::Release()noexcept{auto& p=*state_;for(auto& s:p.programs)s={};p.aoSurface.Reset();p.workingSurface.Reset();p.contactSurface.Reset();p.ao.Reset();p.working.Reset();p.contact.Reset();p.trace.Reset();p.upsample.Reset();p.aoDepth.Reset();p.aoNormals.Reset();p.device.Reset();p.width=p.height=p.ww=p.wh=p.contacts=0;p.aoReady=false;}
void Renderer::BeginFrame(IDirect3DDevice9* d,std::uint64_t serial,const Settings& settings)noexcept{
 auto& p=*state_;const bool enabled=ValidSettings(settings)&&((settings.gtaoEnabled&&settings.gtaoStrength>0)||(settings.contactsEnabled&&settings.contactStrength>0));if(!enabled||p.device.Get()!=d)Release();p.serial=serial;p.settings=settings;p.aoReady=false;p.aoDepth.Reset();p.aoNormals.Reset();p.contacts=0;if(enabled&&d)p.device=d;
 if(!settings.gtaoEnabled){p.aoSurface.Reset();p.ao.Reset();}if(!settings.contactsEnabled){p.workingSurface.Reset();p.contactSurface.Reset();p.working.Reset();p.contact.Reset();p.trace.Reset();p.upsample.Reset();}
}
HRESULT Renderer::PrepareGtao(IDirect3DDevice9* d,gtao_renderer::Renderer& gtao,const gtao_renderer::Inputs& inputs)noexcept{
 auto& p=*state_;if(p.device.Get()!=d||!p.settings.gtaoEnabled||p.settings.gtaoStrength<=0)return S_FALSE;
 if(p.aoReady)return S_OK;D3DSURFACE_DESC desc{};if(!inputs.linearViewDepth||FAILED(inputs.linearViewDepth->GetLevelDesc(0,&desc)))return E_INVALIDARG;
 if(p.width!=desc.Width||p.height!=desc.Height){p.aoSurface.Reset();p.ao.Reset();p.contactSurface.Reset();p.contact.Reset();p.workingSurface.Reset();p.working.Reset();p.width=desc.Width;p.height=desc.Height;p.ww=p.wh=0;}
 if(!p.ao&&!Texture(d,p.width,p.height,D3DFMT_R16F,p.ao,p.aoSurface))return E_OUTOFMEMORY;
 Saved saved(d);if(!saved.valid)return E_FAIL;const auto hr=gtao.UpsampleVisibility(d,inputs,p.aoSurface.Get());const bool restored=saved.Restore();if(hr==S_OK&&restored){p.aoReady=true;p.aoDepth=inputs.linearViewDepth;p.aoNormals=inputs.viewNormals;p.aoPx=inputs.projectionScaleX;p.aoPy=inputs.projectionScaleY;p.aoSign=inputs.depthSign;return S_OK;}return FAILED(hr)?hr:E_FAIL;
}
HRESULT Renderer::BindLight(IDirect3DDevice9* d,const Light& light,Binding& binding,bool allowContact)noexcept{
 binding.Restore();auto& p=*state_;if(p.device.Get()!=d||!ValidSettings(p.settings)||!light.originalShader||!light.depth||!light.normals)return S_FALSE;
 const auto* r=Find(light.shaderHash,light.shaderBytes);if(!r)return S_FALSE;
 try{
  ComPtr<IDirect3DPixelShader9> current;if(FAILED(d->GetPixelShader(&current))||current.Get()!=light.originalShader.Get())return S_FALSE;
  const bool doContact=allowContact&&p.settings.contactsEnabled&&p.settings.contactStrength>0&&p.contacts<p.settings.maxContactLights&&(std::max)(light.diffuseRgb[0],(std::max)(light.diffuseRgb[1],light.diffuseRgb[2]))>.001f;
  const bool doAo=p.settings.gtaoEnabled&&p.aoReady&&p.settings.gtaoStrength>0&&p.aoDepth.Get()==light.depth.Get()&&p.aoNormals.Get()==light.normals.Get()&&p.width==light.width&&p.height==light.height&&std::abs(p.aoPx-light.projectionScaleX)<1e-4f&&std::abs(p.aoPy-light.projectionScaleY)<1e-4f&&p.aoSign==light.depthSign;if(!doContact&&!doAo)return S_FALSE;
  ComPtr<IDirect3DTexture9> visibility=p.ao;float strength=p.settings.gtaoStrength;
  if(doContact){
   if(p.width!=light.width||p.height!=light.height){p.aoSurface.Reset();p.ao.Reset();p.aoReady=false;p.contactSurface.Reset();p.contact.Reset();p.workingSurface.Reset();p.working.Reset();p.width=light.width;p.height=light.height;p.ww=p.wh=0;}
   const float scale=(std::min)(.5f,static_cast<float>(p.settings.maxWorkingDimension)/(std::max)(p.width,p.height));const UINT w=(std::max)(1u,static_cast<UINT>(std::ceil(p.width*scale))),h=(std::max)(1u,static_cast<UINT>(std::ceil(p.height*scale)));
   if(w!=p.ww||h!=p.wh){p.workingSurface.Reset();p.working.Reset();p.ww=w;p.wh=h;}
   if((!p.working&&!Texture(d,w,h,D3DFMT_A16B16G16R16F,p.working,p.workingSurface))||(!p.contact&&!Texture(d,p.width,p.height,D3DFMT_R16F,p.contact,p.contactSurface)))return E_OUTOFMEMORY;
   if((!p.trace&&FAILED(d->CreatePixelShader(nativeContactProgram,&p.trace)))||(!p.upsample&&FAILED(d->CreatePixelShader(nativeContactUpsampleProgram,&p.upsample))))return E_FAIL;
   Saved saved(d);if(!saved.valid)return E_FAIL;bool ok=Common(d,saved.count);
   const float shape[]={1.f/p.width,1.f/p.height,light.projectionScaleX,light.projectionScaleY};const float position[]={light.viewPosition[0],light.viewPosition[1],light.viewPosition[2]*light.depthSign,light.depthSign};const float contact[]={p.settings.contactDistance,p.settings.contactThickness,p.settings.receiverBias,p.settings.contactStrength};const float control[]={doAo?p.settings.gtaoStrength:0,doAo?1.f:0,static_cast<float>(w),static_cast<float>(h)};
   auto check=[&](HRESULT hr){ok=SUCCEEDED(hr)&&ok;};check(d->SetPixelShaderConstantF(0,shape,1));check(d->SetPixelShaderConstantF(1,position,1));check(d->SetPixelShaderConstantF(2,contact,1));check(d->SetPixelShaderConstantF(3,control,1));check(d->SetRenderTarget(0,p.workingSurface.Get()));check(d->SetTexture(0,light.depth.Get()));check(d->SetTexture(1,light.normals.Get()));check(d->SetTexture(2,doAo?p.ao.Get():nullptr));check(d->SetPixelShader(p.trace.Get()));if(ok)check(Quad(d,w,h));
   check(d->SetTexture(2,nullptr));check(d->SetRenderTarget(0,p.contactSurface.Get()));check(d->SetTexture(2,p.working.Get()));check(d->SetPixelShader(p.upsample.Get()));if(ok)check(Quad(d,p.width,p.height));const bool restored=saved.Restore();if(!ok||!restored)return E_FAIL;visibility=p.contact;strength=1;++p.contacts;
  }
  IDirect3DPixelShader9* patched=nullptr;for(auto& entry:p.programs)if(entry.hash==light.shaderHash){patched=entry.clone.Get();break;}
  if(!patched){std::vector<DWORD> source,clone;PatchInfo info{};if(!Code(light.originalShader.Get(),source))return E_FAIL;const auto hr=BuildDiffusePatch(source.data(),static_cast<UINT>(source.size()*4),clone,info);if(hr!=S_OK)return hr;
   for(auto& entry:p.programs)if(!entry.clone){if(FAILED(d->CreatePixelShader(clone.data(),&entry.clone)))return E_FAIL;entry.hash=light.shaderHash;patched=entry.clone.Get();break;}}
  if(!patched)return E_FAIL;if(!binding.state_)binding.state_=std::make_unique<Binding::State>();auto& b=*binding.state_;b.device=d;b.shader=light.originalShader;if(FAILED(d->GetTexture(15,&b.texture))||FAILED(d->GetPixelShaderConstantF(223,b.control,1)))return E_FAIL;for(UINT n=0;n<std::size(samplerStates);++n)if(FAILED(d->GetSamplerState(15,samplerStates[n],&b.samplers[n])))return E_FAIL;b.active=true;
  const float controls[]={strength,1-strength,0,0};bool ok=SUCCEEDED(d->SetPixelShader(patched))&&SUCCEEDED(d->SetTexture(15,visibility.Get()))&&SUCCEEDED(d->SetPixelShaderConstantF(223,controls,1));
  const DWORD values[]={D3DTADDRESS_CLAMP,D3DTADDRESS_CLAMP,D3DTADDRESS_CLAMP,D3DTEXF_POINT,D3DTEXF_POINT,D3DTEXF_NONE,FALSE};for(UINT n=0;n<std::size(samplerStates);++n)ok=SUCCEEDED(d->SetSamplerState(15,samplerStates[n],values[n]))&&ok;if(!ok){binding.Restore();return E_FAIL;}return S_OK;
 }catch(...){binding.Restore();return E_OUTOFMEMORY;}
}
UINT Renderer::ContactLightsThisFrame()const noexcept{return state_->contacts;}
}
