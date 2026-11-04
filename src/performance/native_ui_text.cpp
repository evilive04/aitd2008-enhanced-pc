#include "native_ui_text.h"
#include <dwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <unordered_map>
using Microsoft::WRL::ComPtr;
namespace native_ui_text {
namespace {
constexpr unsigned MaxGlyphs=4096, MaxVertices=786432;
template<class T> bool Read(Bytes b,std::size_t at,T& out) noexcept {
    if(!b.data||at>b.size||sizeof(T)>b.size-at)return false;
    std::memcpy(&out,b.data+at,sizeof(T));return true;
}
bool Finite(const Matrix& m) noexcept {return std::isfinite(m.xx)&&std::isfinite(m.xy)&&std::isfinite(m.yx)&&std::isfinite(m.yy)&&std::isfinite(m.tx)&&std::isfinite(m.ty);}
bool Codepoint(std::uint32_t cp) noexcept {return cp<=0x10ffff&&!(cp>=0xd800&&cp<=0xdfff);}
bool FontGlyph(Bytes b,unsigned index,std::uint32_t& cp) noexcept {
    std::uint32_t kind{},size{},relative{};std::uint16_t count{};
    if(!Read(b,0,kind)||kind!=10||!Read(b,4,size)||size!=b.size||!Read(b,10,count)||index>=count||b.size!=1112u+68u*count)return false;
    if(!Read(b,1112u+8u*index,cp)||!Codepoint(cp)||!Read(b,1116u+8u*index,relative))return false;
    return relative==1108u+8u*count+60u*index;
}
bool In(ui_vector_text::Point p,int edge,const Rect& r) noexcept {
    switch(edge){case 0:return p.x>=r.left;case 1:return p.x<=r.right;case 2:return p.y>=r.top;default:return p.y<=r.bottom;}
}
ui_vector_text::Point Intersect(ui_vector_text::Point a,ui_vector_text::Point b,int edge,const Rect& r) noexcept {
    const bool x=edge<2;const float boundary=edge==0?r.left:edge==1?r.right:edge==2?r.top:r.bottom;
    const float av=x?a.x:a.y,bv=x?b.x:b.y;const float t=(boundary-av)/(bv-av);
    return x?ui_vector_text::Point{boundary,a.y+t*(b.y-a.y)}:ui_vector_text::Point{a.x+t*(b.x-a.x),boundary};
}
HRESULT Append(const ui_vector_text::Triangle& t,const Matrix& glyph,const RenderContext& c,std::uint32_t color,Geometry& mesh) {
    ui_vector_text::Point a[12]{c.parent.Apply(glyph.Apply(t.a)),c.parent.Apply(glyph.Apply(t.b)),c.parent.Apply(glyph.Apply(t.c))},b[12]{};unsigned n=3;
    for(unsigned i=0;i<n;++i)if(!std::isfinite(a[i].x)||!std::isfinite(a[i].y))return E_INVALIDARG;
    for(int edge=0;edge<4&&n;++edge){unsigned m=0;auto previous=a[n-1];bool pin=In(previous,edge,c.clip);
        for(unsigned i=0;i<n;++i){const auto current=a[i];const bool cin=In(current,edge,c.clip);if(cin!=pin)b[m++]=Intersect(previous,current,edge,c.clip);if(cin)b[m++]=current;previous=current;pin=cin;}
        n=m;std::copy(b,b+n,a);
    }
    for(unsigned i=1;i+1<n;++i){if(mesh.triangles.size()>MaxVertices-3)return E_OUTOFMEMORY;for(auto p:{a[0],a[i],a[i+1]})mesh.triangles.push_back({p.x,p.y,color});}
    return S_OK;
}
}
ui_vector_text::Point Matrix::Apply(ui_vector_text::Point p) const noexcept{return {xx*p.x+xy*p.y+tx,yx*p.x+yy*p.y+ty};}
HRESULT Decode(Bytes b,FontResolver resolve,void* user,Capture& output) noexcept {
    output.glyphs.clear();if(!resolve||b.size<56||b.size>1024u*1024u)return E_INVALIDARG;
    try{
        std::uint32_t kind{},size{};Matrix m;
        if(!Read(b,0,kind)||kind!=11||!Read(b,4,size)||size!=b.size||!Read(b,28,m.xx)||!Read(b,32,m.yy)||!Read(b,36,m.yx)||!Read(b,40,m.xy)||!Read(b,44,m.tx)||!Read(b,48,m.ty)||!Finite(m))return E_INVALIDARG;
        Capture result;int x=0,y=0;float sx=0,sy=0;std::uint32_t color=0xffffffff,remaining=0;std::uint16_t font=0;Bytes fontBytes;std::size_t at=52;
        while(at<b.size){const unsigned op=b.data[at++];
            if(!op){if(remaining)return E_INVALIDARG;for(;at<b.size;++at)if(b.data[at])return E_INVALIDARG;output=std::move(result);return S_OK;}
            unsigned extent=0;switch(op){case 1:case 2:case 255:extent=3;break;case 4:case 8:extent=7;break;case 16:extent=5;break;default:return E_NOTIMPL;}
            if(extent>b.size-at)return E_INVALIDARG;
            if(op==1||op==2){std::int16_t value{};Read(b,at+1,value);if(op==1)x=value;else y=value;}
            else if(op==4){Read(b,at+3,color);}
            else if(op==8){if(remaining)return E_INVALIDARG;Read(b,at+1,font);Read(b,at+3,remaining);if(!remaining||remaining>MaxGlyphs||!resolve(user,font,fontBytes))return E_INVALIDARG;}
            else if(op==16){std::uint16_t vx{},vy{};Read(b,at+1,vx);Read(b,at+3,vy);sx=vy/1024.f;sy=vx/1024.f;}
            else {std::uint32_t cp{};std::int16_t advance{};if(!remaining||!FontGlyph(fontBytes,b.data[at],cp)||result.glyphs.size()>=MaxGlyphs)return E_INVALIDARG;Read(b,at+1,advance);
                Glyph glyph;glyph.codepoint=cp;glyph.font=font;glyph.nativeIndex=b.data[at];glyph.nativeColor=color;glyph.advance=advance;
                glyph.transform={m.xx*sx,m.yx*sx,m.xy*sy,m.yy*sy,m.xx*x+m.xy*y+m.tx,m.yx*x+m.yy*y+m.ty};
                if(!Finite(glyph.transform))return E_INVALIDARG;result.glyphs.push_back(glyph);x+=advance;--remaining;
            }at+=extent;
        }return E_INVALIDARG;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
HRESULT Reflow(const std::vector<LayoutRecord>& records,const std::vector<LayoutLine>& lines,float left,float right,float sx,float sy,unsigned alignment,std::vector<Matrix>& output) noexcept {
    output.clear();
    if(records.size()>2048||lines.empty()||lines.size()>512||!std::isfinite(left)||!std::isfinite(right)||left>=right||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||alignment>3)return E_INVALIDARG;
    try{
        struct Metrics {ComPtr<IDWriteFontFace> face;std::unordered_map<unsigned,float> widths;};static thread_local Metrics metrics;
        HRESULT hr=S_OK;
        if(!metrics.face){ComPtr<IDWriteFactory> factory;hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf()));if(FAILED(hr))return hr;
            ComPtr<IDWriteFontCollection> collection;hr=factory->GetSystemFontCollection(&collection);if(FAILED(hr))return hr;UINT32 index{};BOOL exists{};hr=collection->FindFamilyName(ui_typeface::Body,&index,&exists);if(FAILED(hr)||!exists)return E_NOTIMPL;
            ComPtr<IDWriteFontFamily> family;hr=collection->GetFontFamily(index,&family);if(FAILED(hr))return hr;ComPtr<IDWriteFont> font;hr=family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STYLE_NORMAL,&font);if(FAILED(hr))return hr;hr=font->CreateFontFace(&metrics.face);if(FAILED(hr))return hr;
        }
        DWRITE_FONT_METRICS fm{};metrics.face->GetMetrics(&fm);std::vector<float> widths(records.size());
        for(unsigned i=0;i<records.size();++i){const auto cp=records[i].codepoint;if(cp<=0||cp==9||cp==10||cp==13)continue;if(!Codepoint(cp))return E_INVALIDARG;
            auto found=metrics.widths.find(cp);if(found==metrics.widths.end()){UINT32 code=cp;UINT16 glyph{};hr=metrics.face->GetGlyphIndices(&code,1,&glyph);if(FAILED(hr)||!glyph)return E_NOTIMPL;DWRITE_GLYPH_METRICS gm{};hr=metrics.face->GetDesignGlyphMetrics(&glyph,1,&gm,FALSE);if(FAILED(hr))return hr;found=metrics.widths.emplace(cp,1024.f*gm.advanceWidth/fm.designUnitsPerEm).first;}widths[i]=found->second*sx;
        }
        std::vector<Matrix> all(records.size());unsigned covered=0;float commonScale=1.f;
        const auto nondrawing=[](int cp){return cp<=0||cp==9||cp==10||cp==13||cp==32;};
        for(const auto line:lines){if(line.first<covered||line.last<line.first||line.last>=records.size())return E_NOTIMPL;for(unsigned i=covered;i<line.first;++i)if(!nondrawing(records[i].codepoint))return E_NOTIMPL;covered=line.last+1;float natural=0;for(unsigned i=line.first;i<=line.last;++i)natural+=widths[i];if(natural>0)commonScale=(std::min)(commonScale,(right-left)/natural);}
        for(unsigned i=covered;i<records.size();++i)if(!nondrawing(records[i].codepoint))return E_NOTIMPL;covered=0;
        for(unsigned lineIndex=0;lineIndex<lines.size();++lineIndex){const auto line=lines[lineIndex];covered=line.last+1;
            float natural=0;unsigned spaces=0;for(unsigned i=line.first;i<=line.last;++i){natural+=widths[i];spaces+=records[i].codepoint==32;}if(!(natural>0))continue;
            const float available=right-left;const float scale=commonScale;const float width=natural*scale;float pen=static_cast<float>(records[line.first].x);
            if(alignment==1)pen=right-width;else if(alignment==2)pen=left+(available-width)*.5f;
            // Native justified text expands spaces only on non-final lines.
            const float extra=alignment==3&&lineIndex+1<lines.size()&&spaces?(std::max)(0.f,available-width)/spaces:0.f;
            for(unsigned i=line.first;i<=line.last;++i){all[i]={sx*scale,0,0,sy*scale,pen,static_cast<float>(records[i].y)};pen+=widths[i]*scale;if(records[i].codepoint==32)pen+=extra;}
        }
        std::vector<Matrix> result;for(unsigned i=0;i<records.size();++i){const int cp=records[i].codepoint;if(cp>0&&cp!=9&&cp!=10&&cp!=13&&cp!=32)result.push_back(all[i]);}output=std::move(result);return S_OK;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
HRESULT Build(const Capture& text,const RenderContext& c,ColorResolver colors,void* user,Geometry& output,const wchar_t* family) noexcept {
    output.triangles.clear();
    if(!c.rectangularClipProven||!c.colorResolved)return E_NOTIMPL;
    if(!colors||!family||!Finite(c.parent)||!std::isfinite(c.clip.left)||!std::isfinite(c.clip.top)||!std::isfinite(c.clip.right)||!std::isfinite(c.clip.bottom)||c.clip.left>c.clip.right||c.clip.top>c.clip.bottom||text.glyphs.size()>MaxGlyphs)return E_INVALIDARG;
    try{
        ComPtr<IDWriteFactory> factory;HRESULT hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.GetAddressOf()));if(FAILED(hr))return hr;
        ComPtr<IDWriteFontCollection> collection;hr=factory->GetSystemFontCollection(&collection);if(FAILED(hr))return hr;UINT32 index=0;BOOL exists=FALSE;hr=collection->FindFamilyName(family,&index,&exists);if(FAILED(hr))return hr;if(!exists)return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        ComPtr<IDWriteFontFamily> fontFamily;hr=collection->GetFontFamily(index,&fontFamily);if(FAILED(hr))return hr;ComPtr<IDWriteFont> font;hr=fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STYLE_NORMAL,&font);if(FAILED(hr))return hr;
        DWRITE_FONT_METRICS fm{};font->GetMetrics(&fm);const float baseline=1024.f*fm.ascent/fm.designUnitsPerEm;
        Geometry result;std::unordered_map<std::uint32_t,ui_vector_text::Mesh> cache;
        for(const auto& glyph:text.glyphs){if(!Codepoint(glyph.codepoint)||!Finite(glyph.transform))return E_INVALIDARG;std::uint32_t argb{};if(!colors(user,glyph.nativeColor,argb))return E_NOTIMPL;
            wchar_t chars[2]{};unsigned length=1;if(glyph.codepoint<=0xffff)chars[0]=static_cast<wchar_t>(glyph.codepoint);else{const auto cp=glyph.codepoint-0x10000;chars[0]=static_cast<wchar_t>(0xd800+(cp>>10));chars[1]=static_cast<wchar_t>(0xdc00+(cp&1023));length=2;}
            auto cached=cache.find(glyph.codepoint);if(cached==cache.end()){
                // Reject missing glyphs instead of silently mixing another font.
                BOOL supported=FALSE;hr=font->HasCharacter(glyph.codepoint,&supported);if(FAILED(hr))return hr;if(!supported)return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
                ui_vector_text::Options o;o.family=family;o.size=1024;o.width=8192;o.height=8192;o.tolerance=1.5f;ui_vector_text::Mesh mesh;hr=ui_vector_text::Build(chars,length,o,mesh);if(FAILED(hr))return hr;
                cached=cache.emplace(glyph.codepoint,std::move(mesh)).first;
            }
            for(auto tri:cached->second.triangles){tri.a.y-=baseline;tri.b.y-=baseline;tri.c.y-=baseline;hr=Append(tri,glyph.transform,c,argb,result);if(FAILED(hr))return hr;}
        }output=std::move(result);return S_OK;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
}
