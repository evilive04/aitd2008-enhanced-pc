#pragma once
#include <cmath>
#include <algorithm>
namespace sss_edward_mask {
// Authored polygon/holes in the inspected Edward256-square UV atlas. No image pixels.
struct Point{float x,y;};
inline float Strength(float u,float v)noexcept{
    if(!std::isfinite(u)||!std::isfinite(v))return 0;const float x=u*256,y=v*256;
    static constexpr Point outline[]={{82,65},{101,53},{122,49},{146,50},{165,60},{176,78},{184,107},{187,130},{185,155},{175,179},{165,193},{153,206},{135,216},{114,215},{94,207},{78,194},{67,176},{61,155},{59,132},{63,107},{70,83}};
    bool inside=false;float distance=1000;
    for(unsigned i=0,j=static_cast<unsigned>(sizeof(outline)/sizeof(outline[0]))-1;i<sizeof(outline)/sizeof(outline[0]);j=i++){
        const auto a=outline[j],b=outline[i];const float dx=b.x-a.x,dy=b.y-a.y,t=(std::max)(0.f,(std::min)(1.f,((x-a.x)*dx+(y-a.y)*dy)/(dx*dx+dy*dy)));
        const float px=x-a.x-t*dx,py=y-a.y-t*dy;distance=(std::min)(distance,std::sqrt(px*px+py*py));
        if((a.y>y)!=(b.y>y))if(x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x)inside=!inside;
    }
    if(!inside)return 0;const auto ramp=[](float d,float width){const float t=(std::max)(0.f,(std::min)(1.f,d/width));return t*t*(3-2*t);};float strength=ramp(distance,3.f);
    // Brow/eyelash hair, nostrils, mouth interior and dense chin stubble black;
    // the surrounding closed eyelids remain skin. Feather only inward to skin.
    static constexpr float holes[][4]={{96,94,22,6},{155,93,20,7},{96,109,17,2},{155,109,16,2},{117,150,5,4},{135,150,5,4},{127,180,27,8},{128,198,48,31}};
    for(const auto& h:holes){const float dx=(x-h[0])/h[2],dy=(y-h[1])/h[3],d=(std::sqrt(dx*dx+dy*dy)-1)*(std::min)(h[2],h[3]);strength*=ramp(d,2.5f);}return strength;
}
}
