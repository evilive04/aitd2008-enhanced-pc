#define NOMINMAX
#include "ui_theme.h"
#include "ui_bounded_lru.h"
#include <wrl/client.h>
#include <array>
#include <map>
#include <tuple>
#include <string>
#include <cmath>
#include <algorithm>
namespace ui_theme {
using Microsoft::WRL::ComPtr;
void Triangle(Mesh& mesh,Point a,Point b,Point c,D3DCOLOR color){for(auto p:{a,b,c})mesh.push_back({p.x,p.y,0,1,color});}
void Rect(Mesh& mesh,float x,float y,float w,float h,D3DCOLOR color){Triangle(mesh,{x,y},{x+w,y},{x+w,y+h},color);Triangle(mesh,{x,y},{x+w,y+h},{x,y+h},color);}
void Panel(Mesh& mesh,float x,float y,float w,float h,bool){
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||w<32||h<24)return;
    // One filled interior and a disjoint feather ring. Overlapping filled plates
    // would multiply alpha and turn the intended glass into an opaque black card.
    constexpr unsigned segments=20;
    constexpr float pi=3.14159265358979323846f;
    const float radius=std::min(26.f,std::min(w,h)*.18f),feather=std::min(24.f,std::min(w,h)*.16f);
    std::vector<Point> edge,outer;edge.reserve(4*(segments+1));outer.reserve(4*(segments+1));
    for(unsigned corner=0;corner<4;++corner){
        const float cx=x+(corner==0||corner==3?radius:w-radius),cy=y+(corner<2?radius:h-radius);
        const float start=pi+corner*pi*.5f;
        for(unsigned n=0;n<=segments;++n){const float angle=start+pi*.5f*n/segments;
            edge.push_back({cx+std::cos(angle)*radius,cy+std::sin(angle)*radius});
            outer.push_back({cx+std::cos(angle)*(radius+feather),cy+std::sin(angle)*(radius+feather)});
        }
    }
    const Point center{x+w*.5f,y+h*.5f};constexpr D3DCOLOR color=0xa8090d10;
    for(size_t n=0;n<edge.size();++n){const size_t j=(n+1)%edge.size();
        Triangle(mesh,center,edge[n],edge[j],color);
        const Vertex v[]={{edge[n].x,edge[n].y,0,1,color},{outer[n].x,outer[n].y,0,1,color&0xffffff},{outer[j].x,outer[j].y,0,1,color&0xffffff},{edge[j].x,edge[j].y,0,1,color}};
        for(unsigned i:{0u,1u,2u,0u,2u,3u})mesh.push_back(v[i]);
    }
}
void Button(Mesh& mesh,float x,float y,float w,float h,bool selected,bool enabled){
    if(w<8||h<8)return;
    const D3DCOLOR rim=selected&&enabled?0xdfe2b66f:enabled?0x9ac4c6c3:0x626f777c;
    const D3DCOLOR fill=selected&&enabled?0x60292217:0x48090d10;
    const float radius=std::min(10.f,std::min(w,h)*.22f),pi=3.14159265359f;
    std::vector<Point> outer,inner;constexpr unsigned segments=12;
    for(unsigned c=0;c<4;++c){const float cx=x+(c==0||c==3?radius:w-radius),cy=y+(c<2?radius:h-radius);
        for(unsigned n=0;n<=segments;++n){const float angle=pi+c*pi*.5f+n*pi*.5f/segments;
            outer.push_back({cx+std::cos(angle)*radius,cy+std::sin(angle)*radius});
            inner.push_back({cx+std::cos(angle)*(radius-1.f),cy+std::sin(angle)*(radius-1.f)});
        }
    }
    const Point center{x+w*.5f,y+h*.5f};
    for(size_t n=0;n<outer.size();++n){const auto j=(n+1)%outer.size();Triangle(mesh,center,inner[n],inner[j],fill);
        Triangle(mesh,inner[n],outer[n],outer[j],rim);Triangle(mesh,inner[n],outer[j],inner[j],rim);
    }
}
HRESULT BuildLabel(const wchar_t* text,float size,float width,float height,Label& output,const wchar_t* family,unsigned triangleBudget){
    if(!text||!family||!std::isfinite(size)||!std::isfinite(width)||!std::isfinite(height)||size<=0||width<=0||height<=0)return E_INVALIDARG;ui_vector_text::Options options;options.family=family;options.size=size;options.width=width;options.height=height;options.tolerance=.065f;options.maxTriangles=triangleBudget;
    struct Key{std::wstring text,family;float size,width,height;unsigned budget;bool operator<(const Key& b)const{return std::tie(text,family,size,width,height,budget)<std::tie(b.text,b.family,b.size,b.width,b.height,b.budget);}};
    static SRWLOCK cacheLock=SRWLOCK_INIT;static ui_cache::BoundedLru<Key,Label> cache(128,131072);
    const Key key{text,family,size,width,height,triangleBudget};
    AcquireSRWLockExclusive(&cacheLock);bool hit=false;try{hit=cache.Get(key,output);}catch(...){ReleaseSRWLockExclusive(&cacheLock);throw;}ReleaseSRWLockExclusive(&cacheLock);if(hit)return S_OK;
    const auto hr=BuildRichLabel(text,options,output);if(FAILED(hr))return hr;
    if(output.mesh.triangles.size()<32768){AcquireSRWLockExclusive(&cacheLock);try{cache.Put(key,output,output.mesh.triangles.size());}catch(...){/* Valid art remains usable if optional caching fails. */}ReleaseSRWLockExclusive(&cacheLock);}
    return hr;

}
HRESULT BuildRichLabel(const wchar_t* text,const ui_vector_text::Options& options,Label& output){
    if(!text)return E_INVALIDARG;
    Label pending;const auto hr=ui_vector_text::Build(text,static_cast<UINT32>(wcslen(text)),options,pending.mesh);if(FAILED(hr))return hr;
    struct Entry{Point a,b;unsigned count{};};std::map<std::array<long long,4>,Entry> edges;
    for(const auto& tri:pending.mesh.triangles){Point points[]={{tri.a.x,tri.a.y},{tri.b.x,tri.b.y},{tri.c.x,tri.c.y}};
        const float area=(points[1].x-points[0].x)*(points[2].y-points[0].y)-(points[1].y-points[0].y)*(points[2].x-points[0].x);if(std::fabs(area)<1e-9f)continue;
        for(unsigned n=0;n<3;++n){auto a=points[n],b=points[(n+1)%3];if(area<0)std::swap(a,b);
            long long ax=llround(a.x*10000),ay=llround(a.y*10000),bx=llround(b.x*10000),by=llround(b.y*10000);
            if(ax>bx||(ax==bx&&ay>by)){std::swap(ax,bx);std::swap(ay,by);}auto& e=edges[{ax,ay,bx,by}];if(!e.count){e.a=a;e.b=b;}++e.count;}
    }
    for(const auto& e:edges)if(e.second.count==1)pending.edges.push_back({e.second.a,e.second.b});
    output=std::move(pending);return S_OK;
}
void Text(Mesh& mesh,const Label& label,float x,float y,D3DCOLOR color){
    for(const auto& t:label.mesh.triangles)Triangle(mesh,{x+t.a.x,y+t.a.y},{x+t.b.x,y+t.b.y},{x+t.c.x,y+t.c.y},color);
    for(const auto& e:label.edges){const Point a{x+e.a.x,y+e.a.y},b{x+e.b.x,y+e.b.y};const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);if(len<1e-6f)continue;
        const Point aa{a.x+dy/len*.65f,a.y-dx/len*.65f},bb{b.x+dy/len*.65f,b.y-dx/len*.65f};Triangle(mesh,a,aa,bb,color&0xffffff);Triangle(mesh,a,bb,b,color); // fringe endpoints corrected below
        auto n=mesh.size();mesh[n-6].color=color;mesh[n-5].color=color&0xffffff;mesh[n-4].color=color&0xffffff;mesh[n-3].color=color;mesh[n-2].color=color&0xffffff;mesh[n-1].color=color;
    }
}
void Cursor(Mesh& mesh,float x,float y){Triangle(mesh,{x,y},{x,y+28},{x+21,y+19},0xff080d0a);Triangle(mesh,{x+2,y+5},{x+2,y+23},{x+16,y+18},0xffeee7cf);}
bool Draw(IDirect3DDevice9* d,const Mesh& mesh,float scale,float ox,float oy){
    if(!d||mesh.empty()||mesh.size()%3||mesh.size()>2000000||!std::isfinite(scale)||scale<=0)return false;
    ComPtr<IDirect3DStateBlock9> block;ComPtr<IDirect3DVertexBuffer9> stream;ComPtr<IDirect3DIndexBuffer9> indices;ComPtr<IDirect3DVertexDeclaration9> decl;
    D3DVIEWPORT9 viewport{};RECT clip{};UINT offset{},stride{},frequency{};DWORD fvf{};
    if(FAILED(d->GetViewport(&viewport))||FAILED(d->GetScissorRect(&clip))||FAILED(d->GetStreamSource(0,&stream,&offset,&stride))||FAILED(d->GetStreamSourceFreq(0,&frequency))||FAILED(d->GetIndices(&indices))||FAILED(d->GetVertexDeclaration(&decl))||FAILED(d->GetFVF(&fvf))||FAILED(d->CreateStateBlock(D3DSBT_ALL,&block))||FAILED(block->Capture()))return false;
    ComPtr<IDirect3DSurface9> target;D3DSURFACE_DESC targetDesc{};
    if(FAILED(d->GetRenderTarget(0,&target))||FAILED(target->GetDesc(&targetDesc)))return false;
    bool scene=false,okay=false;
    try{
        Mesh transformed=mesh;for(auto& v:transformed){v.x=v.x*scale+ox-.5f;v.y=v.y*scale+oy-.5f;}
        okay=SUCCEEDED(d->BeginScene());scene=okay;
        const auto check=[&](HRESULT hr){if(FAILED(hr))okay=false;};
        if(okay){check(d->SetVertexShader(nullptr));check(d->SetPixelShader(nullptr));check(d->SetTexture(0,nullptr));check(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));
            D3DVIEWPORT9 full{0,0,targetDesc.Width,targetDesc.Height,0,1};check(d->SetViewport(&full));
            check(d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));check(d->SetRenderState(D3DRS_CLIPPLANEENABLE,0));
            check(d->SetRenderState(D3DRS_SHADEMODE,D3DSHADE_GOURAUD));check(d->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff));
            check(d->SetRenderState(D3DRS_ZENABLE,FALSE));check(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE));check(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));check(d->SetRenderState(D3DRS_LIGHTING,FALSE));check(d->SetRenderState(D3DRS_FOGENABLE,FALSE));
            check(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));check(d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE));check(d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA));check(d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA));check(d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD));check(d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE));
            check(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE));check(d->SetRenderState(D3DRS_STENCILENABLE,FALSE));check(d->SetRenderState(D3DRS_COLORWRITEENABLE,15));check(d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE));
            check(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));check(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));check(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));check(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));check(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));check(d->SetStreamSourceFreq(0,1));
            if(okay)check(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(transformed.size()/3),transformed.data(),sizeof(Vertex)));
        }
    }catch(...){okay=false;}
    if(scene&&FAILED(d->EndScene()))okay=false;
    if(FAILED(block->Apply())||FAILED(d->SetViewport(&viewport))||FAILED(d->SetScissorRect(&clip))||FAILED(d->SetStreamSource(0,stream.Get(),offset,stride))||FAILED(d->SetStreamSourceFreq(0,frequency))||FAILED(d->SetIndices(indices.Get()))||(fvf&&FAILED(d->SetFVF(fvf)))||FAILED(d->SetVertexDeclaration(decl.Get())))okay=false;
    return okay;
}
}
