#define NOMINMAX
#include "ui_pointer.h"
#include "ui_pointer_art.h"
#include "options_children_presentation.h"
#include "options_children_scene_cache.h"
#include "phone_roster_content.h"
#include "options_children_controls.h"
#include "ui_sound.h"
#include "phone_status_art.h"
namespace options_children_presentation {
using Microsoft::WRL::ComPtr;
struct Presentation::Impl {
    ui_art::Assets assets;IDirect3DDevice9* owner{};
    options_children_art::View cached{};ui_art::Scene art;options_children_art::Layout layout{};
    SceneCache sceneCache;
    options_children_controls::Controls controls;
    options_children_controls::Requests requests;
    static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_UP,VK_DOWN,VK_TAB,VK_ESCAPE,VK_LEFT,VK_RIGHT,'X','C'};unsigned held=0;for(unsigned n=0;n<10;++n)if(GetAsyncKeyState(codes[n])&0x8000)held|=1u<<n;return held;}
    bool Prepare(const OptionsChildrenStateV1& state){auto view=state.view;
        // These are our actual shortcuts, independent of original hidden keycaps.
        for(auto& f:view.footer){using Action=options_children_art::Action;const wchar_t* key=f.action==Action::Confirm?L"Enter":f.action==Action::Cancel?L"Esc":f.action==Action::Reset?L"X":f.action==Action::Calibration?L"C":nullptr;if(key)wcscpy_s(f.key,key);}
        if(!art.batches.empty()&&options_children_art::UpdateGaugeCurrents(cached,view,art,layout)){cached=view;return true;}
        // Keep recent hover/tooltip variants: revisiting a row reuses its exact
        // geometry rather than synchronously refitting and tessellating all text.
        sceneCache.Remember(cached,art,layout);
        ui_art::Scene next;options_children_art::Layout nextLayout;
        if(sceneCache.Find(view,next,nextLayout)){cached=view;art=std::move(next);layout=nextLayout;return true;}
        if(FAILED(options_children_art::Build(view,next,nextLayout)))return false;
        cached=view;art=std::move(next);layout=nextLayout;sceneCache.Remember(cached,art,layout);return true;
    }
    bool Draw(IDirect3DDevice9* d,const OptionsChildrenStateV1& state,OptionsChildrenSender send,const wchar_t* directory,ui_backdrop::Draw backdrop,PhonePortraitGetter portraitGetter){
        if(!d||!send||!directory||state.size!=sizeof(state)||state.version!=1||(state.flags&(OptionsChildVisible|OptionsChildActionsProven))!=(OptionsChildVisible|OptionsChildActionsProven)||GetTickCount()-state.tick>250)return false;
        ComPtr<IDirect3DSurface9> back,target,mrt;if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get())return false;
        for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
        D3DSURFACE_DESC desc{};if(FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
        HWND window=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window)){controls.Hide(Keys());return false;}
        if(!Prepare(state))return false;
        if(owner!=d){assets.Reset();owner=nullptr;if(FAILED(assets.Load(d,directory)))return false;owner=d;}
        const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT pointer{};
        options_children_controls::Input input;input.held=Keys();input.tick=GetTickCount();
        input.mouse=ui_pointer::ReadClient(window,client,pointer);
        if(input.mouse){input.x=((pointer.x*static_cast<float>(desc.Width)/client.right)-canvas.x)/canvas.scale;input.y=((pointer.y*static_cast<float>(desc.Height)/client.bottom)-canvas.y)/canvas.scale;}
        ui_art::Scene frame;frame.batches.insert(frame.batches.end(),art.batches.begin(),art.batches.end());
        if((state.view.page==options_children_art::Page::PhoneMessages||state.view.page==options_children_art::Page::PhoneContacts)&&state.view.selected>=0){
            const auto row=static_cast<DWORD>(state.view.selected);const auto kind=phone_roster_content::Parse(state.view.rows[row].value);
            if(kind==phone_roster_content::Kind::None)return false;
            if(phone_roster_content::HasPortrait(kind)){
                ComPtr<IDirect3DTexture9> image;D3DSURFACE_DESC info{};
                if(!portraitGetter||portraitGetter(row,state.generation,&image)!=S_OK||!image||FAILED(image->GetLevelDesc(0,&info))||info.Width!=128||info.Height!=256)return false;
                if(assets.Texture(ui_art::Material::Image0)!=image.Get()&&FAILED(assets.SetImage(d,0,image.Get())))return false;
                // Original shapes362/373/376 sample128x161 pixels from their
                // 128x256 source texture (2560x3220twips, bitmap scale20).
                // Keep that native crop; the unused lower texture is not art.
                const auto b=options_children_art::RosterPortrait();if(!frame.ImageRegion(0,info.Width,info.Height,{0,0,128,161},b.x,b.y,b.w,b.h))return false;
            }
        }
        if(state.view.page>=options_children_art::Page::PhoneParameters&&(state.flags&OptionsChildPhoneNetworkKnown))phone_status_art::Append(frame,(state.flags&OptionsChildPhoneNetwork)!=0);
        if(state.view.page>=options_children_art::Page::PhoneParameters)ui_pointer::AppendHiddenCursor(frame,input.mouse,input.x,input.y);
        if(backdrop&&!backdrop(d))return false;
        if(!ui_art::Draw(d,frame,assets,canvas.scale,canvas.x,canvas.y))return false;
        requests.Offer(state.generation,controls.Read(state,layout,input));const auto action=requests.Next(state);
        if(action.action!=options_children_art::Action::None){
            const bool accepted=send(static_cast<DWORD>(action.action),action.row,action.normalized,state.generation)!=FALSE;requests.Complete(action,accepted);
            using Action=options_children_art::Action;
            if(accepted){if(action.action==Action::Cancel)ui_sound::Emit(ui_sound::Cue::Back);
                else if(action.action==Action::Confirm||action.action==Action::Activate||action.action==Action::Reset||action.action==Action::Calibration)ui_sound::Emit(ui_sound::Cue::Confirm);
                else if(action.action==Action::Previous||action.action==Action::Next||(action.action==Action::FocusRow&&action.row!=state.view.selected))ui_sound::Emit(ui_sound::Cue::Hover);}
        }return true;
    }
    void Release(){assets.Reset();owner=nullptr;controls.Hide(Keys());requests.Clear();}
};
Presentation::Presentation():impl(new Impl){}
Presentation::~Presentation(){delete impl;}
bool Presentation::Draw(IDirect3DDevice9* d,const OptionsChildrenStateV1& state,OptionsChildrenSender send,const wchar_t* directory,ui_backdrop::Draw backdrop,PhonePortraitGetter portraitGetter)noexcept{try{const bool result=impl->Draw(d,state,send,directory,backdrop,portraitGetter);if(!result){impl->controls.Hide(Impl::Keys());impl->requests.Clear();}return result;}catch(...){impl->controls.Hide(Impl::Keys());impl->requests.Clear();return false;}}
void Presentation::ReleaseDeviceResources()noexcept{impl->Release();}
bool Presentation::OwnsDevice(IDirect3DDevice9* device)const noexcept{return impl->owner==device;}
}
