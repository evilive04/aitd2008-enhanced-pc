#pragma once
#include <windows.h>
// Pointer-free native pause navigation. Values expire after250ms.
struct PauseStateV1 {
    DWORD size,version,tick,flags,generation,enabledMask,dispatches,rejected;
    wchar_t labels[6][128]; // Native slots: resume, skip, options, save, load, quit.
    wchar_t title[128],episode[128],sequence[128],synopsis[768];
};
static_assert(sizeof(PauseStateV1)==3872,"Pause copied UTF16 ABI");
enum PauseFlags : DWORD {PauseVisible=1,PauseDetails=2,PauseBusy=4};
using PauseGetter=BOOL(WINAPI*)(void*,DWORD);
using PauseSender=BOOL(WINAPI*)(DWORD,DWORD);
using PauseReady=void(WINAPI*)(BOOL,DWORD);
