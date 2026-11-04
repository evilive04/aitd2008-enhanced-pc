#pragma once
#include <windows.h>
#include "controls_art.h"
enum ControlsFlags:DWORD{ControlsVisible=1,ControlsBusy=2,ControlsCaptureInput=4,ControlsActionsProven=8};
struct ControlsStateV1{
 DWORD size=sizeof(ControlsStateV1),version=1,tick{},flags{},generation{},dispatches{},rejected{};
 controls_art::View view;
};
static_assert(std::is_standard_layout<ControlsStateV1>::value&&std::is_trivially_copyable<ControlsStateV1>::value,"Copied controls snapshot only");
using ControlsGetter=BOOL(WINAPI*)(void*,DWORD);
using ControlsSender=BOOL(WINAPI*)(DWORD,LONG,double,DWORD);
using ControlsReady=void(WINAPI*)(BOOL,DWORD);
using ControlsWheel=LONG(WINAPI*)(DWORD);
static_assert(offsetof(ControlsStateV1,view)==32&&sizeof(ControlsStateV1)==8880,"Controls snapshot ABI");
