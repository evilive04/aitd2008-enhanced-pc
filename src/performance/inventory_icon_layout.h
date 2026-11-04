#pragma once
#include "inventory_icon_bounds.h"
#include <cmath>
// Each icon's visible longest edge has the same canvas size. Uniform scale
// preserves the original art; transparent atlas padding does not count.
namespace inventory_icon_layout {
struct Box {float x,y,w,h;};
inline constexpr Box Equipment[2]={{840,114,88,88},{992,114,88,88}};
inline constexpr Box Hover={137,143,106,106};
inline constexpr float RimLeft=814,RimRight=1106;
inline constexpr float HoverPanelHeight=186,DetailsTop=286;
template<class Image>inline Box ImageBox(const Image& image,Box target){
 float l=0,t=0,r=image.w,b=image.h;
 for(const auto& q:inventory_icon_bounds::images)if(image.bitmap==q.bitmap&&
   std::fabs(image.x-q.x)<.01f&&std::fabs(image.y-q.y)<.01f&&
   std::fabs(image.w-q.w)<.01f&&std::fabs(image.h-q.h)<.01f){l=q.left;t=q.top;r=q.right;b=q.bottom;break;}
 if(r<=l||b<=t)return target;
 const float scale=std::fmin(target.w/(r-l),target.h/(b-t));
 return {target.x+target.w*.5f-(l+r)*.5f*scale,
         target.y+target.h*.5f-(t+b)*.5f*scale,image.w*scale,image.h*scale};
}
}
