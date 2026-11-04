#define NOMINMAX
#include "controls_art.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
namespace controls_art {
namespace {
constexpr D3DCOLOR Ink=0xffe9e5dc,Muted=0xff818b92,Amber=0xffe2b66f;
template<size_t N> bool Text(const wchar_t(&s)[N]){return std::find(s,s+N,L'\0')!=s+N;}
bool Modal(Phase p){return p>=Phase::ReleaseKeys&&p<=Phase::Forbidden;}
bool Valid(const View& v){
 if(v.version!=1||v.phase<Phase::Bindings||v.phase>Phase::Forbidden||!Text(v.title)||!v.title[0]||v.category>3||v.count>10||v.total>4096||v.offset>v.total||v.count>v.total-v.offset||(v.count&&v.selected>=v.count))return false;
 for(const auto& s:v.categories)if(!Text(s)||!s[0])return false;
 if((v.phase==Phase::Settings||(Modal(v.phase)&&v.category==3))&&v.count>5)return false;
 for(unsigned n=0;n<v.count;++n){const auto& r=v.rows[n];if(!Text(r.caption)||!r.caption[0]||!Text(r.value)||!Text(r.device)||r.enabled>1||r.locked>1||r.warning>1)return false;
  if(r.kind<Kind::Binding||r.kind>Kind::Gauge||(v.phase==Phase::Bindings&&r.kind!=Kind::Binding)||(v.phase==Phase::Settings&&r.kind==Kind::Binding))return false;
  if(r.kind==Kind::Gauge&&(!std::isfinite(r.current)||!std::isfinite(r.minimum)||!std::isfinite(r.maximum)||!std::isfinite(r.maximum-r.minimum)||r.maximum<=r.minimum||r.current<r.minimum||r.current>r.maximum))return false;
 }
 for(const auto& f:v.footer)if(!Text(f.caption)||!Text(f.key)||f.enabled>1||f.action>Action::PageNext||(f.action!=Action::None&&(!f.caption[0]||!f.key[0])))return false;
 return Text(v.modalTitle)&&Text(v.modalBody)&&(!Modal(v.phase)||(v.modalTitle[0]&&v.modalBody[0]));
}
bool Fits(ui_theme::Label& l,float w,float h,bool wrap){auto& m=l.mesh;if(!std::isfinite(m.advanceWidth)||!std::isfinite(m.layoutHeight)||!std::isfinite(m.ink.left)||!std::isfinite(m.ink.top)||!std::isfinite(m.ink.right)||!std::isfinite(m.ink.bottom))return false;
 const float dx=(std::max)(0.f,-m.ink.left),dy=(std::max)(0.f,-m.ink.top);
 if(dx||dy){for(auto& t:m.triangles)for(auto* p:{&t.a,&t.b,&t.c}){p->x+=dx;p->y+=dy;}for(auto& e:l.edges){e.a.x+=dx;e.b.x+=dx;e.a.y+=dy;e.b.y+=dy;}m.ink.left+=dx;m.ink.right+=dx;m.ink.top+=dy;m.ink.bottom+=dy;m.advanceWidth+=dx;m.layoutHeight+=dy;}
 return m.advanceWidth<=w-.8f&&m.layoutHeight<=h-.8f&&m.ink.right<=w-.8f&&m.ink.bottom<=h-.8f&&(wrap||m.lineCount==1);
}
HRESULT MakeLabel(const wchar_t* text,float size,float minimum,float width,float height,ui_theme::Label& label,bool wrap=false,const wchar_t* family=L"Times New Roman"){
 label={};if(!text[0])return S_OK;
 if(!wrap&&(wcschr(text,L'\n')||wcschr(text,L'\r')))return E_INVALIDARG;
 for(float s=size;;s=(std::max)(minimum,s-2.f)){
  ui_theme::Label next;auto hr=ui_theme::BuildLabel(text,s,width,height,next,family);if(FAILED(hr))return hr;
  if(Fits(next,width,height,wrap)){label=std::move(next);return S_OK;}if(s<=minimum)return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
 }
}
HRESULT Caption(ui_art::Scene& s,const wchar_t* text,float x,float y,float width,float height,float size,D3DCOLOR color,float minimum=24,bool wrap=false){ui_theme::Label label;auto hr=MakeLabel(text,size,minimum,width,height,label,wrap);if(SUCCEEDED(hr))s.Text(label,x,y,color);return hr;}
void Add(Layout& l,ui_art::Bounds b,Action action,int row=-1){if(l.count<64)l.regions[l.count++]={b,action,row};}
void Selected(ui_art::Scene& s,ui_art::Bounds b){s.Rect(b.x,b.y,b.w,b.h,0x16e2b66f);s.Line(b.x,b.y,b.x+b.w,b.y,1,0xa6d5d4ca);s.Line(b.x,b.y+b.h,b.x+b.w,b.y+b.h,1,0xa6d5d4ca);s.Line(b.x,b.y,b.x,b.y+b.h,2,Amber);}
void Chevron(ui_art::Scene& s,float x,float y,bool right,D3DCOLOR color){const float d=right?1.f:-1.f;s.Line(x-5*d,y-9,x+5*d,y,1.5f,color);s.Line(x+5*d,y,x-5*d,y+9,1.5f,color);}
HRESULT Key(ui_art::Scene& s,const wchar_t* text,ui_art::Bounds b,D3DCOLOR color){
 if(!text[0])return S_OK;ui_theme::Label l;auto hr=MakeLabel(text,26,20,b.w-24,b.h-6,l,false,L"Segoe UI");if(FAILED(hr))return hr;
 const float width=(std::max)(48.f,l.mesh.advanceWidth+24);const float x=b.x+(b.w-width)*.5f;s.Button(x,b.y,width,b.h,false);s.Text(l,x+12,b.y+3,color);return S_OK;
}
HRESULT FooterArt(const View& v,ui_art::Scene& s,Layout& l,bool modal){
 unsigned active=0;for(const auto& f:v.footer)if(f.action!=Action::None)++active;
 if(modal&&active>2)return E_INVALIDARG;
 unsigned at=0;for(unsigned n=0;n<4;++n){const auto& f=v.footer[n];if(f.action==Action::None)continue;const float x=modal?960-active*200.f+at*400.f:112+at*440.f,y=modal?675.f:978.f;const auto color=f.enabled?Ink:Muted;
  ui_theme::Label key,caption;auto hr=MakeLabel(f.key,23,18,134,34,key,false,L"Segoe UI");if(SUCCEEDED(hr))hr=MakeLabel(f.caption,28,22,245,46,caption);if(FAILED(hr))return hr;
  const float kw=(std::max)(44.f,key.mesh.advanceWidth+16);s.Button(x,y,kw,38,false,f.enabled!=0);s.Text(key,x+8,y+3,color);s.Text(caption,x+kw+16,y+1,color);
  if(f.enabled)Add(l,{x-8,y-8,416,58},f.action,static_cast<int>(n));++at;
 }return S_OK;
}
}
ui_art::Bounds Category(unsigned n)noexcept{return {128+n*416.f,236,400,66};}
ui_art::Bounds Binding(unsigned n)noexcept{return {128,342+n*53.f,1664,51};}
ui_art::Bounds Setting(unsigned n)noexcept{return {335,345+n*101.f,1250,94};}
HRESULT Build(const View& v,ui_art::Scene& out,Layout& layout)noexcept{
 out.batches.clear();layout={};if(!Valid(v))return E_INVALIDARG;
 try{
  ui_art::Scene s;Layout hit;const bool modal=Modal(v.phase),settings=v.phase==Phase::Settings||(modal&&v.category==3);
  s.Frame(57,58,1806,148);s.Frame(57,958,1806,76);s.Frame(89,320,1742,590);
  auto hr=Caption(s,v.title,130,88,1590,96,64,Ink,40);
  for(unsigned n=0;n<4&&SUCCEEDED(hr);++n){const auto b=Category(n);s.Button(b.x,b.y,b.w,b.h,n==v.category);hr=Caption(s,v.categories[n],b.x+17,b.y+17,b.w-34,46,30,n==v.category?Ink:Muted,22);if(!modal)Add(hit,b,Action::Category,static_cast<int>(n));}
  for(unsigned n=0;n<v.count&&SUCCEEDED(hr);++n){const auto& r=v.rows[n];const auto b=settings?Setting(n):Binding(n);const bool enabled=r.enabled&&!r.locked;const auto color=enabled?r.warning?Amber:Ink:Muted;
   if(n==v.selected)Selected(s,b);else if(n+1<v.count)s.Line(b.x+8,b.y+b.h,b.x+b.w,b.y+b.h,.7f,0x397e888e);
   hr=Caption(s,r.caption,b.x+20,b.y+(settings?29.f:10.f),settings?650.f:760.f,settings?58.f:42.f,settings?38.f:30.f,color,24);
   if(FAILED(hr))break;
   if(r.kind==Kind::Gauge){
    const float x=1120,y=b.y+47,width=350;const double unit=(r.current-r.minimum)/(r.maximum-r.minimum);s.Line(x,y,x+width,y,3,Muted);s.Line(x,y,x+width*static_cast<float>(unit),y,3,color);s.Rect(x+width*static_cast<float>(unit)-4,y-12,8,24,color);
    if(enabled&&!modal)Add(hit,{x-8,y-22,width+16,44},Action::Gauge,static_cast<int>(n));
   }else if(r.kind==Kind::Selector){hr=Caption(s,r.value,1125,b.y+31,350,50,34,color,24);Chevron(s,1080,b.y+47,false,color);Chevron(s,1534,b.y+47,true,color);if(enabled&&!modal){Add(hit,{1055,b.y,50,b.h},Action::Previous,static_cast<int>(n));Add(hit,{1509,b.y,50,b.h},Action::Next,static_cast<int>(n));}}
   else{hr=Key(s,r.value,{950,b.y+4,330,42},color);if(SUCCEEDED(hr))hr=Caption(s,r.device,1310,b.y+13,450,39,26,Muted,22);}
   if(enabled&&!modal)Add(hit,b,Action::Activate,static_cast<int>(n));
  }
  if(!settings&&v.total>v.count&&SUCCEEDED(hr)){wchar_t counter[64]{};swprintf_s(counter,L"%lu / %lu",static_cast<unsigned long>(v.total?v.offset+v.selected+1:0),static_cast<unsigned long>(v.total));hr=Caption(s,counter,1540,916,220,37,26,Muted,22);const float height=(std::max)(16.f,530.f*v.count/v.total),travel=530-height,unit=v.total>v.count?static_cast<float>(v.offset)/(v.total-v.count):0;s.Line(1810,342,1810,872,1,0x40818b92);s.Rect(1807,342+travel*unit,6,height,Muted);}
  if(modal){hit={};s.Rect(0,0,1920,1080,0x80000000);s.Frame(450,330,1020,430);if(SUCCEEDED(hr))hr=Caption(s,v.modalTitle,500,370,920,76,48,Ink,32);s.Line(500,454,1420,454,.8f,Muted);if(SUCCEEDED(hr))hr=Caption(s,v.modalBody,500,484,920,160,34,Ink,26,true);}
  if(SUCCEEDED(hr))hr=FooterArt(v,s,hit,modal);if(FAILED(hr))return hr;out=std::move(s);layout=hit;return S_OK;
 }catch(...){out.batches.clear();layout={};return E_FAIL;}
}
Hit HitTest(const Layout& l,float x,float y)noexcept{
 if(l.count>64||!std::isfinite(x)||!std::isfinite(y))return {};
 // Small gauges/arrows win over their enclosing row.
 for(unsigned n=0;n<l.count;++n){const auto& r=l.regions[n];if(r.bounds.Contains(x,y)){double unit=0;if(r.action==Action::Gauge)unit=(std::max)(0.,(std::min)(1.,(x-r.bounds.x-8.)/(r.bounds.w-16.)));return {r.action,r.row,unit};}}
 return {};
}
}
