#include "phone_content_art.h"
#include "phone_status_art.h"
#include "menu_presentation_policy.h"
#include "ui_pointer.h"
#include "ui_pointer_art.h"
namespace phone_content_overlay {
static bool drawing=false;
static PhoneContentGetter getter=nullptr;static PhoneContentSender sender=nullptr;static PhoneContentReady readySetter=nullptr;
static PhoneContentWheel wheelGetter=nullptr;
static DWORD lookup{},readyGeneration{};
struct Presentation{
 IDirect3DDevice9* owner{};ui_art::Assets assets;PhoneContentView cached{};phone_content_art::Prepared art;ui_art::Scene built;
 menu_presentation_policy::KeyEdges keys;DWORD generation{},repeatAt{},builtStatus=~0u;unsigned repeat{},pressed=~0u;LONG wheelRemainder{};float scroll{},builtScroll=-1;bool visible=false,mouseSeen=false;POINT lastMouse{};
 static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_PRIOR,VK_NEXT,VK_BACK};unsigned k=0;for(unsigned n=0;n<10;++n)if(GetAsyncKeyState(codes[n])&0x8000)k|=1u<<n;for(unsigned n=0;n<10;++n)if((GetAsyncKeyState('0'+n)|GetAsyncKeyState(VK_NUMPAD0+n))&0x8000)k|=1u<<(10+n);return k;}
 void Hide(){visible=false;mouseSeen=false;pressed=~0u;repeat=0;wheelRemainder=0;keys.Reset(Keys());owner=nullptr;}
 bool Draw(IDirect3DDevice9* d,const PhoneContentStateV1& s){
  Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};
  if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
  for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
  HWND w=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(w,&pid);if(pid!=GetCurrentProcessId()||IsIconic(w))return false;
  const unsigned held=Keys();bool changed=false;
  if(!visible||generation!=s.generation||owner!=d){keys.Reset(held);repeat=0;wheelRemainder=0;scroll=0;builtScroll=-1;pressed=~0u;visible=true;generation=s.generation;owner=d;changed=true;}
  if(changed||memcmp(&cached,&s.view,sizeof cached)){phone_content_art::Prepared next;if(FAILED(phone_content_art::Prepare(s.view,next)))return false;if(memcmp(cached.body,s.view.body,sizeof cached.body)||memcmp(cached.subtitle,s.view.subtitle,sizeof cached.subtitle))scroll=0;art=std::move(next);cached=s.view;changed=true;}
  const bool map=s.view.kind==PhoneContentKind::Gps;
  const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,map?1.f:.78f);RECT client{};POINT pointer{};const bool mouse=ui_pointer::ReadClient(w,client,pointer);
  float x{},y{};if(mouse){x=(pointer.x*static_cast<float>(desc.Width)/client.right-canvas.x)/canvas.scale;y=(pointer.y*static_cast<float>(desc.Height)/client.bottom-canvas.y)/canvas.scale;}
  const bool moved=mouse&&mouseSeen&&(pointer.x!=lastMouse.x||pointer.y!=lastMouse.y);lastMouse=pointer;mouseSeen=mouse;
  const auto edge=keys.Read(held);const bool list=s.view.kind==PhoneContentKind::List,dial=s.view.kind==PhoneContentKind::Dialer;DWORD action{},row=s.view.selected;
  const auto hit=[&](){if(ui_art::Bounds{100,968,400,60}.Contains(x,y))return 100u;if(list&&s.view.count&&ui_art::Bounds{550,968,420,60}.Contains(x,y))return 101u;
   if(dial){for(unsigned n=0;n<12;++n)if(phone_content_art::DialKey(n).Contains(x,y))return 200u+n;if(ui_art::Bounds{550,968,550,60}.Contains(x,y))return 209u;if(ui_art::Bounds{1170,968,550,60}.Contains(x,y))return 211u;}
   if(s.view.total&&(!list||s.view.total>10)){if(ui_art::Bounds{1000,968,170,60}.Contains(x,y))return 102u;if(ui_art::Bounds{1190,968,170,60}.Contains(x,y))return 103u;}
   if(list)for(unsigned n=0;n<s.view.count;++n)if(phone_content_art::Row(n).Contains(x,y))return n;return ~0u;};
  const auto h=mouse?hit():~0u;const auto now=GetTickCount();unsigned pulse=edge.pressed&0x1f8u;
  if(pulse){repeat=pulse&(~pulse+1u);repeatAt=now+320;}else if(repeat&&(held&repeat)&&static_cast<LONG>(now-repeatAt)>=0){pulse=repeat;repeatAt=now+70;}else if(!(held&repeat))repeat=0;
  if(!(s.flags&PhoneContentBusy)){
   const LONG wheel=wheelGetter?wheelGetter(s.generation):0;
   if(list)wheelRemainder=(std::max)(-3840L,(std::min)(3840L,wheelRemainder+wheel));
   else if(s.view.kind==PhoneContentKind::Reader&&wheel)scroll=(std::max)(0.f,(std::min)(scroll-wheel,(std::max)(0.f,art.bodyHeight-540.f)));
   if(edge.pressed&1)pressed=h;
   if((edge.released&1)&&h==pressed){if(h<s.view.count&&list){action=2;row=h;}else if(h==100)action=3;else if(h==101)action=2;else if(h==102||h==103)action=h==102?4u:5u;else if(dial&&h>=200&&h<212){action=h==209?7u:h==211?8u:6u;row=h==210?0:h-199;}pressed=~0u;}
   // The existing authenticated phone adapter already owns native GPS Esc.
   // Keep the native map's keyboard/mouse drag and zoom lifecycle untouched.
   if((edge.released&4)&&!map)action=3;
   if(dial){for(unsigned n=0;n<10;++n)if(edge.released&(1u<<(10+n))){action=6;row=n;}if(edge.released&512)action=7;if(edge.released&2)action=8;}
   else if(list){
    if(edge.released&2&&s.view.count)action=2;
    if((pulse&(8|16))&&s.view.count){const bool up=(pulse&8)!=0;if(up&&row)row--;else if(!up&&row+1<s.view.count)row++;else action=up?4u:5u;if(!action)action=1;}
    if(pulse&(128|256))action=pulse&128?4u:5u;
    if(!action&&moved&&!(held&1)&&h<s.view.count&&h!=row){action=1;row=h;}
    if(!action&&s.view.count&&(wheelRemainder>=120||wheelRemainder<=-120)){
     const LONG steps=wheelRemainder/120;wheelRemainder%=120;
     const LONG destinationRow=static_cast<LONG>(row)-steps;
     if(destinationRow<0&&s.view.offset)action=4;else if(destinationRow>=static_cast<LONG>(s.view.count)&&s.view.offset+s.view.count<s.view.total)action=5;
     else{row=static_cast<DWORD>((std::max)(0L,(std::min)(static_cast<LONG>(s.view.count)-1,destinationRow)));if(row!=s.view.selected)action=1;}
    }
   }else if(s.view.kind==PhoneContentKind::Reader){
    if(pulse&(8|16|128|256)){scroll+=(pulse&(8|128)?-1.f:1.f)*(pulse&(128|256)?480.f:40.f);scroll=(std::max)(0.f,(std::min)(scroll,(std::max)(0.f,art.bodyHeight-540.f)));}
    if(pulse&(32|64))action=pulse&32?4u:5u;
   }
  }else{pressed=~0u;repeat=0;wheelRemainder=0;keys.Reset(held);}
  const DWORD status=s.flags&(PhoneContentNetworkKnown|PhoneContentNetwork);
  if(changed||scroll!=builtScroll||status!=builtStatus){phone_content_art::Build(s.view,art,scroll,built);if(status&PhoneContentNetworkKnown)phone_status_art::Append(built,(status&PhoneContentNetwork)!=0);builtScroll=scroll;builtStatus=status;}
  ui_art::Scene frame;const ui_art::Scene* draw=&built;
  if(!ui_pointer::centralCursor){frame=built;ui_pointer::AppendHiddenCursor(frame,mouse,x,y);draw=&frame;}
  if(!ui_art::Draw(d,*draw,assets,canvas.scale,canvas.x,canvas.y))return false;
  if(action&&sender(action,row,s.generation))ui_sound::Emit(action==3?ui_sound::Cue::Back:action==1?ui_sound::Cue::Hover:ui_sound::Cue::Confirm);
  return true;
 }
};
static Presentation* presentation=nullptr;
static void Invalidate(IDirect3DDevice9* d){if(drawing||!presentation||presentation->owner!=d)return;presentation->Hide();if(readySetter)readySetter(FALSE,readyGeneration);}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;bool ready=false;PhoneContentStateV1 s{};
 try{
  const auto now=GetTickCount();if((!getter||!sender||!readySetter||!wheelGetter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){
   lookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<PhoneContentGetter>(GetProcAddress(input,"AITDGetPhoneContentState"));sender=reinterpret_cast<PhoneContentSender>(GetProcAddress(input,"AITDQueuePhoneContentAction"));readySetter=reinterpret_cast<PhoneContentReady>(GetProcAddress(input,"AITDSetPhoneContentReady"));wheelGetter=reinterpret_cast<PhoneContentWheel>(GetProcAddress(input,"AITDConsumePhoneWheel"));}
  }
  if(!source&&!destination&&getter&&sender&&readySetter&&getter(&s,sizeof s)&&s.size==sizeof s&&s.version==1&&(s.flags&PhoneContentVisible)&&now-s.tick<=250){if(!presentation)presentation=new Presentation;ready=presentation->Draw(d,s);}
  else if(presentation)presentation->Hide();
 }catch(...){ready=false;}
 if(ready)readyGeneration=s.generation;else if(presentation)presentation->Hide();if(readySetter)readySetter(ready?TRUE:FALSE,readyGeneration);
}
}
