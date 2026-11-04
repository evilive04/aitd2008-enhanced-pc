#pragma once
#include <cmath>
#include <cstdint>
namespace menu_presentation_policy {
struct TitleReveal {
    static constexpr std::uint32_t Duration=1600;
    bool titleSeen=false,fading=false;std::uint32_t begin{};
    void Reset(){titleSeen=false;fading=false;begin=0;}
    void Observe(bool title,bool root,std::uint32_t now){
        if(title){titleSeen=true;fading=false;return;}
        if(root&&titleSeen){titleSeen=false;fading=true;begin=now;}
        if(fading&&now-begin>=Duration)fading=false;
    }
    float Alpha(std::uint32_t now)const{
        if(!fading)return 1.f;const auto elapsed=now-begin;if(elapsed>=Duration)return 1.f;
        const float t=elapsed/static_cast<float>(Duration);return t*t*(3.f-2.f*t);
    }
};
struct Point{float x,y;};
struct KeyEdges {
    struct Changes{unsigned pressed,released;};
    unsigned previous{},blocked{};
    void Reset(unsigned held){previous=0;blocked=held;}
    Changes Read(unsigned held){
        blocked&=held;
        const unsigned current=held&~blocked;
        const Changes result{current&~previous,previous&~current};
        previous=current;return result;
    }
};
// The smoke design is front-facing. Retain one shared transform for glyphs and
// hit tests; its four corners now form an undistorted rectangular menu column.
struct Wall {
    float a,b,c,d,e,f,g,h;
    Wall(){
        const Point p0{180,390},p1{640,390},p2{640,1050},p3{180,1050};
        const float dx1=p1.x-p2.x,dx2=p3.x-p2.x,dx3=p0.x-p1.x+p2.x-p3.x;
        const float dy1=p1.y-p2.y,dy2=p3.y-p2.y,dy3=p0.y-p1.y+p2.y-p3.y;
        const float divisor=dx1*dy2-dx2*dy1;
        g=(dx3*dy2-dx2*dy3)/divisor;h=(dx1*dy3-dx3*dy1)/divisor;
        a=p1.x-p0.x+g*p1.x;b=p3.x-p0.x+h*p3.x;c=p0.x;
        d=p1.y-p0.y+g*p1.y;e=p3.y-p0.y+h*p3.y;f=p0.y;
    }
    Point Forward(float x,float y)const{
        const float u=x/460,v=y/660,w=g*u+h*v+1;
        return {(a*u+b*v+c)/w,(d*u+e*v+f)/w};
    }
    bool Inverse(Point p,Point& out)const{
        const float aa=a-p.x*g,bb=b-p.x*h,dd=d-p.y*g,ee=e-p.y*h;
        const float det=aa*ee-bb*dd;if(std::fabs(det)<1e-7f)return false;
        out={(ee*(p.x-c)-bb*(p.y-f))/det*460,(aa*(p.y-f)-dd*(p.x-c))/det*660};
        return std::isfinite(out.x)&&std::isfinite(out.y);
    }
};
struct Canvas{
    float scale,x,y,width,height;
    Canvas(unsigned w,unsigned h,float density=1.f){scale=std::fmin(w/1920.f,h/1080.f)*density;width=1920*scale;height=1080*scale;x=(w-width)*.5f;y=(h-height)*.5f;}
    Point Forward(Point p)const{return {x+p.x*scale,y+p.y*scale};}
    Point Inverse(Point p)const{return {(p.x-x)/scale,(p.y-y)/scale};}
};
inline unsigned Hit(const Wall& wall,Point canonical){
    Point p{};if(!wall.Inverse(canonical,p)||p.x< -24||p.x>400||p.y<0)return ~0u;
    const auto item=static_cast<unsigned>(p.y/100);
    return item<6&&p.y-item*100<=72?item:~0u;
}
}
