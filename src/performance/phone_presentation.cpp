#define NOMINMAX
#include "ui_pointer.h"
#include "ui_pointer_art.h"
#include "phone_presentation.h"
#include "phone_art.h"
#include "menu_presentation_policy.h"
#include "ui_sound.h"
#include <wincodec.h>
#include <cstring>
namespace phone_presentation {
bool LoadUiImage(IDirect3DDevice9* device,ui_art::Assets& assets,const wchar_t* name,unsigned expectedWidth,unsigned expectedHeight,unsigned slot){
 wchar_t path[MAX_PATH]{};const auto n=GetModuleFileNameW(nullptr,path,MAX_PATH);if(!n||n>=MAX_PATH)return false;
 auto* slash=wcsrchr(path,L'\\');if(!slash)return false;slash[1]=0;
 if(wcscat_s(path,L"assets\\ui\\")||wcscat_s(path,name))return false;
 const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)return false;
 struct Scope{HRESULT result;~Scope(){if(SUCCEEDED(result))CoUninitialize();}} scope{init};
 using Microsoft::WRL::ComPtr;ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
 HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
 if(SUCCEEDED(hr))hr=factory->CreateDecoderFromFilename(path,nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
 if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);UINT width{},height{};if(SUCCEEDED(hr))hr=frame->GetSize(&width,&height);
 if(FAILED(hr)||width!=expectedWidth||height!=expectedHeight)return false;
 hr=factory->CreateFormatConverter(&converter);if(SUCCEEDED(hr))hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
 ComPtr<IDirect3DTexture9> texture;if(SUCCEEDED(hr))hr=device->CreateTexture(width,height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr);
 D3DLOCKED_RECT lock{};if(SUCCEEDED(hr))hr=texture->LockRect(0,&lock,nullptr,0);if(FAILED(hr))return false;
 if(lock.Pitch<static_cast<INT>(width*4)){texture->UnlockRect(0);return false;}
 hr=converter->CopyPixels(nullptr,static_cast<UINT>(lock.Pitch),static_cast<UINT>(lock.Pitch)*height,static_cast<BYTE*>(lock.pBits));texture->UnlockRect(0);
 return SUCCEEDED(hr)&&SUCCEEDED(assets.SetImage(device,slot,texture.Get()));
}
struct Presentation::Impl {
 phone_art::Labels labels;PhoneStateV1 cached{};ui_art::Scene choices[6];bool built[6]{};ui_art::Assets assets;IDirect3DDevice9* owner{};
 DWORD generation{};bool visible=false,haveMouse=false,iconImages=false;unsigned selected=0,pressed=~0u;POINT previous{};menu_presentation_policy::KeyEdges keys;
 static unsigned Keys(){const int codes[]={VK_LBUTTON,VK_RETURN,VK_ESCAPE,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_TAB};unsigned held=0;for(unsigned n=0;n<8;++n)if(GetAsyncKeyState(codes[n])&0x8000)held|=1u<<n;return held;}
 void Hide(){visible=false;haveMouse=false;pressed=~0u;keys.Reset(Keys());}
 void InvalidateArt(){for(auto& ready:built)ready=false;}
 bool Draw(IDirect3DDevice9* d,const PhoneStateV1& s,PhoneSender send){
  if(!d||!send||s.size!=sizeof s||s.version!=1||s.count==0||s.count>6||!(s.flags&PhoneHome)||GetTickCount()-s.tick>250)return false;
  Microsoft::WRL::ComPtr<IDirect3DSurface9> back,target,mrt;D3DSURFACE_DESC desc{};
  if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(d->GetRenderTarget(0,&target))||back.Get()!=target.Get()||FAILED(back->GetDesc(&desc))||desc.Width<320||desc.Height<200)return false;
  for(DWORD n=1;n<4;++n){mrt.Reset();if(SUCCEEDED(d->GetRenderTarget(n,&mrt))&&mrt)return false;}
  DWORD pid{};const auto window=GetForegroundWindow();if(!window||!GetWindowThreadProcessId(window,&pid)||pid!=GetCurrentProcessId()||IsIconic(window)){Hide();return false;}
  if(labels.title.mesh.triangles.empty()||memcmp(cached.title,s.title,sizeof s-sizeof(DWORD)*10)){
   if(FAILED(phone_art::Prepare(s,labels)))return false;cached=s;InvalidateArt();
  }
  if(owner!=d){owner=d;Hide();assets.Reset();iconImages=LoadUiImage(d,assets,L"phone_reference_icons.png",900,180);InvalidateArt();}
  const unsigned held=Keys(),before=selected;const bool wasVisible=visible;
  if(!visible||generation!=s.generation){Hide();generation=s.generation;visible=true;selected=s.selected;keys.Reset(held);}
  if(cached.flags!=s.flags||cached.count!=s.count){cached.flags=s.flags;cached.count=s.count;InvalidateArt();}
  if(selected>=s.count)selected=0;
  const menu_presentation_policy::Canvas canvas(desc.Width,desc.Height,1.f);RECT client{};POINT pointer{};
  const bool mouse=ui_pointer::ReadClient(window,client,pointer);
  ui_theme::Point pos{};unsigned hit=~0u;
  if(mouse){pos={((pointer.x*static_cast<float>(desc.Width)/client.right)-canvas.x)/canvas.scale,((pointer.y*static_cast<float>(desc.Height)/client.bottom)-canvas.y)/canvas.scale};hit=phone_art::Hit(s,pos.x,pos.y);
   if(haveMouse&&(pointer.x!=previous.x||pointer.y!=previous.y)&&hit<s.count&&selected!=hit)selected=hit;previous=pointer;haveMouse=true;}
  const auto edges=keys.Read(held);unsigned action=0;
  if(!(s.flags&PhoneBusy)){
   if(edges.pressed&((1u<<3)|(1u<<4)|(1u<<5)|(1u<<6)))selected=phone_art::Navigate(s,selected,(edges.pressed&(1u<<5))?-1:(edges.pressed&(1u<<6))?1:0,(edges.pressed&(1u<<3))?-1:(edges.pressed&(1u<<4))?1:0);
   if(edges.pressed&(1u<<7)){for(unsigned n=0;n<s.count;++n){selected=(selected+1)%s.count;if(s.items[selected].enabled)break;}}
   if(edges.pressed&1)pressed=hit;
   if(edges.released&1){if(hit<s.count&&hit==pressed)action=s.items[hit].nativeSlot;pressed=~0u;}
   if((edges.released&(1u<<1))&&selected<s.count&&s.items[selected].enabled)action=s.items[selected].nativeSlot;
   if(edges.released&(1u<<2))action=PhoneClose;
  }else{pressed=~0u;keys.Reset(held);}
  if(selected>=s.count)return false;
  if(!built[selected]){phone_art::Build(s,labels,selected,choices[selected],iconImages);built[selected]=true;}
  ui_art::Scene frame;const ui_art::Scene* draw=&choices[selected];
  if(!ui_pointer::centralCursor){frame=*draw;ui_pointer::AppendHiddenCursor(frame,mouse,pos.x,pos.y);draw=&frame;}
  // Phone is vector-only, uses no root/slate textures, and never captures or
  // clears the background. Transparent areas reveal the actual running game.
  if(!ui_art::Draw(d,*draw,assets,canvas.scale,canvas.x,canvas.y))return false;
  if(action){if(send(action,s.generation))ui_sound::Emit(action==PhoneClose?ui_sound::Cue::Back:ui_sound::Cue::Confirm);}
  else if(wasVisible&&selected!=before)ui_sound::Emit(ui_sound::Cue::Hover);
  return true;
 }
};
Presentation::Presentation():impl(new Impl){}Presentation::~Presentation(){delete impl;}
bool Presentation::Draw(IDirect3DDevice9* d,const PhoneStateV1& s,PhoneSender send)noexcept{try{if(impl->Draw(d,s,send))return true;}catch(...){}impl->Hide();return false;}
void Presentation::ReleaseDeviceResources()noexcept{impl->owner=nullptr;impl->Hide();}
bool Presentation::OwnsDevice(IDirect3DDevice9* d)const noexcept{return impl->owner==d;}
}
