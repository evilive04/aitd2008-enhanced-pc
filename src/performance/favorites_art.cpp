#define NOMINMAX
#include "favorites_art.h"
#include "favorites_policy.h"
#include "ui_rich_keycaps.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
namespace favorites_art {
namespace {
constexpr float Pi=3.14159265358979323846f,Cx=960,Cy=496.8f,Inner=129.6f,Outer=275.4f;
constexpr D3DCOLOR Ink=0xffe9e5dc,Silver=0xb6a8b0b5,Muted=0xff929b9f,Amber=0xffe2b66f;
HRESULT Label(const wchar_t* text,float size,float width,float height,ui_theme::Label& out){
    const auto hr=ui_theme::BuildLabel(text,size,width,height,out);
    if(FAILED(hr))return hr;
    return out.mesh.advanceWidth<=width+.1f&&out.mesh.layoutHeight<=height+.1f?S_OK:E_INVALIDARG;
}
void Text(ui_art::Scene& out,const ui_theme::Label& text,float x,float y,D3DCOLOR color){
    out.Text(text,x-text.mesh.advanceWidth*.5f,y,color);
}
void Band(ui_art::Scene& out,float inner,float outer,float first,float last,D3DCOLOR color){
    const auto count=(std::max)(8u,static_cast<unsigned>(std::ceil((last-first)*outer*.8f)));
    for(unsigned n=0;n<count;++n){
        const float a=first+(last-first)*n/count,b=first+(last-first)*(n+1)/count;
        out.Polygon({{Cx+inner*std::cos(a),Cy+inner*std::sin(a)},
                     {Cx+outer*std::cos(a),Cy+outer*std::sin(a)},
                     {Cx+outer*std::cos(b),Cy+outer*std::sin(b)},
                     {Cx+inner*std::cos(b),Cy+inner*std::sin(b)}},color);
    }
}
unsigned Name(const DWORD (&icons)[3]){
    if(icons[2])return 4;if(icons[1]==1&&icons[0]==2)return 2;
    if(icons[1]==1&&icons[0]==3)return 3;if(icons[1]==1)return 1;
    if(icons[0]==2)return 5;if(icons[0]==3)return 6;
    return icons[0]||icons[1]?7:8;
}
void Equipment(ui_art::Scene& out,const Labels& labels,unsigned name,float x,float y,D3DCOLOR color){
    if(name==2||name==3){Text(out,labels.name[1],x,y,color);Text(out,labels.name[name],x,y+24,color);}
    else Text(out,labels.name[name],x,y+12,color);
}
}
HRESULT Prepare(Labels& out)noexcept{try{
    const wchar_t* titles[]={L"FAVORITES",L"EDIT FAVORITES"};
    const wchar_t* centers[]={L"FAVORITES",L"CURRENT SET"};
    for(unsigned n=0;n<2;++n){
        auto hr=Label(titles[n],38,740,58,out.title[n]);if(FAILED(hr))return hr;
        hr=Label(centers[n],24,220,42,out.center[n]);if(FAILED(hr))return hr;
    }
    for(unsigned n=0;n<6;++n){wchar_t text[8]{};swprintf_s(text,L"%u",n+1);const auto hr=Label(text,42,80,55,out.number[n]);if(FAILED(hr))return hr;}
    const wchar_t* names[]={L"EMPTY",L"PISTOL",L"+ FLASHLIGHT",L"+ THROWABLE",L"MELEE OBJECT",L"FLASHLIGHT",L"THROWABLE",L"EQUIPMENT",L"EMPTY HANDS",L"SELECT A SET"};
    for(unsigned n=0;n<10;++n){const auto hr=Label(names[n],17,120,48,out.name[n]);if(FAILED(hr))return hr;}
    const wchar_t* buttons[]={L"CANCEL",L"ADD CURRENT",L"CONFIRM"};
    for(unsigned n=0;n<3;++n){const auto hr=Label(buttons[n],23,210,40,out.button[n]);if(FAILED(hr))return hr;}
    const wchar_t* hints[]={L"MOVE OUTSIDE THE RING TO CANCEL",L"DRAG SETS TO CHANGE THEIR ORDER",L"SET ADDED — CONFIRM TO SAVE"};
    for(unsigned n=0;n<3;++n){const auto hr=Label(hints[n],22,820,42,out.hint[n]);if(FAILED(hr))return hr;}
    const wchar_t* notices[]={L"SET EQUIPPED",L"SET ADDED — CONFIRM TO SAVE",L"FAVORITES SAVED",L"SET UNAVAILABLE — ITEM MISSING",L"COULD NOT SAVE FAVORITES",L"EQUIPPING SET…"};
    for(unsigned n=0;n<6;++n){const auto hr=Label(notices[n],25,720,48,out.notice[n]);if(FAILED(hr))return hr;}
    return ui_rich_keycaps::Build(L"HOLD #color:F2E6C9FF#[;]#nocolor#  CHOOSE / RELEASE TO EQUIP",23,690,56,out.wheelHint,626,850);
}catch(...){return E_FAIL;}}
bool Valid(const FavoritesStateV1& s)noexcept{
    if(s.size!=sizeof s||s.version!=1||!(s.flags&3)||(s.flags&~3u)||s.mode>1||s.status>5||
       (s.hover>=6&&s.hover!=favorites_policy::None)||s.selected>=6||
       !std::isfinite(s.cursorX)||!std::isfinite(s.cursorY)||std::fabs(s.cursorX)>8||std::fabs(s.cursorY)>8)return false;
    for(auto icon:s.current)if(icon>5)return false;
    for(const auto& slot:s.slots){if(slot.valid>1)return false;for(auto icon:slot.icons)if(icon>5)return false;}
    return true;
}
bool Build(const FavoritesStateV1& s,const Labels& labels,ui_art::Scene& out)noexcept{try{
    out={};if(!Valid(s))return false;ui_art::Scene scene;
    if(!(s.flags&1)){
        scene.Frame(552,78,816,84);scene.Rect(578,100,2,40,Amber);
        Text(scene,labels.notice[s.status],Cx,102,s.status==3||s.status==4?Amber:Ink);
        out=std::move(scene);return true;
    }
    Text(scene,labels.title[s.mode],Cx,78,Ink);scene.Line(Cx-42,143,Cx+42,143,1.2f,Silver);
    for(unsigned n=0;n<6;++n){
        const float mid=-Pi*.5f+n*Pi/3,a=mid-Pi/6,b=mid+Pi/6;
        const bool hot=s.hover==n,selected=s.mode&&s.selected==n;
        Band(scene,Inner,Outer,a,b,hot?0xbc3c2b18:0xb50c1115);
        if(hot){for(unsigned glow=0;glow<4;++glow)scene.Arc(Cx,Cy,Outer+glow*2.f,a,b,2.f,(0x18u-glow*4u)<<24|(Amber&0xffffff));}
        scene.Arc(Cx,Cy,Outer,a,b,hot?2.f:1.f,hot?Amber:Silver);
        if(selected)scene.Arc(Cx,Cy,Inner+3,a+.025f,b-.025f,1.5f,Amber);
        scene.Line(Cx+Inner*std::cos(a),Cy+Inner*std::sin(a),Cx+Outer*std::cos(a),Cy+Outer*std::sin(a),hot?1.5f:.8f,hot?Amber:Silver);
        if(hot)scene.Line(Cx+Inner*std::cos(b),Cy+Inner*std::sin(b),Cx+Outer*std::cos(b),Cy+Outer*std::sin(b),1.5f,Amber);
        const float x=Cx+199.f*std::cos(mid),y=Cy+199.f*std::sin(mid);
        Text(scene,labels.number[n],x,y-48,hot?Amber:Ink);scene.Line(x-18,y+3,x+18,y+3,.8f,hot?Amber:Silver);
        Equipment(scene,labels,s.slots[n].valid?Name(s.slots[n].icons):0,x,y+8,hot?Ink:Muted);
    }
    std::vector<ui_theme::Point> disk;for(unsigned n=0;n<192;++n){const float a=n*Pi/96;disk.push_back({Cx+(Inner-6)*std::cos(a),Cy+(Inner-6)*std::sin(a)});}scene.Polygon(disk,0xb80b1014);
    scene.Arc(Cx,Cy,Inner-6,0,2*Pi,1.2f,Ink);scene.Arc(Cx,Cy,Inner-12,0,2*Pi,.6f,Silver);
    Text(scene,labels.center[s.mode],Cx,Cy-45,Ink);scene.Line(Cx-26,Cy-2,Cx+26,Cy-2,.8f,Silver);
    const unsigned current=s.mode?Name(s.current):s.hover<6&&s.slots[s.hover].valid?Name(s.slots[s.hover].icons):9;
    Equipment(scene,labels,current,Cx,Cy+19,Muted);
    if(s.mode){
        const float xs[]={-.30f,-.10f,.12f},ws[]={.18f,.20f,.18f};
        // Cursor/button hover is appended per frame by the overlay. Geometry
        // stays identical to the already tested input policy at every aspect.
        for(unsigned n=0;n<3;++n){const float x=Cx+1080*xs[n],w=1080*ws[n];scene.Button(x,Cy+334.8f,w,54,false);Text(scene,labels.button[n],x+w*.5f,Cy+347,Ink);}
        Text(scene,s.status==4?labels.notice[4]:labels.hint[s.status==1?2:1],Cx,Cy+426.6f,s.status==4?Amber:Muted);
    }else{
        scene.Frame(584,828,752,78);scene.batches.insert(scene.batches.end(),labels.wheelHint.batches.begin(),labels.wheelHint.batches.end());
        Text(scene,labels.hint[0],Cx,934,Muted);
    }
    out=std::move(scene);return true;
}catch(...){out={};return false;}}
void Cursor(const FavoritesStateV1& s,unsigned width,unsigned height,ui_art::Scene& out){
    if(!(s.flags&1)||!width||!height)return;
    if(s.mode){const auto hit=favorites_policy::Button(s.cursorX,s.cursorY,static_cast<float>(width)/height);
        const float xs[]={-.30f,-.10f,.12f},ws[]={.18f,.20f,.18f};
        if(hit<3){const float x=Cx+1080*xs[hit],y=Cy+334.8f,w=1080*ws[hit];
            out.Line(x+9,y+1,x+w-9,y+1,1.5f,Amber);out.Line(x+9,y+53,x+w-9,y+53,1.2f,Amber);
        }
    }
    const float scale=height/1080.f,ox=width*.5f-Cx*scale;
    ui_theme::Mesh cursor;ui_theme::Cursor(cursor,(s.cursorX*width-ox)/scale,s.cursorY*1080.f);
    for(const auto& v:cursor){if(out.batches.empty()||out.batches.back().material!=ui_art::Material::Solid)out.batches.push_back({ui_art::Material::Solid,{}});out.batches.back().vertices.push_back({v.x,v.y,v.z,v.rhw,v.color,0,0});}
}
}
