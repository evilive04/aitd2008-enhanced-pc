#pragma once
#include <windows.h>
enum QteUiFlags:DWORD{QteUiVisible=1,QteUiRepeat=2,QteUiTimingOpen=4};
enum QteUiKind:DWORD{QteUiButton=1,QteUiCompression=2,QteUiBreathing=3,QteUiSteering=4,QteUiCables=5,QteUiCaption=6};
struct QteUiStateV1{
 DWORD size=sizeof(QteUiStateV1),version=1,tick{},flags{},generation{},kind{},nativeFrame{},phase{},pulse{},meter1{},meter2{};
 wchar_t body[1024]{},key[256]{};
};
static_assert(sizeof(QteUiStateV1)==2604,"Copied PC QTE ABI");
using QteUiGetter=BOOL(WINAPI*)(void*,DWORD);
using QteUiReady=void(WINAPI*)(BOOL,DWORD);
