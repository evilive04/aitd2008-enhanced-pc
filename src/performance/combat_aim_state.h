#pragma once
#include <windows.h>
// flags bit0: owned native gun animation and upper-body look have a fresh aim ray.
struct CombatAimStateV1 {
    DWORD size,version,flags,tick;
    float x,y;
    DWORD reserved[2];
};
static_assert(sizeof(CombatAimStateV1)==32,"Combat aim ABI must remain32bytes");
using CombatAimStateGetter=BOOL (WINAPI*)(CombatAimStateV1*,DWORD);
