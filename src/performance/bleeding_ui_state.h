#pragma once
#include <windows.h>
struct BleedingUiValue{DWORD nativeFrame{},minutes{},seconds{},stage{};};
struct BleedingUiStateV1{DWORD size=sizeof(BleedingUiStateV1),version=1,tick{},flags{},generation{};BleedingUiValue timer{};};
static_assert(sizeof(BleedingUiStateV1)==36,"Copied native bleeding timer ABI");
using BleedingUiGetter=BOOL(WINAPI*)(void*,DWORD);
using BleedingUiReady=void(WINAPI*)(BOOL,DWORD);
