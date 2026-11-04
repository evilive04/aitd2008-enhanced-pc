#pragma once
#include <windows.h>
enum EpisodeEndFlags:DWORD {EpisodeEndVisible=1,EpisodeEndBusy=2};
enum class EpisodeEndAction:DWORD {Focus=1,Activate=2};
struct EpisodeEndStateV1 {
 DWORD size=sizeof(EpisodeEndStateV1),version=1,tick{},flags{},generation{},selected{},dispatches{},rejected{};
 wchar_t title[128]{},buttons[2][128]{},select[64]{};
};
static_assert(sizeof(EpisodeEndStateV1)==928,"Copied episode end ABI");
using EpisodeEndGetter=BOOL(WINAPI*)(void*,DWORD);
using EpisodeEndSender=BOOL(WINAPI*)(DWORD,DWORD,DWORD);
using EpisodeEndReady=void(WINAPI*)(BOOL,DWORD);
