#pragma once
#include <windows.h>
#include <cstdint>
#include <type_traits>
enum PhoneContentFlags:DWORD{PhoneContentVisible=1,PhoneContentBusy=2,PhoneContentNetworkKnown=4,PhoneContentNetwork=8};
enum class PhoneContentKind:DWORD{List=1,Reader=2,Call=3,Dialer=4,Gps=5};
enum class PhoneContentAction:DWORD{Focus=1,Open=2,Back=3,Previous=4,Next=5,Digit=6,Delete=7,Call=8};
struct PhoneContentRow{wchar_t title[256]{};DWORD read{};};
struct PhoneContentView{
 PhoneContentKind kind=PhoneContentKind::List;DWORD count{},total{},offset{},selected{};
 wchar_t title[96]{},subtitle[256]{},body[4096]{},back[64]{},open[64]{};
 PhoneContentRow rows[10]{};
};
struct PhoneContentStateV1{
 DWORD size=sizeof(PhoneContentStateV1),version=1,tick{},flags{},generation{},dispatches{},rejected{};
 PhoneContentView view;
};
static_assert(std::is_standard_layout<PhoneContentStateV1>::value&&std::is_trivially_copyable<PhoneContentStateV1>::value,"Copied phone content only");
static_assert(sizeof(PhoneContentStateV1)==14360,"Version1 phone content ABI");
using PhoneContentGetter=BOOL(WINAPI*)(void*,DWORD);
using PhoneContentSender=BOOL(WINAPI*)(DWORD,DWORD,DWORD);
using PhoneContentReady=void(WINAPI*)(BOOL,DWORD);
// Consume once for this exact ready generation. Native high-resolution wheel
// deltas remain signed and accumulate until the presenter reads them.
using PhoneContentWheel=LONG(WINAPI*)(DWORD);
