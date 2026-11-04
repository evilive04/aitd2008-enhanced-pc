#include "phone_art.h"
#include "phone_status_art.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
namespace phone_art {
static constexpr D3DCOLOR Ink=0xffe9e5dc,Amber=0xffe2b66f,Muted=0xa17b8389;
static constexpr float Pi=3.14159265358979323846f;
ui_art::Bounds Item(PhoneItemKind kind)noexcept{
 switch(kind){
 case PhoneItemKind::Gps:return {851,258,220,210};
 case PhoneItemKind::Messages:return {1093,455,220,210};
 case PhoneItemKind::Contacts:return {606,455,220,210};
 case PhoneItemKind::Dial:return {687,692,220,210};
 case PhoneItemKind::Settings:return {1012,692,220,210};
 default:return {};
 }
}
unsigned Hit(const PhoneStateV1& s,float x,float y)noexcept{
 if(s.count>6)return ~0u;
 for(unsigned n=0;n<s.count;++n)if(s.items[n].enabled&&Item(static_cast<PhoneItemKind>(s.items[n].kind)).Contains(x,y))return n;
 return ~0u;
}
unsigned Navigate(const PhoneStateV1& s,unsigned selected,int dx,int dy)noexcept{
 if(s.count>6||!s.count)return ~0u;if(selected>=s.count)selected=0;
 const auto a=Item(static_cast<PhoneItemKind>(s.items[selected].kind));const float x=a.x+a.w*.5f,y=a.y+a.h*.5f;float best=1e30f;unsigned result=selected;
 for(unsigned n=0;n<s.count;++n){if(n==selected||!s.items[n].enabled)continue;const auto b=Item(static_cast<PhoneItemKind>(s.items[n].kind));
  const float vx=b.x+b.w*.5f-x,vy=b.y+b.h*.5f-y,along=vx*dx+vy*dy;
  if(along<=1)continue;const float across=std::abs(vx*dy-vy*dx),score=along+across*3.2f;
  if(score<best){best=score;result=n;}
 }return result;
}
static HRESULT Fit(const wchar_t* text,unsigned cap,float initial,float minimum,float width,float height,bool oneLine,ui_theme::Label& out){
 if(!wmemchr(text,0,cap)||!text[0])return E_INVALIDARG;
 for(float size=initial;size>=minimum;size-=2){ui_theme::Label next;ui_vector_text::Options options;options.family=L"Segoe UI";options.size=size;options.width=width;options.height=height;options.bold=true;options.italic=true;auto hr=ui_theme::BuildRichLabel(text,options,next);if(FAILED(hr))return hr;
  if(next.mesh.advanceWidth<=width&&next.mesh.layoutHeight<=height&&(!oneLine||next.mesh.lineCount==1)){out=std::move(next);return S_OK;}}
 return E_INVALIDARG;
}
HRESULT Prepare(const PhoneStateV1& s,Labels& out)noexcept{
 if(s.size!=sizeof s||s.version!=1||!(s.flags&PhoneHome)||s.count!=5)return E_INVALIDARG;
 try{
  Labels l;auto hr=Fit(s.title,128,60,40,920,110,true,l.title);unsigned kinds=0,slots=0;
  for(unsigned n=0;n<s.count&&SUCCEEDED(hr);++n){const auto& item=s.items[n];if(item.kind<1||item.kind>5||item.nativeSlot<1||item.nativeSlot>5||item.enabled>1||(kinds&(1u<<item.kind))||(slots&(1u<<item.nativeSlot)))return E_INVALIDARG;
   kinds|=1u<<item.kind;slots|=1u<<item.nativeSlot;hr=Fit(item.label,128,28,20,220,56,true,l.items[n]);}
  if(SUCCEEDED(hr)&&(s.flags&PhoneObjective))hr=Fit(s.objectiveTitle,96,27,23,320,55,true,l.objectiveTitle);
  if(SUCCEEDED(hr)&&(s.flags&PhoneObjective))hr=Fit(s.objective,768,27,21,320,345,false,l.objective);
  if(SUCCEEDED(hr))hr=Fit(s.closeText,96,22,18,180,42,true,l.close);
  if(SUCCEEDED(hr))hr=Fit(s.openText,96,22,18,180,42,true,l.open);
  if(SUCCEEDED(hr))hr=ui_theme::BuildLabel(L"Esc",21,60,42,l.escape);
  if(SUCCEEDED(hr))hr=ui_theme::BuildLabel(L"Enter",21,75,42,l.enter);
  if(FAILED(hr))return hr;out=std::move(l);return S_OK;
 }catch(...){return E_OUTOFMEMORY;}
}
static void Curve(ui_art::Scene& s,ui_theme::Point a,ui_theme::Point b,ui_theme::Point c,ui_theme::Point d,float x,float y,float scale,D3DCOLOR color){
 auto before=a;for(unsigned n=1;n<=48;++n){const float t=n/48.f,u=1-t;const ui_theme::Point point{u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y};
 s.Line(x+before.x*scale,y+before.y*scale,x+point.x*scale,y+point.y*scale,1.8f,color);before=point;}
}
static void Icon(ui_art::Scene& s,PhoneItemKind kind,float x,float y,D3DCOLOR color){
 const auto line=[&](float ax,float ay,float bx,float by){s.Line(x+ax,y+ay,x+bx,y+by,1.8f,color);};
 if(kind==PhoneItemKind::Messages){line(-33,-24,33,-24);line(33,-24,33,24);line(33,24,-33,24);line(-33,24,-33,-24);line(-33,-24,0,3);line(0,3,33,-24);}
 else if(kind==PhoneItemKind::Contacts){s.Arc(x,y-19,15,0,Pi*2,1.8f,color);Curve(s,{-29,30},{-29,-3},{29,-3},{29,30},x,y,1,color);line(-29,30,29,30);}
 else if(kind==PhoneItemKind::Gps){s.Arc(x,y-11,14,0,Pi*2,1.8f,color);Curve(s,{0,42},{-59,-13},{-27,-49},{0,-43},x,y,1,color);Curve(s,{0,-43},{27,-49},{59,-13},{0,42},x,y,1,color);}
 else if(kind==PhoneItemKind::Settings){for(unsigned n=0;n<24;++n){const float a=Pi*2*n/24,b=Pi*2*(n+1)/24,ra=n%3==0?36.f:29.f,rb=(n+1)%3==0?36.f:29.f;line(std::cos(a)*ra,std::sin(a)*ra,std::cos(b)*rb,std::sin(b)*rb);}s.Arc(x,y,13,0,Pi*2,1.8f,color);}
 else if(kind==PhoneItemKind::Dial){
  const ui_theme::Point points[]={{-27,-36},{-15,-40},{-4,-20},{-13,-10},{-6,4},{8,15},{19,7},{36,21},{30,35},{18,39}};
  for(unsigned n=1;n<10;++n)line(points[n-1].x,points[n-1].y,points[n].x,points[n].y);
  Curve(s,{18,39},{-22,28},{-46,-14},{-27,-36},x,y,1,color);
 }
}
void Build(const PhoneStateV1& s,const Labels& l,unsigned selected,ui_art::Scene& out,bool imageIcons){
 out.batches.clear();
 // Reference coordinates correspond to a 1920x1080 viewport. Transparent
 // panels keep the live scene visible; no artwork is baked into the backdrop.
 out.Rect(82,68,130,100,0xb0141516);out.Line(82,168,1838,168,1,0xffa5a5a2);
 // Polygon uses a triangle fan: build the house from convex pieces so the
 // roof cannot fold across the walls. The doorway stays genuinely open.
 out.Polygon({{113,115},{147,84},{181,115}},Ink);
 out.Rect(122,115,49,13,Ink);out.Rect(122,128,17,20,Ink);out.Rect(155,128,16,20,Ink);
 out.Text(l.title,244,82,Ink);
 out.Rect(82,207,485,64,0xa2141516);if(selected<s.count)out.Text(l.items[selected],126,220,Ink);
 if(s.flags&PhoneObjective){out.Rect(82,290,485,220,0xbb141516);out.Arc(126,327,11,0,Pi*2,3,0xffff8c4c);out.Text(l.objectiveTitle,160,309,Ink);out.Text(l.objective,160,365,Ink);}
 out.Line(1527,205,1527,1015,.8f,0x637c7d73);
 out.Arc(960,585,254,0,Pi*2,2,0x99b8b6b1);
 for(unsigned n=0;n<s.count;++n){const auto& item=s.items[n];const auto kind=static_cast<PhoneItemKind>(item.kind);const auto b=Item(kind);const float cx=b.x+b.w*.5f,cy=b.y+88;
  const auto color=!item.enabled?Muted:n==selected?Amber:Ink;
  if(imageIcons){const unsigned slot=static_cast<unsigned>(kind)-1;out.ImageRegion(0,900,180,{slot*180.f,0,180,180},cx-82,cy-82,164,164);}
  else{std::vector<ui_theme::Point> disc;disc.reserve(96);for(unsigned i=0;i<96;++i){const float a=Pi*2*i/96;disc.push_back({cx+std::cos(a)*82,cy+std::sin(a)*82});}out.Polygon(disc,0xe31a1b1b);}
  if(n==selected&&item.enabled){s.flags&PhoneBusy?out.Arc(cx,cy,82,0,Pi*2,1.f,Muted):out.Arc(cx,cy,82,0,Pi*2,2.f,0xffdedbd5);}
  else out.Arc(cx,cy,82,0,Pi*2,1.2f,item.enabled?0xbfc4c6c3:0x626f777c);
  if(!imageIcons)Icon(out,kind,cx,cy,color);
 }
 // Status is the copied native network flag. No invented battery percentage.
 if(s.flags&PhoneNetworkKnown)phone_status_art::Append(out,(s.flags&PhoneNetwork)!=0,1770,105);
 out.Rect(82,910,340,65,0xaa141516);out.Button(95,922,70,42,false);out.Text(l.escape,106,929,Ink);out.Text(l.close,183,929,Ink);
}
}
