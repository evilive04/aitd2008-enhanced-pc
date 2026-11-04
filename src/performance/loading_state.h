#pragma once
#include <windows.h>
enum LoadingFlags:DWORD {LoadingVisible=1,LoadingFullCanvas=2,LoadingInformation=4};
//1..5 are General's authentic progress stages;6 identifies the separately
// authenticated MainMenu /loading700 film;7 identifies General's original
// text-only information/tip route;8 its two-movie helper instruction route;
//9 its original illustrated instruction route.
// None of these are invented progress stages.
struct LoadingStateV3 {
 DWORD size=sizeof(LoadingStateV3),version=3,tick{},flags{},generation{},nativeMode{};
 wchar_t message[1024]{};
 wchar_t title[128]{},body[2048]{};
 DWORD picture{}; //1..5 source-authenticated original instruction image;0 none.
};
static_assert(sizeof(LoadingStateV3)==6428,"Copied loading ABI3, no live pointers");
struct IDirect3DTexture9;
using LoadingImage=HRESULT(WINAPI*)(DWORD,IDirect3DTexture9**);
using LoadingGetter=BOOL(WINAPI*)(void*,DWORD);
using LoadingReady=void(WINAPI*)(BOOL,DWORD);
