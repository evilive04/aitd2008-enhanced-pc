#pragma once
#include <windows.h>
#include "options_native_values.h"
enum CalibrationFlags:DWORD{CalibrationVisible=1,CalibrationBusy=2,CalibrationTransition=4,CalibrationInGame=8,CalibrationLevelsKnown=16};
enum CalibrationAction:DWORD{CalibrationFocus=1,CalibrationGauge,CalibrationPrevious,CalibrationNext,CalibrationConfirm,CalibrationCancel};
struct CalibrationStateV1{
 DWORD size=sizeof(CalibrationStateV1),version=1,tick{},flags{},generation{},selected{},page{},dispatches{},rejected{};
 float opacity=1;options_native_values::Gauge gauges[4]{};
 wchar_t title[128]{},brightness[64]{},instructions[512]{},accept[64]{},cancel[64]{};
};
static_assert(sizeof(CalibrationStateV1)==1768,"Copied calibration ABI");
using CalibrationGetter=BOOL(WINAPI*)(void*,DWORD);
using CalibrationSender=BOOL(WINAPI*)(DWORD,DWORD,double,DWORD);
using CalibrationReady=void(WINAPI*)(BOOL,DWORD);
