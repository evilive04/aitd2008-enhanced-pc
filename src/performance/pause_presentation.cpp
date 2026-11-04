#define NOMINMAX
#include "ui_pointer.h"
#include "pause_presentation.h"
#include "pause_art.h"
#include "menu_presentation_policy.h"
#include "ui_sound.h"
#include <cstring>
namespace pause_presentation {
struct Presentation::Impl {
    pause_art::Labels labels;wchar_t cached[1920]{};DWORD mask{},generation{};
    ui_art::Scene art;ui_art::Assets assets;IDirect3DDevice9* owner{};
    HRESULT buildStatus=S_OK;
    bool visible=false,haveMouse=false;unsigned selected=0,pressed=~0u;POINT previous{};menu_presentation_policy::KeyEdges keys;
    static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_UP,VK_DOWN,VK_TAB};unsigned held=0;for(unsigned n=0;n<6;++n)if(GetAsyncKeyState(codes[n])&0x8000)held|=1u<<n;return held;}
    void Hide(){visible=false;haveMouse=false;pressed=~0u;keys.Reset(Keys());}
    bool Draw(IDirect3DDevice9* d,const PauseStateV1& s,PauseSender send){
        if(!d||!send||s.size!=sizeof s||s.version!=1||!(s.flags&PauseVisible)||GetTickCount()-s.tick>250)return false;
        Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};
        if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc)))return false;
        for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
        DWORD pid{};const auto window=GetForegroundWindow();if(!window||!GetWindowThreadProcessId(window,&pid)||pid!=GetCurrentProcessId()||IsIconic(window)){Hide();return false;}
        if(labels.title.mesh.triangles.empty()||memcmp(cached,s.labels,sizeof cached)){buildStatus=pause_art::Prepare(s,labels);if(FAILED(buildStatus))return false;memcpy(cached,s.labels,sizeof cached);art.batches.clear();}
        if(owner!=d){assets.Reset();owner=nullptr;if(FAILED(assets.LoadLocal(d)))return false;owner=d;}
        const unsigned held=Keys();bool changed=false;const unsigned beforeSelection=selected;const bool wasVisible=visible;
        if(!visible||generation!=s.generation){Hide();generation=s.generation;visible=true;selected=0;changed=true;keys.Reset(held);}
        if(mask!=s.enabledMask){mask=s.enabledMask;changed=true;}
        const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT pointer{};
        const bool mouse=ui_pointer::ReadClient(window,client,pointer);
        ui_theme::Point pos{};unsigned hit=~0u;
        if(mouse){pos={((pointer.x*static_cast<float>(desc.Width)/client.right)-canvas.x)/canvas.scale,((pointer.y*static_cast<float>(desc.Height)/client.bottom)-canvas.y)/canvas.scale};hit=pause_art::Hit(pos.x,pos.y);
            if(haveMouse&&(pointer.x!=previous.x||pointer.y!=previous.y)&&hit<6&&(mask&(1u<<(pause_art::Slots[hit]-1)))&&selected!=hit){selected=hit;changed=true;}previous=pointer;haveMouse=true;}
        if(s.flags&PauseBusy){keys.Reset(held);pressed=~0u;}
        const auto edges=keys.Read(held);if(edges.pressed&((1u<<3)|(1u<<4)|(1u<<5))){selected=pause_art::Navigate(selected,edges.pressed&(1u<<3)?-1:1,mask);changed=true;}
        if(changed||art.batches.empty())pause_art::Build(s,labels,selected,art);
        // Never clear/blit a background here. Alpha reveals the real game.
        if(!ui_art::Draw(d,art,assets,canvas.scale,canvas.x,canvas.y))return false;
        if(s.flags&PauseBusy)return true;
        unsigned action=0;if(edges.pressed&1)pressed=hit;
        if(edges.released&1){if(hit<6&&hit==pressed)action=pause_art::Slots[hit];pressed=~0u;}
        if((edges.released&(1u<<1))&&selected<6)action=pause_art::Slots[selected];
        if(edges.released&(1u<<2))action=1;
        if(action&&(mask&(1u<<(action-1)))){if(send(action,s.generation))ui_sound::Emit(action==1?ui_sound::Cue::Back:ui_sound::Cue::Confirm);}
        else if(wasVisible&&selected!=beforeSelection)ui_sound::Emit(ui_sound::Cue::Hover);return true;
    }
};
Presentation::Presentation():impl(new Impl){}Presentation::~Presentation(){delete impl;}
bool Presentation::Draw(IDirect3DDevice9* d,const PauseStateV1& s,PauseSender send)noexcept{try{if(impl->Draw(d,s,send))return true;}catch(...){}impl->Hide();return false;}
void Presentation::ReleaseDeviceResources()noexcept{impl->assets.Reset();impl->owner=nullptr;impl->Hide();}
bool Presentation::OwnsDevice(IDirect3DDevice9* d)const noexcept{return impl->owner==d;}
HRESULT Presentation::BuildStatus()const noexcept{return impl->buildStatus;}
}
