#include "../ballistic_aim_math.h"
#include "../aim_range.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
static unsigned checks=0;
static bool Check(bool value,const char* label){++checks;if(!value)std::fprintf(stderr,"FAIL %s\n",label);return value;}
int main(){
    using namespace ballistic_aim;
    float camera[4]{0,0,0,1},forward[4]{0,0,1,0},muzzle[4]{.15f,-.1f,.1f,1},target[4]{},direction[4]{};
    if(!Check(Convergence(camera,forward,muzzle,.25f,target,direction)==Result::Applied,"close convergence"))return 1;
    if(!Check(std::fabs(target[2]-.25f)<1e-6f&&target[3]==1&&direction[3]==0,"close target / homogeneous vector"))return 1;
    if(!Check(std::fabs(direction[0])>.5f,"close parallax is corrected"))return 1;
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    const auto infinity=std::numeric_limits<float>::infinity();
    const float badRanges[]{0,-1,nan,infinity};
    for(float hit:{.001f,.02f,.1f,.25f,1.f,3.f,10.f,99.f,100.f}){
        const float nativePadded=hit/aim_range::maximum*aim_range::query;
        if(!Check(std::fabs(aim_range::SceneDistance(nativePadded)-hit)<.00002f,"native camera padding removed from sight range"))return 1;
    }
    if(!Check(aim_range::SceneDistance(aim_range::query)==100.f,"native no-hit remains full weapon range"))return 1;
    for(float invalid:{-1.f,nan,infinity,102.f})
        if(!Check(aim_range::SceneDistance(invalid)<0,"bad native result rejected"))return 1;
    for(float range:badRanges)if(!Check(Convergence(camera,forward,muzzle,range,target,direction)==Result::InvalidRange,"invalid range"))return 1;
    float nearMuzzle[4]{0,0,0,1};
    if(!Check(Convergence(camera,forward,nearMuzzle,.001f,target,direction)==Result::Applied&&std::fabs(target[2]-.02f)<1e-7f,"two centimetre clamp"))return 1;
    if(!Check(Convergence(camera,forward,nearMuzzle,500,target,direction)==Result::Applied&&target[2]==100,"maximum range clamp"))return 1;
    float behind[4]{0,0,.3f,1};
    float oldDirection[4]{4,5,6,7},oldTarget[4]{8,9,10,11};
    memcpy(direction,oldDirection,16);memcpy(target,oldTarget,16);
    if(!Check(Convergence(camera,forward,behind,.2f,target,direction)==Result::TargetBehindMuzzle&&
        !memcmp(direction,oldDirection,16)&&!memcmp(target,oldTarget,16),"target behind muzzle preserves native output"))return 1;
    if(!Check(Convergence(camera,forward,behind,.3f,target,direction)==Result::TargetAtMuzzle,"coincident target"))return 1;
    float sideways[4]{.5f,0,.2f,1};
    if(!Check(Convergence(camera,forward,sideways,.2f,target,direction)==Result::TargetBehindMuzzle,"perpendicular muzzle plane"))return 1;
    for(unsigned axis=0;axis<3;++axis){
        float bad[4]{};memcpy(bad,camera,16);bad[axis]=nan;
        if(!Check(Convergence(bad,forward,muzzle,1,target,direction)==Result::InvalidRay,"invalid camera"))return 1;
        memcpy(bad,muzzle,16);bad[axis]=infinity;
        if(!Check(Convergence(camera,forward,bad,1,target,direction)==Result::InvalidMuzzle,"invalid muzzle"))return 1;
        memcpy(bad,forward,16);bad[axis]=nan;
        if(!Check(Convergence(camera,bad,muzzle,1,target,direction)==Result::InvalidRay,"invalid forward"))return 1;
    }
    float zero[4]{};if(!Check(Convergence(camera,zero,muzzle,1,target,direction)==Result::InvalidRay,"zero forward"))return 1;
    std::mt19937 random(103);std::uniform_real_distribution<float> position(-650,650),component(-1,1),length(.05f,100);
    for(unsigned n=0;n<1024;++n){
        for(unsigned a=0;a<3;++a){camera[a]=position(random);forward[a]=component(random);}
        const float norm=std::sqrt(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]);
        if(norm<.01f){--n;continue;}
        for(unsigned a=0;a<3;++a){forward[a]/=norm;muzzle[a]=camera[a]+component(random)*.01f;}
        float previous[4]{};memcpy(previous,muzzle,16);
        const float range=length(random);
        if(!Check(Convergence(camera,forward,muzzle,range,target,direction)==Result::Applied,"random convergence"))return 1;
        float squared=0,error=0;
        for(unsigned a=0;a<3;++a){squared+=direction[a]*direction[a];const float e=target[a]-muzzle[a];error+=e*e;}
        const float distance=std::sqrt(error);
        float maximumError=0;
        for(unsigned a=0;a<3;++a){const float e=std::fabs(muzzle[a]+direction[a]*distance-target[a]);if(e>maximumError)maximumError=e;}
        if(!Check(std::fabs(squared-1)<5e-6f&&maximumError<.0002f&&!memcmp(previous,muzzle,16),"ray reaches camera target; origin unchanged"))return 1;
    }
    std::printf("PASS %u convergence geometry, close/far, invalid inputs and origin preservation checks\n",checks);
    return 0;
}
