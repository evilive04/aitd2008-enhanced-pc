#define NOMINMAX
#include "ui_pointer.h"
#include "dialog_presentation.h"
#include "ui_theme.h"
#include "ui_art.h"
#include "menu_presentation_policy.h"
#include "ui_sound.h"
#include <wrl/client.h>
#include <string>
#include <algorithm>
namespace dialog_presentation {
using Microsoft::WRL::ComPtr;
struct Presentation::Impl {
    ui_theme::Label body,labels[2],enter;std::wstring cachedBody,cachedLabels[2];
    DWORD generation{};unsigned selected{},pressed{~0u};bool visible{};
    menu_presentation_policy::KeyEdges keys;POINT previous{};bool haveMouse{};
    ui_art::Assets assets;IDirect3DDevice9* owner{};
    static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_LEFT,VK_RIGHT,VK_TAB,VK_ESCAPE};unsigned value=0;for(unsigned n=0;n<6;++n)if(GetAsyncKeyState(codes[n])&0x8000)value|=1u<<n;return value;}
    static bool Enabled(const DialogStateV1& state,unsigned index){return !(state.flags&DialogClosing)&&(state.flags&(index?DialogSecondEnabled:DialogFirstEnabled));}
    static bool InButton(float x,float y,unsigned index){return x>=720+index*250&&x<=945+index*250&&y>=645&&y<=709;}
    bool Prepare(const DialogStateV1& state){
        if(cachedBody!=state.body){ui_theme::Label pending;if(FAILED(ui_theme::BuildLabel(state.body,34,1040,240,pending,L"Times New Roman"))||pending.mesh.layoutHeight>240)return false;body=std::move(pending);cachedBody=state.body;}
        const WCHAR* strings[]={state.firstLabel,state.secondLabel};
        for(unsigned i=0;i<2;++i)if(cachedLabels[i]!=strings[i]){ui_theme::Label pending;if(FAILED(ui_theme::BuildLabel(strings[i],34,195,48,pending,L"Times New Roman"))||pending.mesh.layoutHeight>48||pending.mesh.advanceWidth>195)return false;labels[i]=std::move(pending);cachedLabels[i]=strings[i];}
        if(enter.mesh.triangles.empty()&&FAILED(ui_theme::BuildLabel(L"Enter",18,120,35,enter,L"Times New Roman")))return false;
        return true;
    }
    bool Draw(IDirect3DDevice9* d,const DialogStateV1& state,DialogActionSender send,ui_backdrop::Draw backdrop){
        if(!d||!send||state.size!=sizeof(state)||state.version!=1||!(state.flags&DialogReady)||GetTickCount()-state.tick>250)return false;
        ComPtr<IDirect3DSurface9> back,target,mrt;if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get())return false;
        for(DWORD index=1;index<4;++index){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(index,&mrt))&&mrt)return false;}
        D3DSURFACE_DESC desc{};if(FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
        if(!Prepare(state))return false;
        menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);const unsigned held=Keys();
        HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);
        if(pid!=GetCurrentProcessId()||IsIconic(window)){Hide();return false;}
        if(owner!=d){assets.Reset();owner=nullptr;if(FAILED(assets.LoadLocal(d)))return false;owner=d;}
        if(!visible||generation!=state.generation){keys.Reset(held);generation=state.generation;selected=Enabled(state,0)?0:1;pressed=~0u;haveMouse=false;visible=true;}
        const unsigned beforeSelection=selected;
        RECT client{};POINT pointer{};bool mouse=ui_pointer::ReadClient(window,client,pointer);
        ui_theme::Point position{};
        if(mouse){position={((pointer.x*static_cast<float>(desc.Width)/client.right)-canvas.x)/canvas.scale,((pointer.y*static_cast<float>(desc.Height)/client.bottom)-canvas.y)/canvas.scale};
            if(!haveMouse||pointer.x!=previous.x||pointer.y!=previous.y)for(unsigned i=0;i<2;++i)if(Enabled(state,i)&&InButton(position.x,position.y,i))selected=i;
            previous=pointer;haveMouse=true;
        }
        ui_art::Scene geometry;
        // The old film is suppressed by a fresh exact-root paint lease.
        // No fullscreen fill is used to conceal it.
        geometry.Frame(360,350,1200,380);
        geometry.Text(body,425,395,0xffd5d4ca);
        geometry.Rect(418,625,1080,.75f,0xff777b6a);
        for(unsigned i=0;i<2;++i){
            const bool enabled=Enabled(state,i);if(!(state.flags&(i?DialogSecondEnabled:DialogFirstEnabled)))continue;
            const float x=720+i*250.f;geometry.Button(x,645,225,64,selected==i,enabled);
            geometry.Text(labels[i],x+(225-labels[i].mesh.advanceWidth)*.5f,653,enabled?0xffe8e4d5:0xff95978c);
        }
        geometry.Button(420,665,65,32,false,true);geometry.Text(enter,428,670,0xffd9d7c8);
        if(backdrop&&!backdrop(d))return false;
        if(!ui_art::Draw(d,geometry,assets,canvas.scale,canvas.x,canvas.y))return false;
        const auto edges=keys.Read(held);
        if(edges.pressed&((1u<<2)|(1u<<3)|(1u<<4))){const unsigned next=1-selected;if(Enabled(state,next))selected=next;}
        if((edges.pressed&1)&&mouse){pressed=~0u;for(unsigned i=0;i<2;++i)if(Enabled(state,i)&&InButton(position.x,position.y,i))pressed=i;}
        unsigned action=~0u;
        if((edges.released&1)&&pressed<2&&mouse&&InButton(position.x,position.y,pressed))action=pressed;
        if(edges.released&1)pressed=~0u;
        if((edges.released&(1u<<1))&&Enabled(state,selected))action=selected;
        // Custom Escape explicitly chooses the safe cancel/no result, never geou.
        if(edges.released&(1u<<5))for(unsigned i=0;i<2;++i)if(Enabled(state,i)&&(i?state.secondResult:state.firstResult)==2)action=i;
        if(action<2&&Enabled(state,action)){if(send(action+1,state.generation))ui_sound::Emit((action?state.secondResult:state.firstResult)==2?ui_sound::Cue::Back:ui_sound::Cue::Confirm);}
        else if(selected!=beforeSelection)ui_sound::Emit(ui_sound::Cue::Hover);
        return true;
    }
    void Hide(){visible=false;haveMouse=false;pressed=~0u;keys.Reset(Keys());}
};
Presentation::Presentation():impl(new Impl){}
Presentation::~Presentation(){delete impl;}
bool Presentation::Draw(IDirect3DDevice9* d,const DialogStateV1& s,DialogActionSender send,ui_backdrop::Draw backdrop)noexcept{try{return impl->Draw(d,s,send,backdrop);}catch(...){impl->Hide();return false;}}
void Presentation::Hide()noexcept{impl->Hide();}
void Presentation::ReleaseDeviceResources()noexcept{impl->assets.Reset();impl->owner=nullptr;impl->Hide();}
bool Presentation::OwnsDevice(IDirect3DDevice9* device)const noexcept{return impl->owner==device;}
}
