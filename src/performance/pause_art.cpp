#include "pause_art.h"
#include <cwchar>
#include <algorithm>
namespace pause_art {
static HRESULT Detail(const wchar_t* text,float size,float height,ui_theme::Label& out){
    // Native chapter summaries vary in length. Reflow the complete text at a
    // smaller type size rather than rejecting the entire authenticated pause.
    for(float fitted=size;;fitted=(std::max)(14.f,fitted-2.f)){
        ui_theme::Label label;const auto hr=ui_theme::BuildLabel(text,fitted,650,height,label);
        if(SUCCEEDED(hr)&&label.mesh.advanceWidth<=650.125f&&label.mesh.layoutHeight<=height+.125f){out=std::move(label);return S_OK;}
        if(FAILED(hr)&&hr!=E_INVALIDARG)return hr;if(fitted<=14.f)return E_INVALIDARG;
    }
}
ui_art::Bounds Row(unsigned n)noexcept{return n<6?ui_art::Bounds{135,328+80.f*n,665,70}:ui_art::Bounds{};}
unsigned Hit(float x,float y)noexcept{for(unsigned n=0;n<6;++n)if(Row(n).Contains(x,y))return n;return ~0u;}
unsigned Navigate(unsigned selected,int direction,DWORD mask)noexcept{if(selected>=6)selected=0;for(unsigned n=0;n<6;++n){selected=(selected+(direction<0?5u:1u))%6;if(mask&(1u<<(Slots[selected]-1)))return selected;}return ~0u;}
HRESULT Prepare(const PauseStateV1& s,Labels& out)noexcept{
    if(s.size!=sizeof s||s.version!=1||!(s.flags&PauseVisible)||!(s.enabledMask&1)||s.enabledMask>63)return E_INVALIDARG;
    try{
        if(!wmemchr(s.title,0,128)||!s.title[0])return E_INVALIDARG;
        Labels p;auto hr=ui_theme::BuildLabel(s.title,76,690,115,p.title);
        for(unsigned i=0;i<6&&SUCCEEDED(hr);++i){const auto* label=s.labels[Slots[i]-1];if(!wmemchr(label,0,128)||!label[0])return E_INVALIDARG;hr=ui_theme::BuildLabel(label,40,590,65,p.rows[i]);if(SUCCEEDED(hr)&&(p.rows[i].mesh.lineCount!=1||p.rows[i].mesh.advanceWidth>590))return E_INVALIDARG;}
        const struct{const wchar_t* text;ui_theme::Label* label;float width;} footers[]={{L"Enter",&p.enter,70},{L"Select",&p.select,135},{L"Esc",&p.escape,60},{L"Resume",&p.resume,160}};
        for(const auto& f:footers)if(SUCCEEDED(hr))hr=ui_theme::BuildLabel(f.text,21,f.width,42,*f.label);
        if(s.flags&PauseDetails){
            const struct{const wchar_t* text;unsigned cap;float size,height;ui_theme::Label* out;} details[]={
                {s.episode,128,41,125,&p.episode},{s.sequence,128,30,105,&p.sequence},{s.synopsis,768,27,280,&p.synopsis}};
            for(const auto& v:details){if(!wmemchr(v.text,0,v.cap)||!v.text[0])return E_INVALIDARG;
                if(SUCCEEDED(hr))hr=Detail(v.text,v.size,v.height,*v.out);}
        }
        if(FAILED(hr))return hr;out=std::move(p);return S_OK;
    }catch(...){return E_OUTOFMEMORY;}
}
void Build(const PauseStateV1& s,const Labels& labels,unsigned selected,ui_art::Scene& out){
    out.batches.clear();out.Frame(90,130,770,800);out.Text(labels.title,135,202,0xffe9e5dc);out.Line(135,305,800,305,.8f,0x9ac4c6c3);
    for(unsigned n=0;n<6;++n){const auto b=Row(n);const bool enabled=(s.enabledMask&(1u<<(Slots[n]-1)))!=0;
        if(selected==n&&enabled){out.Rect(b.x,b.y,2,b.h,0xffe2b66f);out.Rect(b.x+2,b.y,b.w-2,b.h,0x15e2b66f);out.Line(b.x,b.y+b.h,b.x+b.w,b.y+b.h,.8f,0xb0e2b66f);}
        out.Text(labels.rows[n],b.x+22,b.y+9,enabled?selected==n?0xffe2b66f:0xffe9e5dc:0xa17b8389);
    }
    out.Button(135,865,86,43,false);out.Text(labels.enter,145,874,0xffe9e5dc);out.Text(labels.select,237,874,0xffe9e5dc);
    out.Button(415,865,65,43,false);out.Text(labels.escape,425,874,0xffe9e5dc);out.Text(labels.resume,496,874,0xffe9e5dc);
    if(s.flags&PauseDetails){out.Frame(915,225,770,650);out.Text(labels.episode,970,280,0xffe9e5dc);out.Text(labels.sequence,970,410,0xffe2b66f);out.Line(970,518,1620,518,.8f,0x9ac4c6c3);out.Text(labels.synopsis,970,548,0xffe9e5dc);}
}
}
