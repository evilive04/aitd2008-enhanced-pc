#define NOMINMAX
#include "phone_content_art.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include "pc_keycap_layout.h"
#include "ui_rich_keycaps.h"
namespace phone_content_art {
constexpr D3DCOLOR Ink=0xffe9e5dc,Muted=0xff818b92;
static bool ParseBody(const wchar_t* in,wchar_t(&out)[4096],std::vector<ui_vector_text::InlineSpan>* objects){
 out[0]=0;if(!in)return false;unsigned count=0;
 for(unsigned n=0;n<4096&&in[n];){
  if(!wcsncmp(in+n,L"#nocolor#",9)){n+=9;continue;}
  if(!wcsncmp(in+n,L"#color:",7)){unsigned k=0;while(k<8&&in[n+7+k]&&iswxdigit(in[n+7+k]))++k;if(k!=8||in[n+15]!=L'#')return false;n+=16;continue;}
  if(!wcsncmp(in+n,L"#bitmap:",8)){
   if(wcsncmp(in+n,L"#bitmap:epc_key_",16)||in[n+16]<L'0'||in[n+16]>L'9'||in[n+17]<L'0'||in[n+17]>L'9'||in[n+18]!=L'#'||count==4095)return false;
   const unsigned id=(in[n+16]-L'0')*10u+in[n+17]-L'0';if(id>=pc_keycaps::keyCount)return false;
   if(objects){if(objects->size()==128)return false;objects->push_back({count,1,id,44,40,32});}
   out[count++]=0xfffc;n+=19;continue;
  }
  if(count==4095)return false;out[count++]=in[n++];
 }out[count]=0;return true;
}
bool PlainBody(const wchar_t* in,wchar_t(&out)[4096])noexcept{try{return ParseBody(in,out,nullptr);}catch(...){out[0]=0;return false;}}
ui_art::Bounds Row(unsigned n)noexcept{return {260,304+n*54.f,1400,52};}
ui_art::Bounds DialKey(unsigned n)noexcept{return {672+(n%3)*198.f,374+(n/3)*114.f,180,96};}
static HRESULT Label(ui_art::Scene& scene,const wchar_t* text,float size,float width,float height,float x,float y,D3DCOLOR color){
 ui_theme::Label label;auto hr=ui_theme::BuildLabel(text,size,width,height,label,L"Times New Roman");
 if(FAILED(hr)||label.mesh.advanceWidth>width+.125f||label.mesh.layoutHeight>height+.125f||label.mesh.ink.right>width+.125f||label.mesh.ink.bottom>height+.125f)return FAILED(hr)?hr:E_INVALIDARG;
 scene.Text(label,x,y,color);return S_OK;
}
static bool Valid(const PhoneContentView& v){
 if(v.kind!=PhoneContentKind::List&&v.kind!=PhoneContentKind::Reader&&v.kind!=PhoneContentKind::Call&&v.kind!=PhoneContentKind::Dialer&&v.kind!=PhoneContentKind::Gps)return false;
 if(v.kind!=PhoneContentKind::Dialer&&(v.count>10||v.total>4096||v.offset>v.total||v.count>v.total-v.offset||(v.count&&v.selected>=v.count)))return false;
 for(const auto* text:{v.title,v.subtitle,v.body,v.back,v.open}){const unsigned cap=text==v.title?96:text==v.subtitle?256:text==v.body?4096:64;if(std::find(text,text+cap,L'\0')==text+cap)return false;}
 if(!v.title[0]||!v.back[0]||!v.open[0])return false;
 if(v.kind==PhoneContentKind::Gps)return !v.count&&!v.total&&!v.offset&&!v.selected&&std::find(v.rows[0].title,v.rows[0].title+256,L'\0')!=v.rows[0].title+256&&v.rows[0].title[0]&&!!v.body[0]==!!v.subtitle[0];
 if(v.kind==PhoneContentKind::Dialer){if(v.count||v.total||!v.offset||v.offset>10||wcslen(v.body)>v.offset)return false;for(const wchar_t* p=v.body;*p;++p)if(*p<L'0'||*p>L'9')return false;return true;}
 if(v.kind==PhoneContentKind::List)for(unsigned n=0;n<v.count;++n)if(std::find(v.rows[n].title,v.rows[n].title+256,L'\0')==v.rows[n].title+256||!v.rows[n].title[0]||v.rows[n].read>1)return false;
 return v.kind!=PhoneContentKind::Reader||v.total>0;
}
HRESULT Prepare(const PhoneContentView& v,Prepared& out)noexcept{
 out={};if(!Valid(v))return E_INVALIDARG;
 try{
  if(v.kind==PhoneContentKind::Gps){
   Prepared p;auto& s=p.chrome;s.Frame(57,58,1806,148);s.Frame(57,958,1806,76);
   // Original full-map normalized viewport from54F900. Preserve both axes,
   // image UVs and projection; this outline never fills/crops map pixels.
   constexpr float left=.056f*1920,top=.214f*1080,right=(.056f+.8898f)*1920,bottom=(.214f+.6313f)*1080;
   for(const auto& edge:{std::array<float,4>{left,top,right,top},{right,top,right,bottom},{right,bottom,left,bottom},{left,bottom,left,top}})s.Line(edge[0],edge[1],edge[2],edge[3],1.1f,0xb8d9dad6);
   auto hr=Label(s,v.title,64,700,96,130,88,Ink);
   if(SUCCEEDED(hr)&&v.subtitle[0])hr=Label(s,v.subtitle,26,760,42,930,73,Muted);
   if(SUCCEEDED(hr)&&v.body[0])hr=Label(s,v.body,30,760,82,930,112,Ink);
   const wchar_t* keys[]={L"Esc",L"LMB",L"Wheel"};const wchar_t* labels[]={v.back,v.rows[0].title,v.open};const float positions[]={110,620,1190};
   for(unsigned n=0;n<3&&SUCCEEDED(hr);++n){const float x=positions[n],width=n==2?114.f:84.f;s.Button(x,977,width,38,false);hr=Label(s,keys[n],23,width-12,37,x+8,980,Ink);if(SUCCEEDED(hr))hr=Label(s,labels[n],28,n==2?430.f:350.f,45,x+width+18,978,Ink);}
   if(FAILED(hr))return hr;out=std::move(p);return S_OK;
  }
  Prepared p;auto& s=p.chrome;const bool dial=v.kind==PhoneContentKind::Dialer;s.Frame(57,58,1806,148);s.Frame(dial?520.f:210.f,228,dial?880.f:1500.f,666);s.Frame(57,958,1806,76);
  auto hr=Label(s,v.title,64,1530,96,130,88,Ink);
  if(SUCCEEDED(hr))hr=dial?Label(s,v.body,48,600,72,672,266,Ink):Label(s,v.subtitle,40,1390,60,260,247,Ink);
  s.Button(110,977,74,38,false);if(SUCCEEDED(hr))hr=Label(s,L"Esc",23,100,37,118,980,Ink);
  if(SUCCEEDED(hr))hr=Label(s,v.back,28,280,45,203,978,Ink);
  if(dial){
   s.Line(672,345,1248,345,1,0x807e888e);
   for(unsigned n=0;n<12&&SUCCEEDED(hr);++n){const auto b=DialKey(n);s.Button(b.x,b.y,b.w,b.h,false);wchar_t digit[2]{static_cast<wchar_t>(n==10?L'0':L'1'+n),0};
    const wchar_t* text=n==9?v.subtitle:n==11?v.open:digit;hr=Label(s,text,n==9||n==11?28.f:44.f,160,70,b.x+20,b.y+21,Ink);
   }
   s.Button(560,977,166,38,false);if(SUCCEEDED(hr))hr=Label(s,L"Backspace",23,150,37,570,980,Ink);if(SUCCEEDED(hr))hr=Label(s,v.subtitle,28,280,45,746,978,Ink);
   s.Button(1180,977,110,38,false);if(SUCCEEDED(hr))hr=Label(s,L"Enter",23,100,37,1190,980,Ink);if(SUCCEEDED(hr))hr=Label(s,v.open,28,280,45,1310,978,Ink);
  }
  if(v.total&&(v.kind==PhoneContentKind::Reader||v.total>10)){
   const bool previous=v.kind==PhoneContentKind::Reader?v.offset+v.selected>0:v.offset>0;
   const bool next=v.kind==PhoneContentKind::Reader?v.offset+v.selected+1<v.total:v.offset+v.count<v.total;
   for(unsigned n=0;n<2&&SUCCEEDED(hr);++n){const float x=1010+n*190.f;const bool active=n?next:previous;s.Button(x,977,150,38,false,active);hr=Label(s,v.kind==PhoneContentKind::Reader?(n?L"Right":L"Left"):(n?L"PgDn":L"PgUp"),23,132,37,x+10,980,active?Ink:Muted);}
  }
  if(v.kind==PhoneContentKind::List){
   for(unsigned n=0;n<v.count&&SUCCEEDED(hr);++n){const auto b=Row(n);if(n==v.selected){s.Rect(b.x,b.y,b.w,b.h,0x16e2b66f);s.Line(b.x,b.y,b.x,b.y+b.h,2,0xffe2b66f);}s.Line(b.x,b.y+b.h,b.x+b.w,b.y+b.h,1,0x257e888e);
    if(!v.rows[n].read)s.Arc(b.x+18,b.y+26,3,0,6.2831853f,3,0xffe2b66f);
    hr=Label(s,v.rows[n].title,32,1330,50,b.x+38,b.y+4,Ink);
   }
   if(v.count&&SUCCEEDED(hr)){s.Button(560,977,108,38,false);hr=Label(s,L"Enter",23,100,37,571,980,Ink);if(SUCCEEDED(hr))hr=Label(s,v.open,28,280,45,688,978,Ink);}
  }
  wchar_t counter[64]{};swprintf_s(counter,L"%lu / %lu",v.total?v.offset+v.selected+1:0,v.total);
  if(v.kind!=PhoneContentKind::Call&&!dial&&SUCCEEDED(hr))hr=Label(s,counter,26,220,42,1450,980,Muted);
  if(!dial&&(v.kind==PhoneContentKind::Reader||!v.count)&&SUCCEEDED(hr)){
   if(v.body[0])hr=ui_rich_keycaps::Build(v.body,32,1360,50000,p.body,0,0,Ink,&p.bodyHeight);
   if(FAILED(hr)||!std::isfinite(p.bodyHeight))return FAILED(hr)?hr:E_INVALIDARG;
   if(p.bodyHeight>540&&SUCCEEDED(hr)){s.Button(1440,906,194,34,false);hr=Label(s,L"Up / Down",22,176,34,1450,907,Muted);}
  }
  if(FAILED(hr))return hr;out=std::move(p);return S_OK;
 }catch(...){return E_FAIL;}
}
// Sutherland-Hodgman clipping against the reader's horizontal viewport edges.
// Text X was measured inside1360 DIPs; only scroll Y can cross this box.
static void Clip(ui_art::Scene& s,const ui_art::Vertex* t,float y,ui_art::Material material){
 ui_art::Vertex p[8]{t[0],t[1],t[2]},next[8]{};for(unsigned n=0;n<3;++n){p[n].x+=280;p[n].y+=y;}unsigned count=3;
 for(unsigned edge=0;edge<2;++edge){unsigned size=0;const float bound=edge?868.f:328.f;auto a=p[count-1];bool ai=edge?a.y<=bound:a.y>=bound;
  for(unsigned n=0;n<count;++n){const auto b=p[n];const bool bi=edge?b.y<=bound:b.y>=bound;if(ai!=bi){const float f=(bound-a.y)/(b.y-a.y);auto v=a;v.x=a.x+(b.x-a.x)*f;v.y=bound;v.u=a.u+(b.u-a.u)*f;v.v=a.v+(b.v-a.v)*f;v.color=0;
    for(unsigned shift=0;shift<32;shift+=8){const auto ac=static_cast<float>((a.color>>shift)&255),bc=static_cast<float>((b.color>>shift)&255);v.color|=static_cast<DWORD>(std::lround(ac+(bc-ac)*f))<<shift;}next[size++]=v;
   }if(bi)next[size++]=b;a=b;ai=bi;}count=size;if(!count)return;for(unsigned n=0;n<count;++n)p[n]=next[n];
 }if(count>=3){if(s.batches.empty()||s.batches.back().material!=material)s.batches.push_back({material,{}});auto& target=s.batches.back().vertices;for(unsigned n=1;n+1<count;++n){target.push_back(p[0]);target.push_back(p[n]);target.push_back(p[n+1]);}}
}
void Build(const PhoneContentView& v,const Prepared& p,float scroll,ui_art::Scene& s){
 s=p.chrome;scroll=std::isfinite(scroll)?(std::max)(0.f,(std::min)(scroll,(std::max)(0.f,p.bodyHeight-540.f))):0;
 if(v.kind!=PhoneContentKind::Gps&&v.kind!=PhoneContentKind::Dialer&&(v.kind==PhoneContentKind::Reader||!v.count))for(const auto& batch:p.body.batches)for(size_t n=0;n+2<batch.vertices.size();n+=3)Clip(s,batch.vertices.data()+n,328-scroll,batch.material);
 if(p.bodyHeight>540){const float h=540*540/p.bodyHeight,travel=540-h,t=scroll/(p.bodyHeight-540);s.Line(1674,328,1674,868,1,0x40818b92);s.Rect(1671,328+travel*t,6,h,0xffb7b6a9);}
}
}
