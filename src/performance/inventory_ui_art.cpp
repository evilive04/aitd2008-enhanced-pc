#define NOMINMAX
#include "inventory_ui_art.h"
#include "inventory_icon_layout.h"
#include "ui_rich_keycaps.h"
#include "bleeding_ui_art.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
namespace inventory_ui_art {
template<size_t N>static bool Terminated(const wchar_t(&s)[N]){return std::find(s,s+N,L'\0')!=s+N;}
static HRESULT Text(ui_art::Scene& out,const wchar_t* text,float size,float w,float h,float x,float y,D3DCOLOR color=0xffe9e5dc){ui_art::Scene part;auto hr=ui_rich_keycaps::Build(text,size,w,h,part,x,y,color);if(SUCCEEDED(hr))for(auto& batch:part.batches)out.batches.push_back(std::move(batch));return hr;}
static HRESULT Plain(ui_art::Scene& out,const wchar_t* text,float size,float w,float h,float x,float y,D3DCOLOR color=0xfff2f1eb){ui_vector_text::Options o;o.family=L"Segoe UI";o.bold=true;o.size=size;o.width=w;o.height=h;ui_theme::Label label;const auto hr=ui_theme::BuildRichLabel(text,o,label);if(SUCCEEDED(hr))out.Text(label,x,y,color);return hr;}
static void Plate(ui_art::Scene& s,float x,float y,float w,float h){s.Rect(x,y,w,h,0xd0090909);s.Line(x,y,x+w,y,.8f,0xaab7b4ad);s.Line(x,y,x,y+h,.8f,0xaab7b4ad);s.Line(x+w,y,x+w,y+h,.8f,0xaab7b4ad);s.Line(x,y+h,x+w,y+h,.8f,0xaab7b4ad);}
HRESULT Build(const InventoryUiStateV1& v,ui_art::Scene& out,bool handIcon,bool bulletIcon,bool mouseIcon,unsigned nativeImages)noexcept{
 const DWORD left=(v.flags>>InventoryUiLeftShift)&InventoryUiHandMask,right=(v.flags>>InventoryUiRightShift)&InventoryUiHandMask;
 (void)mouseIcon;(void)handIcon;out={};if(v.size!=sizeof v||v.version!=1||!(v.flags&InventoryUiVisible)||(v.flags&~0x1fffffu)||left>6||right>6||v.ammo>9999||(!(v.flags&InventoryUiHandsKnown)&&(left||right))||((v.flags&(InventoryUiGunItem|InventoryUiBatteryItem|InventoryUiBulletItem))&&!(v.flags&InventoryUiItem))||((v.flags&InventoryUiSelectMouseLeft)&&!v.promptCount)||(v.nativeCase>10&&v.nativeCase!=99)||(v.nativeCase==99&&(!(v.flags&InventoryUiCombining)||(v.flags&InventoryUiItem)||v.promptCount!=1))||v.promptCount>4||v.liquidKind>3||!Terminated(v.item)||!Terminated(v.quantity)||!Terminated(v.bullets)||!Terminated(v.liquid)||!Terminated(v.boxes)||!Terminated(v.instruction)||!Terminated(v.closeKey)||!Terminated(v.closeLabel)||(!(v.flags&InventoryUiTip)&&(!v.closeKey[0]||!v.closeLabel[0]))||((v.flags&InventoryUiTip)&&(v.flags!=(InventoryUiVisible|InventoryUiTip)||!v.instruction[0]||v.promptCount||v.nativeCase||v.closeKey[0]||v.closeLabel[0]))||((v.flags&InventoryUiItem)&&!v.item[0])||((v.flags&InventoryUiCombining)&&!v.instruction[0]))return E_INVALIDARG;
 try{
  if(v.flags&InventoryUiTip){
   ui_art::Scene tip;tip.Frame(260,742,1400,270);auto result=Text(tip,L"FIELD NOTES",29,1300,54,310,766,0xffe2b66f);tip.Line(310,824,1610,824,.8f,0x807e888e);
   if(SUCCEEDED(result))result=Text(tip,v.instruction,29,1300,150,310,846);
   if(SUCCEEDED(result))result=bleeding_ui_art::Append(v.bleeding,tip,745,110);
   if(FAILED(result))return result;out=std::move(tip);return S_OK;
  }
  ui_art::Scene s;HRESULT hr=S_OK;
  // Keep the actual 3D jacket and its objects as the focus, as in the reference.
  // The screen-edge controls are small and retain the real configured key.
  s.Line(360,155,inventory_icon_layout::RimLeft,155,1.f,0x9ae5e1d7);s.Line(inventory_icon_layout::RimRight,155,1627,155,1.f,0x9ae5e1d7);
  const auto originalImage=[&](unsigned image,unsigned slot,float x,float y,float w,float h){const auto& q=v.images[image];const auto at=s.batches.size();const auto fitted=inventory_icon_layout::ImageBox(q,{x,y,w,h});const bool okay=q.bitmap&&s.ImageRegion(slot,q.width,q.height,{q.x,q.y,q.w,q.h},fitted.x,fitted.y,fitted.w,fitted.h);
   if(okay&&image&&q.bitmap==484){const float cx=x+w*.5f,cy=y+h*.5f;for(size_t n=at;n<s.batches.size();++n)for(auto& vtx:s.batches[n].vertices){const float dx=vtx.x-cx,dy=vtx.y-cy;vtx.x=cx-dy;vtx.y=cy+dx;}}
   if(okay&&image==2&&q.bitmap==484){const float cx=x+w*.5f;for(size_t n=at;n<s.batches.size();++n)for(auto& vtx:s.batches[n].vertices)vtx.x=2*cx-vtx.x;}
   if(!okay)hr=E_INVALIDARG;return okay;};
  if((nativeImages&6)||(v.flags&InventoryUiNativeEquipment)){
   std::vector<ui_theme::Point> rim;for(unsigned corner=0;corner<4;++corner){const float cx=corner==0||corner==3?inventory_icon_layout::RimLeft+12:inventory_icon_layout::RimRight-12,cy=corner<2?84.f:232.f;
    const float start=-3.14159265f+corner*1.57079633f;for(unsigned n=0;n<=12;++n){const float a=start+n*1.57079633f/12;rim.push_back({cx+12*std::cos(a),cy+12*std::sin(a)});}}
   // Native variant sprites are painted before this chrome. An opaque plate
   // would erase their liquid/tape/wick/fire layers all over again.
   if(!(v.flags&InventoryUiNativeEquipment))s.Polygon(rim,0xee171410);
   for(size_t n=0;n<rim.size();++n)s.Line(rim[n].x,rim[n].y,rim[(n+1)%rim.size()].x,rim[(n+1)%rim.size()].y,1.4f,0xffbcb4a4);
   for(unsigned hand=0;hand<2;++hand)if(nativeImages&(2u<<hand)){const auto& box=inventory_icon_layout::Equipment[hand];originalImage(hand+1,hand+3,box.x,box.y,box.w,box.h);}
  }

  Plate(s,1627,92,193,101);
  if(v.flags&InventoryUiFavorites){hr=Plain(s,L"FAVORITES",18,145,35,1640,101);if(SUCCEEDED(hr))hr=Plain(s,L"[;]",20,40,35,1770,101,0xffff8b38);}
  if(SUCCEEDED(hr))hr=Text(s,v.closeLabel,19,86,40,1640,147);
  if(SUCCEEDED(hr))hr=Text(s,v.closeKey,19,100,55,1720,140,0xffff8b38);
  // The hovered second item remains relevant during combination selection.
  // Retain its native name, count and contents alongside the operation keys.
  if(v.flags&InventoryUiItem){
   Plate(s,100,92,260,inventory_icon_layout::HoverPanelHeight);if(SUCCEEDED(hr))hr=Plain(s,v.item,20,230,45,112,100);
   if(v.quantity[0]&&SUCCEEDED(hr))hr=Plain(s,v.quantity,23,104,40,280,185);
   const auto& box=inventory_icon_layout::Hover;
   if(nativeImages&1)originalImage(0,1,box.x,box.y,box.w,box.h);
   else if((v.flags&(InventoryUiGunItem|InventoryUiBulletItem))&&bulletIcon)s.Image(1,88,32,box.x,box.y,box.w,box.h);
   // Exact original invntory bitmap60/shape61 flame, mechanically exported by
   // the shared-art pipeline. The flag is the native hovered item bullet_flame,
   // distinct from the loaded-magazine state used by the in-game weapon HUD.
   if(v.flags&InventoryUiFireAmmo)s.ImageRegion(11,32,64,{0,0,27,44},258,185,20,40);
   if((v.flags&InventoryUiBatteryItem)&&!(nativeImages&1)){s.Rect(115,148,63,24,0xffe2d9c0);s.Rect(178,154,6,12,0xffe2d9c0);s.Line(142,153,142,167,2,0xff554739);s.Line(136,160,148,160,2,0xff554739);}
   const wchar_t* details[]={v.bullets,v.liquid,v.boxes};unsigned row=0;for(const auto* value:details)if(value[0]){const float y=inventory_icon_layout::DetailsTop+row*42.f;Plate(s,100,y,210,38);if(SUCCEEDED(hr))hr=Text(s,value,18,180,34,112,y+6);++row;}
  }
  if(v.flags&InventoryUiCombining){
   // The restored Combine + Cancel rows need space below the instruction.
   const float instructionY=std::min(760.f,965.f-v.promptCount*44.f-24.f-125.f);
   s.Frame(70,instructionY,1780,125);if(SUCCEEDED(hr))hr=Text(s,v.instruction,28,1660,90,120,instructionY+22);
  }
  if(v.promptCount){const float panelX=1350,panelY=965-v.promptCount*44.f;Plate(s,panelX,panelY,445,12.f+v.promptCount*44.f);
   for(unsigned n=0;n<v.promptCount&&SUCCEEDED(hr);++n){const auto& p=v.prompts[n];if(!Terminated(p.key)||!Terminated(p.label)||!p.key[0]||!p.label[0])return E_INVALIDARG;const float x=panelX+14,y=panelY+8+n*44.f;
    if(n==0&&(v.flags&InventoryUiSelectMouseLeft)){s.Image(5,65,55,x,y,48,40);hr=S_OK;}else hr=Text(s,p.key,20,160,44,x,y);if(SUCCEEDED(hr))hr=Text(s,p.label,21,250,44,x+170,y);
   }
  }
  if(SUCCEEDED(hr)){const auto bleeding=bleeding_ui_art::Append(v.bleeding,s,880,80);if(FAILED(bleeding))return bleeding;}
  if(FAILED(hr)){
   // Long localized bindings must not erase the entire inventory. Retry in a
   // wider, readable layout; malformed native key markup still fails closed.
   ui_art::Scene safe;Plate(safe,1060,65,790,100);auto retry=Text(safe,v.closeKey,21,440,55,1080,85);
   if(SUCCEEDED(retry))retry=Text(safe,v.closeLabel,21,300,55,1530,85);
   for(unsigned n=0;n<v.promptCount&&SUCCEEDED(retry);++n){const float y=660+n*70.f;Plate(safe,960,y,890,65);retry=Text(safe,v.prompts[n].key,21,440,55,980,y+4);if(SUCCEEDED(retry))retry=Text(safe,v.prompts[n].label,21,390,55,1430,y+4);}
   if(FAILED(retry))return retry;out=std::move(safe);return S_OK;
  }
  out=std::move(s);return S_OK;
 }catch(...){return E_FAIL;}
}
}
