#pragma once
#include <windows.h>
enum class PhoneItemKind:DWORD {Gps=1,Messages=2,Settings=3,Dial=4,Contacts=5};
struct PhoneItemV1 {DWORD nativeSlot,kind,enabled;wchar_t label[128];};
struct PhoneStateV1 {
 DWORD size,version,tick,flags,generation,nativePage,count,selected,dispatches,rejected;
 wchar_t title[128],objectiveTitle[96],objective[768];
 PhoneItemV1 items[6];
 wchar_t closeText[96],openText[96];
};
static_assert(sizeof(PhoneItemV1)==268&&sizeof(PhoneStateV1)==4016,"Copied phone ABI; no live game pointers");
enum PhoneFlags:DWORD {PhoneHome=1,PhoneAuthenticated=2,PhoneBusy=4,PhoneObjective=8,PhoneNetwork=16,PhoneNetworkKnown=32,PhoneNativeSubpage=64,PhoneDirectMap=128};
using PhoneGetter=BOOL(WINAPI*)(void*,DWORD);
using PhoneSender=BOOL(WINAPI*)(DWORD,DWORD);
using PhoneReady=void(WINAPI*)(BOOL,DWORD);
// Actions1..6 refer to captured native slots;7 closes the phone.
constexpr DWORD PhoneClose=7;
