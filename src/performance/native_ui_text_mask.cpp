#include "native_ui_text_mask.h"
#include <cmath>
#include <new>
namespace native_ui_text_mask {
namespace {
using P=ui_vector_text::Point;using T=ui_vector_text::Triangle;
bool Finite(P p) noexcept{return std::isfinite(p.x)&&std::isfinite(p.y);}
double Side(P a,P b,P p) noexcept{return (static_cast<double>(b.x)-a.x)*(static_cast<double>(p.y)-a.y)-(static_cast<double>(b.y)-a.y)*(static_cast<double>(p.x)-a.x);}
HRESULT Pair(const T& source,const T& mask,unsigned limit,std::vector<T>& output){
    const double orientation=Side(mask.a,mask.b,mask.c);if(orientation==0)return S_OK;const double sign=orientation>0?1:-1;
    P polygon[12]{source.a,source.b,source.c},next[12]{};unsigned count=3;const P edges[]={mask.a,mask.b,mask.c,mask.a};
    for(unsigned e=0;e<3&&count;++e){unsigned n=0;P previous=polygon[count-1];double ps=Side(edges[e],edges[e+1],previous)*sign;bool pin=ps>=0;
        for(unsigned i=0;i<count;++i){P current=polygon[i];const double cs=Side(edges[e],edges[e+1],current)*sign;const bool cin=cs>=0;
            if(cin!=pin){const double denominator=ps-cs;if(denominator==0||!std::isfinite(denominator))return E_INVALIDARG;const double t=ps/denominator;P p{static_cast<float>(previous.x+t*(current.x-previous.x)),static_cast<float>(previous.y+t*(current.y-previous.y))};if(!Finite(p)||n>=12)return E_INVALIDARG;next[n++]=p;}
            if(cin){if(n>=12)return E_INVALIDARG;next[n++]=current;}previous=current;ps=cs;pin=cin;
        }count=n;for(unsigned i=0;i<count;++i)polygon[i]=next[i];
    }
    for(unsigned i=1;i+1<count;++i){if(Side(polygon[0],polygon[i],polygon[i+1])==0)continue;if(output.size()>=limit)return E_OUTOFMEMORY;output.push_back({polygon[0],polygon[i],polygon[i+1]});}return S_OK;
}
}
HRESULT Decode(const float* words,unsigned count,bool disabled,Snapshot& output) noexcept{
    output={};if(disabled||!count)return S_OK;if(!words||count>65536)return E_INVALIDARG;
    const float marker=words[count-1];if(!std::isfinite(marker)||marker<0||marker>count-1||std::floor(marker)!=marker)return E_INVALIDARG;
    const unsigned start=static_cast<unsigned>(marker);if((count-start-1)%6)return E_NOTIMPL;
    try{Snapshot pending;for(unsigned at=start;at+5<count-1;at+=6){T t{{words[at],words[at+1]},{words[at+2],words[at+3]},{words[at+4],words[at+5]}};if(!Finite(t.a)||!Finite(t.b)||!Finite(t.c))return E_INVALIDARG;pending.triangles.push_back(t);}pending.active=!pending.triangles.empty();output=std::move(pending);return S_OK;}catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
HRESULT Clip(const std::vector<T>& glyph,const native_ui_text::Matrix& transform,const Snapshot& mask,unsigned limit,std::vector<T>& output) noexcept{
    output.clear();if(!limit||limit>262144||glyph.size()>262144||mask.triangles.size()>10922)return E_INVALIDARG;
    try{std::vector<T> pending;
        for(const auto& source:glyph){T triangle{transform.Apply(source.a),transform.Apply(source.b),transform.Apply(source.c)};if(!Finite(triangle.a)||!Finite(triangle.b)||!Finite(triangle.c))return E_INVALIDARG;
            if(!mask.active){if(pending.size()>=limit)return E_OUTOFMEMORY;pending.push_back(triangle);continue;}
            for(const auto& maskTriangle:mask.triangles){if(!Finite(maskTriangle.a)||!Finite(maskTriangle.b)||!Finite(maskTriangle.c))return E_INVALIDARG;const HRESULT hr=Pair(triangle,maskTriangle,limit,pending);if(FAILED(hr))return hr;}
        }output=std::move(pending);return S_OK;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
}
