#include "main_popup_art.h"
#include "menu_presentation_policy.h"
#include "ui_pointer.h"
#include "ui_pointer_art.h"
namespace main_popup_overlay {
static bool drawing=false;static MainPopupGetter getter=nullptr;static MainPopupSender sender=nullptr;static MainPopupReady setter=nullptr;static DWORD lookup{},readyGeneration{},lastStatus=~0u;
struct Presentation{
 IDirect3DDevice9* owner{};ui_art::Assets assets;ui_art::Scene art;MainPopupStateV1 cached{};
 menu_presentation_policy::KeyEdges keys;DWORD generation{};unsigned selected{},builtSelected=~0u,pressed=~0u;bool visible=false;
 static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_LEFT,VK_RIGHT};unsigned mask=0;for(unsigned n=0;n<5;++n)if(GetAsyncKeyState(codes[n])&0x8000)mask|=1u<<n;return mask;}
 void Hide(){visible=false;pressed=~0u;owner=nullptr;keys.Reset(Keys());}
 bool Draw(IDirect3DDevice9* d,const MainPopupStateV1& s){
  Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};
  if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
  for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
  HWND w=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(w,&pid);if(pid!=GetCurrentProcessId()||IsIconic(w))return false;
  const unsigned held=Keys();bool changed=false;if(!visible||generation!=s.generation||owner!=d){visible=true;generation=s.generation;owner=d;selected=s.type==1?1:0;pressed=builtSelected=~0u;keys.Reset(held);changed=true;}
  const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT pointer{};const bool mouse=ui_pointer::ReadClient(w,client,pointer);float x{},y{};
  if(mouse){x=(pointer.x*static_cast<float>(desc.Width)/client.right-canvas.x)/canvas.scale;y=(pointer.y*static_cast<float>(desc.Height)/client.bottom-canvas.y)/canvas.scale;}
  const auto hit=[&](){if(mouse)for(unsigned n=s.type==1?1:0;n<2;++n)if(main_popup_art::Button(static_cast<MainPopupKind>(s.type),n).Contains(x,y))return n;return ~0u;};
  const auto edge=keys.Read(held);DWORD action=~0u;
  if(!(s.flags&MainPopupBusy)){
   if(s.type!=1&&(edge.pressed&(8|16)))selected=1-selected;
   if((edge.pressed&1))pressed=hit();
   if((edge.released&1)&&hit()==pressed&&pressed<=1){selected=pressed;action=pressed;pressed=~0u;}
   if(edge.released&2)action=selected;
   if(edge.released&4)action=s.type==1?1:0;
  }else{keys.Reset(held);pressed=~0u;}
  if(changed||builtSelected!=selected||cached.type!=s.type||cached.flags!=s.flags||memcmp(cached.title,s.title,sizeof s-32)){
   ui_art::Scene next;if(FAILED(main_popup_art::Build(s,selected,next)))return false;art=std::move(next);cached=s;builtSelected=selected;
  }
  auto frame=art;ui_pointer::AppendHiddenCursor(frame,mouse,x,y);if(!ui_art::Draw(d,frame,assets,canvas.scale,canvas.x,canvas.y))return false;
  if(action<=1&&sender(action,s.generation))ui_sound::Emit(action?ui_sound::Cue::Confirm:ui_sound::Cue::Back);
  return true;
 }
};
static Presentation* presentation=nullptr;
static void Invalidate(IDirect3DDevice9* d){if(drawing||!presentation||presentation->owner!=d)return;presentation->Hide();if(setter)setter(FALSE,readyGeneration);}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;MainPopupStateV1 s{};bool ready=false;
 try{
  const auto now=GetTickCount();if((!getter||!sender||!setter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){
   lookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<MainPopupGetter>(GetProcAddress(input,"AITDGetMainPopupState"));sender=reinterpret_cast<MainPopupSender>(GetProcAddress(input,"AITDQueueMainPopupResponse"));setter=reinterpret_cast<MainPopupReady>(GetProcAddress(input,"AITDSetMainPopupReady"));}
  }
  if(!source&&!destination&&getter&&sender&&setter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&MainPopupVisible)&&now-s.tick<=250){if(!presentation)presentation=new Presentation;ready=presentation->Draw(d,s);}else if(presentation)presentation->Hide();
 }catch(...){ready=false;}
 const DWORD status=(getter?1u:0u)|(sender?2u:0u)|(setter?4u:0u)|(ready?8u:0u)|(s.flags<<8)|(s.type<<16);if(lastStatus!=status){lastStatus=status;Log("Main popup UI status=%08lx generation=%lu sent=%lu rejected=%lu",status,s.generation,s.dispatches,s.rejected);}
 if(ready)readyGeneration=s.generation;else if(presentation)presentation->Hide();if(setter)setter(ready?TRUE:FALSE,readyGeneration);
}
}

