#pragma once
#include <cmath>
#include <algorithm>
// Original native Middle grip constraint. Rotations only: no rig, skin,
// translation, scale, finger, clavicle or trunk replacement.
namespace native_pistol_grip {
struct V {float x,y,z;};struct Q {float x,y,z,w;};
struct Local {Q q;float t[4],s[4];};
static constexpr unsigned bones[15]={0,1,11,12,13,99,100,101,102,103,125,126,127,128,129};
static constexpr unsigned hashes[15]={0x746f6f52,0x564c9899,0x4d979794,0x4e55529f,0xa7948cf2,0x9b8de1eb,0x5f4d5293,0xa49fa1d9,0x539bb1ab,0x444e93a7,0x9b8ddbeb,0x5f4d528d,0xa49fa1d3,0x5395b1ab,0x444e8da7};
static constexpr int parents[15]={-1,0,1,2,3,4,5,6,7,8,4,10,11,12,13};
inline V Add(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V Sub(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V Mul(V a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V Cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Len(V a){return std::sqrt(Dot(a,a));}
inline V Unit(V a){const float n=Len(a);return n>1e-8f?Mul(a,1/n):V{};}
inline Q Inv(Q a){return {-a.x,-a.y,-a.z,a.w};}
inline Q Product(Q a,Q b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline float DotQ(Q a,Q b){return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;}
inline Q Normal(Q a){const float n=std::sqrt(DotQ(a,a));return n>1e-8f?Q{a.x/n,a.y/n,a.z/n,a.w/n}:Q{0,0,0,1};}
inline V Rotate(Q q,V p){const V u{q.x,q.y,q.z};return Add(p,Mul(Cross(u,Add(Cross(u,p),Mul(p,q.w))),2));}
inline Q Between(V a,V b){a=Unit(a);b=Unit(b);const V c=Cross(a,b);return Normal({c.x,c.y,c.z,1+Dot(a,b)});}
inline float Angle(Q a,Q b){return 2*std::acos(std::clamp(std::fabs(DotQ(Normal(a),Normal(b))),0.f,1.f));}
struct Pose {Q q[15]{};V p[15]{};};
inline Pose World(const Local (&local)[15]){
 Pose out{};for(unsigned i=0;i<15;++i){const Q q=Inv(Normal(local[i].q));const V t{local[i].t[0],local[i].t[1],local[i].t[2]};const int p=parents[i];
  out.q[i]=p<0?q:Product(out.q[p],q);out.p[i]=p<0?t:Add(out.p[p],Rotate(out.q[p],t));}return out;
}
inline void SetWorld(Local (&local)[15],unsigned i,Q q){const Pose world=World(local);const int p=parents[i];local[i].q=Inv(Normal(p<0?q:Product(Inv(world.q[p]),q)));}
struct Result {float before{},after{},clamp{},angle[3]{};};
inline bool Solve(Local (&local)[15],Result& result){
 result={};Local work[15];for(unsigned i=0;i<15;++i){work[i]=local[i];
  const float* f=reinterpret_cast<const float*>(&local[i]);for(unsigned n=0;n<12;++n)if(!std::isfinite(f[n]))return false;
  if(std::fabs(DotQ(local[i].q,local[i].q)-1)>.02f)return false;
  for(unsigned n=0;n<3;++n)if(std::fabs(local[i].s[n]-1)>.001f)return false;
 }
 Pose world=World(work);
 // Authored GUN_AIM Middle hand-to-hand transform, with real native palm
 // surface support verified against both original pistol meshes. Not a
 // wrist coincidence: this preserves the original91mm hand separation.
 constexpr V support{-.0468931998f,.0098894311f,.0912484787f};
 const Q supportRotation=Normal({.277792916f,-.825546677f,.191912586f,-.452187283f});
 const V target=Add(world.p[9],Rotate(world.q[9],support));const Q hand=Product(world.q[9],supportRotation);
 result.before=Len(Sub(target,world.p[14]));if(result.before>.08f)return false;
 const Q wrist=Product(hand,Inv(Inv(Normal(work[14].q))));
 const V wristOffset{work[14].t[0],work[14].t[1],work[14].t[2]};
 V goal=Sub(target,Rotate(wrist,wristOffset));const V origin=world.p[11];const V delta=Sub(goal,origin);const float distance=Len(delta);
 const float l1=Len({work[12].t[0],work[12].t[1],work[12].t[2]}),l2=Len({work[13].t[0],work[13].t[1],work[13].t[2]});
 if(distance<.01f||l1<.08f||l2<.08f||l1>.5f||l2>.5f)return false;
 const float reach=std::clamp(distance,std::fabs(l1-l2)+.00001f,l1+l2-.00001f);result.clamp=std::fabs(distance-reach);if(result.clamp>.001f)return false;
 const V axis=Mul(delta,1/distance);goal=Add(origin,Mul(axis,reach));
 const V pole=Sub(world.p[12],origin);V plane=Sub(pole,Mul(axis,Dot(pole,axis)));if(Len(plane)<.0001f)return false;plane=Unit(plane);
 const float x=(l1*l1-l2*l2+reach*reach)/(2*reach);const V elbow=Add(Add(origin,Mul(axis,x)),Mul(plane,std::sqrt(std::max(0.f,l1*l1-x*x))));
 SetWorld(work,11,Product(Between(Sub(world.p[12],origin),Sub(elbow,origin)),world.q[11]));world=World(work);
 SetWorld(work,12,Product(Between(Sub(world.p[13],world.p[12]),Sub(goal,world.p[12])),world.q[12]));SetWorld(work,13,wrist);
 world=World(work);result.after=Len(Sub(world.p[14],target));if(!std::isfinite(result.after)||result.after>.0001f)return false;
 for(unsigned i=0;i<3;++i){result.angle[i]=Angle(local[11+i].q,work[11+i].q);if(result.angle[i]>.436332313f)return false;}
 // Transactional commit touches exactly these three quaternions.
 for(unsigned i=11;i<14;++i)local[i].q=work[i].q;return true;
}
} // namespace native_pistol_grip
