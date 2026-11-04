#pragma once
#include "native_pistol_grip.h"
// Original GUN_TOR_AIM hand support, with exact fixed-length placement.
// Only the four left-arm rotations may be committed by the caller.
namespace native_lamp_grip {
using namespace native_pistol_grip;
enum class RejectReason : unsigned {
 Unknown, None, NonFiniteLocal, NonUnitQuaternion, NonUnitScale, InvalidLinkLength,
 UnreachableTarget, DegenerateElbowPlane, BeamResidual, HandPositionResidual,
 ElbowLimit, WristLimit, HandLimit, InvalidSharedScale
};
struct Result {
 float beamError{},handOffset{},elbow{},wristDeviation{},handDeviation{};
 // Diagnostic only: these fields never feed the solver or its constraints.
 RejectReason reason{RejectReason::Unknown};unsigned bone{~0u},iteration{~0u};
 float distance{},upperLength{},forearmLength{};
};
inline bool Reject(Result& out,RejectReason reason,unsigned bone=~0u){out.reason=reason;out.bone=bone;return false;}
inline const char* ReasonName(RejectReason reason){
 switch(reason){
 case RejectReason::None:return "none";case RejectReason::NonFiniteLocal:return "nonfinite-local";
 case RejectReason::NonUnitQuaternion:return "quaternion";case RejectReason::NonUnitScale:return "scale";
 case RejectReason::InvalidLinkLength:return "link-length";case RejectReason::UnreachableTarget:return "reach";
 case RejectReason::DegenerateElbowPlane:return "elbow-plane";case RejectReason::BeamResidual:return "beam-residual";
 case RejectReason::HandPositionResidual:return "hand-position";case RejectReason::ElbowLimit:return "elbow-limit";
 case RejectReason::WristLimit:return "wrist-limit";case RejectReason::HandLimit:return "hand-limit";
 case RejectReason::InvalidSharedScale:return "shared-scale";
 default:return "unknown";
 }
}
inline bool Solve(Local (&local)[15],Result& out){
 // Both arm chains descend from bone13 (our index4). Solve in that shared
 // parent's coordinate frame, before its authored breathing/aim scale.
 // The native builder later applies the SAME complete affine ancestor chain
 // to both hands, preserving their contact and matching beam directions.
 constexpr unsigned sharedParent=4;
 static_assert(bones[sharedParent]==13&&parents[5]==4&&parents[10]==4,"Native lamp shared-parent layout");
 out={};Local work[15];for(unsigned i=0;i<15;++i){work[i]=local[i];
  const float* f=reinterpret_cast<const float*>(&local[i]);for(unsigned j=0;j<12;++j)if(!std::isfinite(f[j]))return Reject(out,RejectReason::NonFiniteLocal,bones[i]);
  if(std::fabs(DotQ(local[i].q,local[i].q)-1)>.02f)return Reject(out,RejectReason::NonUnitQuaternion,bones[i]);
  for(unsigned j=0;j<3;++j){
   if(i<=sharedParent){
    // Accept finite positive authored scale, fail closed for singular,
    // mirrored or extreme unsupported rigs. The 1/4..4 range comfortably
    // contains the captured native ~1.01 trunk animation without dividing
    // by near-zero values or normalizing any real skeleton/mesh transform.
    if(local[i].s[j]<.25f||local[i].s[j]>4.f)return Reject(out,RejectReason::InvalidSharedScale,bones[i]);
   }else if(std::fabs(local[i].s[j]-1)>.001f)return Reject(out,RejectReason::NonUnitScale,bones[i]);
  }
 }
 // Explicit reference-frame change on SCRATCH DATA ONLY. Keeping all five
 // shared ancestors in the old scale-free World() calculation would be
 // incorrect; merely dropping their unit-scale guard is not sufficient.
 // Only the four final local left-arm quaternions are ever committed below.
 for(unsigned i=0;i<=sharedParent;++i){
  work[i].q={0,0,0,1};
  for(unsigned j=0;j<4;++j){work[i].t[j]=0;work[i].s[j]=j<3?1.f:0.f;}
 }
 // Original authored hand-to-hand support; no backwards positional compromise.
 constexpr V support{-.0474682094f,.0078600404f,.0885207331f};
 const Q relative=Normal({-.575281826f,-.328548491f,.672826732f,.329258103f});
 const Q authoredWrist=Normal({.013824885f,.883419286f,-.080019535f,.461493349f});
 // Preserve the authored upper-arm roll rather than inheriting the unrelated
 // relaxed lamp pose's elbow plane. Clavicle and every translation stay native.
 work[14].q=Normal({-.194921717f,-.511093497f,-.341074854f,.764497817f});
 work[11].q=Normal({-.382149696f,.348130673f,-.268597573f,.812786579f});
 // The original skin weights use BOTH wrist128 and hand129. Split the
 // relative carpal flex between them while retaining the final hand transform.
 // Eight bounded fixed-point steps also resolve the native short wrist offset;
 // no translation/scale or lamp attachment is changed.
 const Q authoredHand=Inv(work[14].q);const Pose initial=World(work);const V target=Add(initial.p[9],Rotate(initial.q[9],support));
 for(unsigned iteration=0;iteration<8;++iteration){
 work[11].q=Normal({-.382149696f,.348130673f,-.268597573f,.812786579f});
 const Pose start=World(work);
 const Q hand=Product(start.q[9],relative);
 const Q wrist=Product(hand,Inv(Inv(Normal(work[14].q))));
 const Q forearm=Product(wrist,Inv(authoredWrist));
 const V forearmT{work[13].t[0],work[13].t[1],work[13].t[2]},handT{work[14].t[0],work[14].t[1],work[14].t[2]};
 const V goal=Sub(target,Rotate(wrist,handT)),origin=start.p[11];
 const V upperT{work[12].t[0],work[12].t[1],work[12].t[2]};
 const float l1=Len(upperT),l2=Len(forearmT),distance=Len(Sub(goal,origin));
 out.iteration=iteration;out.distance=distance;out.upperLength=l1;out.forearmLength=l2;
 if(l1<.08f||l1>.5f||l2<.08f||l2>.5f)return Reject(out,RejectReason::InvalidLinkLength);
 if(distance<.01f||distance>=l1+l2-.00001f||distance<=std::fabs(l1-l2)+.00001f)return Reject(out,RejectReason::UnreachableTarget);
 const V axis=Unit(Sub(goal,origin));const float x=(l1*l1-l2*l2+distance*distance)/(2*distance);
 const V center=Add(origin,Mul(axis,x));
 // Both native link lengths and the requested hand position are constraints.
 // Choose the elbow on their intersection circle closest to the neutral
 // authored forearm; the previous upper-arm sphere allowed a20cm hand drift.
 const V preferredElbow=Sub(goal,Rotate(forearm,forearmT));
 V plane=Sub(Sub(preferredElbow,center),Mul(axis,Dot(Sub(preferredElbow,center),axis)));
 if(Len(plane)<.00001f){const V pole=Sub(start.p[12],center);plane=Sub(pole,Mul(axis,Dot(pole,axis)));}
 if(Len(plane)<.00001f)return Reject(out,RejectReason::DegenerateElbowPlane);
 const V elbow=Add(center,Mul(Unit(plane),std::sqrt(std::max(0.f,l1*l1-x*x))));
 const Q upperTurn=Between(Sub(start.p[12],origin),Sub(elbow,origin));
 const Q forearmTurn=Between(Rotate(forearm,forearmT),Sub(goal,elbow));
 SetWorld(work,11,Product(upperTurn,start.q[11]));SetWorld(work,12,Product(forearmTurn,forearm));SetWorld(work,13,wrist);
 if(iteration<7){
  Q total=Normal(Product(Product(Inv(authoredWrist),Inv(work[13].q)),Product(Inv(work[14].q),Inv(authoredHand))));
  if(total.w<0)total={-total.x,-total.y,-total.z,-total.w};
  const Q half=Normal({total.x,total.y,total.z,total.w+1.f});work[14].q=Inv(Product(half,authoredHand));
 }
 }
 const Pose final=World(work);
 constexpr V beam{.752655306f,-.656597013f,-.048904295f};
 constexpr V barrel{-.000130448f,.993567608f,.113240412f};
 out.beamError=Len(Sub(Unit(Rotate(final.q[14],beam)),Unit(Rotate(final.q[9],barrel))));
 out.handOffset=Len(Sub(final.p[14],target));
 out.elbow=std::acos(std::clamp(Dot(Unit(Sub(final.p[11],final.p[12])),Unit(Sub(final.p[13],final.p[12]))),-1.f,1.f));
 out.handDeviation=Angle(Inv(work[14].q),authoredHand);out.wristDeviation=Angle(Inv(work[13].q),authoredWrist);
 if(!std::isfinite(out.beamError)||out.beamError>.0001f)return Reject(out,RejectReason::BeamResidual);
 if(out.handOffset>.0001f)return Reject(out,RejectReason::HandPositionResidual);
 if(out.elbow<1.22f||out.elbow>2.96706f)return Reject(out,RejectReason::ElbowLimit);
 if(out.wristDeviation>.61086524f)return Reject(out,RejectReason::WristLimit);
 if(out.handDeviation>.61086524f)return Reject(out,RejectReason::HandLimit);
 for(unsigned i=11;i<15;++i)local[i].q=work[i].q;out.reason=RejectReason::None;return true;
}
} // namespace native_lamp_grip
