#define NOMINMAX
#include "ui_art.h"
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace ui_art {
using Microsoft::WRL::ComPtr;
using P=ui_theme::Point;
static constexpr float Pi=3.14159265358979323846f;
static constexpr D3DCOLOR Ink=0xffe9e5dc;
static std::vector<P> Bevel(float x,float y,float w,float h,float c){return {{x+c,y},{x+w-c,y},{x+w,y+c},{x+w,y+h-c},{x+w-c,y+h},{x+c,y+h},{x,y+h-c},{x,y+c}};}
void Scene::Polygon(const std::vector<P>& p,D3DCOLOR color,Material material){
    if(p.size()<3)return;if(batches.empty()||batches.back().material!=material)batches.push_back({material,{}});
    auto& vertices=batches.back().vertices;
    for(size_t i=1;i+1<p.size();++i)for(const auto& v:{p[0],p[i],p[i+1]})vertices.push_back({v.x,v.y,0,1,color,v.x/627.f,v.y/627.f});
}
void Scene::Rect(float x,float y,float w,float h,D3DCOLOR c){Polygon({{x,y},{x+w,y},{x+w,y+h},{x,y+h}},c);}
void Scene::Line(float ax,float ay,float bx,float by,float width,D3DCOLOR color){
    const float dx=bx-ax,dy=by-ay,len=std::hypot(dx,dy);if(len<.0001f)return;const float nx=-dy/len*width*.5f,ny=dx/len*width*.5f;
    Polygon({{ax+nx,ay+ny},{bx+nx,by+ny},{bx-nx,by-ny},{ax-nx,ay-ny}},color);
    // One canonical pixel of outward alpha coverage, on both long edges.
    // Keep the interior width exact; no stretching or opaque edge duplication.
    const float fx=-dy/len*.65f,fy=dx/len*.65f;
    auto& out=batches.back().vertices;
    const auto fringe=[&](P a,P b,P c,P e){
        const Vertex v[]={{a.x,a.y,0,1,color,0,0},{b.x,b.y,0,1,color,0,0},{c.x,c.y,0,1,color&0xffffff,0,0},{e.x,e.y,0,1,color&0xffffff,0,0}};
        for(unsigned i:{0u,1u,2u,0u,2u,3u})out.push_back(v[i]);
    };
    fringe({ax+nx,ay+ny},{bx+nx,by+ny},{bx+nx+fx,by+ny+fy},{ax+nx+fx,ay+ny+fy});
    fringe({bx-nx,by-ny},{ax-nx,ay-ny},{ax-nx-fx,ay-ny-fy},{bx-nx-fx,by-ny-fy});
}
void Scene::Arc(float x,float y,float radius,float a,float b,float width,D3DCOLOR color){
    const unsigned steps=std::max(8u,static_cast<unsigned>(std::ceil(std::fabs(b-a)*radius*.8f)));
    P before{x+std::cos(a)*radius,y+std::sin(a)*radius};
    for(unsigned i=1;i<=steps;++i){const float t=a+(b-a)*i/steps;P next{x+std::cos(t)*radius,y+std::sin(t)*radius};Line(before.x,before.y,next.x,next.y,width,color);before=next;}
}
void Scene::Text(const ui_theme::Label& label,float x,float y,D3DCOLOR color){
    ui_theme::Mesh mesh;ui_theme::Text(mesh,label,x,y,color);
    // An inline-only label has layout objects but no text triangles. Its image
    // is appended by the rich-keycap caller; do not orphan an empty Solid batch
    // before that image, because Draw correctly requires drawable batches.
    if(mesh.empty())return;
    if(batches.empty()||batches.back().material!=Material::Solid)batches.push_back({Material::Solid,{}});
    auto& out=batches.back().vertices;for(const auto& v:mesh)out.push_back({v.x,v.y,v.z,v.rhw,v.color,0,0});
}
void Scene::Frame(float x,float y,float w,float h){
    ui_theme::Mesh mesh;ui_theme::Panel(mesh,x,y,w,h,false);
    if(mesh.empty())return;if(batches.empty()||batches.back().material!=Material::Solid)batches.push_back({Material::Solid,{}});
    auto& out=batches.back().vertices;for(const auto& v:mesh)out.push_back({v.x,v.y,v.z,v.rhw,v.color,0,0});
}
void Scene::Roots(float,float,float,bool){} // Legacy call sites never emit old decoration.
bool Scene::Image(unsigned slot,unsigned sw,unsigned sh,float x,float y,float w,float h){
    return ImageRegion(slot,sw,sh,{0,0,static_cast<float>(sw),static_cast<float>(sh)},x,y,w,h);
}
bool Scene::ImageRegion(unsigned slot,unsigned sw,unsigned sh,const Bounds& source,float x,float y,float w,float h){
    if(slot>=12||!sw||!sh||sw>16384||sh>16384||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||
       !std::isfinite(source.x)||!std::isfinite(source.y)||!std::isfinite(source.w)||!std::isfinite(source.h)||source.x<0||source.y<0||source.w<=0||source.h<=0||source.x+source.w>sw||source.y+source.h>sh)return false;
    const float scale=(std::min)(w/source.w,h/source.h),width=source.w*scale,height=source.h*scale;const float left=x+(w-width)*.5f,top=y+(h-height)*.5f;
    if(!std::isfinite(left+width)||!std::isfinite(top+height))return false;
    const auto material=static_cast<Material>(static_cast<unsigned>(Material::Image0)+slot);if(batches.empty()||batches.back().material!=material)batches.push_back({material,{}});
    const float u0=source.x/sw,v0=source.y/sh,u1=(source.x+source.w)/sw,v1=(source.y+source.h)/sh;
    const Vertex vertices[]={{left,top,0,1,0xffffffff,u0,v0},{left+width,top,0,1,0xffffffff,u1,v0},{left+width,top+height,0,1,0xffffffff,u1,v1},{left,top+height,0,1,0xffffffff,u0,v1}};
    auto& output=batches.back().vertices;for(unsigned index:{0u,1u,2u,0u,2u,3u})output.push_back(vertices[index]);return true;
}
void Scene::Button(float x,float y,float w,float h,bool selected,bool enabled){
    ui_theme::Mesh mesh;ui_theme::Button(mesh,x,y,w,h,selected,enabled);
    if(mesh.empty())return;if(batches.empty()||batches.back().material!=Material::Solid)batches.push_back({Material::Solid,{}});
    auto& out=batches.back().vertices;for(const auto& v:mesh)out.push_back({v.x,v.y,v.z,v.rhw,v.color,0,0});
}
HRESULT Assets::Load(IDirect3DDevice9* d,const std::wstring&){Reset();return d?S_OK:E_INVALIDARG;}
HRESULT Assets::LoadLocal(IDirect3DDevice9* d){Reset();return d?S_OK:E_INVALIDARG;}
void Assets::Reset(){slate_.Reset();roots_.Reset();for(auto& image:images_)image.Reset();for(auto& image:shared_)image.Reset();for(auto& tried:sharedAttempted_)tried=false;}
HRESULT Assets::SetImage(IDirect3DDevice9* device,unsigned slot,IDirect3DTexture9* image){
    if(slot>=5)return E_INVALIDARG;if(!image){images_[slot].Reset();return S_OK;}if(!device)return E_INVALIDARG;
    ComPtr<IDirect3DDevice9> owner;D3DSURFACE_DESC desc{};
    if(FAILED(image->GetDevice(&owner))||owner.Get()!=device||image->GetType()!=D3DRTYPE_TEXTURE||FAILED(image->GetLevelDesc(0,&desc))||!desc.Width||!desc.Height||desc.Width>16384||desc.Height>16384)return E_INVALIDARG;
    images_[slot]=image;return S_OK;
}
IDirect3DTexture9* Assets::Texture(Material m)const{const auto value=static_cast<unsigned>(m),first=static_cast<unsigned>(Material::Image0);if(value>=first&&value<first+5)return images_[value-first].Get();if(value>=first+5&&value<first+12)return shared_[value-first-5].Get();return m==Material::Slate?slate_.Get():m==Material::Roots?roots_.Get():nullptr;}
bool Assets::EnsureShared(IDirect3DDevice9* d,Material material)const{
    const auto index=static_cast<unsigned>(material)-static_cast<unsigned>(Material::MouseLeft);
    if(index>=7)return Texture(material)!=nullptr;
    if(shared_[index]){ComPtr<IDirect3DDevice9> owner;if(SUCCEEDED(shared_[index]->GetDevice(&owner))&&owner.Get()==d)return true;shared_[index].Reset();sharedAttempted_[index]=false;}
    if(sharedAttempted_[index])return false;sharedAttempted_[index]=true;
    constexpr const wchar_t* names[]={L"mouse_left_native.png",L"mouse_right_native.png",L"cursor_metal.png",L"aitd_logo.png",L"episode_cards.png",L"ammo_bullet_native.png",L"ammo_flame_native.png"};
    wchar_t path[MAX_PATH]{};const auto count=GetModuleFileNameW(nullptr,path,MAX_PATH);if(!count||count>=MAX_PATH)return false;
    auto* slash=wcsrchr(path,L'\\');if(!slash)return false;slash[1]=0;if(wcscat_s(path,L"assets\\ui\\")||wcscat_s(path,names[index]))return false;
    const auto init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)return false;
    struct Scope{HRESULT hr;~Scope(){if(SUCCEEDED(hr))CoUninitialize();}}scope{init};
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    auto hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateDecoderFromFilename(path,nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);UINT w{},h{};if(SUCCEEDED(hr))hr=frame->GetSize(&w,&h);
    if(FAILED(hr)||!w||!h||w>4096||h>4096)return false;
    hr=factory->CreateFormatConverter(&converter);if(SUCCEEDED(hr))hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    ComPtr<IDirect3DTexture9> texture;if(SUCCEEDED(hr))hr=d->CreateTexture(w,h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr);
    D3DLOCKED_RECT rect{};if(SUCCEEDED(hr))hr=texture->LockRect(0,&rect,nullptr,0);if(FAILED(hr))return false;
    if(rect.Pitch<static_cast<INT>(w*4)){texture->UnlockRect(0);return false;}
    hr=converter->CopyPixels(nullptr,static_cast<UINT>(rect.Pitch),static_cast<UINT>(rect.Pitch)*h,static_cast<BYTE*>(rect.pBits));const auto unlock=texture->UnlockRect(0);
    if(FAILED(hr)||FAILED(unlock))return false;shared_[index]=texture;return true;
}
bool Draw(IDirect3DDevice9* d,const Scene& scene,const Assets& assets,float scale,float ox,float oy){
    if(!d||scene.batches.empty()||!std::isfinite(scale)||scale<=0||!std::isfinite(ox)||!std::isfinite(oy))return false;
    size_t total=0;for(const auto& b:scene.batches){if(b.vertices.empty()||b.vertices.size()%3)return false;total+=b.vertices.size();if(total>2000000)return false;
        if(b.material!=Material::Solid&&!assets.Texture(b.material)&&!assets.EnsureShared(d,b.material))return false;for(const auto& v:b.vertices)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.u)||!std::isfinite(v.v))return false;}
    ComPtr<IDirect3DStateBlock9> block;ComPtr<IDirect3DVertexBuffer9> stream;ComPtr<IDirect3DIndexBuffer9> indices;ComPtr<IDirect3DVertexDeclaration9> declaration;
    D3DVIEWPORT9 viewport{};RECT clip{};UINT offset{},stride{},frequency{};DWORD fvf{};
    if(FAILED(d->GetViewport(&viewport))||FAILED(d->GetScissorRect(&clip))||FAILED(d->GetStreamSource(0,&stream,&offset,&stride))||FAILED(d->GetStreamSourceFreq(0,&frequency))||FAILED(d->GetIndices(&indices))||FAILED(d->GetVertexDeclaration(&declaration))||FAILED(d->GetFVF(&fvf))||FAILED(d->CreateStateBlock(D3DSBT_ALL,&block))||FAILED(block->Capture()))return false;
    ComPtr<IDirect3DSurface9> target;D3DSURFACE_DESC desc{};if(FAILED(d->GetRenderTarget(0,&target))||FAILED(target->GetDesc(&desc)))return false;
    bool okay=false,began=false;const auto check=[&](HRESULT hr){if(FAILED(hr))okay=false;};
    try{
        okay=SUCCEEDED(d->BeginScene());began=okay;
        if(okay){check(d->SetVertexShader(nullptr));check(d->SetPixelShader(nullptr));check(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1));
            const D3DVIEWPORT9 full{0,0,desc.Width,desc.Height,0,1};check(d->SetViewport(&full));
            const std::pair<D3DRENDERSTATETYPE,DWORD> states[]={{D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_SHADEMODE,D3DSHADE_GOURAUD},{D3DRS_CLIPPLANEENABLE,0},{D3DRS_MULTISAMPLEMASK,0xffffffff},{D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_LIGHTING,FALSE},{D3DRS_FOGENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,TRUE},{D3DRS_SRCBLEND,D3DBLEND_SRCALPHA},{D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA},{D3DRS_BLENDOP,D3DBLENDOP_ADD},{D3DRS_SEPARATEALPHABLENDENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},{D3DRS_COLORWRITEENABLE,15},{D3DRS_SRGBWRITEENABLE,FALSE}};
            for(const auto& s:states)check(d->SetRenderState(s.first,s.second));
            check(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE));check(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE));
            check(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE));check(d->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE));
            check(d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));check(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
            check(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));check(d->SetStreamSourceFreq(0,1));
            check(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));check(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));check(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0));
            for(const auto& b:scene.batches){
                check(d->SetTexture(0,assets.Texture(b.material)));const bool texture=b.material!=Material::Solid;
                check(d->SetTextureStageState(0,D3DTSS_COLOROP,texture?D3DTOP_MODULATE:D3DTOP_SELECTARG2));check(d->SetTextureStageState(0,D3DTSS_ALPHAOP,texture?D3DTOP_MODULATE:D3DTOP_SELECTARG2));
                check(d->SetSamplerState(0,D3DSAMP_ADDRESSU,b.material==Material::Slate?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP));check(d->SetSamplerState(0,D3DSAMP_ADDRESSV,b.material==Material::Slate?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP));
                auto vertices=b.vertices;for(auto& v:vertices){v.x=v.x*scale+ox-.5f;v.y=v.y*scale+oy-.5f;}
                if(okay)check(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(vertices.size()/3),vertices.data(),sizeof(Vertex)));
            }
        }
    }catch(...){okay=false;}
    if(began&&FAILED(d->EndScene()))okay=false;
    if(FAILED(block->Apply())||FAILED(d->SetViewport(&viewport))||FAILED(d->SetScissorRect(&clip))||FAILED(d->SetStreamSource(0,stream.Get(),offset,stride))||FAILED(d->SetStreamSourceFreq(0,frequency))||FAILED(d->SetIndices(indices.Get()))||(fvf&&FAILED(d->SetFVF(fvf)))||FAILED(d->SetVertexDeclaration(declaration.Get())))okay=false;
    if(okay)InterlockedIncrement(&completedScenes);return okay;
}
HRESULT OptionsLabels::Build(const wchar_t* text,const wchar_t* const* values,const wchar_t* backText,const wchar_t* resetText){
    const wchar_t* defaults[]={L"Audio",L"Languages",L"Display",L"Configuration"};
    auto build=[](const wchar_t* value,float size,float width,float height,ui_theme::Label& label){return ui_theme::BuildLabel(value,size,width,height,label,L"Times New Roman");};
    HRESULT hr=build(text,64,1500,90,title);for(unsigned i=0;i<4&&SUCCEEDED(hr);++i)hr=build(values?values[i]:defaults[i],44,530,85,rows[i]);
    if(SUCCEEDED(hr))hr=build(L"Select",30,200,55,select);if(SUCCEEDED(hr))hr=build(resetText,30,370,55,reset);if(SUCCEEDED(hr))hr=build(backText,30,180,55,back);
    if(SUCCEEDED(hr))hr=build(L"Enter",21,70,40,enter);if(SUCCEEDED(hr))hr=build(L"Esc",23,60,40,esc);if(SUCCEEDED(hr))hr=build(L"X",23,40,40,x);return hr;
}
static void Icon(Scene& s,unsigned which,float x,float y){
    const auto line=[&](float a,float b,float c,float d,float width=2.f){s.Line(x+a,y+b,x+c,y+d,width,Ink);};
    switch(which){
    case 0:
        for(auto pair:{std::pair<P,P>{{-23,-8},{-13,-8}},{{-13,-8},{1,-20}},{{1,-20},{1,20}},{{1,20},{-13,8}},{{-13,8},{-23,8}},{{-23,8},{-23,-8}}})line(pair.first.x,pair.first.y,pair.second.x,pair.second.y);
        s.Arc(x+1,y,15,-.9f,.9f,2,Ink);s.Arc(x+1,y,25,-.9f,.9f,2,Ink);break;
    case 1:
        line(-24,-20,18,-20);line(18,-20,18,7);line(18,7,-5,7);line(-5,7,-18,20);line(-18,20,-18,7);line(-18,7,-24,7);line(-24,7,-24,-20);
        for(float i:{-13.f,-3.f,7.f})s.Arc(x+i,y-6,1.9f,0,2*Pi,2,Ink);break;
    case 2:
        line(-24,-20,24,-20);line(24,-20,24,11);line(24,11,-24,11);line(-24,11,-24,-20);line(0,12,0,21);line(-12,21,12,21);break;
    default:
        line(-28,-17,28,-17);line(28,-17,28,17);line(28,17,-28,17);line(-28,17,-28,-17);
        for(int row=0;row<3;++row)for(int col=0;col<7;++col)s.Rect(x-22+col*7.f,y-10+row*7.f,3,2,Ink);line(-14,12,14,12);break;
    }
}
static void Gear(Scene& s,float x,float y){
    const unsigned count=64;std::vector<P> outline;
    for(unsigned n=0;n<count;++n){const float t=n*2*Pi/count,r=(n%8>=2&&n%8<=5)?25.f:19.f;outline.push_back({x+std::cos(t)*r,y+std::sin(t)*r});}
    for(unsigned n=0;n<count;++n)s.Line(outline[n].x,outline[n].y,outline[(n+1)%count].x,outline[(n+1)%count].y,1.6f,Ink);
    s.Arc(x,y,8,0,2*Pi,1.6f,Ink);
}
Bounds OptionRow(unsigned n){return {151,318+132.f*n,720,110};}
void Options(Scene& s,const OptionsLabels& labels,int selected,bool ingame){

    const unsigned count=ingame?3u:4u;s.Frame(100,220,832,ingame?538.f:670.f);
    Gear(s,176,265);s.Text(labels.title,225,225,Ink);
    for(unsigned n=0;n<count;++n){const auto r=OptionRow(n);
        if(selected==static_cast<int>(n)){s.Rect(r.x,r.y,r.w,r.h,0x16e2b66f);s.Line(r.x,r.y,r.x+r.w,r.y,1,0xb9d5d4ca);s.Line(r.x,r.y+r.h,r.x+r.w,r.y+r.h,1,0xb9d5d4ca);s.Line(r.x,r.y,r.x,r.y+r.h,2,0xffe2b66f);}
        else if(n+1<count)s.Line(r.x+6,r.y+r.h,r.x+r.w,r.y+r.h,.7f,0x837c7d73);
        Icon(s,ingame&&n?n+1:n,210,r.y+55);s.Text(labels.rows[n],282,r.y+24,Ink);
        s.Line(844,r.y+46,853,r.y+55,1.4f,Ink);s.Line(853,r.y+55,844,r.y+64,1.4f,Ink);
    }
    const auto key=[&](float x,const ui_theme::Label& label,float w){s.Polygon(Bevel(x,981,w,34,3),0xffa6a89b);s.Polygon(Bevel(x+1,982,w-2,32,2),0xff151a1a);s.Text(label,x+7,984,Ink);};
    key(113,labels.enter,68);s.Text(labels.select,203,978,Ink);
    key(735,labels.x,32);s.Text(labels.reset,790,978,Ink);
    key(1642,labels.esc,51);s.Text(labels.back,1713,978,Ink);
    // Art wraps the outside of each complete frame, with text-safe empty interiors.



}
}
