#include "calibration_art.h"
#include <cmath>
#include <cwchar>
namespace calibration_art {
static constexpr D3DCOLOR Ink=0xffe9e5dc,Amber=0xffe2b66f,Muted=0xffa1aaaf;
ui_art::Bounds Tab(unsigned n)noexcept{return n<2?ui_art::Bounds{290+690.f*n,275,610,56}:ui_art::Bounds{};}
ui_art::Bounds Slider(unsigned row,unsigned page)noexcept{if(row>3||page>1||(page==0&&row)||(page==1&&!row))return {};return {770,page?610+(row-1)*75.f:718.f,800,42};}
ui_art::Bounds Footer(bool accept)noexcept{return {accept?1160.f:1410.f,852,220,50};}
static HRESULT Text(ui_art::Scene& s,const wchar_t* text,unsigned capacity,float size,float x,float y,float width,float height,D3DCOLOR color=Ink){
 if(!text||!wmemchr(text,0,capacity)||!text[0])return E_INVALIDARG;ui_theme::Label label;auto hr=ui_theme::BuildLabel(text,size,width,height,label,L"Times New Roman");if(FAILED(hr)||label.mesh.layoutHeight>height+.125f||label.mesh.advanceWidth>width+.125f)return FAILED(hr)?hr:E_INVALIDARG;s.Text(label,x,y,color);return S_OK;
}
static bool Valid(const CalibrationStateV1& v){return v.size==sizeof v&&v.version==1&&(v.flags&CalibrationVisible)&&!(v.flags&~31u)&&((v.flags&CalibrationTransition)||(v.flags&CalibrationLevelsKnown))&&v.page<2&&v.selected<4&&std::isfinite(v.opacity)&&v.opacity>=0&&v.opacity<=1&&
 wmemchr(v.title,0,128)&&v.title[0]&&wmemchr(v.brightness,0,64)&&v.brightness[0]&&wmemchr(v.instructions,0,512)&&v.instructions[0]&&wmemchr(v.accept,0,64)&&v.accept[0]&&wmemchr(v.cancel,0,64)&&v.cancel[0];}
HRESULT Prepare(const CalibrationStateV1& v,Prepared& out)noexcept{
 out={};if(!Valid(v))return E_INVALIDARG;
 try{
  Prepared p;auto& s=p.scene;s.Frame(230,145,1460,790);auto hr=Text(s,v.title,128,44,290,190,1300,65);s.Line(290,258,1630,258,.8f,0x8fa9aeaf);
  for(unsigned n=0;n<2;++n){const auto b=Tab(n);s.Button(b.x,b.y,b.w,b.h,n==v.page);if(SUCCEEDED(hr))hr=Text(s,n?L"R / G / B":v.brightness,n?10u:64u,29,b.x+20,b.y+9,b.w-40,42,n==v.page?Amber:Muted);}
  // Black is intentional calibration content; outside the pattern all glass
  // preserves the actual menu video or paused game through its alpha.
  s.Rect(290,359,1340,v.page?185.f:210.f,0xff000000);
  if(!v.page){const unsigned levels[]={7,5,2};for(unsigned n=0;n<3;++n){const DWORD l=levels[n];const auto color=0xff000000|(l<<16)|(l<<8)|l;s.Arc(550+n*410.f,464,42,0,6.283185307f,5,color);s.Line(550+n*410.f,430,550+n*410.f,446,4,color);}
   if(SUCCEEDED(hr))hr=Text(s,v.instructions,512,25,330,599,1260,90);if(SUCCEEDED(hr))hr=Text(s,v.brightness,64,31,330,721,400,55);
  }else{
   for(unsigned channel=0;channel<3;++channel)for(unsigned n=0;n<16;++n)s.Rect(290+n*83.75f,359+channel*37.f,83.75f,37,0xff000000|((n*17)<<(16-channel*8)));
   // Actual source grayscale samples:0,2,4,6,8,10,12,14. Authored geometry is
   // entirely new; retaining these values preserves the near-black reference.
   for(unsigned n=0;n<8;++n){const DWORD l=n*2;s.Rect(290+n*167.5f,485,167.5f,59,0xff000000|(l<<16)|(l<<8)|l);}
   for(unsigned row=1;row<4;++row){const wchar_t* channels[]={L"R",L"G",L"B"};if(SUCCEEDED(hr))hr=Text(s,channels[row-1],2,31,330,610+(row-1)*75.f,200,48);}
  }
  const auto yes=Footer(true),no=Footer(false);s.Button(yes.x,yes.y,yes.w,yes.h,false);s.Button(no.x,no.y,no.w,no.h,false);
  if(SUCCEEDED(hr))hr=Text(s,v.accept,64,26,yes.x+20,yes.y+9,yes.w-40,39);if(SUCCEEDED(hr))hr=Text(s,v.cancel,64,26,no.x+20,no.y+9,no.w-40,39);
  s.Button(290,854,74,40,false);s.Button(888,854,108,40,false);s.Button(1028,854,80,40,false);
  if(SUCCEEDED(hr))hr=Text(s,L"Tab",4,22,302,859,54,32,Muted);if(SUCCEEDED(hr))hr=Text(s,L"Enter",6,22,900,861,86,34,Muted);if(SUCCEEDED(hr))hr=Text(s,L"Esc",4,22,1040,861,58,34,Muted);
  if(FAILED(hr))return hr;out=std::move(p);return S_OK;
 }catch(...){return E_OUTOFMEMORY;}
}
bool Build(const CalibrationStateV1& v,const Prepared& p,ui_art::Scene& out)noexcept{
 if(!Valid(v))return false;
 try{auto s=p.scene;
  if(v.flags&CalibrationLevelsKnown)for(unsigned row=v.page?1u:0u;row<(v.page?4u:1u);++row){const auto& g=v.gauges[row];if(!std::isfinite(g.current)||!std::isfinite(g.minimum)||!std::isfinite(g.maximum)||g.minimum>=g.maximum||g.current<g.minimum||g.current>g.maximum)return false;
   const auto b=Slider(row,v.page);const float y=b.y+b.h*.5f,t=(g.current-g.minimum)/(g.maximum-g.minimum);s.Line(b.x,y,b.x+b.w,y,2,0x8f9ca5ab);s.Line(b.x,y,b.x+b.w*t,y,2.5f,row==v.selected?Amber:Ink);
   for(unsigned n=0;n<=10;++n)s.Line(b.x+b.w*n/10.f,y+8,b.x+b.w*n/10.f,y+13,.8f,0x729ca5ab);
   s.Rect(b.x+b.w*t-3,y-12,6,24,row==v.selected?Amber:Ink);
  }
  if(v.opacity<1)for(auto& b:s.batches)for(auto& vertex:b.vertices)vertex.color=(vertex.color&0xffffff)|(static_cast<DWORD>(std::lround((vertex.color>>24)*v.opacity))<<24);
  out=std::move(s);return true;
 }catch(...){return false;}
}
}
