#pragma once
#include <windows.h>
struct FastForwardStateV1 {
    DWORD size,version,flags,rateQ16,tick;
    DWORD reserved[3];
};
static_assert(sizeof(FastForwardStateV1)==32,"Fast-forward ABI must remain32bytes");
using FastForwardStateGetter=BOOL (WINAPI*)(FastForwardStateV1*,DWORD);
