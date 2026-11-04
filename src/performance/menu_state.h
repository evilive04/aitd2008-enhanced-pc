#pragma once
#include <windows.h>
// This transport contains no native pointers. A state expires after250ms.
struct MenuStateV1 {
    DWORD size,version,tick,flags;
    DWORD nativeFlags,nativeState,dispatches,rejected;
};
enum MenuFlags : DWORD { MenuAuthenticated=1,MenuRootVisible=2,MenuCredits=4,MenuContinueKnown=8,MenuContinueAvailable=16,MenuTitleVisible=32,MenuTransitionBackdrop=64 };
enum MenuAction : DWORD { MenuContinue=1,MenuNewGame=2,MenuChapters=3,MenuOptions=4,MenuShowCredits=5,MenuQuit=6,MenuCloseCredits=7 };
using MenuStateGetter=BOOL (WINAPI*)(void*,DWORD);
using MenuActionSender=BOOL (WINAPI*)(DWORD);
