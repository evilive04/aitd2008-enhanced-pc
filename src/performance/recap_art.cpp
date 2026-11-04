#include "recap_art.h"
#include <algorithm>
#include <cwchar>
#include <cmath>
namespace recap_art {
static constexpr D3DCOLOR Ink=0xffe9e5dc,Amber=0xffe2b66f,Muted=0xff9ca5ab;
ui_art::Bounds Button(unsigned action)noexcept{
 switch(action){case RecapBack:return {248,819,210,51};case RecapPreviousSequence:return {248,724,128,54};case RecapPreviousChapter:return {398,724,128,54};case RecapPlay:return {548,724,128,54};case RecapNextChapter:return {698,724,128,54};case RecapNextSequence:return {848,724,128,54};default:return {};}
}
unsigned Hit(float x,float y)noexcept{for(unsigned n=1;n<=6;++n)if(Button(n).Contains(x,y))return n;return 0;}
static HRESULT Label(ui_art::Scene& scene,const wchar_t* text,unsigned cap,float size,float x,float y,float w,float h,D3DCOLOR color){
 if(!text||!wmemchr(text,0,cap)||!text[0])return E_INVALIDARG;
 // Longer native episode summaries must fit without suppressing the entire UI.
 // Reflow at smaller sizes, never crop or distort the actual localized text.
 for(float fitted=size;;fitted=(std::max)(14.f,fitted-2.f)){
  ui_theme::Label label;const auto hr=ui_theme::BuildLabel(text,fitted,w,h,label,L"Times New Roman");
  if(SUCCEEDED(hr)&&label.mesh.layoutHeight<=h+.125f&&label.mesh.advanceWidth<=w+.125f){scene.Text(label,x,y,color);return S_OK;}
  if(FAILED(hr)&&hr!=E_INVALIDARG)return hr;if(fitted<=14.f)return E_INVALIDARG;
 }
}
// Cut a real hole in one feathered glass surface. Each triangle is partitioned
// into disjoint half-planes, preserving interpolated alpha at the outside rim.
static void Frame(ui_art::Scene& out){
 ui_art::Scene panel;panel.Frame(230,165,750,727);const auto hole=Video();
 if(out.batches.empty())out.batches.push_back({ui_art::Material::Solid,{}});auto& target=out.batches.back().vertices;
 for(const auto& batch:panel.batches)for(size_t at=0;at+2<batch.vertices.size();at+=3)for(unsigned region=0;region<4;++region){
  std::vector<ui_art::Vertex> p(batch.vertices.begin()+at,batch.vertices.begin()+at+3);
  const float bounds[]={hole.y,hole.y+hole.h,hole.x,hole.x+hole.w};
  for(unsigned edge=0;edge<(region<2?1u:3u)&&!p.empty();++edge){
   const unsigned which=region<2?region:edge<2?edge:region;
   const bool horizontal=which<2;const float bound=bounds[which];
   const bool greater=region<2?(region==1):(edge==0||(edge==2&&region==3));
   std::vector<ui_art::Vertex> next;auto a=p.back();auto coord=[&](const ui_art::Vertex& v){return horizontal?v.y:v.x;};bool inside=greater?coord(a)>=bound:coord(a)<=bound;
   for(const auto& b:p){const bool bi=greater?coord(b)>=bound:coord(b)<=bound;
    if(inside!=bi){const float f=(bound-coord(a))/(coord(b)-coord(a));auto v=a;v.x=a.x+(b.x-a.x)*f;v.y=a.y+(b.y-a.y)*f;v.color=0;
     for(unsigned shift=0;shift<32;shift+=8){const float ac=static_cast<float>((a.color>>shift)&255),bc=static_cast<float>((b.color>>shift)&255);v.color|=static_cast<DWORD>(std::lround(ac+(bc-ac)*f))<<shift;}next.push_back(v);}
    if(bi)next.push_back(b);a=b;inside=bi;
   }p=std::move(next);
  }for(size_t n=1;n+1<p.size();++n){target.push_back(p[0]);target.push_back(p[n]);target.push_back(p[n+1]);}
 }
}
void Highlight(ui_art::Scene& scene,unsigned selected)noexcept{
 const auto b=Button(selected);if(!b.w)return;
 for(auto& batch:scene.batches)if(batch.material==ui_art::Material::Solid)for(auto& v:batch.vertices)if(v.x>=b.x-.01f&&v.x<=b.x+b.w+.01f&&v.y>=b.y-.01f&&v.y<=b.y+b.h+.01f){
  if((v.color&0xffffffu)==(Ink&0xffffffu))v.color=(v.color&0xff000000u)|(Amber&0xffffffu);
  else if(v.color==0x9ac4c6c3)v.color=0xdfe2b66f;
  else if(v.color==0x48090d10)v.color=0x60292217;
 }
}
HRESULT Build(const RecapStateV1& v,unsigned selected,ui_art::Scene& out)noexcept{
 if(v.size!=sizeof v||v.version!=1||!(v.flags&RecapVisible)||!v.episode||v.episode>v.episodes||!v.sequence||v.sequence>v.sequences||!v.chapter||v.chapter>v.chapters)return E_INVALIDARG;
 try{
  ui_art::Scene s;const auto video=Video();
  // Four regions form a genuine opening over the native movie. A translucent
  // rectangle on top of its pixels would dim the preview twice.
  Frame(s);
  s.Frame(1010,165,680,727);s.Line(video.x,video.y-2,video.x+video.w,video.y-2,1,0xffa9aeaf);s.Line(video.x,video.y+video.h+2,video.x+video.w,video.y+video.h+2,1,0xffa9aeaf);
  auto hr=Label(s,v.title,128,43,268,180,670,55,Ink);
  wchar_t count[96]{};swprintf_s(count,L"%02lu / %02lu     ·     %02lu / %02lu",v.sequence,v.sequences,v.chapter,v.chapters);
  if(SUCCEEDED(hr))hr=Label(s,count,96,25,284,651,660,45,Muted);
  wchar_t episode[64]{};swprintf_s(episode,L"%02lu / %02lu",v.episode,v.episodes);
  if(SUCCEEDED(hr))hr=Label(s,episode,64,25,1056,212,580,42,Muted);
  if(SUCCEEDED(hr))hr=Label(s,v.episodeTitle,128,40,1056,272,580,120,Ink);
  if(SUCCEEDED(hr))hr=Label(s,v.sequenceTitle,128,30,1056,407,580,100,Amber);
  s.Line(1056,524,1644,524,.8f,0x7ba9aeaf);
  if(SUCCEEDED(hr)&&v.summary[0])hr=Label(s,v.summary,768,25,1056,551,580,290,Ink);
  for(unsigned n=1;n<=6;++n){const auto b=Button(n);const bool active=n==RecapBack||(!(v.flags&RecapBusy)&&!(n==RecapPlay&&(v.flags&RecapLocked)));const auto color=active?(selected==n?Amber:Ink):Muted;s.Button(b.x,b.y,b.w,b.h,selected==n,active);
   const float x=b.x+b.w*.5f,y=b.y+b.h*.5f;
   if(n==RecapBack){if(SUCCEEDED(hr))hr=Label(s,v.back,64,25,b.x+16,b.y+9,b.w-32,36,color);}
   else if(n==RecapPlay){s.Polygon({{x-6,y-11},{x+12,y},{x-6,y+11}},color);}
   else {const float direction=n<RecapPlay?-1.f:1.f;const float a=x-direction*5;s.Line(a,y-10,a+direction*12,y,1.6f,color);s.Line(a+direction*12,y,a,y+10,1.6f,color);
    if(n==RecapPreviousSequence||n==RecapNextSequence)s.Line(x-direction*12,y-11,x-direction*12,y+11,1.6f,color);}
  }
  s.Button(474,823,64,39,false);s.Button(630,823,88,39,false);
  if(SUCCEEDED(hr))hr=Label(s,L"Esc",4,20,486,830,44,31,Muted);
  if(SUCCEEDED(hr))hr=Label(s,L"Enter",6,20,642,830,68,31,Muted);
  if(SUCCEEDED(hr))hr=Label(s,v.play,64,24,738,830,222,38,Muted);
  if(FAILED(hr))return hr;out=std::move(s);return S_OK;
 }catch(...){return E_OUTOFMEMORY;}
}
}
