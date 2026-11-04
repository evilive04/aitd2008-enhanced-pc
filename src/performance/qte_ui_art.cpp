#define NOMINMAX
#include "qte_ui_art.h"
#include "qte_ui_content.h"
#include "ui_rich_keycaps.h"
#include <algorithm>
#include <cmath>
namespace qte_ui_art{
static constexpr float Pi=3.14159265358979323846f;
static HRESULT Label(ui_art::Scene& s,const wchar_t* text,float x,float y,float size,float width,D3DCOLOR ink=0xffe9e5dc){ui_theme::Label v;const auto hr=ui_theme::BuildLabel(text,size,width,100,v,L"Times New Roman");if(SUCCEEDED(hr))s.Text(v,x,y,ink);return hr;}
static void Mouse(ui_art::Scene& s,float x,float y,bool both){
 // Own scalable outline; no original console icon or stretched image.
 s.Arc(x+32,y+30,30,Pi,2*Pi,1.3f,0xffb7bec1);s.Line(x+2,y+30,x+2,y+66,1.3f,0xffb7bec1);s.Line(x+62,y+30,x+62,y+66,1.3f,0xffb7bec1);s.Arc(x+32,y+66,30,0,Pi,1.3f,0xffb7bec1);
 s.Line(x+32,y+3,x+32,y+45,1.2f,0xffb7bec1);s.Line(x+4,y+45,x+60,y+45,1.2f,0xffb7bec1);s.Rect(x+11,y+19,14,17,0xffe2b66f);if(both)s.Rect(x+39,y+19,14,17,0xffe2b66f);
}
HRESULT Build(const QteUiStateV1& v,ui_art::Scene& out)noexcept{
 out={};if(v.size!=sizeof v||v.version!=1||!(v.flags&QteUiVisible)||(v.flags&~7u)||v.kind<1||v.kind>6)return E_INVALIDARG;
 if(v.kind==QteUiCaption&&((v.flags&QteUiTimingOpen)||v.nativeFrame<10||v.nativeFrame>24||!v.phase||v.phase>69||v.key[0]||!qte_ui_content::HasNativePcKey(v.body,1024)))return E_INVALIDARG;
 if(v.kind==QteUiButton&&(v.nativeFrame<10||v.nativeFrame>24||!v.phase||v.phase>69||!v.body[0]||!v.key[0]||std::find(v.body,v.body+1024,L'\0')==v.body+1024||std::find(v.key,v.key+256,L'\0')==v.key+256))return E_INVALIDARG;
 if(v.kind==QteUiCompression&&(v.nativeFrame<25||v.nativeFrame>78||v.phase<139||v.phase>266||!v.pulse||v.pulse>10000))return E_INVALIDARG;
 if(v.kind==QteUiBreathing&&(v.nativeFrame<79||v.nativeFrame>106||v.phase<180||v.phase>360||!v.meter1||v.meter1>300||!v.meter2||v.meter2>300))return E_INVALIDARG;
 if(v.kind==QteUiSteering&&(v.flags!=QteUiVisible||v.nativeFrame<107||v.nativeFrame>130||v.phase<156||v.phase>310||v.pulse!=(v.phase<165?0u:v.phase<238?1u:2u)||!v.key[0]||!v.body[0]||std::find(v.key,v.key+256,L'\0')==v.key+256||std::find(v.body,v.body+1024,L'\0')==v.body+1024))return E_INVALIDARG;
 if(v.kind==QteUiCables&&(v.flags&QteUiRepeat||v.nativeFrame<131||v.nativeFrame>146||v.phase<218||v.phase>454||v.meter1>159||!!(v.flags&QteUiTimingOpen)!=(v.phase>=410&&v.phase<=423)||v.pulse!=((v.flags&QteUiTimingOpen)||v.meter1?1u:0u)||!v.key[0]||!v.body[0]||std::find(v.key,v.key+256,L'\0')==v.key+256||std::find(v.body,v.body+1024,L'\0')==v.body+1024))return E_INVALIDARG;
 try{
  ui_art::Scene s;HRESULT hr=S_OK;
  if(v.kind==QteUiCaption){
   ui_art::Scene text;hr=ui_rich_keycaps::Build(v.body,31,820,250,text);if(FAILED(hr))return hr;float bottom=0;
   for(const auto& batch:text.batches)for(const auto& point:batch.vertices)bottom=(std::max)(bottom,point.y);
   const float h=(std::max)(142.f,bottom+68),x=500.f,y=920-h;s.Frame(x,y,920,h);s.Line(x+24,y+30,x+24,y+h-30,2,0xffe2b66f);
   for(auto& batch:text.batches){for(auto& point:batch.vertices){point.x+=x+50;point.y+=y+34;}s.batches.push_back(std::move(batch));}
  }else if(v.kind==QteUiButton){
   ui_art::Scene text,key;hr=ui_rich_keycaps::Build(v.body,30,820,180,text);if(SUCCEEDED(hr))hr=ui_rich_keycaps::Build(v.key,32,700,100,key);if(FAILED(hr))return hr;
   float bottom=0;for(const auto& b:text.batches)for(const auto& p:b.vertices)bottom=(std::max)(bottom,p.y);
   const float h=(std::max)(220.f,bottom+158.f),x=500.f,y=920-h;s.Frame(x,y,920,h);
   for(auto& b:text.batches){for(auto& p:b.vertices){p.x+=x+50;p.y+=y+34;}s.batches.push_back(std::move(b));}
   for(auto& b:key.batches){for(auto& p:b.vertices){p.x+=x+50;p.y+=y+h-90;}s.batches.push_back(std::move(b));}
   hr=Label(s,v.flags&QteUiRepeat?L"Press repeatedly":L"Press",x+540,y+h-84,26,320,0xffe2b66f);
  }else if(v.kind==QteUiSteering){
   s.Frame(540,790,840,200);
   hr=Label(s,L"Left",650,815,28,260,v.pulse==1?0xffe2b66f:0xffe9e5dc);
   if(SUCCEEDED(hr))hr=Label(s,L"Right",1090,815,28,260,v.pulse==2?0xffe2b66f:0xffe9e5dc);
   for(unsigned n=0;n<2&&SUCCEEDED(hr);++n){
    const float x=n?1050.f:620.f;ui_art::Scene key;hr=ui_rich_keycaps::Build(n?v.body:v.key,32,270,80,key);if(FAILED(hr))break;
    for(auto& b:key.batches){for(auto& p:b.vertices){p.x+=x;p.y+=873;}s.batches.push_back(std::move(b));}
    const float sign=n?1.f:-1.f,cx=n?1265.f:590.f;const auto ink=v.pulse==n+1?0xffe2b66fu:0x9ab7bec1u;
    s.Line(cx-sign*20,830,cx+sign*20,830,1.5f,ink);s.Line(cx+sign*20,830,cx,814,1.5f,ink);s.Line(cx+sign*20,830,cx,846,1.5f,ink);
   }
  }else if(v.kind==QteUiCables){
   ui_art::Scene text;hr=ui_rich_keycaps::Build(v.body,30,800,80,text);if(FAILED(hr))return hr;s.Frame(480,705,960,300);
   for(auto& b:text.batches){for(auto& p:b.vertices){p.x+=530;p.y+=739;}s.batches.push_back(std::move(b));}
   for(unsigned n=0;n<2&&SUCCEEDED(hr);++n){const float x=530+n*160.f;s.Button(x,840,130,48,false);hr=Label(s,n?L"RMB":L"LMB",x+22,850,27,110);}
   // Independent physical mouse buttons select the wire. Vertical mouse input
   // is the original PC85A1D0 path; this is not CPR's simultaneous-button cue.
   s.Arc(1295,855,28,Pi,2*Pi,1.3f,0xffb7bec1);s.Line(1267,855,1267,902,1.3f,0xffb7bec1);s.Line(1323,855,1323,902,1.3f,0xffb7bec1);s.Arc(1295,902,28,0,Pi,1.3f,0xffb7bec1);s.Line(1295,830,1295,869,1.1f,0xffb7bec1);s.Line(1269,869,1321,869,1.1f,0xffb7bec1);
   for(float sign:{-1.f,1.f}){const float y=875+sign*65;s.Line(1370,y-sign*17,1370,y+sign*17,1.5f,0xffe2b66f);s.Line(1370,y+sign*17,1357,y,1.5f,0xffe2b66f);s.Line(1370,y+sign*17,1383,y,1.5f,0xffe2b66f);}
   if(SUCCEEDED(hr)&&v.pulse){ui_art::Scene key;hr=ui_rich_keycaps::Build(v.key,30,270,46,key);if(SUCCEEDED(hr)){for(auto& b:key.batches){for(auto& p:b.vertices){p.x+=530;p.y+=926;}s.batches.push_back(std::move(b));}hr=Label(s,L"Accelerate",830,931,26,360,0xffe2b66f);s.Rect(530,984,700,3,0x607e888e);}}
  }else{
   s.Frame(540,740,840,270);Mouse(s,585,824,v.kind==QteUiCompression);
   hr=Label(s,v.kind==QteUiCompression?L"RESUSCITATION":L"BREATHING",585,765,23,720,0xffe2b66f);
   if(SUCCEEDED(hr))hr=Label(s,v.kind==QteUiCompression?L"Press both mouse buttons together":L"Hold and release the left mouse button",690,815,30,640);
   if(SUCCEEDED(hr)&&v.kind==QteUiCompression){wchar_t text[64]{};swprintf_s(text,L"Compression %lu",v.pulse);hr=Label(s,text,690,860,23,620,0xffb7bec1);if(SUCCEEDED(hr))hr=Label(s,L"Time the press within the amber interval",690,897,23,620,0xffb7bec1);s.Rect(690,947,620,3,0x607e888e);s.Line(690+620*28/120.f,940,690+620*88/120.f,940,3,0xbbe2b66f);}
   if(SUCCEEDED(hr)&&v.kind==QteUiBreathing){s.Rect(690,898,620,6,0x607e888e);s.Rect(690,936,620,6,0x607e888e);}
  }
  if(FAILED(hr))return hr;out=std::move(s);return S_OK;
 }catch(...){return E_FAIL;}
}
void Animate(const QteUiStateV1& v,ui_art::Scene& s){
 if(v.kind==QteUiCompression&&v.phase>=146&&v.phase<=266){const auto x=690+620*(v.phase-146)/120.f;s.Line(x,932,x,960,2.2f,v.flags&QteUiTimingOpen?0xffe2b66f:0xffe9e5dc);if(v.flags&QteUiTimingOpen)s.Rect(582,916,70,2,0xffe2b66f);}
 if(v.kind==QteUiBreathing){s.Rect(690,898,620*(v.meter1-1)/299.f,6,0xffe2b66f);s.Rect(690,936,620*(v.meter2-1)/299.f,6,0xffb7bec1);}
 if(v.kind==QteUiCables&&v.pulse&&v.meter1>=7){const auto frame=(std::min<DWORD>)(73u,v.meter1);s.Rect(530,984,700*(frame-7)/66.f,3,0xffe2b66f);} // Actual native charge animation; cablesok is frame73.
}
}
