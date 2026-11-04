#include "controls_state.h"
#include "menu_presentation_policy.h"
#include "ui_pointer.h"
#include "ui_pointer_art.h"
namespace controls_overlay {
using Action=controls_art::Action;using Phase=controls_art::Phase;
static bool drawing=false;static ControlsGetter getter=nullptr;static ControlsSender sender=nullptr;static ControlsReady readySetter=nullptr;static DWORD lookup{},readyGeneration{},lastStatus=~0u;static HRESULT buildResult=S_OK;
static ControlsWheel wheelGetter=nullptr;
struct Presentation{
 IDirect3DDevice9* owner{};ui_art::Assets assets;ui_art::Scene built;controls_art::Layout layout;controls_art::View cached{};
 menu_presentation_policy::KeyEdges keys;DWORD generation{},repeatAt{},deferredTick{};unsigned repeat{};LONG wheelRemainder{};controls_art::Hit pressed{},deferred{};bool visible=false,mouseSeen=false;POINT lastMouse{};
 static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_PRIOR,VK_NEXT,VK_TAB,VK_SHIFT,'X'};unsigned k=0;for(unsigned n=0;n<12;++n)if(GetAsyncKeyState(codes[n])&0x8000)k|=1u<<n;return k;}
 void Hide(){visible=mouseSeen=false;pressed=deferred={};repeat=0;wheelRemainder=0;keys.Reset(Keys());owner=nullptr;}
 bool Send(Action a,LONG row,DWORD epoch,double value=0){if(a==Action::None||!sender)return false;if(a==Action::Back||a==Action::Confirm||a==Action::Cancel||a==Action::Reset)row=-1;const bool sent=sender(static_cast<DWORD>(a),row,value,epoch)!=FALSE;if(sent)ui_sound::Emit(a==Action::Back||a==Action::Cancel?ui_sound::Cue::Back:a==Action::Focus||a==Action::MovePrevious||a==Action::MoveNext?ui_sound::Cue::Hover:ui_sound::Cue::Confirm);return sent;}
 bool Draw(IDirect3DDevice9* d,const ControlsStateV1& s){
  Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};
  if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
  for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
  HWND w=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(w,&pid);if(pid!=GetCurrentProcessId()||IsIconic(w))return false;
  const unsigned held=Keys();bool changed=false;
  if(!visible||generation!=s.generation||owner!=d){keys.Reset(held);repeat=0;wheelRemainder=0;pressed=deferred={};visible=true;generation=s.generation;owner=d;changed=true;}
  if(changed||memcmp(&cached,&s.view,sizeof cached)){ui_art::Scene next;controls_art::Layout hit;buildResult=controls_art::Build(s.view,next,hit);if(FAILED(buildResult))return false;built=std::move(next);layout=hit;cached=s.view;}
  const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT pointer{};const bool mouse=ui_pointer::ReadClient(w,client,pointer);
  float x{},y{};if(mouse){x=(pointer.x*static_cast<float>(desc.Width)/client.right-canvas.x)/canvas.scale;y=(pointer.y*static_cast<float>(desc.Height)/client.bottom-canvas.y)/canvas.scale;}
  const bool moved=mouse&&mouseSeen&&(pointer.x!=lastMouse.x||pointer.y!=lastMouse.y);lastMouse=pointer;mouseSeen=mouse;
  const auto edge=keys.Read(held);const auto hit=mouse?controls_art::HitTest(layout,x,y):controls_art::Hit{};const auto now=GetTickCount();unsigned pulse=edge.pressed&(8|16|32|64|128|256);
  if(pulse){repeat=pulse&(~pulse+1u);repeatAt=now+320;}else if(repeat&&(held&repeat)&&static_cast<LONG>(now-repeatAt)>=0){pulse=repeat;repeatAt=now+70;}else if(!(held&repeat))repeat=0;
  Action action=Action::None;LONG row=-1;double normalized=0;LONG wheelStep=0;
  const bool modal=s.view.phase>=Phase::ReleaseKeys;
  if(!(s.flags&ControlsBusy)&&!modal){
   if(s.view.phase==Phase::Bindings&&wheelGetter)wheelRemainder=static_cast<LONG>((std::max)(-3840LL,(std::min)(3840LL,static_cast<long long>(wheelRemainder)+wheelGetter(s.generation))));else wheelRemainder=0;
   if(deferred.action!=Action::None){
    if(now-deferredTick>1200)deferred={};
    else if(static_cast<LONG>(s.view.selected)==deferred.row){action=deferred.action;row=deferred.row;normalized=deferred.normalized;deferred={};}
   }
   if(action==Action::None){
    if(edge.pressed&1)pressed=hit;
    if((edge.released&1)&&hit.action==pressed.action&&hit.row==pressed.row){action=hit.action;row=hit.row;normalized=hit.normalized;pressed={};
     if(s.view.phase==Phase::Settings&&(action==Action::Previous||action==Action::Next||action==Action::Gauge)&&row!=static_cast<LONG>(s.view.selected)){deferred=hit;deferredTick=now;action=Action::Focus;}}
    if(edge.released&4)action=Action::Cancel;
    else if(edge.released&512){action=Action::Category;row=static_cast<LONG>((s.view.category+((held&1024)?3:1))%4);}
    else if(edge.released&2048)action=Action::Reset;
    else if(edge.released&2){action=s.view.phase==Phase::Bindings?Action::Activate:Action::Confirm;row=s.view.phase==Phase::Bindings?static_cast<LONG>(s.view.selected):-1;}
    else if(pulse&(8|16)){
     if(s.view.phase==Phase::Bindings){action=pulse&8?Action::MovePrevious:Action::MoveNext;row=-1;}
     else{const int step=pulse&8?-1:1;LONG next=static_cast<LONG>(s.view.selected);for(unsigned n=0;n<s.view.count;++n){next=(next+step+static_cast<LONG>(s.view.count))%static_cast<LONG>(s.view.count);if(!s.view.rows[next].locked)break;}action=Action::Focus;row=next;}
    }else if(pulse&(128|256)){action=pulse&128?Action::PagePrevious:Action::PageNext;row=-1;}
    else if((pulse&(32|64))&&s.view.phase==Phase::Settings){action=pulse&32?Action::Previous:Action::Next;row=static_cast<LONG>(s.view.selected);}
    else if(action==Action::None&&(wheelRemainder>=120||wheelRemainder<=-120)){wheelStep=wheelRemainder>0?120:-120;action=wheelStep>0?Action::MovePrevious:Action::MoveNext;row=-1;}
    else if(action==Action::None&&moved&&!(held&1)&&hit.action==Action::Activate&&hit.row>=0&&hit.row!=static_cast<LONG>(s.view.selected)){action=Action::Focus;row=hit.row;}
   }
  }else{pressed={};repeat=0;wheelRemainder=0;deferred={};keys.Reset(held);}
  if(!menu_overlay::DrawBackdrop(d))return false;
  auto scene=built;ui_pointer::AppendHiddenCursor(scene,mouse,x,y);if(!ui_art::Draw(d,scene,assets,canvas.scale,canvas.x,canvas.y))return false;
  if(action!=Action::None&&Send(action,row,s.generation,normalized)&&wheelStep)wheelRemainder-=wheelStep;return true;
 }
};
static Presentation* presentation=nullptr;
static void Invalidate(IDirect3DDevice9* d){if(drawing||!presentation||presentation->owner!=d)return;presentation->Hide();if(readySetter)readySetter(FALSE,readyGeneration);}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;bool ready=false;ControlsStateV1 s{};
 try{
  const auto now=GetTickCount();if((!getter||!sender||!readySetter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<ControlsGetter>(GetProcAddress(input,"AITDGetControlsState"));sender=reinterpret_cast<ControlsSender>(GetProcAddress(input,"AITDQueueControlsAction"));readySetter=reinterpret_cast<ControlsReady>(GetProcAddress(input,"AITDSetControlsReady"));wheelGetter=reinterpret_cast<ControlsWheel>(GetProcAddress(input,"AITDConsumeControlsWheel"));}}
  if(!source&&!destination&&getter&&sender&&readySetter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&(ControlsVisible|ControlsActionsProven))==(ControlsVisible|ControlsActionsProven)&&now-s.tick<=250){if(!presentation)presentation=new Presentation;ready=presentation->Draw(d,s);}else if(presentation)presentation->Hide();
 }catch(...){ready=false;}
 const DWORD status=(getter?1u:0u)|(sender?2u:0u)|(readySetter?4u:0u)|(ready?8u:0u)|(s.flags<<8)|(static_cast<DWORD>(s.view.phase)<<16);if(lastStatus!=status){lastStatus=status;Log("Controls UI status=%08lx generation=%lu category=%u count=%u selected=%u build=%08lx sent=%lu rejected=%lu",status,s.generation,s.view.category,s.view.count,s.view.selected,static_cast<unsigned long>(buildResult),s.dispatches,s.rejected);}
 if(ready)readyGeneration=s.generation;else if(presentation)presentation->Hide();if(readySetter)readySetter(ready?TRUE:FALSE,readyGeneration);
}
}

