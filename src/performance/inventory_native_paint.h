#pragma once
#include "inventory_native_content.h"
#include "inventory_icon_bounds.h"
#include <cmath>
// The native Flash renderer remains responsible for placement, masks, color,
// liquid fills and variant animation. Only exact original shape identities
// pass the replacement root's content exception.
namespace inventory_native_paint {
struct Shapes {DWORD root{},dictionary{},count{},values[32]{};};
struct Cache {DWORD root{},dictionary{},values[32]{},next[32]{};};
template<size_t N>bool Capture(DWORD root,const inventory_native_content::Descriptor(&spec)[N],Shapes& out,Cache& cache){
 static_assert(N<=32,"Bounded inventory native shape group");out={};
 const DWORD dictionary=*reinterpret_cast<const DWORD*>(root+0x74);if(!dictionary)return false;
 if(cache.root!=root||cache.dictionary!=dictionary)cache={root,dictionary,{},{}};
 Shapes next{root,dictionary,static_cast<DWORD>(N),{}};
 for(unsigned n=0;n<N;++n){const auto& d=spec[n];const DWORD shape=*reinterpret_cast<const DWORD*>(dictionary+d.id*4);
  if(!shape||*reinterpret_cast<const DWORD*>(shape+8)!=d.id)return false;
  const DWORD end=*reinterpret_cast<const DWORD*>(shape+4);
  if(end<shape||end-shape!=d.bytes)return false;
  if(cache.values[n]!=shape||cache.next[n]!=end){
   DWORD fnv=2166136261u;for(unsigned at=8;at<d.bytes;++at)fnv=(fnv^*reinterpret_cast<const BYTE*>(shape+at))*16777619u;
   if(fnv!=d.tailFnv)return false;cache.values[n]=shape;cache.next[n]=end;
  }
  next.values[n]=shape;
 }
 out=next;return true;
}
inline bool Contains(const Shapes& shapes,std::uintptr_t root,std::uintptr_t shape){
 if(!root||root!=shapes.root||!shape||shapes.count>32||
    *reinterpret_cast<const DWORD*>(root+0x74)!=shapes.dictionary)return false;
 for(unsigned n=0;n<shapes.count;++n)if(shape==shapes.values[n])return true;
 return false;
}
// 925410 passes its current sprite as the third paint argument. Its ascent
// follows the real parent at +14; never infer an instance from a shared shape.
inline bool Descendant(std::uintptr_t instance,std::uintptr_t ancestor){
 if(!ancestor)return false;
 for(unsigned depth=0;instance&&depth<32;++depth){
  if(*reinterpret_cast<const DWORD*>(instance)!=0xcd0660)return false;
  if(instance==ancestor)return true;
  const DWORD parent=*reinterpret_cast<const DWORD*>(instance+0x14);
  if(parent==instance)return false;instance=parent;
 }
 return false;
}
// 486/499 contain invisible scripting controls (454/488 -> transparent453).
// Native parent bounds include these offscreen controls. Use the evaluated
// bounds of the actual visual children instead; item child bounds retain the
// original movie's atlas masks. Direct shapes use their evaluated placement.
inline bool VisualBounds(const Shapes& shapes,std::uintptr_t root,std::uintptr_t anchor,float* bounds){
 bool found=false;DWORD record=*reinterpret_cast<const DWORD*>(anchor+0x1c);unsigned count=0;
 for(;record&&count<128;++count){
  const DWORD definition=*reinterpret_cast<const DWORD*>(record+4),actor=*reinterpret_cast<const DWORD*>(record+0x3c);
  if(!definition)return false;const DWORD id=*reinterpret_cast<const DWORD*>(definition+8);float b[4]{};bool include=false;
  if(id!=454&&id!=488&&id!=453&&id!=485){
   if(actor){
    if(*reinterpret_cast<const DWORD*>(actor)!=0xcd0660||*reinterpret_cast<const DWORD*>(actor+0x14)!=anchor||*reinterpret_cast<const DWORD*>(actor+0x18)!=definition)return false;
    if((*reinterpret_cast<const DWORD*>(actor+0x6c)&1)&&*reinterpret_cast<const DWORD*>(actor+0x1c)){
     const auto* value=reinterpret_cast<const float*>(actor+0xd0);for(unsigned n=0;n<4;++n)b[n]=value[n];include=true;
    }
   }else if(Contains(shapes,root,definition)){
    const auto* source=reinterpret_cast<const float*>(definition+12);const auto* m=reinterpret_cast<const float*>(record+0x78);
    const bool transformed=(*reinterpret_cast<const DWORD*>(record+0x64)&2)!=0;
    for(unsigned corner=0;corner<4;++corner){const float x=source[corner&1],y=source[2+(corner>>1)];const float px=transformed?m[0]*x+m[3]*y+m[4]:x,py=transformed?m[2]*x+m[1]*y+m[5]:y;
     if(!std::isfinite(px)||!std::isfinite(py))return false;
     if(!corner){b[0]=b[1]=px;b[2]=b[3]=py;}else{b[0]=std::fmin(b[0],px);b[1]=std::fmax(b[1],px);b[2]=std::fmin(b[2],py);b[3]=std::fmax(b[3],py);}
    }include=true;
   }
  }
  if(include){for(float value:b)if(!std::isfinite(value))return false;
   if(b[1]>b[0]&&b[3]>b[2]){if(!found){for(unsigned n=0;n<4;++n)bounds[n]=b[n];found=true;}else{bounds[0]=std::fmin(bounds[0],b[0]);bounds[1]=std::fmax(bounds[1],b[1]);bounds[2]=std::fmin(bounds[2],b[2]);bounds[3]=std::fmax(bounds[3],b[3]);}}
  }
  const DWORD next=*reinterpret_cast<const DWORD*>(record+0x94);if(next==record)return false;record=next;
 }
 return found&&!record;
}
// Normalize to the current primary item art, not the slot envelope. Fire,
// liquid, tape and script children still receive exactly the SAME transform,
// but never make the underlying item shrink. The bounds are metadata from
// original bitmap alpha, expressed in each original sprite's coordinates.
inline bool PrimaryBounds(const Shapes& shapes,std::uintptr_t root,std::uintptr_t anchor,float* bounds){
 if(root!=shapes.root)return false;
 bool found=false;DWORD record=*reinterpret_cast<const DWORD*>(anchor+0x1c);unsigned count=0;
 for(;record&&count<128;++count){
  const DWORD definition=*reinterpret_cast<const DWORD*>(record+4),actor=*reinterpret_cast<const DWORD*>(record+0x3c);
  if(!definition)return false;
  const DWORD id=*reinterpret_cast<const DWORD*>(definition+8);
  if(actor){
   if(*reinterpret_cast<const DWORD*>(actor)!=0xcd0660||*reinterpret_cast<const DWORD*>(actor+0x14)!=anchor||*reinterpret_cast<const DWORD*>(actor+0x18)!=definition)return false;
   if((*reinterpret_cast<const DWORD*>(actor+0x6c)&1)&&*reinterpret_cast<const DWORD*>(actor+0x1c)){
    for(const auto& q:inventory_icon_bounds::sprites)if(q.id==id){
     const auto* m=reinterpret_cast<const float*>(record+0x78);
     const bool transformed=(*reinterpret_cast<const DWORD*>(record+0x64)&2)!=0;
     for(unsigned vertex=0;vertex<q.count;++vertex){
      const auto& point=inventory_icon_bounds::points[q.first+vertex];const float x=point.x,y=point.y;
      const float px=transformed?m[0]*x+m[3]*y+m[4]:x,py=transformed?m[2]*x+m[1]*y+m[5]:y;
      if(!std::isfinite(px)||!std::isfinite(py))return false;
      if(!found){bounds[0]=bounds[1]=px;bounds[2]=bounds[3]=py;found=true;}
      else{bounds[0]=std::fmin(bounds[0],px);bounds[1]=std::fmax(bounds[1],px);bounds[2]=std::fmin(bounds[2],py);bounds[3]=std::fmax(bounds[3],py);}
     }break;
    }
   }
  }
  const DWORD next=*reinterpret_cast<const DWORD*>(record+0x94);if(next==record)return false;record=next;
 }
 return found&&!record&&bounds[1]>bounds[0]&&bounds[3]>bounds[2];
}
// Fit all authentic visual layers with one physical-pixel scale; no geometry
// or UV is deformed and the same map also applies to the native masks.
inline bool Fit(const Shapes& shapes,std::uintptr_t root,std::uintptr_t anchor,float* matrix,DWORD width,DWORD height,float x,float y,float w,float h){
 if(!matrix||width<320||height<200||!anchor)return false;
 const auto* viewport=reinterpret_cast<const float*>(root+0x54);
 float v[4]{viewport[0],viewport[1],viewport[2],viewport[3]},b[4]{};if(!PrimaryBounds(shapes,root,anchor,b)&&!VisualBounds(shapes,root,anchor,b))return false;
 for(float value:v)if(!std::isfinite(value))return false;
 for(float value:b)if(!std::isfinite(value))return false;
 for(unsigned n=0;n<6;++n)if(!std::isfinite(matrix[n]))return false;
 if(v[0]<=0||v[1]<=0||b[1]<=b[0]||b[3]<=b[2])return false;
 // Match inventory_ui_overlay's Canvas(...,1.f), including its letterboxing.
 const float density=std::fmin(width/1920.f,height/1080.f);
 const float sourceW=(b[1]-b[0])*v[0]*width,sourceH=(b[3]-b[2])*v[1]*height;
 if(sourceW<.01f||sourceH<.01f)return false;
 const float scale=std::fmin(w*density/sourceW,h*density/sourceH);
 if(!std::isfinite(scale)||scale<=0||scale>100)return false;
 const float targetX=((width-1920*density)*.5f+(x+w*.5f)*density)/width;
 const float targetY=((height-1080*density)*.5f+(y+h*.5f)*density)/height;
 for(unsigned n=0;n<4;++n)matrix[n]*=scale;
 matrix[4]=matrix[4]*scale+(targetX-v[2])/v[0]-(b[0]+b[1])*.5f*scale;
 matrix[5]=matrix[5]*scale+(targetY-v[3])/v[1]-(b[2]+b[3])*.5f*scale;
 return std::isfinite(matrix[4])&&std::isfinite(matrix[5]);
}
}
