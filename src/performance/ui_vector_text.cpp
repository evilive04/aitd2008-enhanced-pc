#include "ui_vector_text.h"
#include <dwrite.h>
#include <dwrite_1.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
using Microsoft::WRL::ComPtr;
namespace ui_vector_text {
namespace {
class InlineObject final:public IDWriteInlineObject{
 ULONG refs_=1;
public:
 InlineSpan span;explicit InlineObject(const InlineSpan& value):span(value){}
 IFACEMETHOD(QueryInterface)(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(IDWriteInlineObject)){*out=static_cast<IDWriteInlineObject*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 IFACEMETHOD_(ULONG,AddRef)()override{return ++refs_;}
 IFACEMETHOD_(ULONG,Release)()override{const auto value=--refs_;if(!value)delete this;return value;}
 IFACEMETHOD(Draw)(void*,IDWriteTextRenderer*,FLOAT,FLOAT,BOOL,BOOL,IUnknown*)override{return E_NOTIMPL;}
 IFACEMETHOD(GetMetrics)(DWRITE_INLINE_OBJECT_METRICS* out)override{if(!out)return E_POINTER;*out={span.width,span.height,span.baseline,FALSE};return S_OK;}
 IFACEMETHOD(GetOverhangMetrics)(DWRITE_OVERHANG_METRICS* out)override{if(!out)return E_POINTER;*out={};return S_OK;}
 IFACEMETHOD(GetBreakConditions)(DWRITE_BREAK_CONDITION* before,DWRITE_BREAK_CONDITION* after)override{if(!before||!after)return E_POINTER;*before=*after=DWRITE_BREAK_CONDITION_NEUTRAL;return S_OK;}
};
class Sink final : public ID2D1TessellationSink {
    ULONG refs_=1;
    Mesh& mesh_; unsigned limit_; HRESULT status_=S_OK;
public:
    Sink(Mesh& mesh,unsigned limit):mesh_(mesh),limit_(limit){}
    IFACEMETHOD(QueryInterface)(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(ID2D1TessellationSink)){*out=static_cast<ID2D1TessellationSink*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    IFACEMETHOD_(ULONG,AddRef)() override {return ++refs_;}
    IFACEMETHOD_(ULONG,Release)() override {ULONG value=--refs_;if(!value)delete this;return value;}
    IFACEMETHOD_(void,AddTriangles)(const D2D1_TRIANGLE* triangles,UINT count) override {
        if(FAILED(status_))return;
        if(count>limit_-mesh_.triangles.size()){status_=E_OUTOFMEMORY;return;}
        try {for(UINT i=0;i<count;++i){const auto& t=triangles[i];
            if(!std::isfinite(t.point1.x)||!std::isfinite(t.point1.y)||!std::isfinite(t.point2.x)||!std::isfinite(t.point2.y)||!std::isfinite(t.point3.x)||!std::isfinite(t.point3.y)){status_=E_FAIL;return;}
            mesh_.triangles.push_back({{t.point1.x,t.point1.y},{t.point2.x,t.point2.y},{t.point3.x,t.point3.y}});
        }}catch(const std::bad_alloc&){status_=E_OUTOFMEMORY;}catch(...){status_=E_FAIL;}
    }
    IFACEMETHOD(Close)() override {return status_;}
};
class Renderer final : public IDWriteTextRenderer {
    ULONG refs_=1; ID2D1Factory* factory_; Mesh& mesh_;const Options& options_;HRESULT status_=S_OK;
    HRESULT Record(HRESULT hr){if(FAILED(hr)&&SUCCEEDED(status_))status_=hr;return hr;}
    HRESULT Rectangle(float x,float y,float width,float height) {
        ComPtr<Sink> sink;sink.Attach(new(std::nothrow)Sink(mesh_,options_.maxTriangles));if(!sink)return E_OUTOFMEMORY;
        const D2D1_TRIANGLE t[]={{{x,y},{x+width,y},{x+width,y+height}},{{x,y},{x+width,y+height},{x,y+height}}};
        sink->AddTriangles(t,2);return sink->Close();
    }
public:
    Renderer(ID2D1Factory* factory,Mesh& mesh,const Options& options):factory_(factory),mesh_(mesh),options_(options){}
    HRESULT Status() const{return status_;}
    IFACEMETHOD(QueryInterface)(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IDWriteTextRenderer)||id==__uuidof(IDWritePixelSnapping)){*out=static_cast<IDWriteTextRenderer*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    IFACEMETHOD_(ULONG,AddRef)() override {return ++refs_;}
    IFACEMETHOD_(ULONG,Release)() override {ULONG value=--refs_;if(!value)delete this;return value;}
    IFACEMETHOD(IsPixelSnappingDisabled)(void*,BOOL* value) override {if(!value)return E_POINTER;*value=TRUE;return S_OK;}
    IFACEMETHOD(GetCurrentTransform)(void*,DWRITE_MATRIX* value) override {if(!value)return E_POINTER;*value={1,0,0,1,0,0};return S_OK;}
    IFACEMETHOD(GetPixelsPerDip)(void*,FLOAT* value) override {if(!value)return E_POINTER;*value=1;return S_OK;}
    IFACEMETHOD(DrawGlyphRun)(void*,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE,const DWRITE_GLYPH_RUN* run,const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown*) override {
        if(FAILED(status_))return status_;return Record(GlyphRun(x,y,run));
    }
    HRESULT GlyphRun(FLOAT x,FLOAT y,const DWRITE_GLYPH_RUN* run){
        if(!run||!run->fontFace)return E_INVALIDARG;
        ComPtr<ID2D1PathGeometry> path;HRESULT hr=factory_->CreatePathGeometry(&path);if(FAILED(hr))return hr;
        ComPtr<ID2D1GeometrySink> outline;hr=path->Open(&outline);if(FAILED(hr))return hr;
        hr=run->fontFace->GetGlyphRunOutline(run->fontEmSize,run->glyphIndices,run->glyphAdvances,run->glyphOffsets,run->glyphCount,run->isSideways,(run->bidiLevel&1)!=0,outline.Get());
        const HRESULT close=outline->Close();if(FAILED(hr))return hr;if(FAILED(close))return close;
        ComPtr<Sink> triangles;triangles.Attach(new(std::nothrow)Sink(mesh_,options_.maxTriangles));if(!triangles)return E_OUTOFMEMORY;
        const D2D1_MATRIX_3X2_F transform={1,0,0,1,x,y};
        hr=path->Tessellate(&transform,options_.tolerance,triangles.Get());const HRESULT finish=triangles->Close();return FAILED(hr)?hr:finish;
    }
    IFACEMETHOD(DrawUnderline)(void*,FLOAT x,FLOAT y,const DWRITE_UNDERLINE* line,IUnknown*) override {return Record(line?Rectangle(x,y+line->offset,line->width,line->thickness):E_INVALIDARG);}
    IFACEMETHOD(DrawStrikethrough)(void*,FLOAT x,FLOAT y,const DWRITE_STRIKETHROUGH* line,IUnknown*) override {return Record(line?Rectangle(x,y+line->offset,line->width,line->thickness):E_INVALIDARG);}
    IFACEMETHOD(DrawInlineObject)(void*,FLOAT x,FLOAT y,IDWriteInlineObject* object,BOOL sideways,BOOL rtl,IUnknown*) override {
        const auto own=dynamic_cast<InlineObject*>(object);if(!own||sideways||rtl||!std::isfinite(x)||!std::isfinite(y))return Record(E_NOTIMPL);
        try{mesh_.inlineObjects.push_back({own->span.id,x,y,own->span.width,own->span.height});return S_OK;}catch(...){return Record(E_OUTOFMEMORY);}
    }
};
bool Finite(float value,float low,float high){return std::isfinite(value)&&value>=low&&value<=high;}
bool ValidUtf16(const wchar_t* text,unsigned length){
    for(unsigned i=0;i<length;++i){const unsigned c=text[i];if(c>=0xd800&&c<=0xdbff){if(++i==length||text[i]<0xdc00||text[i]>0xdfff)return false;}else if(c>=0xdc00&&c<=0xdfff)return false;}
    return true;
}
}
HRESULT Build(const wchar_t* text,std::uint32_t length,const Options& o,Mesh& output) noexcept {
    output={};
    if((!text&&length)||length>65536||!o.family||!o.family[0]||!Finite(o.size,1,4096)||!Finite(o.width,1,1000000)||!Finite(o.height,1,1000000)||!Finite(o.tolerance,.001f,8)||!o.maxTriangles||o.maxTriangles>1048576)return E_INVALIDARG;
    if(!length)return S_OK;if(!ValidUtf16(text,length))return E_INVALIDARG;
    if(o.inlineCount>128||(o.inlineCount&&!o.inlineSpans))return E_INVALIDARG;
    unsigned prior=0;for(unsigned n=0;n<o.inlineCount;++n){const auto& span=o.inlineSpans[n];if(!span.length||span.start<prior||span.start>=length||span.length>length-span.start||!Finite(span.width,1,4096)||!Finite(span.height,1,4096)||!Finite(span.baseline,0,span.height))return E_INVALIDARG;prior=span.start+span.length;}
    try {
        ComPtr<IDWriteFactory> write;HRESULT hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.GetAddressOf()));if(FAILED(hr))return hr;
        ComPtr<IDWriteFontCollection> fonts;hr=write->GetSystemFontCollection(&fonts);if(FAILED(hr))return hr;
        UINT32 index=0;BOOL exists=FALSE;hr=fonts->FindFamilyName(o.family,&index,&exists);if(FAILED(hr))return hr;if(!exists)return DWRITE_E_NOFONT;
        ComPtr<IDWriteTextFormat> format;hr=write->CreateTextFormat(o.family,fonts.Get(),o.bold?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,o.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,o.size,L"en-us",&format);if(FAILED(hr))return hr;
        ComPtr<IDWriteTextLayout> layout;hr=write->CreateTextLayout(text,length,format.Get(),o.width,o.height,&layout);if(FAILED(hr))return hr;
        ComPtr<IDWriteTextLayout1> layout1;hr=layout.As(&layout1);if(FAILED(hr))return hr;
        hr=layout1->SetPairKerning(TRUE,DWRITE_TEXT_RANGE{0,length});if(FAILED(hr))return hr;
        for(unsigned n=0;n<o.inlineCount;++n){ComPtr<InlineObject> object;object.Attach(new(std::nothrow)InlineObject(o.inlineSpans[n]));if(!object)return E_OUTOFMEMORY;hr=layout->SetInlineObject(object.Get(),{o.inlineSpans[n].start,o.inlineSpans[n].length});if(FAILED(hr))return hr;}
        ComPtr<ID2D1Factory> geometry;hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,geometry.GetAddressOf());if(FAILED(hr))return hr;
        Mesh pending;ComPtr<Renderer> renderer;renderer.Attach(new(std::nothrow)Renderer(geometry.Get(),pending,o));if(!renderer)return E_OUTOFMEMORY;
        hr=layout->Draw(nullptr,renderer.Get(),0,0);if(FAILED(hr))return hr;if(FAILED(renderer->Status()))return renderer->Status();
        DWRITE_TEXT_METRICS metrics{};hr=layout->GetMetrics(&metrics);if(FAILED(hr))return hr;
        pending.advanceWidth=metrics.widthIncludingTrailingWhitespace;pending.layoutHeight=metrics.height;pending.lineCount=metrics.lineCount;
        if(!pending.triangles.empty()){
            Bounds b{(std::numeric_limits<float>::max)(),(std::numeric_limits<float>::max)(),std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()};
            for(const auto& t:pending.triangles)for(const Point p:{t.a,t.b,t.c}){b.left=(std::min)(b.left,p.x);b.top=(std::min)(b.top,p.y);b.right=(std::max)(b.right,p.x);b.bottom=(std::max)(b.bottom,p.y);}pending.ink=b;
        }
        output=std::move(pending);return S_OK;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
}
