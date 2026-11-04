#pragma once
#include <cmath>
namespace melee_locomotion {
// The native HHMV gait derives speed from the angle between its temporary
// forward and desired movement. Camera-owned facing needs a directional gait
// without waiting for the body to turn. Only the local native vector changes.
inline bool Forward(const float* desired,const float* camera,float* output) noexcept {
    if(!desired||!camera||!output)return false;
    const float dx=desired[0],dz=desired[2],cx=camera[0],cz=camera[2];
    const float d2=dx*dx+dz*dz,c2=cx*cx+cz*cz;
    if(!std::isfinite(d2)||!std::isfinite(c2)||d2<.0001f||c2<.0001f)return false;
    const float di=1.f/std::sqrt(d2),ci=1.f/std::sqrt(c2);
    const float x=dx*di,z=dz*di;
    // Preserve the original backward gait/speed above its native 120deg cutoff.
    const float sign=(x*cx+z*cz)*ci<-.5f?-1.f:1.f;
    output[0]=x*sign;output[1]=0;output[2]=z*sign;output[3]=0;return true;
}
}
