#include "sss_diffusion.h"
#include "sss_diffusion_shaders.h"
#include <algorithm>
#include <cmath>
namespace sss_diffusion {
namespace {
template<class T>void Drop(T*& p)noexcept{if(p){p->Release();p=nullptr;}}
struct State {
    IDirect3DDevice9* d;IDirect3DStateBlock9* all=nullptr;IDirect3DSurface9* rt[4]{},*depth=nullptr;D3DVIEWPORT9 viewport{};unsigned count=0;bool captured=false;
    explicit State(IDirect3DDevice9* device):d(device){}
    HRESULT Capture(unsigned targets)noexcept{
        count=(std::min)(targets,4u);HRESULT hr=d->CreateStateBlock(D3DSBT_ALL,&all);if(FAILED(hr))return hr;
        hr=all->Capture();if(FAILED(hr))return hr;
        for(unsigned i=0;i<count;++i){hr=d->GetRenderTarget(i,rt+i);if(FAILED(hr)&&!(i&&hr==D3DERR_NOTFOUND))return hr;}
        hr=d->GetDepthStencilSurface(&depth);if(FAILED(hr)&&hr!=D3DERR_NOTFOUND)return hr;
        hr=d->GetViewport(&viewport);if(FAILED(hr))return hr;captured=true;return S_OK;
    }
    HRESULT Restore()noexcept{
        if(!captured)return S_OK;captured=false;HRESULT result=S_OK;
        auto check=[&](HRESULT hr){if(FAILED(hr)&&SUCCEEDED(result))result=hr;};
        for(unsigned i=0;i<7;++i)check(d->SetTexture(i,nullptr));
        check(d->SetDepthStencilSurface(nullptr));for(unsigned i=1;i<count;++i)check(d->SetRenderTarget(i,nullptr));
        check(d->SetRenderTarget(0,rt[0]));for(unsigned i=1;i<count;++i)if(rt[i])check(d->SetRenderTarget(i,rt[i]));
        check(d->SetDepthStencilSurface(depth));check(all->Apply());check(d->SetViewport(&viewport));return result;
    }
    ~State(){Restore();Drop(all);for(auto*& p:rt)Drop(p);Drop(depth);}
};
bool Finite(float x)noexcept{return std::isfinite(x);}
HRESULT SetCommon(IDirect3DDevice9* d)noexcept{
    HRESULT result=S_OK;auto set=[&](HRESULT hr){if(FAILED(hr)&&SUCCEEDED(result))result=hr;};
    set(d->SetVertexShader(nullptr));set(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
    set(d->SetRenderState(D3DRS_ZENABLE,FALSE));set(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE));set(d->SetRenderState(D3DRS_STENCILENABLE,FALSE));
    set(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));set(d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE));set(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));
    set(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));set(d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));set(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE));
    set(d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE));set(d->SetRenderState(D3DRS_COLORWRITEENABLE,15));set(d->SetRenderState(D3DRS_COLORWRITEENABLE1,15));set(d->SetRenderState(D3DRS_FOGENABLE,FALSE));
    set(d->SetRenderState(D3DRS_CLIPPLANEENABLE,0));set(d->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,FALSE));
    for(unsigned i=0;i<7;++i){
        set(d->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_POINT));set(d->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_POINT));set(d->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
        set(d->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));set(d->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));set(d->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE));
    }
    return result;
}
HRESULT Quad(IDirect3DDevice9* d,unsigned w,unsigned h)noexcept{
    D3DVIEWPORT9 v{0,0,w,h,0,1};HRESULT hr=d->SetViewport(&v);if(FAILED(hr))return hr;
    struct V{float x,y,z,rhw,u,v;};const V vertices[]={{-.5f,-.5f,0,1,0,0},{w-.5f,-.5f,0,1,1,0},{-.5f,h-.5f,0,1,0,1},{w-.5f,h-.5f,0,1,1,1}};
    return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(V));
}
}
void Renderer::Release()noexcept{Drop(filter_);Drop(compose_);Drop(direct_);for(auto*& p:temporary_)Drop(p);owner_=nullptr;width_=height_=0;}
HRESULT Renderer::Prepare(IDirect3DDevice9* d,unsigned w,unsigned h,bool full)noexcept{
    if(owner_==d&&width_==w&&height_==h&&(full?direct_!=nullptr:filter_&&compose_&&temporary_[0]&&temporary_[1]&&temporary_[2]))return S_OK;
    Release();D3DCAPS9 caps{};HRESULT hr=d->GetDeviceCaps(&caps);if(FAILED(hr))return hr;
    if(caps.PixelShaderVersion<D3DPS_VERSION(3,0)||(!full&&caps.NumSimultaneousRTs<2)||w>caps.MaxTextureWidth||h>caps.MaxTextureHeight)return D3DERR_NOTAVAILABLE;
    if(full){hr=d->CreatePixelShader(sssDirectShader,&direct_);if(FAILED(hr)){Release();return hr;}owner_=d;width_=w;height_=h;return S_OK;}
    D3DDEVICE_CREATION_PARAMETERS cp{};hr=d->GetCreationParameters(&cp);if(FAILED(hr))return hr;
    IDirect3D9* api=nullptr;hr=d->GetDirect3D(&api);if(FAILED(hr))return hr;
    D3DDISPLAYMODE mode{};hr=api->GetAdapterDisplayMode(cp.AdapterOrdinal,&mode);
    if(SUCCEEDED(hr))hr=api->CheckDeviceFormat(cp.AdapterOrdinal,cp.DeviceType,mode.Format,D3DUSAGE_RENDERTARGET|D3DUSAGE_QUERY_FILTER,D3DRTYPE_TEXTURE,D3DFMT_A16B16G16R16F);
    api->Release();if(FAILED(hr))return hr;
    hr=d->CreatePixelShader(sssFilterShader,&filter_);if(SUCCEEDED(hr))hr=d->CreatePixelShader(sssComposeShader,&compose_);
    for(unsigned i=0;i<3&&SUCCEEDED(hr);++i)hr=d->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,D3DFMT_A16B16G16R16F,D3DPOOL_DEFAULT,temporary_+i,nullptr);
    if(FAILED(hr)){Release();return hr;}owner_=d;width_=w;height_=h;return S_OK;
}
HRESULT Renderer::Apply(IDirect3DDevice9* d,const Inputs& in,const Settings& s)noexcept{
    if(!s.enabled)return S_FALSE;
    if(!Finite(s.radiusWorld)||!Finite(s.amount)||!Finite(s.depthAbsolute)||!Finite(s.depthRelative)||!Finite(s.maxRadiusPixels)||
       s.radiusWorld<0||s.amount<0||s.amount>1||s.depthAbsolute<0||s.depthRelative<0||s.maxRadiusPixels<=0||s.maxRadiusPixels>64||
       s.maxWorkingDimension<16||s.maxWorkingDimension>2048||(s.depthSign!=1.f&&s.depthSign!=-1.f))return E_INVALIDARG;
    if(s.radiusWorld==0||s.amount==0)return S_FALSE;
    if(!d||!in.sceneLinear||!in.diffuseLinear||!in.linearDepth||!in.skinMask||!in.destination||
       !Finite(in.projectionScaleX)||!Finite(in.projectionScaleY)||in.projectionScaleX<=0||in.projectionScaleY<=0)return E_INVALIDARG;
    if(!Finite(s.radiusWorld*in.projectionScaleX*.5f)||!Finite(s.radiusWorld*in.projectionScaleY*.5f))return E_INVALIDARG;
    D3DSURFACE_DESC desc{},destination{};HRESULT hr=in.sceneLinear->GetLevelDesc(0,&desc);if(FAILED(hr))return hr;
    if(!desc.Width||!desc.Height)return E_INVALIDARG;
    IDirect3DTexture9* inputs[]={in.sceneLinear,in.diffuseLinear,in.linearDepth,in.skinMask};
    for(auto* texture:inputs){
        D3DSURFACE_DESC shape{};hr=texture->GetLevelDesc(0,&shape);if(FAILED(hr))return hr;
        if(shape.Width!=desc.Width||shape.Height!=desc.Height)return E_INVALIDARG;
        IDirect3DSurface9* surface=nullptr;hr=texture->GetSurfaceLevel(0,&surface);if(FAILED(hr))return hr;
        bool aliases=surface==in.destination;surface->Release();if(aliases)return E_INVALIDARG;
        IDirect3DDevice9* owner=nullptr;hr=texture->GetDevice(&owner);if(FAILED(hr))return hr;bool same=owner==d;owner->Release();if(!same)return E_INVALIDARG;
    }
    hr=in.destination->GetDesc(&destination);if(FAILED(hr))return hr;
    IDirect3DDevice9* targetOwner=nullptr;hr=in.destination->GetDevice(&targetOwner);if(FAILED(hr))return hr;
    const bool targetMatches=targetOwner==d;targetOwner->Release();if(!targetMatches)return E_INVALIDARG;
    if(destination.Width!=desc.Width||destination.Height!=desc.Height||!(destination.Usage&D3DUSAGE_RENDERTARGET)||destination.MultiSampleType!=D3DMULTISAMPLE_NONE)return E_INVALIDARG;
    const float scale=s.fullResolution?1.f:(std::min)(1.f,static_cast<float>(s.maxWorkingDimension)/(std::max)(desc.Width,desc.Height));
    const unsigned w=(std::max)(1u,static_cast<unsigned>(std::ceil(desc.Width*scale))),h=(std::max)(1u,static_cast<unsigned>(std::ceil(desc.Height*scale)));
    hr=Prepare(d,w,h,s.fullResolution);if(FAILED(hr))return hr;
    D3DCAPS9 caps{};hr=d->GetDeviceCaps(&caps);if(FAILED(hr))return hr;
    State state(d);hr=state.Capture(caps.NumSimultaneousRTs);if(FAILED(hr))return hr;
    auto set=[&](HRESULT result){if(FAILED(result)&&SUCCEEDED(hr))hr=result;};
    set(d->SetDepthStencilSurface(nullptr));for(unsigned i=1;i<(std::min)(static_cast<unsigned>(caps.NumSimultaneousRTs),4u);++i)set(d->SetRenderTarget(i,nullptr));
    set(SetCommon(d));
    const float radius[]={s.radiusWorld*in.projectionScaleX*.5f,s.radiusWorld*in.projectionScaleY*.5f,s.depthAbsolute,s.depthRelative};
    const float controls[]={s.maxRadiusPixels,s.amount,0,0};const float dims[]={1.f/desc.Width,1.f/desc.Height,static_cast<float>(w),static_cast<float>(h)};
    set(d->SetPixelShaderConstantF(1,radius,1));set(d->SetPixelShaderConstantF(2,controls,1));set(d->SetPixelShaderConstantF(3,dims,1));
    const float depthOptions[]={s.depthSign,s.maskDepth?1.f:0.f,0,0};set(d->SetPixelShaderConstantF(5,depthOptions,1));
    if(s.fullResolution){
        const float step[]={1.f/w,1.f/h,0,0};
        const float sourceSize[]={1.f/w,1.f/h,static_cast<float>(w),static_cast<float>(h)};
        set(d->SetPixelShaderConstantF(0,step,1));set(d->SetPixelShaderConstantF(4,sourceSize,1));
        set(d->SetRenderTarget(0,in.destination));set(d->SetPixelShader(direct_));
        set(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));set(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
        set(d->SetTexture(0,in.diffuseLinear));set(d->SetTexture(1,in.linearDepth));set(d->SetTexture(2,in.skinMask));
        set(d->SetTexture(3,in.sceneLinear));set(d->SetTexture(5,in.diffuseLinear));
        if(SUCCEEDED(hr))set(Quad(d,w,h));
        const HRESULT restored=state.Restore();return FAILED(hr)?hr:restored;
    }
    for(unsigned pass=0;pass<2&&SUCCEEDED(hr);++pass){
        if(pass)set(d->SetRenderTarget(1,nullptr));
        IDirect3DSurface9* target=nullptr;set(temporary_[pass]->GetSurfaceLevel(0,&target));
        if(SUCCEEDED(hr))set(d->SetRenderTarget(0,target));Drop(target);
        if(!pass){set(temporary_[2]->GetSurfaceLevel(0,&target));if(SUCCEEDED(hr))set(d->SetRenderTarget(1,target));Drop(target);}
        const float step[]={1.f/w,1.f/h,pass?0.f:1.f,pass?1.f:0.f};set(d->SetPixelShaderConstantF(0,step,1));set(d->SetPixelShader(filter_));
        const float sourceSize[]={pass?1.f/w:1.f/desc.Width,pass?1.f/h:1.f/desc.Height,static_cast<float>(pass?w:desc.Width),static_cast<float>(pass?h:desc.Height)};
        set(d->SetPixelShaderConstantF(4,sourceSize,1));set(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));set(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
        set(d->SetTexture(0,pass?temporary_[0]:in.diffuseLinear));set(d->SetTexture(1,in.linearDepth));set(d->SetTexture(2,in.skinMask));
        set(d->SetTexture(5,pass?temporary_[0]:in.diffuseLinear));
        if(SUCCEEDED(hr))set(Quad(d,w,h));
    }
    if(SUCCEEDED(hr)){
        set(d->SetRenderTarget(0,in.destination));
        set(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT));set(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT));
        set(d->SetTexture(0,in.sceneLinear));set(d->SetTexture(1,in.diffuseLinear));set(d->SetTexture(2,temporary_[1]));set(d->SetTexture(3,in.linearDepth));set(d->SetTexture(4,in.skinMask));set(d->SetPixelShader(compose_));
        set(d->SetTexture(6,temporary_[2]));
        if(SUCCEEDED(hr))set(Quad(d,desc.Width,desc.Height));
    }
    const HRESULT restored=state.Restore();return FAILED(hr)?hr:restored;
}
}
