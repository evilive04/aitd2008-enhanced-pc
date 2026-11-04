#define NOMINMAX
#include "healing_ui_art.h"
#include "ui_rich_keycaps.h"
#include <algorithm>
namespace healing_ui_art{
static HRESULT Text(ui_art::Scene& s,const wchar_t* value,float x,float y,float size,float w,float h,D3DCOLOR ink=0xffe9e5dc){ui_art::Scene part;const auto hr=ui_rich_keycaps::Build(value,size,w,h,part,x,y,ink);if(SUCCEEDED(hr))for(auto& b:part.batches)s.batches.push_back(std::move(b));return hr;}
HRESULT Build(const HealingUiStateV2& v,ui_art::Scene& out)noexcept{
 out={};if(v.size!=sizeof v||v.version!=2||!(v.flags&HealingUiVisible)||(v.flags&~3u)||v.selected>16||v.severity>4||v.injuries>64||(v.medicine&&v.medicine!=14&&v.medicine!=15)||((v.flags&HealingUiSelected)&&(!v.severity||v.selected==16)))return E_INVALIDARG;
 for(unsigned n=0;n<8;++n){const auto& k=v.keys[n];if(std::find(k,k+256,L'\0')==k+256||((n<7||v.medicine)&&!k[0]))return E_INVALIDARG;}
 try{ui_art::Scene s;s.Frame(70,70,600,220);auto hr=Text(s,L"WOUND TREATMENT",108,96,35,520,62);s.Line(108,161,632,161,.8f,0x807e888e);
  const wchar_t* names[]={L"",L"Injury",L"Flesh wound",L"Serious wound",L"Open wound"};
  if(SUCCEEDED(hr))hr=Text(s,(v.flags&HealingUiSelected)?names[v.severity]:L"No active wound selected",108,177,28,520,60,0xffe2b66f);
  wchar_t count[80]{};swprintf_s(count,L"Active wounds  %lu",v.injuries);if(SUCCEEDED(hr))hr=Text(s,count,108,236,23,520,42,0xffb7bec1);
  s.Frame(1320,80,530,v.medicine?250.f:180.f);if(SUCCEEDED(hr))hr=Text(s,L"TREATMENT",1352,99,20,465,40,0xffb7bec1);
  if(SUCCEEDED(hr))hr=Text(s,v.medicine==14?L"Medical spray":v.medicine==15?L"Bandage":L"No treatment equipped",1352,143,28,465,60);
  if(v.medicine){
   s.Line(1352,213,1818,213,.8f,0x807e888e);
   if(SUCCEEDED(hr))hr=Text(s,L"Use held treatment",1352,231,23,465,40,0xffb7bec1);
   if(SUCCEEDED(hr))hr=Text(s,v.keys[7],1352,276,26,465,48,0xffe2b66f);
  }else if(SUCCEEDED(hr))hr=Text(s,L"Equip medical supplies from inventory",1352,198,21,465,56,0xffb7bec1);
  s.Frame(70,912,1780,130);if(SUCCEEDED(hr))hr=Text(s,L"Select wound",108,929,23,300,44,0xffb7bec1);
  for(unsigned n=0;n<4&&SUCCEEDED(hr);++n)hr=Text(s,v.keys[n],108+n*190.f,975,25,180,48);
  if(SUCCEEDED(hr))hr=Text(s,L"Other side",930,929,23,420,44,0xffb7bec1);
  for(unsigned n=4;n<6&&SUCCEEDED(hr);++n)hr=Text(s,v.keys[n],930+(n-4)*190.f,975,25,180,48);
  if(SUCCEEDED(hr))hr=Text(s,L"Back",1540,929,23,265,44,0xffb7bec1);if(SUCCEEDED(hr))hr=Text(s,v.keys[6],1540,975,25,270,48);
  if(FAILED(hr))return hr;out=std::move(s);return S_OK;
 }catch(...){return E_FAIL;}
}
}
