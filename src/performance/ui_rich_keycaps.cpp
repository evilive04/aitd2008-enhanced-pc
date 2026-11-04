#define NOMINMAX
#include "ui_rich_keycaps.h"
#include "pc_keycap_layout.h"
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <cmath>
namespace ui_rich_keycaps {
HRESULT Build(const wchar_t* text,float size,float width,float height,ui_art::Scene& out,float x,float y,D3DCOLOR ink,float* layoutHeight)noexcept{
 out={};if(layoutHeight)*layoutHeight=0;if(!text||!std::isfinite(size)||!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(x)||!std::isfinite(y)||size<12||size>100||width<20||height<20||width>1920||height>50000)return E_INVALIDARG;
 try{
  const auto length=wcsnlen_s(text,4096);if(!length||length==4096)return E_INVALIDARG;
  wchar_t plain[4096]{};unsigned used=0,n=0;std::vector<ui_vector_text::InlineSpan> spans;std::vector<std::wstring> nativeKeys;
  constexpr wchar_t bindingPrefix[]=L"#color:F2E6C9FF#[",bindingSuffix[]=L"]#nocolor#";
  constexpr unsigned head=sizeof(bindingPrefix)/sizeof(wchar_t)-1,tail=sizeof(bindingSuffix)/sizeof(wchar_t)-1;
  while(n<length){
   // Exact native PC binding wrappers may occur inside localized instructions.
   // Keep their labels verbatim, without guessing a device from translated text.
   if(length-n>head+tail&&!wcsncmp(text+n,bindingPrefix,head)){
    const auto* end=wcsstr(text+n+head,bindingSuffix);const auto count=end?static_cast<size_t>(end-(text+n+head)):0;
    bool valid=count>0&&count<64;
    for(size_t k=0;valid&&k<count;++k)valid=text[n+head+k]>=L' '&&text[n+head+k]!=L'['&&text[n+head+k]!=L']'&&text[n+head+k]!=L'#';
    if(valid){if(spans.size()==128||used==4095)return E_INVALIDARG;const auto id=pc_keycaps::keyCount+static_cast<unsigned>(nativeKeys.size());nativeKeys.emplace_back(text+n+head,count);spans.push_back({used,1,id,44,40,32});plain[used++]=0xfffc;n+=head+static_cast<unsigned>(count)+tail;continue;}
   }
   if(!wcsncmp(text+n,L"#nocolor#",9)){n+=9;continue;}
   if(!wcsncmp(text+n,L"#color:",7)){if(length-n<16)return E_INVALIDARG;unsigned k=0;while(k<8&&iswxdigit(text[n+7+k]))++k;if(k!=8||text[n+15]!=L'#')return E_INVALIDARG;n+=16;continue;}
   if(!wcsncmp(text+n,L"#bitmap:",8)){
    if(length-n<19||wcsncmp(text+n,L"#bitmap:epc_key_",16)||text[n+16]<L'0'||text[n+16]>L'9'||text[n+17]<L'0'||text[n+17]>L'9'||text[n+18]!=L'#'||spans.size()==128||used==4095)return E_INVALIDARG;
    const unsigned id=(text[n+16]-L'0')*10u+text[n+17]-L'0';if(id>=pc_keycaps::keyCount)return E_INVALIDARG;spans.push_back({used,1,id,44,40,32});plain[used++]=0xfffc;n+=19;continue;
   }
   if(used==4095)return E_INVALIDARG;plain[used++]=text[n++];
  }
  if(n==4096)return E_INVALIDARG;std::vector<ui_theme::Label> keys(pc_keycaps::keyCount+nativeKeys.size());
  const auto mouseSlot=[&](unsigned id)->int{if(id<pc_keycaps::keyCount){if(id==pc_keycaps::keyCount-2)return 5;if(id==pc_keycaps::keyCount-1)return 6;return -1;}const auto& name=nativeKeys[id-pc_keycaps::keyCount];return name==L"MOUSE LEFT"?5:name==L"MOUSE RIGHT"?6:-1;};
  for(auto& span:spans){if(mouseSlot(span.id)>=0){span.width=47.f;continue;}auto& key=keys[span.id];if(key.mesh.triangles.empty()){wchar_t label[64]{};if(span.id>=pc_keycaps::keyCount)wcscpy_s(label,nativeKeys[span.id-pc_keycaps::keyCount].c_str());else{const auto* value=pc_keycaps::keys[span.id].label;for(unsigned k=0;value[k];++k)label[k]=value[k];}auto hr=ui_theme::BuildLabel(label,23,440,36,key,L"Segoe UI");if(FAILED(hr))return hr;}span.width=(std::max)(44.f,key.mesh.advanceWidth+20);}
  ui_vector_text::Options options;options.family=L"Times New Roman";options.size=size;options.width=width;options.height=height;options.tolerance=.065f;options.maxTriangles=524288;options.inlineSpans=spans.data();options.inlineCount=static_cast<unsigned>(spans.size());ui_theme::Label label;auto hr=ui_theme::BuildRichLabel(plain,options,label);
  if(FAILED(hr)||label.mesh.layoutHeight>height+.125f||label.mesh.ink.right>width+.125f||label.mesh.ink.bottom>height+.125f)return FAILED(hr)?hr:E_INVALIDARG;
  ui_art::Scene s;s.Text(label,x,y,ink);for(const auto& object:label.mesh.inlineObjects){if(object.x<0||object.y<0||object.x+object.width>width+.125f||object.y+object.height>height+.125f)return E_INVALIDARG;if(const auto slot=mouseSlot(object.id);slot>=0){s.Image(static_cast<unsigned>(slot),65,55,x+object.x,y+object.y,object.width,object.height);continue;}const auto& key=keys[object.id];s.Button(x+object.x+2,y+object.y+2,object.width-4,object.height-4,false);s.Text(key,x+object.x+(object.width-key.mesh.advanceWidth)*.5f,y+object.y+5,ink);}if(layoutHeight)*layoutHeight=label.mesh.layoutHeight;out=std::move(s);return S_OK;
 }catch(...){return E_FAIL;}
}
}
