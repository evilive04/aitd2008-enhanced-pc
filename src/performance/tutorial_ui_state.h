#pragma once
#include <windows.h>
enum TutorialUiFlags:DWORD{TutorialUiVisible=1,TutorialUiBlocking=2,TutorialUiClosing=4};
struct TutorialUiStateV1{DWORD size=sizeof(TutorialUiStateV1),version=1,tick{},flags{},generation{},nativeFrame{},phase{};wchar_t body[2048]{},key[256]{};};
static_assert(sizeof(TutorialUiStateV1)==4636,"Copied PC tutorial ABI");
using TutorialUiGetter=BOOL(WINAPI*)(void*,DWORD);
using TutorialUiReady=void(WINAPI*)(BOOL,DWORD);
