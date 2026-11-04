#define NOMINMAX
#include "inventory_popup_art.h"
#include "ui_rich_keycaps.h"
#include "bleeding_ui_art.h"
#include <algorithm>
#include <cmath>
namespace inventory_popup_art {
template<size_t N>static bool Terminated(const wchar_t(&s)[N]){return std::find(s,s+N,L'\0')!=s+N;}
static bool Valid(const InventoryPopupStateV1& v){
 if((v.bleeding.nativeFrame&&!bleeding_ui_art::Valid(v.bleeding))||v.size!=sizeof v||v.version!=1||!v.kind||v.kind>3||!(v.flags&InventoryPopupVisible)||(v.flags&~15u)||!Terminated(v.title)||!v.title[0]||!Terminated(v.cancel)||!v.cancel[0]||!Terminated(v.confirm)||!v.confirm[0])return false;
 if(v.flags&InventoryPopupNativeAnimation){const auto f=v.nativeFrame;const bool phase=v.kind==3?((f>=125&&f<=206)||(f>=213&&f<=310)):v.kind==2&&f>=78&&f<=123;if(!phase||!(v.flags&InventoryPopupBusy)||(v.flags&InventoryPopupClosing))return false;}
 if(v.flags&InventoryPopupClosing)return v.flags==(InventoryPopupVisible|InventoryPopupBusy|InventoryPopupClosing)&&v.kind==3&&!v.count&&!v.selected&&v.nativeFrame>=48&&v.nativeFrame<=56;
 if(!v.count||v.count>2||v.selected>=v.count||(v.kind==3&&(!(v.flags&InventoryPopupBusy)||v.count!=1))||(v.kind==2&&(v.count!=1||v.amount>v.capacity||v.capacity>10000||v.liquid>1)))return false;
 for(unsigned n=0;n<v.count;++n)if(!Terminated(v.choices[n])||!v.choices[n][0])return false;return true;
}
ui_art::Bounds Row(unsigned n)noexcept{return {520,450+n*122.f,880,108};}
Hit HitTest(const InventoryPopupStateV1& v,float x,float y)noexcept{if(!Valid(v)||(v.flags&InventoryPopupBusy)||!std::isfinite(x)||!std::isfinite(y))return {};if(v.kind==2&&ui_art::Bounds{520,600,880,72}.Contains(x,y)){const float fraction=(std::clamp)((x-540.f)/840.f,0.f,1.f);return {InventoryPopupAction::Amount,static_cast<unsigned>(std::lround(fraction*v.capacity))};}for(unsigned n=0;n<(v.kind==1?v.count:0);++n)if(Row(n).Contains(x,y))return {InventoryPopupAction::Focus,n};if(ui_art::Bounds{520,756,330,56}.Contains(x,y))return {InventoryPopupAction::Cancel,0};if(ui_art::Bounds{1000,756,400,56}.Contains(x,y))return v.kind==2?(v.amount?Hit{InventoryPopupAction::Confirm,v.amount}:Hit{}):Hit{InventoryPopupAction::Confirm,v.selected};return {};}
static HRESULT Text(ui_art::Scene& s,const wchar_t* text,float size,float width,float height,float x,float y,D3DCOLOR color=0xffe9e5dc){ui_art::Scene part;const auto hr=ui_rich_keycaps::Build(text,size,width,height,part,x,y,color);if(SUCCEEDED(hr))for(auto& b:part.batches)s.batches.push_back(std::move(b));return hr;}
HRESULT Build(const InventoryPopupStateV1& v,ui_art::Scene& out)noexcept{
 out={};if(!Valid(v))return E_INVALIDARG;
 try{ui_art::Scene s;
  if(v.flags&InventoryPopupNativeAnimation){
   // The original movie paints the moving items at its own authored positions.
   // Leave that area open: an opaque replacement panel would hide the animation.
   auto hr=Text(s,v.title,48,880,100,520,220);s.Line(520,312,1400,312,.8f,0x9ac4c6c3);
   if(SUCCEEDED(hr))hr=Text(s,v.choices[0],31,880,100,520,850);
   if(SUCCEEDED(hr))hr=bleeding_ui_art::Append(v.bleeding,s,745,145);
   if(FAILED(hr))return hr;out=std::move(s);return S_OK;
  }
  s.Frame(460,280,1000,570);auto hr=Text(s,v.title,48,880,100,520,326);s.Line(520,426,1400,426,.8f,0x9ac4c6c3);
  for(unsigned n=0;n<(v.kind==1?v.count:0)&&SUCCEEDED(hr);++n){const auto r=Row(n);const bool selected=n==v.selected;if(selected){s.Rect(r.x,r.y,2,r.h,0xffe2b66f);s.Rect(r.x+2,r.y,r.w-2,r.h,0x16e2b66f);s.Line(r.x,r.y+r.h,r.x+r.w,r.y+r.h,.8f,0xb0e2b66f);}hr=Text(s,v.choices[n],31,830,94,r.x+22,r.y+12,selected?0xffe2b66f:0xffe9e5dc);}
  if(v.kind==2&&SUCCEEDED(hr)){
   hr=Text(s,v.choices[0],31,880,100,520,450);
   wchar_t quantity[80]{};swprintf_s(quantity,L"%lu / %lu",v.amount,v.capacity);
   if(SUCCEEDED(hr))hr=Text(s,quantity,40,880,68,520,552,0xffe2b66f);
   s.Rect(540,632,840,2,0x60c4c6c3);const float fraction=v.capacity?static_cast<float>(v.amount)/v.capacity:0;
   s.Rect(540,632,840*fraction,2,0xffe2b66f);s.Rect(537+840*fraction,618,6,30,0xffe2b66f);
   if(SUCCEEDED(hr))hr=Text(s,L"Left / Right  ·  Adjust quantity",24,880,44,520,686,0xffc4c6c3);
  }
  if(v.kind==3&&!(v.flags&InventoryPopupClosing)){
   if(SUCCEEDED(hr))hr=Text(s,v.choices[0],31,880,150,520,478);
   s.Line(520,710,1400,710,1,0x407e888e);
  }
  if(v.kind!=3){
  s.Button(520,766,78,40,false);if(SUCCEEDED(hr))hr=Text(s,L"Esc",23,64,40,530,771);if(SUCCEEDED(hr))hr=Text(s,v.cancel,25,248,44,620,770);
  s.Button(1000,766,100,40,false);if(SUCCEEDED(hr))hr=Text(s,L"Enter",23,88,40,1010,771);if(SUCCEEDED(hr))hr=Text(s,v.confirm,25,278,44,1120,770);
  }
  if(SUCCEEDED(hr))hr=bleeding_ui_art::Append(v.bleeding,s,745,145);
  if(FAILED(hr))return hr;out=std::move(s);return S_OK;
 }catch(...){return E_FAIL;}
}
}
