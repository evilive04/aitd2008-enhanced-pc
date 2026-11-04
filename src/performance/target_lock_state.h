#pragma once
#include <windows.h>
// Renderer receives a copied native selection; it never dereferences actors.
struct TargetLockStateV1 {
    DWORD size,version,flags,tick;
    DWORD actor,guidLow,guidHigh,reserved;
    float world[3];DWORD padding;
};
static_assert(sizeof(TargetLockStateV1)==48,"Target lock ABI");
using TargetLockStateGetter=BOOL (WINAPI*)(TargetLockStateV1*,DWORD);
// V2 supplies the game's own world-to-screen result. Selection validity remains
// independent of whether the selected enemy's anchor is visible on screen.
struct TargetLockStateV2 {
    DWORD size,version,flags,tick;
    DWORD actor,guidLow,guidHigh,projectionStatus;
    float world[3];DWORD padding;
    float screenX,screenY;DWORD projectionTick,camera;
};
static_assert(sizeof(TargetLockStateV2)==64,"Target lock projected ABI");
using TargetLockProjectedStateGetter=BOOL (WINAPI*)(TargetLockStateV2*,DWORD);
