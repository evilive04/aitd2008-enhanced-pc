#pragma once
#include <windows.h>
enum InteractionUiFlags:DWORD{InteractionUiVisible=1,InteractionUiAllowed=2,InteractionUiAboveNativeHint=4};
struct InteractionUiStateV1{
 DWORD size=sizeof(InteractionUiStateV1),version=1,tick{},flags{},generation{},action{},traits{},nativeFrame{};
 DWORD bitmap{}; // Authenticated current helper /shape/ic sprite60 frame.
 wchar_t key[256]{},label[128]{},unavailable[512]{};
};
static_assert(sizeof(InteractionUiStateV1)==1828,"Copied interaction UI ABI");
using InteractionUiGetter=BOOL(WINAPI*)(void*,DWORD);
using InteractionUiReady=void(WINAPI*)(BOOL,DWORD);

struct IDirect3DTexture9;
using InteractionUiImageGetter=HRESULT(WINAPI*)(DWORD,DWORD,IDirect3DTexture9**);
