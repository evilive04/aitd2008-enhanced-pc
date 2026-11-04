#pragma once
#include <cmath>
namespace native_camera_ray {
// Native595760 multiplies row-vector world points by camera+0. Native8A4260
// uses perspective2 (LH) or3 (RH); their clip-W signs are respectively +Z/-Z.
inline bool Center(const float* view,unsigned perspective,float* origin,float* forward){
 if(!view||!origin||!forward||(perspective!=2&&perspective!=3))return false;
 for(unsigned i=0;i<16;++i)if(!std::isfinite(view[i]))return false;
 if(std::fabs(view[3])>.001f||std::fabs(view[7])>.001f||std::fabs(view[11])>.001f||std::fabs(view[15]-1.f)>.001f)return false;
 // Transpose is an inverse only for an orthonormal camera basis. Reject scale,
 // shear and incomplete frames rather than manufacturing a plausible ray.
 for(unsigned a=0;a<3;++a)for(unsigned b=a;b<3;++b){
  float dot=0;for(unsigned n=0;n<3;++n)dot+=view[a*4+n]*view[b*4+n];
  if(std::fabs(dot-(a==b?1.f:0.f))>.002f)return false;
 }
 float eye[4]{0,0,0,1},direction[4]{};
 const float sign=perspective==2?1.f:-1.f;
 for(unsigned n=0;n<3;++n){
  eye[n]=-(view[12]*view[n*4]+view[13]*view[n*4+1]+view[14]*view[n*4+2]);
  direction[n]=sign*view[n*4+2];
  if(!std::isfinite(eye[n])||std::fabs(eye[n])>1000000.f)return false;
 }
 for(unsigned n=0;n<4;++n){origin[n]=eye[n];forward[n]=direction[n];}
 return true;
}
}
