#pragma once
#include <windows.h>
enum UiGuardFlags:DWORD{UiGuardActive=1,UiGuardPointer=2,UiGuardFrontend=4,UiGuardTitle=8,UiGuardMenuPaint=16,UiGuardRenderer=32};
struct UiGuardStateV1{DWORD size=sizeof(UiGuardStateV1),version=1,tick{},flags{},roots[5]{},suppressed{},warmup{};};
static_assert(sizeof(UiGuardStateV1)==44,"UI guard copied protocol");
using UiGuardGetter=BOOL(WINAPI*)(void*,DWORD);
using UiGuardSetter=BOOL(WINAPI*)(BOOL,DWORD);
