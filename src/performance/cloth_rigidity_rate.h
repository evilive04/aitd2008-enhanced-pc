#pragma once
// Native4E8410 expresses geometric length-error deadzones as 1-rigidity.
// Its separate force/damping/correction factors still use actual physics dt.
// At >60Hz, multiplying rigidity by60/frequency incorrectly widens those
// deadzones. Preserve their authored60Hz values without changing any clock.
namespace cloth_rigidity_rate {
static volatile long highFrequencyApplications=0;
static __declspec(naked) void ScaleRigid0(){
 __asm {
  pushfd
  push eax
  movd eax,xmm1
  // At all four authenticated sites XMM1 is exactly60/nativeFrequency.
  // Positive finite float bits below1 select high-frequency behavior only.
  test eax,eax
  jle original_scale
  cmp eax,03f800000h
  jae original_scale
  lock inc dword ptr [highFrequencyApplications]
  jmp rigidity_ready
 original_scale:
  mulss xmm0,xmm1
 rigidity_ready:
  movaps xmm2,xmm4
  pop eax
  popfd
  ret
 }
}
static __declspec(naked) void ScaleRigid3(){
 __asm {
  pushfd
  push eax
  movd eax,xmm1
  test eax,eax
  jle original_scale
  cmp eax,03f800000h
  jae original_scale
  lock inc dword ptr [highFrequencyApplications]
  jmp rigidity_ready
 original_scale:
  mulss xmm3,xmm1
 rigidity_ready:
  movaps xmm2,xmm4
  pop eax
  popfd
  ret
 }
}
}
