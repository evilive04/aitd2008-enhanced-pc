#pragma once
#include <windows.h>
#include <cstddef>
#include "options_children_art.h"
struct OptionsChildrenStateV1 {
 DWORD size,version,tick,flags,generation,nativeMode,dispatches,rejected,actions;
 options_children_art::View view;
};
enum OptionsChildrenFlags : DWORD {OptionsChildVisible=1,OptionsChildActionsProven=2,OptionsChildBusy=4,OptionsChildInGame=8,OptionsChildPhoneNetworkKnown=16,OptionsChildPhoneNetwork=32};
using OptionsChildrenGetter=BOOL(WINAPI*)(void*,DWORD);
using OptionsChildrenSender=BOOL(WINAPI*)(DWORD,LONG,double,DWORD);
static_assert(sizeof(options_children_art::Row)==552,"Child row ABI");
static_assert(sizeof(options_children_art::Footer)==184,"Child footer ABI");
static_assert(sizeof(options_children_art::View)==6536,"Child View ABI");
static_assert(offsetof(options_children_art::View,rows)==1936,"Child rows ABI");
static_assert(offsetof(options_children_art::View,footer)==5800,"Child footer offset ABI");
static_assert(offsetof(OptionsChildrenStateV1,view)==40,"Child snapshot View ABI");
static_assert(sizeof(OptionsChildrenStateV1)==6576,"Child snapshot ABI");
// Flags/actions authenticate native functionality independently of visibility.
// A presenter must acknowledge this generation before Queue can consume input.
