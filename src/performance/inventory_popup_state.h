#pragma once
#include <windows.h>
#include "bleeding_ui_state.h"
enum InventoryPopupFlags:DWORD {InventoryPopupVisible=1,InventoryPopupBusy=2,InventoryPopupClosing=4,InventoryPopupNativeAnimation=8};
enum class InventoryPopupAction:DWORD {Focus=1,Confirm=2,Cancel=3,Amount=4};
struct InventoryPopupStateV1 {
 DWORD size=sizeof(InventoryPopupStateV1),version=1,tick{},flags{},generation{},count{},selected{},nativeFrame{};
 DWORD kind=1,amount{},capacity{},liquid{};
 wchar_t title[128]{},choices[2][512]{},cancel[64]{},confirm[64]{};
 BleedingUiValue bleeding{};
};
static_assert(sizeof(InventoryPopupStateV1)==2624,"Copied inventory choice ABI");
using InventoryPopupGetter=BOOL(WINAPI*)(void*,DWORD);
using InventoryPopupSender=BOOL(WINAPI*)(DWORD,DWORD,DWORD);
using InventoryPopupReady=void(WINAPI*)(BOOL,DWORD);
