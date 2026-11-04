#define NOMINMAX
#include "bleeding_ui_art.h"
namespace bleeding_ui_art{
bool Valid(const BleedingUiValue& v)noexcept{return v.nativeFrame>=2&&v.nativeFrame<=123&&v.minutes<=999&&v.seconds<=59&&v.stage>=1&&v.stage<=4;}
HRESULT Append(const BleedingUiValue& v,ui_art::Scene& out,float x,float y)noexcept{
 if(!v.nativeFrame)return S_OK;if(!Valid(v))return E_INVALIDARG;
 try{ui_art::Scene s;ui_theme::Label title,time;wchar_t text[24]{};swprintf_s(text,L"%02lu:%02lu",v.minutes,v.seconds);auto hr=ui_theme::BuildLabel(L"BLEEDING",20,220,45,title,L"Times New Roman");if(SUCCEEDED(hr))hr=ui_theme::BuildLabel(text,39,190,65,time,L"Times New Roman");if(FAILED(hr))return hr;
  s.Frame(x,y,430,100);s.Text(title,x+28,y+32,0xffb7bec1);s.Text(time,x+225,y+23,v.stage>=3?0xffe1a786:0xffe2b66f);s.Line(x+205,y+25,x+205,y+75,1,0x607e888e);
  for(auto& b:s.batches)out.batches.push_back(std::move(b));return S_OK;
 }catch(...){return E_FAIL;}
}
HRESULT Build(const BleedingUiStateV1& v,ui_art::Scene& out)noexcept{out={};if(v.size!=sizeof v||v.version!=1||v.flags!=1||!Valid(v.timer))return E_INVALIDARG;return Append(v.timer,out);}
}
