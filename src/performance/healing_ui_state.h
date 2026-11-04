#pragma once
#include <windows.h>
enum HealingUiFlags:DWORD{HealingUiVisible=1,HealingUiSelected=2};
struct HealingUiStateV2{
 DWORD size=sizeof(HealingUiStateV2),version=2,tick{},flags{},generation{},selected{},severity{},injuries{},medicine{};
 wchar_t keys[8][256]{};
};
static_assert(sizeof(HealingUiStateV2)==4132,"Copied healing observer ABI");
using HealingUiGetter=BOOL(WINAPI*)(void*,DWORD);
