#define NOMINMAX
#include "ui_pointer.h"
#include "options_presentation.h"
#include "ui_art.h"
#include "menu_presentation_policy.h"
#include "options_route_policy.h"
#include "ui_sound.h"
#include <string>
#include <algorithm>
namespace options_presentation {
using Microsoft::WRL::ComPtr;
struct Presentation::Impl {
    ui_art::OptionsLabels labels;std::wstring cached[7];ui_art::Scene art;
    ui_art::Assets assets;IDirect3DDevice9* owner{};
    DWORD generation{};unsigned selected{},pressed{~0u};bool visible{},haveMouse{},cachedInGame{};POINT previous{};
    menu_presentation_policy::KeyEdges keys;
    static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_UP,VK_DOWN,VK_TAB,VK_ESCAPE,'X'};unsigned held=0;for(unsigned n=0;n<7;++n)if(GetAsyncKeyState(codes[n])&0x8000)held|=1u<<n;return held;}
    static unsigned Hit(float x,float y,unsigned count){
        for(unsigned n=0;n<count;++n)if(ui_art::OptionRow(n).Contains(x,y))return n+1;
        if(y>=970&&y<1024){if(x>=1630&&x<1820)return OptionsBack;if(x>=725&&x<1180)return OptionsResetAll;if(x>=105&&x<330)return 7;}
        return ~0u;
    }
    bool Prepare(const OptionsStateV1& state){
        const bool ingame=(state.flags&OptionsInGame)!=0;bool changed=labels.title.mesh.triangles.empty()||cachedInGame!=ingame;for(unsigned n=0;n<7;++n){if(ingame&&n==2)continue;if(!wmemchr(state.labels[n],0,128)||!state.labels[n][0])return false;changed|=cached[n]!=state.labels[n];}
        if(changed){const wchar_t* values[]={state.labels[1],state.labels[ingame?3:2],state.labels[ingame?4:3],state.labels[4]};ui_art::OptionsLabels pending;
            if(FAILED(pending.Build(state.labels[0],values,state.labels[5],state.labels[6])))return false;
            // A translated label must fit naturally. Never distort a font to fit.
            for(const auto& row:pending.rows)if(row.mesh.lineCount!=1||row.mesh.advanceWidth>530)return false;
            if(pending.title.mesh.lineCount!=1||pending.back.mesh.lineCount!=1||pending.back.mesh.advanceWidth>140||pending.reset.mesh.lineCount!=1||pending.reset.mesh.advanceWidth>370)return false;
            labels=std::move(pending);for(unsigned n=0;n<7;++n)cached[n]=ingame&&n==2?L"":state.labels[n];cachedInGame=ingame;art.batches.clear();
        }return true;
    }
    bool Draw(IDirect3DDevice9* d,const OptionsStateV1& state,OptionsActionSender send,const wchar_t* directory,ui_backdrop::Draw backdrop){
        if(!d||!send||!directory||state.size!=sizeof(state)||state.version!=1||(state.flags&(OptionsAuthenticated|OptionsRootVisible))!=(OptionsAuthenticated|OptionsRootVisible)||GetTickCount()-state.tick>250)return false;
        ComPtr<IDirect3DSurface9> back,target,mrt;if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get())return false;
        for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
        D3DSURFACE_DESC desc{};if(FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
        HWND window=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window)){Hide();return false;}
        if(!Prepare(state))return false;
        if(owner!=d){assets.Reset();owner=nullptr;if(FAILED(assets.Load(d,directory)))return false;owner=d;}
        const bool ingame=(state.flags&OptionsInGame)!=0;const unsigned count=options_route_policy::Count(ingame);const unsigned held=Keys();bool changed=false;
        if(!visible||generation!=state.generation){keys.Reset(held);generation=state.generation;selected=0;pressed=~0u;haveMouse=false;visible=true;changed=true;}
        if(selected>=count){selected=0;changed=true;}
        const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT pointer{};
        const bool mouse=ui_pointer::ReadClient(window,client,pointer);
        const unsigned beforeSelection=selected;
        ui_theme::Point pos{};unsigned hit=~0u;
        if(mouse){pos={((pointer.x*static_cast<float>(desc.Width)/client.right)-canvas.x)/canvas.scale,((pointer.y*static_cast<float>(desc.Height)/client.bottom)-canvas.y)/canvas.scale};hit=Hit(pos.x,pos.y,count);
            if(haveMouse&&(pointer.x!=previous.x||pointer.y!=previous.y)&&hit>=1&&hit<=count&&selected!=hit-1){selected=hit-1;changed=true;}previous=pointer;haveMouse=true;}
        const auto edges=keys.Read(held);
        if(edges.pressed&((1u<<2)|(1u<<3)|(1u<<4))){selected=(selected+((edges.pressed&(1u<<2))?count-1:1u))%count;changed=true;}
        if(changed||art.batches.empty()){art.batches.clear();ui_art::Options(art,labels,static_cast<int>(selected),ingame);}
        // Keep the live background visible outside and through the compact smoke panel.
        ui_art::Scene frame;
        frame.batches.insert(frame.batches.end(),art.batches.begin(),art.batches.end());
        if(backdrop&&!backdrop(d))return false;
        if(!ui_art::Draw(d,frame,assets,canvas.scale,canvas.x,canvas.y))return false;
        unsigned action=0;
        if(edges.pressed&1)pressed=hit;
        if(edges.released&1){if(pressed>=1&&pressed<=7&&hit==pressed)action=pressed==7?selected+1:pressed;pressed=~0u;}
        if(edges.released&(1u<<1))action=selected+1;
        if(edges.released&(1u<<5))action=OptionsBack;
        if(edges.released&(1u<<6))action=OptionsResetAll;
        if(action>=1&&action<=count)action=options_route_policy::RowAction(action-1,ingame);
        if(action){if(send(action,state.generation))ui_sound::Emit(action==OptionsBack?ui_sound::Cue::Back:ui_sound::Cue::Confirm);}
        else if(changed&&beforeSelection!=selected)ui_sound::Emit(ui_sound::Cue::Hover);return true;
    }
    void Hide(){visible=false;haveMouse=false;pressed=~0u;keys.Reset(Keys());}
};
Presentation::Presentation():impl(new Impl){}
Presentation::~Presentation(){delete impl;}
bool Presentation::Draw(IDirect3DDevice9* d,const OptionsStateV1& state,OptionsActionSender send,const wchar_t* directory,ui_backdrop::Draw backdrop)noexcept{try{return impl->Draw(d,state,send,directory,backdrop);}catch(...){impl->Hide();return false;}}
void Presentation::Hide()noexcept{impl->Hide();}
void Presentation::ReleaseDeviceResources()noexcept{impl->assets.Reset();impl->owner=nullptr;impl->Hide();}
bool Presentation::OwnsDevice(IDirect3DDevice9* device)const noexcept{return impl->owner==device;}
}
