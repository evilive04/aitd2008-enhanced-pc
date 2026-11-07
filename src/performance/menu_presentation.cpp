#define NOMINMAX
#include "ui_pointer.h"
#include "menu_presentation.h"
#include "ui_art.h"
#include "menu_presentation_policy.h"
#include "ui_sound.h"
#include "menu_video.h"
#include "ui_vector_text.h"
#include "ui_theme.h"
#include "menu_nv12_color.h"
#include "menu_nv12_shader.h"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <system_error>
namespace menu_presentation {
using Microsoft::WRL::ComPtr;using Point=menu_presentation_policy::Point;
struct Vertex{float x,y,z,rhw;D3DCOLOR color;float u,v;};
struct Edge{Point a,b;};
struct Label{ui_vector_text::Mesh mesh;std::vector<Edge> edges;float y{};};
// Extract only exposed contour edges from the tessellator. Adjacent filled
// triangles share edges; a one-pixel alpha fringe follows each real glyph
// contour, including holes. It does not blur a bitmap or stretch any asset.
static void Outline(Label& pending){
    using Key=std::array<long long,4>;struct Entry{Edge edge;unsigned count;};std::map<Key,Entry> edges;
    for(const auto& t:pending.mesh.triangles){
        const Point points[]={{t.a.x,t.a.y},{t.b.x,t.b.y},{t.c.x,t.c.y}};
        const float area=(points[1].x-points[0].x)*(points[2].y-points[0].y)-(points[1].y-points[0].y)*(points[2].x-points[0].x);
        if(std::fabs(area)<1e-9f)continue;
        for(unsigned i=0;i<3;++i){Point a=points[i],b=points[(i+1)%3];if(area<0)std::swap(a,b);
            long long ax=llround(a.x*10000),ay=llround(a.y*10000),bx=llround(b.x*10000),by=llround(b.y*10000);
            if(ax>bx||(ax==bx&&ay>by)){std::swap(ax,bx);std::swap(ay,by);}
            auto& entry=edges[{ax,ay,bx,by}];if(!entry.count)entry.edge={a,b};++entry.count;
        }
    }
    for(const auto& pair:edges)if(pair.second.count==1)pending.edges.push_back(pair.second.edge);
}
static HRESULT BuildLabel(const wchar_t* text,float size,Label& out){
    ui_vector_text::Options options;options.family=ui_typeface::Body;options.size=size;options.width=1150;options.height=1000;options.tolerance=.065f;options.maxTriangles=32768;
    Label pending;HRESULT hr=ui_vector_text::Build(text,static_cast<std::uint32_t>(wcslen(text)),options,pending.mesh);if(FAILED(hr))return hr;
    Outline(pending);out=std::move(pending);return S_OK;
}
static void Triangle(std::vector<Vertex>& v,Point a,Point b,Point c,D3DCOLOR color){
    v.push_back({a.x-.5f,a.y-.5f,0,1,color,0,0});v.push_back({b.x-.5f,b.y-.5f,0,1,color,0,0});v.push_back({c.x-.5f,c.y-.5f,0,1,color,0,0});
}
static void Quad(std::vector<Vertex>& v,float x,float y,float w,float h,D3DCOLOR color){Triangle(v,{x,y},{x+w,y},{x+w,y+h},color);Triangle(v,{x,y},{x+w,y+h},{x,y+h},color);}
template<class Transform>static void Text(std::vector<Vertex>& v,const Label& text,const Transform& transform,D3DCOLOR color){
    for(const auto& t:text.mesh.triangles)Triangle(v,transform({t.a.x,t.a.y}),transform({t.b.x,t.b.y}),transform({t.c.x,t.c.y}),color);
    for(const auto& e:text.edges){const auto a=transform(e.a),b=transform(e.b);const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);if(len<.001f)continue;
        const Point aa{a.x+dy/len*.85f,a.y-dx/len*.85f},bb{b.x+dy/len*.85f,b.y-dx/len*.85f};const auto clear=color&0xffffffu;
        v.push_back({a.x-.5f,a.y-.5f,0,1,color,0,0});v.push_back({aa.x-.5f,aa.y-.5f,0,1,clear,0,0});v.push_back({bb.x-.5f,bb.y-.5f,0,1,clear,0,0});
        v.push_back({a.x-.5f,a.y-.5f,0,1,color,0,0});v.push_back({bb.x-.5f,bb.y-.5f,0,1,clear,0,0});v.push_back({b.x-.5f,b.y-.5f,0,1,color,0,0});
    }
}
struct SavedState {
    IDirect3DDevice9* d;ComPtr<IDirect3DStateBlock9> block;ComPtr<IDirect3DVertexBuffer9> stream;ComPtr<IDirect3DIndexBuffer9> indices;ComPtr<IDirect3DVertexDeclaration9> declaration;
    D3DVIEWPORT9 viewport{};RECT scissor{};UINT offset{},stride{},frequency{};DWORD fvf{};bool captured{},scene{};
    explicit SavedState(IDirect3DDevice9* device):d(device){}
    bool Capture(){
        if(FAILED(d->GetViewport(&viewport))||FAILED(d->GetScissorRect(&scissor))||FAILED(d->GetStreamSource(0,&stream,&offset,&stride))||FAILED(d->GetStreamSourceFreq(0,&frequency))||FAILED(d->GetIndices(&indices))||FAILED(d->GetVertexDeclaration(&declaration))||FAILED(d->GetFVF(&fvf))||FAILED(d->CreateStateBlock(D3DSBT_ALL,&block))||FAILED(block->Capture()))return false;
        captured=true;return true;
    }
    bool Restore()noexcept{
        bool good=true;if(scene){good=SUCCEEDED(d->EndScene());scene=false;}
        if(captured){
            if(FAILED(block->Apply()))good=false;
            if(FAILED(d->SetViewport(&viewport))||FAILED(d->SetScissorRect(&scissor))||FAILED(d->SetStreamSource(0,stream.Get(),offset,stride))||FAILED(d->SetStreamSourceFreq(0,frequency))||FAILED(d->SetIndices(indices.Get()))||FAILED(d->SetVertexDeclaration(declaration.Get()))||(fvf&&FAILED(d->SetFVF(fvf))))good=false;
            captured=false;
        }
        return good;
    }
    ~SavedState(){Restore();}
};
struct Presentation::Impl {
    ui_art::Assets logoAssets;menu_video::Player player;menu_video::Frame frame;ComPtr<IDirect3DTexture9> texture,chroma;ComPtr<IDirect3DPixelShader9> videoShader;IDirect3DDevice9* owner{};
    Label labels[6],title,subtitle,edition,beginKey,beginText,creditsTitle,backKey,backText;std::vector<Label> credits;std::vector<Vertex> vertices;
    bool started{},labelsReady{},wasActive{},wasCredits{},ready{},dirty{};DWORD lastDraw{},creditsTick{};HRESULT error{S_OK};
    unsigned selected{1},downItem{~0u};POINT previousMouse{};bool haveMouse{};menu_presentation_policy::KeyEdges keyEdges;
    menu_presentation_policy::TitleReveal reveal;
    Diagnostic diagnostic{};
    void Failure(HRESULT hr,DWORD kind,DWORD code,const char* detail)noexcept{
        error=hr;diagnostic.error=hr;diagnostic.exceptionKind=kind;diagnostic.exceptionCode=code;
        if(detail)strncpy_s(diagnostic.detail,detail,_TRUNCATE);
    }
    static unsigned Keys(){const int keys[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_UP,VK_DOWN,'W','S',VK_SPACE};unsigned mask=0;for(unsigned i=0;i<8;++i)if(GetAsyncKeyState(keys[i])&0x8000)mask|=1u<<i;return mask;}
    static bool Mouse(const menu_presentation_policy::Canvas& canvas,POINT& point){
        HWND window=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(window,&pid);RECT client{};
        if(pid!=GetCurrentProcessId()||IsIconic(window)||!ui_pointer::ReadClient(window,client,point))return false;
        if(point.x<0||point.y<0||point.x>=client.right||point.y>=client.bottom)return false;
        point.x=static_cast<LONG>(std::lround(point.x*(canvas.width+2*canvas.x)/client.right));point.y=static_cast<LONG>(std::lround(point.y*(canvas.height+2*canvas.y)/client.bottom));return true;
    }
    HRESULT Labels(){
        if(labelsReady)return S_OK;
        const wchar_t* names[]={L"Continue",L"New Game",L"Episodes",L"Options",L"Credits",L"Quit"};
        for(unsigned i=0;i<6;++i){const auto hr=BuildLabel(names[i],44,labels[i]);if(FAILED(hr))return hr;}
        const struct{const wchar_t* text;float size;Label* label;} extra[]={{L"ALONE IN",72,&title},{L"THE DARK",72,&subtitle},{L"ENHANCED PC EDITION",21,&edition},{L"Enter",22,&beginKey},{L"PRESS ANY KEY",22,&beginText},{L"CREDITS",42,&creditsTitle},{L"Esc",22,&backKey},{L"Back",24,&backText}};
        for(const auto& item:extra){const auto hr=BuildLabel(item.text,item.size,*item.label);if(FAILED(hr))return hr;}
        labelsReady=true;return S_OK;
    }
    HRESULT Upload(IDirect3DDevice9* d){
        diagnostic.stage=200;const bool changed=player.CopyLatest(frame);dirty=dirty||changed;
        if(FAILED(player.Error())){const auto failure=player.Error();player.Stop();if(!frame.serial)return failure;}
        if(!frame.serial)return S_FALSE;
        if(owner!=d){texture.Reset();chroma.Reset();videoShader.Reset();owner=d;}
        if(frame.pixels.size()!=static_cast<std::size_t>(frame.width)*frame.height*3/2)return E_INVALIDARG;
        float colors[8]{};if(!menu_nv12_color::Constants(frame.matrix,frame.nominalRange,colors))return E_NOTIMPL;
        float offset[4]{};if(!menu_nv12_color::ChromaOffset(frame.chromaSiting,frame.width,frame.height,offset)||frame.transfer==15||frame.transfer==16)return E_NOTIMPL;
        diagnostic.stage=201;if(texture){D3DSURFACE_DESC existing{};const auto hr=texture->GetLevelDesc(0,&existing);if(FAILED(hr))return hr;if(existing.Width!=frame.width||existing.Height!=frame.height){texture.Reset();chroma.Reset();}}
        bool created=false;
        diagnostic.stage=202;
        if(!videoShader){const auto hr=d->CreatePixelShader(menuNv12Shader,&videoShader);if(FAILED(hr))return hr;}
        if(!texture){HRESULT hr=d->CreateTexture(frame.width,frame.height,1,D3DUSAGE_DYNAMIC,D3DFMT_L8,D3DPOOL_DEFAULT,&texture,nullptr);if(FAILED(hr))return hr;created=true;}
        if(!chroma){HRESULT hr=d->CreateTexture(frame.width/2,frame.height/2,1,D3DUSAGE_DYNAMIC,D3DFMT_A8L8,D3DPOOL_DEFAULT,&chroma,nullptr);if(FAILED(hr))return hr;created=true;}
        if(dirty||created){
            IDirect3DTexture9* planes[]={texture.Get(),chroma.Get()};
            for(unsigned plane=0;plane<2;++plane){
                diagnostic.stage=203;D3DLOCKED_RECT lock{};HRESULT hr=planes[plane]->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD);if(FAILED(hr))return hr;
                const auto row=static_cast<std::size_t>(frame.width);const auto rows=plane?frame.height/2:frame.height;
                const auto* source=frame.pixels.data()+(plane?static_cast<std::size_t>(frame.width)*frame.height:0);
                if(lock.Pitch<0||static_cast<std::size_t>(lock.Pitch)<row){planes[plane]->UnlockRect(0);return D3DERR_INVALIDCALL;}
                diagnostic.stage=204;for(unsigned y=0;y<rows;++y)std::memcpy(static_cast<unsigned char*>(lock.pBits)+static_cast<std::size_t>(y)*lock.Pitch,source+row*y,row);
                diagnostic.stage=205;hr=planes[plane]->UnlockRect(0);if(FAILED(hr))return hr;
            }
            dirty=false;
        }
        return S_OK;
    }
    void Poll(const MenuStateV1& state,MenuActionSender sender,const menu_presentation_policy::Canvas& canvas,const menu_presentation_policy::Wall& wall){
        const unsigned keys=Keys();POINT mouse{};const bool got=Mouse(canvas,mouse);HWND window=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        if(pid!=GetCurrentProcessId()||IsIconic(window)){wasActive=false;keyEdges.Reset(keys);downItem=~0u;return;}
        const bool credit=(state.flags&MenuCredits)!=0;
        if(!wasActive||wasCredits!=credit){keyEdges.Reset(keys);downItem=~0u;previousMouse=mouse;haveMouse=got;wasActive=true;}
        const unsigned beforeSelection=selected;
        unsigned hot=~0u;if(got)hot=menu_presentation_policy::Hit(wall,canvas.Inverse({static_cast<float>(mouse.x),static_cast<float>(mouse.y)}));
        if(got&&hot<6)selected=hot;
        previousMouse=mouse;haveMouse=got;
        const auto edges=keyEdges.Read(keys);const unsigned pressed=edges.pressed,released=edges.released;
        if(credit){if((released&((1u<<2)|(1u<<1)))&&sender&&sender(MenuCloseCredits))ui_sound::Emit(ui_sound::Cue::Back);return;}
        if(pressed&((1u<<3)|(1u<<5)))selected=(selected+5)%6;
        if(pressed&((1u<<4)|(1u<<6)))selected=(selected+1)%6;
        if(selected==0&&!(state.flags&16u))selected=(pressed&((1u<<3)|(1u<<5)))?5:1;
        if(pressed&1)downItem=hot;
        unsigned action=~0u;
        if((released&1)&&hot<6&&hot==downItem)action=hot;
        if(released&((1u<<1)|(1u<<7)))action=selected;
        if(released&1)downItem=~0u;
        // Continue is intentionally disabled until native availability is proven.
        if(action==0&&!(state.flags&16u))return;
    if(action<6&&sender){if(sender(action+1))ui_sound::Emit(ui_sound::Cue::Confirm);}
    else if(selected!=beforeSelection)ui_sound::Emit(ui_sound::Cue::Hover);
    }
    void Mesh(const MenuStateV1& state,UINT w,UINT h){
        vertices.clear();const menu_presentation_policy::Canvas canvas(w,h);const menu_presentation_policy::Wall wall;
        const auto cream=D3DCOLOR_ARGB(255,233,229,220),amber=D3DCOLOR_ARGB(255,226,182,111),muted=D3DCOLOR_ARGB(190,129,139,146);
        const auto flat=[&canvas](Point p){return canvas.Forward(p);};
        if(state.flags&MenuTitleVisible){
            // Same authored logo as the main menu, centered on genuine black.
            // No native title/video or old text plate can show underneath it.
            Text(vertices,edition,[&](Point p){return flat({960-edition.mesh.advanceWidth*.5f+p.x,535+p.y});},muted);
            Text(vertices,beginText,[&](Point p){return flat({960-beginText.mesh.advanceWidth*.5f+p.x,733+p.y});},cream);
            return;
        }
        if(state.flags&MenuCredits){
            // Readable dark veil, without substituting a static screenshot for video.
            Quad(vertices,0,0,static_cast<float>(w),static_cast<float>(h),D3DCOLOR_ARGB(145,3,4,3));
            Text(vertices,creditsTitle,[&](Point p){return flat({960-creditsTitle.mesh.advanceWidth*.5f+p.x,48+p.y});},cream);
            // The complete original list has over 550 nonempty lines. Keep a
            // normal film-credit pace rather than a sixteen-minute crawl.
            const float scroll=(GetTickCount()-creditsTick)*.080f;
            for(const auto& label:credits){const float y=1080+label.y-scroll;if(y+label.mesh.layoutHeight<130||y>980)continue;const auto first=vertices.size();
                Text(vertices,label,[&](Point p){return flat({960-label.mesh.advanceWidth*.5f+p.x,y+p.y});},cream);
                for(auto n=first;n<vertices.size();++n){const float canonicalY=(vertices[n].y-canvas.y)/canvas.scale;const float fade=std::clamp(std::min((canonicalY-130)/55,(970-canonicalY)/55),0.f,1.f);const auto alpha=static_cast<DWORD>((vertices[n].color>>24)*fade);vertices[n].color=(vertices[n].color&0xffffffu)|(alpha<<24);}
            }
            const auto a=flat({72,994}),b=flat({133,1037});
            Quad(vertices,a.x,a.y,b.x-a.x,canvas.scale,cream);Quad(vertices,a.x,b.y,b.x-a.x,canvas.scale,cream);
            Quad(vertices,a.x,a.y,canvas.scale,b.y-a.y,cream);Quad(vertices,b.x,a.y,canvas.scale,b.y-a.y,cream);
            Text(vertices,backKey,[&](Point p){return flat({80+p.x,998+p.y});},cream);
            Text(vertices,backText,[&](Point p){return flat({151+p.x,996+p.y});},cream);
            return;
        }
        const auto transformed=[&](Point p){return flat(wall.Forward(p.x,p.y));};
        ui_theme::Mesh panel;ui_theme::Panel(panel,125,65,575,945,false);
        for(const auto& p:panel){const auto v=flat({p.x,p.y});vertices.push_back({v.x-.5f,v.y-.5f,0,1,p.color,0,0});}
        Text(vertices,edition,[&](Point p){return flat({180+p.x,302+p.y});},muted);
        for(unsigned i=0;i<6;++i){
            const bool available=i!=0||(state.flags&16u)!=0;const auto color=available?(selected==i?amber:cream):muted;
            Text(vertices,labels[i],[&](Point p){return transformed({p.x+1,100.f*i+p.y+2});},D3DCOLOR_ARGB(170,0,0,0));
            Text(vertices,labels[i],[&](Point p){return transformed({p.x,100.f*i+p.y});},color);
            if(selected==i){const auto a=transformed({-3,100.f*i+72}),b=transformed({370,100.f*i+72});const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);
                if(len>.01f){const Point n{-dy/len*canvas.scale*.7f,dx/len*canvas.scale*.7f};Triangle(vertices,{a.x+n.x,a.y+n.y},{b.x+n.x,b.y+n.y},{b.x-n.x,b.y-n.y},amber);Triangle(vertices,{a.x+n.x,a.y+n.y},{b.x-n.x,b.y-n.y},{a.x-n.x,a.y-n.y},amber);}
                const auto p=transformed({-26,100.f*i+34});const float s=5*canvas.scale;Triangle(vertices,{p.x-s,p.y},{p.x,p.y-s},{p.x+s,p.y},amber);Triangle(vertices,{p.x-s,p.y},{p.x+s,p.y},{p.x,p.y+s},amber);
            }
        }
    }
};
Presentation::Presentation():impl_(new Impl){}
Presentation::~Presentation(){Shutdown();}
bool Presentation::Start(const wchar_t* path){
    if(impl_->started||!path||!*path)return false;
    impl_->error=S_OK;impl_->diagnostic={};impl_->dirty=true;
    impl_->haveMouse=false;impl_->downItem=~0u;impl_->reveal.Reset();
    impl_->started=true;
    if(!impl_->player.Start(path))impl_->Failure(impl_->player.Error(),0,0,"Video startup");
    return true; // Menu navigation remains available if media startup fails.
}
void Presentation::SetVisible(bool visible)noexcept{impl_->player.SetActive(visible);if(!visible){impl_->ready=false;impl_->wasActive=false;impl_->reveal.Reset();}}
void Presentation::SuspendVideoForLoad(){impl_->player.Stop();}
bool Presentation::ResumeVideo(const wchar_t* path){
    if(!impl_->started)return false;
    impl_->error=S_OK;impl_->diagnostic={};
    // Keep the last uploaded frame visible while the new worker starts. Its
    // session-local serial must not collide with the previous session's frame.
    if(impl_->frame.serial)impl_->frame.serial=~std::uint64_t{};
    return impl_->player.Start(path);
}
HRESULT Presentation::SetCredits(const wchar_t* text){
    if(!text||wcslen(text)>65536)return E_INVALIDARG;
    try{std::vector<Label> pending;std::wstring source(text);std::size_t start=0;float y=0;unsigned triangles=0;
        while(start<source.size()){
            const auto end=source.find(L'\n',start);std::wstring line=source.substr(start,end==std::wstring::npos?source.size()-start:end-start);if(!line.empty()&&line.back()==L'\r')line.pop_back();
            if(!line.empty()){Label label;const auto hr=BuildLabel(line.c_str(),28,label);if(FAILED(hr))return hr;triangles+=static_cast<unsigned>(label.mesh.triangles.size());if(triangles>1048576)return E_OUTOFMEMORY;label.y=y;y+=label.mesh.layoutHeight+12;pending.push_back(std::move(label));}
            else y+=35;
            if(end==std::wstring::npos)break;start=end+1;
        }
        impl_->credits=std::move(pending);return S_OK;
    }catch(...){return E_OUTOFMEMORY;}
}
bool Presentation::Draw(IDirect3DDevice9* d,const MenuStateV1& state,MenuActionSender sender,bool backdropOnly)noexcept{
    impl_->ready=false;
    const bool title=(state.flags&MenuTitleVisible)!=0,root=(state.flags&MenuRootVisible)!=0;
    if(!d||!impl_->started||state.size!=sizeof(state)||state.version!=1||!(state.flags&MenuAuthenticated)||
       (!backdropOnly&&(!root&&!title||root&&title||title&&(state.flags&MenuCredits)))||GetTickCount()-state.tick>250){SetVisible(false);return false;}
    SetVisible(true);
    try{
        impl_->diagnostic.stage=100;HRESULT hr=impl_->Labels();if(FAILED(hr)){impl_->Failure(hr,0,0,"Labels");return false;}
        // The native title retains its black logo. Genuine navigation never
        // waits for a first video frame: a stalled helper must not turn an
        // already accepted PressAny into a non-interactive title forever.
        bool drawTitle=title,videoAvailable=false;
        if(!title){hr=impl_->Upload(d);if(FAILED(hr)){
            impl_->Failure(hr,0,0,"Upload");
            if(hr==D3DERR_DEVICELOST||hr==D3DERR_DEVICENOTRESET)return false;
            // A media failure must never strand navigation behind an invisible
            // title. Keep the genuine menu and logo usable on a clean backdrop.
            // A last complete video frame, when available, is retained by Upload.
            hr=S_OK;
        }else if(hr!=S_FALSE)videoAvailable=true;}
        impl_->diagnostic.stage=102;
        ComPtr<IDirect3DSurface9> back,target;D3DSURFACE_DESC desc{};D3DCAPS9 caps{};
        if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(back->GetDesc(&desc))||FAILED(d->GetRenderTarget(0,&target))||target.Get()!=back.Get()||FAILED(d->GetDeviceCaps(&caps)))return false;
        for(UINT n=1;n<caps.NumSimultaneousRTs;++n){ComPtr<IDirect3DSurface9> extra;hr=d->GetRenderTarget(n,&extra);if(extra||(FAILED(hr)&&hr!=D3DERR_NOTFOUND))return false;}
        const bool credit=(state.flags&MenuCredits)!=0;if(credit&&!impl_->wasCredits)impl_->creditsTick=GetTickCount();
        if(!backdropOnly)impl_->reveal.Observe(drawTitle,root&&!drawTitle,GetTickCount());
        const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height);const menu_presentation_policy::Wall wall;
        impl_->diagnostic.stage=103;auto paintState=state;if(drawTitle)paintState.flags=(paintState.flags&~(MenuRootVisible|MenuCredits))|MenuTitleVisible;
        if(backdropOnly)impl_->vertices.clear();else impl_->Mesh(paintState,desc.Width,desc.Height);
        const float revealAlpha=impl_->reveal.Alpha(GetTickCount());
        if(!backdropOnly&&root&&!drawTitle&&revealAlpha<1.f)Quad(impl_->vertices,0,0,static_cast<float>(desc.Width),static_cast<float>(desc.Height),D3DCOLOR_ARGB(static_cast<DWORD>(std::lround((1.f-revealAlpha)*255)),0,0,0));
        if(impl_->vertices.size()/3>caps.MaxPrimitiveCount)return false;
        impl_->diagnostic.stage=104;SavedState saved(d);if(!saved.Capture())return false;
        impl_->diagnostic.stage=105;hr=d->BeginScene();if(FAILED(hr))return false;saved.scene=true;
#define MENU_TRY(call) if(SUCCEEDED(hr))hr=(call)
        const D3DVIEWPORT9 viewport{0,0,desc.Width,desc.Height,0,1};MENU_TRY(d->SetViewport(&viewport));MENU_TRY(d->SetVertexShader(nullptr));MENU_TRY(d->SetPixelShader(nullptr));MENU_TRY(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1));MENU_TRY(d->SetStreamSourceFreq(0,1));
        const struct{D3DRENDERSTATETYPE key;DWORD value;} settings[]={{D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_LIGHTING,FALSE},{D3DRS_FOGENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_COLORWRITEENABLE,15},{D3DRS_ALPHABLENDENABLE,TRUE},{D3DRS_SRCBLEND,D3DBLEND_SRCALPHA},{D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA},{D3DRS_BLENDOP,D3DBLENDOP_ADD},{D3DRS_SEPARATEALPHABLENDENABLE,FALSE},{D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_SRGBWRITEENABLE,FALSE},{D3DRS_CLIPPLANEENABLE,0},{D3DRS_WRAP0,0},{D3DRS_MULTISAMPLEMASK,0xffffffff}};
        for(const auto& item:settings){MENU_TRY(d->SetRenderState(item.key,item.value));}
        MENU_TRY(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));MENU_TRY(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));MENU_TRY(d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));MENU_TRY(d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT));
        MENU_TRY(d->SetTexture(0,nullptr));MENU_TRY(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));MENU_TRY(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));MENU_TRY(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));MENU_TRY(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));
        std::vector<Vertex> black;Quad(black,0,0,static_cast<float>(desc.Width),static_cast<float>(desc.Height),0xff000000);MENU_TRY(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,black.data(),sizeof(Vertex)));
        if(!drawTitle&&videoAvailable){
        const float x=canvas.x-.5f,y=canvas.y-.5f,w=canvas.width,h=canvas.height;
        const Vertex quad[]={{x,y,0,1,0xffffffff,0,0},{x+w,y,0,1,0xffffffff,1,0},{x+w,y+h,0,1,0xffffffff,1,1},{x,y,0,1,0xffffffff,0,0},{x+w,y+h,0,1,0xffffffff,1,1},{x,y+h,0,1,0xffffffff,0,1}};
        float colors[8]{};menu_nv12_color::Constants(impl_->frame.matrix,impl_->frame.nominalRange,colors);
        float chromaOffset[4]{};menu_nv12_color::ChromaOffset(impl_->frame.chromaSiting,impl_->frame.width,impl_->frame.height,chromaOffset);
        MENU_TRY(d->SetTexture(0,impl_->texture.Get()));MENU_TRY(d->SetTexture(1,impl_->chroma.Get()));MENU_TRY(d->SetPixelShader(impl_->videoShader.Get()));MENU_TRY(d->SetPixelShaderConstantF(0,colors,2));MENU_TRY(d->SetPixelShaderConstantF(2,chromaOffset,1));
        for(DWORD sampler=0;sampler<2;++sampler){MENU_TRY(d->SetSamplerState(sampler,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));MENU_TRY(d->SetSamplerState(sampler,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));MENU_TRY(d->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_NONE));MENU_TRY(d->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));MENU_TRY(d->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));MENU_TRY(d->SetSamplerState(sampler,D3DSAMP_SRGBTEXTURE,FALSE));}
        MENU_TRY(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,quad,sizeof(Vertex)));
        }
        MENU_TRY(d->SetPixelShader(nullptr));MENU_TRY(d->SetTexture(0,nullptr));MENU_TRY(d->SetTexture(1,nullptr));MENU_TRY(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));
        if(!impl_->vertices.empty()){MENU_TRY(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(impl_->vertices.size()/3),impl_->vertices.data(),sizeof(Vertex)));}
#undef MENU_TRY
        impl_->diagnostic.stage=109;const bool restored=saved.Restore();if(FAILED(hr)){impl_->Failure(hr,0,0,"native D3D draw");return false;}if(!restored)return false;
        if(!backdropOnly&&!(state.flags&MenuCredits)){
            ui_art::Scene logo;logo.Image(8,1990,485,drawTitle?660.f:164.f,drawTitle?350.f:120.f,drawTitle?600.f:510.f,drawTitle?146.f:124.f);
            if(!drawTitle&&revealAlpha<1.f)for(auto& batch:logo.batches)for(auto& v:batch.vertices)v.color=(v.color&0xffffffu)|(static_cast<DWORD>(255*revealAlpha)<<24);
            if(!ui_art::Draw(d,logo,impl_->logoAssets,canvas.scale,canvas.x,canvas.y))return false;
        }
        impl_->diagnostic.stage=110;if(!backdropOnly&&(state.flags&MenuRootVisible)){if(!drawTitle)impl_->Poll(state,sender,canvas,wall);else impl_->wasActive=false;impl_->wasCredits=credit;impl_->lastDraw=GetTickCount();impl_->ready=true;}else impl_->wasActive=false;impl_->error=S_OK;impl_->diagnostic.error=S_OK;return true;
    }catch(const std::bad_alloc& e){impl_->Failure(E_OUTOFMEMORY,1,0,e.what());return false;}
    catch(const std::system_error& e){impl_->Failure(E_UNEXPECTED,2,static_cast<DWORD>(e.code().value()),e.what());return false;}
    catch(const std::exception& e){impl_->Failure(E_UNEXPECTED,3,0,e.what());return false;}
    catch(...){impl_->Failure(E_UNEXPECTED,4,0,"unknown C++ exception");return false;}
}
void Presentation::ReleaseDeviceResources()noexcept{impl_->logoAssets.Reset();impl_->texture.Reset();impl_->chroma.Reset();impl_->videoShader.Reset();impl_->owner=nullptr;impl_->ready=false;}
void Presentation::Shutdown(){ReleaseDeviceResources();impl_->player.Stop();impl_->started=false;impl_->frame={};impl_->wasActive=false;}
bool Presentation::Ready()const noexcept{return impl_->ready&&GetTickCount()-impl_->lastDraw<=250;}
HRESULT Presentation::Error()const noexcept{return FAILED(impl_->player.Error())?impl_->player.Error():impl_->error;}
Diagnostic Presentation::Diagnostics()const noexcept{auto result=impl_->diagnostic;result.media=impl_->player.Diagnostics();return result;}
}
