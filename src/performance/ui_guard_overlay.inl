#include "ui_guard_state.h"
#include "ui_pointer_art.h"
namespace ui_guard_overlay {
static UiGuardGetter getter=nullptr;static UiGuardSetter setter=nullptr;static DWORD lookup{};static LONG initialScenes{};static ui_art::Assets assets;
static volatile LONG cursorFrames{},fallbackFrames{},guardFrames{};
static void Begin(){ui_pointer::centralCursor=false;initialScenes=InterlockedCompareExchange(&ui_art::completedScenes,0,0);if(!options_overlay::enabled)return;
 const auto now=GetTickCount();if((!getter||!setter)&&(!lookup||static_cast<LONG>(now-lookup)>=0)){lookup=now+1000;auto module=GetModuleHandleW(L"dinput8.dll");if(module&&FfSameDirectory(module)){getter=reinterpret_cast<UiGuardGetter>(GetProcAddress(module,"AITDGetUiGuardState"));setter=reinterpret_cast<UiGuardSetter>(GetProcAddress(module,"AITDSetUiGuardRenderer"));}}
 UiGuardStateV1 s{};ui_pointer::centralCursor=getter&&setter&&getter(&s,sizeof s)&&s.version==1&&s.size==sizeof s;
}
static void Draw(IDirect3DDevice9* d,const RECT* source,const RECT* destination){
 if(!ui_pointer::centralCursor||source||destination)return;UiGuardStateV1 s{};if(!getter(&s,sizeof s)||s.tick+250<GetTickCount()||s.size!=sizeof s||s.version!=1)return;
 Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return;
 for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return;}
 HWND window=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||IsIconic(window))return;
 bool okay=true;ui_art::Scene scene;const auto before=InterlockedCompareExchange(&ui_art::completedScenes,0,0);const bool fallback=(s.flags&UiGuardActive)&&!(s.flags&(UiGuardTitle|UiGuardMenuPaint))&&before==initialScenes;
 if(fallback){if(s.flags&UiGuardFrontend)okay=menu_overlay::DrawBackdrop(d);/* Keep the real background during handoff; no loading dots. */}
 const float scale=static_cast<float>(desc.Height)/1080.f;RECT client{};POINT point{};const bool mouse=(s.flags&UiGuardPointer)&&ui_pointer::ReadClient(window,client,point);
 const float x=mouse?point.x*static_cast<float>(desc.Width)/client.right/scale:0,y=mouse?point.y*static_cast<float>(desc.Height)/client.bottom/scale:0;
 // Draw exactly once above every menu, even when native SetCursor(NULL) reports
 // CURSOR_SHOWING. No ShowCursor counter changes or warps/clips are performed.
 const auto count=scene.batches.size();const auto vertices=count?scene.batches.back().vertices.size():0;ui_pointer::centralPass=true;ui_pointer::AppendHiddenCursor(scene,mouse,x,y);ui_pointer::centralPass=false;const bool cursor=scene.batches.size()>count||(!scene.batches.empty()&&scene.batches.back().vertices.size()>vertices);
 if(!scene.batches.empty())okay=okay&&ui_art::Draw(d,scene,assets,scale,0,0);
 if(okay){setter(TRUE,1);InterlockedIncrement(&guardFrames);if(cursor)InterlockedIncrement(&cursorFrames);if(fallback)InterlockedIncrement(&fallbackFrames);}
}
}
