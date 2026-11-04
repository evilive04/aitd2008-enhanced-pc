#pragma once
// Exact x86 native nominal-rate load replacement. It adds precisely one x87
// value, preserving all CPU flags, integer/SSE registers and the control word.
// The caller's unchanged FLD1/FDIVRP then obtains the reference delta.
namespace rope_reference_rate {
static __declspec(naked) void Load() {
 __asm {
  pushfd
  push eax
  cmp dword ptr ds:[00faecb8h],60
  jne native_rate
  mov eax,dword ptr ds:[00f503c8h]
  // Unsigned comparisons reject negative, zero, denormal, NaN and infinity.
  // Native lower-FPS/substep behavior is preserved. Bounds are exact float bits.
  cmp eax,03a83126fh
  jb native_rate
  cmp eax,03c888889h
  jae native_rate
  fld1
  fdiv dword ptr ds:[00f503c8h]
  jmp rate_loaded
 native_rate:
  fild dword ptr ds:[00faecb8h]
 rate_loaded:
  pop eax
  popfd
  ret
 }
}
}
