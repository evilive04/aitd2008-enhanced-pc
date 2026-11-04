#define NOMINMAX
#include "general_hud_art.h"
#include "ui_rich_keycaps.h"
#include <algorithm>
namespace general_hud_art{
HRESULT Build(const GeneralHudStateV1& s,ui_art::Scene& out)noexcept{
 out={};if(s.size!=sizeof s||s.version!=2||!(s.flags&GeneralHudVisible)||!(s.flags&14)||(s.flags&~15u)||s.minutes>999||s.seconds>59)return E_INVALIDARG;
 try{
  ui_art::Scene scene;
  if(s.flags&GeneralHudSubtitle){
   if(!s.subtitle[0]||std::find(s.subtitle,s.subtitle+2048,L'\0')==s.subtitle+2048)return E_INVALIDARG;
   ui_art::Scene text;const auto hr=ui_rich_keycaps::Build(s.subtitle,32,1352,188,text,284,852,0xffe9e5dc);if(FAILED(hr))return hr;
   float bottom=852;for(const auto& b:text.batches)for(const auto& v:b.vertices)bottom=(std::max)(bottom,v.y);
   scene.Frame(250,832,1420,(std::max)(92.f,bottom-832+28));
   for(auto& b:text.batches)scene.batches.push_back(std::move(b));
  }
  if(s.flags&GeneralHudTimer){
   wchar_t time[32]{};swprintf_s(time,L"%02lu:%02lu",s.minutes,s.seconds);ui_theme::Label label;
   const auto hr=ui_theme::BuildLabel(time,39,240,60,label,L"Times New Roman");if(FAILED(hr))return hr;
   scene.Frame(1530,62,300,108);scene.Line(1558,88,1558,144,2,0xffe2b66f);scene.Text(label,1582,84,0xffe9e5dc);
  }
  if(s.flags&GeneralHudCabinetExit){
   if(!s.exitKey[0]||!s.exitLabel[0]||std::find(s.exitKey,s.exitKey+256,L'\0')==s.exitKey+256||std::find(s.exitLabel,s.exitLabel+128,L'\0')==s.exitLabel+128)return E_INVALIDARG;
   wchar_t prompt[448]{};if(swprintf_s(prompt,L"Hold %s  %s",s.exitKey,s.exitLabel)<0)return E_INVALIDARG;
   ui_art::Scene text;const auto hr=ui_rich_keycaps::Build(prompt,26,660,76,text,104,982,0xffe9e5dc);if(FAILED(hr))return hr;
   scene.Frame(80,962,708,86);for(auto& b:text.batches)scene.batches.push_back(std::move(b));
  }
  out=std::move(scene);return S_OK;
 }catch(...){return E_FAIL;}
}
}
