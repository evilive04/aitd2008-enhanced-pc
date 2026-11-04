#pragma once
#include <cmath>
namespace ballistic_aim {
enum class Result : unsigned { Ineligible=0,Applied=1,InvalidRay=2,InvalidRange=3,InvalidMuzzle=4,TargetBehindMuzzle=5,TargetAtMuzzle=6,Exception=7 };
inline bool UnitForward(const float* source,float* unit) {
    if(!source||!unit)return false;
    for(unsigned n=0;n<3;++n)if(!std::isfinite(source[n]))return false;
    const float norm=source[0]*source[0]+source[1]*source[1]+source[2]*source[2];
    if(!std::isfinite(norm)||norm<.5f||norm>1.5f)return false;
    const float inverse=1.0f/std::sqrt(norm);
    for(unsigned n=0;n<3;++n)unit[n]=source[n]*inverse;
    unit[3]=0;return true;
}
inline Result Convergence(const float* camera,const float* forward,const float* muzzle,float range,float* target,float* direction) {
    if(!camera||!forward||!muzzle||!target||!direction)return Result::InvalidRay;
    for(unsigned n=0;n<3;++n) {
        if(!std::isfinite(camera[n])||!std::isfinite(forward[n]))return Result::InvalidRay;
        if(!std::isfinite(muzzle[n]))return Result::InvalidMuzzle;
    }
    float unitForward[4]{};
    if(!UnitForward(forward,unitForward))return Result::InvalidRay;
    if(!std::isfinite(range)||range<=0)return Result::InvalidRange;
    const float distance=range<.02f?.02f:range>100.0f?100.0f:range;
    float destination[4]{0,0,0,1},ray[4]{};
    float squared=0,along=0;
    for(unsigned n=0;n<3;++n) {
        const float unit=unitForward[n];
        destination[n]=camera[n]+unit*distance;
        ray[n]=destination[n]-muzzle[n];
        squared+=ray[n]*ray[n];along+=ray[n]*unit;
    }
    if(!std::isfinite(squared)||!std::isfinite(along))return Result::InvalidMuzzle;
    if(squared<1.0e-8f)return Result::TargetAtMuzzle;
    if(along<=0)return Result::TargetBehindMuzzle;
    const float reciprocal=1.0f/std::sqrt(squared);
    for(unsigned n=0;n<3;++n){target[n]=destination[n];direction[n]=ray[n]*reciprocal;}
    target[3]=1;direction[3]=0;
    return Result::Applied;
}
}
