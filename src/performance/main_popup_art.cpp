#define NOMINMAX
#include "main_popup_art.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
namespace main_popup_art {
namespace {
constexpr D3DCOLOR Ink=0xffe9e5dc,Muted=0xff818b92;
template<size_t N>bool Text(const wchar_t(&v)[N]){return std::find(v,v+N,L'\0')!=v+N;}
HRESULT Label(ui_art::Scene& scene,const wchar_t* text,float size,float min,float x,float y,float w,float h,D3DCOLOR color,bool wrap=false){
 for(float actual=size;;actual=(std::max)(min,actual-2.f)){
  ui_theme::Label label;const auto hr=ui_theme::BuildLabel(text,actual,w,h,label,L"Times New Roman");if(FAILED(hr))return hr;
  auto& m=label.mesh;const float dx=(std::max)(0.f,-m.ink.left),dy=(std::max)(0.f,-m.ink.top);
  if(std::isfinite(m.advanceWidth)&&std::isfinite(m.layoutHeight)&&m.advanceWidth+dx<=w-.8f&&m.layoutHeight+dy<=h-.8f&&m.ink.right+dx<=w-.8f&&m.ink.bottom+dy<=h-.8f&&(wrap||m.lineCount==1)){
   scene.Text(label,x+dx,y+dy,color);return S_OK;
  }if(actual<=min)return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
 }
}
}
ui_art::Bounds Button(MainPopupKind kind,unsigned n)noexcept{return kind==MainPopupKind::Information?ui_art::Bounds{790,699,340,60}:ui_art::Bounds{590+n*400.f,699,340,60};}
HRESULT Build(const MainPopupStateV1& v,unsigned selected,ui_art::Scene& output)noexcept{
 output.batches.clear();if(v.size!=sizeof v||v.version!=1||v.type<1||v.type>3||selected>1||!Text(v.title)||!v.title[0]||!Text(v.body)||!v.body[0]||!Text(v.labels[0])||!Text(v.labels[1])||!v.labels[1][0]||(v.type!=1&&!v.labels[0][0]))return E_INVALIDARG;
 try{
  ui_art::Scene s;s.Rect(0,0,1920,1080,0x60000000);s.Frame(470,300,980,510);
  auto hr=Label(s,v.title,48,30,530,346,860,90,Ink);s.Line(530,448,1390,448,1,0x637e888e);
  if(SUCCEEDED(hr))hr=Label(s,v.body,34,26,530,480,860,185,Ink,true);
  const unsigned first=v.type==1?1:0;
  for(unsigned n=first;n<2&&SUCCEEDED(hr);++n){const auto b=Button(static_cast<MainPopupKind>(v.type),n);s.Button(b.x,b.y,b.w,b.h,n==selected,!(v.flags&MainPopupBusy));hr=Label(s,v.labels[n],32,24,b.x+30,b.y+13,b.w-60,44,(v.flags&MainPopupBusy)?Muted:Ink);}
  s.Button(734,831,120,42,false);s.Button(1064,831,100,42,false);
  if(SUCCEEDED(hr))hr=Label(s,L"Enter",23,20,748,840,100,38,Muted);
  if(SUCCEEDED(hr))hr=Label(s,L"Esc",23,20,1080,840,80,38,Muted);
  if(FAILED(hr))return hr;output=std::move(s);return S_OK;
 }catch(...){output.batches.clear();return E_FAIL;}
}
}

