#define NOMINMAX
#include "episode_end_art.h"
#include <algorithm>
namespace episode_end_art {
ui_art::Bounds Row(unsigned n)noexcept{return {640,504+n*92.f,640,70};}
HRESULT Build(const EpisodeEndStateV1& v,ui_art::Scene& out)noexcept{
 out={};if(v.size!=sizeof v||v.version!=1||!(v.flags&EpisodeEndVisible)||v.selected>=2||!v.title[0]||!v.select[0]||!v.buttons[0][0]||!v.buttons[1][0]||std::find(v.title,v.title+128,L'\0')==v.title+128||std::find(v.select,v.select+64,L'\0')==v.select+64)return E_INVALIDARG;
 try{ui_art::Scene s;s.Frame(570,305,780,470);ui_theme::Label heading,title;auto hr=ui_theme::BuildLabel(L"EPISODE COMPLETE",40,640,60,heading,L"Times New Roman");if(SUCCEEDED(hr))hr=ui_theme::BuildLabel(v.title,30,640,76,title,L"Times New Roman");if(FAILED(hr)||title.mesh.layoutHeight>76.125f||title.mesh.ink.right>640.125f)return FAILED(hr)?hr:E_INVALIDARG;s.Text(heading,640,345,0xffe9e5dc);s.Line(640,411,1280,411,.8f,0x9ac4c6c3);s.Text(title,640,426,0xffe9e5dc);
  for(unsigned n=0;n<2;++n){if(std::find(v.buttons[n],v.buttons[n]+128,L'\0')==v.buttons[n]+128)return E_INVALIDARG;ui_theme::Label label;hr=ui_theme::BuildLabel(v.buttons[n],35,594,58,label,L"Times New Roman");if(FAILED(hr)||label.mesh.layoutHeight>58.125f||label.mesh.ink.right>594.125f)return FAILED(hr)?hr:E_INVALIDARG;const auto r=Row(n);const bool selected=n==v.selected;if(selected){s.Rect(r.x,r.y,2,r.h,0xffe2b66f);s.Rect(r.x+2,r.y,r.w-2,r.h,0x16e2b66f);}s.Text(label,r.x+20,r.y+14,selected?0xffe2b66f:0xffe9e5dc);}
  ui_theme::Label key,label;hr=ui_theme::BuildLabel(L"Enter",23,86,40,key,L"Segoe UI");if(SUCCEEDED(hr))hr=ui_theme::BuildLabel(v.select,25,440,42,label,L"Times New Roman");if(FAILED(hr)||label.mesh.ink.right>440.125f||label.mesh.layoutHeight>42.125f)return FAILED(hr)?hr:E_INVALIDARG;s.Button(640,705,100,40,false);s.Text(key,652,709,0xffe9e5dc);s.Text(label,760,711,0xffe9e5dc);out=std::move(s);return S_OK;
 }catch(...){return E_FAIL;}
}
}
