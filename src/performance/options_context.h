#pragma once
#include <windows.h>
// Pointer-free copied native strings. Generation and timestamp bound each action.
struct OptionsStateV1 {
 DWORD size,version,tick,flags,generation,nativeFlags,nativeState,dispatches,rejected;
 wchar_t labels[7][128]; // Options, Audio, Languages, Display, Configuration, Back, Reset all
};
enum OptionsFlags : DWORD {OptionsAuthenticated=1,OptionsRootVisible=2,OptionsInGame=4};
enum OptionsAction : DWORD {OptionsAudio=1,OptionsLanguages=2,OptionsDisplay=3,OptionsConfiguration=4,OptionsBack=5,OptionsResetAll=6};
using OptionsStateGetter=BOOL(WINAPI*)(void*,DWORD);
using OptionsActionSender=BOOL(WINAPI*)(DWORD,DWORD);
using OptionsPresentationSetter=void(WINAPI*)(BOOL,DWORD);
