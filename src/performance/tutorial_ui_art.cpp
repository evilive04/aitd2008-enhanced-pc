#define NOMINMAX
#include "tutorial_ui_art.h"
#include "ui_rich_keycaps.h"
#include <algorithm>
namespace tutorial_ui_art{
HRESULT Build(const TutorialUiStateV1& v,ui_art::Scene& out)noexcept{
 out={};if(v.size!=sizeof v||v.version!=1||!(v.flags&TutorialUiVisible)||(v.flags&~7u)||v.phase<1||v.phase>3||v.nativeFrame<2||v.nativeFrame>227||!v.body[0]||std::find(v.body,v.body+2048,L'\0')==v.body+2048)return E_INVALIDARG;
 if((v.flags&TutorialUiBlocking)&&(!v.key[0]||std::find(v.key,v.key+256,L'\0')==v.key+256))return E_INVALIDARG;
 try{
  const bool blocking=(v.flags&TutorialUiBlocking)!=0;const float width=blocking?1060.f:1300.f;ui_art::Scene body;
  auto hr=ui_rich_keycaps::Build(v.body,30,width,310,body,0,0,0xffe9e5dc);if(FAILED(hr))return hr;
  float bottom=0;for(const auto& b:body.batches)for(const auto& p:b.vertices)bottom=(std::max)(bottom,p.y);
  const float height=(std::max)(110.f,bottom+(blocking?176.f:64.f)),x=(1920-width-80)*.5f,y=blocking?(1080-height)*.5f:62.f;
  ui_art::Scene s;if(blocking)s.Rect(0,0,1920,1080,0x30000000);s.Frame(x,y,width+80,height);
  const float bodyY=y+(blocking?90:32);for(auto& b:body.batches){for(auto& p:b.vertices){p.x+=x+40;p.y+=bodyY;}s.batches.push_back(std::move(b));}
  if(blocking){
   ui_theme::Label heading,caption;hr=ui_theme::BuildLabel(L"INSTRUCTIONS",20,width,40,heading,L"Times New Roman");
   if(SUCCEEDED(hr))hr=ui_theme::BuildLabel(L"Continue",26,300,44,caption,L"Times New Roman");if(FAILED(hr))return hr;
   ui_art::Scene key;hr=ui_rich_keycaps::Build(v.key,26,260,52,key,x+40,y+height-65,0xffe9e5dc);if(FAILED(hr))return hr;
   s.Text(heading,x+40,y+28,0xffe2b66f);s.Line(x+40,y+66,x+width+40,y+66,.8f,0x507e888e);
   for(auto& b:key.batches)s.batches.push_back(std::move(b));s.Text(caption,x+330,y+height-59,0xffe9e5dc);
  }
  out=std::move(s);return S_OK;
 }catch(...){return E_FAIL;}
}
void ClosingFade(const TutorialUiStateV1& v,ui_art::Scene& s)noexcept{
 // Own closing fade follows the original known PC/ordinary close spans.
  float alpha=1;if(v.flags&TutorialUiClosing){if(v.nativeFrame>=214)alpha=(227.f-v.nativeFrame)/14.f;else if(v.nativeFrame>=50&&v.nativeFrame<=63)alpha=(63.f-v.nativeFrame)/14.f;}
  alpha=(std::clamp)(alpha,0.f,1.f);if(alpha<1)for(auto& b:s.batches)for(auto& p:b.vertices){const auto a=static_cast<DWORD>((p.color>>24)*alpha);p.color=(p.color&0x00ffffff)|(a<<24);}

}

}
