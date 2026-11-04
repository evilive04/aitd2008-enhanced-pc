#define NOMINMAX
#include "options_children_art.h"
#include "phone_roster_content.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <new>
#include <array>
namespace options_children_art {
namespace {
constexpr D3DCOLOR Ink=0xffe9e5dc,Muted=0xff818b92;
template<size_t N> bool TextValid(const wchar_t (&text)[N]){return std::find(text,text+N,L'\0')!=text+N;}
bool Valid(const View& v){
 if(v.version!=1||!TextValid(v.title)||!v.title[0]||!TextValid(v.infoTitle)||!TextValid(v.information))return false;
 const unsigned count=v.page==Page::Audio?3:v.page==Page::Languages?1:v.page==Page::Display?7:v.page==Page::PhoneParameters?4:v.page==Page::PhoneMessages?5:v.page==Page::PhoneContacts?4:0;
 if(!count||v.count!=count||v.selected< -1||v.selected>=static_cast<int>(count))return false;
 for(unsigned n=0;n<count;++n){const auto& r=v.rows[n];if(!TextValid(r.label)||!r.label[0]||!TextValid(r.value)||r.enabled>1||r.locked>1||r.checked>1)return false;
  const bool navigation=v.page==Page::PhoneMessages||v.page==Page::PhoneContacts;
  if((r.kind==Kind::Navigation)!=navigation)return false;
  if(r.kind==Kind::Gauge){if(!std::isfinite(r.current)||!std::isfinite(r.minimum)||!std::isfinite(r.maximum)||r.maximum<=r.minimum||!std::isfinite(r.maximum-r.minimum)||r.current<r.minimum||r.current>r.maximum)return false;}
  else if(r.kind!=Kind::Selector&&r.kind!=Kind::CheckBox&&r.kind!=Kind::Navigation)return false;
 }
 for(const auto& f:v.footer){if(!TextValid(f.label)||!TextValid(f.key)||f.enabled>1)return false;
  if(f.action!=Action::None&&(f.action<Action::Confirm||f.action>Action::Calibration||!f.label[0]||!f.key[0]))return false;
 }
 return true;
}
// Measure real layout/outline metrics and rebuild at a smaller actual font size.
// Never scale glyph X/Y independently, crop wrapped labels, or silently clip info.
bool Fits(ui_theme::Label& label,float width,float height,bool multiline){
 auto& m=label.mesh;
 if(!std::isfinite(m.advanceWidth)||!std::isfinite(m.layoutHeight)||!std::isfinite(m.ink.left)||!std::isfinite(m.ink.top)||!std::isfinite(m.ink.right)||!std::isfinite(m.ink.bottom))return false;
 if(!multiline&&m.lineCount!=1)return false;
 const float dx=std::max(0.f,-m.ink.left),dy=std::max(0.f,-m.ink.top);
 // A small overhang is translated into the text box, never cut away.
 if(dx||dy){for(auto& t:m.triangles)for(auto* p:{&t.a,&t.b,&t.c}){p->x+=dx;p->y+=dy;}for(auto& e:label.edges){e.a.x+=dx;e.b.x+=dx;e.a.y+=dy;e.b.y+=dy;}m.ink.left+=dx;m.ink.right+=dx;m.ink.top+=dy;m.ink.bottom+=dy;m.advanceWidth+=dx;m.layoutHeight+=dy;}
 constexpr float fringe=.8f;
 return m.advanceWidth<=width-fringe&&m.layoutHeight<=height-fringe&&m.ink.right<=width-fringe&&m.ink.bottom<=height-fringe;
}
HRESULT Label(const wchar_t* text,float preferred,float minimum,float width,float height,bool multiline,ui_theme::Label& out,float& size){
 out={};size=0;if(!text[0])return S_OK;
 if(!multiline&&(wcschr(text,L'\n')||wcschr(text,L'\r')))return E_INVALIDARG;
 const auto attempt=[&](float font,ui_theme::Label& label){return ui_theme::BuildLabel(text,font,width,height,label,L"Times New Roman");};
 ui_theme::Label pending;auto hr=attempt(preferred,pending);if(FAILED(hr))return hr;
 if(Fits(pending,width,height,multiline)){size=preferred;out=std::move(pending);return S_OK;}
 hr=attempt(minimum,pending);if(FAILED(hr))return hr;
 if(!Fits(pending,width,height,multiline))return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
 ui_theme::Label best=std::move(pending);float lo=minimum,hi=preferred;
 // Six steps bound expensive outline work and keep actual type size within .6DIP.
 for(unsigned n=0;n<6;++n){const float mid=(lo+hi)*.5f;hr=attempt(mid,pending);if(FAILED(hr))return hr;
  if(Fits(pending,width,height,multiline)){lo=mid;best=std::move(pending);}else hi=mid;
 }
 size=lo;out=std::move(best);return S_OK;
}
struct Labels {ui_theme::Label title,infoTitle,info,row[7],value[7],footer[4],key[4];};
HRESULT Prepare(const View& v,Labels& l,TextSizes& sizes){
 HRESULT hr=Label(v.title,64,40,1530,96,false,l.title,sizes.title);
 if(SUCCEEDED(hr)&&v.page==Page::Display)hr=Label(v.infoTitle,32,26,470,55,false,l.infoTitle,sizes.infoTitle);
 if(SUCCEEDED(hr)&&v.page==Page::Display)hr=Label(v.information,30,26,465,470,true,l.info,sizes.information);
 for(unsigned n=0;n<v.count&&SUCCEEDED(hr);++n){
  const float labelWidth=v.rows[n].kind==Kind::Navigation?680.f:v.page==Page::Display?520.f:600.f;
  if(v.page!=Page::Languages)hr=Label(v.rows[n].label,v.page==Page::Display?34.f:42.f,v.page==Page::Display?26.f:28.f,labelWidth,v.page==Page::Display?51.f:63.f,false,l.row[n],sizes.row[n]);
  if(SUCCEEDED(hr)&&v.rows[n].kind!=Kind::Gauge&&v.rows[n].kind!=Kind::Navigation)hr=Label(v.rows[n].value,v.page==Page::Languages?56.f:v.page==Page::Display?32.f:38.f,v.page==Page::Languages?36.f:v.page==Page::Display?24.f:28.f,v.page==Page::Languages?750.f:330.f,v.page==Page::Display?50.f:70.f,false,l.value[n],sizes.value[n]);
 }
 for(unsigned n=0;n<4&&SUCCEEDED(hr);++n)if(v.footer[n].action!=Action::None){
  hr=Label(v.footer[n].label,28,22,245,46,false,l.footer[n],sizes.footer[n]);if(SUCCEEDED(hr))hr=Label(v.footer[n].key,23,18,134,32,false,l.key[n],sizes.key[n]);
 }
 return hr;
}
void Add(Layout& l,ui_art::Bounds b,Action a,int row=-1){if(l.count<40)l.regions[l.count++]={b,a,row};}
void Arrow(ui_art::Scene& s,float x,float y,bool right,D3DCOLOR c){const float d=right?1.f:-1.f;s.Line(x-5*d,y-9,x+5*d,y,1.5f,c);s.Line(x+5*d,y,x-5*d,y+9,1.5f,c);}
void Selection(ui_art::Scene& s,const ui_art::Bounds& r){s.Rect(r.x,r.y,r.w,r.h,0x16e2b66f);s.Line(r.x,r.y,r.x+r.w,r.y,1,0xb9d5d4ca);s.Line(r.x,r.y+r.h,r.x+r.w,r.y+r.h,1,0xb9d5d4ca);s.Line(r.x,r.y,r.x,r.y+r.h,2,0xffe2b66f);}
void Header(ui_art::Scene& s,const Labels& l,Page page){
 s.Frame(57,58,1806,148);s.Frame(57,958,1806,76);
 // Small native-setting family pictograms; no controller glyphs.
 if(page==Page::Audio){s.Line(108,120,123,120,2,Ink);s.Line(123,120,140,105,2,Ink);s.Line(140,105,140,157,2,Ink);s.Line(140,157,123,142,2,Ink);s.Line(123,142,108,142,2,Ink);s.Line(108,142,108,120,2,Ink);s.Arc(140,131,19,-.9f,.9f,2,Ink);s.Arc(140,131,29,-.9f,.9f,2,Ink);}
 else if(page==Page::Display){s.Rect(107,110,54,3,Ink);s.Rect(107,110,3,37,Ink);s.Rect(158,110,3,37,Ink);s.Rect(107,145,54,3,Ink);s.Line(134,149,134,161,2,Ink);s.Line(120,162,148,162,2,Ink);}
 else if(page==Page::PhoneParameters){for(unsigned n=0;n<3;++n){const float y=113+n*17.f,x=119+n*12.f;s.Line(107,y,161,y,1.8f,Ink);s.Rect(x,y-5,5,10,Ink);}}
 else if(page==Page::PhoneMessages){s.Line(107,111,163,111,1.8f,Ink);s.Line(163,111,163,151,1.8f,Ink);s.Line(163,151,107,151,1.8f,Ink);s.Line(107,151,107,111,1.8f,Ink);s.Line(107,111,135,134,1.8f,Ink);s.Line(135,134,163,111,1.8f,Ink);}
 else if(page==Page::PhoneContacts){s.Arc(134,118,13,0,6.2831853f,1.8f,Ink);s.Arc(134,158,25,3.14159265f,6.2831853f,1.8f,Ink);s.Line(109,158,159,158,1.8f,Ink);}
 else {s.Line(108,110,160,110,2,Ink);s.Line(160,110,160,143,2,Ink);s.Line(160,143,132,143,2,Ink);s.Line(132,143,118,156,2,Ink);s.Line(118,156,118,143,2,Ink);s.Line(118,143,108,143,2,Ink);s.Line(108,143,108,110,2,Ink);for(float x:{120.f,134.f,148.f})s.Arc(x,126,1.5f,0,6.2831853f,2,Ink);}
 s.Text(l.title,184,88,Ink);
}
void FooterArt(ui_art::Scene& s,const View& v,const Labels& l,Layout& hits){
 for(unsigned n=0;n<4;++n){const auto& f=v.footer[n];if(f.action==Action::None)continue;
  const float x=112+n*440.f,kw=std::max(42.f,std::min(150.f,l.key[n].mesh.advanceWidth+16.f));const auto color=f.enabled?Ink:Muted;
  s.Button(x,978,kw,37,false,f.enabled!=0);s.Text(l.key[n],x+8,981,color);s.Text(l.footer[n],x+kw+16,978,color);
  if(f.enabled)Add(hits,{x-8,968,424,58},f.action);
 }

}
}
ui_art::Bounds RosterPortrait()noexcept{return {1240,330,240,301.875f};}
ui_art::Bounds RowBounds(Page page,unsigned index) noexcept {
 if(page==Page::Audio&&index<3)return {403,421+124.f*index,1120,108};
 if(page==Page::PhoneParameters&&index<4)return {403,330+124.f*index,1120,108};
 if((page==Page::PhoneMessages&&index<5)||(page==Page::PhoneContacts&&index<4))return {403,330+92.f*index,760,84};
 if(page==Page::Display&&index<7)return {127,307+81.f*index,1080,79};
 if(page==Page::Languages&&index==0)return {452,475,1016,118};
 return {};
}
HRESULT Build(const View& v,ui_art::Scene& out,Layout& layout) noexcept {
 out.batches.clear();layout={};if(!Valid(v))return E_INVALIDARG;
 try{
  Labels l;Layout hits;auto hr=Prepare(v,l,hits.text);if(FAILED(hr))return hr;ui_art::Scene scene;Header(scene,l,v.page);
  if(v.page==Page::Audio){scene.Frame(348,366,1230,462);}
  else if(v.page==Page::PhoneParameters||v.page==Page::PhoneMessages||v.page==Page::PhoneContacts){scene.Frame(348,276,1230,596);}
  else if(v.page==Page::Languages){scene.Frame(402,420,1116,225);}
  else {scene.Frame(89,266,1166,630);scene.Frame(1295,266,568,630);scene.Text(l.infoTitle,1335,295,Ink);scene.Line(1335,358,1820,358,.8f,0xff7c7d73);scene.Text(l.info,1335,390,Ink);}
  for(unsigned n=0;n<v.count;++n){const auto& r=v.rows[n];const auto b=RowBounds(v.page,n);const bool active=r.enabled&&!r.locked;const auto color=active?Ink:Muted;
   if(v.selected==static_cast<int>(n))Selection(scene,b);
   else if(n+1<v.count)scene.Line(b.x+7,b.y+b.h,b.x+b.w,b.y+b.h,.7f,0x837c7d73);
   if(active)Add(hits,b,Action::FocusRow,static_cast<int>(n));
   if(v.page!=Page::Languages)scene.Text(l.row[n],b.x+22,b.y+(v.page==Page::Display?20.f:26.f),color);
   const float cx=v.page==Page::Display?985.f:v.page==Page::Languages?960.f:1290.f,cy=b.y+b.h*.5f;
   if(r.kind==Kind::Navigation){Arrow(scene,b.x+b.w-37,cy,true,color);if(active)Add(hits,b,Action::Activate,static_cast<int>(n));}
   else if(r.kind==Kind::Gauge){
    const float gx=v.page==Page::Display?809.f:1095.f,gw=v.page==Page::Display?326.f:330.f;
    scene.Line(gx,cy,gx+gw,cy,4,0xff6c726b);
    const double t=(r.current-r.minimum)/(r.maximum-r.minimum);const float hx=gx+gw*static_cast<float>(t);
    scene.Rect(hx-4,cy-13,8,26,color);
    hits.knobs[n]={1,static_cast<std::uint32_t>(scene.batches.size()-1),static_cast<std::uint32_t>(scene.batches.back().vertices.size()-6),gx,gw,cy};
    scene.Line(gx-31,cy,gx-17,cy,1.5f,color);scene.Line(gx+gw+18,cy,gx+gw+32,cy,1.5f,color);scene.Line(gx+gw+25,cy-7,gx+gw+25,cy+7,1.5f,color);
    if(active){Add(hits,{gx,cy-22,gw,44},Action::Gauge,static_cast<int>(n));Add(hits,{gx-42,cy-22,40,44},Action::Previous,static_cast<int>(n));Add(hits,{gx+gw+3,cy-22,40,44},Action::Next,static_cast<int>(n));}
   }else{
    const float gap=v.page==Page::Languages?475.f:v.page==Page::Display?178.f:217.f;
    Arrow(scene,cx-gap,cy,false,color);Arrow(scene,cx+gap,cy,true,color);
    const auto& text=l.value[n];scene.Text(text,cx-text.mesh.advanceWidth*.5f,b.y+(v.page==Page::Languages?23.f:v.page==Page::Display?21.f:28.f),color);
    if(r.kind==Kind::CheckBox){const float x=cx-gap+35;scene.Rect(x,cy-10,20,20,0xff858a80);scene.Rect(x+2,cy-8,16,16,0xff151a1a);if(r.checked){scene.Line(x+4,cy,x+9,cy+5,2,color);scene.Line(x+9,cy+5,x+17,cy-5,2,color);}}
    if(active){Add(hits,{cx-gap-23,cy-24,46,48},Action::Previous,static_cast<int>(n));Add(hits,{cx+gap-23,cy-24,46,48},Action::Next,static_cast<int>(n));}
   }
  }
  if((v.page==Page::PhoneMessages||v.page==Page::PhoneContacts)&&v.selected>=0){
   const auto b=RosterPortrait();for(const auto& e:{std::array<float,4>{b.x,b.y,b.x+b.w,b.y},{b.x+b.w,b.y,b.x+b.w,b.y+b.h},{b.x+b.w,b.y+b.h,b.x,b.y+b.h},{b.x,b.y+b.h,b.x,b.y}})scene.Line(e[0],e[1],e[2],e[3],.8f,0x58c4c6c3);
   const auto kind=phone_roster_content::Parse(v.rows[v.selected].value);const float x=b.x+b.w*.5f,y=b.y+b.h*.5f;
   if(kind==phone_roster_content::Kind::Documents){scene.Polygon({{x-51,y-70},{x+26,y-70},{x+51,y-45},{x+51,y+70},{x-51,y+70}},0x157e888e);scene.Line(x-51,y-70,x+26,y-70,2,Ink);scene.Line(x+26,y-70,x+51,y-45,2,Ink);scene.Line(x+51,y-45,x+51,y+70,2,Ink);scene.Line(x+51,y+70,x-51,y+70,2,Ink);scene.Line(x-51,y+70,x-51,y-70,2,Ink);scene.Line(x+26,y-70,x+26,y-45,2,Ink);scene.Line(x+26,y-45,x+51,y-45,2,Ink);for(unsigned n=0;n<4;++n)scene.Line(x-32,y-20+n*20.f,x+30,y-20+n*20.f,1.5f,Muted);}
   else if(kind==phone_roster_content::Kind::Tips){scene.Arc(x,y-24,30,3.5f,7.7f,2.5f,Ink);scene.Line(x+4,y+5,x+4,y+31,2.5f,Ink);scene.Arc(x+4,y+52,3,0,6.2831853f,2.5f,Ink);}
  }
  if(v.page==Page::Audio){}
  else if(v.page==Page::Languages){}
  else {}
  FooterArt(scene,v,l,hits);out=std::move(scene);layout=hits;return S_OK;
 }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
bool UpdateGaugeCurrents(const View& old,const View& next,ui_art::Scene& scene,const Layout& layout) noexcept {
 // Explicit semantic comparisons: trailing UTF16 storage and struct padding are
 // not part of cache identity. Validate both snapshots before any string reads.
 if(!Valid(old)||!Valid(next)||old.version!=next.version||old.page!=next.page||old.count!=next.count||old.selected!=next.selected||wcscmp(old.title,next.title)||wcscmp(old.infoTitle,next.infoTitle)||wcscmp(old.information,next.information)||layout.count>40)return false;
 for(unsigned n=0;n<4;++n){const auto& a=old.footer[n];const auto& b=next.footer[n];if(a.action!=b.action||a.enabled!=b.enabled||wcscmp(a.label,b.label)||wcscmp(a.key,b.key))return false;}
 struct Patch {unsigned batch{},first{};float x[6]{};};Patch pending[7]{};unsigned count=0;
 for(unsigned n=0;n<old.count;++n){const auto& a=old.rows[n];const auto& b=next.rows[n];
  if(a.kind!=b.kind||a.enabled!=b.enabled||a.locked!=b.locked||a.checked!=b.checked||wcscmp(a.label,b.label)||wcscmp(a.value,b.value))return false;
  const auto& k=layout.knobs[n];
  if(a.kind!=Kind::Gauge){if(k.present)return false;continue;}
  if(a.minimum!=b.minimum||a.maximum!=b.maximum||k.present!=1)return false;
  const float gx=old.page==Page::Display?809.f:1095.f,gw=old.page==Page::Display?326.f:330.f,cy=RowBounds(old.page,n).y+RowBounds(old.page,n).h*.5f;
  if(k.trackX!=gx||k.trackWidth!=gw||k.centerY!=cy||k.batch>=scene.batches.size())return false;
  const auto& batch=scene.batches[k.batch];if(batch.material!=ui_art::Material::Solid||k.firstVertex>batch.vertices.size()||batch.vertices.size()-k.firstVertex<6)return false;
  const auto left=[&](double current){return gx+gw*static_cast<float>((current-a.minimum)/(a.maximum-a.minimum))-4.f;};
  const float before=left(a.current),after=left(b.current),top=cy-13.f;
  const float x[]={before,before+8,before+8,before,before+8,before};
  const float y[]={top,top,top+26,top,top+26,top+26};
  const D3DCOLOR color=a.enabled&&!a.locked?Ink:Muted;
  for(unsigned j=0;j<6;++j){const auto& v=batch.vertices[k.firstVertex+j];if(v.x!=x[j]||v.y!=y[j]||v.z!=0||v.rhw!=1||v.color!=color)return false;}
  auto& patch=pending[count++];patch.batch=k.batch;patch.first=k.firstVertex;
  const float updated[]={after,after+8,after+8,after,after+8,after};for(unsigned j=0;j<6;++j)patch.x[j]=updated[j];
 }
 // No write occurs until every row/knob has passed. Solid batches do not sample UV.
 for(unsigned n=0;n<count;++n){auto& batch=scene.batches[pending[n].batch];for(unsigned j=0;j<6;++j)batch.vertices[pending[n].first+j].x=pending[n].x[j];}
 return true;
}
Hit HitTest(const Layout& l,float x,float y) noexcept {
 if(!std::isfinite(x)||!std::isfinite(y)||l.count>40)return {};
 // Fine controls are added after row focus rectangles and win shared drawing geometry.
 for(unsigned n=l.count;n>0;--n){const auto& r=l.regions[n-1];if(r.bounds.Contains(x,y))return {r.action,r.row,r.action==Action::Gauge?std::max(0.f,std::min(1.f,(x-r.bounds.x)/r.bounds.w)):0.f};}
 return {};
}
}
