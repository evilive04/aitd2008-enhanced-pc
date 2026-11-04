#define NOMINMAX
#include "chapters_art.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
namespace chapters_art {
namespace {
constexpr D3DCOLOR Ink=0xffe9e5dc,Muted=0xff818b92,Amber=0xffe2b66f;
template<size_t N>bool Terminated(const wchar_t(&v)[N]){return std::find(v,v+N,L'\0')!=v+N;}
bool Valid(const ChaptersStateV1& v){
 if(v.size!=sizeof v||v.version!=1||!v.chapter||v.chapter>v.chapters||v.chapters>8||!v.count||v.count>5||v.total>4096||v.offset>=v.total||v.count!=(std::min<DWORD>)(5,v.total-v.offset)||v.selected>=v.count||
    !Terminated(v.title)||!v.title[0]||!Terminated(v.number)||!v.number[0]||!Terminated(v.information)||!Terminated(v.back)||!v.back[0]||!Terminated(v.open)||!v.open[0])return false;
 for(unsigned n=0;n<v.count;++n)if(!Terminated(v.rows[n].title)||!v.rows[n].title[0]||v.rows[n].locked>1)return false;return true;
}
HRESULT Label(ui_art::Scene& scene,const wchar_t* text,float size,float minimum,float x,float y,float w,float h,D3DCOLOR color,bool wrap=false){
 for(float actual=size;;actual=(std::max)(minimum,actual-2.f)){
  ui_theme::Label label;const auto hr=ui_theme::BuildLabel(text,actual,w,h,label,L"Times New Roman");if(FAILED(hr))return hr;const auto& m=label.mesh;
  const float dx=(std::max)(0.f,-m.ink.left),dy=(std::max)(0.f,-m.ink.top);
  if(std::isfinite(m.advanceWidth)&&std::isfinite(m.layoutHeight)&&m.advanceWidth+dx<w&&m.layoutHeight+dy<h&&m.ink.right+dx<w&&m.ink.bottom+dy<h&&(wrap||m.lineCount==1)){scene.Text(label,x+dx,y+dy,color);return S_OK;}
  if(actual<=minimum)return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
 }
}
}
ui_art::Bounds Preview(unsigned n)noexcept{return {244+n*178.f,340,166,150};}
ui_art::Bounds Row(unsigned n)noexcept{return {244,570+n*58.f,690,56};}
unsigned NativePreview(unsigned visual,unsigned selectedCard)noexcept{return (visual+6-selectedCard)%5;}
Hit HitTest(const ChaptersStateV1& v,float x,float y,unsigned selectedCard)noexcept{
 if(!Valid(v)||(v.flags&ChaptersBusy)||!std::isfinite(x)||!std::isfinite(y))return {};
 (void)selectedCard;
 for(unsigned n=0;n<v.chapters;++n)if(Preview(n).Contains(x,y)&&(v.flags&ChaptersCanChange)&&n+1!=v.chapter)return {Action::ChapterCard,static_cast<LONG>(n)};
 for(unsigned n=0;n<v.count;++n)if(Row(n).Contains(x,y))return {Action::Focus,static_cast<LONG>(n)};
 if(ui_art::Bounds{244,880,280,52}.Contains(x,y))return {Action::Back};
 if(ui_art::Bounds{1456,880,204,52}.Contains(x,y)&&!v.rows[v.selected].locked)return {Action::Activate,static_cast<LONG>(v.selected)};
 if((v.flags&ChaptersCanChange)&&ui_art::Bounds{1456,207,84,62}.Contains(x,y))return {Action::PreviousChapter};
 if((v.flags&ChaptersCanChange)&&ui_art::Bounds{1576,207,84,62}.Contains(x,y))return {Action::NextChapter};return {};
}
HRESULT Build(const ChaptersStateV1& v,ui_art::Scene& output,unsigned selectedCard)noexcept{
 output.batches.clear();if(!Valid(v))return E_INVALIDARG;(void)selectedCard;
 try{
  ui_art::Scene s;s.Frame(190,150,1540,810);auto hr=Label(s,v.number,28,24,244,195,1080,42,Muted);
  if(SUCCEEDED(hr))hr=Label(s,v.title,48,32,244,241,1100,72,Ink);
  // Measured source-cell crops from the supplied2196x341 reference. No redraw.
  constexpr float lefts[]={39,304,568,833,1098,1364,1629,1895};
  for(unsigned n=0;n<v.chapters&&SUCCEEDED(hr);++n){const auto p=Preview(n);s.ImageRegion(9,2196,341,{lefts[n],35,248,256},p.x,p.y,p.w,p.h);
   if(n+1==v.chapter){s.Line(p.x,p.y,p.x+p.w,p.y,1.5f,Amber);s.Line(p.x,p.y+p.h,p.x+p.w,p.y+p.h,1.5f,Amber);}
  }
  s.Line(244,548,1660,548,1,0x637e888e);
  for(unsigned n=0;n<v.count&&SUCCEEDED(hr);++n){const auto row=Row(n);s.Button(row.x,row.y,row.w,row.h,n==v.selected,!(v.flags&ChaptersBusy));hr=Label(s,v.rows[n].title,29,22,row.x+24,row.y+9,row.w-80,44,v.rows[n].locked?Muted:Ink);
   if(v.rows[n].locked){const auto x=row.x+row.w-40,y=row.y+22;s.Line(x,y,x+12,y,1.5f,Muted);s.Line(x,y,x,y+14,1.5f,Muted);s.Line(x+12,y,x+12,y+14,1.5f,Muted);s.Line(x,y+14,x+12,y+14,1.5f,Muted);s.Arc(x+6,y,6,3.14159265f,6.28318531f,1.5f,Muted);}
  }
  s.Line(978,570,978,846,1,0x337e888e);
  if(SUCCEEDED(hr))hr=Label(s,v.information[0]?v.information:v.rows[v.selected].title,24,20,1024,788,610,64,v.rows[v.selected].locked?Muted:Ink,true);
  s.Button(1456,207,84,62,false,(v.flags&ChaptersCanChange)&&!(v.flags&ChaptersBusy));s.Button(1576,207,84,62,false,(v.flags&ChaptersCanChange)&&!(v.flags&ChaptersBusy));
  if(SUCCEEDED(hr))hr=Label(s,L"<",34,30,1482,219,46,50,Ink);if(SUCCEEDED(hr))hr=Label(s,L">",34,30,1602,219,46,50,Ink);
  s.Button(544,882,96,48,false);s.Button(1340,882,100,48,false);
  s.Button(244,880,280,52,false,!(v.flags&ChaptersBusy));s.Button(1456,880,204,52,false,!(v.flags&ChaptersBusy)&&!v.rows[v.selected].locked);
  if(SUCCEEDED(hr))hr=Label(s,v.back,26,22,270,891,220,38,Ink);if(SUCCEEDED(hr))hr=Label(s,v.open,26,22,1474,891,168,38,v.rows[v.selected].locked?Muted:Ink);
  if(SUCCEEDED(hr))hr=Label(s,L"Esc",24,22,550,892,100,38,Muted);if(SUCCEEDED(hr))hr=Label(s,L"Enter",24,22,1350,892,94,38,Muted);
  if(FAILED(hr))return hr;output=std::move(s);return S_OK;
 }catch(...){output.batches.clear();return E_FAIL;}
}
}
