#pragma once
#include <windows.h>
enum GeneralHudFlags:DWORD {GeneralHudVisible=1,GeneralHudSubtitle=2,GeneralHudTimer=4,GeneralHudCabinetExit=8};
struct GeneralHudStateV1{
 DWORD size=sizeof(GeneralHudStateV1),version=2,tick{},flags{},generation{},minutes{},seconds{};
 wchar_t subtitle[2048]{};
 wchar_t exitKey[256]{},exitLabel[128]{};
};
static_assert(sizeof(GeneralHudStateV1)==4892,"Copied General HUD ABI");
using GeneralHudGetter=BOOL(WINAPI*)(void*,DWORD);
using GeneralHudReady=void(WINAPI*)(BOOL,DWORD);
