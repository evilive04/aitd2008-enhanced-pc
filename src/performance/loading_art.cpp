#define NOMINMAX
#include "loading_art.h"
#include "ui_rich_keycaps.h"
#include <algorithm>
#include <cmath>
namespace loading_art {
HRESULT Prepare(const LoadingStateV3& s,Prepared& out)noexcept{
 out={};if(s.size!=sizeof s||s.version!=3||!(s.flags&LoadingVisible)||(s.flags&~(LoadingVisible|LoadingFullCanvas|LoadingInformation))||!s.nativeMode||s.nativeMode>9||s.nativeMode==4||(s.nativeMode==6&&!(s.flags&LoadingFullCanvas))||s.picture>5||(s.nativeMode==9?!s.picture:s.picture))return E_INVALIDARG;
 try{
  if(s.flags&LoadingInformation){
   if(s.flags!=(LoadingVisible|LoadingInformation)||(s.nativeMode<7||s.nativeMode>9)||!s.title[0]||!s.body[0]||
      std::find(s.title,s.title+128,L'\0')==s.title+128||std::find(s.body,s.body+2048,L'\0')==s.body+2048)return E_INVALIDARG;
   Prepared p;p.information=true;
   if(s.picture){
    ui_theme::Label title;auto hr=ui_theme::BuildLabel(s.title,38,1344,100,title,L"Times New Roman");if(FAILED(hr)||title.mesh.layoutHeight>100.125f||title.mesh.ink.right>1344.125f)return FAILED(hr)?hr:E_INVALIDARG;
    const float top=(std::max)(380.f,264+title.mesh.layoutHeight+36);ui_art::Scene body;hr=ui_rich_keycaps::Build(s.body,29,674,360,body,958,top,0xffe9e5dc);if(FAILED(hr))return hr;
    p.picture=s.picture;p.imageBounds={288,top,620,360};p.scene.Frame(240,230,1440,620);p.scene.Text(title,288,264,0xffe9e5dc);p.scene.Line(288,top-20,1632,top-20,1,0x707e888e);
    for(auto& batch:body.batches)p.scene.batches.push_back(std::move(batch));out=std::move(p);return S_OK;
   }
   ui_theme::Label title;auto hr=ui_theme::BuildLabel(s.title,38,1152,100,title,L"Times New Roman");
   if(FAILED(hr)||title.mesh.layoutHeight>100.125f||title.mesh.ink.right>1152.125f)return FAILED(hr)?hr:E_INVALIDARG;
   const float bodyTop=(std::max)(430.f,324+title.mesh.layoutHeight+32);
   ui_art::Scene body;hr=ui_rich_keycaps::Build(s.body,30,1176,364,body,372,bodyTop,0xffe9e5dc);if(FAILED(hr))return hr;
   float bottom=bodyTop;for(const auto& batch:body.batches)for(const auto& v:batch.vertices)bottom=(std::max)(bottom,v.y);
   const float height=(std::max)(264.f,bottom-300+42);
   if(height>536.125f)return E_INVALIDARG;
   p.scene.Frame(330,300,1260,height);p.scene.Line(372,318,372,374,2,0xffe2b66f);p.scene.Text(title,396,324,0xffe9e5dc);
   p.scene.Line(372,bodyTop-20,1548,bodyTop-20,1,0x707e888e);for(auto& batch:body.batches)p.scene.batches.push_back(std::move(batch));out=std::move(p);return S_OK;
  }
  if(s.nativeMode>=7||!s.message[0]||std::find(s.message,s.message+1024,L'\0')==s.message+1024)return E_INVALIDARG;
  Prepared p;p.full=(s.flags&LoadingFullCanvas)!=0;const float x=p.full?90.f:1100.f,y=p.full?820.f:868.f,w=p.full?1740.f:730.f;
  // Rounded smoke geometry has real alpha. The full-canvas backdrop is drawn
  // separately at physical backbuffer size, including letterbox margins.
  p.scene.Frame(x,y,w,p.full?190.f:148.f);ui_theme::Label label;const auto hr=ui_theme::BuildLabel(s.message,p.full?42.f:27.f,w-140,102,label,L"Times New Roman");
  if(FAILED(hr)||label.mesh.layoutHeight>102.125f||label.mesh.ink.right>w-139.875f)return FAILED(hr)?hr:E_INVALIDARG;
  p.scene.Text(label,p.full?148.f:x+94,p.full?850.f:y+(148-label.mesh.layoutHeight)*.5f,0xffe9e5dc);if(p.full)p.scene.Line(148,970,1638,970,1,0x707e888e);out=std::move(p);return S_OK;
 }catch(...){return E_FAIL;}
}
void Build(const Prepared& p,DWORD elapsed,ui_art::Scene& out){
 out=p.scene;if(p.information)return;const float x=p.full?1730.f:1146.f,y=p.full?960.f:942.f;constexpr float pi=3.14159265358979323846f;
 const float angle=(elapsed%1800)*(2*pi/1800.f);
 // Indeterminate activity, never a fabricated completion percentage.
 out.Arc(x,y,18,0,2*pi,1.25f,0x457e888e);out.Arc(x,y,18,angle,angle+pi*.9f,2,0xffe2b66f);
 if(p.full){const float phase=(elapsed%4800)/4800.f;const float t=.5f-.5f*std::cos(phase*2*pi);out.Line(148+t*1290,970,348+t*1290,970,2,0xffe2b66f);}
}
}
