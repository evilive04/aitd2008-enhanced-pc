#define NOMINMAX
#include "interaction_ui_art.h"
#include "ui_rich_keycaps.h"
#include <algorithm>
namespace interaction_ui_art{
template<size_t N>static bool Text(const wchar_t(&s)[N]){return std::find(s,s+N,L'\0')!=s+N;}
static HRESULT Label(ui_art::Scene& s,const wchar_t* text,float size,float width,float height,float x,float y,D3DCOLOR ink){ui_art::Scene p;const auto hr=ui_rich_keycaps::Build(text,size,width,height,p,x,y,ink);if(SUCCEEDED(hr))for(auto& b:p.batches)s.batches.push_back(std::move(b));return hr;}
HRESULT Build(const InteractionUiStateV1& v,ui_art::Scene& out)noexcept{
 out={};if(v.size!=sizeof v||v.version!=1||!(v.flags&InteractionUiVisible)||(v.flags&~7u)||v.action>24||v.traits>31||v.bitmap<16||v.bitmap>58||(v.bitmap&1)||!Text(v.key)||!v.key[0]||!Text(v.label)||!v.label[0]||!Text(v.unavailable))return E_INVALIDARG;
 try{
  const bool allowed=(v.flags&InteractionUiAllowed)!=0;const auto ink=allowed?0xffe9e5dcu:0xff9fa4a4u;
  ui_art::Scene s;s.Rect(590,888,2,54,allowed?0xffe2b66f:0xff8e7970);
  ui_art::Scene key;auto hr=ui_rich_keycaps::Build(v.key,24,260,56,key,612,894,ink);float keyRight=612;
  if(SUCCEEDED(hr)){for(auto& batch:key.batches){for(const auto& vertex:batch.vertices)keyRight=(std::max)(keyRight,vertex.x);s.batches.push_back(std::move(batch));}}
  if(SUCCEEDED(hr))hr=Label(s,v.label,28,540,64,keyRight+18,892,ink);
  if(v.unavailable[0]&&SUCCEEDED(hr))hr=Label(s,v.unavailable,24,720,86,602,963,0xffb7aaa1);
  // Exact native helper textures, fixed square canvas and UV extent. The
  // object/action pictogram and its traits form one centered group.
  s.ImageRegion(0,128,128,{0,0,128,128},454,872,88,88);
  if(v.traits&1)s.ImageRegion(1,64,64,{0,0,54,54},550,856,38,38);
  if(v.traits&2)s.ImageRegion(2,64,64,{0,0,54,54},550,906,38,38);
  for(unsigned n=0;n<3;++n)if(v.traits&(4u<<n))s.ImageRegion(3,64,64,{0,0,54,54},450+n*31.f,963,28,28);
  if(FAILED(hr))return hr;
  // Move the whole group, including its unscaled original textures and any
  // unavailable reason, above the native lower instruction. No hint is hidden.
  if(v.flags&InteractionUiAboveNativeHint)for(auto& batch:s.batches)for(auto& vertex:batch.vertices)vertex.y-=180.f;
  out=std::move(s);return S_OK;
 }catch(...){return E_FAIL;}
}
}
