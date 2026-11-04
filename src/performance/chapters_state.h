#pragma once
#include <windows.h>
#include <type_traits>
enum ChaptersFlags:DWORD{ChaptersVisible=1,ChaptersBusy=2,ChaptersInGame=4,ChaptersCanChange=8,ChaptersTransition=16};
// The BOOL-compatible ready export accepts0=unavailable,1=interactive,2=paint
// only. A cached transition frame must never authorize actions for a new page.
enum ChaptersReadyMode:BOOL{ChaptersUnavailable=0,ChaptersInteractive=1,ChaptersPaintOnly=2};
struct ChaptersRow{wchar_t title[128]{};DWORD locked{};};
struct ChaptersStateV1{
 DWORD size=sizeof(ChaptersStateV1),version=1,tick{},flags{},generation{},chapter{},chapters{},selected{},offset{},count{},total{};
 wchar_t number[96]{},title[128]{},information[512]{},back[64]{},open[64]{};
 ChaptersRow rows[5]{};
};
static_assert(sizeof(ChaptersStateV1)==3072&&std::is_trivially_copyable<ChaptersStateV1>::value,"Copied chapters content ABI");
using ChaptersGetter=BOOL(WINAPI*)(void*,DWORD);
using ChaptersSender=BOOL(WINAPI*)(DWORD,LONG,DWORD);
using ChaptersReady=void(WINAPI*)(BOOL,DWORD);
struct IDirect3DTexture9;
using ChaptersPreviewGetter=HRESULT(WINAPI*)(DWORD,DWORD,IDirect3DTexture9**);
