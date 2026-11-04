#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace fiery_trail {
// Native trail stores a four-lane origin/direction and a scalar length. Do not
// silently normalize it here: that would conceal the geometry being diagnosed.
inline bool Endpoint(const float* origin,const float* direction,float length,float* result){
    if(!origin||!direction||!result||!std::isfinite(length)||length<0)return false;
    float temporary[4]{};
    for(unsigned i=0;i<4;++i){
        if(!std::isfinite(origin[i])||!std::isfinite(direction[i]))return false;
        temporary[i]=origin[i]+direction[i]*length;
        if(!std::isfinite(temporary[i]))return false;
    }
    for(unsigned i=0;i<4;++i)result[i]=temporary[i];return true;
}
inline float Distance3(const float* a,const float* b){
    float squared=0;for(unsigned i=0;i<3;++i){const float d=a[i]-b[i];squared+=d*d;}
    return std::isfinite(squared)?std::sqrt(squared):-1.0f;
}
// Read-only diagnostic: distance from a point to the infinite measured query
// axis. Do not assume a normalized direction or use the recomputed segment end
// as proof that its visible centerline shares the physical ray's origin.
inline bool AxisDistance(const float* point,const float* origin,const float* direction,float* result){
    if(!point||!origin||!direction||!result)return false;
    double squared=0,dot=0;for(unsigned i=0;i<3;++i){if(!std::isfinite(point[i])||!std::isfinite(origin[i])||!std::isfinite(direction[i]))return false;
        squared+=static_cast<double>(direction[i])*direction[i];dot+=(static_cast<double>(point[i])-origin[i])*direction[i];}
    if(!std::isfinite(squared)||squared<.000000000001)return false;
    const double along=dot/squared;double error=0;
    for(unsigned i=0;i<3;++i){const double delta=static_cast<double>(point[i])-origin[i]-along*direction[i];error+=delta*delta;}
    if(!std::isfinite(error))return false;
    const double distance=std::sqrt(error);if(!std::isfinite(distance)||distance>1.0e20)return false;
    *result=static_cast<float>(distance);return std::isfinite(*result);
}
// Native7017C0 feeds62FB10 a moving-head frame and local strip length
// min(travel,effect+128).62FB10 emits opposite XY pairs at localZ=0 andZ=length;
// their WORLD centers are T andT+length*MZ. This inspects that actual renderer
// geometry, never substitutes full segment length or guesses a particle axis.
inline bool StreakCenters(const float* matrix,float travel,float maximum,float* head,float* tail,float* length){
    if(!matrix||!head||!tail||!length||!std::isfinite(travel)||travel<0||!std::isfinite(maximum)||maximum<0)return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(matrix[i]))return false;
    const float extent=maximum<travel?maximum:travel;float h[3]{},t[3]{};
    for(unsigned i=0;i<3;++i){h[i]=matrix[12+i];t[i]=h[i]+extent*matrix[8+i];if(!std::isfinite(t[i]))return false;}
    for(unsigned i=0;i<3;++i){head[i]=h[i];tail[i]=t[i];}*length=extent;return true;
}
// Express the genuine world gun-marker direction in the unmodified FX frame.
// Keeping these coordinates makes later parent/animation movement coherent;
// retaining a world vector from the shot would be stale when the gun rotates.
inline bool FrameLocalDirection(const float* matrix,const float* world,float* local){
    if(!matrix||!world||!local)return false;
    for(unsigned row=0;row<3;++row)for(unsigned i=0;i<3;++i)if(!std::isfinite(matrix[4*row+i])||!std::isfinite(world[i]))return false;
    float cofactors[3][3]{};
    for(unsigned row=0;row<3;++row){const auto* a=matrix+4*((row+1)%3);const auto* b=matrix+4*((row+2)%3);
        cofactors[row][0]=a[1]*b[2]-a[2]*b[1];cofactors[row][1]=a[2]*b[0]-a[0]*b[2];cofactors[row][2]=a[0]*b[1]-a[1]*b[0];}
    const float det=matrix[0]*cofactors[0][0]+matrix[1]*cofactors[0][1]+matrix[2]*cofactors[0][2];
    if(!std::isfinite(det)||std::fabs(det)<.000001f)return false;
    float result[3]{};for(unsigned row=0;row<3;++row){for(unsigned i=0;i<3;++i)result[row]+=world[i]*cofactors[row][i];result[row]/=det;if(!std::isfinite(result[row]))return false;}
    for(unsigned i=0;i<3;++i)local[i]=result[i];return true;
}
inline bool FrameWorldDirection(const float* matrix,const float* local,float* world){
    if(!matrix||!local||!world)return false;
    float result[3]{};for(unsigned i=0;i<3;++i){for(unsigned row=0;row<3;++row){if(!std::isfinite(local[row])||!std::isfinite(matrix[4*row+i]))return false;result[i]+=local[row]*matrix[4*row+i];}if(!std::isfinite(result[i]))return false;}
    for(unsigned i=0;i<3;++i)world[i]=result[i];return true;
}
// Rotate the authored frame by the same world-space change as its genuine
// gun-marker direction. No particle-local forward-axis convention is assumed.
// Translation, row lengths, handedness and roll are preserved; never shear it.
inline bool AimEmitterFromDirection(float* matrix,const float* nativeForward,const float* hit){
    if(!matrix||!nativeForward||!hit)return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(matrix[i]))return false;
    float direction[3]{},length=0,forwardLength=0;
    for(unsigned i=0;i<3;++i){if(!std::isfinite(hit[i])||!std::isfinite(nativeForward[i]))return false;direction[i]=hit[i]-matrix[12+i];length+=direction[i]*direction[i];forwardLength+=nativeForward[i]*nativeForward[i];}
    if(!std::isfinite(length)||length<.0001f||!std::isfinite(forwardLength)||forwardLength<.000001f)return false;
    length=std::sqrt(length);forwardLength=std::sqrt(forwardLength);
    float from[3]{};for(unsigned i=0;i<3;++i){from[i]=nativeForward[i]/forwardLength;direction[i]/=length;}
    float axis[3]{from[1]*direction[2]-from[2]*direction[1],from[2]*direction[0]-from[0]*direction[2],from[0]*direction[1]-from[1]*direction[0]};
    float cosine=from[0]*direction[0]+from[1]*direction[1]+from[2]*direction[2];
    if(cosine>1)cosine=1;if(cosine<-1)cosine=-1;
    float sine=std::sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);
    if(sine<.000001f){
        if(cosine>0)return true;
        // At 180 degrees use an existing orthogonal frame axis. If its right
        // row is parallel to the marker direction, try the other two rows.
        float squared=0;for(unsigned row=0;row<3&&squared<.000001f;++row){squared=0;const auto* v=matrix+4*row;const float dot=v[0]*from[0]+v[1]*from[1]+v[2]*from[2];for(unsigned i=0;i<3;++i){axis[i]=v[i]-from[i]*dot;squared+=axis[i]*axis[i];}}
        if(squared<.000001f||!std::isfinite(squared))return false;
        squared=std::sqrt(squared);for(unsigned i=0;i<3;++i)axis[i]/=squared;sine=0;
    }else for(unsigned i=0;i<3;++i)axis[i]/=sine;
    float rotated[12]{};
    for(unsigned row=0;row<3;++row){const float* v=matrix+row*4;const float dot=v[0]*axis[0]+v[1]*axis[1]+v[2]*axis[2];
        const float cross[3]{axis[1]*v[2]-axis[2]*v[1],axis[2]*v[0]-axis[0]*v[2],axis[0]*v[1]-axis[1]*v[0]};
        for(unsigned i=0;i<3;++i){rotated[row*4+i]=v[i]*cosine+cross[i]*sine+axis[i]*dot*(1-cosine);if(!std::isfinite(rotated[row*4+i]))return false;}
        rotated[row*4+3]=v[3];
    }
    for(unsigned i=0;i<12;++i)matrix[i]=rotated[i];return true;
}
// Compatibility helper for callers whose resource explicitly defines +Z.
inline bool AimEmitter(float* matrix,const float* hit){
    return matrix&&AimEmitterFromDirection(matrix,matrix+8,hit);
}

// Exact native CanonTrail resource, not a guessed particle forward axis.
// 911AF0 relocates the raw emitter offset at60;9172D0 shape8/subtype3 emits
// positions (x,0,0), x in[-.5,.5], transformed by its authored matrix at80.
inline bool GlockTrailLine(const unsigned char* resource,std::size_t bytes,float* nearPoint,float* farPoint){
    if(!resource||!nearPoint||!farPoint||bytes!=57040)return false;
    auto word=[resource](std::size_t offset){std::uint32_t value{};std::memcpy(&value,resource+offset,4);return value;};
    if(word(8)!=bytes||word(12)!=0x5452502eu||word(16)!=0x5452502eu||word(20)!=0x18u||word(0x60)!=0x150u||word(0x68)!=1u)return false;
    const auto* emitter=resource+0x150;
    if(emitter[0x28a]!=8||emitter[0x28b]!=0x30||emitter[0x2f0]!=1)return false;
    float local[16]{};std::memcpy(local,emitter+0x80,sizeof(local));
    for(float value:local)if(!std::isfinite(value))return false;
    float a[3]{},b[3]{};
    for(unsigned i=0;i<3;++i){a[i]=local[12+i]+.5f*local[i];b[i]=local[12+i]-.5f*local[i];if(!std::isfinite(a[i])||!std::isfinite(b[i]))return false;}
    for(unsigned i=0;i<3;++i){nearPoint[i]=a[i];farPoint[i]=b[i];}return true;
}
// Rigidly place the genuine authored flame line on this single shot's ray.
// Its measured line is tilted2.32deg from group+Z. Preserve every basis length,
// handedness and texture coordinate; no emitter/particle geometry is modified.
inline bool AimAuthoredLine(float* matrix,const float* nearPoint,const float* farPoint,const float* shotOrigin,const float* shotEnd){
    if(!matrix||!nearPoint||!farPoint||!shotOrigin||!shotEnd)return false;
    float result[16],localAxis[3]{},worldAxis[3]{};std::memcpy(result,matrix,sizeof(result));
    for(unsigned i=0;i<3;++i){if(!std::isfinite(nearPoint[i])||!std::isfinite(farPoint[i])||!std::isfinite(shotOrigin[i]))return false;localAxis[i]=farPoint[i]-nearPoint[i];result[12+i]=shotOrigin[i];}
    if(!FrameWorldDirection(matrix,localAxis,worldAxis)||!AimEmitterFromDirection(result,worldAxis,shotEnd))return false;
    float nearOffset[3]{};if(!FrameWorldDirection(result,nearPoint,nearOffset))return false;
    for(unsigned i=0;i<3;++i){result[12+i]=shotOrigin[i]-nearOffset[i];if(!std::isfinite(result[12+i]))return false;}
    std::memcpy(matrix,result,sizeof(result));return true;
}

}
