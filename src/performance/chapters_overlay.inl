#include "chapters_art.h"
#include "menu_presentation_policy.h"
#include "ui_pointer.h"
#include "ui_pointer_art.h"
namespace chapters_overlay {
static bool drawing=false;static ChaptersGetter getter=nullptr;static ChaptersSender sender=nullptr;static ChaptersReady readySetter=nullptr;static ChaptersPreviewGetter previewGetter=nullptr;static DWORD lookup{},readyGeneration{},lastStatus=~0u;
struct Presentation{
 IDirect3DDevice9* owner{};ui_art::Assets assets;ui_art::Scene art;ChaptersStateV1 cached{};D3DSURFACE_DESC imageDescriptions[5]{};DWORD generation{},fallbackTick{},failedGeneration{},lastClickTick{},lastClickChapter{},imageRefreshTick{};unsigned selectedCard=1,cachedCard=1;LONG lastClickRow=-1;bool visible=false,failed=false;chapters_art::Hit pressed{};menu_presentation_policy::KeyEdges keys;
 static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_PRIOR,VK_NEXT};unsigned mask=0;for(unsigned n=0;n<9;++n)if(GetAsyncKeyState(codes[n])&0x8000)mask|=1u<<n;return mask;}
 void Hide(bool clearFailure=true){visible=false;pressed={};lastClickTick=lastClickChapter=imageRefreshTick=0;lastClickRow=-1;selectedCard=cachedCard=1;keys.Reset(Keys());assets.Reset();art.batches.clear();cached={};owner=nullptr;if(clearFailure){failed=false;fallbackTick=0;}}
 bool Paint(IDirect3DDevice9* device,const menu_presentation_policy::Canvas& canvas,bool mouse,float x,float y){
  if(!menu_overlay::DrawBackdrop(device))return false;
  ui_art::Scene scene=art;
  if(assets.Texture(ui_art::Material::Image1)&&!scene.Image(1,imageDescriptions[1].Width,imageDescriptions[1].Height,1024,570,400,210))return false;
  ui_pointer::AppendHiddenCursor(scene,mouse,x,y);return scene.batches.empty()||ui_art::Draw(device,scene,assets,canvas.scale,canvas.x,canvas.y);
 }
 BOOL Draw(IDirect3DDevice9* device,const ChaptersStateV1& s){
  Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(device->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
  for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(device->GetRenderTarget(n,&mrt))&&mrt)return false;}
  HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
  const unsigned held=Keys();const bool changed=!visible||generation!=s.generation||owner!=device;
  if(generation!=s.generation){failed=false;fallbackTick=0;}if(failed&&failedGeneration==s.generation)return ChaptersUnavailable;
  if(changed){keys.Reset(held);visible=true;pressed={};generation=s.generation;owner=device;}
  const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,.78f);RECT client{};POINT point{};const bool mouse=ui_pointer::ReadClient(window,client,point);float x{},y{};
  if(mouse){x=(point.x*static_cast<float>(desc.Width)/client.right-canvas.x)/canvas.scale;y=(point.y*static_cast<float>(desc.Height)/client.bottom-canvas.y)/canvas.scale;}
  const bool rebuild=changed||memcmp(reinterpret_cast<const BYTE*>(&cached)+12,reinterpret_cast<const BYTE*>(&s)+12,sizeof s-12);ui_art::Scene next;bool complete=!rebuild||SUCCEEDED(chapters_art::Build(s,next,selectedCard));
  const DWORD frameNow=GetTickCount();const bool refresh=rebuild||!imageRefreshTick||frameNow-imageRefreshTick>=250;
  if(refresh&&!(s.flags&ChaptersBusy)){
   Microsoft::WRL::ComPtr<IDirect3DTexture9> image;D3DSURFACE_DESC description{};
   if(previewGetter(1,s.generation,&image)==S_OK&&image&&SUCCEEDED(image->GetLevelDesc(0,&description))&&SUCCEEDED(assets.SetImage(device,1,image.Get())))imageDescriptions[1]=description;
   else if(cached.chapter!=s.chapter){assets.SetImage(device,1,nullptr);imageDescriptions[1]={};}
   imageRefreshTick=frameNow;
  }
  if(rebuild&&complete){art=std::move(next);cached=s;cachedCard=selectedCard;}
  // First entry can lack sequence data for a few UI ticks. Keep ownership
  // without poisoning this generation; original actions remain busy-gated.
  if(!complete)return Paint(device,canvas,mouse,x,y)?ChaptersPaintOnly:ChaptersUnavailable;
  const auto hit=chapters_art::HitTest(s,x,y,selectedCard);const auto edges=keys.Read(held);chapters_art::Hit action{};
  if(!(s.flags&ChaptersBusy)){
   if((edges.pressed&1)&&mouse)pressed=hit;
   if((edges.released&1)&&mouse&&hit.action==pressed.action&&hit.row==pressed.row){
    action=hit;pressed={};if(hit.action==chapters_art::Action::Focus){const DWORD now=GetTickCount();
     if(lastClickTick&&now-lastClickTick<=GetDoubleClickTime()&&lastClickChapter==s.chapter&&lastClickRow==hit.row&&static_cast<DWORD>(hit.row)==s.selected&&!s.rows[hit.row].locked){action.action=chapters_art::Action::Activate;lastClickTick=0;}
     else{lastClickTick=now;lastClickChapter=s.chapter;lastClickRow=hit.row;}
    }else lastClickTick=0;
   }
   using A=chapters_art::Action;
   if(edges.released&4)action={A::Back};else if(edges.released&2)action={A::Activate,static_cast<LONG>(s.selected)};
   else if(edges.pressed&8)action={A::MovePrevious};else if(edges.pressed&16)action={A::MoveNext};
   else if(edges.pressed&32)action={A::PreviousChapter};else if(edges.pressed&64)action={A::NextChapter};
   else if(edges.pressed&128)action={A::PagePrevious};else if(edges.pressed&256)action={A::PageNext};
  }else{keys.Reset(held);pressed={};}
  if(!Paint(device,canvas,mouse,x,y))return ChaptersUnavailable;
  const bool prepared=action.action==chapters_art::Action::Activate&&menu_overlay::PrepareForLoad();
  const bool queued=action.action!=chapters_art::Action::None&&sender(static_cast<DWORD>(action.action),action.row,s.generation);
  if(!queued&&prepared)menu_overlay::CancelPreparedLoad();
  if(queued){
   using A=chapters_art::Action;

   // Native chapter-change timelines already emit the replaced movechap cue.
   // Do not add a second click before that same original sound event.
   if(action.action==A::Back)ui_sound::Emit(ui_sound::Cue::Back);else if(action.action==A::Activate)ui_sound::Emit(ui_sound::Cue::Confirm);else if(action.action!=A::PreviousChapter&&action.action!=A::NextChapter&&action.action!=A::ChapterCard)ui_sound::Emit(ui_sound::Cue::Hover);
  }return (s.flags&ChaptersBusy)?ChaptersPaintOnly:ChaptersInteractive;
 }
};
static Presentation* presentation=nullptr;
static void Invalidate(IDirect3DDevice9* device){if(drawing||!presentation||presentation->owner!=device)return;presentation->Hide();if(readySetter)readySetter(FALSE,readyGeneration);}
static void Draw(IDirect3DDevice9* device,const RECT* source,const RECT* destination){
 if(drawing||!options_overlay::enabled)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;ChaptersStateV1 state{};BOOL ready=ChaptersUnavailable;
 try{
  const auto now=GetTickCount();if((!getter||!sender||!readySetter||!previewGetter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){getter=reinterpret_cast<ChaptersGetter>(GetProcAddress(input,"AITDGetChaptersState"));sender=reinterpret_cast<ChaptersSender>(GetProcAddress(input,"AITDQueueChaptersAction"));readySetter=reinterpret_cast<ChaptersReady>(GetProcAddress(input,"AITDSetChaptersReady"));previewGetter=reinterpret_cast<ChaptersPreviewGetter>(GetProcAddress(input,"AITDAcquireChapterPreview"));}}
  if(!source&&!destination&&getter&&sender&&readySetter&&previewGetter&&getter(&state,sizeof state)&&state.size==sizeof state&&state.version==1&&(state.flags&ChaptersVisible)&&now-state.tick<=250){if(!presentation)presentation=new Presentation;ready=presentation->Draw(device,state);}else if(presentation)presentation->Hide();
 }catch(...){ready=false;}
 const DWORD status=(getter?1u:0u)|(sender?2u:0u)|(readySetter?4u:0u)|(previewGetter?8u:0u)|(ready?16u:0u)|(ready==ChaptersPaintOnly?32u:0u)|(state.flags<<8);if(status!=lastStatus){lastStatus=status;Log("Chapters UI status=%08lx generation=%lu chapter=%lu selected=%lu offset=%lu count=%lu",status,state.generation,state.chapter,state.selected,state.offset,state.count);}
 if(ready)readyGeneration=state.generation;else if(presentation)presentation->Hide(false);if(readySetter)readySetter(ready,readyGeneration);
}
}
