#pragma once
#include <windows.h>
#include <type_traits>
enum class MainPopupKind:DWORD{Information=1,Choice=2,Conflict=3};
enum MainPopupFlags:DWORD{MainPopupVisible=1,MainPopupBusy=2};
struct MainPopupStateV1{
 DWORD size=sizeof(MainPopupStateV1),version=1,tick{},flags{},generation{},type=1,dispatches{},rejected{};
 wchar_t title[96]{},body[1024]{},labels[2][64]{};
};
static_assert(sizeof(MainPopupStateV1)==2528&&std::is_trivially_copyable<MainPopupStateV1>::value,"Copied main popup ABI");
using MainPopupGetter=BOOL(WINAPI*)(void*,DWORD);
using MainPopupSender=BOOL(WINAPI*)(DWORD,DWORD);
using MainPopupReady=void(WINAPI*)(BOOL,DWORD);
