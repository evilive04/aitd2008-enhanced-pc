#include "native_lamp_grip.h"
namespace scoped_lamp_aim {
static volatile LONG applied=0,rejected=0;
static volatile float lastBeamResidual=0,lastHandOffset=0,lastBlend=0;
static volatile float lastWristDeviation=0,lastHandDeviation=0;
static unsigned traceMask=0;
static unsigned rejectTraceMask=0;
static void TraceReject(const native_lamp_grip::Result& result){
 const unsigned reason=static_cast<unsigned>(result.reason);
 if(reason>=32||(rejectTraceMask&(1u<<reason)))return;
 rejectTraceMask|=1u<<reason;
 Log("native_lamp_grip rejected tick=%lu reason=%s/%u bone=%u iteration=%u distance=%g links=%g/%g beam=%g hand=%g elbow=%g wrist=%g handdev=%g",
  GetTickCount(),native_lamp_grip::ReasonName(result.reason),reason,result.bone,result.iteration,
  result.distance,result.upperLength,result.forearmLength,result.beamError,result.handOffset,
  result.elbow,result.wristDeviation,result.handDeviation);
}
static void Trace(unsigned reason,unsigned char* hero,unsigned char* animation){
 if(reason>=32||(traceMask&(1u<<reason)))return;
 traceMask|=1u<<reason;
 auto* hands=hero?Pointer(hero,0xf9c):nullptr;
 auto* left=hero?Pointer(hero,0xf7c):nullptr;auto* right=hero?Pointer(hero,0xf78):nullptr;
 auto* lamp=hands?Pointer(hands,4):nullptr;auto* pistol=hands?Pointer(hands,0x14):nullptr;
 Log("native_lamp_grip stage=%u aim=%d owner=%d age=%lu left=%08lx/%lu right=%08lx/%lu lamp=%08lx/%08lx pistol=%08lx/%08lx pose=%p/%p",
  reason,decision.aim,actionRoutingActive,GetTickCount()-actionControls.lastTick,
  left?Word(left,0):0,left?Word(left,0xb0):~0u,right?Word(right,0):0,right?Word(right,0xb0):~0u,
  lamp?Word(lamp,0):0,lamp?Word(lamp,0x58c):0,pistol?Word(pistol,0):0,pistol?Word(pistol,0x58c):0,
  animation?Pointer(animation,0x18):nullptr,animation?Pointer(animation,0x1c):nullptr);
}
static bool Items(unsigned char* hero,unsigned char* hands,unsigned char* lamp,unsigned char* pistol){
 if(!hero||!hands||Pointer(hands,0)!=hero||Pointer(hands,4)!=lamp||Pointer(hands,0x14)!=pistol||Pointer(hands,0x24))return false;
 // Calibrated for original unmodified Glock/Maglite rigid attachments.
 // Type/variant checks bound that support; they do not fingerprint modded
 // meshes or attachment data. Unverified variants retain their native pose.
 if(!IsLamp(lamp)||Word(lamp,0)!=0xc92e78||Word(lamp,0x58c)!=0||
    !IsPistol(pistol)||Word(pistol,0)!=0xc95000||Word(pistol,0x58c)!=0x30001)return false;
 auto* left=Pointer(hero,0xf7c);auto* right=Pointer(hero,0xf78);
 return left&&right&&Word(left,0)==0xc8bfc0&&Pointer(left,0x20)==hero&&Pointer(left,0x118)==lamp&&Word(left,0xb0)==2&&
  Word(right,0)==0xc89e68&&Pointer(right,0x20)==hero&&Pointer(right,0x118)==pistol&&Word(right,0xb0)==2;
}
static bool Apply(unsigned char* hero,unsigned char* animation){
 __try{
  if(!decision.aim||!actionRoutingActive||GetTickCount()-actionControls.lastTick>100||
     !scoped_gun_aim::NativeAimBound(hero,animation))return false;
  auto* hands=Pointer(hero,0xf9c);auto* lamp=hands?Pointer(hands,4):nullptr;auto* pistol=hands?Pointer(hands,0x14):nullptr;
  if(!lamp)return false;
  if(!Items(hero,hands,lamp,pistol)){Trace(1,hero,animation);return false;}
  auto* lookup=Pointer(animation,0x10);auto* pose=Pointer(animation,0x18);
  if(!lookup||!pose||!Pointer(animation,0x1c)||*reinterpret_cast<const WORD*>(lookup)!=199){Trace(2,hero,animation);return false;}
  for(unsigned i=0;i<15;++i){const int parent=native_pistol_grip::parents[i];const unsigned expected=parent<0?~0u:native_pistol_grip::bones[parent];
   if(Word(lookup,4+native_pistol_grip::bones[i]*4)!=expected)return false;bool found=false;
   for(unsigned n=0;n<199;++n)if(Word(lookup,4+199*4+n*4)==native_pistol_grip::hashes[i]){
    if(Word(lookup,4+199*8+n*4)!=native_pistol_grip::bones[i])return false;found=true;break;
   }if(!found)return false;
  }
  const ItemKey h=OwnedItemKey(hero),l=OwnedItemKey(lamp),p=OwnedItemKey(pistol);
  native_pistol_grip::Local local[15]{},before[15]{};
  for(unsigned i=0;i<15;++i)memcpy(&local[i],pose+48*native_pistol_grip::bones[i],48);memcpy(before,local,sizeof(local));
  native_lamp_grip::Result result{};if(!native_lamp_grip::Solve(local,result)){Trace(3,hero,animation);TraceReject(result);InterlockedIncrement(&rejected);return false;}
  if(!scoped_gun_aim::Owner(hero,animation)||Pointer(animation,0x18)!=pose||Pointer(animation,0x10)!=lookup||
     !Items(hero,hands,lamp,pistol)||!(OwnedItemKey(hero)==h)||!(OwnedItemKey(lamp)==l)||!(OwnedItemKey(pistol)==p))return false;
  for(unsigned i=0;i<15;++i)if(memcmp(&before[i],pose+48*native_pistol_grip::bones[i],48)!=0)return false;
  for(unsigned i=11;i<15;++i)memcpy(pose+48*native_pistol_grip::bones[i],&local[i].q,16);
  const DWORD changed[16]={0,0,0,0xc0000000u,3u};scoped_gun_aim::RebuildGrip(animation,changed);
  lastBlend=1.f;const auto finalPose=native_pistol_grip::World(local);
  lastBeamResidual=native_pistol_grip::Len(native_pistol_grip::Sub(
   native_pistol_grip::Unit(native_pistol_grip::Rotate(finalPose.q[14],{.752655306f,-.656597013f,-.048904295f})),
   native_pistol_grip::Unit(native_pistol_grip::Rotate(finalPose.q[9],{-.000130448f,.993567608f,.113240412f}))));
  lastHandOffset=result.handOffset;
  lastWristDeviation=result.wristDeviation;lastHandDeviation=result.handDeviation;
  if(InterlockedIncrement(&applied)<=4)Log("native_lamp_grip applied blend=%g beam_residual=%g hand_offset=%g frame=shared-parent",1.f,result.beamError,result.handOffset);
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){InterlockedIncrement(&rejected);return false;}
}
} // namespace scoped_lamp_aim
